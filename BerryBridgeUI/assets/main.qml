import bb.cascades 1.4
import bb.system 1.2

TabbedPane {
    property variant activeUploads: ({})
    property variant navPane;
    id: tabbedPane
    showTabsOnActionBar: false
    
    onActiveTabChanged: {
        navPane = activeTab.content;
        activeTab.newContentAvailable = false;
        app.clearNewContent(activeTab.accountID);
        // Renk doğrudan GenericTab içerisinden dinamik olarak gelir
        if (activeTab && activeTab.primaryColor) {
            app.setActionBarColor(activeTab.primaryColor);
        } else {
            app.setActionBarColor("#444444"); // Fallback
        }
        
        // SON AÇILAN SEKKEYİ QSETTINGS'E KAYDET
        if (activeTab && activeTab.accountID && dat && typeof dat.updateSetting === "function") {
            app.updateSetting("last_active_tab", activeTab.accountID);
            console.log("[MAIN-QML] Saved last active tab: " + activeTab.accountID);
        }
    }
    
    function getTabIcon(networkName) {
        var net = networkName ? networkName.toLowerCase() : "";
        if (net.indexOf("whatsapp") !== -1) return "asset:///images/whatsapp.png";
        if (net.indexOf("telegram") !== -1) return "asset:///images/telegram.png";
        if (net.indexOf("instagram") !== -1) return "asset:///images/instagram.png";
        if (net.indexOf("signal") !== -1) return "asset:///images/signal.png";
        if (net.indexOf("facebook") !== -1) return "asset:///images/facebook.png";
        if (net.indexOf("twitter") !== -1) return "asset:///images/x.png";
        if (net.indexOf("gmessages") !== -1 || net.indexOf("googlemessages") !== -1 || net.indexOf("rcs") !== -1 || net.indexOf("sms") !== -1) return "asset:///images/googlemessages.png";
        if (net.indexOf("googlechat") !== -1 || net.indexOf("gchat") !== -1) return "asset:///images/googlechat.png";
        if (net.indexOf("googlevoice") !== -1 || net.indexOf("gvoice") !== -1) return "asset:///images/googlevoice.png";
        if (net.indexOf("linkedin") !== -1) return "asset:///images/linkedin.png";
        if (net.indexOf("discord") !== -1) return "asset:///images/discord.png";
        if (net.indexOf("slack") !== -1) return "asset:///images/slack.png";
        if (net.indexOf("irc") !== -1) return "asset:///images/irc.png";
        if (net.indexOf("matrix") !== -1) return "asset:///images/matrix.png";
        if (net.indexOf("imessage") !== -1) return "asset:///images/imessage.png";
        if (net.indexOf("line") !== -1) return "asset:///images/line.png";
        if (net.indexOf("tumblr") !== -1) return "asset:///images/tumblr.png";
    }
    
    function addActiveUpload(msgId, msgObj) {
        var temp = activeUploads;
        temp[msgId] = msgObj;
        activeUploads = temp;
    }
    
    function removeActiveUpload(msgId) {
        var temp = activeUploads;
        delete temp[msgId];
        activeUploads = temp;
    }
    
    function getActiveUploadsForChat(accId, cId) {
        var list = [];
        if (!activeUploads) {
            return list;
        }
        
        for (var key in activeUploads) {
            var item = activeUploads[key];
            if (item && item.accountID === accId && item.chatID === cId) {
                list.push(item);
            }
        }
        return list;
    }
    
    Menu.definition: MenuDefinition {
        // Specify the actions that should be included in the menu
        actions: [
            
            ActionItem {
                id: menuSettings
                title: "Settings"
                imageSource: "asset:///images/ic_settings.png"
                
                onTriggered: {
                    var s = settingsPageDefinition.createObject();               
                    setupSheet.setContent(s);
                    setupSheet.open();
                }
            }
            ,ActionItem {
                id: menuAbout
                title: "About"
                imageSource: "asset:///images/ic_info.png"
                
                onTriggered: {
                    navPane.push(aboutPage.createObject());
                }
            }
        ]       
    }
    
    
    sidebarState: SidebarState.VisibleFull
    
    onCreationCompleted: {
        app.dbUpdateTriggerChanged.connect(function() {
                refreshTabModels();
        });
    
        dat.dataRefreshRequested.connect(function() {
                console.log("refresh request received from Database.cpp")
                refreshTabModels();
        });
    
        dat.messageSentSuccessfully.connect(tabbedPane.removeActiveUpload);
        app.updateCheckCompleted.connect(tabbedPane.checkUpdate);
        app.checkForUpdates();
        
        if (!dat.initRun) {
            var s = settingsPageDefinition.createObject();
            setupSheet.setContent(s);
            setupSheet.open();
        } else {
            updateAccountTabs();
        }
    }
    
    function checkUpdate(updateRequired,latestVersion, releaseNotes){
        console.log("latestVersion:"+latestVersion);
        if (updateRequired){
            updateDialog.body=updateDialog.body+"\n"+formatToPlainText(releaseNotes);
            updateDialog.show();         
        }
    }
    
    function updateAccountTabs() {
        console.log("[MAIN-QML] updateAccountTabs called");
        var accountsRaw = dat.getSelectedAccountsForMain();
        if (!accountsRaw) {
            console.log("[MAIN-QML] No accounts returned from database.");
            return;
        }
        
        var accounts = accountsRaw;
        
        var count = tabbedPane.count();
        for (var k = count - 1; k >= 0; k--) {
            var t = tabbedPane.at(k);
            tabbedPane.remove(t);
            t.destroy();
        }
        
        // --- YENİ: Kaydedilen son aktif sekme ID'sini al ---
        var targetTabToSelect = null;
        var lastSavedAccountID = (dat && typeof dat.getSetting === "function") ? app.getSetting("last_active_tab", "") : "";
        console.log("[MAIN-QML] Last saved tab from settings: " + lastSavedAccountID);
        
        for (var i = 0; i < accounts.length; i++) {
            var account = accounts[i];
            
            // Artık tek tip tab kullanıyoruz
            var newTab = genericTabDelegate.createObject();
            
            if (newTab) {
                newTab.tabTitle = account.network;
                newTab.titleBarTitle = account.network;
                newTab.imgSource = getTabIcon(account.accountID);
                newTab.accountID = account.accountID;
                newTab.mainRef = tabbedPane;
                newTab.newContentAvailable = app.hasNewContent(account.accountID);
                // Not: Kodundaki mükerrer newTab.accountID ataması temizlendi
                
                if (typeof newTab.getModel === "function") {
                    var model = newTab.getModel();
                    if (model) {
                        model.clear();
                        var limit = ("pageLimit" in newTab) ? newTab.pageLimit : 25;
                        var chats = dat.getChatListForAccount(account.accountID, limit, 0);
                        if (Array.isArray(chats)) {
                            for (var j = 0; j < chats.length; ++j) {
                                model.append(chats[j]);
                            }
                            if (typeof newTab.ensureLoadMoreButton === "function" && chats.length >= limit) {
                                newTab.ensureLoadMoreButton();
                            }
                        }
                    }
                }
                
                tabbedPane.add(newTab);
                
                // --- YENİ: Bu sekme kaydedilen son sekme mi kontrol et ---
                if (lastSavedAccountID !== "" && account.accountID === lastSavedAccountID) {
                    targetTabToSelect = newTab;
                }
            }
        }
        
        // --- YENİ: Hedef sekme varsa onu seç, yoksa ilk sekmeye dön ---
        if (tabbedPane.count() > 0) {
            if (targetTabToSelect) {
                tabbedPane.activeTab = targetTabToSelect;
            } else {
                tabbedPane.activeTab = tabbedPane.at(0);
            }
        }
    }
    
    // BlackBerry 10 Qt 4.8 / ES5 Uyumlu Plain Text Formatlayıcı
    function formatToPlainText(input) {
        if (!input) return "";
        
        var text = input;
        
        // 1. Satır sonlarını standardize et (\r\n veya \r -> \n)
        text = text.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
        
        // 2. HTML etiketlerini ve yapılarını temizle (Eğer önceden HTML/C++ dönüşümü geldiyse)
        text = text.replace(/<br\s*\/?>/gi, "\n");
        text = text.replace(/<a\s+[^>]*href=["']([^"']+)["'][^>]*>(.*?)<\/a>/gi, "$2 ($1)");
        text = text.replace(/<[^>]+>/g, ""); // Diğer tüm HTML etiketlerini kaldır (<b>, <i> vs)
        
        // 3. HTML Entity'lerini çöz
        text = text.replace(/&bull;/g, "•")
        .replace(/&amp;/g, "&")
        .replace(/&lt;/g, "<")
        .replace(/&gt;/g, ">")
        .replace(/&quot;/g, '"')
        .replace(/&#39;/g, "'")
        .replace(/&nbsp;/g, " ");
        
        // 4. Markdown Link Yapısı: [Başlık](URL) -> Başlık (URL)
        text = text.replace(/\[([^\]]+)\]\(([^)]+)\)/g, "$1 ($2)");
        
        // 5. Markdown Kalın / İtalik temizleme: **metin** veya *metin* -> metin
        text = text.replace(/\*\*(.*?)\*\*/g, "$1");
        text = text.replace(/\*(.*?)\*/g, "$1");
        text = text.replace(/__(.*?)__/g, "$1");
        
        // 6. Başlıklar: # Başlık -> --- BAŞLIK ---
        text = text.replace(/^#{1,6}\s*(.*)$/gm, function(match, p1) {
                return "\n--- " + p1.toUpperCase() + " ---";
        });
    
        // 7. Liste Maddelerini Okunaklı Yap: "- " veya "* " -> "  • "
        text = text.replace(/^[\s]*[\-\*]\s+/gm, "  • ");
        
        // 8. Ardışık gereksiz boş satırları temizle (En fazla 2 satır kalabilir)
        text = text.replace(/\n{3,}/g, "\n\n");
        
        // 9. Başındaki ve sonundaki boşlukları temizle
        return text.trim();
    }
    
    function refreshTabModels() {
        // [Bu fonksiyon orijinalindekiyle aynı kalıyor...]
        console.log("[MAIN-QML] Refreshing models for existing tabs...");
        var count = tabbedPane.count();
        for (var i = 0; i < count; i++) {
            var tab = tabbedPane.at(i);
            if(tabbedPane.activeTab === tab){
                app.clearNewContent(tab.accountID);
            }else{
                tab.newContentAvailable = app.hasNewContent(tab.accountID);    
            }
                        
            if (tab && typeof tab.getModel === "function" && tab.accountID) {
                var model = tab.getModel();
                if (model) {
                    var currentOffset = ("chatOffset" in tab) ? tab.chatOffset : 0;
                    var limit = ("pageLimit" in tab) ? tab.pageLimit : 25;
                    var totalToFetch = currentOffset + limit;
                    
                    var chats = dat.getChatListForAccount(tab.accountID, totalToFetch, 0);
                    if (Array.isArray(chats)) {
                        var existingSize = model.size();
                        var hasButton = false;
                        if (existingSize > 0) {
                            var lastItem = model.value(existingSize - 1);
                            if (lastItem && (lastItem.isLoadMore === true || lastItem.isLoadMore === "true")) {
                                hasButton = true;
                            }
                        }
                        
                        var realChatCount = hasButton ? (existingSize - 1) : existingSize;
                        var newSize = chats.length;
                        
                        for (var j = 0; j < newSize; ++j) {
                            if (j < realChatCount) {
                                model.replace(j, chats[j]);
                            } else {
                                if (hasButton) {
                                    model.insert(j, chats[j]);
                                } else {
                                    model.append(chats[j]);
                                }
                            }
                        }
                        
                        while (realChatCount > newSize) {
                            model.removeAt(newSize - 1);
                            realChatCount--;
                        }
                        
                        if (typeof tab.ensureLoadMoreButton === "function") {
                            if (newSize >= totalToFetch) {
                                tab.ensureLoadMoreButton();
                            } else if (typeof tab.removeLoadMoreButton === "function") {
                                tab.removeLoadMoreButton();
                            }
                        }
                    }
                }
            }
        }
    }
    
    attachedObjects: [
        ComponentDefinition {
            id: settingsPageDefinition
            source: "settings.qml"
        },
        ComponentDefinition {
            id: aboutPage
            source: "about.qml"
        },
        ComponentDefinition {
            id: genericTabDelegate
            source: "GenericTab.qml"
        },
        Sheet { 
            id: setupSheet 
            peekEnabled: false
            onClosed: {
                console.log("[MAIN-QML] Settings sheet closed. Refreshing UI...");
                updateAccountTabs();
            }
        },
        SystemDialog {
            id: updateDialog
            title: "Update Notice"
            body: "A newer version is available!"
            confirmButton.label: "OK"
            cancelButton.label: ""            
        }
    ]
}