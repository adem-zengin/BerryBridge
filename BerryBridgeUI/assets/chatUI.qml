import bb.cascades 1.4
import bb.system 1.2
import bb.device 1.4

Page {
    id: chatPage
    property TabbedPane mainRef
    property string accountID
    property string chatID
    property string chatTitle
    property string chatType
    property string replyToMessageID:""
    property string replyMessage:""
    property string replyAttachImgUrl:""
    property bool replyAttachVisible:false
    property real replyAttachWidth
    property real replyAttachHeight
    property real replyAttachRatio:1.0
    property string replySender
    property bool readOnly
    property bool isReplying:false
    property int unreadCount
    property real startX: 0
    property real startY: 0
    property bool isEmojiPickerVisible: false
    property string targetMessageID: ""
    property bool isSearchMode: false // Arama modunu tutan bayrak
    property string attachUrl:""
    property bool attachSending:false
    property string attachFileSizeStr
    property string attachExtension
    property string attachFileName
    property string sendButtonUrl:"asset:///images/ic_microphone.png"
    property variant attachImageSize
    property string attachFileType
    
    //Load more button
    property int messageOffset: 0
    property int pageLimit: 25
    property bool hasMoreMessages: true
    
    // --- TEMA DEĞİŞKENLERİ ---
    property string primaryColor: "#444444"
    property string chatBgColor: "#E5DDD5"
    property string bubbleColor: "#d8fdd2"
    property variant datObject :dat
    property alias chatPage:chatPage
    property alias eCont: editCont
    property alias eLabel: editLabel
    property alias iField: inputField
    property bool isEditing: false
    property string editMsgID:""
    property alias atButton: attachButton
    
    resizeBehavior: PageResizeBehavior.Resize
    
    onIsEmojiPickerVisibleChanged: {
        if (isEmojiPickerVisible && messageModel.size() > 0) {
            // Çekmece açıldıysa ve mesaj varsa listeyi en alta kaydır
            listView.scrollToItem([messageModel.size() - 1], ScrollAnimation.Default);
        }
    }
    
    
    actionBarVisibility: ChromeVisibility.Hidden
    
    // HEX rengi belirtilen miktar kadar koyulaştıran fonksiyon
    function darkenColor(hexStr, amount) {
        if (!hexStr || hexStr.indexOf("#") !== 0) return "#333333"; // Güvenlifallback
        var num = parseInt(hexStr.replace("#", ""), 16);
        var r = Math.max(0, (num >> 16) - amount);
        var g = Math.max(0, ((num >> 8) & 0x00FF) - amount);
        var b = Math.max(0, (num & 0x0000FF) - amount);
        return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
    }
    
    function loadMoreMessages() {
        if (!hasMoreMessages) return;
        
        // 1. Yeni ofset değerini hesapla
        var nextOffset = messageOffset + pageLimit;
        
        // 2. C++ katmanından offset ile mesajları iste 
        // (C++ tarafında dat.getMessagesForChat(accId, chatId, targetId, limit, offset) şeklinde güncellenmelidir)
        var olderMessages = dat.getMessagesForChat(accountID, chatID, "", pageLimit, nextOffset);
        
        if (!olderMessages || olderMessages.length === 0) {
            hasMoreMessages = false;
            removeLoadMoreButton();
            return;
        }
        
        messageOffset = nextOffset;
        
        // 3. Geçici olarak Load More butonunu kaldır
        removeLoadMoreButton();
        
        // 4. Gelen eski mesajları listenin BAŞINA (kronolojik sırayla) ekle
        for (var i = 0; i < olderMessages.length; i++) {
            // Sürekli 0. indekse eklemek, gelen diziyi tersine çevirerek listeye dizer
            // Bu sayede en eski mesaj en üstte, daha yeniler onun altında kalır.
            messageModel.insert(0, olderMessages[i]);
        }
        
        // 5. Hala çekilecek mesaj varsa Load More butonunu tekrar başa koy
        if (olderMessages.length >= pageLimit) {
            ensureLoadMoreButton();
        }
    }
    
    // Load More butonunu modele ekleyen yardımcı fonksiyon
    function ensureLoadMoreButton() {
        // Çift eklemeyi önlemek için kontrol et
        if (messageModel.size() > 0 && messageModel.data([0]).isLoadMore) {
            return;
        }
        
        // Eski mesajlar üste geleceği için 0. indekse ekliyoruz
        // (Eğer listenin en altına eklemek isterseniz append kullanabilirsiniz)
        messageModel.insert(0, {
                "isLoadMore": true,
                "isHeader": false
        });
    }
    
    function removeLoadMoreButton() {
        if (messageModel.size() > 0 && messageModel.data([0]).isLoadMore) {
            messageModel.removeAt(0);
        }
    }
    
    // Beeper / Matrix sunucusuna gidecek Markdown dönüşümü
    function formatToOutgoingMarkdown(text) {
        if (!text) return "";
        
        var raw = text;
        var protectedBlocks = [];
        
        // 1. Çoklu satır kod bloklarını koru ve yeni satır birikmelerini temizle
        raw = raw.replace(/```([\s\S]*?)```/g, function(match, innerContent) {
                // Baştaki ve sondaki tüm \r ve \n karakterlerini tamamen temizle (ES5 Regex)
                var cleanCode = innerContent.replace(/^[\r\n]+|[\r\n]+$/g, "");
                
                // Tam olarak 1 adet açılış ve 1 adet kapanış satırı ile paketle
                var formattedCodeBlock = "```\n" + cleanCode + "\n```";
                
                protectedBlocks.push(formattedCodeBlock);
                return "@@PROTECTED_" + (protectedBlocks.length - 1) + "@@";
        });
        
        // 2. Tekli satır (inline) kod bloklarını koru (`kod`)
        raw = raw.replace(/`([^`\n]+)`/g, function(match) {
                protectedBlocks.push(match);
                return "@@PROTECTED_" + (protectedBlocks.length - 1) + "@@";
        });

        // 3. URL / Link yapısını koru
        var urlRegex = /(https?:\/\/[^\s]+|www\.[^\s]+)/gi;
        raw = raw.replace(urlRegex, function(match) {
                protectedBlocks.push(match);
                return "@@PROTECTED_" + (protectedBlocks.length - 1) + "@@";
        });
        
        // 4. Kalın dönüşümü: *metin* -> **metin**
        raw = raw.replace(/\*([^\*\n]+)\*/g, '**$1**');
        
        // 5. İtalik dönüşümü: _metin_ -> *metin*
        raw = raw.replace(/_([^\_\n]+)_/g, '*$1*');
        
        // 6. Korumaya alınan yapıları geri yükle
        raw = raw.replace(/@@PROTECTED_(\d+)@@/g, function(match, index) {
                return protectedBlocks[parseInt(index)];
        });
        
        return raw;
    }
    
    // Ekranda Optimistic UI baloncuğunda görünecek HTML dönüşümü
    function formatToHtml(text) {
        if (!text) return "";
        
        var protectedBlocks = [];
        var raw = text;
        
        // 1. Çoklu satır kod bloklarını koru (```kod```)
        raw = raw.replace(/```([\s\S]*?)```/g, function(match, codeContent) {
                protectedBlocks.push({ type: "code_multi", content: codeContent });
                return "@@PROTECTED_" + (protectedBlocks.length - 1) + "@@";
        });
        
        // 2. Tekli satır (inline) kod bloklarını koru (`kod`)
        raw = raw.replace(/`([^`\n]+)`/g, function(match, codeContent) {
                protectedBlocks.push({ type: "code_single", content: codeContent });
                return "@@PROTECTED_" + (protectedBlocks.length - 1) + "@@";
        });

        // 3. URL/Link yapılarını koru (İçindeki _ veya * karakterlerinin bozulmaması için)
        var urlRegex = /(https?:\/\/[^\s]+|www\.[^\s]+)/gi;
        raw = raw.replace(urlRegex, function(match) {
                protectedBlocks.push({ type: "url", content: match });
                return "@@PROTECTED_" + (protectedBlocks.length - 1) + "@@";
        });
        
        // 4. Genel HTML Karakterlerini Escape Et
        var formatted = raw.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
        
        // 5. Üstü Çizili (Markdown ~metin~ ve escaped <del>/<s> etiketleri)
        formatted = formatted.replace(/~([^~\n]+)~/g, '<span style="text-decoration: line-through;">$1</span>');
        formatted = formatted.replace(/&lt;del[^&]*&gt;(.*?)&lt;\/del&gt;/gi, '<span style="text-decoration: line-through;">$1</span>');
        formatted = formatted.replace(/&lt;s[^&]*&gt;(.*?)&lt;\/s&gt;/gi, '<span style="text-decoration: line-through;">$1</span>');
        
        // 6. Kalın ve İtalik
        formatted = formatted.replace(/\*([^\*\n]+)\*/g, '<b>$1</b>');
        formatted = formatted.replace(/_([^\_\n]+)_/g, '<i>$1</i>');
        
        // 7. Satır atlamaları
        formatted = formatted.replace(/\n/g, "<br/>");
        
        // 8. Korumaya alınan yapıları güvenli HTML olarak geri yükle
        formatted = formatted.replace(/@@PROTECTED_(\d+)@@/g, function(match, index) {
                var item = protectedBlocks[parseInt(index)];
                var safeContent = item.content.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
                
                if (item.type === "code_multi") {
                    return '<span style="font-family:monospace;">' + safeContent.replace(/\n/g, "<br/>") + '</span>';
                } else if (item.type === "code_single") {
                    return '<span style="font-family:monospace; background-color:rgba(0,0,0,0.12);">' + safeContent + '</span>';
                }
                return safeContent; // URL
        });
        
        return formatted;
    }
    
    function normalizeForMatching(str) {
        if (!str) return "";
        var cleaned = str;
        
        // 1. Satır sonları ve <br/> etiketlerini boşluğa çevir (Eşleşme kopmalarını önler)
        cleaned = cleaned.replace(/<br\s*\/?>/gi, " ").replace(/\r?\n/g, " ");
        
        // 2. Tüm HTML etiketlerini temizle
        cleaned = cleaned.replace(/<[^>]+>/g, "");
        
        // 3. HTML Entity'lerini çöz
        cleaned = cleaned.replace(/&amp;/g, "&")
        .replace(/&lt;/g, "<")
        .replace(/&gt;/g, ">")
        .replace(/&quot;/g, "\"")
        .replace(/&#39;/g, "'");
        
        // 4. Biçimlendirme karakterlerini temizle (*, _, ~, `)
        cleaned = cleaned.replace(/[\*\_\~\`]/g, "");
        
        // 5. Arda arda gelen boşlukları teke indir ve küçük harfe çevir
        return cleaned.replace(/\s+/g, " ").trim().toLowerCase();
    }
    
    function handleDbUpdate() {
        console.log("[CHAT-UI] Dynamic variable dbUpdateTrigger changed! Checking for relevance...");
        if (!accountID || !chatID) return;
        
        var newMessages = dat.getMessagesForChat(accountID, chatID, "", pageLimit, messageOffset);
        if (!newMessages || newMessages.length === 0) {
            if (messageModel.size() > 0) loadMessages();
            return;
        }
        
        var hasChanges = false; // Değişim takip bayrağı
        var fetchedMap = {};
        for (var i = 0; i < newMessages.length; i++) {
            var nm = newMessages[i];
            if (nm && nm.id) {
                fetchedMap[nm.id] = nm;
            }
        }
        
        var hasDeletedMessage = false;
        var pendingIndexes = [];
        
        // =========================================================
        // AŞAMA 1: MEVCUT VE GEÇİCİ MESAJLARI KONTROL ET
        // =========================================================
        for (var m = 0; m < messageModel.size(); m++) {
            var modelItem = messageModel.value(m);
            if (!modelItem || modelItem.isHeader || !modelItem.id) continue;
            
            if (modelItem.id.toString().indexOf("pending_") === 0) {
                pendingIndexes.push(m);
                continue; 
            }
            
            var dbItem = fetchedMap[modelItem.id];
            
            if (!dbItem) {
                if (m > messageModel.size() - 20) {
                    console.log("[CHAT-UI] Message deletion detected for Msg ID:", modelItem.id);
                    hasDeletedMessage = true;
                    //hasChanges = true; // 1. Değişim: Silinme tespit edildi
                }
            } else {
                var dbReactions = dbItem.reactionsText ? dbItem.reactionsText : "";
                var modelReactions = modelItem.reactionsText ? modelItem.reactionsText : "";
                var dbMyReaction = dbItem.myReaction ? dbItem.myReaction : "";
                var modelMyReaction = modelItem.myReaction ? modelItem.myReaction : "";
                var dbText = dbItem.text ? dbItem.text : "";
                var modelText = modelItem.text ? modelItem.text : "";
                var dbSeen = dbItem.isSeen === true;
                var modelSeen = modelItem.isSeen === true;
                
                if (dbReactions !== modelReactions || dbMyReaction !== modelMyReaction || dbText !== modelText || dbSeen !== modelSeen) {
                    dbItem.isDownloading = modelItem.isDownloading;
                    dbItem.downloadProgress = modelItem.downloadProgress;
                    dbItem.localImagePath = modelItem.localImagePath;
                    messageModel.replace(m, dbItem);
                    //hasChanges = true; // 2. Değişim: Mesaj içeriği/reaksiyon güncellendi
                }
            }
        }
        
        // =========================================================
        // AŞAMA 2: YENİ MESAJLARI EKLE & GEÇİCİLERİ GERÇEĞE ÇEVİR
        // =========================================================
        var lastMsgIdInModel = null;
        var lastDateHeader = "";
        
        for (var k = messageModel.size() - 1; k >= 0; k--) {
            var item = messageModel.value(k);
            if (item.isHeader && lastDateHeader === "") {
                lastDateHeader = item.headerText;
            } else if (!item.isHeader && item.id && item.id.toString().indexOf("pending_") !== 0 && !lastMsgIdInModel) {
                lastMsgIdInModel = item.id;
            }
            if (lastMsgIdInModel && lastDateHeader !== "") break;
        }
        
        var lastIndexInDb = -1;
        for (var j = 0; j < newMessages.length; j++) {
            if (newMessages[j].id === lastMsgIdInModel) {
                lastIndexInDb = j;
                break;
            }
        }
        
        if (hasDeletedMessage || (lastMsgIdInModel !== null && lastIndexInDb === -1)) {
            console.log("[CHAT-UI] Deletion or structural change. Doing full reload.");
            loadMessages();
            //dat.markChatAsRead(accountID, chatID); // Yapısal değişim/silinmede okundu yap
            return;
        }
        
        if (lastIndexInDb > 0) {
            var batchedNewMessages = [];
            
            for (var index = lastIndexInDb - 1; index >= 0; index--) {
                var newMsg = newMessages[index];
                var matchedPendingIndex = -1;
                
                if (newMsg.isSender) {
                    // Veritabanından gelen metnin HTML etiketlerini soyup ham halini alıyoruz
                    var newNormText = normalizeForMatching(newMsg.text);
                    
                    for (var p = 0; p < pendingIndexes.length; p++) {
                        var pIdx = pendingIndexes[p];
                        var pItem = messageModel.value(pIdx);
                        
                        // Bekleyen mesajın da HTML etiketlerini soyuyoruz
                        var pNormText = normalizeForMatching(pItem.text);
                        
                        // === YENİ EŞLEŞME KONTROLÜ ===
                        if (pNormText === newNormText) {
                            // 1. Durum: Metin var ve aynı (Örn: "Merhaba")
                            // 2. Durum: Metin boş (Sadece attachment) ama mesaj Tipleri (IMAGE, VIDEO, AUDIO) aynı!
                            if (pNormText.length > 0 || pItem.type === newMsg.type) {
                                matchedPendingIndex = pIdx;
                                pendingIndexes.splice(p, 1); // Eşleşeni listeden çıkar
                                break;
                            }
                        }
                    }
                }
                
                if (matchedPendingIndex !== -1) {
                    messageModel.replace(matchedPendingIndex, newMsg);
                    hasChanges = true; // Bekleyen mesaj gerçek veritabanı mesajına dönüştü
                } else {
                    if (newMsg.dateHeader && newMsg.dateHeader !== lastDateHeader) {
                        batchedNewMessages.push({
                                "isHeader": true, 
                                "headerText": newMsg.dateHeader
                        });
                    lastDateHeader = newMsg.dateHeader;
                    }
                    batchedNewMessages.push(newMsg);
                }
            }
            
            if (batchedNewMessages.length > 0) {
                messageModel.append(batchedNewMessages);
                listView.scrollToItem([messageModel.size() - 1], ScrollAnimation.Default);
                hasChanges = true; // Yeni mesaj eklendi
            }
        }
        
        // --- SONUÇ: Yalnızca değişiklik varsa Okundu İşaretle ---
        if (hasChanges) {
            console.log("[CHAT-UI] Message changes detected. Marking chat as read.");
            dat.markChatAsRead(accountID, chatID);
        }
    }
    function handleImageDownloaded(messageId, localPath) {
        console.log("[CHAT-UI] Image downloaded signal received for " + messageId);
        for (var i = 0; i < messageModel.size(); i++) {
            var item = messageModel.value(i);
            if (item.id === messageId) {
                // Update item with localImagePath
                item.localImagePath = localPath;
                item.isDownloading = false;
                item.downloadProgress = 0;
                messageModel.replace(i, item);
                break;
            }
        }
    }
    
    function handleDownloadProgress(messageId, progress) {
        for (var i = 0; i < messageModel.size(); i++) {
            var item = messageModel.value(i);
            if (item.id === messageId) {
                item.downloadProgress = progress;
                item.isDownloading = true;
                messageModel.replace(i, item);
                break;
            }
        }
    }
    
    function cleanup() {
        console.log("[CHAT-UI] Cleaning up chatUI...");
        app.dbUpdateTriggerChanged.disconnect(chatPage.handleDbUpdate);
        dat.imageDownloaded.disconnect(chatPage.handleImageDownloaded);
        dat.downloadProgress.disconnect(chatPage.handleDownloadProgress);
    }
    
    titleBar: TitleBar {
        title: chatPage.chatTitle
        scrollBehavior: TitleBarScrollBehavior.Sticky
    }
    
    onCreationCompleted: {
        app.dbUpdateTriggerChanged.connect(chatPage.handleDbUpdate);
        dat.imageDownloaded.connect(chatPage.handleImageDownloaded);
        dat.downloadProgress.connect(chatPage.handleDownloadProgress);
        dat.scrollToTargetRequested.connect(chatPage.executeScroll);
        //loadMessages();
        listView.requestFocus();
        
        var supportedEmojis = [
        "😀", "😁", "😂", "😃", "😄", "😅", "😆", "😉", "😊", "😋", "😎", 
        "😍", "😘", "😗", "😙", "😚", "☺", "😐", 
        "😑", "😶", "😏", "😣", "😥", "😮",  "😯", "😪", "😫", "😴", 
        "😌", "😛", "😜", "😝", "😒", "😓", "😔", "😕", "😲", 
        "👍", "👎", "👌", "✌", "👈", "👉", "👆", "👇", 
        "☝", "✋", "👋", "👏", "🙌", "🙏", "❤", "🔥"
        ];
        
        for (var i = 0; i < supportedEmojis.length; i++) {
            emojiPickerModel.append(supportedEmojis[i]);
        }
    }
    
    Container {
        id: mainC
        layout: DockLayout {}
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        background: Color.create(chatPage.chatBgColor)
        
        /*ImageView {
         imageSource: "asset:///images/ChatBack.jpg"
         
         // Resmi container boyutlarına yaymak için
         horizontalAlignment: HorizontalAlignment.Fill
         verticalAlignment: VerticalAlignment.Fill
         }*/
        
        
        // Wrapper to handle vertical stacking of list and input bar
        Container {
            layout: StackLayout {}
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            
            ListView {
                id: listView
                dataModel: messageModel
                property variant rootPage: chatPage
                property variant navPane: navigationPane
                opacity: 1.0
                
                // Use spaceQuota so the list shrinks when the keyboard opens
                layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
                horizontalAlignment: HorizontalAlignment.Fill
                
                attachedObjects: [
                    LayoutUpdateHandler {
                        id: listLayoutHandler
                        property real lastHeight: 0 
                        
                        onLayoutFrameChanged: {
                            // 1. Kalkan: lastHeight 0'dan büyük olmalı. 
                            // Bu, sayfanın ilk 0 boyutundan gerçek boyutuna geçişindeki sıçramayı yoksayar.
                            if (lastHeight > 0 && layoutFrame.height < lastHeight) {
                                
                                // 2. Kalkan: Sadece inputField'a dokunulduysa (klavye açılıyorsa) 
                                // VEYA emoji menüsü açılıyorsa kaydır.
                                // Böylece sayfa ilk açılışındaki layout güncellemeleri tamamen es geçilir.
                                if (inputField.focused || chatPage.isEmojiPickerVisible) {
                                    if (messageModel && messageModel.size() > 0) {
                                        listView.scrollToPosition(ScrollPosition.End, ScrollAnimation.None);
                                    }
                                }
                            }
                            
                            // Sonraki kıyaslama için mevcut yüksekliği kaydet
                            lastHeight = layoutFrame.height;
                        }
                    }
                ]
                
                // Property to store page variables so the delegates can reach them
                property string chatType: chatPage.chatType
                property int displayWidth: displayInfo.pixelSize.width
                
                // Add a hook to call functions from delegate
                function requestAttachmentDownload(mxcUrl, msgId, fileName, type, ext, fSize) {
                    console.log("[CHAT-UI] Delegate requested attachment download: " + mxcUrl + " type: " + type + " ext: " + ext);
                    for (var i = 0; i < messageModel.size(); i++) {
                        var item = messageModel.value(i);
                        if (item.id === msgId) {
                            item.isDownloading = true;
                            item.downloadProgress = 0;
                            messageModel.replace(i, item);
                            break;
                        }
                    }
                    dat.downloadAttachment(mxcUrl, msgId, fileName, type, chatPage.accountID, ext || "", fSize);
                }
                
                function playMedia(path) {
                    console.log("[CHAT-UI] playVideo tapped: " + path);
                    dat.openMedia(path);
                }
                
                function openDocument(path) {
                    console.log("[CHAT-UI] openDocument tapped: " + path);
                    dat.openDocument(path);
                }
                
                function openPhoto(path) {
                    console.log("[CHAT-UI] openImage tapped: " + path);
                    dat.openImage(path);
                }
        
                // Reduced padding since we are now using a StackLayout
                bottomPadding: ui.du(2.0)
                
                listItemComponents: [
                    // --- YENİ EKLENEN: GÜN AYRACI TASARIMI ---
                    ListItemComponent {
                        type: "header"
                        // Ana konteyner ekranın tamamına yayılır
                        Container {
                            horizontalAlignment: HorizontalAlignment.Fill
                            topPadding: ui.du(2.5)
                            bottomPadding: ui.du(2.5)
                            
                            // StackLayout kullanarak elemanları yan yana (LeftToRight) diziyoruz
                            layout: StackLayout {
                                orientation: LayoutOrientation.LeftToRight
                            }
                            
                            // 1. SOL BOŞLUK: Esnek yapısıyla içeriği sağa iter
                            Container {
                                layoutProperties: StackLayoutProperties {
                                    spaceQuota: 1.0 // Ekranın boş kalan kısmını kaplar
                                }
                            }
                            
                            // 2. ORTA İÇERİK: Asıl tarih balonumuz
                            Container {
                                background: Color.create("#f5f5f5")
                                leftPadding: ui.du(2.5)
                                rightPadding: ui.du(2.5)
                                topPadding: ui.du(0.8)
                                bottomPadding: ui.du(0.8)
                                
                                Label {
                                    text: ListItemData.headerText
                                    textStyle.fontSize: FontSize.XSmall
                                    textStyle.fontWeight: FontWeight.W500
                                    textStyle.color: Color.create("#444444")
                                    // Metin çok uzunsa kendi içinde de ortalansın
                                    horizontalAlignment: HorizontalAlignment.Center
                                }
                            }
                            
                            // 3. SAĞ BOŞLUK: Esnek yapısıyla içeriği sola iter
                            Container {
                                layoutProperties: StackLayoutProperties {
                                    spaceQuota: 1.0 // Ekranın boş kalan kısmını kaplar
                                }
                            }
                        }
                    },
                    
                    ListItemComponent {
                        type: "message"
                        
                        Container {
                            id: rootMessageItem
                            // En dış seviyede ListView referansını sabitliyoruz
                            property variant listView: ListItem.view
                            property string chatType: listView ? listView.chatType : ""
                            horizontalAlignment: HorizontalAlignment.Fill
                            ControlDelegate {
                                visible: ListItemData.isLoadMore === true
                                delegateActive: visible
                                
                                sourceComponent: ComponentDefinition {
                                    Container {
                                        horizontalAlignment: HorizontalAlignment.Fill
                                        topPadding: ui.du(2.5)
                                        bottomPadding: ui.du(1.0)
                                        layout: StackLayout {
                                            orientation: LayoutOrientation.LeftToRight
                                        }
                                        
                                        // Sol tarafı dolduran esnek boşluk
                                        Container {
                                            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                                        }
                                        
                                        Button {
                                            text: "Load More Messages"
                                            onClicked: {
                                                rootMessageItem.listView.rootPage.loadMoreMessages();
                                            }
                                            color: Color.create("#ff282828")
                                        }
                                        
                                        // Sağ tarafı dolduran esnek boşluk
                                        Container {
                                            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                                        }
                                    }
                                }
                            }
                            
                            // Normal Mesaj Görünümü
                            Container {
                                visible: !ListItemData.isLoadMore
                                Container {
                                    id: msgContainer
                                    property string chatType: rootMessageItem.listView.chatType
                                    contextActions: [
                                        ActionSet {
                                            title: "Message Options"
                                            //subtitle: "Message Options"
                                            
                                            ActionItem {
                                                title: "Copy"
                                                imageSource:"asset:///images/ic_copy.png"
                                                enabled:ListItemData.text!=""
                                                
                                                onTriggered: {
                                                    var cLV = rootMessageItem.listView;
                                                    cLV.rootPage.datObject.copyToClipboard(ListItemData.text)
                                                }
                                            }
                                            
                                            ActionItem {
                                                property string upperType: ListItemData.type ? ListItemData.type.toString().toUpperCase() : ""
                                                title: "Edit"
                                                imageSource:"asset:///images/ic_edit.png"
                                                enabled:ListItemData.isSender && ListItemData.type.toString().toUpperCase()=="TEXT"
                                                onTriggered: {
                                                    var cLV = rootMessageItem.listView;
                                                    cLV.opacity=0.1;
                                                    cLV.rootPage.eCont.visible=true;
                                                    cLV.rootPage.atButton.visible=false;
                                                    cLV.rootPage.eLabel.text=ListItemData.text;
                                                    cLV.rootPage.iField.text=cLV.rootPage.datObject.toPlainText(ListItemData.text);
                                                    cLV.rootPage.iField.requestFocus();
                                                    cLV.rootPage.sendButtonUrl="asset:///images/ic_done.png"
                                                    cLV.rootPage.isEditing=true;
                                                    cLV.rootPage.editMsgID=ListItemData.id;
                                                    
                                                }
                                            }
                                            
                                            ActionItem {
                                                title: "Reply"
                                                imageSource:"asset:///images/ic_reply.png"
                                                property bool canReply: rootMessageItem.listView.rootPage.readOnly
                                                enabled: !canReply
                                                
                                                onTriggered: {
                                                    var cLV = rootMessageItem.listView;                                            
                                                    cLV.rootPage.replyToMessageID=ListItemData.id;
                                                    var rText = ListItemData.text;
                                                    
                                                    cLV.rootPage.replySender=ListItemData.isSender ? "You":ListItemData.senderName
                                                    cLV.rootPage.isReplying=true;
                                                    if(ListItemData.type==="IMAGE"){
                                                        var ratio=ListItemData.size ? ListItemData.size.height/ListItemData.size.width:1
                                                        cLV.rootPage.replyAttachWidth=rootMessageItem.listView.displayWidth * 0.1;
                                                        cLV.rootPage.replyAttachHeight=cLV.rootPage.replyAttachWidth*ratio
                                                        cLV.rootPage.replyAttachVisible=true
                                                        cLV.rootPage.replyAttachRatio=ratio
                                                        if(ListItemData.localImagePath){
                                                            cLV.rootPage.replyAttachImgUrl=ListItemData.localImagePath
                                                        }else{
                                                            cLV.rootPage.replyAttachImgUrl="asset:///images/ic_view_image.png"
                                                        }                                                                     
                                                    }else if(ListItemData.type==="VIDEO"){                                               
                                                        rText = "🎥 Video "+rText;
                                                    } else if (ListItemData.type==="AUDIO" || ListItemData.type==="VOICE") {
                                                        rText = "🔉 Audio "+rText;
                                                    } else if (ListItemData.type==="FILE") {
                                                        if(rText===""){
                                                            rText = "📄  "+ListItemData.fileName;
                                                        }else{
                                                            rText = "📄  "+ListItemData.fileName+" : "+rText;
                                                        }
                                                    
                                                    }  else if (ListItemData.type==="LOCATION") {
                                                        rText = "📍 Location "+rText;
                                                    } else if (ListItemData.type==="STICKER") {
                                                        rText = "🌞 Sticker "+rText;
                                                    }
                                                    cLV.rootPage.replyMessage=rText;
                                                
                                                }
                                            }
                                            
                                            ActionItem {
                                                // 1. KULLANICI REAKSİYONU KONTROLÜ
                                                // (ListItemData.myReaction veritabanından gelen kendi emojinizdir, örn: "👍")
                                                property string userReaction: (ListItemData && ListItemData.myReaction) ? ListItemData.myReaction : ""
                                                property bool canReact: rootMessageItem.listView.rootPage.readOnly
                                                
                                                // 2. DİNAMİK BAŞLIK VE İKON
                                                title: userReaction !== "" ? "Remove " + userReaction : "React"
                                                imageSource: userReaction !== "" ? "asset:///images/ic_cancel.png" : "asset:///images/ic_emoji.png"
                                                enabled: !canReact
                                                
                                                // 3. DİNAMİK TETİKLENME (Tıklandığında Ne Yapılacak?)
                                                onTriggered: {
                                                    var cLV = rootMessageItem.listView;
                                                    var chatID = cLV.rootPage.chatID;
                                                    var msgID = ListItemData.id;
                                                    
                                                    if (userReaction !== "") {
                                                        // Reaksiyon ZATEN VAR -> Reaksiyonu Sil
                                                        console.log("[QML ACTION] Removing reaction:", userReaction, "for Msg:", msgID);
                                                        cLV.rootPage.datObject.removeReaction(chatID, msgID, userReaction);
                                                    } else {
                                                        // Reaksiyon YOK -> Emoji Seçim Sayfasını Aç
                                                        var emPage = emPageDefinition.createObject();
                                                        emPage.chatID = chatID;
                                                        emPage.msgID = msgID;
                                                        emPage.nPane = cLV.navPane;
                                                        cLV.navPane.push(emPage);
                                                    }
                                                }
                                            }             attachedObjects: [
                                                ComponentDefinition {
                                                    id: emPageDefinition
                                                    source: "emoji.qml"
                                                }, SystemDialog {
                                                    id: deleteConfirmDialog
                                                    title: "Delete Message"
                                                    
                                                    // Buton tanımlamaları (3 Seçenek)
                                                    confirmButton.label: "For everyone"
                                                    confirmButton.enabled: ListItemData.isSender;
                                                    customButton.label: "For me"
                                                    cancelButton.label: "Cancel"
                                                    
                                                    // onFinished: Kullanıcı butona bastığında tetiklenir
                                                    onFinished: {
                                                        // ListItemData üzerinden o anki listenin datasını çekiyoruz.
                                                        // Sizin modelinizde anahtarlar farklıysa (örn. chat_id, id vb.) buraları güncelleyin.
                                                        var cLV = rootMessageItem.listView;
                                                        var cID = cLV.rootPage.chatID;
                                                        var mID = ListItemData.id;
                                                        
                                                        if (value == SystemUiResult.ConfirmButtonSelection) {
                                                            // "For everyone" seçildi
                                                            cLV.rootPage.datObject.deleteMessage(cID, mID, true);
                                                        } 
                                                        else if (value == SystemUiResult.CustomButtonSelection) {
                                                            // "For me" seçildi
                                                            cLV.rootPage.datObject.deleteMessage(cID, mID, false);
                                                        } 
                                                        else if (value == SystemUiResult.CancelButtonSelection) {
                                                            // "Cancel" seçildi (Ekstra bir şey yapmaya gerek yok, diyalog kapanır)
                                                        }
                                                    }
                                                }
                                            ]
                                            
                                            DeleteActionItem {
                                                title: "Delete Message"
                                                onTriggered: {
                                                    deleteConfirmDialog.show();
                                                }
                                            } 
                                        
                                        
                                        }
                                    ]
                                    horizontalAlignment: HorizontalAlignment.Fill
                                    layout: StackLayout {
                                        orientation: LayoutOrientation.LeftToRight
                                    }
                                    leftPadding: ui.du(2.0); rightPadding: ui.du(2.0); topPadding: ui.du(1.0); bottomPadding: ui.du(1.0)
                                    
                                    // Reference the ListView property via the reserved ListItem.view pointer
                                    //property string chatType: rootMessageItem.listView.chatType
                                    
                                    // Spacer pushes our replies to the right
                                    Container {
                                        visible: ListItemData.isSender
                                        layoutProperties: StackLayoutProperties {
                                            spaceQuota: 1.0
                                        }
                                        minWidth: ui.du(10.0)
                                    }
                                    
                                    Container {
                                        background: ListItemData.isSender ? Color.create(rootMessageItem.listView.rootPage.bubbleColor) : Color.White
                                        leftPadding: ui.du(1.5); rightPadding: ui.du(1.5); topPadding: ui.du(1.0); bottomPadding: ui.du(1.0)
                                        layout: StackLayout {
                                        }
                                        
                                        // Show sender name if it's a group and not from us
                                        Label {
                                            id: senderNameLabel                                                   
                                            visible: (rootMessageItem.chatType !== "single" && rootMessageItem.chatType !== "channel" && ! ListItemData.isSender && ListItemData.senderName.indexOf("bot") === -1);
                                            text: ListItemData.senderName
                                            textStyle.fontWeight: FontWeight.Bold
                                            textStyle.color: Color.create(rootMessageItem.listView.rootPage.darkenColor(rootMessageItem.listView.rootPage.primaryColor,90))
                                            textStyle.fontSize: FontSize.XSmall
                                        }
                                        
                                        // Show mention container
                                        Container {
                                            id: mentionCont
                                            visible: ListItemData.hasMention                                   
                                            background: ListItemData.isSender ? Color.create(rootMessageItem.listView.rootPage.darkenColor(rootMessageItem.listView.rootPage.bubbleColor,15)): Color.create("#f6f5f3")
                                            bottomMargin: ui.du(1.0)
                                            layout: StackLayout { orientation: LayoutOrientation.LeftToRight}
                                            Container {
                                                preferredWidth:ui.du(1.0)
                                                minWidth: ui.du(1.0)
                                                verticalAlignment: VerticalAlignment.Fill
                                                background: ListItemData.mentionSenderName==="You" ? Color.create(rootMessageItem.listView.rootPage.darkenColor(rootMessageItem.listView.rootPage.primaryColor,50)):Color.Gray
                                            }
                                            Container {
                                                bottomMargin: ui.du(1.0)
                                                layout: StackLayout { orientation: LayoutOrientation.TopToBottom }
                                                
                                                // İç boşluklar (Padding)
                                                leftPadding: ui.du(1.0)
                                                rightPadding: ui.du(1.0)
                                                topPadding: ui.du(0.5)
                                                bottomPadding: ui.du(1.0)
                                                rightMargin: 0
                                                
                                                Label {
                                                    id: mentionSender
                                                    // C++'tan gelen veriyi doğrudan okuyoruz
                                                    text: ListItemData.mentionSenderName !== undefined ? ListItemData.mentionSenderName : "Bilinmeyen"
                                                    //textStyle.color: Color.create("#075E54")
                                                    textStyle.fontSize: FontSize.XSmall
                                                    textStyle.fontWeight: FontWeight.Bold
                                                    bottomMargin: 0
                                                    //visible: rootMessageItem.chatType !== "single"; 
                                                }
                                                
                                                Label {
                                                    id: mentionMessage
                                                    // C++'tan gelen attachment türü veya mesaj içeriği
                                                    text: ListItemData.mentionText !== undefined ? ListItemData.mentionText : ""
                                                    textStyle.fontSize: FontSize.XSmall
                                                    //textStyle.color: Color.Gray
                                                    topMargin: 0
                                                    multiline: true
                                                    autoSize.maxLineCount: 2
                                                    textFormat: TextFormat.Html
                                                    visible: ListItemData.mentionText != ""
                                                }
                                            }
                                            
                                            ImageView {
                                                property real menRatio: ListItemData.mentionImgRatio ? ListItemData.mentionImgRatio:1
                                                property real menContWidth: rootMessageItem.listView.displayWidth * 0.1
                                                property real menContHeight: menContWidth*menRatio
                                                property bool isMentUrl: ListItemData.mentionImgLocalUrl!==undefined
                                                property bool isAsset: isMentUrl ? ListItemData.mentionImgLocalUrl.indexOf("asset") !== -1:false
                                                
                                                visible: ListItemData.isMentionImg
                                                //preferredWidth: menContWidth
                                                //preferredHeight: menContHeight
                                                preferredWidth: {if (isAsset) return ui.du(8.0)
                                                else return menContWidth}
                                                minWidth: {if(isAsset) {return ui.du(8.0)}
                                                else{return menContWidth}}
                                                preferredHeight: {if(isAsset) {return undefined}
                                                else{return menContHeight}}
                                                imageSource: isMentUrl ? ListItemData.mentionImgLocalUrl:""
                                                horizontalAlignment: HorizontalAlignment.Left
                                                verticalAlignment: VerticalAlignment.Fill
                                                scalingMethod:ScalingMethod.AspectFit
                                                leftMargin: 0
                                            }
                                        }
                                        
                                        
                                        // Show image or placeholder icon in a gray clickable container
                                        Container {
                                            id: imgCont
                                            property real localX: 0
                                            property real localY: 0
                                            property real ratio:ListItemData.size ? ListItemData.size.height/ListItemData.size.width:1
                                            property real contWidth: ListItemData.text===""?rootMessageItem.listView.displayWidth*0.5:rootMessageItem.listView.displayWidth*0.7
                                            property real contHeight: contWidth*ratio
                                            visible: (ListItemData.type && ListItemData.type === "IMAGE")
                                            background: ListItemData.isSender ? Color.create(rootMessageItem.listView.rootPage.darkenColor(rootMessageItem.listView.rootPage.bubbleColor,15)): Color.create("#f6f5f3")
                                            preferredWidth: ListItemData.size ? contWidth : rootMessageItem.listView.displayWidth * 0.5
                                            preferredHeight: {if(ListItemData.localImagePath === undefined || ListItemData.localImagePath === "") {if (ratio>1.25){return contWidth*1.25} else{return contHeight} }
                                            else {if (ratio>1.25) return contWidth*1.25
                                                    else return undefined}
                                            }
                                            //preferredHeight:contHeight                                       
                                            horizontalAlignment: HorizontalAlignment.Center
                                            bottomMargin: ui.du(1.0)
                                            layout: DockLayout {
                                            }
                                            gestureHandlers: [
                                                TapHandler {
                                                    onTapped: {
                                                        if (! ListItemData.localImagePath && ListItemData.mxcUrl) {
                                                            rootMessageItem.listView.requestAttachmentDownload(ListItemData.mxcUrl, ListItemData.id, ListItemData.fileName, ListItemData.type, ListItemData.extension, ListItemData.fileSize);
                                                        } else if (ListItemData.localImagePath && ListItemData.localImagePath !== "") {
                                                                rootMessageItem.listView.openPhoto(ListItemData.localImagePath);
                                                        }
                                                    }
                                                }
                                            ]
                                            
                                            Container {
                                                visible:(ListItemData.localImagePath === undefined || ListItemData.localImagePath === "")
                                                horizontalAlignment: HorizontalAlignment.Fill
                                                verticalAlignment: VerticalAlignment.Fill
                                                layout: DockLayout {
                                                }
                                                ImageView {
                                                    imageSource: "asset:///images/ic_gallery.png"
                                                    minWidth: ui.du(10.0)
                                                    minHeight: ui.du(10.0)
                                                    preferredWidth: ui.du(10.0)
                                                    preferredHeight: ui.du(10.0)                                   
                                                    filterColor: Color.DarkGray
                                                    horizontalAlignment: HorizontalAlignment.Right
                                                    verticalAlignment: VerticalAlignment.Top
                                                }
                                                
                                                Container {
                                                    visible: !ListItemData.isDownloading
                                                    horizontalAlignment: HorizontalAlignment.Center
                                                    verticalAlignment: VerticalAlignment.Center
                                                    layout: StackLayout {
                                                        orientation: LayoutOrientation.TopToBottom
                                                    }
                                                    ImageView {
                                                        imageSource: "asset:///images/download.png"
                                                        minWidth: ui.du(10.0)
                                                        minHeight: ui.du(10.0)
                                                        preferredWidth: ui.du(10.0)
                                                        preferredHeight: ui.du(10.0)
                                                        horizontalAlignment: HorizontalAlignment.Center
                                                    }
                                                    Label {
                                                        text:ListItemData.fileSizeStr
                                                        textStyle.fontSize: FontSize.Medium
                                                        horizontalAlignment: HorizontalAlignment.Center
                                                        textStyle.color: Color.DarkGray
                                                        topMargin: 0
                                                    }
                                                }                
                                            }
                                            
                                            ImageView {
                                                id:imView
                                                property bool noDownload:ListItemData.localImagePath === undefined || ListItemData.localImagePath === ""
                                                property bool tooLong:ListItemData.size!==undefined && ListItemData.size.height/ListItemData.size.width>1.25
                                                
                                                visible: !noDownload
                                                imageSource: ListItemData.localImagePath ? ListItemData.localImagePath:""
                                                horizontalAlignment: HorizontalAlignment.Fill
                                                verticalAlignment: VerticalAlignment.Fill
                                                scalingMethod: noDownload ? undefined : tooLong ? 1:0                                   
                                            }
                                            // Circular Progress Overlay
                                            Container {
                                                visible: !!ListItemData.isDownloading
                                                horizontalAlignment: HorizontalAlignment.Center
                                                verticalAlignment: VerticalAlignment.Center
                                                preferredWidth: ui.du(10.0); preferredHeight: ui.du(10.0)
                                                background: Color.DarkGray
                                                layout: DockLayout {}
                                                Label {
                                                    text: (ListItemData.downloadProgress || 0) + "%"
                                                    textStyle.color: Color.White
                                                    textStyle.fontSize: FontSize.Small
                                                    horizontalAlignment: HorizontalAlignment.Center
                                                    verticalAlignment: VerticalAlignment.Center
                                                }
                                            }
                                        }                               
                                        
                                        Container {
                                            id:audioVideoCont
                                            property string upperType: ListItemData.type ? ListItemData.type.toString().toUpperCase() : ""
                                            visible: (upperType === "AUDIO" || upperType ==="VOICE" || upperType ==="VIDEO")
                                            background: ListItemData.isSender ? Color.create(rootMessageItem.listView.rootPage.darkenColor(rootMessageItem.listView.rootPage.bubbleColor,15)): Color.create("#f6f5f3")
                                            preferredWidth:ListItemData.text===""?undefined:rootMessageItem.listView.displayWidth*0.7
                                            
                                            
                                            horizontalAlignment: HorizontalAlignment.Fill
                                            leftPadding: ui.du(1.0)
                                            rightPadding: ui.du(2.0)
                                            topPadding: ui.du(1.0)
                                            bottomPadding: ui.du(1.0)
                                            
                                            layout: DockLayout {
                                            }
                                            gestureHandlers: [
                                                TapHandler {
                                                    onTapped: {
                                                        if (! ListItemData.localImagePath && ListItemData.mxcUrl && ! ListItemData.isDownloading) {
                                                            rootMessageItem.listView.requestAttachmentDownload(ListItemData.mxcUrl, ListItemData.id, ListItemData.fileName, ListItemData.type, ListItemData.extension, ListItemData.fileSize);
                                                        } else if (ListItemData.localImagePath && ListItemData.localImagePath !== "") {
                                                                rootMessageItem.listView.playMedia(ListItemData.localImagePath);   
                                                        }
                                                    }
                                                }
                                            ]
                                            
                                            Container {
                                                //visible:(ListItemData.localImagePath === undefined || ListItemData.localImagePath === "")
                                                horizontalAlignment: HorizontalAlignment.Fill
                                                verticalAlignment: VerticalAlignment.Fill
                                                layout: StackLayout {
                                                    orientation: LayoutOrientation.LeftToRight
                                                }
                                                ImageView {
                                                    property string upperType: ListItemData.type ? ListItemData.type.toString().toUpperCase() : ""
                                                    imageSource: upperType === "AUDIO" || upperType ==="VOICE" ? "asset:///images/ic_doctype_music.png":"asset:///images/ic_doctype_video.png"
                                                    minWidth: ui.du(9.0)
                                                    minHeight: ui.du(9.0)
                                                    preferredWidth: ui.du(9.0)
                                                    preferredHeight: ui.du(9.0)                                   
                                                    horizontalAlignment: HorizontalAlignment.Left
                                                    verticalAlignment: VerticalAlignment.Center
                                                    filterColor: upperType === "AUDIO" || upperType ==="VOICE" ? Color.DarkBlue:Color.Red
                                                    rightMargin: ui.du(1.5)
                                                }
                                                
                                                Container {
                                                    layoutProperties: StackLayoutProperties {
                                                        spaceQuota: ListItemData.text===""?undefined:1.0
                                                    }
                                                    Container {
                                                        //visible: !ListItemData.isDownloading                                     
                                                        layout: StackLayout {
                                                            orientation: LayoutOrientation.TopToBottom
                                                        }
                                                        
                                                        Label {
                                                            text:ListItemData.type
                                                            textStyle.fontSize: FontSize.Small                                            
                                                            horizontalAlignment: HorizontalAlignment.Left
                                                            multiline: true
                                                            autoSize.maxLineCount: 2
                                                        }
                                                        Label {
                                                            text:ListItemData.extension ? ListItemData.fileSizeStr+" • "+ListItemData.extension.substring(1).toUpperCase():""
                                                            textStyle.fontSize: FontSize.XSmall
                                                            textStyle.color: Color.DarkGray                                        
                                                            topMargin: 0
                                                        }
                                                    }
                                                } 
                                                
                                                
                                                Container {
                                                    preferredWidth: ui.du(2.0)
                                                    minWidth:ui.du(2.0) 
                                                }                                   
                                                ImageView {
                                                    visible: !ListItemData.isDownloading
                                                    imageSource: ListItemData.localImagePath === undefined || ListItemData.localImagePath === "" && !ListItemData.isDownloading ? "asset:///images/download.png":"asset:///images/ic_play.png"
                                                    preferredWidth: ui.du(8.0)
                                                    minWidth:ui.du(8.0)
                                                    preferredHeight: ui.du(8.0)
                                                    verticalAlignment: VerticalAlignment.Center
                                                    filterColor: ListItemData.localImagePath === undefined || ListItemData.localImagePath === "" && !ListItemData.isDownloading ? undefined:Color.DarkGray                           
                                                }
                                                // Circular Progress Overlay
                                                Container {
                                                    visible: !!ListItemData.isDownloading
                                                    horizontalAlignment: HorizontalAlignment.Center
                                                    verticalAlignment: VerticalAlignment.Center
                                                    preferredWidth: ui.du(8.0); preferredHeight: ui.du(8.0)
                                                    minWidth:ui.du(10.0)
                                                    background: Color.DarkGray
                                                    layout: DockLayout {}
                                                    Label {
                                                        text: (ListItemData.downloadProgress || 0) + "%"
                                                        textStyle.color: Color.White
                                                        textStyle.fontSize: FontSize.XSmall
                                                        horizontalAlignment: HorizontalAlignment.Center
                                                        verticalAlignment: VerticalAlignment.Center
                                                    }
                                                }                
                                            }
                                        }
                                        
                                        // Unified File Container (Documents and Others)
                                        Container {
                                            id:fileCont
                                            property string upperType: ListItemData.type ? ListItemData.type.toString().toUpperCase() : ""
                                            visible: (upperType === "FILE")
                                            background: ListItemData.isSender ? Color.create(rootMessageItem.listView.rootPage.darkenColor(rootMessageItem.listView.rootPage.bubbleColor,15)): Color.create("#f6f5f3")
                                            preferredWidth:ListItemData.text===""?undefined:rootMessageItem.listView.displayWidth*0.7
                                            
                                            
                                            horizontalAlignment: HorizontalAlignment.Fill
                                            leftPadding: ui.du(1.0)
                                            rightPadding: ui.du(2.0)
                                            topPadding: ui.du(1.0)
                                            bottomPadding: ui.du(1.0)
                                            
                                            layout: DockLayout {
                                            }
                                            gestureHandlers: [
                                                TapHandler {
                                                    onTapped: {
                                                        if (! ListItemData.localImagePath && ListItemData.mxcUrl && ! ListItemData.isDownloading) {
                                                            rootMessageItem.listView.requestAttachmentDownload(ListItemData.mxcUrl, ListItemData.id, ListItemData.fileName, ListItemData.type, ListItemData.extension, ListItemData.fileSize);
                                                        } else if (ListItemData.localImagePath && ListItemData.localImagePath !== "") {
                                                            rootMessageItem.listView.openDocument(ListItemData.localImagePath);
                                                        }
                                                    }
                                                }
                                            ]
                                            
                                            Container {
                                                //visible:(ListItemData.localImagePath === undefined || ListItemData.localImagePath === "")
                                                horizontalAlignment: HorizontalAlignment.Fill
                                                verticalAlignment: VerticalAlignment.Fill
                                                layout: StackLayout {
                                                    orientation: LayoutOrientation.LeftToRight
                                                }
                                                ImageView {
                                                    imageSource: "asset:///images/ic_doctype_generic.png"
                                                    minWidth: ui.du(10.0)
                                                    minHeight: ui.du(10.0)
                                                    preferredWidth: ui.du(10.0)
                                                    preferredHeight: ui.du(10.0)                                   
                                                    horizontalAlignment: HorizontalAlignment.Left
                                                    verticalAlignment: VerticalAlignment.Center
                                                    filterColor: {
                                                        var ext = ListItemData.extension ? ListItemData.extension.toLowerCase() : "";
                                                        
                                                        if (ext === ".pdf") {
                                                            return Color.create("#D32F2F")
                                                        } else if (ext === ".xls" || ext === ".xlsx") {
                                                            return Color.create("#388E3C")
                                                        } else if (ext === ".doc" || ext === ".docx") {
                                                            return Color.create("#1976D2")
                                                        } else if (ext === ".ppt" || ext === ".pptx") {
                                                            return Color.create("#E64A19");
                                                        } else if (ext === ".zip" || ext === ".rar" || ext === ".7z") {
                                                            return Color.create("#FBC02D");
                                                        } else {
                                                            return Color.DarkGray; // Tanımsız türler için varsayılan
                                                        }
                                                    }
                                                }
                                                
                                                Container {
                                                    layoutProperties: StackLayoutProperties {
                                                        spaceQuota: ListItemData.text===""?undefined:1.0
                                                    }
                                                    Container {
                                                        //visible: !ListItemData.isDownloading                                     
                                                        layout: StackLayout {
                                                            orientation: LayoutOrientation.TopToBottom
                                                        }
                                                        
                                                        Label {
                                                            text:ListItemData.fileName
                                                            textStyle.fontSize: FontSize.Small                                            
                                                            horizontalAlignment: HorizontalAlignment.Left
                                                            multiline: true
                                                            autoSize.maxLineCount: 2
                                                        }
                                                        Label {
                                                            text:ListItemData.extension ? ListItemData.fileSizeStr+" • "+ListItemData.extension.substring(1).toUpperCase():""
                                                            textStyle.fontSize: FontSize.XSmall
                                                            textStyle.color: Color.DarkGray                                        
                                                            topMargin: 0
                                                        }
                                                    }
                                                } 
                                                
                                                
                                                Container {
                                                    preferredWidth: ui.du(2.0)
                                                    minWidth:ui.du(2.0)
                                                }
                                                                                   
                                                ImageView {
                                                    visible: (ListItemData.localImagePath === undefined || ListItemData.localImagePath === "" && !ListItemData.isDownloading)
                                                    imageSource: "asset:///images/download.png"
                                                    preferredWidth: ui.du(8.0)
                                                    minWidth:ui.du(8.0)
                                                    preferredHeight: ui.du(8.0)
                                                    verticalAlignment: VerticalAlignment.Center                            
                                                }
                                                // Circular Progress Overlay
                                                Container {
                                                    visible: !!ListItemData.isDownloading
                                                    horizontalAlignment: HorizontalAlignment.Center
                                                    verticalAlignment: VerticalAlignment.Center
                                                    preferredWidth: ui.du(8.0); preferredHeight: ui.du(8.0)
                                                    minWidth:ui.du(10.0)
                                                    background: Color.DarkGray
                                                    layout: DockLayout {}
                                                    Label {
                                                        text: (ListItemData.downloadProgress || 0) + "%"
                                                        textStyle.color: Color.White
                                                        textStyle.fontSize: FontSize.XSmall
                                                        horizontalAlignment: HorizontalAlignment.Center
                                                        verticalAlignment: VerticalAlignment.Center
                                                    }
                                                }                
                                            }
                                        }
                                        
                                        Container {
                                            visible:(ListItemData.text !== undefined && ListItemData.text.length > 0)
                                            preferredWidth:{if(ListItemData.type!=="TEXT" && ListItemData.text!==""){
                                                    return rootMessageItem.listView.displayWidth * 0.7}else{return undefined}}  
                                            Label {
                                                text: ListItemData.text
                                                multiline: true
                                                textStyle.fontSize: FontSize.Medium
                                                textFormat: TextFormat.Html
                                                topMargin: 0;
                                                bottomMargin: 0;
                                                textStyle.color:Color.Black
                                            
                                            }
                                        }
                                        // Using a Container to force the time to the bottom right
                                        Container {
                                            horizontalAlignment: HorizontalAlignment.Fill
                                            topMargin: ui.du(0.5)
                                            
                                            layout: DockLayout {}
                                            
                                            /*// 1. ÖZEL PROPERTY TANIMI (Cascades bunun için otomatik geçerli bir sinyal oluşturur)
                                             property string currentReactions: (ListItemData && ListItemData.reactionsText) ? ListItemData.reactionsText : ""
                                             
                                             // 2. GEÇERLİ SİNYAL: Property her değiştiğinde tetiklenir
                                             onCurrentReactionsChanged: {
                                             if (currentReactions !== "") {
                                             console.log("=== [REACTION QML DEBUG] ===");
                                             console.log("  -> Msg ID:", ListItemData.id);
                                             console.log("  -> Emojis:", currentReactions);
                                             }
                                             }*/

// 1. GENİŞLİK HESAPLAYICI (Görünmez Yapı)
Container {
                                                opacity: 0.0
                                                preferredHeight: 0.0
                                                
                                                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                                                
                                                Label { 
                                                    text: ListItemData.reactionsText ? ListItemData.reactionsText : ""
                                                    textStyle.fontSize: FontSize.XSmall 
                                                }
                                                Container { 
                                                    preferredWidth: ui.du(1.5)
                                                }
                                                Label { 
                                                    text: ListItemData.timestamp ? ListItemData.timestamp : ""
                                                    textStyle.fontSize: FontSize.XSmall 
                                                }
}

// 2. GÖRÜNÜR EMOJİLER (Sol Köşe)
Label {
    text: ListItemData.reactionsText ? ListItemData.reactionsText : ""
    textStyle.fontSize: FontSize.XSmall
    textStyle.color: Color.Gray
    horizontalAlignment: HorizontalAlignment.Left
    
    visible: ListItemData.reactionsText && ListItemData.reactionsText !== ""
}

// 3. GÖRÜNÜR SAAT (Sağ Köşe)
Label {
    text: {if (!ListItemData.timestamp) return "";        
            if (!ListItemData.isSender) return ListItemData.timestamp;            
            if (ListItemData.isPending) return (ListItemData.timestamp+" ⦿");        
            if (ListItemData.isSeen) return ListItemData.timestamp+" <span style='color: #34B7F1;'>✔</span>";        
            return ListItemData.timestamp+" ✔";}
    textFormat: TextFormat.Html                                 
    textStyle.fontSize: FontSize.XSmall
    textStyle.color: Color.Gray
    horizontalAlignment: HorizontalAlignment.Right
}

                                        }
                                    }
                                    
                                    // Spacer pushes their messages to the left
                                    Container {
                                        id:marginCont
                                        visible: !ListItemData.isSender
                                        layoutProperties: StackLayoutProperties {
                                            spaceQuota: 1.0
                                        }
                                        minWidth: ui.du(10.0)
                                    }
                                }
                            }
                        }
                        
                    
                    }
                
                ]
                                
                function itemType(data, indexPath) {
                    if (data.isHeader) {
                        return "header"; // Başlık objesi geldiğini belirtir
                    }
                    return "message";    // Normal mesaj
                }
            }
            
            Container{
                id:editCont
                visible: false
                horizontalAlignment: HorizontalAlignment.Fill
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }
                leftPadding: ui.du(2.0); rightPadding: ui.du(2.0); topPadding: ui.du(1.0); bottomPadding: ui.du(2.0)

                Container {
                    layoutProperties: StackLayoutProperties {
                        spaceQuota: 1.0
                    }
                    minWidth: ui.du(10.0)
                    Button {
                        preferredWidth: ui.du(8.0)
                        text: "✖"
                        color: Color.create("#ff282828")
                        onClicked: {
                            isEditing=false
                            editCont.visible=false
                            attachButton.visible=true;
                            inputField.text = "";
                            inputField.requestFocus();
                            listView.opacity=1.0;
                            sendButtonUrl="asset:///images/ic_microphone.png";
                        }   
                    }
                }
                
                Container {
                    background:Color.create(bubbleColor)
                    leftPadding: ui.du(1.5); rightPadding: ui.du(1.5); topPadding: ui.du(1.0); bottomPadding: ui.du(1.0)
                    layout: StackLayout {
                    }   
                     Label {
                         id: editLabel
                         text: "Edit"
                         multiline: true
                         textStyle.fontSize: FontSize.Medium
                         textFormat: TextFormat.Html
                         topMargin: 0;
                         bottomMargin: 0;
                         textStyle.color:Color.Black
                     
                     }
                }
         }
            
            
            
            // Custom Bottom Bar (Back Button + Emoji + Input + Send/Voice)
            Container {
                id: bottomBar
                visible: (!chatPage.isSearchMode && !readOnly)
                background: Color.create("#ff282828")
                
                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                leftPadding: ui.du(1.0); rightPadding: ui.du(1.5); topPadding: ui.du(1.0); bottomPadding: ui.du(1.0)
                
                // 1. Back Button
                /*ImageButton {
                 defaultImageSource: "asset:///images/ic_previous.png"
                 verticalAlignment: VerticalAlignment.Center
                 preferredWidth: ui.du(8.0)
                 preferredHeight: ui.du(8.0)
                 onClicked: {
                 navigationPane.pop(); 
                 }
                 }*/
                
                // 1. Emoji Button
                ImageButton {
                    // Çekmece açıksa klavye ikonuna, kapalıysa emoji ikonuna dönüştür
                    defaultImageSource: chatPage.isEmojiPickerVisible ? "asset:///images/ic_keyboard.png" : "asset:///images/ic_emoji.png"
                    verticalAlignment: VerticalAlignment.Center
                    preferredWidth: ui.du(8.0)
                    preferredHeight: ui.du(8.0)
                    
                    onClicked: {
                        chatPage.isEmojiPickerVisible = !chatPage.isEmojiPickerVisible;
                        if (chatPage.isEmojiPickerVisible) {
                            // Çekmece açılırken native klavyeyi gizle
                            inputField.loseFocus();
                        } else {
                            // Çekmece kapanırken klavyeyi geri çağır
                            inputField.requestFocus();
                        }
                    }
                }
                
                Container {
                    layout: StackLayout { orientation: LayoutOrientation.TopToBottom}
                    layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
                    Container {
                        preferredHeight:ui.du(1.0)
                        minHeight:ui.du(1.0)
                        horizontalAlignment: HorizontalAlignment.Fill                        
                        background: Color.create("#f5f5f5")
                        bottomMargin: 0
                        visible: isReplying
                    }
                    
                    Container {
                        id: replyCont
                        visible: isReplying                                  
                        background: Color.create("#f5f5f5")
                        bottomMargin: ui.du(0)
                        
                        //topPadding: ui.du(0.5)
                        layout: StackLayout { orientation: LayoutOrientation.LeftToRight}
                        Container {
                            layout: StackLayout { orientation: LayoutOrientation.LeftToRight}
                            rightPadding: ui.du(1.0)
                            background: Color.create("#E0E0E0")
                            Container {                            
                                preferredWidth:ui.du(1.0)
                                minWidth:ui.du(1.0)
                                verticalAlignment: VerticalAlignment.Fill
                                background: Color.create("#f5f5f5")
                            }
                            Container {
                                preferredWidth:ui.du(1.0)
                                minWidth:ui.du(1.0)
                                verticalAlignment: VerticalAlignment.Fill
                                background: replySender==="You" ? Color.create(darkenColor(primaryColor,50)):Color.Gray
                            }
                            Container {
                                layoutProperties: StackLayoutProperties {
                                    spaceQuota: 1
                                }
                                layout: StackLayout { orientation: LayoutOrientation.TopToBottom }

                                // İç boşluklar (Padding)
                                leftPadding: ui.du(1.0)
                                rightPadding: ui.du(1.0)
                                topPadding: ui.du(0.5)
                                bottomPadding: ui.du(1.0)
                                
                                Label {
                                    text:replySender
                                    textStyle.fontSize: FontSize.XSmall
                                    textStyle.fontWeight: FontWeight.Bold
                                    bottomMargin: 0
                                }
                                
                                Label {
                                    text:replyMessage
                                    //text:"doo"
                                    textStyle.fontSize: FontSize.XSmall
                                    horizontalAlignment: HorizontalAlignment.Fill
                                    topMargin: 0
                                    multiline: true
                                    autoSize.maxLineCount: 2
                                    visible: replyMessage != ""
                                    textFormat: TextFormat.Html
                                }
                            }
                            
                            ImageView {
                                visible: replyAttachVisible
                                //preferredWidth: replyAttachWidth
                                //preferredHeight: replyAttachHeight
                                preferredWidth: {if(replyAttachImgUrl.indexOf("asset") !== -1) {return ui.du(8.0)}
                                else{return replyAttachWidth}}
                                preferredHeight: {if(replyAttachImgUrl.indexOf("asset") !== -1) {return ui.du(8.0)}
                                else{return replyAttachHeight}}
                                minWidth: {if(replyAttachImgUrl.indexOf("asset") !== -1) {return ui.du(8.0)}
                                else{return replyAttachWidth}}
                                imageSource: replyAttachImgUrl
                                //imageSource: "asset:///images/ic_view_image.png"
                                horizontalAlignment: HorizontalAlignment.Left
                                verticalAlignment: VerticalAlignment.Fill
                                scalingMethod: ScalingMethod.AspectFit
                                rightMargin: 0
                            
                            
                            }
                            Label {
                                text: "✖ "
                                horizontalAlignment: HorizontalAlignment.Right
                                onTouch: {
                                    isReplying = false;
                                    replyToMessageID = "";
                                    replyAttachVisible = false;
                                
                                }
                                textStyle.textAlign: TextAlign.Right
                                textStyle.color: Color.DarkGray
                                leftMargin: 0
                                rightMargin: 0
                            
                            }
                        }
                        
                        Container {
                            preferredWidth: ui.du(1.0)
                            minWidth: ui.du(1.0)
                            verticalAlignment: VerticalAlignment.Fill
                            background: Color.create("#f5f5f5")
                        }
                    }
                    
                    Container {
                        id:attachCont
                        //property string upperType: ListItemData.type ? ListItemData.type.toString().toUpperCase() : ""
                        visible: attachSending
                        
                        //preferredWidth:ListItemData.text===""?undefined:rootMessageItem.listView.displayWidth*0.7
                        
                        
                        horizontalAlignment: HorizontalAlignment.Center                        
                        
                        layout: StackLayout {
                            orientation: LayoutOrientation.LeftToRight
                        }
                        
                        Container {                            
                            preferredWidth:ui.du(1.0)
                            minWidth:ui.du(1.0)
                            verticalAlignment: VerticalAlignment.Fill
                            background: Color.create("#f5f5f5")
                        }
                        
                        Container {
                            layout: StackLayout {
                                orientation: LayoutOrientation.TopToBottom
                            }
                            
                            Container {                            
                                preferredHeight:ui.du(1.0)
                                minHeight:ui.du(1.0)
                                horizontalAlignment: HorizontalAlignment.Fill
                                background: Color.create("#f5f5f5")
                            }
                            
                            Container {
                                //visible:(ListItemData.localImagePath === undefined || ListItemData.localImagePath === "")
                                horizontalAlignment: HorizontalAlignment.Fill
                                verticalAlignment: VerticalAlignment.Fill
                                background: Color.create("#E0E0E0")
                                leftPadding: ui.du(1.0)
                                rightPadding: ui.du(1.0)
                                topPadding: ui.du(0.5)
                                bottomPadding: ui.du(1.0)                           
                                layout: StackLayout {
                                    orientation: LayoutOrientation.LeftToRight
                                }
                                ImageView {
                                    imageSource: {
                                    if(attachFileType=="IMAGE") return attachUrl
                                    else if(attachFileType=="VIDEO") return "asset:///images/ic_doctype_video.png"
                                    else if(attachFileType=="AUDIO") return "asset:///images/ic_doctype_music.png"
                                        else "asset:///images/ic_doctype_generic.png"
                                    }
                                    minWidth: ui.du(10.0)
                                    //minHeight: ui.du(10.0)
                                    preferredWidth: ui.du(10.0)
                                    //preferredHeight: ui.du(10.0)                                   
                                    horizontalAlignment: HorizontalAlignment.Left
                                    verticalAlignment: VerticalAlignment.Center
                                    scalingMethod: ScalingMethod.AspectFit
                                    filterColor: {
                                        var ext = attachExtension;
                                        if (ext === "JPG" || ext === "JPEG" || ext === "PNG"){
                                            return undefined
                                        }
                                        else if (ext === "PDF") {
                                            return Color.create("#D32F2F")
                                        } else if (ext === "XLS" || ext === "XLSX") {
                                            return Color.create("#388E3C")
                                        } else if (ext === "DOC" || ext === "DOCX") {
                                            return Color.create("#1976D2")
                                        } else if (ext === "PPT" || ext === "PPTX") {
                                            return Color.create("#E64A19");
                                        } else if (ext === "ZIP" || ext === "RAR" || ext === "7Z") {
                                            return Color.create("#FBC02D");
                                        } else {
                                            return Color.DarkGray; // Tanımsız türler için varsayılan
                                        }
                                    }
                                }
                                
                                Container {
                                    layoutProperties: StackLayoutProperties {
                                        spaceQuota:1.0
                                    }
                                    leftPadding: ui.du(1.0)
                                    Container {
                                        //visible: !ListItemData.isDownloading                                     
                                        layout: StackLayout {
                                            orientation: LayoutOrientation.TopToBottom
                                        }
                                        
                                        Label {
                                            text:attachFileName
                                            textStyle.fontSize: FontSize.Small                                            
                                            horizontalAlignment: HorizontalAlignment.Left
                                            multiline: true
                                            autoSize.maxLineCount: 2
                                        }
                                        Label {
                                            text:attachFileSizeStr+" • "+attachExtension
                                            textStyle.fontSize: FontSize.XSmall
                                            textStyle.color: Color.DarkGray                                        
                                            topMargin: 0
                                        }
                                    }
                                }
                                
                                Label {
                                    text: "✖ "
                                    horizontalAlignment: HorizontalAlignment.Right
                                    onTouch: {
                                        attachSending = false;
                                        attachUrl = "";
                                        var inText= inputField.text;
                                        inputField.text =""
                                        sendButtonUrl = "asset:///images/ic_microphone.png";
                                        inputField.text = inText;
                                    
                                    }
                                    textStyle.textAlign: TextAlign.Right
                                    textStyle.color: Color.DarkGray
                                    leftMargin: 0
                                    rightMargin: 0
                                
                                }
                            
                            }
                            
                            Container {                            
                                preferredHeight:ui.du(1.0)
                                minHeight:ui.du(1.0)
                                horizontalAlignment: HorizontalAlignment.Fill
                                background: Color.create("#f5f5f5")
                            }
                            
                        }
                        
                        Container {                            
                            preferredWidth:ui.du(1.0)
                            minWidth:ui.du(1.0)
                            verticalAlignment: VerticalAlignment.Fill
                            background: Color.create("#f5f5f5")
                        }
                        
                        
                    }                    
                    
                    // 2. Text Input
                    TextArea {
                        id: inputField
                        hintText: "Enter text"
                        verticalAlignment: VerticalAlignment.Center
                        
                        // Plain Text modunda kalması satır atlamalarının yapıştırırken korunmasını sağlar
                        textFormat: TextFormat.Plain
                        
                        onTextChanging: {
                            if (inputField.text.trim().length > 0) {
                                sendButtonUrl = "asset:///images/ic_play.png";
                            } else {
                                sendButtonUrl = "asset:///images/ic_microphone.png";
                            }
                        }
                        
                        onFocusedChanged: {
                            if (focused) {
                                chatPage.isEmojiPickerVisible = false;
                                if (messageModel.size() > 0) {
                                    listView.scrollToItem([ messageModel.size() - 1 ], ScrollAnimation.Default);
                                }
                            }
                            
                        }
                        
                        autoSize.maxLineCount: 8
                        input.submitKey: SubmitKey.None
                        topMargin: ui.du(0)
                        topPadding: isReplying ? ui.du(3) : undefined
                   
                    }
                }
                
                // 3. Attach Button
                ImageButton {
                    id:attachButton
                    visible:true
                    defaultImageSource: "asset:///images/ic_attach.png"
                    verticalAlignment: VerticalAlignment.Center
                    preferredWidth: ui.du(7.0)
                    preferredHeight: ui.du(7.0)
                    onClicked: {
                        // navigationPane nesnesinin ana NavigationPane id'niz olduğundan emin olun
                        var attachmentPage = attachmentPageDefinition.createObject();
                        attachmentPage.chatPageRef = chatPage;
                        attachmentPage.accountID = chatPage.accountID;
                        attachmentPage.chatID = chatPage.chatID;
                        navigationPane.push(attachmentPage);
                    }
                }
                
                // 4. Send/Voice Button
                ImageButton {
                    id: sendButton
                    //defaultImageSource: sendButtonUrl
                    defaultImageSource: "asset:///images/ic_send.png";
                    verticalAlignment: VerticalAlignment.Center
                    preferredWidth: ui.du(8.0)
                    preferredHeight: ui.du(8.0)
                    leftPadding: 0
                    leftMargin: 0
                    
                    onClicked: {
                        var displayMsg = inputField.text.trim();
                        if (!isEditing && (attachSending || displayMsg.length > 0)) {
                            var outgoingMsg = "";
                            if (displayMsg.length > 0) {
                                outgoingMsg = formatToOutgoingMarkdown(displayMsg);
                                displayMsg = formatToHtml(displayMsg);
                                displayMsg = "<font face=\"Slate Pro\">" + displayMsg + "</font>";
                            }
                            
                            var type;
                            if (!attachSending) type = "TEXT";
                            else type = attachFileType;
                            
                            inputField.text = "";
                            inputField.requestFocus();
                            var pendingMsgID = "pending_" + new Date().getTime();
                            var timeS = Qt.formatDateTime(new Date(), "hh:mm");
                            
                            // === OPTIMISTIC UI ===
                            var pendingMsg = {
                                "id": pendingMsgID,
                                "chatID": chatID,
                                "accountID": accountID,
                                "text": displayMsg,
                                "isSender": true,
                                "isSeen": false,
                                "isPending": true,
                                "type": type,
                                "timestamp": timeS,
                                
                                "hasMention": chatPage.isReplying ? true : false,
                                "mentionSenderName": chatPage.isReplying ? (chatPage.replySender ? chatPage.replySender : "Unknown") : "",
                                "mentionText": chatPage.isReplying ? (chatPage.replyMessage ? chatPage.replyMessage : "") : "",
                                "isMentionImg": chatPage.replyAttachVisible,
                                "mentionImgRatio": chatPage.replyAttachRatio,
                                "mentionImgLocalUrl": chatPage.replyAttachImgUrl,
                                
                                "extension": "." + attachExtension.toLowerCase(),
                                "isDownloading": true,
                                "fileName": attachFileName,
                                "fileSizeStr": attachFileSizeStr,
                                "localImagePath": attachUrl,
                                "size": type == "IMAGE" ? attachImageSize : {}
                            };
                          
                            if (type == "TEXT") {
                                dat.sendMessage(chatPage.accountID, chatPage.chatID, pendingMsgID, outgoingMsg, {}, chatPage.replyToMessageID);
                            } else {
                                // 1. Ekli dosya ise main.qml üzerindeki activeUploads'a kaydet
                                mainRef.addActiveUpload(pendingMsgID, pendingMsg);                        
                                dat.uploadAssetAndSend(attachUrl, accountID, chatID, outgoingMsg, pendingMsgID, chatPage.replyToMessageID);
                            }
                            
                            // 2. Anlık açık olan chat ekranına ekle
                            messageModel.append(pendingMsg);
                            listView.scrollToItem([ messageModel.size() - 1 ], ScrollAnimation.Default);
                            
                            chatPage.replyToMessageID = "";
                            chatPage.isReplying = false;
                            replyAttachVisible = false;
                            attachSending = false;
                            sendButtonUrl = "asset:///images/ic_microphone.png";
                        }else if (isEditing && displayMsg.length > 0){
                            var outgoingMsg = formatToOutgoingMarkdown(displayMsg);
                            isEditing=false;
                            editCont.visible=false;
                            attachButton.visible=true;
                            inputField.text = "";
                            inputField.requestFocus();
                            listView.opacity=1.0;
                            sendButtonUrl="asset:///images/ic_microphone.png";
                            dat.editMessage(chatID, editMsgID, outgoingMsg);
                        }
                    }
                }
            }
            
            // Alt çubuğu (Bottom Bar) kapatan '}' ayracının hemen altına ekleyin:
            
            Container {
                id: emojiPickerContainer
                visible: chatPage.isEmojiPickerVisible
                horizontalAlignment: HorizontalAlignment.Fill
                // Native klavyenin yaklaşık kapladığı alan boyutunda sabit bir yükseklik
                preferredHeight: ui.du(38.0)
                background: Color.create("#E5DDD5")
                
                layout: DockLayout {
                }
                
                ListView {
                    id: emojiListView
                    horizontalAlignment: HorizontalAlignment.Fill
                    verticalAlignment: VerticalAlignment.Fill
                    
                    layout: GridListLayout {
                        columnCount: 7
                        cellAspectRatio: 1.0
                    }
                    
                    dataModel: ArrayDataModel {
                        id: emojiPickerModel
                    }
                    
                    listItemComponents: [
                        ListItemComponent {
                            type: "" 
                            
                            Container {
                                layout: DockLayout {}
                                horizontalAlignment: HorizontalAlignment.Fill
                                verticalAlignment: VerticalAlignment.Fill
                                
                                // YENİ EKLENEN KISIM: Seçili olma veya basılma durumuna göre arka plan rengi
                                background: ListItem.selected || ListItem.active ? Color.create("#C0C0C0") : Color.Transparent
                                
                                Label {
                                    text: ListItemData
                                    textStyle.fontSize: FontSize.XLarge
                                    horizontalAlignment: HorizontalAlignment.Center
                                    verticalAlignment: VerticalAlignment.Center
                                }
                            }
                        }
                    ]
                    
                    onTriggered: {
                        var selectedEmoji = dataModel.data(indexPath);
                        // Seçilen emojiyi inputField içerisindeki mevcut metnin sonuna ekle
                        inputField.text = inputField.text + selectedEmoji;
                    }
                }
            }
        }
                
    }
    
    attachedObjects: [
        TitleBar {
            id: titleBar
            title: chatPage.chatTitle
        },
        DisplayInfo {
            id: displayInfo
        },
        ArrayDataModel {
            id: messageModel
        },
        ComponentDefinition {
            id: attachmentPageDefinition
            source: "attach.qml"
        }
    
    ]
    
    onAccountIDChanged: {
        if (accountID && chatID) {
            loadMessages();
        }
    }
    
    onChatIDChanged: {
        if (accountID && chatID) {
            loadMessages();
        }
    }
    
    function scrollToTargetMessage() {
        if (! chatPage.targetMessageID || chatPage.targetMessageID === "" || messageModel.size() === 0) return;
        
        var foundIndex = -1;
        for (var i = 0; i < messageModel.size(); i ++) {
            var item = messageModel.value(i);
            if (item && item.id === chatPage.targetMessageID) {
                foundIndex = i;
                break;
            }
        }
        
        if (foundIndex !== -1) {
            // Eğer hedef mesaj listenin en sonundaki mesaj ise doğrudan listenin tabanına kaydır
            if (foundIndex === messageModel.size() - 1) {
                listView.scrollToPosition(ScrollPosition.End, ScrollAnimation.Default);
            } else {
                listView.scrollToItem([ foundIndex ], ScrollAnimation.Default);
            }
        }
    }
    
    function executeScroll() {
        if (! chatPage.targetMessageID || messageModel.size() === 0) return;
        
        var foundIndex = -1;
        for (var i = 0; i < messageModel.size(); i ++) {
            var item = messageModel.value(i);
            if (item && item.id === chatPage.targetMessageID) {
                foundIndex = i;
                break;
            }
        }
        
        if (foundIndex !== -1) {
            if (foundIndex === messageModel.size() - 1) {
                listView.scrollToPosition(ScrollPosition.End, ScrollAnimation.Default);
            } else {
                listView.scrollToItem([ foundIndex ], ScrollAnimation.Default);
            }
        }
    }
    
    function loadMessages() {
        messageOffset = 0;
        hasMoreMessages = true;
        console.log("[CHAT-UI] Loading messages for account: " + chatPage.accountID + " chat: " + chatPage.chatID);
        if (! accountID || ! chatID) {
            console.log("[CHAT-UI] Error: Missing IDs. Account: " + accountID + " Chat: " + chatID);
            return;
        }
        
        // Mark as read when loading the chat view or refreshing
        if (unreadCount > 0) dat.markChatAsRead(accountID, chatID);
        
        // chatPage.targetMessageID veya local targetMessageID kontrolü
        var targetId = (typeof targetMessageID !== "undefined" && targetMessageID !== null) ? targetMessageID : "";
        
        // Debug ile QML'deki değerini kontrol edin
        console.log("[QML DEBUG] Calling getMessagesForChat with targetId:", targetId);
        
        var messages = dat.getMessagesForChat(accountID, chatID, targetMessageID, pageLimit, messageOffset);
        
        var previousCount = messageModel.size();
        
        if (messages) {
            messageModel.clear();
            console.log("[CHAT-UI] Found " + messages.length + " messages");
            var newCount = messages.length;
            var currentDateHeader = "";
            
            // 1. Veritabanından gelen mesajları kronolojik sırayla modele ekle
            for (var i = newCount - 1; i >= 0; i --) {
                var msg = messages[i];
                
                // Tarih başlığı değiştiyse araya ayraç ekle
                if (msg.dateHeader && msg.dateHeader !== currentDateHeader) {
                    messageModel.append({
                            "isHeader": true, // QML bunun bir mesaj değil, ayraç olduğunu anlayacak
                            "headerText": msg.dateHeader
                    });
                currentDateHeader = msg.dateHeader;
                }
                
                messageModel.append(msg);
            }
            
            // === OPTIMISTIC UI: Devam Eden Aktif Yüklemeleri Sona Ekle ===
            var pendingList = [];
            
            if (mainRef && typeof mainRef.getActiveUploadsForChat === "function") {
                // Yanıt null gelirse varsayılan olarak [] (boş dizi) atanır
                pendingList = mainRef.getActiveUploadsForChat(chatPage.accountID, chatPage.chatID) || [];
            }
            
            // Artık pendingList'in null olma ihtimali olmadığı için güvenle uzunluk kontrolü yapılabilir
            if (pendingList.length > 0) {
                for (var p = 0; p < pendingList.length; p++) {
                    var pendingMsg = pendingList[p];
                    
                    var exists = false;
                    for (var j = 0; j < messageModel.size(); j++) {
                        var currentItem = messageModel.data([j]);
                        if (currentItem && currentItem.id === pendingMsg.id) {
                            exists = true;
                            break;
                        }
                    }
                    
                    if (!exists) {
                        messageModel.append(pendingMsg);
                    }
                }
            }
            
            if (messages.length >= pageLimit) {
                ensureLoadMoreButton();
            }
            
            // 2. Mesajlar ve aktif yüklemeler eklendikten sonra en alta kaydır
            if (messageModel.size() > 0) {
                // Since the model clears and rebuilds, the ListView loses scroll position.
                // Always jump to the bottom so the user isn't thrown to the top of the chat history.
                listView.scrollToItem([ messageModel.size() - 1 ], ScrollAnimation.None);
            }
        }
        
        if (chatPage.isSearchMode) {
            // Model dolduktan sonra C++ timer'ı tetikle
            dat.requestDelayedScroll();
        }
    }
}