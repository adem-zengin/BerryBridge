import bb.cascades 1.4
import bb 1.0

Page {
    id: aboutP
    actionBarVisibility: ChromeVisibility.Visible
    titleBar: TitleBar {
        title: "About"       
    }
    
    Container {
    horizontalAlignment: HorizontalAlignment.Fill
    verticalAlignment: VerticalAlignment.Fill
 
        ScrollView {
            horizontalAlignment: HorizontalAlignment.Center
            Container {
                topPadding: ui.sdu(4.0)
                leftPadding: ui.sdu(3.0)
                rightPadding: ui.sdu(3.0)

                Container {                  
                    ImageView {
                        imageSource: "asset:///images/icon.png"
                        horizontalAlignment: HorizontalAlignment.Center
                        preferredHeight: ui.sdu(15.0)
                        preferredWidth: ui.sdu(15.0)
                    }
                    Label {
                        text: "Berry Beeper "+"v"+appInfo.version
                        horizontalAlignment: HorizontalAlignment.Center
                    }
                    
                    Label {
                        id: latestLabel
                        text: "Checking for updates"
                        horizontalAlignment: HorizontalAlignment.Center
                        textStyle.color: undefined
                        multiline: true
                        textStyle.fontWeight: FontWeight.W500
                    }
                    Label {
                        id: whatIsNew
                        visible: false
                        text: "Checking for updates"
                        horizontalAlignment: HorizontalAlignment.Center
                        textStyle.color: undefined
                        multiline: true
                        // HTML biçimlendirmesini aktif et
                        textFormat: TextFormat.Html
                    }
                    ActivityIndicator {
                        id: versionAct
                        running: true
                        minHeight: ui.du(8.0)     
                        horizontalAlignment: HorizontalAlignment.Center
                        topMargin: ui.sdu(3.0)
                    }
                    ImageView {
                        id: updateImg
                        visible: false
                        imageSource: "asset:///images/ic_done.png"
                        horizontalAlignment: HorizontalAlignment.Center
                        filterColor: Color.DarkGreen
                        topMargin: ui.sdu(3.0)
                    }
                    Divider {
                        topMargin: ui.sdu(3.0)
                    }
                    Label {
                        text: "Project Page"
                        textStyle.fontWeight: FontWeight.W500
                        bottomMargin: 0
                    }
                   
                    
                    Label {
                        text: "<a href='https://github.com/adem-zengin/BerryBridge'>https://github.com/adem-zengin/BerryBridge</a>"
                        textFormat: TextFormat.Html
                        multiline: true
                        topMargin: 0
                    }
                    Divider {
                        topMargin: ui.sdu(3.0)
                    }
                    Label {
                        text: "Support Developer"
                        textStyle.fontWeight: FontWeight.W500
                        bottomMargin: 0
                    }
                    
                    
                    Label {
                        text: "<a href='https://www.patreon.com/16129770/join'>https://www.patreon.com/16129770/join</a>"
                        textFormat: TextFormat.Html
                        multiline: true
                        topMargin: 0
                    }
                }
        
            }
        }
    
    }

    actions: [
        /*InvokeActionItem {
            title: qsTr("Share") + Retranslate.onLanguageChanged
            ActionBar.placement: ActionBarPlacement.OnBar
            //query.uri: "appworld://content/59962452"
            query.invokeActionId: "bb.action.SHARE"
            query.mimeType: "text/plain"
            query.data: "Download uNote from \n http://appworld.blackberry.com/webstore/content/59962452/"
        
        },*/
        InvokeActionItem {
            ActionBar.placement: ActionBarPlacement.Signature
            title: qsTr("Report Bugs") + Retranslate.onLanguageChanged
            query {
                invokeTargetId: "sys.pim.uib.email.hybridcomposer"
                invokeActionId: "bb.action.SENDEMAIL"
                uri: "mailto:zead29@gmail.com?"
            }
        }
    
    ]
    attachedObjects: [
        ApplicationInfo{
            id: appInfo
        
        }
    ]
    
    function checkUpdate(updateRequired,latestVersion, releaseNotes){
        console.log("latestVersion:"+latestVersion);
        versionAct.running=false;
        if (updateRequired){
            updateImg.visible=false;
            latestLabel.text="A newer version is available.";
            latestLabel.textStyle.color= Color.Red;
            whatIsNew.visible=true;
            whatIsNew.text=releaseNotes;
        }else{
            updateImg.visible=true;
            whatIsNew.visible=false
            latestLabel.text="You have the latest version.";
            latestLabel.textStyle.color= undefined;
        }

    }
    
    function updateFailed(errorMessage){
        versionAct.running=false;
        updateImg.visible=false;
        latestLabel.text="Checking for update: "+errorMessage;
        latestLabel.textStyle.color= Color.Red;
    }
    
    onCreationCompleted: {
        app.updateCheckCompleted.connect(aboutP.checkUpdate);
        app.updateCheckFailed.connect(aboutP.updateFailed);
        app.checkForUpdates();
    }
}
