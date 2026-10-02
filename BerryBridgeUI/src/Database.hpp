#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <QObject>
#include <QSettings>
#include <QVariantList>
#include <QVariantMap>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QStringList>
#include <QDir>
#include <bb/system/InvokeManager>
#include <QTimer>
#include <QtNetwork/QNetworkConfigurationManager>

class Database : public QObject
{
    Q_OBJECT
    // Property used in main.qml to check if setup is needed
    Q_PROPERTY(bool initRun READ initRun WRITE setInitRun NOTIFY initRunChanged)


public:
    explicit Database(QObject *parent = 0);
    bool initRun() const;
    Q_INVOKABLE void setInitRun(bool value);
    QStringList getActiveAccountIDs();
    Q_INVOKABLE QVariantList getSettingsList(); // eğer varsa hangi Beeper hesaplarının senk. için seçildiğini getirir.
    Q_INVOKABLE void fetchAccounts(); // settings.qml'den Beeper hesap listesi için istek gönderir.
    Q_INVOKABLE void updateSetting(const QString &accID, bool value); // settings.qml ve whatsAppSettings.qml
    Q_INVOKABLE bool getSetting(const QString &name, bool defaultValue); // whatsAppSettings.qml
    Q_INVOKABLE void initializeDatabaseSync(); // settings.qml initialize button
    Q_INVOKABLE QVariantList getSelectedAccountsForMain(); // main.qml'de tab oluşturma
    Q_INVOKABLE QVariantList getChatListForAccount(const QString &accountID, int limit = 25, int offset = 0); // bir tab için chat listesini veritabanından yükleme
    Q_INVOKABLE void syncChats(const QString &cursor, QString callbackAction);
    Q_INVOKABLE void setMute(const QString &chatID, bool value);
    Q_INVOKABLE void markChatAsRead(const QString &accountID, const QString &chatID); // whatsappTab.qml'de bir chat'e giriş için tıklandığında çalışır.
    Q_INVOKABLE void markChatAsUnread(const QString &accountID, const QString &chatID); // whatsappTab.qml'de bir chat'e giriş için tıklandığında çalışır.
    Q_INVOKABLE QVariantList getMessagesForChat(const QString &accountID, const QString &chatID, const QString &targetMsgId = "", int limit = 25, int offset = 0);
    Q_INVOKABLE void sendMessage(const QString &accountID, const QString &chatID, const QString &pendingMsgID, const QString &text = "", const QVariantMap &attachment = QVariantMap(), const QString &replyToMessageID = "");
    Q_INVOKABLE void uploadAssetAndSend(const QString &filePath, const QString &accountID, const QString &chatID, const QString &text = "", const QString &msgId = "", const QString &replyToMessageID="");
    Q_INVOKABLE void openMedia(const QString &localPath);
    Q_INVOKABLE void openImage(const QString &localPath);
    Q_INVOKABLE void openDocument(const QString &localPath);
    Q_INVOKABLE void markAllChatsAsRead(const QString &accountID);
    Q_INVOKABLE QVariantList getGroupsFromDb(const QString &searchQuery, const QString &accountID);
    Q_INVOKABLE QString searchChatByPhone(const QString &searchTerm, const QString &accID);
    Q_INVOKABLE QVariantList getDeviceContacts(const QString &filter = "");
    Q_INVOKABLE QString getNetworkNameByAccountID(const QString &accountID);
    Q_INVOKABLE void downloadAttachment(
        const QString &mxcUrl,
        const QString &messageId,
        const QString &fileName,
        const QString &type,
        const QString &accountID,
        const QString &preferredExt = "",
        const qint64 fileSize = 0 // Varsayılan değer eklendi
    );
    Q_INVOKABLE void copyToClipboard(const QString& text);
    Q_INVOKABLE void saveCredentials(const QString &key, const QString &value);
    Q_INVOKABLE QString getCredentials(const QString &key, const QString &defaultValue = "");
    Q_INVOKABLE void createChat(const QString& accountID, const QString& participantID, const QString& chatType);
    Q_INVOKABLE QVariantList searchMessages(const QString &accountID, const QString &query);
    Q_INVOKABLE void requestDelayedScroll();
    Q_INVOKABLE void sendReaction(const QString &chatID, const QString &msgID, const QString &reactionKey);
    Q_INVOKABLE void removeReaction(const QString &chatID, const QString &msgID, const QString &reactionKey);
    Q_INVOKABLE void deleteMessage(const QString &chatID, const QString &msgID, bool forEveryone);
    Q_INVOKABLE void editMessage(const QString &chatID, const QString &msgID, const QString &newText);
    Q_INVOKABLE void deleteChat(const QString &chatID);
    Q_INVOKABLE void setPin(const QString &chatID, bool value);
    Q_INVOKABLE QVariantMap getImageDimensions(const QString &filePath);
    Q_INVOKABLE QString toPlainText(const QString& htmlText);



signals:
    void dataRefreshRequested(); // main.qml dinliyor.
    void initRunChanged(); // ilk çalıştırma yapıldı sinyali
    void settingsReady(QVariantList list); // Beeper'daki hesap listesi
    void connectionStatus(const QString &response);
    void syncProgress(QString message, int progress); // Yüzde veya mesaj sayısını arayüze gönderir
    void syncComplete(); // settings.qml'de tüm işlem bittiğinde tetiklenir
    void chatsUpdated(QString accountID);
    void downloadProgress(const QString &messageId, int progress);
    void imageDownloaded(const QString &messageId, const QString &localPath);
    void chatCreatedSuccess(const QString& responseJson); // İstek tamamlandığında QML tarafına sonucu bildiren sinyal
    void chatCreatedError(int statusCode, const QString& errorString);
    void messageSentSuccessfully(const QString &pendingMsgId);
    void scrollToTargetRequested();
    void reactionSentSuccessfully(const QString &msgID, const QString &reactionKey);
    void reactionSendFailed(const QString &msgID, const QString &errorMsg);
    void reactionRemovedSuccessfully(const QString &msgID, const QString &reactionKey);
    void reactionRemoveFailed(const QString &msgID, const QString &errorMsg);
    void messageDeletedSuccessfully(const QString &msgID);
    void messageDeleteFailed(const QString &msgID, const QString &error);
    void messageEditedSuccessfully(const QString &msgID);
    void messageEditFailed(const QString &msgID, const QString &error);
    void chatDeletedSuccessfully(const QString &chatID);
    void chatMarkedReadSuccessfully();
    void chatMarkedUnreadSuccessfully();
    void requestNotification(const QString &msgId, const QString &accountID, const QString &chatID, const QString &senderName, const QString &msgType, const QString &text);
    void messagesUpdated();


private slots:
    void onAccountsFetched(); // Beeper'dan hesap isimlerini çekip işler
    void onInitialMessagesFetched();
    void onChatsFetched();
    void onChatsDetailsFetched();
    void onMessageSent();
    void onAttachmentDownloaded();
    void onDownloadProgress(qint64 bytesReceived);
    void onCreateChatFinished(); // Asenkron ağ yanıtını yakalayan slot
    void onAssetUploadFinished();
    void onSendReactionFinished();
    void onRemoveReactionFinished();
    void onDeleteMessageFinished();
    void onEditMessageFinished();
    void onUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void onGlobalSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    void onChatMarkedRead();
    void onChatMarkedUnread();
    void startSyncLoop();
    void pauseSyncing();
    void resumeSyncing();
    void performPeriodicSync();
    void onSyncResponseReceived();

private:
    bool m_initRun;
    QSettings m_settings;
    QNetworkAccessManager* m_netManager;
    bb::system::InvokeManager* m_invokeManager;
    QNetworkReply* m_accountsReply;
    QNetworkReply* m_chatsReply;
    QNetworkReply* m_chatsDetailsReply;
    QNetworkReply* m_initialSyncReply; // mesajların çekilmesi
    static const QString BEARER_TOKEN;
    static const QString API_BASE;
    bool initContext();

    // İndirme istatistikleri
    int m_totalMessagesDownloaded;

    // Yardımcı fonksiyon: Mesajları diske yazar
    //void saveMessagesToDb(const QVariantList &messages);

    // Yardımcı fonksiyon: Bir sonraki sayfayı ister
    void fetchInitialMessages(QString chatId, QString cursor);
    void fetchChatsDetails(QString chatId);

    // Yardımcı fonksiyon: İşlemi bitirir
    void finishDatabaseSync();
    int m_testLoopCounter; // Test için döngü sayacı
    int chatPercent; //percentage of chats its messages downloaded
    int totalChat;

    QStringList m_accountQueue;      // Senkronize edilecek hesaplar
    QString m_currentSyncAccountID;  // Şu an işlenen hesap
    QList<QVariant> m_syncChatQueue; // Mesajları indirilecek chat ID listesi
    void processNextChatFromQueue(); // Kuyruktaki sıradaki sohbeti işler
    void prepareMessageSyncQueue();

    bool removeDirectoryRecursively(const QDir &dir);
    QString formatTimeForDisplay(const QString &timestamp);
    QString getInitial(const QString &name);
    QString formatMessageLinks(const QString& rawText);
    QString convertToPlainText(const QString& rawText);

    QString m_url;
    QString m_accessToken;
    QTimer* m_syncTimer;
    QNetworkReply* m_syncReply;
    QNetworkConfigurationManager* m_netConfManager;
    QString m_lastSyncTimestamp;
    QString m_nextCursor;

    void sendNotificationToService(
            const QString &accountID,
            const QString &chatID,
            const QString &senderName,
            const QString &msgType,
            const QString &text
        );

};

#endif // DATABASE_HPP
