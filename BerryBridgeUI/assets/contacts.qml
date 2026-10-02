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
    
    function onChatCreatedSuccessHandler(responseJson) {
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
            openChatPage(pendingChatName, chatID, "single");
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
    
    onAccountIDChanged: {
        console.log("[CONTACTS] accountID set to: " + accountID);
        refreshContacts(searchField.text, accountID);
    }
    
    titleBar: TitleBar {
        title: "Contacts & Groups"
        dismissAction: ActionItem {
            title: "Cancel"
            onTriggered: { navigationPane.pop(); }
        }
    }
    
    Container {
        layout: StackLayout {}
        topPadding: 20.0; leftPadding: 20.0; rightPadding: 20.0
        
        TextField {
            id: searchField
            hintText: "Search everyone..."
            onTextChanging: {
                refreshContacts(text, accountID);
            }
        }
        
        ListView {
            id: contactsList
            dataModel: contactModel
            topMargin: 20.0
            onTriggered: {
                var selectedItem = dataModel.data(indexPath);
                if (selectedItem.type === "group") {
                    openChatPage(selectedItem.displayName, selectedItem.id, "group");
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
                
                // 2. Varsa tüm e-posta adreslerini de AYNı diziye ekle
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
                    // Kişinin birden fazla iletişim adresi var (Örn: 2 tel, veya 1 tel + 1 email, veya 3 email)
                    var sheet = phoneSheetDefinition.createObject();
                    
                    // Mevcut numbersArray değişkenine birleştirilmiş listeyi gönderiyoruz 
                    // (İçinde hem telefon hem email listelenecek)
                    sheet.numbersArray = contactMethods; 
                    sheet.contactName = selectedItem.displayName;
                    sheet.numberSelected.connect(processDirectChat);
                    sheet.open();
                } 
                else if (contactMethods.length === 1) {
                    // Kişinin sadece tek bir iletişim bilgisi var (Sadece 1 tel VEYA sadece 1 email)
                    processDirectChat(selectedItem.displayName, contactMethods[0]);
                } 
                else {
                    // Kişiye ait hiçbir iletişim verisi yok
                    console.log("Hata: Kişinin geçerli bir numarası veya e-postası yok.");
                }
            }
            
            // Yardımcı Fonksiyon: Numarayı sorgula ve odayı aç
            function processDirectChat(name, phone) {
                var foundId = dat.searchChatByPhone(phone, accountID);
                if (foundId !== "") {
                    openChatPage(name, foundId, "single");
                } else {                 
                    // Sinyal geldiğinde hatırlamak için bilgileri yukarıdaki değişkenlere kilitliyoruz
                    pendingChatName = name;
                    pendingChatPhone = phone;
                    
                    // C++ tarafındaki fonksiyonu tetikliyoruz
                    dat.createChat(accountID, phone, "single");
                    
                    // NOT: Eski koddaki processDirectChat(name, phone) satırı 
                    // sonsuz döngü yaratıyordu, asenkron mimaride burası boş bırakılmalıdır.
                }
            }
            
            listItemComponents: [
                ListItemComponent {
                    type: "item" 
                    StandardListItem {
                        title: ListItemData.displayName
                        description: {
                            // 1. Önce gelen verinin bir grup olup olmadığını kontrol ediyoruz
                            if (ListItemData.type === "group") {
                                return "Group Chat";
                            }
                            
                            // 2. Grup değilse, sizin yazdığınız kişi numarası/e-posta mantığı çalışıyor
                            var desc = "";
                            if (ListItemData.number && ListItemData.number.toString().trim() !== "") {
                                desc = ListItemData.number.toString().split(",")[0].trim(); 
                            } else if (ListItemData.emails && ListItemData.emails.toString().trim() !== "") {
                                desc = ListItemData.emails.toString().split(",")[0].trim();
                            }
                            return desc;
                        }
                        imageSource: ListItemData.type === "group" ? "asset:///images/ic_group.png" : "asset:///images/ic_contact.png"
                    }
                },
                ListItemComponent {
                    type: "header" // Harf başlıklarının (A, B, vs.) görünmesi için eklenmeli
                    Header {
                        title: ListItemData
                    }
                }
            ]
        }
    }
    
    // ARTIK KÖK SEVİYEDE: Dosyadaki tüm bileşenler ve attachedObjects doğrudan erişebilir
    function openChatPage(title, id, type) {
        var chatPage = goChat.createObject();
        chatPage.chatTitle = title;
        chatPage.chatID = id;
        chatPage.chatType = type;
        chatPage.accountID = accountID;
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
            onFinished: {
                // User pressed OK
            }
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
                signal numberSelected(string name, string phone)
                
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
                                selectionSheet.numberSelected(selectionSheet.contactName, chosenNum);
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
    
    function refreshContacts(searchQuery, accID) {
        if (!accID || accID === "") return;
        if (!dat) return;
        
        contactModel.clear();
        var combinedData = [];
        
        // 1. Grupları çek ve birleştir
        var groups = dat.getGroupsFromDb(searchQuery, accID);
        if (groups && groups.length > 0) {
            for (var i = 0; i < groups.length; i++) {
                combinedData.push(groups[i]);
            }
        }
        
        // 2. Kişileri çek ve birleştir
        var contacts = dat.getDeviceContacts(searchQuery);
        if (contacts && contacts.length > 0) {
            for (var j = 0; j < contacts.length; j++) {
                combinedData.push(contacts[j]);
            }
        }
        
        // 3. Tek seferde modele aktar
        if (combinedData.length > 0) {
            contactModel.insertList(combinedData);
        }
    }
    
    onCreationCompleted: {
        dat.chatCreatedSuccess.connect(onChatCreatedSuccessHandler);
        dat.chatCreatedError.connect(onChatCreatedErrorHandler);
    }
}