/*
 * Copyright (c) 2013-2015 BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "service.hpp"
#include <QtNetwork/QNetworkRequest>
#include <bb/Application>
#include <bb/platform/Notification>
#include <bb/platform/NotificationDefaultApplicationSettings>
#include <bb/system/InvokeManager>
#include <bb/system/InvokeRequest>
#include <bb/data/JsonDataAccess>
#include <QUuid>
#include <QSettings>
#include <QSqlRecord>
#include <sys/time.h>
#include <time.h>

#include <QTimer>

using namespace bb::platform;
using namespace bb::system;
using namespace bb::data;

Service::Service() :
        QObject(),
        m_netConfManager(0),
        m_syncTimer(0),
        m_notify(new Notification(this)),
        m_invokeManager(new InvokeManager(this)),
        m_networkManager(new QNetworkAccessManager(this)),
        m_lastSyncTimestamp("1970-01-01T00:00:00Z"),
        m_pushSocket(new QSslSocket(this)),
        m_pushPingTimer(new QTimer(this)),
        m_pushWatchdogTimer(new QTimer(this))
{
    // --- GLOBAL SSL/TLS KONFİGÜRASYONU ---
    // (QNetworkAccessManager üzerinden yapılacak REST API istekleri için varsayılan ayar)
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setProtocol(QSsl::SecureProtocols);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    // REST istekleri için SSL hata bastırma
    connect(m_networkManager, SIGNAL(sslErrors(QNetworkReply*, QList<QSslError>)),
            this, SLOT(onGlobalSslErrors(QNetworkReply*, QList<QSslError>)));

    // NOT: m_pushSocket için olan setSslConfiguration ve sslErrors sinyal
    // bağlantıları buradan kaldırıldı. Çünkü bu işlemler startPushConnection()
    // içerisinde URL'in HTTP veya HTTPS olmasına göre dinamik olarak yapılıyor.

    m_initRun = m_settings.value("initRun").toBool();
    m_accessToken = m_settings.value("accessToken").toString();
    m_url = m_settings.value("serverUrl").toString();
    m_pushType = "";
    m_pushSender = "";
    m_pushDeletedID = "";
    m_pushEditedID = "";

    m_invokeManager->connect(m_invokeManager, SIGNAL(invoked(const bb::system::InvokeRequest&)),
            this, SLOT(handleInvoke(const bb::system::InvokeRequest&)));

    m_netConfManager = new QNetworkConfigurationManager(this);
    connect(m_netConfManager, SIGNAL(onlineStateChanged(bool)), this, SLOT(handleConnectivityChange(bool)));

    QTimer::singleShot(2000, this, SLOT(startSyncLoop()));

    if (m_netConfManager->isOnline()) {
        qDebug() << "[service.cpp] [NETWORK] Device is ONLINE at startup.";
        startPushConnection();
    } else {
        qDebug() << "[service.cpp] [NETWORK] Device is OFFLINE at startup. Waiting for connection...";
    }
}

// Servis REST istekleri SSL slotu
void Service::onGlobalSslErrors(QNetworkReply *reply, const QList<QSslError> &errors) {
    Q_UNUSED(errors);
    if (reply) {
        reply->ignoreSslErrors();
    }
}


void Service::handleInvoke(const bb::system::InvokeRequest & request)
{
        if (request.action().compare("com.example.BerryBridgeUIService.PAUSE_SYNC") == 0) {
            qDebug() << "[service.cpp]    → Handling PAUSE_SYNC action";
            pauseSyncing();
        }
        else if (request.action().compare("com.example.BerryBridgeUIService.DELAY_SYNC") == 0) {
            qDebug() << "[service.cpp]    → Handling DELAY_SYNC action";
            if (m_syncTimer) {
                m_syncTimer->start(120000); // 2 dakika ertele
            }
        }
        else if (request.action().compare("com.example.BerryBridgeUIService.RESUME_SYNC") == 0) {
            qDebug() << "[service.cpp]    → Handling RESUME_SYNC action";
            resumeSyncing();
        }else if (request.action().compare("com.example.BerryBridgeUIService.CRED_UPDATE") == 0) {
        qDebug() << "[service.cpp]    → Handling CRED_UPDATE action";
        m_settings.sync();
        m_accessToken=m_settings.value("accessToken").toString();
        m_url=m_settings.value("serverUrl").toString();
    }
    else if (request.action().compare("com.example.BerryBridgeUIService.INIT_UPDATE") == 0) {
        qDebug() << "[service.cpp]    → Handling INIT_UPDATE action";
        //initDatabases();
        m_settings.sync();
        m_initRun=m_settings.value("initRun").toBool();
        qDebug() << "[service.cpp] m_initRun: "<<m_initRun;
        startPushConnection();
    }else if (request.action() == "com.example.BerryBridgeUIService.CREATE_NOTIFICATION") {

        QByteArray data = request.data();
        bb::data::JsonDataAccess jda;
        QVariantMap map = jda.loadFromBuffer(data).toMap();

        QString accountID  = map.value("accountID").toString();
        QString chatID     = map.value("chatID").toString();
        QString senderName = map.value("senderName").toString();
        QString msgType    = map.value("msgType").toString();
        QString text       = map.value("text").toString();

        // service.cpp içinde var olan mevcut fonksiyonunuz çağrılıyor:
        createMessageNotification(accountID, chatID, senderName, msgType, text);
    }

}

void Service::pauseSyncing()
{
    qDebug() << "[SERVICE] Pausing sync loop - initial database download in progress";
    if (m_syncTimer) {
        m_syncTimer->stop();
    }
}

void Service::resumeSyncing()
{
    qDebug() << "[SERVICE] Resuming sync loop - initial database download complete";

    // Load user preferences when resuming
    loadUserPreferences();

    if (m_syncTimer) {
        m_syncTimer->start();
    }
}



void Service::startSyncLoop()
{

    if(!m_initRun){
        QTimer::singleShot(30000, this, SLOT(startSyncLoop()));
        qDebug() << "[SERVICE] waiting for m_initRun"<<m_initRun;
        m_initRun=m_settings.value("initRun").toBool();
        return;
    }
    qDebug() << "[SERVICE] startSyncLoop initializing with persistent connections...";

    QSqlDatabase chatsDb = QSqlDatabase::database("chats_db_conn");
    QSqlDatabase msgsDb = QSqlDatabase::database("messages_db_conn");

    if (!chatsDb.isOpen() || !msgsDb.isOpen()) {
        qWarning() << "[SERVICE] [DB] Critical connections not open! Retrying in 30s...";
        initDatabases();
        QTimer::singleShot(30000, this, SLOT(startSyncLoop()));
        return;
    }

    qDebug() << "[SERVICE] [OK] Persistent connections established.";

    // KALICI HAFIZADAN SON ZAMANI YÜKLE
    if (m_lastSyncTimestamp.isEmpty()) {
        m_settings.sync();
        m_lastSyncTimestamp = m_settings.value("lastSyncTimestamp", "").toString();
    }

    if (m_syncTimer) {
        m_syncTimer->stop();
        m_syncTimer->deleteLater();
    }

    m_syncTimer = new QTimer(this);
    connect(m_syncTimer, SIGNAL(timeout()), this, SLOT(performPeriodicSync()));

    performPeriodicSync();
    m_syncTimer->start(60000);
}

void Service::performPeriodicSync()
{
    if (m_netConfManager && !m_netConfManager->isOnline()) return;

    bool isPaginating = !m_nextCursor.isEmpty();

    if (!isPaginating && m_pushSocket && m_pushSocket->state() == QAbstractSocket::ConnectedState && m_initialSyncComplete) {
        m_pushSkipCount++;
        if (m_pushSkipCount < 5) return;
        m_pushSkipCount = 0;
    }

    QUrl url(m_url + "/v1/messages/search");

    if (isPaginating) {
        url.addQueryItem("cursor", m_nextCursor);
        url.addQueryItem("direction", "before");
    } else {
        // Eğer cihazda kayıtlı hiçbir zaman yoksa, çok eski bir tarih yerine
        // daha mantıklı bir başlangıç yapılabilir ama mevcut yapını koruyoruz.
        QString ts = m_lastSyncTimestamp.isEmpty() ? "1970-01-01T00:00:00Z" : m_lastSyncTimestamp;
        url.addQueryItem("dateAfter", ts);
    }
    url.addQueryItem("limit", "5");

    QStringList activeAccountIDs = getActiveAccountIDs();

    foreach (const QString &accountID, activeAccountIDs) {
        if (!accountID.isEmpty()) {
            url.addQueryItem("accountIDs", accountID);
            qDebug() << "accountIDs: "<< accountID;
        }
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setRawHeader("Accept", "application/json");

    m_syncReply = m_networkManager->get(request);
    if (m_syncReply) {
        connect(m_syncReply, SIGNAL(finished()), this, SLOT(onSyncResponseReceived()));
    }
}

void Service::onSyncResponseReceived()
{
    // Sinyali gönderen reply nesnesini sender() ile alıyoruz
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    // Güvenlik kontrolü (Eğer sender null ise m_syncReply'a düş)
    if (!reply) {
        reply = m_syncReply;
    }
    if (!reply) return;

    qint64 bytesAvailable = reply->bytesAvailable();
    qDebug() << "[DATABASE] [DEBUG] Sync Response Size:" << (bytesAvailable / 1024) << "KB";

    // 1. AŞIRI BÜYÜK YANIT KORUMASI (RAM Koruması - BB10)
    if (bytesAvailable > 102400) {
        qWarning() << "[DATABASE] [WARN] Response too large! Aborting request.";
        reply->abort();
        reply->readAll();
        if (m_syncReply == reply) m_syncReply = 0;
        reply->deleteLater();
        if (m_syncTimer) m_syncTimer->start(60000);
        return;
    }

    // 2. HTTP / AĞ HATASI KORUMASI
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || httpStatus >= 400) {
        qWarning() << "[DATABASE] Sync Failed! HTTP:" << httpStatus;
        m_nextCursor = "";
        if (m_syncTimer) m_syncTimer->start(60000);
        if (m_syncReply == reply) m_syncReply = 0;
        reply->deleteLater();
        return;
    }

    bool hasMore = false;
    int newMessagesCounter = 0;
    int alreadyExistingMessages = 0; // FREN MEKANİZMASI İÇİN
    int totalMessagesInBatch = 0;
    QString maxTimestampInBatch = "";

    // Kilitlenmeyi önlemek için transaction bayrakları
    bool msgInTx = false;
    bool chatInTx = false;

    try {
        QByteArray responseData = reply->readAll();
        bb::data::JsonDataAccess jda;
        QVariantMap root = jda.loadFromBuffer(responseData).toMap();
        responseData.clear(); // Ham yanıtı bellekten derhal serbest bırak

        if (jda.hasError()) throw std::runtime_error("JSON Parse Error");

        QVariantList items = root.take("items").toList();
        QVariantMap chatsMap = root.take("chats").toMap();
        hasMore = root.take("hasMore").toBool();
        QString oldestCursor = root.take("oldestCursor").toString();
        root.clear();

        if (!QSqlDatabase::contains("messages_db_conn") ||
            !QSqlDatabase::contains("chats_db_conn") ||
            !QSqlDatabase::database("messages_db_conn").isOpen() ||
            !QSqlDatabase::database("chats_db_conn").isOpen())
        {
            qWarning() << "[DATABASE] Veritabanı bağlantıları kapalı veya eksik! initDatabases tetikleniyor...";
            initDatabases();
        }

        QSqlDatabase msgDb = QSqlDatabase::database("messages_db_conn");
        QSqlDatabase chatDb = QSqlDatabase::database("chats_db_conn");

        if (!msgDb.isOpen() || !chatDb.isOpen()) {
            qCritical() << "[DATABASE] Kritik Hata: Veritabanları açılamadı!";
            if (m_syncTimer) m_syncTimer->start(60000);
            if (m_syncReply == reply) m_syncReply = 0;
            reply->deleteLater();
            return;
        }

        QSqlQuery msgCheck(msgDb);
        msgCheck.prepare("SELECT 1 FROM messages WHERE id = ?");

        QSqlQuery ins(msgDb);
        ins.prepare("INSERT OR REPLACE INTO messages ("
                "id, chatID, accountID, senderID, senderName, timestamp, sortKey, type, text, "
                "isSender, isUnread, attachments, reactions, editedTimestamp, isDeleted, "
                "linkedMessageID, mentions, seen) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        QSqlQuery chatCheck(chatDb);
        chatCheck.prepare("SELECT unreadCount, isPinned, isMuted, lastActivity, preview_json FROM chats WHERE id = ?");

        QSqlQuery upsertChat(chatDb);
        upsertChat.prepare("INSERT OR REPLACE INTO chats ("
                "id, localChatID, accountID, network, title, description, imgURL, type, "
                "isReadOnly, lastActivity, unreadCount, unreadMentionsCount, "
                "lastReadMessageSortKey, draft, reminder, snooze, "
                "isArchived, isMarkedUnread, isMuted, isPinned, isLowPriority, "
                "messageExpirySeconds, participants_json, capabilities_json, preview_json) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        QMap<QString, QVariantMap> latestMessagesPerChat;
        QMap<QString, QVariantMap> previewMap;
        QMap<QString, int> incomingUnreadCounts;

        // Transaction başlatma
        if (msgDb.isOpen() && msgDb.transaction()) msgInTx = true;
        if (chatDb.isOpen() && chatDb.transaction()) chatInTx = true;

        totalMessagesInBatch = items.size();
        QString msgId;
        QString nText = "";
        QString nType = "TEXT";
        bool isSender = false;
        QString senderName = "";
        QString nPreview;

        // 1. MESAJLARI İŞLE
        while (!items.isEmpty()) {
            QVariantMap msg = items.takeFirst().toMap();
            msgId = msg["id"].toString();
            QString chatId = msg["chatID"].toString();
            QString currentTs = msg["timestamp"].toString();
            nText = msg["text"].toString();
            nType = msg["type"].toString();
            isSender = msg["isSender"].toBool();
            senderName = msg["senderName"].toString();

            if (nType == "TEXT") { nPreview = nText; }
            else if (nType == "IMAGE") { nText = QString::fromUtf8("📷 Photo ") + nText; }
            else if (nType == "VIDEO" || nType == "VIDEO_MESSAGE") { nText = QString::fromUtf8("🎥 Video ") + nText; }
            else if (nType.contains("AUDIO") || nType.contains("VOICE")) { nText = QString::fromUtf8("🔉 Audio ") + nText; }
            else if (nType == "FILE") { nText = QString::fromUtf8("📄   File ") + nText; }
            else if (nType == "LOCATION") { nText = QString::fromUtf8("📍 Location ") + nText; }
            else if (nType == "STICKER") { nText = QString::fromUtf8("🌞 Sticker ") + nText; }
            else { nText = nType + " " + nText; }
            nPreview = nText;
            if (maxTimestampInBatch.isEmpty() || currentTs > maxTimestampInBatch) {
                maxTimestampInBatch = currentTs;
            }

            if (!latestMessagesPerChat.contains(chatId) ||
                currentTs > latestMessagesPerChat[chatId]["timestamp"].toString()) {

                latestMessagesPerChat[chatId] = msg;
                latestMessagesPerChat[chatId]["timestamp"] = currentTs; // Güncel zamanı tutuyoruz

                previewMap[chatId].insert("type", nType);
                previewMap[chatId].insert("isSender", isSender);
                previewMap[chatId].insert("senderName", senderName);
                previewMap[chatId].insert("text", nPreview);
                previewMap[chatId].insert("timestamp", currentTs); // ZAMAN DAMGASINI (TIMESTAMP) EKLİYORUZ
            }

            msgCheck.bindValue(0, msgId);
            msgCheck.exec();

            if (!msgCheck.next()) { // Yeni mesaj

                QString attachmentsJson = "";
                QVariantList atts = msg["attachments"].toList();
                QByteArray buffer;
                if (!atts.isEmpty()) {
                    buffer.clear();
                    jda.saveToBuffer(atts, &buffer);
                    attachmentsJson = QString::fromUtf8(buffer);
                }

                QString reactionsJson = "[]";
                QVariantList rawReactions = msg["reactions"].toList();
                if (!rawReactions.isEmpty()) {
                    buffer.clear();
                    jda.saveToBuffer(rawReactions, &buffer);
                    reactionsJson = QString::fromUtf8(buffer);
                }

                QString mentionsJson = "[]";
                QVariantList mentionsList = msg["mentions"].toList();
                if (!mentionsList.isEmpty()) {
                    buffer.clear();
                    jda.saveToBuffer(mentionsList, &buffer);
                    mentionsJson = QString::fromUtf8(buffer);
                }

                QString seenJson = "{}";
                QVariantMap seenMap = msg["seen"].toMap();
                if (!seenMap.isEmpty()) {
                    buffer.clear();
                    jda.saveToBuffer(seenMap, &buffer);
                    seenJson = QString::fromUtf8(buffer);
                }

                ins.bindValue(0, msgId);
                ins.bindValue(1, chatId);
                ins.bindValue(2, msg["accountID"].toString());
                ins.bindValue(3, msg["senderID"].toString());
                ins.bindValue(4, senderName);
                ins.bindValue(5, currentTs);
                ins.bindValue(6, msg["sortKey"].toString());
                ins.bindValue(7, nType);
                ins.bindValue(8, msg["text"].toString());
                ins.bindValue(9, isSender ? 1 : 0);
                ins.bindValue(10, msg["isUnread"].toBool() ? 1 : 0);
                ins.bindValue(11, attachmentsJson);
                ins.bindValue(12, reactionsJson);
                ins.bindValue(13, msg["editedTimestamp"].toString());
                ins.bindValue(14, msg["isDeleted"].toBool() ? 1 : 0);
                ins.bindValue(15, msg["linkedMessageID"].toString());
                ins.bindValue(16, mentionsJson);
                ins.bindValue(17, seenJson);

                if (ins.exec()) {
                    newMessagesCounter++;
                    if (!msg["isSender"].toBool() && msg["isUnread"].toBool()) {
                        incomingUnreadCounts[chatId]++;
                    }
                }
            } else {
                alreadyExistingMessages++;
            }
        }

        // 2. CHATLERİ İŞLE
        QStringList chatIds = chatsMap.keys();
        foreach (const QString& chatId, chatIds) {
            QVariantMap rawChat = chatsMap.take(chatId).toMap();

            chatCheck.bindValue(0, chatId);
            chatCheck.exec();

            int currentUnreadInDb = 0;
            int isPinned, isMuted;
            bool hasDbRecord = false;
            QString dbLastActivity;
            QString oldPreviewJson = "";
            QString cAccountID = rawChat["accountID"].toString();

            if (chatCheck.next()) {
                hasDbRecord = true;
                currentUnreadInDb = chatCheck.value(0).toInt();
                isPinned = chatCheck.value(1).toInt();
                isMuted = chatCheck.value(2).toInt();
                dbLastActivity = chatCheck.value(3).toString();
                oldPreviewJson=chatCheck.value(4).toString();
            } else {
                isPinned = rawChat["isPinned"].toBool() ? 1 : 0;
                isMuted = rawChat["isMuted"].toBool() ? 1 : 0;
            }

            QString apiLastActivity = rawChat.value("lastActivity", "").toString();
            QString finalLastActivity = apiLastActivity;

            // Eğer DB'deki tarih, API'den gelenden daha yeniyse, DB'dekini koru!
            if (hasDbRecord && !dbLastActivity.isEmpty() && dbLastActivity > apiLastActivity) {
                finalLastActivity = dbLastActivity;
            }

            QVariantMap batchPreview = previewMap.value(chatId);
            QString PreviewJson = oldPreviewJson; // Varsayılan olarak db'deki eski preview'i koru
            bool isNewPreviewValid = false;

            // Eğer bu batch içinde bu chat'e ait bir mesaj varsa
            if (!batchPreview.isEmpty()) {
                QString batchPreviewTs = batchPreview.value("timestamp").toString();

                // SADECE VE SADECE gelen mesaj veritabanındaki son aktiviteden daha yeniyse
                // veya veritabanında daha önce hiç kayıt/preview yoksa güncelleyelim!
                if (!hasDbRecord || oldPreviewJson.isEmpty() || batchPreviewTs >= dbLastActivity) {
                    QByteArray jsonBuffer;
                    jda.saveToBuffer(batchPreview, &jsonBuffer);
                    PreviewJson = QString::fromUtf8(jsonBuffer);
                    isNewPreviewValid = true; // Bildirim atabilmek için işaretliyoruz
                }
            }

            upsertChat.bindValue(0, rawChat.value("id", chatId).toString());
            upsertChat.bindValue(1, rawChat.value("localChatID", "").toString());
            upsertChat.bindValue(2, cAccountID);
            upsertChat.bindValue(3, rawChat.value("network", "").toString());
            upsertChat.bindValue(4, rawChat.value("title", "").toString());
            upsertChat.bindValue(5, rawChat.value("description", "").toString());
            upsertChat.bindValue(6, rawChat.value("imgURL"));
            upsertChat.bindValue(7, rawChat.value("type", "").toString());
            upsertChat.bindValue(8, rawChat.value("isReadOnly", false).toBool() ? 1 : 0);
            upsertChat.bindValue(9, finalLastActivity);
            upsertChat.bindValue(10, currentUnreadInDb + incomingUnreadCounts.value(chatId, 0));
            upsertChat.bindValue(11, rawChat.value("unreadMentionsCount", 0).toInt());
            upsertChat.bindValue(12, rawChat.value("lastReadMessageSortKey", "").toString());
            upsertChat.bindValue(13, rawChat.value("draft").toString());
            upsertChat.bindValue(14, rawChat.value("reminder").toString());
            upsertChat.bindValue(15, rawChat.value("snooze").toString());
            upsertChat.bindValue(16, rawChat.value("isArchived", false).toBool() ? 1 : 0);
            upsertChat.bindValue(17, rawChat.value("isMarkedUnread", false).toBool() ? 1 : 0);
            upsertChat.bindValue(18, isMuted);
            upsertChat.bindValue(19, isPinned);
            upsertChat.bindValue(20, rawChat.value("isLowPriority", false).toBool() ? 1 : 0);
            upsertChat.bindValue(21, rawChat.value("messageExpirySeconds", 0).toInt());

            QString participantsJsonStr = "[]";
            if (!rawChat.value("participants").isNull()) {
                jda.saveToBuffer(rawChat.value("participants"), &participantsJsonStr);
            }

            QString capabilitiesJsonStr = "{}";
            if (!rawChat.value("capabilities").isNull()) {
                jda.saveToBuffer(rawChat.value("capabilities"), &capabilitiesJsonStr);
            }

            upsertChat.bindValue(22, participantsJsonStr);
            upsertChat.bindValue(23, capabilitiesJsonStr);
            upsertChat.bindValue(24, PreviewJson);

            if (upsertChat.exec()) {
                // Sadece geçerli (gerçekten en yeni) bir mesaj geldiğinde bildirim gönder
                if (isNewPreviewValid) {
                    QString nPreviewNot = batchPreview.value("text").toString();
                    bool isSenderNot = batchPreview.value("isSender", false).toBool();

                    if (!nPreviewNot.isEmpty() && !isSenderNot && incomingUnreadCounts.value(chatId, 0) > 0) {
                        createMessageNotification(cAccountID, chatId,
                                                  batchPreview.value("senderName", "SenderName").toString(),
                                                  batchPreview.value("type", "TEXT").toString(),
                                                  nPreviewNot);
                    }
                }
            }
        }

        // İŞLEMLER BAŞARILI -> COMMIT
        if (msgInTx) { msgDb.commit(); msgInTx = false; }
        if (chatInTx) { chatDb.commit(); chatInTx = false; }

        // SON TARİHİ CİHAZA KAYDET
        if (!maxTimestampInBatch.isEmpty() && maxTimestampInBatch > m_lastSyncTimestamp) {
            m_lastSyncTimestamp = maxTimestampInBatch;
            QSettings settings;
            settings.setValue("lastSyncTimestamp", m_lastSyncTimestamp);
        }

        // FREN MEKANİZMASI
        if (totalMessagesInBatch > 0 && alreadyExistingMessages == totalMessagesInBatch) {
            qDebug() << "[DATABASE] All messages in batch exist. Stopping pagination loop.";
            m_nextCursor = "";
        } else if (hasMore && !oldestCursor.isEmpty()) {
            m_nextCursor = oldestCursor;
        } else {
            m_nextCursor = "";
        }

        latestMessagesPerChat.clear();

    } catch (const std::bad_alloc&) {
        qCritical() << "[DATABASE] [OUT OF MEMORY] JSON Parse esnasında bellek yetersiz!";
        if (msgInTx && QSqlDatabase::database("messages_db_conn").isOpen()) QSqlDatabase::database("messages_db_conn").rollback();
        if (chatInTx && QSqlDatabase::database("chats_db_conn").isOpen()) QSqlDatabase::database("chats_db_conn").rollback();
        m_nextCursor = "";
    } catch (const std::exception& e) {
        qWarning() << "[DATABASE] [EXCEPTION]" << e.what();
        // Exception durumunda SQLite kilitli kalmasın diye ROLLBACK
        if (msgInTx && QSqlDatabase::database("messages_db_conn").isOpen()) QSqlDatabase::database("messages_db_conn").rollback();
        if (chatInTx && QSqlDatabase::database("chats_db_conn").isOpen()) QSqlDatabase::database("chats_db_conn").rollback();
        m_nextCursor = "";
    } catch (...) {
        qCritical() << "[DATABASE] [UNKNOWN EXCEPTION] Bilinmeyen bir hata oluştu!";
        if (msgInTx && QSqlDatabase::database("messages_db_conn").isOpen()) QSqlDatabase::database("messages_db_conn").rollback();
        if (chatInTx && QSqlDatabase::database("chats_db_conn").isOpen()) QSqlDatabase::database("chats_db_conn").rollback();
        m_nextCursor = "";
    }

    if (newMessagesCounter > 0) {
        emit messagesUpdated();
        QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
        if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
            refreshFile.close();
        }
    }

    if (m_syncTimer) {
        m_syncTimer->start(!m_nextCursor.isEmpty() ? 1000 : 60000);
    }

    if (m_syncReply == reply) m_syncReply = 0;
    reply->deleteLater();
}


void Service::handleConnectivityChange(bool isOnline)
{
    if (isOnline) {
        qDebug() << "[service.cpp] [NETWORK] Connected! Resuming network tasks...";
        // Force an immediate catch-up sync cycle on the next HTTP loop (do not skip for 5 mins)
        //m_pushSkipCount = 5;
        startPushConnection();
        startSyncLoop();
    } else {
         qDebug() << "[service.cpp] [NETWORK] Connection lost. Suspending active sockets...";
         //sendStatusNotification("Push Status", "No Internet Connection. Pending...");
         if (m_pushSocket->state() == QAbstractSocket::ConnectedState) {
             m_pushSocket->abort();  // Force abort the connection without waiting
         }
         m_pushPingTimer->stop();
         m_pushWatchdogTimer->stop();
    }
}

void Service::loadUserPreferences()
{
    m_selectedAccountIDs.clear();

    // Get list of all account IDs from QSettings
    QStringList accountNames = m_settings.value("network_names").toStringList();
    QStringList accountIDs = m_settings.value("account_ids").toStringList();

    qDebug() << "[SERVICE] Found " << accountNames.size() << " total accounts in settings";

    // Check which accounts are enabled
    for (int i = 0; i < accountIDs.size(); ++i) {
        QString idds = accountIDs[i];
        bool isSelected = m_settings.value("state_" + idds, true).toBool();

        if (isSelected && i < accountIDs.size()) {
            QString accountID = accountIDs[i];
            m_selectedAccountIDs.append(accountID);
            qDebug() << "[SERVICE] Account selected: " << idds << " (" << accountID << ")";
        } else {
            qDebug() << "[SERVICE] Account not selected: " << idds;
        }
    }

    qDebug() << "[SERVICE] Will sync only " << m_selectedAccountIDs.size() << " selected accounts";
}

bool Service::isAccountSelected(const QString &accountID) const
{
    return m_selectedAccountIDs.contains(accountID);
}

QString Service::getNetworkNameByAccountID(const QString &accountID) {
    if (accountID.isEmpty()) return QString();

    // Kaydettiğimiz gruptan veriyi çekiyoruz
    // value(key, defaultValue) yapısı sayesinde veri yoksa "" döner
    return m_settings.value("network_mapping/" + accountID, "").toString();
}

void Service::createMessageNotification(const QString& accountID, const QString& chatID, // chatID eklendi
                                      const QString& senderName, const QString& msgType, const QString& text)
{
    // Load user preferences from QSettings
    QSettings settings;
    settings.setValue(QString("newContent/%1").arg(accountID), true);

    bool notificationsEnabled = settings.value("state_" + accountID, true).toBool();
    if (!notificationsEnabled) return;
    // Notification toggle check (frontend prefixes 'state_' onto the string we use here)
    bool personalEnabled = settings.value("state_state_pref_personal_" + accountID, true).toBool();
    bool groupsEnabled = settings.value("state_state_pref_groups_" + accountID, true).toBool();
    bool statusEnabled = settings.value("state_state_pref_status_" + accountID, false).toBool();
    bool channelsEnabled = settings.value("state_state_pref_channels_" + accountID, false).toBool();

    QSqlDatabase chatDb = QSqlDatabase::database("chats_db_conn");

    if (!chatDb.isOpen()) {
        qCritical() << "[SERVICE] Kritik Hata: initDatabases çalıştırılmasına rağmen veritabanları açılamadı!";
        return;
    }

    QSqlQuery selectOldChat(chatDb);
    selectOldChat.prepare("SELECT type, title, isMuted  FROM chats WHERE id = ?");
    selectOldChat.bindValue(0, chatID);
    QString chatType = "";
    QString chatTitle = "";
    bool isChatMuted = false;
    if (selectOldChat.exec() && selectOldChat.next()) {
        chatType = selectOldChat.value(0).toString();
        chatTitle = selectOldChat.value(1).toString();
        isChatMuted = selectOldChat.value(2).toBool();
    }
    // Determine Filtering based on chatType
    bool shouldNotify = true;

    if (chatType == "status") {
        shouldNotify = statusEnabled;
    } else if (chatType == "channel") {
        shouldNotify = channelsEnabled;
    } else if (chatType == "group") {
        shouldNotify = groupsEnabled;
    } else {
        shouldNotify = personalEnabled;
    }

    if (!shouldNotify) {
        qDebug() << "[service.cpp] [FILTER] Skipping notification for type:" << chatType;
        return;
    }


    if(isChatMuted) return;

    bb::platform::Notification *msgNotify = new bb::platform::Notification(chatID, this);

    // Include account and sender OR group/channel/community name in title
    QString titleStr;
    QString networkName=getNetworkNameByAccountID(accountID);
    if ((chatType == "group" || chatType == "channel") && !chatTitle.isEmpty()) {
        titleStr = QString("%1: %2").arg(networkName).arg(chatTitle);
    } else if (chatType == "single") {
        titleStr = QString("%1: %2").arg(networkName).arg(senderName);
    } else if (chatType == "status") {
        titleStr = QString("%1: %2").arg(networkName).arg("Status");
    } else {
        titleStr = QString("%1: %2").arg(networkName).arg(chatTitle);
    }
    msgNotify->setTitle(titleStr);

    // Determine media type for text
    QString displayBody = convertToPlainText(text);
    if (displayBody.isEmpty() && msgType != "TEXT") {
        displayBody = QString("[%1 attached]").arg(msgType);
    }
    //displayBody.replace("<", "&lt;").replace(">", "&gt;");
    //displayBody.remove('*');

    QString plainBody;

    if (chatType != "single" && chatType != "channel") {
        // Grup sohbeti mantığı
        plainBody = QString("%1: %2").arg(senderName.toUpper()).arg(displayBody);
    } else {
        // Özel mesaj veya kanal mantığı
        plainBody = displayBody;
    }

    msgNotify->setBody(plainBody);

    bb::system::InvokeRequest invokeReq;
    invokeReq.setTarget("com.example.BerryBeeperUI");
    invokeReq.setAction("bb.action.START");
    msgNotify->setInvokeRequest(invokeReq);
    msgNotify->notify();
    // Belleği temizle
    msgNotify->deleteLater();
}

void Service::sendStatusNotification(const QString &title, const QString &body)
{
    static const QString statusNotificationId = "push-connection-status";
    bb::platform::Notification *statusNotify = new bb::platform::Notification(statusNotificationId, this);
    statusNotify->setTitle(title.isEmpty() ? "WhatsApp Service" : title);
    statusNotify->setBody(body.isEmpty() ? "Status Update" : body);

    bb::system::InvokeRequest invokeReq;
    invokeReq.setTarget("com.example.WhatsApp");
    invokeReq.setAction("bb.action.START");
    statusNotify->setInvokeRequest(invokeReq);

    statusNotify->notify();
}

// Veritabanı bağlantılarını global/class seviyesinde bir kez açmalısın
void Service::initDatabases() {
    // 1. MESSAGES DATABASE
    if (QSqlDatabase::contains("messages_db_conn")) {
        {
            QSqlDatabase oldDb = QSqlDatabase::database("messages_db_conn");
            oldDb.close();
        }
        QSqlDatabase::removeDatabase("messages_db_conn");
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "messages_db_conn");
        db.setDatabaseName("/accounts/1000/shared/misc/Beeper/messages.db");

        if (db.open()) {
            QSqlQuery query(db);
            query.exec("PRAGMA journal_mode=WAL;");
            query.exec("PRAGMA synchronous=NORMAL;");
            query.exec("PRAGMA busy_timeout=5000;"); // Kilitlenmeleri önlemek için şart

            // UI tarafındaki şemanın birebir aynısı (Sonundaki virgül hatası düzeltildi)
            bool success = query.exec(
                "CREATE TABLE IF NOT EXISTS messages ("
                "id TEXT PRIMARY KEY, "
                "chatID TEXT, "
                "accountID TEXT, "
                "senderID TEXT, "
                "senderName TEXT, "
                "timestamp TEXT, "
                "sortKey TEXT, "
                "type TEXT, "
                "text TEXT, "
                "isSender INTEGER, "
                "isUnread INTEGER, "
                "attachments TEXT, "
                "reactions TEXT, "
                "editedTimestamp TEXT, "
                "isDeleted INTEGER, "
                "linkedMessageID TEXT, "
                "mentions TEXT, "
                "seen TEXT"
                ")"
            );

            if (success) {
                qDebug() << "[SERVICE] Messages DB opened & verified successfully.";
            } else {
                qWarning() << "[SERVICE] Messages table verification failed:" << query.lastError().text();
            }
        } else {
            qWarning() << "[SERVICE] Messages DB failed to open:" << db.lastError().text();
        }
    }

    // 2. CHATS DATABASE
    if (QSqlDatabase::contains("chats_db_conn")) {
        {
            QSqlDatabase oldDb = QSqlDatabase::database("chats_db_conn");
            oldDb.close();
        }
        QSqlDatabase::removeDatabase("chats_db_conn");
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "chats_db_conn");
        db.setDatabaseName("/accounts/1000/shared/misc/Beeper/chats.db");

        if (db.open()) {
            QSqlQuery query(db);
            query.exec("PRAGMA journal_mode=WAL;");
            query.exec("PRAGMA synchronous=NORMAL;");
            query.exec("PRAGMA busy_timeout=5000;");

            // UI tarafındaki chats şemasının birebir aynısı
            QString createTable = "CREATE TABLE IF NOT EXISTS chats ("
                                  "id TEXT PRIMARY KEY, "
                                  "localChatID TEXT, "
                                  "accountID TEXT, "
                                  "network TEXT, "
                                  "title TEXT, "
                                  "description TEXT, "
                                  "imgURL TEXT, "
                                  "type TEXT, "
                                  "isReadOnly INTEGER, "
                                  "lastActivity TEXT, "
                                  "unreadCount INTEGER, "
                                  "unreadMentionsCount INTEGER, "
                                  "lastReadMessageSortKey TEXT, "
                                  "draft TEXT, "
                                  "reminder TEXT, "
                                  "snooze TEXT, "
                                  "isArchived INTEGER, "
                                  "isMarkedUnread INTEGER, "
                                  "isMuted INTEGER, "
                                  "isPinned INTEGER, "
                                  "isLowPriority INTEGER, "
                                  "messageExpirySeconds INTEGER, "
                                  "participants_json TEXT, "
                                  "capabilities_json TEXT, "
                                  "preview_json TEXT)";

            if (query.exec(createTable)) {
                qDebug() << "[SERVICE] Chats DB opened & verified successfully.";
            } else {
                qWarning() << "[SERVICE] Chats table verification failed:" << query.lastError().text();
            }
        } else {
            qWarning() << "[SERVICE] Chats DB failed to open:" << db.lastError().text();
        }
    }
}

void Service::startPushConnection()
{
    qDebug() << "[service.cpp] [PUSH] starting push connection.";
    if (!m_initRun) return;

    if (m_netConfManager && !m_netConfManager->isOnline()) return;

    QUrl url(m_url);
    qDebug()<<"m_url:"<<m_url;
    QString scheme = url.scheme().toLower();

    // Protocol tespiti (https ve wss SSL kullanır)
    bool isEncrypted = (scheme == "https" || scheme == "wss");

    // Scheme belirtilmemişse veya http ise varsayılan port 80, https ise 443 seçilir
    quint16 defaultPort = isEncrypted ? 443 : 80;
    quint16 port = url.port(defaultPort);
    QString host = url.host();

    if (host.isEmpty()) {
        qDebug() << "[PUSH] Hata: Gecersiz veya bos server URL:" << m_url;
        return;
    }

    if (m_pushSocket->state() != QAbstractSocket::UnconnectedState) {
        m_pushSocket->abort();
    }

    m_pushHandshakeDone = false;
    m_wsBuffer.clear();

    // Önceki dinleyicileri temizleyerek sinyal çakışmasını önlüyoruz
    disconnect(m_pushSocket, SIGNAL(encrypted()), this, SLOT(onPushConnected()));
    disconnect(m_pushSocket, SIGNAL(connected()), this, SLOT(onPushConnected()));
    disconnect(m_pushSocket, SIGNAL(disconnected()), this, SLOT(onPushDisconnected()));
    disconnect(m_pushSocket, SIGNAL(readyRead()), this, SLOT(onPushReadyRead()));
    disconnect(m_pushSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onPushError(QAbstractSocket::SocketError)));
    disconnect(m_pushSocket, SIGNAL(sslErrors(QList<QSslError>)), m_pushSocket, SLOT(ignoreSslErrors()));

    // Ortak sinyaller
    connect(m_pushSocket, SIGNAL(disconnected()), this, SLOT(onPushDisconnected()), Qt::UniqueConnection);
    connect(m_pushSocket, SIGNAL(readyRead()), this, SLOT(onPushReadyRead()), Qt::UniqueConnection);
    connect(m_pushSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onPushError(QAbstractSocket::SocketError)), Qt::UniqueConnection);

    if (isEncrypted) {
        // --- SSL / TLS Bağlantısı ---
        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        sslConfig.setProtocol(QSsl::SecureProtocols); // Qt 4.8 TLS 1.2
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
        m_pushSocket->setSslConfiguration(sslConfig);

        // SSL El sıkışması bittiğinde onPushConnected çağrılır
        connect(m_pushSocket, SIGNAL(encrypted()), this, SLOT(onPushConnected()), Qt::UniqueConnection);
        connect(m_pushSocket, SIGNAL(sslErrors(QList<QSslError>)), m_pushSocket, SLOT(ignoreSslErrors()), Qt::UniqueConnection);

        qDebug() << "[PUSH] Guvenli TLS SSL baglantisi kuruluyor..." << host << ":" << port;
        m_pushSocket->connectToHostEncrypted(host, port);
    } else {
        // --- Düz HTTP / Şifresiz TCP Bağlantısı ---
        // Normal TCP soketi bağlandığında doğrudan onPushConnected çağrılır
        connect(m_pushSocket, SIGNAL(connected()), this, SLOT(onPushConnected()), Qt::UniqueConnection);

        qDebug() << "[PUSH] Duz (HTTP) soket baglantisi kuruluyor..." << host << ":" << port;
        m_pushSocket->connectToHost(host, port);
    }

    // Zamanlayıcılar
    connect(m_pushPingTimer, SIGNAL(timeout()), this, SLOT(onPushPingTimeout()), Qt::UniqueConnection);
    connect(m_pushWatchdogTimer, SIGNAL(timeout()), this, SLOT(onPushWatchdogTimeout()), Qt::UniqueConnection);
}

void Service::onPushConnected()
{
    qDebug() << "[service.cpp] [PUSH] Socket baglantisi sağlandi! WebSocket Upgrade istegi gonderiliyor...";

    m_pushSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    // Benzersiz WebSocket Key
    QString secKey = QUuid::createUuid().toRfc4122().toBase64();

    QUrl url(m_url);
    QString host = url.host();

    // PORT DÜZELTMESİ: Varsayılan HTTP(80) ve HTTPS(443) dışındaki özel portlar Host header'ına eklenir
    QString hostHeader = host;
    int userPort = url.port();
    if (userPort > 0 && userPort != 80 && userPort != 443) {
        hostHeader += ":" + QString::number(userPort);
    }

    // HTTP Upgrade Request
    QString request = "GET /v1/ws HTTP/1.1\r\n"
                      "Host: " + hostHeader + "\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: " + secKey + "\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "Authorization: Bearer " + m_accessToken + "\r\n\r\n";

    m_pushSocket->write(request.toUtf8());
    m_pushSocket->flush();

    m_pushPingTimer->start(20000);
    m_pushWatchdogTimer->start(45000);
}

void Service::onPushDisconnected()
{
    qDebug() << "[service.cpp] [PUSH] Disconnected from WhatsApp API. Reconnecting in 5s...";
    //sendStatusNotification("Push Connection", "Disconnected from Server. Reconnecting...");
    m_pushPingTimer->stop();
    m_pushWatchdogTimer->stop();
    QTimer::singleShot(5000, this, SLOT(startPushConnection()));
}

void Service::onPushReadyRead()
{
    try {
        // Reset watchdog on any data receive
        m_pushWatchdogTimer->start(45000);

        m_wsBuffer.append(m_pushSocket->readAll());

        // DEBUG: İşlem öncesi buffer boyutu
        // qDebug() << "[service.cpp] [PUSH] İşlem öncesi buffer boyutu:" << m_wsBuffer.size();

        if (!m_pushHandshakeDone) {
            int idx = m_wsBuffer.indexOf("\r\n\r\n");
            if (idx != -1) {
                m_pushHandshakeDone = true;
                m_wsBuffer.remove(0, idx + 4);
                qDebug() << "[service.cpp] [PUSH] WebSocket Handshake complete!";

                // Send subscription payload
                QString sub = "{\"type\":\"subscriptions.set\",\"requestID\":\"bb10-init\",\"chatIDs\":[\"*\"]}";
                QByteArray frame;
                frame.append(char(0x81)); // FIN + Text
                frame.append(char(sub.length() | 0x80)); // Mask bit set
                frame.append(char(0)); frame.append(char(0)); frame.append(char(0)); frame.append(char(0)); // mask key (0)
                frame.append(sub.toUtf8());
                m_pushSocket->write(frame);
                m_pushSocket->flush();
            } else {
                return;
            }
        }

        // Parse WS frames
        while(m_wsBuffer.size() >= 2) {
            unsigned char byte1 = m_wsBuffer[0];
            unsigned char byte2 = m_wsBuffer[1];

            bool fin = (byte1 & 0x80) != 0;
            int opcode = byte1 & 0x0F;
            bool masked = (byte2 & 0x80) != 0;
            quint64 payloadLen = byte2 & 0x7F;

            int headerLen = 2;
            if (payloadLen == 126) {
                if (m_wsBuffer.size() < 4) return;
                quint16 len;
                memcpy(&len, m_wsBuffer.constData() + 2, 2);
                payloadLen = qFromBigEndian(len);
                headerLen += 2;
            } else if (payloadLen == 127) {
                if (m_wsBuffer.size() < 10) return;
                quint64 len;
                memcpy(&len, m_wsBuffer.constData() + 2, 8);
                payloadLen = qFromBigEndian(len);
                headerLen += 8;
            }

            QByteArray maskingKey;
            if (masked) {
                 if (m_wsBuffer.size() < headerLen + 4) return;
                 maskingKey = m_wsBuffer.mid(headerLen, 4);
                 headerLen += 4;
            }

            if ((quint64)m_wsBuffer.size() < (quint64)headerLen + payloadLen) {
                // Need more data, bekliyoruz
                return;
            }

            QByteArray payload = m_wsBuffer.mid(headerLen, payloadLen);
            if (masked) {
                 for(quint64 i = 0; i < payloadLen; ++i) {
                     payload[(int)i] = payload[(int)i] ^ maskingKey[(int)(i % 4)];
                 }
            }

            // İşlenen frame'i ana buffer'dan sil
            m_wsBuffer.remove(0, headerLen + payloadLen);

            // 1. Text (0x01) veya Binary (0x02) Frame
            if (opcode == 0x01 || opcode == 0x02) {
                if (fin) {
                    // Mesaj tek frame'den oluşuyor, doğrudan gönder
                    handlePushEvent(payload);
                } else {
                    // Fragmentation (Bölünmüş mesaj) başlangıcı
                    m_fragmentedPayload = payload;
                    m_fragmentedOpcode = opcode;
                }
            }
            // 2. Continuation Frame (0x00) - Önceki paketin devamı
            else if (opcode == 0x00) {
                m_fragmentedPayload.append(payload);
                if (fin) {
                    // Bölünmüş mesaj bitti, toplu veriyi gönder ve bufferı temizle
                    handlePushEvent(m_fragmentedPayload);
                    m_fragmentedPayload.clear();
                    m_fragmentedOpcode = 0;
                }
            }
            // 3. Ping (0x09)
            else if (opcode == 0x09) {
                QByteArray pongFrame;
                pongFrame.append(char(0x8A)); // 0x80 (FIN) | 0x0A (Pong)
                pongFrame.append(char(payloadLen | 0x80)); // Mask bit set
                // Mask key olarak 0 kullanıyorsunuz, bu yüzden payload'u XOR yapmaya gerek yok
                pongFrame.append(char(0)); pongFrame.append(char(0)); pongFrame.append(char(0)); pongFrame.append(char(0));
                pongFrame.append(payload); // RFC zorunluluğu: Ping payload'u Pong'a eklenmeli
                m_pushSocket->write(pongFrame);
                m_pushSocket->flush();
            }
            // 4. Close Frame (0x08)
            else if (opcode == 0x08) {
                qDebug() << "[service.cpp] [PUSH] Sunucu baglantiyi kapatmak istedi (Close Frame 0x08).";
                // m_pushSocket->close(); // İhtiyaca göre burada soketi kapatabilirsiniz.
            }
            // 5. Pong Frame (0x0A) - Genelde sunucudan client'a gelmez ama gelirse logla
            else if (opcode == 0x0A) {
                //qDebug() << "[service.cpp] [PUSH] Pong Frame alindi";
            }
            // 6. Bilinmeyen/İşlenmeyen Opcodeler (Hata Ayıklama için)
            else {
                qWarning() << "[service.cpp] [PUSH-DEBUG] ISLENMEYEN FRAME! Opcode:" << opcode << "Uzunluk:" << payloadLen;
            }
        }

        // DEBUG: Döngüden sonra kalan buffer boyutu (Eğer sürekli artıyorsa kısır döngü veya yanlış boyut var demektir)
        if (m_wsBuffer.size() > 0) {
            // qDebug() << "[service.cpp] [PUSH] Döngü çıkışı bekleyen buffer:" << m_wsBuffer.size() << "byte.";
        }

    } catch (const std::exception& e) {
        qWarning() << "[service.cpp] [PUSH-ERROR] Caught standard exception in onPushReadyRead:" << e.what();
    } catch (...) {
        qWarning() << "[service.cpp] [PUSH-ERROR] Caught unknown exception in onPushReadyRead!";
    }
}

void Service::onPushError(QAbstractSocket::SocketError socketError)
{
    qDebug() << "[service.cpp] [PUSH] Socket Error:" << m_pushSocket->errorString();

    bool wasConnected = (m_pushSocket->state() == QAbstractSocket::ConnectedState);
    m_pushSocket->abort();

    // Qt ONLY emits disconnected() if the socket fully reached ConnectedState.
    // If the network drops or connection is refused while *attempting* to connect,
    // disconnected() never fires, causing the background reconnect loop to die silently forever!
    // We must manually trigger a retry here if we were not fully connected.
    if (!wasConnected) {
        qDebug() << "[service.cpp] [PUSH] Connection attempt failed. Scheduling retry in 10s...";
        QTimer::singleShot(10000, this, SLOT(startPushConnection()));
    }
}

void Service::handlePushEvent(const QByteArray &payload)
{

    try {

        QString payloadStr = QString::fromUtf8(payload);
        int chunkSize = 500;
        qDebug() << "[PUSH PAYLOAD] Total size:" << payloadStr.size() << "bytes";
        for (int i = 0; i < payloadStr.size(); i += chunkSize) {
            qDebug() << "[CHUNK" << (i / chunkSize) << "]:" << payloadStr.mid(i, chunkSize);
        }
        // 1. ADIM: Hızlı JSON Ayrıştırma
        bb::data::JsonDataAccess jda;
        QVariantMap pushMap = jda.loadFromBuffer(payload).toMap();

        if (jda.hasError()) return;


        QString type = pushMap.value("type").toString();
        QString upsertedChatID = pushMap.value("chatID").toString();

        if (type == "chat.upserted" && !upsertedChatID.isEmpty()) {
            QSqlDatabase chatDb = QSqlDatabase::database("chats_db_conn");

            if (!chatDb.isOpen()) {
                return;
            }
            QSqlQuery selectOldChat(chatDb);
            selectOldChat.prepare("SELECT preview_json FROM chats WHERE id = ?");
            selectOldChat.bindValue(0, upsertedChatID);

            QString previewJsonStr="";
            if (selectOldChat.exec() && selectOldChat.next()) {
                qDebug() << "[CHAT_UPSERTED]:" << upsertedChatID;
                previewJsonStr=selectOldChat.value(0).toString();
                QMetaObject::invokeMethod(this, "processChatAsync",
                                          Qt::QueuedConnection,
                                          Q_ARG(QString, ""),
                                          Q_ARG(QString, upsertedChatID),
                                          Q_ARG(QString, previewJsonStr));

            }
        }

        QVariantList entries = pushMap.value("entries").toList();

        if (type != "message.upserted" || entries.isEmpty()) {
            return;
        }
        // DB Bağlantıları
        if (!QSqlDatabase::contains("messages_db_conn") ||
            !QSqlDatabase::contains("chats_db_conn") ||
            !QSqlDatabase::database("messages_db_conn").isOpen() ||
            !QSqlDatabase::database("chats_db_conn").isOpen()) {
            qWarning() << "[SERVICE] Veritabanı bağlantıları eksik veya kapalı! initDatabases tetikleniyor...";
            initDatabases();
        }

        QSqlDatabase msgDb = QSqlDatabase::database("messages_db_conn");
        QSqlDatabase chatDb = QSqlDatabase::database("chats_db_conn");

        if (!msgDb.isOpen() || !chatDb.isOpen()) {
            qCritical() << "[SERVICE] Kritik Hata: initDatabases çalıştırılmasına rağmen veritabanları açılamadı!";
            return;
        }

        // Transaction Başlatılıyor
        msgDb.transaction();
        chatDb.transaction();

        // OPTİMİZASYON: Sabit sorguyu döngü dışına alıyoruz (Pre-compiled)
        QSqlQuery updateChat(chatDb);
        // Eğer gelen timestamp, mevcut lastActivity'den büyükse güncelle, değilse eskisini koru!
        updateChat.prepare("UPDATE chats SET "
                           "preview_json = CASE WHEN ? >= COALESCE(lastActivity, '') THEN ? ELSE preview_json END, "
                           "lastActivity = CASE WHEN ? >= COALESCE(lastActivity, '') THEN ? ELSE lastActivity END, "
                           "unreadCount = unreadCount + ? "
                           "WHERE id = ?");

        // Bellek tahsisini azaltmak için döngü dışında tek bir buffer tanımlıyoruz
        QByteArray jsonBuffer;
        QStringList updateFields, insertColumns, insertPlaceholders;
        QVariantList fieldValues;
        // QString için reserve kullanarak performans artırılabilir
        updateFields.reserve(15);
        insertColumns.reserve(15);
        insertPlaceholders.reserve(15);
        fieldValues.reserve(15);

        foreach (const QVariant& entryVar, entries) {
            QVariantMap entry = entryVar.toMap();
            QString msgId = entry.value("id").toString();
            QString chatId = entry.value("chatID").toString();
            QString msgTimestamp = entry.value("timestamp").toString();

            if (msgTimestamp.isEmpty()) {
                msgTimestamp = getTimestamp(); // Güvenlik kalkanı: Boş gelirse mevcut zamanı kullan
            }

            if (msgId.isEmpty()) continue;

            // Listeleri her döngüde temizliyoruz (yeniden obje oluşturmaktan daha hızlıdır)
            updateFields.clear();
            insertColumns.clear();
            insertPlaceholders.clear();
            fieldValues.clear();

            // OPTİMİZASYON: Çift aramayı (contains + index) önleyen yapı
            #define ADD_MAP_FIELD(keyName, dbField, conversion) \
                do { \
                    QVariantMap::const_iterator it = entry.find(keyName); \
                    if (it != entry.constEnd()) { \
                        updateFields << (QString(dbField) + " = ?"); \
                        insertColumns << dbField; \
                        insertPlaceholders << "?"; \
                        fieldValues << (conversion); \
                    } \
                } while (0)

            // Doğrudan aktarılanlar
            ADD_MAP_FIELD("chatID", "chatID", it.value());
            ADD_MAP_FIELD("accountID", "accountID", it.value());
            ADD_MAP_FIELD("senderID", "senderID", it.value());
            ADD_MAP_FIELD("senderName", "senderName", it.value());
            ADD_MAP_FIELD("timestamp", "timestamp", it.value());
            ADD_MAP_FIELD("sortKey", "sortKey", it.value());
            ADD_MAP_FIELD("type", "type", it.value());
            ADD_MAP_FIELD("text", "text", it.value());
            ADD_MAP_FIELD("editedTimestamp", "editedTimestamp", it.value());
            ADD_MAP_FIELD("linkedMessageID", "linkedMessageID", it.value());

            // Boolean'dan Integer'a çevrilenler
            ADD_MAP_FIELD("isSender", "isSender", it.value().toBool() ? 1 : 0);
            ADD_MAP_FIELD("isUnread", "isUnread", it.value().toBool() ? 1 : 0);
            ADD_MAP_FIELD("isDeleted", "isDeleted", it.value().toBool() ? 1 : 0);

            // JSON String'e çevrilmesi gereken nesneler
            const char* jsonKeys[] = {"mentions", "seen", "attachments", "reactions"};
            for (int k = 0; k < 4; ++k) {
                QVariantMap::const_iterator it = entry.find(jsonKeys[k]);
                if (it != entry.constEnd()) {
                    jsonBuffer.clear();
                    jda.saveToBuffer(it.value(), &jsonBuffer);
                    updateFields << (QString(jsonKeys[k]) + " = ?");
                    insertColumns << jsonKeys[k];
                    insertPlaceholders << "?";
                    fieldValues << QString::fromUtf8(jsonBuffer);
                }
            }

            #undef ADD_MAP_FIELD

            if (updateFields.isEmpty()) continue;


            // -------------------------------------------------------------
            // UPDATE & INSERT İŞLEMLERİ
            // -------------------------------------------------------------
            QString updateSql = "UPDATE messages SET " + updateFields.join(", ") + " WHERE id = ?";
            QSqlQuery updateQuery(msgDb);
            updateQuery.prepare(updateSql);

            for (int i = 0; i < fieldValues.size(); ++i) {
                updateQuery.addBindValue(fieldValues.at(i));
            }
            updateQuery.addBindValue(msgId);

            /*qDebug() << "=== SQL UPDATE KONTROLÜ ===";
            qDebug() << "Sorgu:" << updateSql;
            qDebug() << "Güncellenecek Mesaj ID:" << msgId;

            for (int i = 0; i < updateFields.size(); ++i) {
                qDebug() << updateFields[i] << "Değeri:" << fieldValues.at(i).toString();
            }*/

            bool isNew = false;
            if (!updateQuery.exec()) {
                qWarning() << "[UPDATE HATA]:" << updateQuery.lastError().text();
                continue;
            }

            if (updateQuery.numRowsAffected() == 0) {
                isNew = true;
                QString insertSql = QString("INSERT INTO messages (id, %1) VALUES (?, %2)")
                                    .arg(insertColumns.join(", "))
                                    .arg(insertPlaceholders.join(", "));

                QSqlQuery insertQuery(msgDb);
                insertQuery.prepare(insertSql);
                insertQuery.addBindValue(msgId);

                for (int i = 0; i < fieldValues.size(); ++i) {
                    insertQuery.addBindValue(fieldValues.at(i));
                }

                if (!insertQuery.exec()) {
                    qWarning() << "[INSERT HATA]:" << insertQuery.lastError().text();
                    isNew = false;
                }
            }

            // Değişkenleri QVariantMap üzerinden güvenli okuma (çift arama engellendi)
            QString nText = entry.value("text").toString();
            QString nType = entry.value("type").toString();
            QString senderName = entry.value("senderName").toString();
            bool isSender = entry.value("isSender").toBool();
            bool isDeleted = entry.value("isDeleted").toBool();
            bool isEdited = entry.contains("editedTimestamp");
            bool isReaction = entry.contains("reactions");
            QString cAccountID = entry.value("accountID").toString();
            QString fileName="";

            if (nType == "FILE") {
                // 2. attachments nesnesini QVariantList (JSON Array) olarak al
                QVariantList attachments = entry.value("attachments").toList();

                if (!attachments.isEmpty()) {
                    // Dizideki ilk nesneyi QVariantMap (JSON Object) olarak al
                    QVariantMap attachmentMap = attachments.at(0).toMap();

                    // fileName anahtarının değerini oku
                    fileName = attachmentMap.value("fileName").toString();
                }
            }

            QString nPreview;
            QString fText;
            int unreadIncrement = 0;
            bool shouldNotify = false;

            if(isNew){
                m_pushType = nType;
                m_pushSender = senderName;
                shouldNotify = true;
            }

            if (isDeleted && !isNew && m_pushDeletedID != msgId) {
                nPreview = " deleted a " + nType;
                m_pushDeletedID = msgId;
            } else if (isEdited && !isNew && m_pushEditedID != msgId) {
                nPreview = " edited a message";
                m_pushEditedID = msgId;
            } else if (isNew && nType == "TEXT") {
                nPreview = nText;
                fText = nPreview;
                unreadIncrement += 1;
            } else if (isNew && entry.contains("attachments") && nType != "TEXT" && nType != "REACTION") {
                if (nType == "IMAGE") { nText = QString::fromUtf8("📷 Photo ") + nText; }
                else if (nType == "VIDEO" || nType == "VIDEO_MESSAGE") { nText = QString::fromUtf8("🎥 Video ") + nText; }
                else if (nType.contains("AUDIO") || nType.contains("VOICE")) { nText = QString::fromUtf8("🔉 Audio ") + nText; }
                else if (nType == "FILE") { if(nText==""){nText = QString::fromUtf8("📄  ") + fileName;}else{nText = QString::fromUtf8("📄  ") + fileName+ " : "+ nText; }}
                else if (nType == "LOCATION") { nText = QString::fromUtf8("📍 Location ") + nText; }
                else if (nType == "STICKER") { nText = QString::fromUtf8("🌞 Sticker ") + nText; }
                else { nText = nType + " " + nText; }
                nPreview = nText;
                fText = nPreview;
                unreadIncrement += 1;
            }

            //Gruplarda verilen reaksiyonlar m_pushType==REACTION üretmiyor. O yüzden grup reaksiyonları preview olarak ui'a gönderilmiyor.
            //qDebug()<<"isReaction:"<<isReaction<<"  m_pushType:"<<m_pushType;
            if (isReaction && m_pushType == "REACTION"){
                QVariantList rList = entry.value("reactions").toList();
                if (!rList.isEmpty()) {
                    QVariantMap rMap = rList.at(0).toMap();
                    QString participantID = rMap.value("participantID").toString();
                    QString rKey = rMap.value("reactionKey").toString();
                    QStringList loginIDs = m_settings.value("login_ids").toStringList();

                    if (nType == "IMAGE") { nText = QString::fromUtf8("📷 Photo ") + nText; }
                    else if (nType == "VIDEO" || nType == "VIDEO_MESSAGE") { nText = QString::fromUtf8("🎥 Video ") + nText; }
                    else if (nType.contains("AUDIO") || nType.contains("VOICE")) { nText = QString::fromUtf8("🔉 Audio ") + nText; }
                    else if (nType == "FILE") { if(nText==""){nText = QString::fromUtf8("📄  ") + fileName;}else{nText = QString::fromUtf8("📄  ") + fileName+ " : "+ nText; }}
                    else if (nType == "LOCATION") { nText = QString::fromUtf8("📍 Location ") + nText; }
                    else if (nType == "STICKER") { nText = QString::fromUtf8("🌞 Sticker ") + nText; }
                    else if (nType == "TEXT") { }
                    else { nText = nType + " " + nText; }

                    if (loginIDs.contains(participantID)) {
                        nPreview = "You reacted " + rKey + " to '" + nText + "'";
                    } else {
                        nPreview = m_pushSender + " reacted " + rKey + " to '" + nText + "'";
                    }
                    m_pushType = "";
                    m_pushSender = "";
                }
            }

            if(!nPreview.isEmpty()){
                QVariantMap previewMap;
                previewMap.insert("type", nType);
                previewMap.insert("isSender", isSender);
                previewMap.insert("senderName", senderName);
                previewMap.insert("text", nPreview);
                previewMap.insert("timestamp", msgTimestamp);

                jsonBuffer.clear();
                jda.saveToBuffer(previewMap, &jsonBuffer);
                QString previewJsonStr = QString::fromUtf8(jsonBuffer);

                // ==========================================
                // SOHBETLER (CHATS) İŞLEMLERİ
                // ==========================================
                updateChat.bindValue(0, msgTimestamp);      // 1. CASE: preview_json timestamp kontrolü
                updateChat.bindValue(1, previewJsonStr);    // 2. CASE: preview_json yeni değer
                updateChat.bindValue(2, msgTimestamp);      // 3. CASE: lastActivity timestamp kontrolü
                updateChat.bindValue(3, msgTimestamp);      // 4. CASE: lastActivity yeni değer
                updateChat.bindValue(4, unreadIncrement);   // Okunmamış mesaj artışı
                updateChat.bindValue(5, chatId);            // WHERE id = ?

                if (!updateChat.exec()) {
                    qWarning() << "[SERVICE] Chat DB güncelleme hatası:" << updateChat.lastError().text();
                } else if (updateChat.numRowsAffected() == 0) {
                    QMetaObject::invokeMethod(this, "processChatAsync",
                                              Qt::QueuedConnection,
                                              Q_ARG(QString, msgId),
                                              Q_ARG(QString, chatId),
                                              Q_ARG(QString, previewJsonStr));
                }
            }

            if (!fText.isEmpty() && !isSender && shouldNotify) {
                QMetaObject::invokeMethod(this, "createMessageNotification",
                      Qt::QueuedConnection,
                      Q_ARG(QString, cAccountID),
                      Q_ARG(QString, chatId),
                      Q_ARG(QString, senderName),
                      Q_ARG(QString, nType),
                      Q_ARG(QString, fText));
            }
        }

        // 5. İşlemi Onayla
        msgDb.commit();
        chatDb.commit();
        emit messagesUpdated();

        // Dosyaya yazma işlemi sonlandırılıyor
        QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
        if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
            refreshFile.close();
        }

    } catch (const std::exception& e) {
        qWarning() << "[PUSH-ERROR] handlePushEvent exception:" << e.what();
    }
}

void Service::onPushPingTimeout()
{
    if (m_pushSocket->state() == QAbstractSocket::ConnectedState && m_pushHandshakeDone) {
        QByteArray pingFrame;
        pingFrame.append(char(0x89)); // FIN + Ping
        pingFrame.append(char(0x80)); // Masked, length 0
        pingFrame.append(char(0)); pingFrame.append(char(0)); pingFrame.append(char(0)); pingFrame.append(char(0));
        m_pushSocket->write(pingFrame);
        m_pushSocket->flush();
    }
}

void Service::onPushWatchdogTimeout()
{
    qDebug() << "[service.cpp] [PUSH] Watchdog timeout after 45s of silence! Connection silently died. Reconnecting...";
    m_pushSocket->abort();  // abort() avoids TIME_WAIT overhead and cleans socket state instantly, triggering disconnected()
}

void Service::onStatusUpdateTimeout()
{
    qDebug() << "[service.cpp] [STATUS] 30 minute periodic status update triggered.";
    if (m_netConfManager && !m_netConfManager->isOnline()) {
        //sendStatusNotification("Push Status", "Still waiting for Internet connection...");
    } else {
        if (m_pushSocket->state() == QAbstractSocket::ConnectedState) {
            //sendStatusNotification("Push Status", "Push Service is active and connected.");
        } else if (m_pushSocket->state() == QAbstractSocket::ConnectingState) {
            //sendStatusNotification("Push Status", "Push Service is currently attempting to connect...");
        } else {
            //sendStatusNotification("Push Status", "Push Service disconnected. Attempting to recover...");
            // Extra safety net just in case the reconnect loop somehow broke
            startPushConnection();
        }
    }
}

QVariantMap Service::fetchChatMetadataSync(const QString &chatId)
{
    // m_url port ve şema içerdiğinden (örn: http://192.168.0.2:23373) QUrl bunu otomatik algılar.
    QUrl url(QString(m_url + "/v1/chats/%1").arg(chatId));
    QNetworkRequest request(url);

    // BAĞLANTIYI ISRARCI (KALICI) YAPMAK
    request.setRawHeader("Connection", "keep-alive");
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toLatin1());

    QNetworkReply *reply = m_networkManager->get(request);
    QEventLoop loop;
    QTimer failTimer;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(&failTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
    failTimer.start(5000);
    loop.exec();

    QVariantMap chat;
    if (failTimer.isActive() && reply->error() == QNetworkReply::NoError) {
        bb::data::JsonDataAccess jda;
        chat = jda.loadFromBuffer(reply->readAll()).toMap();
    }

    reply->deleteLater();
    return chat;
}

QString Service::convertToPlainText(const QString& rawText) {
    QString plainText = rawText;

    // 1. JSON Ayıkla
    if (plainText.trimmed().startsWith("{")) {
        bb::data::JsonDataAccess jda;
        QVariant jsonVar = jda.loadFromBuffer(plainText);
        if (!jda.hasError() && jsonVar.canConvert<QVariantMap>()) {
            QVariantMap msgMap = jsonVar.toMap();
            if (msgMap.contains("text")) {
                plainText = msgMap["text"].toString();
            }
        }
    }

    // 2. HTML Etiketlerini Kaldır (<...>)
    plainText.remove(QRegExp("<[^>]*>"));

    // 3. Markdown Karakterlerini Kaldır
    // ``` kod işaretlerini sil
    plainText.replace("```", "");
    // Kalın ve İtalik işaretçilerini (*, **, _) sil
    plainText.remove("*").remove("_");

    // 4. HTML Escape Karakterlerini Geri Çevir
    plainText.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"");

    // 5. Satır Atlamalarını Boşluğa Çevir (Opsiyonel: Tek satır önizleme için)
    plainText.replace("\n", " ").replace("<br/>", " ").replace("<br>", " ");

    return plainText.trimmed();
}

QStringList Service::getActiveAccountIDs() {
    m_settings.sync();
    QStringList activeList;

    // 1. Tüm kayıtlı ID listesini hafızadan çek
    QStringList allIDs = m_settings.value("account_ids").toStringList();

    // 2. Her ID için kontrol yap
    foreach (const QString &accID, allIDs) {
        // "state_ID" ayarını oku, değer yoksa varsayılanı true (aktif) kabul et
        bool isActive = m_settings.value("state_" + accID, true).toBool();

        // Sadece aktifse listeye ekle
        if (isActive) {
            activeList << accID;
        }
    }

    return activeList;
}

void Service::processChatAsync(const QString &msgId, const QString &chatId, const QString &previewJsonStr)
{
    try {
        // --- 1. Veritabanı Kontrolü ---
        if (!QSqlDatabase::contains("chats_db_conn") ||
            !QSqlDatabase::database("chats_db_conn").isOpen())
        {
            qWarning() << "[SERVICE] Veritabanı bağlantıları eksik veya kapalı! initDatabases tetikleniyor...";
            initDatabases();
        }

        QSqlDatabase chatDb = QSqlDatabase::database("chats_db_conn");

        if (!chatDb.isOpen()) {
            qCritical() << "[SERVICE] Kritik Hata: Veritabanları açılamadı!";
            return;
        }

        // Senkron olarak API'den chat detaylarını çekiyoruz
        QVariantMap rawChat = fetchChatMetadataSync(chatId);
        QString cTitle = "Unknown chat";
        QString cType = "UNKNOWN";
        QString cAccountID = "";
        QString participantsJsonStr = "{}";
        bb::data::JsonDataAccess jda;

        bool chatDbUpdated = false;

        if (!rawChat.isEmpty()) {
            if (!chatDb.transaction()) {
                qWarning() << "Sohbet veritabanı işlemi başlatılamadı!";
                return;
            }

            cTitle = rawChat.value("title", cTitle).toString();
            cType = rawChat.value("type", cType).toString();
            cAccountID = rawChat.value("accountID", cAccountID).toString();

            if (!rawChat.value("participants").isNull()) {
                jda.saveToBuffer(rawChat.value("participants"), &participantsJsonStr);
            }

            QString capabilitiesJsonStr;
            if (!rawChat.value("capabilities").isNull()) {
                jda.saveToBuffer(rawChat.value("capabilities"), &capabilitiesJsonStr);
            }

            // ==========================================
            // YENİ MANTIK: MEVCUT DB DURUMUNU ÖĞREN VE KIYASLA
            // ==========================================
            QString dbLastActivity = "";
            QString dbPreviewJson = "";
            QSqlQuery checkQuery(chatDb);
            checkQuery.prepare("SELECT lastActivity, preview_json FROM chats WHERE id = ?");
            checkQuery.bindValue(0, chatId);
            if (checkQuery.exec() && checkQuery.next()) {
                dbLastActivity = checkQuery.value(0).toString();
                dbPreviewJson = checkQuery.value(1).toString();
            }

            // Gelen previewJsonStr içindeki timestamp'i bul
            QString incomingPreviewTs = "";
            if (!previewJsonStr.isEmpty()) {
                QVariantMap pMap = jda.loadFromBuffer(previewJsonStr).toMap();
                incomingPreviewTs = pMap.value("timestamp").toString();
            }

            // API'den gelen lastActivity
            QString apiLastActivity = rawChat.value("lastActivity", "").toString();

            // 1. Karar: lastActivity hangisi olmalı?
            QString finalLastActivity = apiLastActivity;
            if (!dbLastActivity.isEmpty() && dbLastActivity > apiLastActivity) {
                finalLastActivity = dbLastActivity; // DB'deki daha yeniyse onu koru
            }

            // 2. Karar: preview_json hangisi olmalı?
            QString finalPreviewJson = previewJsonStr;
            bool isPreviewNewAndValid = false;

            if (!incomingPreviewTs.isEmpty() && !dbLastActivity.isEmpty() && incomingPreviewTs < dbLastActivity) {
                // Gelen preview, db'deki son aktiviteden eski! DB'dekini koru.
                finalPreviewJson = dbPreviewJson;
            } else if (previewJsonStr.isEmpty() && !dbPreviewJson.isEmpty()) {
                // Sadece metadata güncellemesi gelmiş (boş previewStr). Eski önizlemeyi silme.
                finalPreviewJson = dbPreviewJson;
            } else if (!previewJsonStr.isEmpty() && (dbPreviewJson != previewJsonStr)) {
                // Gerçekten yeni ve geçerli bir önizleme elimizde var.
                isPreviewNewAndValid = true;
            }
            // ==========================================

            QSqlQuery upsertChat(chatDb);
            if (msgId.isEmpty()) {
                upsertChat.prepare("UPDATE chats SET id = ?, localChatID = ?, accountID = ?, network = ?, title = ?, description = ?, imgURL = ?, type = ?, "
                                   "isReadOnly = ?, lastActivity = ?, unreadCount = ?, unreadMentionsCount = ?, "
                                   "lastReadMessageSortKey = ?, draft = ?, reminder = ?, snooze = ?, "
                                   "isArchived = ?, isMarkedUnread = ?, isMuted = ?, isPinned = ?, isLowPriority = ?, "
                                   "messageExpirySeconds = ?, participants_json = ?, capabilities_json = ?, preview_json = ? WHERE id = ? ");
                upsertChat.bindValue(25, chatId);
            } else {
                upsertChat.prepare("INSERT OR REPLACE INTO chats ("
                                   "id, localChatID, accountID, network, title, description, imgURL, type, "
                                   "isReadOnly, lastActivity, unreadCount, unreadMentionsCount, "
                                   "lastReadMessageSortKey, draft, reminder, snooze, "
                                   "isArchived, isMarkedUnread, isMuted, isPinned, isLowPriority, "
                                   "messageExpirySeconds, participants_json, capabilities_json, preview_json) "
                                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            }

            upsertChat.bindValue(0, rawChat.value("id", chatId).toString());
            upsertChat.bindValue(1, rawChat.value("localChatID", "").toString());
            upsertChat.bindValue(2, cAccountID);
            upsertChat.bindValue(3, rawChat.value("network", "").toString());
            upsertChat.bindValue(4, cTitle);
            upsertChat.bindValue(5, rawChat.value("description", "").toString());
            upsertChat.bindValue(6, rawChat.value("imgURL"));
            upsertChat.bindValue(7, cType);
            upsertChat.bindValue(8, rawChat.value("isReadOnly", false).toBool() ? 1 : 0);

            // OPTİMİZE EDİLMİŞ DEĞERLER BAĞLANIYOR
            upsertChat.bindValue(9, finalLastActivity);

            upsertChat.bindValue(10, rawChat.value("unreadCount", 0).toInt());
            upsertChat.bindValue(11, rawChat.value("unreadMentionsCount", 0).toInt());
            upsertChat.bindValue(12, rawChat.value("lastReadMessageSortKey", "").toString());
            upsertChat.bindValue(13, rawChat.value("draft").toString());
            upsertChat.bindValue(14, rawChat.value("reminder").toString());
            upsertChat.bindValue(15, rawChat.value("snooze").toString());
            upsertChat.bindValue(16, rawChat.value("isArchived", false).toBool() ? 1 : 0);
            upsertChat.bindValue(17, rawChat.value("isMarkedUnread", false).toBool() ? 1 : 0);
            upsertChat.bindValue(18, rawChat.value("isMuted", false).toBool() ? 1 : 0);
            upsertChat.bindValue(19, rawChat.value("isPinned", false).toBool() ? 1 : 0);
            upsertChat.bindValue(20, rawChat.value("isLowPriority", false).toBool() ? 1 : 0);
            upsertChat.bindValue(21, rawChat.value("messageExpirySeconds", 0).toInt());
            upsertChat.bindValue(22, participantsJsonStr);
            upsertChat.bindValue(23, capabilitiesJsonStr);

            // OPTİMİZE EDİLMİŞ ÖNİZLEME BAĞLANIYOR
            upsertChat.bindValue(24, finalPreviewJson);

            if (upsertChat.exec()) {
                if (upsertChat.numRowsAffected() > 0) {
                    chatDbUpdated = true;
                }
                chatDb.commit();
            } else {
                qWarning() << "[SERVICE] upsertChat sorgusu başarısız:" << upsertChat.lastError().text();
                chatDb.rollback();
            }

            // SADECE VE SADECE yepyeni/gerçek bir önizleme kaydedildiyse bildirim at!
            // Hayalet bildirim (eski önizlemelerin tekrar tetiklenmesi) engellendi.
            if (isPreviewNewAndValid && !msgId.isEmpty()) {
                QVariantMap previewMap = jda.loadFromBuffer(finalPreviewJson).toMap();
                if (!jda.hasError()) {
                    QString nText = previewMap.value("text").toString();
                    QString nType = previewMap.value("type", "TEXT").toString();
                    bool isSender = previewMap.value("isSender", false).toBool();
                    QString senderName = previewMap.value("senderName", "SenderName").toString();

                    if (!nText.isEmpty() && !isSender) {
                        createMessageNotification(cAccountID, chatId, senderName, nType, nText);
                        qDebug() << "[NOTIFY] Bildirim tetiklendi - mId:" << msgId;
                    }
                }
            }
        }

        if (chatDbUpdated) {
            qDebug() << "[SERVICE] Değişim işlendi - Chat:" << chatId << ", UI Refreshed!";
            QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
            if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
                refreshFile.close();
            }
        }

    } catch (const std::exception& e) {
        qWarning() << "[PUSH-ERROR] processChatAsync:" << e.what();
        if (QSqlDatabase::database("chats_db_conn").isOpen()) {
            QSqlDatabase::database("chats_db_conn").rollback();
        }
    }
}

QString Service::getTimestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t nowtime = tv.tv_sec;
    struct tm *nowtm = gmtime(&nowtime); // UTC zamanı için gmtime kullanıyoruz

    char buf[64];
    // Önce YYYY-MM-DDTHH:MM:SS kısmını oluşturuyoruz
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", nowtm);

    // Milisaniyeyi (tv.tv_usec / 1000) ve sonundaki 'Z' harfini ekliyoruz
    char result[80];
    sprintf(result, "%s.%03dZ", buf, (int)(tv.tv_usec / 1000));

    return QString::fromUtf8(result);
}

