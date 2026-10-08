#include "Database.hpp"
#include <QDebug>
#include <QtNetwork/QNetworkRequest>
#include <QUrl>
#include <bb/data/JsonDataAccess>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>
#include <bb/system/InvokeRequest>
#include <bb/system/InvokeManager>
#include <bb/system/InvokeTargetReply>
#include <QDateTime>
#include <bb/pim/contacts/ContactService>
#include <bb/pim/contacts/Contact>
#include <bb/pim/contacts/ContactListFilters> // Örnekteki gibi ekle
#include <bb/pim/contacts/ContactSearchFilters>
#include <QRegExp>
#include <bb/system/Clipboard>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QHttpMultiPart>
#include <QtNetwork/QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QtGui/QImageReader>

using namespace bb::data;
using namespace bb::system;
using namespace bb::pim::contacts;

Database::Database(QObject *parent) : QObject(parent)
{
    // --- GLOBAL SSL/TLS KONFİGÜRASYONU ---
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setProtocol(QSsl::SecureProtocols);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    m_initRun = m_settings.value("initRun", false).toBool();
    m_netManager = new QNetworkAccessManager(this);

    // Periodic Sync
    m_syncTimer = 0;
    m_syncReply = 0;
    m_totalMessagesDownloaded = 0;
    m_netConfManager = new QNetworkConfigurationManager(this);
    m_lastSyncTimestamp = "1970-01-01T00:00:00Z";
    m_nextCursor = "";

    if (!m_invokeManager) {
        m_invokeManager = new bb::system::InvokeManager(this);
    }

    //QTimer::singleShot(2000, this, SLOT(startSyncLoop()));

    // QNetworkAccessManager üzerindeki tüm HTTPS SSL hatalarını otomatik yok say
    connect(m_netManager, SIGNAL(sslErrors(QNetworkReply*, QList<QSslError>)),
            this, SLOT(onGlobalSslErrors(QNetworkReply*, QList<QSslError>)));

    m_invokeManager = new InvokeManager(this);
    m_chatsReply = 0;
    chatPercent = 0;
    initContext();
    m_url = m_settings.value("serverUrl").toString();
    m_accessToken = m_settings.value("accessToken").toString();
}

// REST Isteklerinde olusan SSL hatalarini otomatik bastiran slot
void Database::onGlobalSslErrors(QNetworkReply *reply, const QList<QSslError> &errors) {
    Q_UNUSED(errors);
    if (reply) {
        reply->ignoreSslErrors();
    }
}

bool Database::initRun() const {
    return m_initRun;
}

void Database::setInitRun(bool value) {
    qDebug() << "[Database.cpp] \n[DATABASE] setInitRun() called with value:" << value;
    if (m_initRun != value) {
        qDebug() << "[DATABASE] Value changed from" << m_initRun << "to" << value;
        m_initRun = value;

        // Save the value persistently
        m_settings.setValue("initRun", value);
        m_settings.sync();
        qDebug() << "[DATABASE] Saved initRun=" << value << "to QSettings";
        emit initRunChanged();
        //m_url = m_settings.value("serverUrl").toString();
        //m_accessToken=m_settings.value("accessToken").toString();
        qDebug() << "[DATABASE] Emitted initRunChanged() signal";

    } else {
        qDebug() << "[DATABASE] Value unchanged (still" << value << "), no signal emitted";
    }

}

QVariantList Database::getSettingsList() {
    QVariantList uiList;
    m_settings.sync();
    QStringList names = m_settings.value("network_names").toStringList();
    QStringList accIDs = m_settings.value("account_ids").toStringList();


    qDebug() << "[Database.cpp] --- C++: Reading names:" << names;

    for (int i = 0; i < names.size(); ++i) {
        QString name = names[i];
        QString accID = accIDs[i];

        QVariantMap row;
        row["account"] = accID;
        row["network"] = name;
        row["active"] = m_settings.value("state_" + accID, true).toBool();
        uiList.append(row);
    }
    return uiList;
}

QVariantList Database::getSelectedAccountsForMain() {
    QVariantList selectedList;
    m_settings.sync();

    QStringList accountNames = m_settings.value("network_names").toStringList();
    QStringList accountIDs = m_settings.value("account_ids").toStringList();
    //QStringList displayTexts = m_settings.value("display_texts").toStringList();

    // Return only accounts where state_X is true
    for (int i = 0; i < accountNames.size(); ++i) {
        QString name = accountNames[i];
        QString idds = accountIDs[i];
        bool isSelected = m_settings.value("state_" + idds, true).toBool();

        if (isSelected) {
            QVariantMap accountData;
            accountData["network"] = name;

            // USE THE API 'network' FIELD SAVED IN displayTexts
            //QString display = (i < displayTexts.size()) ? displayTexts[i] : name;
            //accountData["label"] = display;
            //accountData["displayText"] = display;

            accountData["accountID"] = (i < accountIDs.size()) ? accountIDs[i] : "";
            //accountData["index"] = selectedList.size();
            selectedList.append(accountData);
        }
    }

    //qDebug() << "[DATABASE] getSelectedAccounts: Labels found:" << selectedList;
    return selectedList;
}

// settings.qml'de hesaplar aktif/deaktif edildiğinde çalışır.
void Database::updateSetting(const QString &accID, bool value) {
    m_settings.setValue("state_" + accID, value);
    m_settings.sync();
    qDebug()<<"DATABASE updateSetting called!";
}

bool Database::getSetting(const QString &accID, bool defaultValue) {
    qDebug()<<"DATABASE getSettings called!";
    return m_settings.value("state_" + accID, defaultValue).toBool();
}

// hesapların listesini ve etkinlik ayarını döndürür
QStringList Database::getActiveAccountIDs() {
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

void Database::fetchAccounts() {
    try {
        QString urlStr = m_url + "/v1/accounts";
        QUrl url(urlStr);
        QNetworkRequest request(url);

        // Header ayarları
        QString authHeader = "Bearer " + m_accessToken;
        request.setRawHeader("Authorization", authHeader.toLatin1());
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // İsteği gönderiyoruz
        m_accountsReply = m_netManager->get(request);

        if (!m_accountsReply) {
            qWarning() << "[DATABASE ERROR] Failed to create network request";
            return;
        }

        // --- SERTİFİKA HATALARI İÇİN SON GÜVENCE ---
        // Protokol uyuşsa bile sertifika zincirinden patlamasını engeller
        m_accountsReply->ignoreSslErrors();

        connect(m_accountsReply, SIGNAL(finished()), this, SLOT(onAccountsFetched()));
        emit syncProgress("Fetching accounts...", chatPercent);

    } catch (const std::exception& e) {
        qWarning() << "[DATABASE ERROR] Exception:" << e.what();
    }
}

void Database::onAccountsFetched() {
    if (!m_accountsReply) return;

    // 1. Ham veriyi oku
    QByteArray responseData = m_accountsReply->readAll();

    // 2. HTTP Durum kodunu kontrol et (200, 401, 404, 500 vb.)
    int statusCode = m_accountsReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "[DATABASE] HTTP Status Code:" << statusCode;

    // 3. Ağ hatası kontrolü
    if (m_accountsReply->error() != QNetworkReply::NoError) {
        qWarning() << "[DATABASE ERROR] Network: " << m_accountsReply->errorString();
        qWarning() << "[DATABASE ERROR] Server Error Body: " << responseData; // Hata mesajının içeriğini gör
        emit connectionStatus("Network Error: "+m_accountsReply->errorString());
        m_accountsReply->deleteLater();
        m_accountsReply = 0;
        return;
    }

    // Eğer veri boşsa hiç parse etmeye çalışıp crash/hata almayalım
    if (responseData.isEmpty()) {
        qWarning() << "[DATABASE ERROR] Received empty response from server.";
        m_accountsReply->deleteLater();
        m_accountsReply = 0;
        return;
    }

    JsonDataAccess jda;
    QVariant jsonVar = jda.loadFromBuffer(responseData);

    if (jda.hasError()) {
        qWarning() << "[DATABASE ERROR] JSON Parse Error: " << jda.error().errorMessage();
        qWarning() << "[DATABASE ERROR] Invalid JSON Content was: " << responseData;
        m_accountsReply->deleteLater();
        m_accountsReply = 0;
        return;
    }

    QStringList networkNames;
    QStringList accountIDs;
    QStringList loginIDs; // YENİ: loginID listesi eklendi

    QVariantList accountsList = jsonVar.toList();

    m_settings.beginGroup("network_mapping");

    for (int i = 0; i < accountsList.size(); ++i) {
        QVariantMap obj = accountsList.at(i).toMap();
        QString accID = obj["accountID"].toString();
        QString network = obj["network"].toString();
        QString loginID = obj["loginID"].toString(); // YENİ: JSON'dan loginID çekildi

        if (!accID.isEmpty()) {
            accountIDs << accID;
            networkNames << network;
            loginIDs << loginID; // YENİ: Listeye eklendi

            // accID -> network eşleşmesi
            m_settings.setValue(accID, network);

            // YENİ: accID ile ilişkili loginID eşleşmesi (Örn: "whatsapp_loginID" -> "905356822935")
            m_settings.setValue(accID + "_loginID", loginID);

            if (!m_settings.contains("state_" + accID)) {
                m_settings.setValue("state_" + accID, true);
            }
        }
    }

    m_settings.endGroup();

    m_settings.setValue("network_names", networkNames);
    m_settings.setValue("account_ids", accountIDs);
    m_settings.setValue("login_ids", loginIDs); // YENİ: Liste ayarları kaydedildi
    m_settings.sync();

    emit settingsReady(getSettingsList());
    emit syncProgress("Accounts found:" + QString::number(accountIDs.size()), chatPercent);

    m_accountsReply->deleteLater();
    m_accountsReply = 0;
}

QString Database::getNetworkNameByAccountID(const QString &accountID) {
    if (accountID.isEmpty()) return QString();

    // Kaydettiğimiz gruptan veriyi çekiyoruz
    // value(key, defaultValue) yapısı sayesinde veri yoksa "" döner
    return m_settings.value("network_mapping/" + accountID, "").toString();
}

QVariantList Database::getChatListForAccount(const QString &accountID, int limit, int offset) {
    QVariantList chatList;
    if (accountID.isEmpty()) return chatList;

    QSqlDatabase db = QSqlDatabase::database("chats_db_conn");

    if (!db.isOpen()) {
        qCritical() << "[SERVICE] Kritik Hata: Veritabanı açık değil!";
        return chatList;
    }

    QSqlQuery query(db);
    // LIMIT ve OFFSET eklendi
    query.prepare("SELECT id, title, type, unreadCount, lastActivity, "
                  "isPinned, isArchived, isMuted, preview_json, isReadOnly "
                  "FROM chats "
                  "WHERE accountID = ? "
                  "ORDER BY isPinned DESC, lastActivity DESC "
                  "LIMIT ? OFFSET ?");

    query.addBindValue(accountID);
    query.addBindValue(limit);
    query.addBindValue(offset);

    if (!query.exec()) {
        qWarning() << "[DATABASE] Chat list query failed:" << query.lastError().text();
        return chatList;
    }

    // 3. ADIM: Veri İşleme
    QSqlRecord rec = query.record();
    const int idIdx = rec.indexOf("id");
    const int titleIdx = rec.indexOf("title");
    const int typeIdx = rec.indexOf("type");
    const int unreadIdx = rec.indexOf("unreadCount");
    const int lastActivityIdx = rec.indexOf("lastActivity");
    const int pinnedIdx = rec.indexOf("isPinned");
    const int archivedIdx = rec.indexOf("isArchived");
    const int mutedIdx = rec.indexOf("isMuted");
    const int readOnlyIdx = rec.indexOf("isReadOnly");

    // İPTAL EDİLEN: const int previewIdx = rec.indexOf("preview");
    // EKLENEN: Sütun adını preview_json olarak güncelledik
    const int previewIdx = rec.indexOf("preview_json");

    bb::data::JsonDataAccess jda; // JSON ayrıştırıcıyı döngü dışında tanımlıyoruz

    while (query.next()) {
        QString chatTitle = query.value(titleIdx).toString();
        QString timestamp = query.value(lastActivityIdx).toString();
        QString chatType = query.value(typeIdx).toString();

        QVariantMap chat;
        chat["accountID"] = accountID;
        chat["chatID"] = query.value(idIdx).toString();
        chat["chatName"] = chatTitle;
        chat["unreadCount"] = query.value(unreadIdx).toInt();

        chat["isPinned"] = query.value(pinnedIdx).toBool();
        chat["isArchived"] = query.value(archivedIdx).toBool();
        chat["isMuted"] = query.value(mutedIdx).toBool();
        chat["isReadOnly"] = query.value(readOnlyIdx).toBool();

        chat["lastMessageTime"] = formatTimeForDisplay(timestamp);
        chat["timestamp"] = timestamp;


        // --- YENİ PREVIEW JSON İŞLEME MANTIĞI ---
        QString previewJsonStr = query.value(previewIdx).toString();
        QString lastMsgText = "";
        QString lastMsgType = "TEXT";
        bool isSender = false;
        QString senderName = "";

        //qDebug()<<"[DATABASE] previewJsonStr"<<previewJsonStr;

        if (!previewJsonStr.isEmpty()) {
            QVariantMap previewMap = jda.loadFromBuffer(previewJsonStr).toMap();
            if (!jda.hasError()) {
                lastMsgText = previewMap.value("text").toString();
                lastMsgType = previewMap.value("type", "TEXT").toString();
                isSender = previewMap.value("isSender", false).toBool();
                senderName = previewMap.value("senderName", "SenderName").toString();
            }
        }

        QString displayBody = convertToPlainText(lastMsgText);
        QString plainBody;

        if (chatType == "group") {
            QString lowerTitle = chatTitle.toLower();
            if (lowerTitle.contains("status") || lowerTitle.contains("broadcast")) {
                chatType = "status";
            } else if (senderName.contains("bot", Qt::CaseInsensitive)) { // Küçük/büyük harf duyarsız yapıldı
                chatType = "channel";
            }
        }

        if (isSender){
            senderName = "you";
        }

        if (chatType != "single" && chatType != "channel" && !displayBody.isEmpty()){
            // Grup sohbeti mantığı
            plainBody = QString("%1: %2").arg(senderName.toUpper()).arg(displayBody);
        }else {
            plainBody = displayBody;
        }

        //bool isReaction=displayBody.contains("reacted");
        bool isDel=displayBody.contains("deleted");
        bool isEdit=displayBody.contains("edited");

        if (isDel || isEdit){
            plainBody = QString("%1 %2").arg(senderName.toUpper()).arg(displayBody);
        }

        if(isDel && chatType == "group"){
            plainBody = " A message is deleted";
        }

        chat["displayLower"] = plainBody;
        chat["displayUpper"] = chatTitle;
        chat["avatarInitial"] = getInitial(chatTitle);

        if (chatType == "status") {
            chat["displayUpper"] = "Status Update";
            chat["avatarInitial"] = "S";
        }

        chat["chatType"] = chatType; // Manipüle edilmiş type'ı sonradan atıyoruz

        chatList.append(chat);
    }

    qDebug() << "[DATABASE] Loaded" << chatList.size() << "chats for account:" << accountID;
    return chatList;
}

void Database::syncChats(const QString &cursor, QString callbackAction) {
    if (m_chatsReply && cursor.isEmpty()) return;

    // İlk sayfa çağrıldığında kuyruğu tazeleyelim
    if (cursor.isEmpty()) {
        m_syncChatQueue.clear();
    }

    QUrl url(m_url + "/v1/chats");
    url.addQueryItem("limit", "25");

    if (!cursor.isEmpty() && cursor != "null") {
        url.addQueryItem("cursor", cursor);
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toLatin1());

    m_chatsReply = m_netManager->get(request);
    m_chatsReply->setProperty("callbackAction", callbackAction);

    connect(m_chatsReply, SIGNAL(finished()), this, SLOT(onChatsFetched()));

    qDebug() << "[DATABASE] Global chat sync started. Cursor:" << (cursor.isEmpty() ? "FIRST_PAGE" : cursor);
}

void Database::onChatsFetched() {
    if (!m_chatsReply) return;

    QString callback = m_chatsReply->property("callbackAction").toString();
    QByteArray responseData = m_chatsReply->readAll();

    m_chatsReply->deleteLater();
    m_chatsReply = 0;

    bb::data::JsonDataAccess jda;
    QVariantMap root = jda.loadFromBuffer(responseData).toMap();

    if (jda.hasError()) {
        qWarning() << "[DATABASE] JSON Parse Error in onChatsFetched";
        return;
    }

    QVariantList items = root["items"].toList();
    bool hasMore = root["hasMore"].toBool();
    QString oldestCursor = root["oldestCursor"].toString();

    QVariantList selectedAccounts = getSelectedAccountsForMain();
    QStringList activeAccountIDs;
    foreach (const QVariant &acc, selectedAccounts) {
        activeAccountIDs << acc.toMap()["accountID"].toString();
    }

    qDebug() << "[DATABASE] Gelen toplam:" << items.size() << "Filtrelenecek aktif hesaplar:" << activeAccountIDs;

    if (!items.isEmpty()) {
        foreach (const QVariant &v, items) {
            QVariantMap chat = v.toMap();
            QString chatAccountID = chat["accountID"].toString();

            if (!activeAccountIDs.contains(chatAccountID)) {
                continue; // Hesap pasifse atla
            }

            QString chatId = chat["id"].toString();
            if (!chatId.isEmpty()) {
                QVariantMap chatNode;
                chatNode["chatId"] = chatId;
                chatNode["accountId"] = chatAccountID;
                m_syncChatQueue.append(chatNode); // Doğrudan kuyruğa ekle
            }
        }
    }

    // --- PAGINATION & CALLBACK KARARLARI ---
    if (hasMore && !oldestCursor.isEmpty() && !items.isEmpty()) {
        // Sonraki sayfayı iste (Kuyruk birikmeye devam eder)
        syncChats(oldestCursor, callback);
    } else {
        // Tüm sayfalar bitti, kuyruk tamamen doldu. İşlemi başlatabiliriz.
        qDebug() << "[DATABASE] Tum sayfalama tamamlandi. Toplam Kuyruk Boyutu:" << m_syncChatQueue.size();

        if (callback == "PREPARE_QUEUE") {
            prepareMessageSyncQueue();
        } else if (callback == "JUST_REFRESH") {
            emit dataRefreshRequested();
        }
    }
}

void Database::initializeDatabaseSync() {
    qDebug() << "[DATABASE] --- Starting Global Centralized Sync ---";

    QString utcTimestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // 2. Settings içerisine kaydet
    m_settings.setValue("lastSyncTimestamp", utcTimestamp);
    m_settings.sync();

    // 3. Doğrulamak için debug çıktısı alalım
    qDebug() << "Kaydedilen UTC Zamanı:" << utcTimestamp;


    pauseSyncing();

    // 1. ADIM: Bağlantıları sadece kapatma, Qt listesinden TAMAMEN SİL
    {
        // Kapsam (scope) oluşturuyoruz ki 'db' nesnesi yok olsun,
        // yoksa removeDatabase 'database is still in use' hatası verir.
        if (QSqlDatabase::contains("messages_db_conn")) {
            QSqlDatabase::database("messages_db_conn").close();
        }
        if (QSqlDatabase::contains("chats_db_conn")) {
            QSqlDatabase::database("chats_db_conn").close();
        }
    }
    // Bağlantı isimlerini siliyoruz
    QSqlDatabase::removeDatabase("messages_db_conn");
    QSqlDatabase::removeDatabase("chats_db_conn");

    QDir sharedDir("/accounts/1000/shared/misc/Beeper");
    if (sharedDir.exists()) {
        QStringList filters;
        filters << "*.db" << "*.db-shm" << "*.db-wal";

        QStringList fileList = sharedDir.entryList(filters, QDir::Files);
        qDebug() << "Silinecek dosya sayisi:" << fileList.count();

        foreach(QString file, fileList) {
            // remove() fonksiyonunun sonucunu alıyoruz
            bool success = sharedDir.remove(file);

            if (success) {
                qDebug() << "Basariyla silindi:" << file;
            } else {
                qWarning() << "DOSYA SILINEMEDI (Kilitli veya izin yok):" << file;
            }
        }

        // Son kontrol: Klasörde hala hedef dosyalardan kalmış mı?
        QStringList remainingFiles = sharedDir.entryList(filters, QDir::Files);
        if (remainingFiles.isEmpty()) {
            qDebug() << "Temizlik tamamlandi. Hedeflenen tum dosyalar silindi.";
        } else {
            qWarning() << "Temizlik tamamlanamadi! Kalan dosya sayisi:" << remainingFiles.count();
        }
    } else {
        qWarning() << "Hedef klasor bulunamadi:" << sharedDir.absolutePath();
    }

    // 3. ADIM: initContext artık 'contains' kontrolünden geçecek ve tabloları OLUŞTURACAK
    if (!initContext()) {
        qCritical() << "[DATABASE] Re-init failed!";
        return;
    }

    qDebug() << "[DATABASE] Tablolar sıfırlandı ve yeniden oluşturuldu.";

    emit syncProgress("Fetching all chats from Beeper...", chatPercent);
    syncChats("", "PREPARE_QUEUE");
}

void Database::prepareMessageSyncQueue() {
    qDebug() << "[DATABASE] Central Message Queue is being prepared...";

    totalChat = m_syncChatQueue.size();
    qDebug() << "[DATABASE] Total chats added to sync queue:" << totalChat;

    // Karar Mekanizması
    if (m_syncChatQueue.isEmpty()) {
        qDebug() << "[DATABASE] No chats found to sync messages. Finishing...";
        finishDatabaseSync();
    } else {
        emit syncProgress(QString("Found %1 chats. Downloading messages...").arg(m_syncChatQueue.size()), chatPercent);
        m_testLoopCounter = 0;
        chatPercent = 0;

        // Mesaj indirme döngüsünü başlat
        processNextChatFromQueue();
    }
}

void Database::processNextChatFromQueue() {
    if (m_syncChatQueue.isEmpty()) {
        qDebug() << "[DATABASE] All messages for all chats have been downloaded.";
        finishDatabaseSync();
        return;
    }

    QVariantMap currentSyncNode = m_syncChatQueue.takeFirst().toMap();
    QString chatId = currentSyncNode["chatId"].toString();
    QString accId = currentSyncNode["accountId"].toString();
    m_testLoopCounter++;
    qDebug() << "[DATABASE] Syncing messages for chat:" << chatId << "-" << accId << "-" << m_testLoopCounter;

    fetchInitialMessages(chatId, "");
    fetchChatsDetails(chatId);
}

void Database::fetchInitialMessages(QString chatId, QString cursor) {
    QUrl url(m_url + QString("/v1/chats/%1/messages").arg(chatId));
    url.addQueryItem("limit", "50");

    if (!cursor.isEmpty() && cursor != "null") {
        url.addQueryItem("cursor", cursor);
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toLatin1());

    m_initialSyncReply = m_netManager->get(request);
    m_initialSyncReply->setProperty("currentChatId", chatId);

    connect(m_initialSyncReply, SIGNAL(finished()), this, SLOT(onInitialMessagesFetched()));
}

void Database::onInitialMessagesFetched() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString chatId = reply->property("currentChatId").toString();
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();

    reply->deleteLater();
    if (reply == m_initialSyncReply) m_initialSyncReply = 0;

    if (httpStatus != 200) {
        qWarning() << "[DATABASE] Error fetching messages for" << chatId << "HTTP:" << httpStatus;
        processNextChatFromQueue();
        return;
    }

    bb::data::JsonDataAccess jda;
    QVariantMap root = jda.loadFromBuffer(responseData).toMap();
    QVariantList messages = root["items"].toList();

    if (!messages.isEmpty()) {
        // --- MERKEZİ DB BAĞLANTISINI KULLAN ---
        QSqlDatabase db = QSqlDatabase::database("messages_db_conn");
        if (db.isOpen()) {
            db.transaction();
            foreach (const QVariant &v, messages) {
                QVariantMap msg = v.toMap();

                QSqlQuery ins(db);
                ins.prepare("INSERT OR REPLACE INTO messages (id, chatID, accountID, senderID, senderName, "
                            "timestamp, sortKey, type, text, isSender, isUnread, attachments, "
                            "reactions, editedTimestamp, isDeleted, linkedMessageID, mentions, seen) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? ,?, ?)");

                ins.addBindValue(msg["id"].toString());
                ins.addBindValue(chatId);
                ins.addBindValue(msg["accountID"].toString());
                ins.addBindValue(msg["senderID"].toString());
                ins.addBindValue(msg["senderName"].toString());
                ins.addBindValue(msg["timestamp"].toString()); // UTC olarak kalıyor
                ins.addBindValue(msg["sortKey"].toString());
                ins.addBindValue(msg["type"].toString());
                ins.addBindValue(msg["text"].toString());
                ins.addBindValue(msg["isSender"].toBool() ? 1 : 0);
                ins.addBindValue(msg["isUnread"].toBool() ? 1 : 0);

                // Ekleri JSON string olarak kaydet
                QString attachmentsJson = "";
                QVariantList atts = msg["attachments"].toList();
                if (!atts.isEmpty()) {
                    QByteArray buffer;
                    jda.saveToBuffer(atts, &buffer);
                    attachmentsJson = QString::fromUtf8(buffer);
                }
                ins.addBindValue(attachmentsJson);

                QString reactionsJson = "[]";
                QVariantList rawReactions = msg["reactions"].toList();
                if (!rawReactions.isEmpty()) {
                    QByteArray buffer;
                    jda.saveToBuffer(rawReactions, &buffer);
                    reactionsJson = QString::fromUtf8(buffer);
                }
                ins.addBindValue(reactionsJson);

                // Diğer standart değerler
                ins.addBindValue(msg["editedTimestamp"].toString());
                ins.addBindValue(msg["isDeleted"].toBool() ? 1 : 0);
                ins.addBindValue(msg["linkedMessageID"].toString());

                // --- 2. Mentions ---
                QString mentionsJson = "[]";
                QVariantList mentionsList = msg["mentions"].toList();
                if (!mentionsList.isEmpty()) {
                    QByteArray buffer;
                    jda.saveToBuffer(mentionsList, &buffer);
                    mentionsJson = QString::fromUtf8(buffer);
                }
                ins.addBindValue(mentionsJson);

                // --- 3. Seen (Sorun Yaşadığınız Kısım) ---
                QString seenJson = "{}";
                QVariantMap seenMap = msg["seen"].toMap();
                if (!seenMap.isEmpty()) {
                    QByteArray buffer;
                    jda.saveToBuffer(seenMap, &buffer);
                    seenJson = QString::fromUtf8(buffer);
                }
                ins.addBindValue(seenJson);

                ins.exec();
            }
            db.commit();

            m_totalMessagesDownloaded += messages.size();
            chatPercent = (100 * m_testLoopCounter) / (totalChat+1);
            emit syncProgress(QString("Downloaded %1 messages...").arg(m_totalMessagesDownloaded), chatPercent);
        }
    }

    bool hasMore = root["hasMore"].toBool();
    QString oldestCursor = root["oldestCursor"].toString();

    if (hasMore && !oldestCursor.isEmpty() && oldestCursor != "null") {
        fetchInitialMessages(chatId, oldestCursor);
    } else {
        processNextChatFromQueue();
    }
}

void Database::fetchChatsDetails(QString chatId) {
    QUrl url(QString(m_url+"/v1/chats/%1").arg(chatId));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toLatin1());

    m_chatsDetailsReply = m_netManager->get(request);
    m_chatsDetailsReply->setProperty("currentChatId", chatId);

    connect(m_chatsDetailsReply, SIGNAL(finished()), this, SLOT(onChatsDetailsFetched()));
}

void Database::onChatsDetailsFetched() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString chatId = reply->property("currentChatId").toString();
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();

    reply->deleteLater();
    if (reply == m_chatsDetailsReply) m_chatsDetailsReply = 0;

    if (httpStatus != 200) {
        qWarning() << "[DATABASE] Error fetching messages for" << chatId << "HTTP:" << httpStatus;
        processNextChatFromQueue();
        return;
    }

    bb::data::JsonDataAccess jda;
    QVariantMap rawChat = jda.loadFromBuffer(responseData).toMap();
    QSqlDatabase chatDb = QSqlDatabase::database("chats_db_conn");

    QSqlQuery upsertChat(chatDb);
            upsertChat.prepare("INSERT OR REPLACE INTO chats ("
                               "id, localChatID, accountID, network, title, description, imgURL, type, "
                               "isReadOnly, lastActivity, unreadCount, unreadMentionsCount, "
                               "lastReadMessageSortKey, draft, reminder, snooze, "
                               "isArchived, isMarkedUnread, isMuted, isPinned, isLowPriority, "
                               "messageExpirySeconds, participants_json, capabilities_json) "
                               "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    // --- VERİYİ SQLITE FORMATINA DÖNÜŞTÜR ---
    QVariantMap chat;
    chat["localChatID"]            = rawChat.value("localChatID", "").toString();
    chat["accountID"]              = rawChat.value("accountID", "").toString();
    chat["network"]                = rawChat.value("network", "").toString();
    chat["title"]                  = rawChat.value("title", "").toString();
    chat["description"]            = rawChat.value("description", "").toString();
    chat["imgURL"]                 = rawChat.value("imgURL");
    chat["type"]                   = rawChat.value("type", "").toString();
    chat["isReadOnly"]             = rawChat.value("isReadOnly", false).toBool() ? 1 : 0;
    chat["lastActivity"]           = rawChat.value("lastActivity", "").toString();
    chat["unreadCount"]            = rawChat.value("unreadCount", 0).toInt();
    chat["unreadMentionsCount"]    = rawChat.value("unreadMentionsCount", 0).toInt();
    chat["lastReadMessageSortKey"] = rawChat.value("lastReadMessageSortKey", "").toString();
    chat["draft"]                  = rawChat.value("draft").isNull() ? "" : rawChat.value("draft").toString();
    chat["reminder"]               = rawChat.value("reminder").isNull() ? "" : rawChat.value("reminder").toString();
    chat["snooze"]                 = rawChat.value("snooze").isNull() ? "" : rawChat.value("snooze").toString();

    // Boolean bayraklar
    chat["isArchived"]     = rawChat.value("isArchived", false).toBool() ? 1 : 0;
    chat["isMarkedUnread"] = rawChat.value("isMarkedUnread", false).toBool() ? 1 : 0;
    chat["isMuted"]        = rawChat.value("isMuted", false).toBool() ? 1 : 0;
    chat["isPinned"]       = rawChat.value("isPinned", false).toBool() ? 1 : 0;
    chat["isLowPriority"]  = rawChat.value("isLowPriority", false).toBool() ? 1 : 0;

    chat["messageExpirySeconds"] = rawChat.value("messageExpirySeconds").isNull() ? 0 : rawChat.value("messageExpirySeconds").toInt();

    // Participants objesini JSON string'e çevir
    QString participantsJsonStr = "[]";
    if (!rawChat.value("participants").isNull()) {
        jda.saveToBuffer(rawChat.value("participants"), &participantsJsonStr);
    }
    chat["participants_json"] = participantsJsonStr;

    // Capabilities objesini JSON string'e çevir
    QString capabilitiesJsonStr = "{}";
    if (!rawChat.value("capabilities").isNull()) {
        jda.saveToBuffer(rawChat.value("capabilities"), &capabilitiesJsonStr);
    }
    chat["capabilities_json"] = capabilitiesJsonStr;

    // --- UPSERT BIND İŞLEMLERİ ---
    upsertChat.bindValue(0, chatId);
    upsertChat.bindValue(1, chat["localChatID"].toString());
    upsertChat.bindValue(2, chat["accountID"].toString());
    upsertChat.bindValue(3, chat["network"].toString());
    upsertChat.bindValue(4, chat["title"].toString());
    upsertChat.bindValue(5, chat["description"].toString());
    upsertChat.bindValue(6, chat["imgURL"]);
    upsertChat.bindValue(7, chat["type"].toString());
    upsertChat.bindValue(8, chat["isReadOnly"].toInt());
    upsertChat.bindValue(9, chat["lastActivity"].toString());
    upsertChat.bindValue(10, chat["unreadCount"].toInt());
    upsertChat.bindValue(11, chat["unreadMentionsCount"].toInt());
    upsertChat.bindValue(12, chat["lastReadMessageSortKey"].toString());
    upsertChat.bindValue(13, chat["draft"].toString());
    upsertChat.bindValue(14, chat["reminder"].toString());
    upsertChat.bindValue(15, chat["snooze"].toString());
    upsertChat.bindValue(16, chat["isArchived"].toInt());
    upsertChat.bindValue(17, chat["isMarkedUnread"].toInt());
    upsertChat.bindValue(18, chat["isMuted"].toInt());
    upsertChat.bindValue(19, chat["isPinned"].toInt());
    upsertChat.bindValue(20, chat["isLowPriority"].toInt());
    upsertChat.bindValue(21, chat["messageExpirySeconds"].toInt());
    upsertChat.bindValue(22, chat["participants_json"].toString());
    upsertChat.bindValue(23, chat["capabilities_json"].toString());

    if (!upsertChat.exec()) {
        qWarning() << "[SERVICE] Chat DB yazma hatası:" << upsertChat.lastError().text();
    }

    chatDb.commit();
}

void Database::finishDatabaseSync() {
    emit syncProgress("Finalizing database...", chatPercent);

    // 1. Servis senkronizasyonunu tekrar başlat
    resumeSyncing();

    // 2. İlk kurulumun bittiğini kaydet
    m_settings.setValue("initRun", true);
    m_settings.sync();
    m_initRun = true;

    bb::system::InvokeRequest request;
    request.setTarget("com.example.BerryBridgeUIService");
    request.setAction("com.example.BerryBridgeUIService.INIT_UPDATE");
    m_invokeManager->invoke(request);

    // 3. UI tarafına her şeyin hazır olduğunu bildir
    emit syncProgress("Complete! Total: " + QString::number(m_totalMessagesDownloaded), chatPercent);
    emit dataRefreshRequested();
    emit syncComplete();

    qDebug() << "[DATABASE] Initial Sync Finished. Total messages:" << m_totalMessagesDownloaded;

    // Not: db.close() yapmıyoruz, çünkü uygulama açık kaldıkça mesaj listesi için
    // bu bağlantılar kullanılmaya devam edecek.
}

/*void Database::saveMessagesToDb(const QVariantList &messages) {
    if (messages.isEmpty()) return;

    // 1. ADIM: initContext ile oluşturulan kalıcı bağlantıyı al
    // Artık her seferinde addDatabase/removeDatabase ile uğraşmıyoruz.
    QSqlDatabase db = QSqlDatabase::database("messages_db_conn");

    if (!db.isOpen()) {
        qWarning() << "[DATABASE] messages_db_conn is not open, trying to recover...";
        if (!db.open()) return;
    }

    // 2. ADIM: Transaction Başlat (Performans için KRİTİK)
    // SQLite'ta 50-100 kaydı transaction olmadan atmak, diski 50-100 kez kilitleyip açar.
    db.transaction();

    bb::data::JsonDataAccess jda;
    QSqlQuery ins(db); // Sorgu nesnesini döngü dışında bir kez oluşturmak daha verimlidir.

    ins.prepare("INSERT OR REPLACE INTO messages ("
                "id, chatID, accountID, senderID, senderName, "
                "timestamp, sortKey, type, text, isSender, isUnread, attachments) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    foreach (const QVariant &v, messages) {
        QVariantMap msg = v.toMap();

        ins.addBindValue(msg["id"].toString());
        ins.addBindValue(msg["chatID"].toString());
        ins.addBindValue(msg["accountID"].toString());
        ins.addBindValue(msg["senderID"].toString());
        ins.addBindValue(msg["senderName"].toString());
        ins.addBindValue(msg["timestamp"].toString());
        ins.addBindValue(msg["sortKey"].toString()); // Sayısal değer olarak saklamak sıralama için iyidir
        ins.addBindValue(msg["type"].toString());
        ins.addBindValue(msg["text"].toString());
        ins.addBindValue(msg["isSender"].toBool() ? 1 : 0);
        ins.addBindValue(msg["isUnread"].toBool() ? 1 : 0);

        // Attachment İşleme
        QString attachmentsJson = "";
        QVariantList atts = msg["attachments"].toList();
        if (!atts.isEmpty()) {
            QByteArray buffer;
            jda.saveToBuffer(atts, &buffer);
            attachmentsJson = QString::fromUtf8(buffer);
        }
        ins.addBindValue(attachmentsJson);

        if (!ins.exec()) {
            qWarning() << "[DATABASE] Insert Error:" << ins.lastError().text();
        }
    }

    // 3. ADIM: Tek seferde diske yaz
    if (db.commit()) {
        // qDebug() << "[DATABASE] Batch save successful:" << messages.size() << "messages.";
    } else {
        qCritical() << "[DATABASE] Transaction commit failed!";
        db.rollback();
    }

    // db.close() YAPMIYORUZ. Bağlantı messages_db_conn adıyla havuzda kalmaya devam ediyor.
}*/

bool Database::removeDirectoryRecursively(const QDir &dir) {
    bool result = true;
    if (dir.exists()) {
        QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::AllDirs | QDir::Files, QDir::DirsFirst);
        foreach (QFileInfo info, list) {
            if (info.isDir()) {
                result = removeDirectoryRecursively(QDir(info.absoluteFilePath()));
            } else {
                result = QFile::remove(info.absoluteFilePath());
            }
            if (!result) return false;
        }
        result = dir.rmdir(dir.absolutePath());
    }
    return result;
}

// Helper: Format timestamp for display (show time if today, date if older)
QString Database::formatTimeForDisplay(const QString &timestamp) {
    if (timestamp.isEmpty()) return "";

    // 1. Parse ve UTC Belirleme (Tek adımda)
    QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
    if (!dt.isValid()) return timestamp;

    dt.setTimeSpec(Qt::UTC); // Gelen ham verinin UTC olduğunu işaretle
    QDateTime localDt = dt.toLocalTime(); // Cihazın yerel saatine tek seferde dönüştür

    // 2. Tarih Nesnelerini Hazırla
    QDate itemDate = localDt.date();
    QDate today = QDate::currentDate();

    // 3. Mantık Karşılaştırmaları
    // Bugün ise: HH:mm
    if (itemDate == today) {
        return localDt.time().toString("HH:mm");
    }

    // Dün ise: "Yesterday"
    if (itemDate == today.addDays(-1)) {
        return "Yesterday";
    }

    // Daha eski ise: Kullanıcının yerel formatı (Örn: 07.05.2026)
    return itemDate.toString(Qt::DefaultLocaleShortDate);
}

// Helper: Extract first letter from name for avatar
QString Database::getInitial(const QString &name) {
    if (name.isEmpty()) return "?";
    return name.left(1).toUpper();
}


// --- SOHBETİ OKUNDU İŞARETLEME ---
void Database::markChatAsRead(const QString &accountID, const QString &chatID) {
    if (chatID.trimmed().isEmpty()) return;

    qDebug() << "[DATABASE] markChatAsRead called for" << accountID << "chat" << chatID;

    // 1. REST İSTEĞİ (Doğrudan Database NWM üzerinden)
    QUrl url(m_url + "/v1/chats/" + chatID + "/read");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toLatin1());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_netManager->post(request, "{}");
    if (reply) {
        connect(reply, SIGNAL(finished()), this, SLOT(onChatMarkedRead()));
    }

    // 2. YEREL VERİTABANI GÜNCELLEMELERİ
    bool changed = false;

    QSqlDatabase db = QSqlDatabase::database("chats_db_conn");
    if (db.isOpen()) {
        QSqlQuery query(db);
        query.prepare("UPDATE chats SET unreadCount = 0 WHERE id = ? AND unreadCount > 0");
        query.addBindValue(chatID);

        if (query.exec() && query.numRowsAffected() > 0) {
            qDebug() << "[DATABASE] Local unreadCount reset to 0 for chat:" << chatID;
            changed = true;
        }
    }

    QSqlDatabase mdb = QSqlDatabase::database("messages_db_conn");
    if (mdb.isOpen()) {
        QSqlQuery mQuery(mdb);
        mQuery.prepare("UPDATE messages SET isUnread = 0 WHERE chatID = ? AND isUnread = 1");
        mQuery.addBindValue(chatID);
        if (mQuery.exec() && mQuery.numRowsAffected() > 0) {
            changed = true;
        }
    }

    if (changed) {
        emit dataRefreshRequested();

        QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
        if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
            refreshFile.close();
        }

        emit chatsUpdated(accountID);
    }
}

// --- SOHBETİ OKUNMADI İŞARETLEME ---
void Database::markChatAsUnread(const QString &accountID, const QString &chatID) {
    if (chatID.trimmed().isEmpty()) return;

    qDebug() << "[DATABASE] markChatAsUnread called for" << accountID << "chat" << chatID;

    // 1. REST İSTEĞİ (Doğrudan Database NWM üzerinden)
    QUrl url(m_url + "/v1/chats/" + chatID + "/unread");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toLatin1());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_netManager->post(request, "{}");
    if (reply) {
        connect(reply, SIGNAL(finished()), this, SLOT(onChatMarkedUnread()));
    }

    // 2. YEREL VERİTABANI GÜNCELLEMELERİ
    bool changed = false;

    QSqlDatabase db = QSqlDatabase::database("chats_db_conn");
    if (db.isOpen()) {
        QSqlQuery query(db);
        query.prepare("UPDATE chats SET unreadCount = 1 WHERE id = ?");
        query.addBindValue(chatID);

        if (query.exec() && query.numRowsAffected() > 0) {
            qDebug() << "[DATABASE] Local unreadCount reset to 1 for chat:" << chatID;
            changed = true;
        }
    }

    QSqlDatabase mdb = QSqlDatabase::database("messages_db_conn");
    if (mdb.isOpen()) {
        QSqlQuery mQuery(mdb);
        mQuery.prepare("UPDATE messages SET isUnread = 1 WHERE chatID = ? AND isUnread = 0");
        mQuery.addBindValue(chatID);
        if (mQuery.exec() && mQuery.numRowsAffected() > 0) {
            changed = true;
        }
    }

    if (changed) {
        emit dataRefreshRequested();

        QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
        if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
            refreshFile.close();
        }

        emit chatsUpdated(accountID);
    }
}

// --- REST YANIT SLOTLARI ---
void Database::onChatMarkedRead() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "[DATABASE] Chat marked as read successfully! Status:" << httpStatus;
        emit chatMarkedReadSuccessfully();
    } else {
        qWarning() << "[DATABASE ERROR] Failed to mark chat as read.";
        qWarning() << "  HTTP Status:" << httpStatus;
        qWarning() << "  Error String:" << reply->errorString();

        QByteArray errorBody = reply->readAll();
        if (!errorBody.isEmpty()) {
            qWarning() << "  Server Response:" << QString::fromUtf8(errorBody);
        }
    }

    reply->deleteLater();
}

void Database::onChatMarkedUnread() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "[DATABASE] Chat marked as unread successfully! Status:" << httpStatus;
        emit chatMarkedUnreadSuccessfully();
    } else {
        qWarning() << "[DATABASE ERROR] Failed to mark chat as unread.";
        qWarning() << "  HTTP Status:" << httpStatus;
        qWarning() << "  Error String:" << reply->errorString();

        QByteArray errorBody = reply->readAll();
        if (!errorBody.isEmpty()) {
            qWarning() << "  Server Response:" << QString::fromUtf8(errorBody);
        }
    }

    reply->deleteLater();
}

QVariantList Database::getMessagesForChat(const QString &accountID, const QString &chatID, const QString &targetMsgId, int limit, int offset) {
    QVariantList messages;
    if (accountID.isEmpty() || chatID.isEmpty()) return messages;

    // 1. ADIM: Kalıcı bağlantıyı kullan
    QSqlDatabase db = QSqlDatabase::database("messages_db_conn");
    if (!db.isOpen()) {
        if (!db.open()) return messages;
    }

    // LoginID listesini okuma
    QStringList loginIDs = m_settings.value("login_ids").toStringList();

    // 2. ADIM: Sorguyu hazırla (Yeni sütunlar eklendi: linkedMessageID, mentions, seen)
    QSqlQuery query(db);

    if (!targetMsgId.isEmpty()) {
        query.prepare(
            "SELECT * FROM ("
            "  SELECT senderName, text, timestamp, type, isSender, isUnread, id, attachments, reactions, linkedMessageID, seen, editedTimestamp, isDeleted "
            "  FROM messages WHERE chatID = ? AND (type IS NULL OR type != 'REACTION') "
            "  AND timestamp > (SELECT timestamp FROM messages WHERE id = ?) "
            "  ORDER BY timestamp ASC LIMIT 48"
            ") UNION ALL "
            "SELECT senderName, text, timestamp, type, isSender, isUnread, id, attachments, reactions, linkedMessageID, seen, editedTimestamp, isDeleted "
            "FROM messages WHERE id = ? AND (type IS NULL OR type != 'REACTION') "
            "UNION ALL "
            "SELECT * FROM ("
            "  SELECT senderName, text, timestamp, type, isSender, isUnread, id, attachments, reactions, linkedMessageID, seen, editedTimestamp, isDeleted "
            "  FROM messages WHERE chatID = ? AND (type IS NULL OR type != 'REACTION') "
            "  AND timestamp < (SELECT timestamp FROM messages WHERE id = ?) "
            "  ORDER BY timestamp DESC LIMIT 48"
            ") ORDER BY timestamp DESC"
        );

        query.addBindValue(chatID);
        query.addBindValue(targetMsgId);
        query.addBindValue(targetMsgId);
        query.addBindValue(chatID);
        query.addBindValue(targetMsgId);
    } else {
        //QSqlQuery query(db);
        query.prepare("SELECT senderName, text, timestamp, type, isSender, isUnread, id, attachments, reactions, linkedMessageID, seen, editedTimestamp, isDeleted "
                      "FROM messages WHERE chatID = ? AND (type IS NULL OR type != 'REACTION') "
                      "ORDER BY timestamp DESC LIMIT ? OFFSET ?");

        query.addBindValue(chatID);
        query.addBindValue(limit);
        query.addBindValue(offset);
    }

    if (query.exec()) {
        bb::data::JsonDataAccess jda;
        bool isOlderMessagesSeen = false;

        while (query.next()) {
            QVariantMap message;
            QString msgId = query.value(6).toString();

            message["id"] = msgId;
            message["senderName"] = query.value(0).toString();
            //qDebug()<<"[Orjinal mesaj]:"<<query.value(1).toString();
            //qDebug()<<"[Formatli mesaj]:"<<formatMessageLinks(query.value(1).toString());
            QString msgText=formatMessageLinks(query.value(1).toString());
            message["text"] = msgText;
            QString msgType = query.value(3).toString();
            message["type"] = msgType;
            message["isSender"] = query.value(4).toBool();
            bool isSenderMain = query.value(4).toBool();
            message["isUnread"] = query.value(5).toBool();
            message["mxcUrl"] = "";
            message["localImagePath"] = "";

            // --- YENİ EKLENEN KISIM: linkedMessageID, mentions, seen ---
            QString linkedMessageID = query.value(9).toString();
            QString seenJson = query.value(10).toString();
            QString editedTimestamp = query.value(11).toString();
            bool isDeleted = query.value(12).toBool();

            // Reaksiyon İşleme
            QString reactionsJson = query.value(8).toString();
            //qDebug()<<formatMessageLinks(messageBody)<<":"<<reactionsJson<<":"<<seenJson;
            QString formattedReactions = "";
            QString myReaction = "";

            if (!reactionsJson.isEmpty()) {
                QVariant reactions = jda.loadFromBuffer(reactionsJson);
                message["reactions"] = reactions;

                QVariantList rList = reactions.toList();
                QStringList uniqueKeys;

                for (int i = 0; i < rList.size(); ++i) {
                    QVariantMap rMap = rList.at(i).toMap();
                    QString key = rMap.value("reactionKey").toString();

                    if (!key.isEmpty() && !uniqueKeys.contains(key)) {
                        uniqueKeys.append(key);
                    }

                    QString participantID = rMap.value("participantID").toString();

                    if (!participantID.isEmpty() && loginIDs.contains(participantID)) {
                        myReaction = key;
                    }
                }
                formattedReactions = uniqueKeys.join(" ");
            } else {
                message["reactions"] = QVariantList();
            }

            message["reactionsText"] = formattedReactions;
            message["myReaction"] = myReaction;


            if(isDeleted){
                message["text"]=QString::fromUtf8("⊘ This message is deleted");
                message["type"]="TEXT";
                message["reactionsText"] ="";
            }

            // Seen Parse ve isSeen Mantığı
            bool isSeen = false;

            if (!seenJson.isEmpty()) {
                QVariantMap seenMap = jda.loadFromBuffer(seenJson).toMap();
                QStringList seenKeys = seenMap.keys();

                for (int k = 0; k < seenKeys.size(); ++k) {
                    if (!loginIDs.contains(seenKeys.at(k))) {
                        isSeen = true;
                        isOlderMessagesSeen = true;
                        break;
                    }
                }
            }

            if (isOlderMessagesSeen) {
                isSeen = true;
            }

            message["isSeen"] = isSeen;

            // -----------------------------------------------------------

            // Zaman Formatlama (UTC -> Yerel)
            QDateTime localDt;
            QString timestampStr = query.value(2).toString();
            QDateTime dt = QDateTime::fromString(timestampStr, Qt::ISODate);
            if (dt.isValid()){
                dt.setTimeSpec(Qt::UTC);
                localDt = dt.toLocalTime();
                timestampStr = localDt.time().toString("HH:mm");
                QDate msgDate = localDt.date();
                QDate today = QDate::currentDate();
                int daysDiff = msgDate.daysTo(today);

                QString dateHeader;
                if (daysDiff == 0) {
                    dateHeader = tr("Today");
                } else if (daysDiff == 1) {
                    dateHeader = tr("Yesterday");
                } else if (daysDiff > 1 && daysDiff < 7) {
                    dateHeader = msgDate.toString("dddd");
                } else {
                    dateHeader = msgDate.toString(Qt::DefaultLocaleShortDate);
                }

                message["dateHeader"] = dateHeader;
            } else {
                timestampStr = "??:??";
                message["dateHeader"] = "Unknown";
            }

            if(!editedTimestamp.isEmpty()){
                timestampStr="Edited "+timestampStr;
            }

            if(msgType == "STICKER"){
                message["text"]=QString::fromUtf8("🌞  A sticker is received");
                if (isSenderMain) message["text"]=QString::fromUtf8("🌞  A sticker is sent");
                timestampStr="Not supported! "+timestampStr;
            }
            message["timestamp"] = timestampStr;

            // Ek (Attachment) İşleme
            QString attachmentsJson = query.value(7).toString();
            if (!attachmentsJson.isEmpty()) {
                QVariantList attachments = jda.loadFromBuffer(attachmentsJson).toList();
                if (!attachments.isEmpty()) {
                    QVariantMap firstAttachment = attachments.first().toMap();

                    QString fileName = firstAttachment["fileName"].toString();
                    QString mimeType = firstAttachment["mimeType"].toString().toLower();
                    QString attachmentID = firstAttachment["id"].toString();
                    bool isVoiceNote = firstAttachment["isVoiceNote"].toBool();
                    qint64 fileSize = firstAttachment["fileSize"].toLongLong();
                    // Boyutu okunabilir formata dönüştürme (KB veya MB)
                    QString fileSizeStr = "";

                    double bytes = static_cast<double>(fileSize);

                    if (bytes < 1024.0) {
                        fileSizeStr = QString::number(bytes, 'f', 0) + " Bytes";
                    } else if (bytes < (1024.0 * 1024.0)) {
                        fileSizeStr = QString::number(bytes / 1024.0, 'f', 0) + " KB";
                    } else if (bytes < (1024.0 * 1024.0 * 1024.0)) {
                        fileSizeStr = QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
                    } else {
                        fileSizeStr = QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
                    }

                    QVariantMap sizeMap;
                    if (firstAttachment.contains("size") && firstAttachment["size"].type() == QVariant::Map) {
                        sizeMap = firstAttachment["size"].toMap();
                    }
                    if (!sizeMap.isEmpty()) {
                        message["size"] = sizeMap;
                    }

                    QString typeUpper = mimeType.toUpper();
                    QString subDir = "files";

                    // Sadece klasörleri belirliyoruz
                    if (typeUpper.contains("VIDEO")) {
                        subDir = "videos";
                    } else if (typeUpper.contains("AUDIO") || typeUpper.contains("VOICE") || isVoiceNote) {
                        subDir = "audio";
                    } else if (typeUpper.contains("IMAGE")) {
                        subDir = "images";
                    }

                    // 1. ÖNCELİK: Gelen gerçek dosya adını kullan
                    QString finalFileName = fileName;

                    // 2. GÜVENLİK AĞI (Fallback): Eğer dosya adı boş geldiyse (örn: anlık ses kaydı) msgId ile isim uydur
                    // 1. Dosya adını temizle ve küçük harfe çevir
                    QString cleanFileName = finalFileName.trimmed();
                    QString lowerFileName = cleanFileName.toLower();

                    // 2. Jenerik/varsayılan sunucu isimlerini tespit et
                    bool isGenericName = lowerFileName.isEmpty()
                        || lowerFileName == "image.jpg"
                        || lowerFileName == "image.jpeg"
                        || lowerFileName == "image.png"
                        || lowerFileName == "video.mp4"
                        || lowerFileName == "voice message.ogg"
                        || lowerFileName == "voice_message.ogg"
                        || lowerFileName == "audio.ogg"
                        || lowerFileName == "audio.m4a"
                        || lowerFileName == "file"
                        || lowerFileName == "attachment";

                    // 3. Dosya uzantısını al (Varsa mevcut uzantıyı koru, yoksa subDir/MIME'dan türet)
                    QString ext0 = QFileInfo(cleanFileName).suffix().toLower();

                    if (ext0.isEmpty()) {
                        if (subDir == "videos") {
                            ext0 = "mp4";
                        } else if (subDir == "audio") {
                            if (typeUpper.contains("OGG") || typeUpper.contains("OPUS")) ext0 = "ogg";
                            else ext0 = "m4a";
                        } else if (subDir == "images") {
                            ext0 = typeUpper.contains("PNG") ? "png" : "jpg";
                        } else {
                            ext0 = "bin";
                        }
                    }


                    // 4. Nihai Dosya Adını Oluştur
                    if (isGenericName) {
                        // Jenerik veya boş isimlerde çakışmayı önlemek için benzersiz msgId kullanıyoruz
                        finalFileName = msgType + localDt.toString("_yyMMdd_") + msgId +"." + ext0;
                    } //else {
                        // Özel bir dosya adı geldiyse (örneğin "fatura_2026.pdf"), yine de çakışmayı engellemek için msgId ekliyoruz
                        //finalFileName = msgId + "_" + cleanFileName;
                    //}



                    // Uzantıyı dinamik olarak nihai dosya adından (finalFileName) çıkart
                    QString ext = "";
                    int lastDot = finalFileName.lastIndexOf(".");
                    if (lastDot != -1) {
                        ext = finalFileName.mid(lastDot);
                    }

                    // Mesaj objesini güncelle
                    message["attachments"] = attachments;
                    message["fileName"]    = finalFileName;
                    message["extension"]   = ext;
                    message["mxcUrl"]      = attachmentID;
                    message["fileSize"]    = fileSize;
                    message["fileSizeStr"] = fileSizeStr;

                    // Dosya yolunu yeni mantığa göre birleştir ve kontrol et
                    QString localPath = "/accounts/1000/shared/misc/Beeper/" + subDir + "/" + finalFileName;

                    if (QFile::exists(localPath)) {
                        message["localImagePath"] = "file://" + localPath;
                    }
                }
            }

            message["linkedMessageID"] = linkedMessageID;

            bool hasMention = !linkedMessageID.isEmpty();
            message["hasMention"] = hasMention;


            QString mentionSenderName = "";
            QString mentionPreview = "";
            QString mentionImgLocalUrl = "";
            double mentionImgRatio = 1.0;
            bool isMentionImg = false;

            if (hasMention) {
                // Bağlantı hatası olmaması için (db) eklendi
                QSqlQuery linkedQuery(db);
                linkedQuery.prepare("SELECT senderName, text, type, isSender, attachments, timestamp FROM messages WHERE id = :id");
                linkedQuery.bindValue(":id", linkedMessageID);

                if (linkedQuery.exec() && linkedQuery.next()) {
                    mentionSenderName = linkedQuery.value(0).toString();
                    QString linkedType = linkedQuery.value(2).toString().toUpper();
                    bool isSender = linkedQuery.value(3).toBool();
                    if(isSender){
                        mentionSenderName="You";
                    }
                    mentionPreview = linkedQuery.value(1).toString();
                    mentionPreview = formatMessageLinks(mentionPreview);

                    //Get mention message fileName
                    QString menFileName ="";
                    QString menAttachJson = linkedQuery.value(4).toString();
                    QVariantMap sizeMap;
                    QString menMimeType;
                    if (!menAttachJson.isEmpty()) {
                        QVariantList attachments = jda.loadFromBuffer(menAttachJson).toList();
                        if (!attachments.isEmpty()) {
                            QVariantMap firstAttachment = attachments.first().toMap();
                            menFileName = firstAttachment["fileName"].toString();
                            sizeMap = firstAttachment["size"].toMap();
                            menMimeType = firstAttachment["mimeType"].toString().toLower();
                        }
                    }

                    QDateTime localDtMen;
                    QString timestampStrMen = linkedQuery.value(5).toString();
                    QDateTime dtMen = QDateTime::fromString(timestampStrMen, Qt::ISODate);
                    if (dtMen.isValid()){
                        dtMen.setTimeSpec(Qt::UTC);
                        localDtMen = dtMen.toLocalTime();
                    }

                    // Mention Attachment kontrolü
                    if (linkedType == "IMAGE") {
                        //mentionPreview = QString::fromUtf8("📷 Photo");
                        isMentionImg = true;
                        if(isSender){
                            mentionImgLocalUrl = "/accounts/1000/shared/misc/Beeper/images/" + menFileName;
                            if (!QFile::exists(mentionImgLocalUrl)) {
                                mentionImgLocalUrl = "/accounts/1000/shared/misc/Beeper/images/" + linkedType + localDtMen.toString("_yyMMdd_") + linkedMessageID +"." +(menMimeType.contains("png") ? "png" : "jpg");
                            }
                        }else{
                            mentionImgLocalUrl = "/accounts/1000/shared/misc/Beeper/images/" + linkedType + localDtMen.toString("_yyMMdd_") + linkedMessageID +"." +(menMimeType.contains("png") ? "png" : "jpg");
                        }
                        if (QFile::exists(mentionImgLocalUrl)) {
                            mentionImgLocalUrl = "file://" + mentionImgLocalUrl;
                        }else{
                            mentionImgLocalUrl = "asset:///images/ic_view_image.png";
                        }
                        if (!sizeMap.isEmpty()) {
                            mentionImgRatio = sizeMap["height"].toDouble()/sizeMap["width"].toDouble();
                        }
                    } else if (linkedType == "VIDEO" || linkedType == "VIDEO_MESSAGE") {
                        mentionPreview = QString::fromUtf8("🎥 Video ")+mentionPreview;
                    } else if (linkedType.contains("AUDIO") || linkedType.contains("VOICE")) {
                        mentionPreview = QString::fromUtf8("🔉 Audio ")+mentionPreview;
                    } else if (linkedType == "FILE") {
                        if(mentionPreview.isEmpty()){
                            mentionPreview = QString::fromUtf8("📄  ")+menFileName;
                        }else{
                            mentionPreview = QString::fromUtf8("📄  ")+menFileName+" : "+mentionPreview;
                        }

                    }  else if (linkedType.contains("LOCATION")) {
                        mentionPreview = QString::fromUtf8("📍 Location ")+mentionPreview;
                    } else if (linkedType == "STICKER") {
                        mentionPreview = QString::fromUtf8("🌞 Sticker ")+mentionPreview;
                    }

                } else {
                    mentionPreview = "Message not found";
                }
            }

            // Bulunan verileri QML'in okuyabilmesi için map'e ekliyoruz
            message["mentionSenderName"] = mentionSenderName;
            message["mentionText"] = mentionPreview;
            message["isMentionImg"] = isMentionImg;
            message["mentionImgRatio"] = mentionImgRatio;
            message["mentionImgLocalUrl"] = mentionImgLocalUrl;

            messages.append(message);
        }
    } else {
        qWarning() << "[DATABASE] getMessages query failed:" << query.lastError().text();
    }

    return messages;
}

void Database::downloadAttachment(const QString &mxcUrl, const QString &messageId, const QString &fileName,
                                  const QString &type, const QString &accountID, const QString &preferredExt,
                                  const qint64 fileSize) {
    if (mxcUrl.isEmpty() || messageId.isEmpty() || fileName.isEmpty()) return;

    QString typeUpper = type.toUpper();
    QString subDir = "files"; // Varsayılan klasör

    // Sadece klasör adını belirliyoruz
    if (typeUpper.contains("VIDEO")) {
        subDir = "videos";
    } else if (typeUpper.contains("AUDIO") || typeUpper.contains("VOICE")) {
        subDir = "audio";
    } else if (typeUpper.contains("IMAGE")) {
        subDir = "images";
    }

    // Dosya yolunu messageId yerine fileName ile oluşturuyoruz
    QString dirPath = "/accounts/1000/shared/misc/Beeper/" + subDir;
    QString localPath = dirPath + "/" + fileName;

    qDebug() << "[DATABASE] Requesting download for type:" << typeUpper << " Path:" << localPath;

    // Dosya zaten indirilmişse tekrar ağ isteği atma, doğrudan önbellekten kullan
    if (QFile::exists(localPath)) {
        emit imageDownloaded(messageId, "file://" + localPath);
        return;
    }

    // Klasör yoksa oluştur
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QUrl url(m_url + "/v1/assets/serve");
    url.addQueryItem("url", mxcUrl);

    QNetworkRequest request(url);
    QString authHeader = "Bearer " + m_accessToken;
    request.setRawHeader("Authorization", authHeader.toLatin1());

    QNetworkReply* reply = m_netManager->get(request);

    // QML'e sinyal yollarken hangi mesaja ait olduğunu bilmek için messageId'yi reply'a saklıyoruz
    reply->setProperty("messageId", messageId);
    reply->setProperty("localPath", localPath);
    reply->setProperty("expectedSize", fileSize);

    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(onDownloadProgress(qint64)));
    connect(reply, SIGNAL(finished()), this, SLOT(onAttachmentDownloaded()));
}

void Database::onAttachmentDownloaded() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[DATABASE] Image download failed:" << reply->errorString();
        return;
    }

    QString messageId = reply->property("messageId").toString();
    QString localPath = reply->property("localPath").toString();

    QFile file(localPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reply->readAll());
        file.close();
        emit imageDownloaded(messageId, "file://" + localPath);
    }
}

void Database::onDownloadProgress(qint64 bytesReceived) {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    qint64 expectedSize = reply->property("expectedSize").toLongLong();

    if (expectedSize > 0) {
        QString messageId = reply->property("messageId").toString();

        qint64 current = (bytesReceived > expectedSize) ? expectedSize : bytesReceived;

        int progress = (int)((current * 100) / expectedSize);

        emit downloadProgress(messageId, progress);
    }
}

void Database::openImage(const QString &localPath) {
    bb::system::InvokeManager manager;
    bb::system::InvokeRequest request;

    QString cleanPath = localPath;
    if (cleanPath.startsWith("file://")) {
        cleanPath.remove("file://");
    }

    // Setup what to show and in what target.
    request.setUri(QUrl::fromLocalFile(cleanPath));
    request.setTarget("sys.pictures.card.previewer");
    request.setAction("bb.action.VIEW");
    InvokeTargetReply *targetReply = manager.invoke(request);
    //setting the parent to "this" will make the manager live on after this function is destroyed
    manager.setParent(this);

    if (targetReply == NULL) {
        qDebug() << "InvokeTargetReply is NULL: targetReply = " << targetReply;
    } else {
        targetReply->setParent(this);
    }
}

void Database::openMedia(const QString &localPath) {
    bb::system::InvokeManager manager;
        bb::system::InvokeRequest request;

        QString cleanPath = localPath;
        if (cleanPath.startsWith("file://")) {
            cleanPath.remove("file://");
        }

        // Setup what to show and in what target.
        request.setUri(QUrl::fromLocalFile(cleanPath));
        request.setTarget("sys.mediaplayer.previewer");
        request.setAction("bb.action.VIEW");
        InvokeTargetReply *targetReply = manager.invoke(request);
        manager.setParent(this);

        if (targetReply == NULL) {
            qDebug() << "InvokeTargetReply is NULL: targetReply = " << targetReply;
        } else {
            targetReply->setParent(this);
        }
}

void Database::openDocument(const QString &localPath) {
    bb::system::InvokeRequest request;
    request.setAction("bb.action.VIEW");
    request.setUri(QUrl(localPath));
    m_invokeManager->invoke(request);
}

void Database::sendMessage(const QString &accountID, const QString &chatID, const QString &pendingMsgID, const QString &text, const QVariantMap &attachment, const QString &replyToMessageID) {
    // Hem metin hem de eklenti boşsa işlem yapma
    if (text.trimmed().isEmpty() && attachment.isEmpty()) return;

    m_settings.sync();

    // DOĞRU URL: /v1/chats/{chatID}/messages
    QUrl url(m_url + "/v1/chats/" + chatID + "/messages");
    QNetworkRequest request(url);
    qDebug()<<"url"<<url;
    qDebug()<<"m_accessToken"<<m_accessToken;

    QString authHeader = "Bearer " + m_accessToken;
    request.setRawHeader("Authorization", authHeader.toLatin1());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QVariantMap payload;

    // Metin varsa payload'a ekle
    if (!text.isEmpty()) {
        payload["text"] = text;
    }
    // Reply varsa payload'a ekle
    if (!replyToMessageID.isEmpty()) {
        payload["replyToMessageID"] = replyToMessageID;
    }

    // Eklenti nesnesi doluysa payload'a dahil et
    if (!attachment.isEmpty()) {
        payload["attachment"] = attachment;
    }

    bb::data::JsonDataAccess jda;
    QByteArray jsonData;
    jda.saveToBuffer(payload, &jsonData);

    qDebug() << "[DATABASE] URL:" << url.toString();
    qDebug() << "[DATABASE] Payload:" << jsonData;

    QNetworkReply* reply = m_netManager->post(request, jsonData);
    reply->setProperty("pendingMsgID", pendingMsgID);
    // BB10 QNAM HTTPS/SSL hatalarını yoksaymak için:
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));
    connect(reply, SIGNAL(finished()), this, SLOT(onMessageSent()));
}

void Database::onMessageSent() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "[DATABASE] Message sent successfully! Status:" << httpStatus;
        QString messageId = reply->property("pendingMsgID").toString();
        emit messageSentSuccessfully(messageId);
    } else {
        qWarning() << "[DATABASE ERROR] Failed to send message.";
        qWarning() << "  HTTP Status:" << httpStatus;
        qWarning() << "  Error String:" << reply->errorString();


        // Print the error body from the server if available
        QByteArray errorBody = reply->readAll();
        if (!errorBody.isEmpty()) {
            qWarning() << "  Server Response:" << QString::fromUtf8(errorBody);
        }
    }
    reply->deleteLater();
}

void Database::markAllChatsAsRead(const QString &accountID) {
    qDebug() << "[DATABASE] markAllChatsAsRead called for account:" << accountID;
    if (accountID.isEmpty()) return;

    bool changed = false;
    QStringList unreadChatIDs; // Server'a bildirilecek chatID'leri tutacağımız liste

    // --- 1. ADIM: CHATS.DB GÜNCELLEME VE LİSTELEME ---
    QSqlDatabase chatsDb = QSqlDatabase::database("chats_db_conn");
    if (chatsDb.isOpen()) {
        // A. Önce hangi sohbetlerin okunmamış olduğunu bulalım (Server'a bildirmek için)
        QSqlQuery selectQuery(chatsDb);
        selectQuery.prepare("SELECT id FROM chats WHERE accountID = ? AND unreadCount > 0");
        selectQuery.addBindValue(accountID);

        if (selectQuery.exec()) {
            while (selectQuery.next()) {
                unreadChatIDs << selectQuery.value(0).toString();
            }
        }

        // B. Şimdi lokal veri tabanını güncelleyelim
        QSqlQuery query(chatsDb);
        query.prepare("UPDATE chats SET unreadCount = 0 WHERE accountID = ? AND unreadCount > 0");
        query.addBindValue(accountID);

        if (query.exec()) {
            if (query.numRowsAffected() > 0) {
                qDebug() << "[DATABASE] Chat counts reset to 0 for account:" << accountID;
                changed = true;
            }
        }
    }

    // --- 2. ADIM: MESSAGES.DB GÜNCELLEME ---
    QSqlDatabase msgsDb = QSqlDatabase::database("messages_db_conn");
    if (msgsDb.isOpen()) {
        QSqlQuery query(msgsDb);
        query.prepare("UPDATE messages SET isUnread = 0 WHERE accountID = ? AND isUnread = 1");
        query.addBindValue(accountID);

        if (query.exec()) {
            int affected = query.numRowsAffected();
            if (affected > 0) {
                qDebug() << "[DATABASE]" << affected << "messages marked as read for account:" << accountID;
                changed = true;
            }
        }
    }

    // --- 3. ADIM: BİLDİRİM VE TETİKLEYİCİ ---
    if (changed) {

        for (int i = 0; i < unreadChatIDs.size(); ++i) {
            markChatAsRead(accountID, unreadChatIDs.at(i).toUtf8());
        }

        emit dataRefreshRequested();

        // UI tetikleyici dosya yazımı
        QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
        if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
            refreshFile.close();
        }

        emit chatsUpdated(accountID);
    }
}

Q_INVOKABLE QVariantList Database::getGroupsFromDb(const QString &searchQuery, const QString &accountID) {
    QVariantList list;

    // 1. ADIM: initContext ile oluşturulan kalıcı bağlantıyı al
    QSqlDatabase db = QSqlDatabase::database("chats_db_conn");

    if (!db.isOpen()) {
        qWarning() << "[DATABASE] chats_db_conn kapalı, açılmaya çalışılıyor...";
        if (!db.open()) return list;
    }

    // 2. ADIM: Dinamik SQL sorgusunu kurgula
    // SQLite'da verimlilik için sütunları açıkça belirtiyoruz
    QString sql = "SELECT id, title, accountID FROM chats WHERE type = 'group'";

    if (!accountID.isEmpty()) {
        sql += " AND accountID = ?";
    }

    if (!searchQuery.isEmpty()) {
        sql += " AND title LIKE ?";
    }

    QSqlQuery query(db);
    query.prepare(sql);

    // 3. ADIM: Değerleri bağla (Bind)
    int bindIdx = 0;
    if (!accountID.isEmpty()) {
        query.bindValue(bindIdx++, accountID);
    }
    if (!searchQuery.isEmpty()) {
        // LIKE sorgusu için joker karakterleri ekliyoruz
        query.bindValue(bindIdx++, "%" + searchQuery + "%");
    }

    // 4. ADIM: Sorguyu çalıştır ve listeyi doldur
    if (query.exec()) {
        while (query.next()) {
            // 1. Verileri al
            QString cTitle = query.value(1).toString();
            QString lowerTitle = cTitle.toLower();

            // 2. Filtreleri uygula (Map oluşturmadan önce)
            if (lowerTitle.contains("status") || lowerTitle.contains("broadcast")) {
                continue;
            }

            // 3. Filtreyi geçenleri listeye ekle
            QVariantMap map;
            map["id"] = query.value(0).toString();
            map["displayName"] = cTitle;
            map["network"] = query.value(2).toString();
            map["type"] = "group";

            // --- EKSİK ALANLAR (QML Hata Vermesin Diye) ---
            map["number"] = "";
            map["emails"] = QStringList(); // Rehberdeki emails dizisiyle aynı tipte boş dizi
            map["avatarUrl"] = "";         // Boş resim URL'si (default_avatar.png'ye düşer)
            // ---------------------------------------------

            list << map;
        }
    } else {
        qWarning() << "[DATABASE] getGroupsFromDb hatası:" << query.lastError().text();
    }

    // !!! KRİTİK KONTROL: DB'den gerçekten grup dönüyor mu?
    qDebug() << "[DEBUG-GROUPS] accountID:" << accountID << " | Arama:" << searchQuery
             << " | Bulunan Grup Sayısı:" << list.size();

    return list;
}

QString Database::searchChatByPhone(const QString &searchTerm, const QString &accID) {
    if (searchTerm.isEmpty()) return "";

    // 1. URL Hazırlama
    QUrl url(m_url + "/v1/chats/search");
    url.addQueryItem("query", searchTerm);
    url.addQueryItem("scope", "participants");
    url.addQueryItem("type", "single");

    if (!accID.isEmpty()) {
        url.addQueryItem("accountIDs", accID);
    }

    qDebug() << "[DEBUG-REQUEST] Giden Tam URL:" << url.toEncoded();

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toLatin1());

    QNetworkReply* reply = m_netManager->get(request);

    // 3. Senkron Bekleme Döngüsü
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    QString chatID = "";

    // 4. Yanıtı İşle
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();

        bb::data::JsonDataAccess jda;
        QVariantMap root = jda.loadFromBuffer(response).toMap();
        QVariantList items = root["items"].toList();

        if (!items.isEmpty()) {
            bool exactMatchFound = false;

            // Dönen her bir sohbet (chat) objesini döngüye al
            foreach (const QVariant &itemVar, items) {
                QVariantMap chat = itemVar.toMap();

                // Senin paylaştığın JSON'daki participants objesine iniyoruz
                QVariantMap participantsObj = chat["participants"].toMap();
                QVariantList participantItems = participantsObj["items"].toList();

                // Katılımcıları kontrol et
                foreach (const QVariant &pVar, participantItems) {
                    QVariantMap participant = pVar.toMap();
                    QString pEmail = participant["email"].toString();
                    QString pId = participant["id"].toString(); // Bazen numara ID içinde geçer
                    QString pPhone = participant["phone"].toString(); // Olası telefon anahtarı

                    // Eğer katılımcının emaili veya ID'si aradığımız değere tam uyuyorsa:
                    if (pEmail == searchTerm || pId.contains(searchTerm) || pPhone == searchTerm) {
                        chatID = chat["id"].toString();
                        exactMatchFound = true;
                        qDebug() << "[SEARCH] Tam eşleşen ChatID bulundu:" << chatID;
                        break; // İç döngüden çık
                    }
                }

                if (exactMatchFound) {
                    break; // Dış döngüden çık, doğru chat'i bulduk
                }
            }

            // Eğer tam bir katılımcı eşleşmesi bulamazsak ama API sonuç döndürdüyse,
            // fallback (B planı) olarak eski sistemdeki gibi ilk sonucu alalım.
            if (chatID.isEmpty() && !items.isEmpty()) {
                chatID = items.first().toMap()["id"].toString();
                qDebug() << "[SEARCH] Tam eşleşme yok, fallback olarak ilk ChatID alındı:" << chatID;
            }

        } else {
            qWarning() << "[SEARCH] İstek başarılı ama 'items' listesi boş döndü!";
        }
    } else {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qWarning() << "[SEARCH] Ağ Hatası Kodu:" << reply->error()
                   << "HTTP Kodu:" << statusCode
                   << "Hata Mesajı:" << reply->errorString();
    }

    reply->deleteLater();
    return chatID;
}

QVariantList Database::getDeviceContacts(const QString &userFilter) {
    QVariantList combinedContacts;
    ContactService contactService;
    QVariantList contactsList;
    QList<Contact> contacts;

    if (userFilter.isEmpty()) {
        // No filter has been specified, so just list all contacts
        ContactListFilters filter;
        contacts = contactService.contacts(filter);
    } else {
        // Use the entered filter string as search value
        ContactSearchFilters filter;
        filter.setSearchValue(userFilter);
        contacts = contactService.searchContacts(filter);
    }

    //qDebug()<<"contacts: "<<contacts.size();

    foreach (const Contact &idContact, contacts) {
        // 2. ADIM: Detayları çek
        const Contact contact = contactService.contactDetails(idContact.id());

        // 3. ADIM: İsim ayıklama
        QString display = (contact.firstName() + " " + contact.lastName()).trimmed();
        if (display.isEmpty()) {
            display = contact.displayName();
        }

        QVariantMap entry;
        entry["displayName"] = display;

        // --- NUMARALARI EKLEME ---
        QStringList phoneNumbers;
        QList<ContactAttribute> attrs = contact.phoneNumbers();

        foreach (const ContactAttribute &attr, attrs) {
            if (!attr.value().isEmpty()) {
                phoneNumbers.append(attr.value());
            }
        }
        entry["number"] = phoneNumbers;

        // --- E-POSTA BİLGİLERİNİ EKLEME ---
        QStringList emails;
        QList<ContactAttribute> emailAttrs = contact.emails();

        foreach (const ContactAttribute &attr, emailAttrs) {
            if (!attr.value().isEmpty()) {
                emails.append(attr.value()); // Sadece e-posta metnini listeye ekle
            }
        }
        entry["emails"] = emails; // QML tarafında bu anahtar ile çağrılacak
        // ---------------------------------------
        combinedContacts.append(entry);
    }

    qDebug() << "!!! [DEBUG] Listelenen toplam isimli kisi:" << combinedContacts.size();
    return combinedContacts;
}

// Database.cpp içinde
bool Database::initContext() {
    bool messagesSuccess = false;
    bool chatsSuccess = false;

    // 1. MESSAGES KANALI
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
        if (!db.open()) {
            qCritical() << "[CRITICAL] Messages dosyası acilamadi:" << db.lastError().text();
            return false;
        }

        QSqlQuery q(db);
        q.exec("PRAGMA journal_mode=WAL;");
        q.exec("PRAGMA synchronous=NORMAL;");
        q.exec("PRAGMA busy_timeout=5000;"); // Service tarafıyla yarış durumunu (Race Condition) engellemek için

        messagesSuccess = q.exec(
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

        if (!messagesSuccess) {
            qWarning() << "Messages tablosu olusturulamadi:" << q.lastError().text();
        } else {
            qDebug() << "Messages tablosu başarıyla hazırlandı.";
        }
    }

    // 2. CHATS KANALI
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
        if (!db.open()) {
            qCritical() << "[CRITICAL] Chats dosyası acilamadi:" << db.lastError().text();
            return false;
        }
        QSqlQuery q(db);
        q.exec("PRAGMA journal_mode=WAL;");
        q.exec("PRAGMA synchronous=NORMAL;");
        q.exec("PRAGMA busy_timeout=5000;");

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

        chatsSuccess = q.exec(createTable);

        if (!chatsSuccess) {
            qCritical() << "Chats Tablo Olusturma Hatasi:" << q.lastError().text();
        } else {
            qDebug() << "Chats tablosu başarıyla hazırlandı.";
        }
    }

    // Sadece bağlantıların açık olmasını değil, tabloların da başarıyla kurulduğunu garanti ediyoruz
    return messagesSuccess && chatsSuccess;
}

Q_INVOKABLE void Database::setMute(const QString &chatID, bool value) {
    qDebug() << "[DATABASE] setMute called for chat:" << chatID << "Value:" << value;

    if (chatID.isEmpty()) return;

    // 1. ADIM: Kalıcı bağlantıyı al
    QSqlDatabase db = QSqlDatabase::database("chats_db_conn");

    if (!db.isOpen()) {
        qWarning() << "[DATABASE] chats_db_conn is not open!";
        if (!db.open()) return;
    }

    // 2. ADIM: Sorguyu hazırla ve çalıştır
    // SQLite'da boolean değerleri 1 veya 0 olarak sakladığımız için dönüşümü yapıyoruz.
    QSqlQuery query(db);
    query.prepare("UPDATE chats SET isMuted = ? WHERE id = ?");
    query.addBindValue(value ? 1 : 0);
    query.addBindValue(chatID);

    if (query.exec()) {
        // Eğer en az bir satır güncellendiyse (yani chat bulunduysa) sinyal gönder
        if (query.numRowsAffected() > 0) {
            qDebug() << "[DATABASE] Chat mute status updated in DB.";

            // UI tarafındaki modelleri yenilemek için sinyal gönder
            emit dataRefreshRequested();

            // 3. ADIM: Cross-process tetikleyici (Servis tarafı için)
            // Servis'in bu değişikliği algılayıp gerekirse bildirimleri susturması sağlanır.
            QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
            if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
                refreshFile.close();
            }
        } else {
            qWarning() << "[DATABASE] No chat found with ID:" << chatID;
        }
    } else {
        qWarning() << "[DATABASE] setMute update failed:" << query.lastError().text();
    }
}

QString Database::formatMessageLinks(const QString& rawText) {
    if (rawText.isEmpty()) return "";

    QString formattedText = rawText;

    // 1. ADIM: JSON İçeriğini Ayıkla
    if (formattedText.trimmed().startsWith("{")) {
        bb::data::JsonDataAccess jda;
        QVariant jsonVar = jda.loadFromBuffer(formattedText);
        if (!jda.hasError() && jsonVar.canConvert<QVariantMap>()) {
            QVariantMap msgMap = jsonVar.toMap();
            if (msgMap.contains("text")) {
                formattedText = msgMap["text"].toString();
            }
        }
    }

    // 2. ADIM: Çifte ve Tekli HTML Escape'leri Çöz
    formattedText.replace("&amp;lt;", "<").replace("&amp;gt;", ">")
                 .replace("&amp;quot;", "\"").replace("&amp;amp;", "&");
    formattedText.replace("&lt;", "<").replace("&gt;", ">")
                 .replace("&quot;", "\"").replace("&nbsp;", " ");
    formattedText.replace("&amp;", "&");

    // 3. ADIM: Satır Atlamalarını Standartlaştır
    formattedText.replace("@@BRTAGPLACEHOLDER@@", "\n");
    formattedText.replace("##BRTAG##", "\n");
    formattedText.replace("##BR_TAG##", "\n");
    formattedText.replace("</p>", "\n");
    formattedText.replace("<br>", "\n");
    formattedText.replace("<br/>", "\n");

    QString bulletPoint = QString("\n") + QChar(0x2022) + " ";
    formattedText.replace("<li>", bulletPoint);
    formattedText.replace("</li>", "");
    formattedText.replace("<ul>", "");
    formattedText.replace("</ul>", "\n");

    formattedText.replace(QRegExp("[\r\n]+</code>"), "</code>");

    // 4. ADIM: HTML Etiketlerini ve Monospace Yapılarını Korumaya Al
    formattedText.replace("<strong>", "@@BOLD_S@@").replace("</strong>", "@@BOLD_E@@");
    formattedText.replace("<b>", "@@BOLD_S@@").replace("</b>", "@@BOLD_E@@");
    formattedText.replace("<em>", "@@ITALIC_S@@").replace("</em>", "@@ITALIC_E@@");
    formattedText.replace("<i>", "@@ITALIC_S@@").replace("</i>", "@@ITALIC_E@@");

    // A) Beeper/Matrix Çok Satırlı Bloklar: <pre><code> ve </code></pre> kombinasyonlarını yakala
    QRegExp preCodeOpen("<pre[^>]*>\\s*<code[^>]*>", Qt::CaseInsensitive);
    formattedText.replace(preCodeOpen, "@@CODE_MULTI_S@@");

    QRegExp preCodeClose("</code>\\s*</pre>", Qt::CaseInsensitive);
    formattedText.replace(preCodeClose, "@@CODE_MULTI_E@@");

    // B) Yalın <pre> ve </pre> etiketlerini yakala
    QRegExp preOpen("<pre[^>]*>", Qt::CaseInsensitive);
    formattedText.replace(preOpen, "@@CODE_MULTI_S@@");
    formattedText.replace("</pre>", "@@CODE_MULTI_E@@", Qt::CaseInsensitive);

    // C) Kalan tüm tekli <code> ve </code> etiketlerini yakala (Inline Monospace)
    QRegExp codeOpen("<code[^>]*>", Qt::CaseInsensitive);
    formattedText.replace(codeOpen, "@@CODE_INLINE_S@@");
    formattedText.replace("</code>", "@@CODE_INLINE_E@@", Qt::CaseInsensitive);

    // Beeper / Matrix <del> ve <s> Etiketlerini Korumaya Al
    QRegExp delOpen("<del[^>]*>", Qt::CaseInsensitive);
    formattedText.replace(delOpen, "@@DEL_S@@");
    formattedText.replace("</del>", "@@DEL_E@@", Qt::CaseInsensitive);

    QRegExp sOpen("<s[^>]*>", Qt::CaseInsensitive);
    formattedText.replace(sOpen, "@@DEL_S@@");
    formattedText.replace("</s>", "@@DEL_E@@", Qt::CaseInsensitive);

    // 5. ADIM: Kalan İstenmeyen Tüm HTML Etiketlerini Temizle
    QRegExp htmlTags("<[^>]+>");
    htmlTags.setMinimal(true);
    formattedText.replace(htmlTags, "");

    // 6. ADIM: Link Tespiti ve Koruma
    QRegExp urlRegex("((?:https?|ftp)://[-a-zA-Z0-9$_.+!*'(),;:?%#=/@&~]+)|(www\\.[-a-zA-Z0-9$_.+!*'(),;:?%#=/@&~]+)");
    QStringList protectedUrls;
    int urlIndex = 0;
    int pos = 0;

    while ((pos = urlRegex.indexIn(formattedText, pos)) != -1) {
        QString detectedUrl = urlRegex.cap(1).isEmpty() ? urlRegex.cap(2) : urlRegex.cap(1);
        while (!detectedUrl.isEmpty() && (detectedUrl.endsWith('.') || detectedUrl.endsWith(',') || detectedUrl.endsWith(';'))) {
            detectedUrl.chop(1);
        }
        QString hrefUrl = detectedUrl.startsWith("www.") ? "http://" + detectedUrl : detectedUrl;
        QString htmlHref = hrefUrl;
        htmlHref.replace("&", "&amp;");
        QString displayText = detectedUrl;
        displayText.replace("&", "&amp;");
        QString linkTag = QString("<a href=\"%1\">%2</a>").arg(htmlHref).arg(displayText);
        QString placeholder = QString("@@URLTAG%1@@").arg(urlIndex++);
        protectedUrls.append(linkTag);
        formattedText.replace(pos, detectedUrl.length(), placeholder);
        pos += placeholder.length();
    }


    // 7. ADIM: HTML ve Markdown Dönüştürmeleri
    formattedText.replace("@@BOLD_S@@", "<b>").replace("@@BOLD_E@@", "</b>");
    formattedText.replace("@@ITALIC_S@@", "<i>").replace("@@ITALIC_E@@", "</i>");

    // Çok satırlı (Triple): Arka plan yok, sadece monospace font
    formattedText.replace("@@CODE_MULTI_S@@", "<span style=\"font-family:monospace;\">");
    formattedText.replace("@@CODE_MULTI_E@@", "</span>");

    // Satır içi (Single): Saydam arka plan var
    formattedText.replace("@@CODE_INLINE_S@@", "<span style=\"font-family:monospace; background-color:rgba(0,0,0,0.12);\">");
    formattedText.replace("@@CODE_INLINE_E@@", "</span>");

    formattedText.replace("@@DEL_S@@", "<span style=\"text-decoration: line-through;\">").replace("@@DEL_E@@", "</span>");

    // Ham Markdown Backtick Dönüşümleri
    //QRegExp codeBlockTriple("```([^`]+)```");
    //codeBlockTriple.setMinimal(true);
    //formattedText.replace(codeBlockTriple, "<span style=\"font-family:monospace;\">\\1</span>");

    //QRegExp codeBlockSingle("`([^`\\n]+)`");
    //codeBlockSingle.setMinimal(true);
    //formattedText.replace(codeBlockSingle, "<span style=\"font-family:monospace; background-color:rgba(0,0,0,0.12);\">\\1</span>");

    // Bold / İtalik Markdown
    QRegExp boldDouble("\\*\\*([^\\*]+)\\*\\*");
    boldDouble.setMinimal(true);
    formattedText.replace(boldDouble, "<b>\\1</b>");

    QRegExp boldSingle("\\*([^\\*]+)\\*");
    boldSingle.setMinimal(true);
    formattedText.replace(boldSingle, "<b>\\1</b>");

    QRegExp italicReg("_([^_]+)_");
    italicReg.setMinimal(true);
    formattedText.replace(italicReg, "<i>\\1</i>");

    // Üstü Çizili (Strikethrough) - Qt 4.8 Cascades Uyumlu
    QRegExp strikeReg("~([^~\\n]+)~");
    strikeReg.setMinimal(true);
    formattedText.replace(strikeReg, "<span style=\"text-decoration: line-through;\">\\1</span>");

    formattedText.replace("&", "&amp;");

    // 8. ADIM: Korunan Linkleri ve Satır Atlamalarını Geri Yükle
    for (int i = 0; i < protectedUrls.size(); ++i) {
        QString placeholder = QString("@@URLTAG%1@@").arg(i);
        formattedText.replace(placeholder, protectedUrls.at(i));
    }

    QRegExp multipleNewlines("\\n{3,}");
    formattedText.replace(multipleNewlines, "\n\n");
    formattedText.replace("\n", "<br/>");

    return "<font face=\"Slate Pro\">" + formattedText + "</font>";
}


QString Database::convertToPlainText(const QString& rawText) {
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

void Database::copyToClipboard(const QString& text) {
    bb::system::Clipboard clipboard;
    clipboard.clear();

    // 1. HTML Desteklemeyen yerler için temiz düz metin (Etiketlerden arındırılmış)
    QString plainText = text;
    plainText.remove(QRegExp("<[^>]*>")); // HTML taglarını temizler
    clipboard.insert("text/plain", plainText.toUtf8());

    // 2. HTML Destekleyen zengin metin alanları için biçimlendirilmiş metin
    clipboard.insert("text/html", text.toUtf8());
}

void Database::saveCredentials(const QString &key, const QString &value) {
    if(key=="serverUrl"){
        m_url = value;
    }
    else if(key=="accessToken"){
        m_accessToken = value;
    }

    QSettings settings;
    settings.setValue(key, value);
    settings.sync();

    bb::system::InvokeRequest request;
    request.setTarget("com.example.BerryBridgeUIService");
    request.setAction("com.example.BerryBridgeUIService.CRED_UPDATE");
    m_invokeManager->invoke(request);
}

QString Database::getCredentials(const QString &key, const QString &defaultValue) {
    QSettings settings;
    return settings.value(key, defaultValue).toString();
}

void Database::createChat(const QString& accountID, const QString& participantID, const QString& chatType)
{
    // Şemadaki endpoint: /v1/chats
    QString fullUrl = m_url;
    if (!fullUrl.endsWith("/")) {
        fullUrl += "/v1/chats";
    } else {
        fullUrl += "v1/chats";
    }

    QUrl url(fullUrl);
    QNetworkRequest request(url);

    // Header Yapılandırması
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Şema Security: bearerAuth
    QByteArray authHeader = "Bearer " + m_accessToken.toUtf8();
    request.setRawHeader("Authorization", authHeader);

    // Şema 200 Response Header referansı: X-Beeper-Desktop-Version
    request.setRawHeader("X-Beeper-Desktop-Version", "1.0.0");

    // Şemadaki CreateChatInput yapısına uygun dinamik JSON oluşturma (C++98)
    // accountID string, participantIDs ise array (dizi) tipindedir.
    QString jsonTemplate = "{"
                               "\"accountID\": \"%1\","
                               "\"participantIDs\": [\"%2\"],"
                               "\"type\": \"%3\""
                               "}";

    // Değerleri sırasıyla şablona giydiriyoruz
    QString jsonString = jsonTemplate.arg(accountID).arg(participantID).arg(chatType);
    QByteArray jsonPayload = jsonString.toUtf8();

    QNetworkReply* reply = m_netManager->post(request, jsonPayload);
    connect(reply, SIGNAL(finished()), this, SLOT(onCreateChatFinished()));
}

void Database::onCreateChatFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();

    reply->deleteLater();

    if (statusCode == 200) {
        emit chatCreatedSuccess(QString::fromUtf8(responseData));
        return;
    }

    if (statusCode == 404) {
        //QTimer::singleShot(1000, this, SLOT(verifyChatFromList()));
        emit chatCreatedSuccess(QString::fromUtf8(responseData));
        return;
    }

    // 3. Durum: Gerçek Hatalar (401, 403, 429 vb.)
    emit chatCreatedError(statusCode, QString::fromUtf8(responseData));
}

void Database::uploadAssetAndSend(const QString &filePath, const QString &accountID, const QString &chatID, const QString &text, const QString &pendingMsgID, const QString &replyToMessageID) {

    QString cleanFilePath = filePath;
    if (cleanFilePath.startsWith("file://")) {
        cleanFilePath = QUrl(filePath).toLocalFile();
    }

    QFile *file = new QFile(cleanFilePath);
    if (!file->open(QIODevice::ReadOnly)) {
        qDebug() << "[DATABASE] Upload için dosya açılamadı:" << cleanFilePath;
        delete file;
        return;
    }

    m_settings.sync();
    QUrl url(m_url + "/v1/assets/upload");
    QNetworkRequest request(url);

    QString authHeader = "Bearer " + m_accessToken;
    request.setRawHeader("Authorization", authHeader.toLatin1());

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    QFileInfo fileInfo(cleanFilePath);

    // --- MIME Tipi Belirleme (Qt 4.8 için basit çözüm) ---
    QString mimeType;
    QString ext = fileInfo.suffix().toLower();

    if (ext == "jpg" || ext == "jpeg") mimeType = "image/jpeg";
    else if (ext == "png") mimeType = "image/png";
    else if (ext == "gif") mimeType = "image/gif";
    else if (ext == "webp") mimeType = "image/webp";
    else if (ext == "bmp") mimeType = "image/bmp";
    else if (ext == "heic") mimeType = "image/heic";
    else if (ext == "heif") mimeType = "image/heif";
    else if (ext == "svg") mimeType = "image/svg+xml";

    // Ses (Voice note, müzik vb.)
    else if (ext == "mp3") mimeType = "audio/mpeg";
    else if (ext == "ogg" || ext == "oga" || ext == "opus") mimeType = "audio/ogg";
    else if (ext == "wav") mimeType = "audio/wav";
    else if (ext == "m4a") mimeType = "audio/mp4";
    else if (ext == "aac") mimeType = "audio/aac";
    else if (ext == "flac") mimeType = "audio/flac";

    // Video
    else if (ext == "mp4") mimeType = "video/mp4";
    else if (ext == "webm") mimeType = "video/webm";
    else if (ext == "mkv") mimeType = "video/x-matroska";
    else if (ext == "mov") mimeType = "video/quicktime";
    else if (ext == "avi") mimeType = "video/x-msvideo";
    else if (ext == "3gp") mimeType = "video/3gpp";

    // Belgeler & İletişim
    else if (ext == "pdf") mimeType = "application/pdf";
    else if (ext == "vcf") mimeType = "text/vcard";
    else if (ext == "txt") mimeType = "text/plain";
    else if (ext == "csv") mimeType = "text/csv";
    else if (ext == "json") mimeType = "application/json";
    else if (ext == "doc") mimeType = "application/msword";
    else if (ext == "docx") mimeType = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    else if (ext == "xls") mimeType = "application/vnd.ms-excel";
    else if (ext == "xlsx") mimeType = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    else if (ext == "ppt") mimeType = "application/vnd.ms-powerpoint";
    else if (ext == "pptx") mimeType = "application/vnd.openxmlformats-officedocument.presentationml.presentation";

    // Sıkıştırılmış Dosyalar
    else if (ext == "zip") mimeType = "application/zip";
    else if (ext == "rar") mimeType = "application/x-rar-compressed";
    else if (ext == "7z") mimeType = "application/x-7z-compressed";
    else if (ext == "tar") mimeType = "application/x-tar";
    else if (ext == "gz") mimeType = "application/gzip";

    // Bilinmeyen türler için varsayılan ikili akış
    else mimeType = "application/octet-stream";

    qDebug() << "[FILEINFO]: " << fileInfo.fileName();

    // Sunucuya dosyanın türünü bildiriyoruz
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mimeType));

    // --- RFC 5987 Uyumlu Content-Disposition Yapılandırması (Qt 4.8 / BB10) ---
    QString originalFileName = fileInfo.fileName();

    // Dosya adını percent-encoding ile ASCII formatına dönüştürüyoruz (öğle.txt -> %C3%B6%C4%9Fle.txt)
    QByteArray encodedName = QUrl::toPercentEncoding(originalFileName);

    // RFC 5987 desteklemeyen eski istemciler/sunucular için ASCII fallback adı
    QString fallbackName = "upload_file." + (ext.isEmpty() ? "bin" : ext);

    // Standarda uygun başlık dizesini oluşturuyoruz
    QString dispositionString = QString("form-data; name=\"file\"; filename=\"%1\"; filename*=utf-8''%2")
                                    .arg(fallbackName)
                                    .arg(QString::fromLatin1(encodedName));

    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(dispositionString));
    filePart.setBodyDevice(file);

    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkReply *reply = m_netManager->post(request, multiPart);
    multiPart->setParent(reply);

    reply->setProperty("accountID", accountID);
    reply->setProperty("chatID", chatID);
    reply->setProperty("text", text);
    reply->setProperty("pendingMsgID", pendingMsgID);
    reply->setProperty("replyMessageId", replyToMessageID);

    // Mime tipi ve Orijinal dosya adını sonraki adım için saklıyoruz
    reply->setProperty("mimeType", mimeType);
    reply->setProperty("fileName", originalFileName);

    reply->setProperty("originalFilePath", cleanFilePath);

    connect(reply, SIGNAL(uploadProgress(qint64, qint64)), this, SLOT(onUploadProgress(qint64, qint64)));
    connect(reply, SIGNAL(finished()), this, SLOT(onAssetUploadFinished()));
}

void Database::onUploadProgress(qint64 bytesSent, qint64 bytesTotal) {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;

    if (bytesTotal > 0) {
        QString pendingMsgID = reply->property("pendingMsgID").toString();
        if (pendingMsgID.isEmpty()) return;

        // Yüzde hesabı
        int progress = (int)((bytesSent * 100) / bytesTotal);

        // QML tarafında değişime gerek kalmaması için mevcut sinyali yayıyoruz
        emit downloadProgress(pendingMsgID, progress);
    }
}

void Database::onAssetUploadFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "[DATABASE] Dosya upload hatası:" << reply->errorString();
        return;
    }

    QByteArray responseData = reply->readAll();
    bb::data::JsonDataAccess jda;
    QVariant jsonResponse = jda.loadFromBuffer(responseData);

    if (jda.hasError()) {
        qDebug() << "[DATABASE] Upload JSON parse hatası.";
        return;
    }

    QVariantMap resultMap = jsonResponse.toMap();
    QString uploadID = resultMap["uploadID"].toString();

    if (uploadID.isEmpty()) return;

    QString accountID = reply->property("accountID").toString();
    QString chatID    = reply->property("chatID").toString();
    QString text      = reply->property("text").toString();

    // Sakladığımız ek verileri alıyoruz
    QString mimeType         = reply->property("mimeType").toString();
    QString fileName         = reply->property("fileName").toString();
    QString originalFilePath = reply->property("originalFilePath").toString(); // <- Dosyanın ilk yüklendiği yer
    QString rMsgId           = reply->property("replyMessageId").toString();
    QString pendingMsgID           = reply->property("pendingMsgID").toString();
    // =========================================================================
    // DOSYAYI PAYLAŞILAN "SHARED" DİZİNİNE KOPYALAMA İŞLEMİ
    // =========================================================================
    if (!originalFilePath.isEmpty() && !fileName.isEmpty()) {
        QString typeUpper = mimeType.toUpper();
        QString subDir = "files";

        if (typeUpper.contains("VIDEO")) {
            subDir = "videos";
        } else if (typeUpper.contains("AUDIO") || typeUpper.contains("VOICE")) {
            subDir = "audio";
        } else if (typeUpper.contains("IMAGE")) {
            subDir = "images";
        }

        QString dirPath = "/accounts/1000/shared/misc/Beeper/" + subDir;
        QString localPath = dirPath + "/" + fileName;

        QDir dir;
        if (!dir.exists(dirPath)) {
            dir.mkpath(dirPath);
        }

        // === EKLENEN KRİTİK KONTROL ===
        if (originalFilePath == localPath) {
            qDebug() << "[DATABASE] Kaynak ve hedef aynı. Kopyalamaya gerek yok:" << localPath;
        } else {
            // Kaynak ve hedef farklıysa eski dosyayı sil ve yenisini kopyala
            if (QFile::exists(localPath)) {
                QFile::remove(localPath);
            }

            if (QFile::copy(originalFilePath, localPath)) {
                qDebug() << "[DATABASE] Dosya başarıyla Beeper klasörüne kopyalandı:" << localPath;
            } else {
                qDebug() << "[DATABASE] HATA: Dosya kopyalanamadı! Kaynak:" << originalFilePath << "Hedef:" << localPath;
            }
        }
    } else {
        qDebug() << "[DATABASE] UYARI: originalFilePath veya fileName eksik, kopyalama atlandı.";
    }
    // =========================================================================

    // Eklenti objesini daha zengin hale getiriyoruz
    QVariantMap attachment;
    attachment["uploadID"]    = uploadID;
    attachment["contentType"] = mimeType;
    attachment["name"]        = fileName;

    // Mesajı Beeper/Matrix'e bildir
    sendMessage(accountID, chatID, pendingMsgID, text, attachment,rMsgId);
}

QVariantList Database::searchMessages(const QString &accountID, const QString &query)
{
    QVariantList results;

    QString trimmedQuery = query.trimmed();
    if (trimmedQuery.isEmpty() || accountID.isEmpty()) {
        return results;
    }

    // 1. ADIM: Mesajlar veritabanı bağlantısı
    QSqlDatabase dbMessages = QSqlDatabase::database("messages_db_conn");
    if (!dbMessages.isOpen() && !dbMessages.open()) {
        qCritical() << "[DATABASE] Failed to open messages_db_conn!";
        return results;
    }

    // 2. ADIM: Chat veritabanı bağlantısı
    QSqlDatabase dbChats = QSqlDatabase::database("chats_db_conn");
    if (!dbChats.isOpen() && !dbChats.open()) {
        qCritical() << "[DATABASE] Failed to open chats_db_conn!";
        return results;
    }

    // 3. ADIM: Arama sorgusunu çalıştırma
    QSqlQuery qMsg(dbMessages);
    qMsg.prepare("SELECT id, chatID, timestamp, text FROM messages "
                 "WHERE accountID = ? AND text LIKE ? "
                 "ORDER BY sortKey DESC");
    qMsg.addBindValue(accountID);
    qMsg.addBindValue("%" + trimmedQuery + "%");

    if (!qMsg.exec()) {
        qWarning() << "[DATABASE] Search Query Error:" << qMsg.lastError().text();
        return results;
    }

    // Chat başlıklarını önbelleğe alarak mükerrer sorguları önlüyoruz
    QHash<QString, QString> titleCache;
    QSqlQuery qChat(dbChats);
    qChat.prepare("SELECT title FROM chats WHERE id = ? AND accountID = ? LIMIT 1");

    while (qMsg.next()) {
        QString messageID = qMsg.value(0).toString();
        QString chatID = qMsg.value(1).toString();
        QString timestamp = qMsg.value(2).toString();
        QString fullText = qMsg.value(3).toString();

        // chatID üzerinden title bulma
        QString chatTitle;
        if (titleCache.contains(chatID)) {
            chatTitle = titleCache.value(chatID);
        } else {
            qChat.bindValue(0, chatID);
            qChat.bindValue(1, accountID);
            if (qChat.exec() && qChat.next()) {
                chatTitle = qChat.value(0).toString();
            }
            titleCache.insert(chatID, chatTitle);
        }

        // Metin kesiti (snippet) alma
        int index = fullText.indexOf(trimmedQuery, 0, Qt::CaseInsensitive);
        QString snippet = fullText;

        if (index != -1) {
            int margin = 30;
            int start = qMax(0, index - margin);
            int length = trimmedQuery.length() + (index - start) + margin;

            snippet = fullText.mid(start, length);

            if (start > 0) snippet.prepend("...");
            if (start + length < fullText.length()) snippet.append("...");
        }

        QVariantMap item;
        item["type"] = "";
        item["messageID"] = messageID; // Tıklanan mesaja gitmek için kritik
        item["chatID"] = chatID;
        item["chatTitle"] = chatTitle.isEmpty() ? "Sohbet" : chatTitle;
        item["timestamp"] = formatTimeForDisplay(timestamp);
        item["snippet"] = snippet;

        results.append(item);
    }

    return results;
}

#include <QTimer>
void Database::requestDelayedScroll() {
    // 100 ms sonra QML'e kaydırma sinyali gönderir
    QTimer::singleShot(100, this, SIGNAL(scrollToTargetRequested()));
}

void Database::sendReaction(const QString &chatID, const QString &msgID, const QString &reactionKey) {
    // Reaksiyon emojisi boşsa işlem yapma
    if (reactionKey.trimmed().isEmpty()) return;

    m_settings.sync();

    // DOĞRU URL: /v1/chats/{chatID}/messages/{messageID}/reactions
    QUrl url(m_url + "/v1/chats/" + chatID + "/messages/" + msgID + "/reactions");
    QNetworkRequest request(url);

    // Beeper API Auth Başlığı
    QString authHeader = "Bearer " + m_accessToken;
    request.setRawHeader("Authorization", authHeader.toLatin1());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // JSON Payload Oluşturma
    QVariantMap payload;
    payload["reactionKey"] = reactionKey;
    // transactionID opsiyonel olduğu için eklemiyoruz, Beeper kendi oluşturur.

    bb::data::JsonDataAccess jda;
    QByteArray jsonData;
    jda.saveToBuffer(payload, &jsonData);

    qDebug() << "[DATABASE] Reaction URL:" << url.toString();
    qDebug() << "[DATABASE] Reaction Payload:" << jsonData;

    // Ağa gönderim yapılıyor
    QNetworkReply* reply = m_netManager->post(request, jsonData);

    // Slot içinde kullanabilmek için msgID ve reactionKey bilgilerini reply nesnesine iliştiriyoruz
    reply->setProperty("msgID", msgID);
    reply->setProperty("reactionKey", reactionKey);

    connect(reply, SIGNAL(finished()), this, SLOT(onSendReactionFinished()));
}

void Database::onSendReactionFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // İstek gönderilirken property olarak kaydettiğimiz değerleri geri alıyoruz
    QString msgID = reply->property("msgID").toString();
    QString reactionKey = reply->property("reactionKey").toString();

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "[DATABASE] Reaction sent successfully! Status:" << httpStatus << "MsgID:" << msgID;
        emit reactionSentSuccessfully(msgID, reactionKey);
    } else {
        qWarning() << "[DATABASE ERROR] Failed to send reaction.";
        qWarning() << "  HTTP Status:" << httpStatus;
        qWarning() << "  Error String:" << reply->errorString();

        // Sunucudan dönen detaylı hatayı logla (API docs: 400, 404 vb. dönebilir)
        QByteArray errorBody = reply->readAll();
        if (!errorBody.isEmpty()) {
            qWarning() << "  Server Response:" << QString::fromUtf8(errorBody);
        }

        emit reactionSendFailed(msgID, reply->errorString());
    }

    // Hafıza yönetimi
    reply->deleteLater();
}

void Database::removeReaction(const QString &chatID, const QString &msgID, const QString &reactionKey) {
    if (reactionKey.trimmed().isEmpty()) return;

    m_settings.sync();

    // DİKKAT: reactionKey (emoji) doğrudan URL'ye eklendiği için QUrl::toPercentEncoding ile encode edilmelidir!
    //QString encodedEmoji = QString::fromUtf8(QUrl::toPercentEncoding(reactionKey));
    QUrl url(m_url + "/v1/chats/" + chatID + "/messages/" + msgID + "/reactions/" + reactionKey);
    QNetworkRequest request(url);

    // Beeper API Auth Başlığı
    QString authHeader = "Bearer " + m_accessToken;
    request.setRawHeader("Authorization", authHeader.toLatin1());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    qDebug() << "[DATABASE] Remove Reaction URL:" << url.toString();

    // Ağa DELETE isteği gönderiliyor (m_netManager->deleteResource)
    QNetworkReply* reply = m_netManager->deleteResource(request);

    // Slot içinde kullanabilmek için msgID ve reactionKey bilgilerini reply nesnesine iliştiriyoruz
    reply->setProperty("msgID", msgID);
    reply->setProperty("reactionKey", reactionKey);

    connect(reply, SIGNAL(finished()), this, SLOT(onRemoveReactionFinished()));
}

void Database::onRemoveReactionFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // İstek gönderilirken property olarak kaydettiğimiz değerleri geri alıyoruz
    QString msgID = reply->property("msgID").toString();
    QString reactionKey = reply->property("reactionKey").toString();

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "[DATABASE] Reaction removed successfully! Status:" << httpStatus << "MsgID:" << msgID;

        // Remove Reaction from local database
        QStringList loginIDs = m_settings.value("login_ids").toStringList();
        QSqlDatabase db = QSqlDatabase::database("messages_db_conn");

        if (!db.isOpen()) {
            db.open();
        }

        if (db.isOpen()) {
            // 1. ADIM: İlgili mesajın mevcut reaksiyon verisini veritabanından çek
            QSqlQuery selectQuery(db);
            selectQuery.prepare("SELECT reactions FROM messages WHERE id = ?");
            selectQuery.addBindValue(msgID);

            if (selectQuery.exec() && selectQuery.next()) {
                QString currentReactionsStr = selectQuery.value(0).toString();

                if (!currentReactionsStr.isEmpty()) {
                    bb::data::JsonDataAccess jda;
                    QVariantList reactionsList = jda.loadFromBuffer(currentReactionsStr).toList();

                    QVariantList updatedList;
                    bool modified = false;

                    // 2. ADIM: Reaksiyonları dön ve yerel kullanıcınınkini listeden çıkar
                    for (int i = 0; i < reactionsList.size(); ++i) {
                        QVariantMap reactionMap = reactionsList.at(i).toMap();

                        // participantID veya senderID alanını oku
                        QString participantID = reactionMap.value("participantID").toString();
                        if (participantID.isEmpty()) {
                            participantID = reactionMap.value("senderID").toString();
                        }

                        // Eğer reaksiyon sahibi (login_ids içindeki) kullanıcımız ise listeye ekleme
                        if (loginIDs.contains(participantID)) {
                            modified = true;
                        } else {
                            // Başkalarının reaksiyonlarını yeni listeye ekleyerek koru
                            updatedList.append(reactionMap);
                        }
                    }

                    // 3. ADIM: Listeden kendi reaksiyonumuz silindiyse veritabanını güncelle
                    if (modified) {
                        QString newReactionsStr = "[]"; // Varsayılan olarak boş liste

                        if (!updatedList.isEmpty()) {
                            QByteArray buffer;
                            jda.saveToBuffer(updatedList, &buffer);
                            newReactionsStr = QString::fromUtf8(buffer);
                        }

                        // Sadece spesifik mesaja (msgID) ait reaksiyon alanını güncelle
                        QSqlQuery updateQuery(db);
                        updateQuery.prepare("UPDATE messages SET reactions = ? WHERE id = ?");
                        updateQuery.addBindValue(newReactionsStr);
                        updateQuery.addBindValue(msgID);

                        if (updateQuery.exec()) {
                            qDebug() << "[DATABASE] Local reaction soft-deleted successfully for MsgID:" << msgID;
                        } else {
                            qWarning() << "[DATABASE ERROR] Failed to soft-delete reaction:" << updateQuery.lastError().text();
                        }
                    } else {
                        qDebug() << "[DATABASE] No local user reaction found to delete on MsgID:" << msgID;
                    }
                }
            } else {
                qWarning() << "[DATABASE ERROR] Failed to fetch message for reaction deletion:" << selectQuery.lastError().text();
            }
        }

        // UI Arayüzünü Tetikle
        QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
        if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
            refreshFile.close();
        }
        emit reactionRemovedSuccessfully(msgID, reactionKey);
    } else {
        qWarning() << "[DATABASE ERROR] Failed to remove reaction.";
        qWarning() << "  HTTP Status:" << httpStatus;
        qWarning() << "  Error String:" << reply->errorString();

        // Sunucudan dönen detaylı hatayı logla
        QByteArray errorBody = reply->readAll();
        if (!errorBody.isEmpty()) {
            qWarning() << "  Server Response:" << QString::fromUtf8(errorBody);
        }

        emit reactionRemoveFailed(msgID, reply->errorString());
    }

    // Hafıza yönetimi
    reply->deleteLater();
}

void Database::deleteMessage(const QString &chatID, const QString &msgID, bool forEveryone) {
    if (chatID.trimmed().isEmpty() || msgID.trimmed().isEmpty()) return;

    // --- "FOR ME" SEÇİLDİYSE: SADECE LOKAL VERİTABANINDAN SİL VE BİTİR ---
    if (!forEveryone) {
        qDebug() << "[DATABASE] Soft-deleting message locally for MsgID:" << msgID;

        QSqlDatabase db = QSqlDatabase::database("messages_db_conn");
        if (!db.isOpen()) {
            db.open();
        }

        if (db.isOpen()) {
            QSqlQuery updateQuery(db);
            updateQuery.prepare("UPDATE messages SET isDeleted = 1 WHERE id = ?");
            updateQuery.addBindValue(msgID);

            if (updateQuery.exec()) {
                qDebug() << "[DATABASE] Local message soft-deleted successfully for MsgID:" << msgID;
            } else {
                qWarning() << "[DATABASE ERROR] Failed to soft-delete message:" << updateQuery.lastError().text();
            }
        }

        // UI Arayüzünü Tetikle
        QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
        if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
            refreshFile.close();
        }

        emit messageDeletedSuccessfully(msgID);
        return;
    }

    // --- "FOR EVERYONE" SEÇİLDİYSE: SUNUCUYA AĞ İSTEĞİ GÖNDER ---
    m_settings.sync();

    QString urlStr = m_url + "/v1/chats/" + chatID + "/messages/" + msgID + "?forEveryone=true";
    QUrl url(urlStr);
    QNetworkRequest request(url);

    // Beeper API Auth Başlığı
    QString authHeader = "Bearer " + m_accessToken;
    request.setRawHeader("Authorization", authHeader.toLatin1());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    qDebug() << "[DATABASE] Delete Message URL (For Everyone):" << url.toString();

    // Ağa DELETE isteği gönderiliyor
    QNetworkReply* reply = m_netManager->deleteResource(request);

    // Slot içinde kullanabilmek için msgID bilgisini reply nesnesine iliştiriyoruz
    reply->setProperty("msgID", msgID);

    connect(reply, SIGNAL(finished()), this, SLOT(onDeleteMessageFinished()));
}

void Database::onDeleteMessageFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString msgID = reply->property("msgID").toString();
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "[DATABASE] Message deleted successfully! Status:" << httpStatus << "MsgID:" << msgID;
        emit messageDeletedSuccessfully(msgID);
    } else {
        qWarning() << "[DATABASE ERROR] Failed to delete message.";
        qWarning() << "  HTTP Status:" << httpStatus;
        qWarning() << "  Error String:" << reply->errorString();

        QByteArray errorBody = reply->readAll();
        if (!errorBody.isEmpty()) {
            qWarning() << "  Server Response:" << QString::fromUtf8(errorBody);
        }

        emit messageDeleteFailed(msgID, reply->errorString());
    }

    reply->deleteLater();
}

void Database::editMessage(const QString &chatID, const QString &msgID, const QString &text) {
    if (chatID.trimmed().isEmpty() || msgID.trimmed().isEmpty() || text.trimmed().isEmpty()) return;

    m_settings.sync();

    // URL: /v1/chats/{chatID}/messages/{messageID}
    QUrl url(m_url + "/v1/chats/" + chatID + "/messages/" + msgID);
    QNetworkRequest request(url);

    QString authHeader = "Bearer " + m_accessToken;
    request.setRawHeader("Authorization", authHeader.toLatin1());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Payload Hazırlığı
    QVariantMap payload;
    payload["text"] = text;

    bb::data::JsonDataAccess jda;
    QByteArray jsonData;
    jda.saveToBuffer(payload, &jsonData);

    qDebug() << "[DATABASE] Edit Message URL:" << url.toString();
    qDebug() << "[DATABASE] Payload:" << jsonData;

    // PUT İsteği
    QNetworkReply* reply = m_netManager->put(request, jsonData);
    reply->setProperty("msgID", msgID);

    connect(reply, SIGNAL(finished()), this, SLOT(onEditMessageFinished()));
}

void Database::onEditMessageFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString msgID = reply->property("msgID").toString();
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "[DATABASE] Message edited successfully! Status:" << httpStatus << "MsgID:" << msgID;
        emit messageEditedSuccessfully(msgID);
    } else {
        qWarning() << "[DATABASE ERROR] Failed to edit message.";
        qWarning() << "  HTTP Status:" << httpStatus;
        qWarning() << "  Error String:" << reply->errorString();

        QByteArray errorBody = reply->readAll();
        if (!errorBody.isEmpty()) {
            qWarning() << "  Server Response:" << QString::fromUtf8(errorBody);
        }

        emit messageEditFailed(msgID, reply->errorString());
    }

    reply->deleteLater();
}

void Database::deleteChat(const QString &chatID) {
    if (chatID.trimmed().isEmpty()) return;

    qDebug() << "[DATABASE] Deleting chat locally for chatID:" << chatID;

    QSqlDatabase db = QSqlDatabase::database("chats_db_conn");
    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "[DATABASE ERROR] Failed to open database:" << db.lastError().text();
            return; // Veritabanı açılamazsa işleme devam etme
        }
    }

    if (db.isOpen()) {
        QSqlQuery deleteQuery(db);

        // DİKKAT: 'chats' ve 'chat_id' isimlerini kendi veritabanı şemanıza göre güncelleyin.
        deleteQuery.prepare("DELETE FROM chats WHERE id = ?");
        deleteQuery.addBindValue(chatID);

        if (deleteQuery.exec()) {
            if (deleteQuery.numRowsAffected() > 0) {
                qDebug() << "[DATABASE] Local chat deleted successfully for chatID:" << chatID;
            } else {
                qDebug() << "[DATABASE] ChatID not found, no rows deleted:" << chatID;
            }
        } else {
            qWarning() << "[DATABASE ERROR] Failed to delete chat:" << deleteQuery.lastError().text();
            return; // Hata durumunda UI'ı tetiklememek için fonksiyondan çık
        }
    }

    // UI Arayüzünü Tetikle
    QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
    if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
        refreshFile.close();
    }

    emit chatDeletedSuccessfully(chatID);
}

Q_INVOKABLE void Database::setPin(const QString &chatID, bool value) {
    qDebug() << "[DATABASE] setMute called for chat:" << chatID << "Value:" << value;

    if (chatID.isEmpty()) return;

    // 1. ADIM: Kalıcı bağlantıyı al
    QSqlDatabase db = QSqlDatabase::database("chats_db_conn");

    if (!db.isOpen()) {
        qWarning() << "[DATABASE] chats_db_conn is not open!";
        if (!db.open()) return;
    }

    // 2. ADIM: Sorguyu hazırla ve çalıştır
    // SQLite'da boolean değerleri 1 veya 0 olarak sakladığımız için dönüşümü yapıyoruz.
    QSqlQuery query(db);
    query.prepare("UPDATE chats SET isPinned = ? WHERE id = ?");
    query.addBindValue(value ? 1 : 0);
    query.addBindValue(chatID);

    if (query.exec()) {
        // Eğer en az bir satır güncellendiyse (yani chat bulunduysa) sinyal gönder
        if (query.numRowsAffected() > 0) {
            qDebug() << "[DATABASE] Chat pin status updated in DB.";

            // UI tarafındaki modelleri yenilemek için sinyal gönder
            emit dataRefreshRequested();

            // 3. ADIM: Cross-process tetikleyici (Servis tarafı için)
            // Servis'in bu değişikliği algılayıp gerekirse bildirimleri susturması sağlanır.
            QFile refreshFile("/accounts/1000/shared/misc/Beeper/ui_refresh_trigger.txt");
            if (refreshFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                refreshFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
                refreshFile.close();
            }
        } else {
            qWarning() << "[DATABASE] No chat found with ID:" << chatID;
        }
    } else {
        qWarning() << "[DATABASE] setMute update failed:" << query.lastError().text();
    }
}

QVariantMap Database::getImageDimensions(const QString &filePath) {
    QString cleanPath = filePath.startsWith("file://") ? QUrl(filePath).toLocalFile() : filePath;

    QImageReader reader(cleanPath);
    QSize size = reader.size(); // Sadece başlık bilgisini okur

    QVariantMap result;
    result["width"] = size.width();
    result["height"] = size.height();
    result["isValid"] = size.isValid(); // Dosya geçerli bir resim mi?

    return result;
}

QString Database::toPlainText(const QString& htmlText) {
    if (htmlText.isEmpty()) return "";

    QString plainText = htmlText;

    // 1. ADIM: En dıştaki <font face="Slate Pro"> ve </font> etiketlerini temizle
    QRegExp fontStart("^<font[^>]*>", Qt::CaseInsensitive);
    plainText.replace(fontStart, "");
    QRegExp fontEnd("</font>$", Qt::CaseInsensitive);
    plainText.replace(fontEnd, "");

    // 2. ADIM: Satır atlamalarını geri çevir (<br/> -> \n)
    plainText.replace("<br/>", "\n");
    plainText.replace("<br>", "\n");

    // 3. ADIM: Linkleri temizle (<a href="...">METİN</a> -> METİN)
    // formatMessageLinks fonksiyonunda eklenen link etiketini kaldırıp sadece görünen metni bırakıyoruz
    QRegExp aTag("<a[^>]*>(.*)</a>", Qt::CaseInsensitive);
    aTag.setMinimal(true);
    plainText.replace(aTag, "\\1");

    // 4. ADIM: Biçimlendirmeleri WhatsApp / Markdown Formatına Geri Çevir
    // DİKKAT: Qt 4.8 uyumluluğu için (.*?) yerine (.*) ve setMinimal(true) kullanıldı!

    // A) Satır İçi Kod Bloğu
    QRegExp inlineCode("<span style=\"font-family:monospace; background-color:rgba\\(0,0,0,0\\.12\\);?\">(.*)</span>", Qt::CaseInsensitive);
    inlineCode.setMinimal(true);
    plainText.replace(inlineCode, "`\\1`");

    // B) Çok Satırlı Kod Bloğu
    QRegExp multiCode("<span style=\"font-family:monospace;\">(.*)</span>", Qt::CaseInsensitive);
    multiCode.setMinimal(true);
    plainText.replace(multiCode, "```\\1```");

    // C) Kalın (Bold) - <b>metin</b> -> *metin*
    QRegExp boldTag("<b>(.*)</b>", Qt::CaseInsensitive);
    boldTag.setMinimal(true);
    plainText.replace(boldTag, "*\\1*");

    // D) İtalik (Italic) - <i>metin</i> -> _metin_
    QRegExp italicTag("<i>(.*)</i>", Qt::CaseInsensitive);
    italicTag.setMinimal(true);
    plainText.replace(italicTag, "_\\1_");

    // E) Üstü Çizili (Strikethrough) - <span style="text-decoration: line-through;">metin</span> -> ~metin~
    QRegExp strikeTag("<span style=\"text-decoration: line-through;\">(.*)</span>", Qt::CaseInsensitive);
    strikeTag.setMinimal(true);
    plainText.replace(strikeTag, "~\\1~");

    // 5. ADIM: Listeleme (•) işaretlerini veya fazladan kalan istenmeyen HTML etiketlerini sil
    QRegExp htmlTags("<[^>]+>");
    htmlTags.setMinimal(true);
    plainText.replace(htmlTags, "");

    // 6. ADIM: Formatlama sırasında (formatMessageLinks 7. adımda) eklenen kaçış (Escape) karakterlerini geri al
    plainText.replace("&amp;", "&");
    plainText.replace("&lt;", "<");
    plainText.replace("&gt;", ">");
    plainText.replace("&quot;", "\"");
    plainText.replace("&nbsp;", " ");

    return plainText;
}

void Database::startSyncLoop()
{
    m_initRun = m_settings.value("initRun", true).toBool();
    if (!m_initRun) {
        qDebug() << "[DATABASE] Setup not completed (initRun=false), retrying sync loop in 30s...";
        QTimer::singleShot(30000, this, SLOT(startSyncLoop()));
        return;
    }

    qDebug() << "[DATABASE] Starting Periodic Sync Loop...";

    if (m_syncTimer) {
        m_syncTimer->stop();
        m_syncTimer->deleteLater();
        m_syncTimer = 0;
    }

    m_syncTimer = new QTimer(this);
    connect(m_syncTimer, SIGNAL(timeout()), this, SLOT(performPeriodicSync()));

    // İlk senkronizasyonu hemen çalıştır ve zamanlayıcıyı 60 saniyeye ayarla
    performPeriodicSync();
    m_syncTimer->start(60000);
}

void Database::pauseSyncing()
{
    qDebug() << "[DATABASE] Pausing sync loop";
    if (m_syncTimer) m_syncTimer->stop();
}

void Database::resumeSyncing()
{
    qDebug() << "[DATABASE] Resuming sync loop";
    if (m_syncTimer) m_syncTimer->start();
}

void Database::performPeriodicSync()
{
    if (m_netConfManager && !m_netConfManager->isOnline()) {
        qDebug() << "[DATABASE] Device offline, skipping sync iteration";
        return;
    }

    m_accessToken = m_settings.value("accessToken", "").toString();
    m_url = m_settings.value("serverUrl", "").toString();

    if (m_accessToken.isEmpty() || m_url.isEmpty()) {
        qWarning() << "[DATABASE] Missing access token or server URL";
        return;
    }

    bool isPaginating = !m_nextCursor.isEmpty();
    QUrl url(m_url + "/v1/messages/search");

    if (isPaginating) {
        url.addQueryItem("cursor", m_nextCursor);
        url.addQueryItem("direction", "before");
    } else {
        QString ts = m_lastSyncTimestamp.isEmpty() ? "1970-01-01T00:00:00Z" : m_lastSyncTimestamp;
        url.addQueryItem("dateAfter", ts);
    }
    url.addQueryItem("limit", "10");

    // Database.hpp içindeki getActiveAccountIDs() fonksiyonunu kullanıyoruz
    QStringList activeAccounts = getActiveAccountIDs();
    foreach (const QString &accountID, activeAccounts) {
        if (!accountID.isEmpty()) {
            url.addQueryItem("accountIDs", accountID);
        }
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setRawHeader("Accept", "application/json");

    if (m_syncReply) {
        m_syncReply->deleteLater();
        m_syncReply = 0;
    }

    m_syncReply = m_netManager->get(request);
    if (m_syncReply) {
        connect(m_syncReply, SIGNAL(finished()), this, SLOT(onSyncResponseReceived()));
    }
}

void Database::onSyncResponseReceived()
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
            initContext();
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
                        sendNotificationToService(cAccountID, chatId,
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
        bb::system::InvokeRequest request;
        request.setTarget("com.example.BerryBridgeUIService");
        request.setAction("com.example.BerryBridgeUIService.DELAY_SYNC");
        m_invokeManager->invoke(request);
    }

    if (m_syncReply == reply) m_syncReply = 0;
    reply->deleteLater();
}

void Database::sendNotificationToService(
    const QString &accountID,
    const QString &chatID,
    const QString &senderName,
    const QString &msgType,
    const QString &text)
{
    bb::system::InvokeRequest request;
    request.setTarget("com.example.BerryBridgeUIService");
    request.setAction("com.example.BerryBridgeUIService.CREATE_NOTIFICATION");

    QVariantMap payload;
    payload["accountID"] = accountID;
    payload["chatID"] = chatID;
    payload["senderName"] = senderName;
    payload["msgType"] = msgType;
    payload["text"] = text;

    QByteArray data;
    bb::data::JsonDataAccess jda;
    jda.saveToBuffer(payload, &data);

    request.setData(data);
    request.setMimeType("application/json");

    if (m_invokeManager) {
        m_invokeManager->invoke(request);
    }
}
