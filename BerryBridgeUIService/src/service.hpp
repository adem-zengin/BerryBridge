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

#ifndef SERVICE_H_
#define SERVICE_H_

#include <QObject>
#include <QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslSocket>
#include <QTimer>
#include <QNetworkConfigurationManager>
#include <QStringList>
#include <QTcpSocket>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>
#include <QQueue>

namespace bb {
    class Application;
    namespace platform {
        class Notification;
    }
    namespace system {
        class InvokeManager;
        class InvokeRequest;
    }
}

class Service: public QObject
{
    Q_OBJECT
public:
    Service();
    virtual ~Service() {}
    // Pause/resume syncing for initial database download
    Q_INVOKABLE void pauseSyncing();
    Q_INVOKABLE void resumeSyncing();
    QStringList getActiveAccountIDs();
    Q_INVOKABLE void processChatAsync(const QString &msgId, const QString &chatId, const QString &previewJsonStr);
    Q_INVOKABLE void createMessageNotification(const QString& accountID, const QString& chatID, const QString& senderName,
                                              const QString& msgType, const QString& text);

signals:
    void messagesUpdated();

private slots:
    void handleInvoke(const bb::system::InvokeRequest &);
    void handleConnectivityChange(bool isOnline);
    void performPeriodicSync();
    void startSyncLoop();
    void onSyncResponseReceived();

    void startPushConnection();
    void onPushConnected();
    void onPushDisconnected();
    void onPushReadyRead();
    void onPushError(QAbstractSocket::SocketError socketError);
    void onPushPingTimeout();
    void onPushWatchdogTimeout();
    void onStatusUpdateTimeout();
    void onGlobalSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);

private:
    bb::platform::Notification * m_notify;
    bb::system::InvokeManager * m_invokeManager;
    QNetworkAccessManager * m_networkManager;
    QString m_pushType;
    QString m_pushSender;
    QString m_pushDeletedID;
    QString m_pushEditedID;
    QString m_accessToken;
    QString m_lastSyncTimestamp;
    QNetworkConfigurationManager * m_netConfManager;
    QTimer * m_syncTimer;
    QStringList m_selectedAccountIDs;  // User's selected accounts from QSettings
    bool isAccountSelected(const QString &accountID) const; // Check if account is selected by user
    QString getNetworkNameByAccountID(const QString &accountID);
    // Push connection objects
    //QTcpSocket * m_pushSocket;
    QSslSocket *m_pushSocket;
    QTimer * m_pushPingTimer;
    QTimer * m_pushWatchdogTimer;
    // Load user preferences from QSettings
    void loadUserPreferences();
    int m_pushSkipCount;
    bool m_initialSyncComplete;
    void sendStatusNotification(const QString &title, const QString &body);
    QSettings m_settings;
    QNetworkReply *m_syncReply;
    void initDatabases();
    void handlePushEvent(const QByteArray &payload);
    // Direct WebSocket State
    bool m_pushHandshakeDone;
    bool m_initRun;
    QString m_url;
    QByteArray m_wsBuffer;
    QVariantMap fetchChatMetadataSync(const QString &chatId);
    QString m_nextCursor;
    QString convertToPlainText(const QString& rawText);
    QVariantList fetchChatRecentMessagesSync(const QString &chatId, int limit);
    void markChatRead(const QString &chatID, const QString &messageID = QString());
    void markChatUnread(const QString &chatID, const QString &messageID = QString());
    QByteArray m_fragmentedPayload;
    int m_fragmentedOpcode;
    QString getTimestamp();
};

#endif /* SERVICE_H_ */
