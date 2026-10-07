import bb.cascades 1.4
import bb.system 1.2

Tab {
    id: genericTab
    
    // accountID değiştiğinde (veya ilk atandığında) otomatik yükle
    property string accountID: ""
    onAccountIDChanged: {
        if (accountID !== "") {
            loadThemeSettings();
        }
    }
    
    property alias tabTitle: genericTab.title
    property alias titleBarTitle: titleBar.title
    property TabbedPane mainRef
    property variant database: dat
    
    // --- TEMA DEĞİŞKENLERİ ---
    property string primaryColor: "#444444"
    property string chatBgColor: "#FFFA00"
    property string bubbleColor: "#E0E0E0"
    property string unreadBadgeColor: "#FF0000"
    
    // --- SAYFALAMA DEĞİŞKENLERİ ---
    property int chatOffset: 0
    property int pageLimit: 25
    property bool hasMoreChats: true
    
    property string imgSource
    
    // Uygulama ikonu (Gerekirse bu da dinamik yapılabilir)
    imageSource: imgSource
    
    attachedObjects: [
        ArrayDataModel {
            id: internalModel
        }
    ]
    
    function getModel() {
        return internalModel;
    }
    
    // Platform bazlı varsayılan renkleri döndüren fonksiyon
    function getDefaultColors(accID) {
        var net = accID.toLowerCase();
        if (net.indexOf("whatsapp") !== -1) return { primary: "#21BD5C", bg: "#E5DDD5", bubble: "#D8FDD2", badge: "#21BD5C" };
        if (net.indexOf("telegram") !== -1) return { primary: "#24A1DE", bg: "#E4ECEF", bubble: "#E1FFC7", badge: "#24A1DE" };
        if (net.indexOf("instagram") !== -1) return { primary: "#E1306C", bg: "#CCCCCC", bubble: "#f9d6e2", badge: "#E1306C" };
        if (net.indexOf("signal") !== -1) return { primary: "#3A76F0", bg: "#CCCCCC", bubble: "#d8e4fc", badge: "#3A76F0" };
        if (net.indexOf("facebook") !== -1) return { primary: "#0084FF", bg: "#CCCCCC", bubble: "#cce6ff", badge: "#ADD8E6" };
        if (net.indexOf("twitter") !== -1) return { primary: "#000000", bg: "#CCCCCC", bubble: "#d9d9d9", badge: "#000000" };
        if (net.indexOf("gmessages") !== -1 || net.indexOf("googlemessages") !== -1  || net.indexOf("rcs") !== -1 || net.indexOf("sms") !== -1) return { primary: "#1A73E8", bg: "#CCCCCC", bubble: "#d1e3fa", badge: "#1A73E8" };
        if (net.indexOf("googlechat") !== -1 || net.indexOf("gchat") !== -1) return { primary: "#00796B", bg: "#CCCCCC", bubble: "#cce4e1", badge: "#00796B" };
        if (net.indexOf("googlevoice") !== -1 || net.indexOf("gvoice") !== -1) return { primary: "#009688", bg: "#CCCCCC", bubble: "#cceae7", badge: "#009688" };
        if (net.indexOf("linkedin") !== -1) return { primary: "#0A66C2", bg: "#E9E5DF", bubble: "#cee0f3", badge: "#0A66C2" };
        if (net.indexOf("discord") !== -1) return { primary: "#5865F2", bg: "#36393F", bubble: "#dee0fc", badge: "#5865F2" };
        if (net.indexOf("slack") !== -1) return { primary: "#4A154B", bg: "#CCCCCC", bubble: "#dbd0db", badge: "#4A154B" };
        if (net.indexOf("irc") !== -1) return { primary: "#808080", bg: "#000000", bubble: "#d9d9d9", badge: "#808080" };
        if (net.indexOf("matrix") !== -1) return { primary: "#0DBD8B", bg: "#CCCCCC", bubble: "#cff2e8", badge: "#0DBD8B" };
        if (net.indexOf("imessage") !== -1) return { primary: "#34C759", bg: "#CCCCCC", bubble: "#d6f4de", badge: "#34C759" };
        if (net.indexOf("line") !== -1) return { primary: "#06C755", bg: "#8BACD9", bubble: "#cdf4dd", badge: "#06C755" };
        if (net.indexOf("tumblr") !== -1) return { primary: "#36465D", bg: "#001935", bubble: "#d7dadf", badge: "#36465D" };
        
        return { primary: "#444444", bg: "#FF0000", bubble: "#E0E0E0", badge: "#FF0000" }; // Default
    }
    
    // QSettings üzerinden temayı yükleme (main.qml içinden çağrılacak)
    function loadThemeSettings() {
        var def = getDefaultColors(accountID);
        
        // Veritabanı ve metodların varlığını güvenli şekilde kontrol et
        if (app && typeof app.getSetting === "function") {
            var p = app.getSetting("theme_primary_" + accountID, "");
            var bg = app.getSetting("theme_bg_" + accountID, "");
            var bubble = app.getSetting("theme_bubble_" + accountID, "");
            var badge = app.getSetting("theme_badge_" + accountID, "");
            
            primaryColor = (p && p.length > 3) ? p : def.primary;
            chatBgColor = (bg && bg.length > 3) ? bg : def.bg;
            bubbleColor = (bubble && bubble.length > 3) ? bubble : def.bubble;
            unreadBadgeColor = (badge && badge.length > 3) ? badge : def.badge;
            
            // Eğer veritabanında kayıtlı değilse varsayılanı kaydet
            app.updateSetting("theme_primary_" + accountID, primaryColor);
            app.updateSetting("theme_bg_" + accountID, chatBgColor);
            app.updateSetting("theme_bubble_" + accountID, bubbleColor);
            app.updateSetting("theme_badge_" + accountID, unreadBadgeColor);
        } else {
            // C++ tarafında getsetting yoksa veya database null ise varsayılanları kullan
            primaryColor = def.primary;
            chatBgColor = def.bg;
            bubbleColor = def.bubble;
            unreadBadgeColor = def.badge;
            console.log("[GENERIC-TAB] Database getSetting method not found, using defaults for: " + accountID);
        }
    }
    
    // HEX rengi belirtilen miktar kadar koyulaştıran fonksiyon
    function darkenColor(hexStr, amount) {
        if (!hexStr || hexStr.indexOf("#") !== 0) return "#333333"; // Güvenlifallback
        var num = parseInt(hexStr.replace("#", ""), 16);
        var r = Math.max(0, (num >> 16) - amount);
        var g = Math.max(0, ((num >> 8) & 0x00FF) - amount);
        var b = Math.max(0, (num & 0x0000FF) - amount);
        return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
    }
    
    // --- SAYFALAMA FONKSİYONLARI ---
    function ensureLoadMoreButton() {
        var size = internalModel.size();
        if (size > 0) {
            var last = internalModel.value(size - 1);
            if (last && (last.isLoadMore === true || last.isLoadMore === "true")) {
                return;
            }
        }
        internalModel.append({
                "isLoadMore": true,
                "isHeader": false
        });
    }
    
    function removeLoadMoreButton() {
        var size = internalModel.size();
        if (size > 0) {
            var last = internalModel.value(size - 1);
            if (last && (last.isLoadMore === true || last.isLoadMore === "true")) {
                internalModel.removeAt(size - 1);
            }
        }
    }
    
    function loadMoreChats() {
        if (!hasMoreChats || !database || !accountID) return;
        
        var nextOffset = chatOffset + pageLimit;
        var olderChats = database.getChatListForAccount(accountID, pageLimit, nextOffset);
        
        removeLoadMoreButton();
        
        if (!olderChats || olderChats.length === 0) {
            hasMoreChats = false;
            return;
        }
        
        chatOffset = nextOffset;
        for (var i = 0; i < olderChats.length; i++) {
            internalModel.append(olderChats[i]);
        }
        
        if (olderChats.length >= pageLimit) {
            ensureLoadMoreButton();
        } else {
            hasMoreChats = false;
        }
    }
    
    NavigationPane {
        id: navigationPane
        onPopTransitionEnded: { 
            if (typeof page.cleanup === 'function') {
                page.cleanup();
            }
            page.destroy(); 
        }
        
        onTopChanged: {
            if (page && page.targetMessageID && page.targetMessageID !== "") {
                page.scrollToTargetMessage();
            }
        }
        
        Page {
            titleBar: TitleBar { 
                id: titleBar
                kind: TitleBarKind.Default
                scrollBehavior: TitleBarScrollBehavior.Sticky
            }
            
            actions: [
                ActionItem {
                    title: "Mark All Read"
                    imageSource: "asset:///images/ic_done.png"
                    ActionBar.placement: ActionBarPlacement.OnBar
                    onTriggered: {
                        if (genericTab.database) {
                            genericTab.database.markAllChatsAsRead(genericTab.accountID);
                        }
                        var modelSize = internalModel.size();
                        for (var i = 0; i < modelSize; ++i) {
                            var item = internalModel.value(i);
                            if (item && item.unreadCount > 0) {
                                item.unreadCount = 0;
                                internalModel.replace(i, item);
                            }
                        }
                    }
                },
                ActionItem {
                    title: "Compose"
                    imageSource: "asset:///images/ic_compose.png"
                    ActionBar.placement: ActionBarPlacement.Signature
                    onTriggered: {
                        console.log("[GENERIC-TAB] Compose triggered for " + accountID);
                        var contactsPage = contactsPageDelegate.createObject();
                        contactsPage.accountID = accountID;
                        contactsPage.mainRef = mainRef;
                        
                        // Renkleri chatUI'a geçiriyoruz
                        contactsPage.primaryColor = genericTab.primaryColor;
                        contactsPage.chatBgColor = genericTab.chatBgColor;
                        contactsPage.bubbleColor = genericTab.bubbleColor;
                        navigationPane.push(contactsPage);
                    }
                },
                ActionItem {
                    title: "Search"
                    imageSource: "asset:///images/ic_search.png"
                    ActionBar.placement: ActionBarPlacement.OnBar
                    onTriggered: {
                        console.log("[GENERIC-TAB] Search triggered for " + genericTab.accountID);
                        var searchPage = searchPageDelegate.createObject();
                        searchPage.accountID = accountID;
                        searchPage.genericTabRef = genericTab;
                        navigationPane.push(searchPage);
                    }
                },
                ActionItem {
                    title: tabTitle+" Settings"
                    imageSource: "asset:///images/ic_settings.png"
                    ActionBar.placement: ActionBarPlacement.InOverflow
                    onTriggered: {
                        console.log("[GENERIC-TAB] Account Settings triggered");
                        var s = genericSettingsDelegate.createObject();
                        s.accountID = accountID;
                        s.accountName = tabTitle;
                        s.mainRef = mainRef;
                        // Renkleri ayarlara aktar
                        s.tPrimaryColor = primaryColor;
                        s.tChatBgColor = chatBgColor;
                        s.tBubbleColor = bubbleColor;
                        s.tUnreadBadgeColor = unreadBadgeColor;
                        navigationPane.push(s);
                    }
                }
            ]
            
            ListView {
                id: listView
                dataModel: internalModel
                property variant rootTab: genericTab           
                
                function itemType(data, indexPath) {
                    if (data && (data.isLoadMore === true || data.isLoadMore === "true")) {
                        return "loadMore";
                    }
                    return "";
                }
                
                onTriggered: {
                    var selectedItem = dataModel.data(indexPath);
                    
                    if (selectedItem && (selectedItem.isLoadMore === true || selectedItem.isLoadMore === "true")) {
                        return;
                    }
                    
                    if (selectedItem.unreadCount > 0) {
                        selectedItem.unreadCount = 0;
                        dataModel.replace(indexPath[0], selectedItem);
                        
                        if (genericTab.database) {
                            genericTab.database.markChatAsRead(selectedItem.accountID, selectedItem.chatID);
                        }
                    }
                    
                    var chatPage = chatUIDelegate.createObject(navigationPane);
                    chatPage.accountID = selectedItem.accountID;
                    chatPage.chatID = selectedItem.chatID;
                    chatPage.chatTitle = selectedItem.displayUpper;
                    chatPage.chatType = selectedItem.chatType; 
                    chatPage.readOnly = selectedItem.isReadOnly;
                    chatPage.unreadCount = selectedItem.unreadCount;
                    chatPage.mainRef = mainRef;
                    
                    // Renkleri chatUI'a geçiriyoruz
                    chatPage.primaryColor = genericTab.primaryColor;
                    chatPage.chatBgColor = genericTab.chatBgColor;
                    chatPage.bubbleColor = genericTab.bubbleColor;
                    
                    chatPage.loadMessages();
                    navigationPane.push(chatPage);
                }
                
                listItemComponents: [
                    // --- 1. LOAD MORE BUTTON BİLEŞENİ ---
                    ListItemComponent {
                        type: "loadMore"
                        CustomListItem {
                            id: loadMoreItemRoot
                            dividerVisible: false
                            
                            Container {
                                horizontalAlignment: HorizontalAlignment.Fill
                                topPadding: ui.sdu(2.0)
                                bottomPadding: ui.sdu(2.0)
                                layout: DockLayout {}
                                
                                Button {
                                    text: "Load More Chats"
                                    horizontalAlignment: HorizontalAlignment.Center
                                    verticalAlignment: VerticalAlignment.Center
                                    
                                    onClicked: {
                                        var currentListView = loadMoreItemRoot.ListItem.view;
                                        if (currentListView && currentListView.rootTab) {
                                            currentListView.rootTab.loadMoreChats();
                                        }
                                    }
                                }
                            }
                        }
                    },
                    
                    // --- 2. NORMAL CHAT BİLEŞENİ ---
                    ListItemComponent {
                        type: ""
                        CustomListItem {
                            dividerVisible: false
                            highlightAppearance: HighlightAppearance.Full
                            id: itemRoot
                            
                            onFocusedChanged: {
                                if (focused) {
                                    circle.visible = false;
                                    lastMessageTime.textStyle.color=Color.White;
                                    chatName.textStyle.color=Color.White;
                                    chatPreview.textStyle.color=Color.White;
                                    mute.filterColor=Color.White;
                                    pin.filterColor=Color.White;
                                } else {
                                    circle.visible = true;
                                    lastMessageTime.textStyle.color=(ListItemData.unreadCount > 0) ? Color.create(itemRoot.ListItem.view.rootTab.unreadBadgeColor) : Color.Gray
                                    chatName.textStyle.color=undefined;
                                    chatPreview.textStyle.color=undefined;
                                    mute.filterColor=Color.Gray;
                                    pin.filterColor=Color.Gray;

                                }
                            }
                            
                            contextActions: [
                                ActionSet {
                                    title: ListItemData.displayUpper
                                    subtitle: "Chat Options"
                                    
                                    ActionItem {
                                        title: (ListItemData.isPinned) ? "Unpin" : "Pin"
                                        imageSource: (ListItemData.isPinned) ? "asset:///images/unpin.png" : "asset:///images/pin.png"
                                        
                                        onTriggered: {
                                            var currentListView = itemRoot.ListItem.view;
                                            if(ListItemData.isPinned){
                                                currentListView.rootTab.database.setPin(ListItemData.chatID,false);
                                            }else{
                                                currentListView.rootTab.database.setPin(ListItemData.chatID,true);
                                            }                                                                                                                     
                                        }
                                    }
                                    
                                    ActionItem {
                                        title: (ListItemData.isMuted) ? "Unmute" : "Mute"
                                        imageSource: (ListItemData.isMuted) ? "asset:///images/unmute.png" : "asset:///images/mute.png"
                                        
                                        onTriggered: {
                                            var currentListView = itemRoot.ListItem.view; 
                                            if (currentListView && currentListView.rootTab && currentListView.rootTab.database) {
                                                currentListView.rootTab.database.setMute(ListItemData.chatID, ! ListItemData.isMuted);
                                                console.log("Chat:"+ListItemData.chatID+" için setMute "+ ! ListItemData.isMuted+" ayarı gönderildi!");
                                            }
                                        }
                                    }
                                    
                                    ActionItem {
                                        title: (ListItemData.unreadCount === 0) ? "Mark Unread" : "Mark Read"
                                        imageSource: (ListItemData.unreadCount === 0) ? "asset:///images/unread.png" : "asset:///images/read.png"
                                        
                                        onTriggered: {
                                            var currentListView = itemRoot.ListItem.view;
                                            if(ListItemData.unreadCount === 0){
                                                currentListView.rootTab.database.markChatAsUnread(ListItemData.accountID,ListItemData.chatID);
                                            }else{
                                                currentListView.rootTab.database.markChatAsRead(ListItemData.accountID,ListItemData.chatID);
                                            }                                                                                                                     
                                        }
                                    }
                                    
                                    attachedObjects: [
                                        SystemDialog {
                                            id: deleteConfirmDialog
                                            title: "Delete Chat"
                                            body: ListItemData.chatName
                                            
                                            confirmButton.label: "Delete"
                                            cancelButton.label: "Cancel"
                                            
                                            onFinished: {
                                                var currentListView = itemRoot.ListItem.view;  
                                                if (value == SystemUiResult.ConfirmButtonSelection) {
                                                    currentListView.rootTab.database.deleteChat(ListItemData.chatID);
                                                }
                                            }
                                        }
                                    ]
                                    
                                    DeleteActionItem {
                                        title: "Delete Chat"
                                        onTriggered: {
                                            deleteConfirmDialog.show();
                                        }
                                    }
                                }
                            ]
                            ListItem.onSelectionChanged: {
                                if (selected) {
                                    circle.visible=false;
                                }else{
                                    circle.visible=true;
                                }
                            }
                            Container {
                                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                                leftPadding: ui.sdu(2.0); rightPadding: ui.sdu(2.0); topPadding: ui.sdu(3.0); bottomPadding: ui.sdu(3.0)
                                
                                
                                Container {
                                    preferredWidth: ui.sdu(12.0)
                                    preferredHeight: ui.sdu(12.0)
                                    // Profil resmi arka planı primary color olsun
                                    background: Color.create(itemRoot.ListItem.view.rootTab.darkenColor(itemRoot.ListItem.view.rootTab.primaryColor, 90))
                                    verticalAlignment: VerticalAlignment.Center
                                    layout: DockLayout {}
                                    Label {
                                        visible: ListItemData.chatType=="single"
                                        text: ListItemData.avatarInitial
                                        textStyle.color: Color.White
                                        textStyle.fontSize: FontSize.Large
                                        horizontalAlignment: HorizontalAlignment.Center
                                        verticalAlignment: VerticalAlignment.Center
                                    }
                                    ImageView {
                                        visible: ListItemData.chatType=="channel"
                                        imageSource: "asset:///images/channel.png"
                                        horizontalAlignment: HorizontalAlignment.Center
                                        verticalAlignment: VerticalAlignment.Center
                                        preferredWidth: ui.sdu(8.0)
                                        preferredHeight: ui.sdu(8.0)
                                    }
                                    ImageView {
                                        visible: ListItemData.chatType=="group"
                                        imageSource: "asset:///images/ic_group_white.png"
                                        horizontalAlignment: HorizontalAlignment.Center
                                        verticalAlignment: VerticalAlignment.Center
                                        preferredWidth: ui.sdu(8.0)
                                        preferredHeight: ui.sdu(8.0)
                                    }
                                    ImageView {
                                        id: circle
                                        imageSource: "asset:///images/wPro.png"
                                        horizontalAlignment: HorizontalAlignment.Fill
                                        verticalAlignment: VerticalAlignment.Fill
                                        
                                    }
                                }
                                
                                Container {
                                    layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
                                    leftPadding: ui.sdu(2.5)
                                    layout: StackLayout { orientation: LayoutOrientation.TopToBottom }
                                    
                                    Container {
                                        
                                        horizontalAlignment: HorizontalAlignment.Fill
                                        layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                                        
                                        Label {
                                            id: chatName
                                            layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
                                            text: ListItemData.displayUpper
                                            textStyle.fontWeight: (ListItemData.unreadCount > 0) ? FontWeight.W500 : FontWeight.Normal
                                            textStyle.fontSize: FontSize.Medium
                                            topMargin: 0; bottomMargin: 0
                                        }
                                        Label {
                                            id: lastMessageTime
                                            text: ListItemData.lastMessageTime
                                            textStyle.fontSize: FontSize.XSmall
                                            textStyle.color: (ListItemData.unreadCount > 0) ? Color.create(itemRoot.ListItem.view.rootTab.unreadBadgeColor) : Color.Gray
                                            verticalAlignment: VerticalAlignment.Center                                           
                                        }
                                    }
                                    
                                    Container {
                                        topMargin: ui.sdu(1.0)
                                        horizontalAlignment: HorizontalAlignment.Fill
                                        layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                                        
                                        Label {
                                            id: chatPreview
                                            layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
                                            text: ListItemData.displayLower
                                            textStyle.fontSize: FontSize.Small
                                            textStyle.fontWeight: FontWeight.W200
                                            topMargin: 0; bottomMargin: 0
                                            multiline: false 
                                        }
                                        
                                        ImageView {
                                            id: mute
                                            visible: (ListItemData.isMuted) 
                                            imageSource: "asset:///images/mute.png" 
                                            preferredWidth: ui.sdu(4.5)
                                            preferredHeight: ui.sdu(4.5)
                                            verticalAlignment: VerticalAlignment.Center
                                            leftMargin: ui.sdu(1.0)
                                            filterColor: Color.Gray 
                                        }
                                        
                                        ImageView {
                                            id: pin
                                            visible: (ListItemData.isPinned) 
                                            imageSource: "asset:///images/pin.png" 
                                            preferredWidth: ui.sdu(4.5)
                                            preferredHeight: ui.sdu(4.5)
                                            verticalAlignment: VerticalAlignment.Center
                                            leftMargin: ui.sdu(1.0)
                                            filterColor: Color.Gray 
                                        }
                                        
                                        Container {
                                            visible: (ListItemData.unreadCount > 0)
                                            // Unread Badge Rengi
                                            background: Color.create(itemRoot.ListItem.view.rootTab.unreadBadgeColor)
                                            preferredWidth: ui.sdu(4.5); preferredHeight: ui.sdu(4.5)
                                            verticalAlignment: VerticalAlignment.Center
                                            leftMargin: ui.sdu(1.0)
                                            layout: DockLayout {}
                                            Label {
                                                text: ListItemData.unreadCount
                                                textStyle.color: Color.White
                                                textStyle.fontSize: FontSize.XSmall
                                                horizontalAlignment: HorizontalAlignment.Center
                                                verticalAlignment: VerticalAlignment.Center
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                ]
            }
        }
        
        attachedObjects: [
            ComponentDefinition {
                id: chatUIDelegate
                source: "chatUI.qml"
            },
            ComponentDefinition {
                id: genericSettingsDelegate
                source: "genericSettings.qml"
            },
            ComponentDefinition {
                id: contactsPageDelegate
                source: "contacts.qml"
            },
            ComponentDefinition {
                id: searchPageDelegate
                source: "search.qml"
            },       
            ComponentDefinition {
                id: aboutPage
                source: "about.qml"
            
            }
        ]
    }
}