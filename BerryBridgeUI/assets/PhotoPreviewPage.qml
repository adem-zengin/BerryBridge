import bb.cascades 1.4

Page {
    id: photoPreviewPage
    property string fullFilePath
    property string fileSize
    property string fileType
    property variant size 
    property Page chatPageRef
    
    actionBarVisibility: ChromeVisibility.Hidden
    
    titleBar: TitleBar {
        title: "Preview"
        
        // Sol taraftaki Back butonu
        dismissAction: ActionItem {
            title: "Back"
            onTriggered: {
                navigationPane.pop();
            }
        }
        
        
        // Sağ taraftaki Send butonu
        acceptAction: ActionItem {
            title: "Select"
            onTriggered: {
                chatPageRef.attachUrl=fullFilePath;
                chatPageRef.attachSending=true;
                chatPageRef.attachExtension=fullFilePath.substring(fullFilePath.lastIndexOf(".") + 1).toUpperCase();
                chatPageRef.attachFileName=fullFilePath.split('/').pop();
                chatPageRef.attachFileSizeStr=fileSize;
                chatPageRef.attachFileType=fileType;
                chatPageRef.attachImageSize=size;
                chatPageRef.sendButtonUrl = "asset:///images/ic_play.png";

                navigationPane.pop();
                navigationPane.pop(attachmentPage);
            }
        }
    }
    
    // Görüntünün üstüne text alanı bindirebilmek için DockLayout kullanılıyor
    Container {
        layout: DockLayout {}
        verticalAlignment: VerticalAlignment.Fill
        horizontalAlignment: HorizontalAlignment.Fill
        background: Color.Black
        
        // Arka plandaki tam boyutlu resim        
        
        Container {
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
            ImageView {
                id: filePreview
                imageSource: fullFilePath
                horizontalAlignment: HorizontalAlignment.Center
                verticalAlignment: VerticalAlignment.Center
                scalingMethod: ScalingMethod.AspectFit
            }

        }

    }

}