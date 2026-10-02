import bb.cascades 1.4
import com.database.web 1.0

Page {
    id: emojiPage
    property string chatID
    property string msgID
    property variant nPane
    property string reactionKey
    actionBarVisibility: ChromeVisibility.Hidden
    
    titleBar: TitleBar {
        title: "Reactions"
        
        // 1. Sol taraftaki Eylem (Cancel)
        dismissAction: ActionItem {
            title: "Cancel"
            onTriggered: {
                // İptal butonuna basıldığında yapılacak işlemler
                console.log("Cancel tıklandı");
                nPane.pop(); 
            }
        }
        
        // 2. Sağ taraftaki Eylem (Send)
        acceptAction: ActionItem {
            title: "Send"
            onTriggered: {
                dat.sendReaction(chatID, msgID, reactionKey);
            }
        }
    }
    
    Container {
        id: emojiPickerContainer
        visible: chatPage.isEmojiPickerVisible 
        horizontalAlignment: HorizontalAlignment.Fill
        // Native klavyenin yaklaşık kapladığı alan boyutunda sabit bir yükseklik
        background: Color.create("#E5DDD5")
        
        layout: DockLayout {}
        
        ListView {
            id: emojiListView
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
            
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
                // YENİ EKLENEN KISIM: Önceki seçimi temizle ve tıklananı seç
                clearSelection();
                select(indexPath);
                
                reactionKey = dataModel.data(indexPath);
            }
        }
    }
    
    onCreationCompleted: {
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
    
    attachedObjects: [
        Database {
            id: dat
            onReactionSentSuccessfully: {
                nPane.pop();
            }
        }
    ]
}