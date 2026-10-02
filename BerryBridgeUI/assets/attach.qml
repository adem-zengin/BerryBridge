import bb.cascades 1.4
import bb.cascades.pickers 1.0 // FilePicker bileşenini kullanabilmek için ekledik

Page {
    id: attachmentPage
    property string accountID
    property string chatID
    property string selItem
    property Page chatPageRef
    
    titleBar: TitleBar {
        title: "Attach"
    }
    
    Container {
        layout: StackLayout {}
        verticalAlignment: VerticalAlignment.Fill
        horizontalAlignment: HorizontalAlignment.Fill
        
        ListView {
            id: attachmentListView
            
            dataModel: ArrayDataModel {
                id: attachmentModel
            }
            
            onCreationCompleted: {                
                attachmentModel.append({ "title": "Picture",  "icon": "asset:///images/ic_doctype_picture.png",  "type": "IMAGE", "color":Color.DarkGreen });
                attachmentModel.append({ "title": "Video",   "icon": "asset:///images/ic_doctype_video.png",   "type": "VIDEO", "color":Color.Red });
                attachmentModel.append({ "title": "Location", "icon": "asset:///images/ic_location.png", "type": "LOCATION", "color":Color.DarkRed});
                attachmentModel.append({ "title": "Audio", "icon": "asset:///images/ic_doctype_music.png", "type": "AUDIO", "color":Color.DarkBlue });
                attachmentModel.append({ "title": "Document",  "icon": "asset:///images/ic_doctype_doc.png",  "type": "DOCUMENT", "color":Color.DarkMagenta });
                attachmentModel.append({ "title": "File",  "icon": "asset:///images/ic_doctype_generic.png",  "type": "FILE", "color":Color.DarkGray });
            }
            
            listItemComponents: [
                ListItemComponent {
                    type: ""
                    
                    CustomListItem {
                        dividerVisible: true
                        highlightAppearance: HighlightAppearance.Frame
                        
                        Container {
                            layout: StackLayout {
                                orientation: LayoutOrientation.LeftToRight
                            }
                            verticalAlignment: VerticalAlignment.Center
                            leftPadding: ui.du(2.0)
                            rightPadding: ui.du(2.0)
                            topPadding: ui.du(1.5)
                            bottomPadding: ui.du(1.5)
                            
                            Container {
                                //Color.create("#262626") 
                                background:ListItemData.color
                                preferredWidth: ui.du(12.0)
                                preferredHeight: ui.du(12.0)
                                verticalAlignment: VerticalAlignment.Center
                                
                                layout: DockLayout {}
                                
                                ImageView {
                                    imageSource: ListItemData.icon
                                    horizontalAlignment: HorizontalAlignment.Center
                                    verticalAlignment: VerticalAlignment.Center
                                    preferredWidth: ui.du(9.0)
                                    preferredHeight: ui.du(9.0)
                                }
                            }
                            
                            Label {
                                text: ListItemData.title
                                verticalAlignment: VerticalAlignment.Center
                                leftMargin: ui.du(2.0)
                                textStyle.base: SystemDefaults.TextStyles.TitleText
                            }
                        }
                    }
                }
            ]
            
            onTriggered: {
                var selectedItem = dataModel.data(indexPath);
                selItem=selectedItem.type;
                if (selectedItem.type === "LOCATION") {
                var mapPage = mapPageDefinition.createObject();
                mapPage.accountID = attachmentPage.accountID;
                mapPage.chatID = attachmentPage.chatID;
                navigationPane.push(mapPage);
                }
                else if (selItem === "IMAGE") {
                    galleryPicker.type=FileType.Picture
                    galleryPicker.open();
                }
                else if (selItem === "VIDEO") {
                    galleryPicker.type=FileType.Video
                    galleryPicker.open();
                }
                else if (selItem === "AUDIO") {
                    galleryPicker.type=FileType.Music
                    galleryPicker.open();
                }
                else if (selItem === "DOCUMENT") {
                    galleryPicker.type=FileType.Document
                    galleryPicker.open();
                }    
                else {
                    galleryPicker.type=FileType.Other
                    galleryPicker.open();
                }
                
            }
        }
    }
    
    attachedObjects: [
        ComponentDefinition {
            id: mapPageDefinition
            source: "MapPage.qml"
        },
        ComponentDefinition {
            id: previewPageDefinition
            source: "PhotoPreviewPage.qml"
        },
        
        // Sadece grid fotoğrafları gösterecek olan Native FilePicker
        FilePicker {
            id: galleryPicker
            
            title: "Select File"
            mode: FilePickerMode.Picker // Seçim modu
            type: FileType.Picture // Sadece resimleri filtreler
            viewMode: FilePickerViewMode.GridView // Sadece grid view şeklinde gösterir
            
            onFileSelected: {
                // Seçilen dosyanın dizinini alıyoruz[cite: 4]
                var selectedFilePath = selectedFiles[0];
                var formattedSize = app.getFileSizeFormatted(selectedFilePath);
                var fullFilePath = "file://" + selectedFilePath;         
                // Önizleme sayfamızı oluşturuyoruz
                if (selItem=="IMAGE"){
                    var previewPage = previewPageDefinition.createObject();
                    previewPage.chatPageRef = attachmentPage.chatPageRef;
                    previewPage.fullFilePath = fullFilePath;
                    previewPage.fileSize = formattedSize;
                    previewPage.size = dat.getImageDimensions(selectedFilePath);
                    previewPage.fileType=selItem,                  
                    navigationPane.push(previewPage);
                }else{
                    if(selItem=="DOCUMENT") selItem="FILE";
                    chatPageRef.attachUrl=fullFilePath;
                    chatPageRef.attachSending=true;
                    chatPageRef.attachExtension=fullFilePath.substring(fullFilePath.lastIndexOf(".") + 1).toUpperCase();
                    chatPageRef.attachFileName=fullFilePath.split('/').pop();
                    chatPageRef.attachFileSizeStr=formattedSize;
                    chatPageRef.attachFileType=selItem,
                    chatPageRef.sendButtonUrl = "asset:///images/ic_play.png";
                    navigationPane.pop();
                }

            }
        }
    ]
}