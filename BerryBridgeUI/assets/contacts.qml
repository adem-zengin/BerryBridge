import bb.cascades 1.4
import bb.system 1.2

Page {
    id: contactsPage
    property string accountID
    property TabbedPane mainRef
    // ASENKRON İŞLEMLER İÇİN GEÇİCİ HAFIZA ALANLARI
    property string pendingChatName: ""
    property string pendingChatPhone: ""
    property string primaryColor: "#444444"
    property string chatBgColor: "#E5DDD5"
    property string bubbleColor: "#d8fdd2"
    property bool darkTheme
    property bool hasResults: false // Arama sonuçlarının gelip gelmediğini takip eden bayrak
    
    function darkenColor(hexStr, amount) {
        if (!hexStr || hexStr.indexOf("#") !== 0) return "#333333"; // Güvenlifallback
        var num = parseInt(hexStr.replace("#", ""), 16);
        var r = Math.max(0, (num >> 16) - amount);
        var g = Math.max(0, ((num >> 8) & 0x00FF) - amount);
        var b = Math.max(0, (num & 0x0000FF) - amount);
        return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
    }
    
    function onAvatarDownloaded(id, localUrl) {  
        // 1. Ana grup sayısını alıyoruz (childCount içine boş dizi verilir)
        var groupCount = contactModel.childCount([]);
        
        // 2. Grupları dönüyoruz
        for (var g = 0; g < groupCount; g++) {
            // O gruptaki eleman sayısını alıyoruz
            var itemCount = contactModel.childCount([g]);
            
            // 3. Grubun içindeki elemanları dönüyoruz
            for (var item = 0; item < itemCount; item++) {
                var indexPath = [g, item]; // GroupDataModel'in konum yolu (IndexPath)
                var currentData = contactModel.data(indexPath);
                
                // ID eşleşmesini kontrol ediyoruz
                if (currentData.id === id) {
                    // Veriyi güncelliyoruz
                    currentData.avatarUrl = localUrl;
                    
                    // GroupDataModel'deki elemanı bu indexPath ile güncelliyoruz
                    contactModel.updateItem(indexPath, currentData);
                    console.log("GroupDataModel içinde avatar güncellendi, ID:", id);
                    return;
                }
            }
        }
    }
    
    function onChatCreatedSuccessHandler(responseJson, avatar) {
        console.log("Sohbet başarıyla oluşturuldu: " + responseJson);
        
        var chatID = "";
        
        // 1. C++'tan dönen JSON'ı parse edip ID'yi almaya çalışalım
        try {
            var data = JSON.parse(responseJson);
            if (data && data.chatID) {
                chatID = data.chatID;
            }
        } catch(e) {
            console.log("JSON Parse hatası, yerel DB'den denenecek...");
        }
        
        // 2. Eğer JSON'dan ID gelmediyse yerel DB'den (C++ dat objesinden) numarayı sorgula
        if (chatID === "") {
            chatID = dat.searchChatByPhone(pendingChatPhone, accountID);
        }
        
        // 3. Bulduğumuz ID ile sayfayı açıyoruz
        if (chatID !== "") {
            openChatPage(pendingChatName, chatID, "single", avatar, false);
        } else {
            console.log("Hata: Sohbet oluşturuldu fakat Chat ID tespit edilemedi.");
        }
        
        // Geçici hafızayı temizliyoruz
        pendingChatName = "";
        pendingChatPhone = "";
    }
    
    function onChatCreatedErrorHandler(statusCode, errorString) {
        console.log("Error code: " + statusCode + " | Details: " + errorString);
        
        // Clear memory on error
        pendingChatName = "";
        pendingChatPhone = "";
        
        // Prepare the English user message
        var userMessage = "An error occurred while creating the chat.";
        
        // Set body and show dialog
        chatErrorDialog.body = userMessage;
        chatErrorDialog.show();
    }
    
    function onSearchContactsFinished() {
        contactModel.clear();
        refreshContacts(searchField.text, accountID);
        errorCont.visible= false;
    }
    
    function onSearchContactsError(errorString) {
        contactModel.clear();
        refreshContacts(searchField.text, accountID);
        errorLabel.text= errorString;
        errorCont.visible= true;
    }
    
    onAccountIDChanged: {
        initializeContacts(accountID);
    }
    
    Container {
        layout: StackLayout {}
        topPadding: ui.du(2.0); bottomPadding: ui.du(2.0)
        leftPadding: ui.du(2.0); rightPadding: ui.du(2.0)
        
        TextField {
            id: searchField
            hintText: "Search contacts, groups and channels"
            input.submitKey: SubmitKey.Search
            input.onSubmitted: {
                if (text.trim() === "") {
                    contactModel.clear();
                    initializeContacts(accountID);
                    hasResults = false;
                    filterCont.visible = false;
                    return;
                }else{
                    filterCont.visible = true;
                }
                statusCont.visible= false;
                errorCont.visible= false;
                searchAct.running = true;
                contactModel.clear();
                dat.searchContacts(accountID, "", text);              
            }

        }
        
        
        Container {
            id: filterCont  
            visible: false
            SegmentedControl {
                id: filterControl               
                onSelectedOptionChanged: {
                    if (searchField.text.trim() !== "") {
                        refreshContacts(searchField.text, accountID);
                    }
                }
                options: [
                    Option {
                        text: "All"
                        selected: true
                    },
                    Option {
                        text: "Local"
                    },
                    Option {
                        text: "Account"
                    }
                ]
            }
        }
        
        ActivityIndicator {
            id: searchAct
            running: false
            minHeight: ui.du(8.0)     
            horizontalAlignment: HorizontalAlignment.Center
            topMargin: ui.sdu(3.0)
        }
        
        Container {
            id: statusCont
            visible: false
            horizontalAlignment: HorizontalAlignment.Fill
            topPadding: ui.sdu(2.0)
            Label {
                id: statusLabel
                horizontalAlignment: HorizontalAlignment.Fill
                text: "No results found"
                textStyle.textAlign: TextAlign.Center
                multiline: true
            }
        }
        
        Container {
            id: errorCont
            visible: false
            horizontalAlignment: HorizontalAlignment.Fill
            background: Color.Red
            Label {
                id: errorLabel
                horizontalAlignment: HorizontalAlignment.Fill
                textStyle.textAlign: TextAlign.Center
                multiline: true
                textStyle.color: Color.White
            }
        }
        
        
        ListView {
            id: contactsList
            property variant rootPage: contactsPage  
            dataModel: contactModel
            onTriggered: {
                var selectedItem = dataModel.data(indexPath);
                if (selectedItem.type === "group") {
                    openChatPage(selectedItem.displayName, selectedItem.id, "group", selectedItem.avatarUrl, selectedItem.isReadOnly);
                    return;
                }
                
                // Tüm iletişim yöntemlerini (numara + e-posta) bu dizide toplayacağız
                var contactMethods = [];
                
                // 1. Varsa tüm telefon numaralarını diziye ekle
                if (selectedItem.number && selectedItem.number.toString().trim() !== "") {
                    var nList = selectedItem.number.toString().split(",");
                    for (var i = 0; i < nList.length; i++) {
                        var num = nList[i].trim();
                        if (num !== "") {
                            contactMethods.push(num);
                        }
                    }
                } 
                
                // 2. Varsa tüm e-posta adreslerini de AYNI diziye ekle
                if (selectedItem.emails && selectedItem.emails.toString().trim() !== "") {
                    var eList = selectedItem.emails.toString().split(",");
                    for (var j = 0; j < eList.length; j++) {
                        var mail = eList[j].trim();
                        if (mail !== "") {
                            contactMethods.push(mail);
                        }
                    }
                }
                
                // 3. Toplam iletişim adresi sayısına göre karar ver
                if (contactMethods.length > 1) {
                    var sheet = phoneSheetDefinition.createObject();
                    sheet.numbersArray = contactMethods;
                    sheet.avatar = selectedItem.avatarUrl; 
                    sheet.contactName = selectedItem.displayName;
                    sheet.numberSelected.connect(processDirectChat);
                    sheet.open();
                } 
                else if (contactMethods.length === 1) {
                    processDirectChat(selectedItem.displayName, contactMethods[0], selectedItem.avatarUrl);
                } 
                else {
                    console.log("Hata: Kişinin geçerli bir numarası veya e-postası yok.");
                }
            }
            
            function processDirectChat(name, phone, avatar) {
                var foundId = dat.searchChatByPhone(phone, accountID);
                if (foundId !== "") {
                    openChatPage(name, foundId, "single", avatar, false);
                } else {                 
                    pendingChatName = name;
                    pendingChatPhone = phone;
                    dat.createChat(accountID, phone, "single", avatar);
                }
            }
            
            listItemComponents: [
                ListItemComponent {
                    type: "item" 
                    StandardListItem {
                        id: itemRoot
                        title: ListItemData.displayName
                        description: {
                            if (ListItemData.type === "group") {
                                return "Group Chat";
                            }
                            
                            var desc = "";
                            if (ListItemData.number && ListItemData.number.toString().trim() !== "") {
                                desc = ListItemData.number.toString().split(",")[0].trim(); 
                            } else if (ListItemData.emails && ListItemData.emails.toString().trim() !== "") {
                                desc = ListItemData.emails.toString().split(",")[0].trim();
                            }
                            return desc;
                        }
                        
                        imageSource: {
                            if (ListItemData.avatarUrl && ListItemData.avatarUrl !== "") {
                                return ListItemData.avatarUrl;
                            }
                            var isDark = itemRoot.ListItem.view.rootPage.darkTheme;
                            return ListItemData.type === "group" 
                            ? (isDark ? "asset:///images/ic_group.png" : "asset:///images/ic_group_dark.png")
                            : (isDark ? "asset:///images/ic_contact.png" : "asset:///images/ic_contact_dark.png");
                        }    
                    }
                },
                ListItemComponent {
                    type: "header"
                    Header {
                        title: ListItemData
                    }
                }
            ]
        }
    }
    
    function openChatPage(title, id, type, avatar, isReadOnly) {
        var chatPage = goChat.createObject();
        chatPage.chatTitle = title;
        chatPage.chatID = id;
        chatPage.chatType = type;
        chatPage.accountID = accountID;
        chatPage.avatarUrl = avatar;
        chatPage.readOnly = isReadOnly;
        chatPage.avatarColor = darkenColor(primaryColor, 90);
        chatPage.avatarInitial = title ? title.trim().charAt(0).toUpperCase() : "";
        chatPage.mainRef = mainRef;
        chatPage.primaryColor = primaryColor;
        chatPage.chatBgColor = chatBgColor;
        chatPage.bubbleColor = bubbleColor;
        chatPage.loadMessages();
        navigationPane.push(chatPage);
        navigationPane.remove(contactsPage);
    }
    
    attachedObjects: [
        SystemDialog {
            id: chatErrorDialog
            title: "Failed to Start Chat"
            confirmButton.label: "OK"
            cancelButton.label: "" 
            onFinished: {}
        },
        GroupDataModel {
            id: contactModel
            sortingKeys: ["displayName"]
            grouping: ItemGrouping.ByFirstChar
        },
        ComponentDefinition {
            id: goChat
            source: "chatUI.qml"
        },
        ComponentDefinition {
            id: phoneSheetDefinition
            Sheet {
                id: selectionSheet
                property variant numbersArray: [] 
                property string contactName: ""
                property string avatar
                
                signal numberSelected(string name, string phone, string avatar)
                
                Page {
                    titleBar: TitleBar {
                        title: "Select Number"
                        dismissAction: ActionItem {
                            title: "Cancel"
                            onTriggered: { selectionSheet.close(); }
                        }
                    }
                    
                    Container {
                        topPadding: 40.0
                        Label {
                            text: selectionSheet.contactName
                            horizontalAlignment: HorizontalAlignment.Center
                            textStyle.base: SystemDefaults.TextStyles.TitleText
                        }
                        
                        ListView {
                            dataModel: ArrayDataModel { id: sheetModel }
                            onCreationCompleted: {
                                if (selectionSheet.numbersArray.length > 0) {
                                    sheetModel.append(selectionSheet.numbersArray);
                                }
                            }
                            listItemComponents: [
                                ListItemComponent {
                                    StandardListItem { title: ListItemData }
                                }
                            ]
                            onTriggered: {
                                var chosenNum = dataModel.data(indexPath);                                
                                selectionSheet.numberSelected(selectionSheet.contactName, chosenNum, selectionSheet.avatar);
                                selectionSheet.close();                                
                            }
                        }
                    }
                }
                onNumbersArrayChanged: {
                    sheetModel.clear();
                    sheetModel.append(numbersArray);
                }
            }
        }
    ]
    
    function initializeContacts(accID) {
        if (!accID || accID === "") return;
        if (!dat) return;
        
        contactModel.clear();
        hasResults = false;
        var combinedData = [];
        
        var groups = dat.getGroupsFromDb("", accID);
        if (groups && groups.length > 0) {
            for (var i = 0; i < groups.length; i++) {
                combinedData.push(groups[i]);
            }
        }
        
        if (combinedData.length > 0) {
            contactModel.insertList(combinedData);
        }
    }
    
    function refreshContacts(searchQuery, accID) {
        if (!accID || accID === "") return;
        if (!dat) return;
        
        contactModel.clear();
        var combinedData = [];
        var mode = filterControl.selectedIndex; // 0: All, 1: Local, 2: Account
        
        // Mode 2 veya Mode 0 (All / Account): getGroupsFromDb ve getContactsFromDb[cite: 1]
        if (mode === 0 || mode === 2) {
            var groups = dat.getGroupsFromDb(searchQuery, accID);
            if (groups && groups.length > 0) {
                for (var i = 0; i < groups.length; i++) {
                    combinedData.push(groups[i]);
                }
            }
            
            var contacts = dat.getContactsFromDb(searchQuery, accID);
            if (contacts && contacts.length > 0) {
                for (var j = 0; j < contacts.length; j++) {
                    combinedData.push(contacts[j]);
                }
            }
        }
        
        // Mode 1 veya Mode 0 (All / Local): getDeviceContacts[cite: 1]
        if (mode === 0 || mode === 1) {
            var locals = dat.getDeviceContacts(searchQuery);
            if (locals && locals.length > 0) {
                for (var k = 0; k < locals.length; k++) {
                    combinedData.push(locals[k]);
                }
            }
        }
        
        if (combinedData.length > 0) {
            contactModel.insertList(combinedData);
            hasResults = true;
            statusCont.visible = false;
        } else {
            hasResults = false;
            statusCont.visible= true;
        }
        searchAct.running = false;
    }
    
    onCreationCompleted: {
        dat.chatCreatedSuccess.connect(onChatCreatedSuccessHandler);
        dat.chatCreatedError.connect(onChatCreatedErrorHandler);
        dat.searchContactsFinished.connect(onSearchContactsFinished);
        dat.searchContactsError.connect(onSearchContactsError);
        dat.avatarDownloaded.connect(onAvatarDownloaded);
        darkTheme = app.darkMode;
    }
    
    function cleanup() {
        console.log("Contacts page cleaning up connections...");
        try {
            dat.chatCreatedSuccess.disconnect(onChatCreatedSuccessHandler);
            dat.chatCreatedError.disconnect(onChatCreatedErrorHandler);
            dat.searchContactsFinished.disconnect(onSearchContactsFinished);
            dat.searchContactsError.disconnect(onSearchContactsError);
            dat.avatarDownloaded.disconnect(onAvatarDownloaded);
        } catch (e) {}
    }
}