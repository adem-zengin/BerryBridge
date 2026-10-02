import bb.cascades 1.4

Page {
    id: searchPage
    property string accountID
    property Tab genericTabRef
    
    content: Container {
        layout: StackLayout { orientation: LayoutOrientation.TopToBottom }
        topPadding: ui.du(2.0); bottomPadding: ui.du(2.0)
        leftPadding: ui.du(2.0); rightPadding: ui.du(2.0)
        
        // Arama Barı
        Container {
            horizontalAlignment: HorizontalAlignment.Fill
            bottomPadding: ui.du(1.5)
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            
            TextField {
                id: searchField
                hintText: "Search in text messages..."
                verticalAlignment: VerticalAlignment.Center
                layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
            }
            
            Button {
                text: "Go"
                verticalAlignment: VerticalAlignment.Center
                leftMargin: ui.du(1.5)
                preferredWidth: ui.du(12.0)
                onClicked: {
                    searchModel.clear();
                    var results = dat.searchMessages(accountID, searchField.text);
                    searchModel.append(results);
                }
            }
        }
        
        // Arama Sonuçları Listesi
        ListView {
            id: listView
            dataModel: searchModel
            layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
            
            // TIKLAMA OLAYI (Navigation & Chat Açma)
            onTriggered: {
                var selectedItem = dataModel.data(indexPath);
                
                var chatPage = chatUIDelegate.createObject();
                chatPage.accountID = searchPage.accountID;
                chatPage.chatID = selectedItem.chatID;
                chatPage.chatTitle = selectedItem.chatTitle;
                // Hedef mesajın ID'sini chatUI'ye aktarıyoruz:
                chatPage.targetMessageID = selectedItem.messageID;
                chatPage.isSearchMode = true;
                chatPage.mainRef = genericTabRef.mainRef;
                
                // Renkleri chatUI'a geçiriyoruz
                chatPage.primaryColor = genericTabRef.primaryColor;
                chatPage.chatBgColor = genericTabRef.chatBgColor;
                chatPage.bubbleColor = genericTabRef.bubbleColor;   
                
                chatPage.loadMessages();             
                
                navigationPane.push(chatPage);
            }
            
            listItemComponents: [
                ListItemComponent {
                    type: ""
                    CustomListItem {
                        dividerVisible: true
                        highlightAppearance: HighlightAppearance.Full
                        
                        Container {
                            layout: StackLayout { orientation: LayoutOrientation.TopToBottom }
                            leftPadding: ui.du(2.0); rightPadding: ui.du(2.0)
                            topPadding: ui.du(1.5); bottomPadding: ui.du(1.5)
                            
                            Container {
                                horizontalAlignment: HorizontalAlignment.Fill
                                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                                
                                Label {
                                    layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
                                    text: ListItemData.chatTitle
                                    textStyle.fontWeight: FontWeight.Bold
                                    textStyle.fontSize: FontSize.Medium
                                }
                                Label {
                                    text: ListItemData.timestamp
                                    textStyle.fontSize: FontSize.XSmall
                                    textStyle.color: Color.Gray
                                    verticalAlignment: VerticalAlignment.Center
                                }
                            }
                            
                            Label {
                                text: ListItemData.snippet
                                textStyle.color: Color.DarkGray
                                textStyle.fontSize: FontSize.Small
                                textStyle.fontWeight: FontWeight.W200
                                multiline: true
                                topMargin: ui.du(0.5)
                            }
                        }
                    }
                }
            ]
        }
    }
    
    attachedObjects: [
        ArrayDataModel {
            id: searchModel
        },
        ComponentDefinition {
            id: chatUIDelegate
            source: "chatUI.qml"
        }
    ]
}