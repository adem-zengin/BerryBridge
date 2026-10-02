import bb.cascades 1.4

NavigationPane {
    id: settingsNavigation
    property variant datSet:dat
    property variant accs:[]
    
    // Sinyal Fonksiyonları:
    function onSettingsReadyHandler(list) {
        populateSettingsList(list);
        fetchAct.running = false;
        initButton.visible = true;
    }
    
    function onSyncProgressHandler(message, progress) {
        syncStatusLabel.text = message;
        syncProgressIndicator.value = progress;
        fetchAct.running = false;
        console.log("[QML] Sync progress: " + message);
    }
    
    function onSyncCompleteHandler() {
        syncStatusLabel.text = "SYNC COMPLETED";
        fetchAct.running = false;
        syncProgressIndicator.visible = false;
        initButton.visible = false;
        closeButton.visible = true;
    }
    
    function onConnectionStatusHandler(response) {
        fetchAct.running = false;
        syncStatusLabel.text = response;
    }

    function populateSettingsList(list) {
        if (!list) return;
        
        // Manually map the list to our 10 rows
        var rows = [row0, row1, row2, row3, row4, row5, row6, row7, row8, row9];
        var labs = [lab0, lab1, lab2, lab3, lab4, lab5, lab6, lab7, lab8, lab9];
        var togs = [tog0, tog1, tog2, tog3, tog4, tog5, tog6, tog7, tog8, tog9];
        var tempAccs = [];
        // Hide all rows first to reset the view
        for(var j=0; j<10; j++) if(rows[j]) rows[j].visible = false;
        
        // Show and fill only what C++ provides (up to 10 items)
        for (var i = 0; i < list.length && i < 10; i++) {
            if(labs[i]) labs[i].text = list[i].network;
            if(togs[i]) togs[i].checked = list[i].active;
            if(rows[i]) rows[i].visible = true;
            tempAccs[i]=list[i].account;
        }
        accs = tempAccs;
    }

    onCreationCompleted: {    
        // Immediate population from memory
        var list = datSet.getSettingsList();
        if (list && list.length > 0) {
            populateSettingsList(list);
        }
        
        datSet.settingsReady.connect(onSettingsReadyHandler);
        datSet.syncProgress.connect(onSyncProgressHandler);
        datSet.syncComplete.connect(onSyncCompleteHandler);
        datSet.connectionStatus.connect(onConnectionStatusHandler);
                
    }
    

    Page {
        id: settingsPage
        
        titleBar: TitleBar {
            title: "Settings"
            dismissAction: ActionItem {
                title: "Back"
                onTriggered: {
                    setupSheet.close()
                }
            }
            acceptAction: ActionItem {
                title: "Help"
                onTriggered: {
                    settingsNavigation.push(helpPage.createObject());
                }
            }
        }

    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        
        

        ScrollView {
            Container {
                topPadding: ui.du(3.0)
                leftPadding: ui.du(3.0)
                rightPadding: ui.du(3.0)
				
				//Horizontal filling
				//Divider {verticalAlignment: VerticalAlignment.Bottom
                    //opacity: 0.0}
				
                Container {

                    Label {
                        text: "Server URL"
                        textStyle.fontWeight: FontWeight.W500
                    }
                    TextField {
                        id: serverUrlField
                        onTextChanging: {
                            datSet.saveCredentials("serverUrl", text);
                        }
                    }
                    
                    Label {
                        text: "Access Token"
                        textStyle.fontWeight: FontWeight.W500
                    }
                    TextField {
                        id: accessTokenField
                        onTextChanging: {
                            datSet.saveCredentials("accessToken", text);
                        }
                    
                    }
                    
                    Button {
                        id: fetchAccountsButton
                        text: "List Accounts"
                        horizontalAlignment: HorizontalAlignment.Center
                        topMargin: ui.du(5.0)
                        onClicked: {
                            // Trigger fetch to populate the static labels
                            datSet.fetchAccounts();
                            syncStatusLabel.visible = true;
                            fetchAct.running=true
                        }
                    }
                    
                    onCreationCompleted: {
                        // C++ tarafındaki getSetting/getCredentials fonksiyonun varsa önce onu dene:
                        var savedUrl = datSet.getCredentials("serverUrl");
                        var savedToken = datSet.getCredentials("accessToken"); 
                        var defaultUrl = "Your URL";
                        var defaultToken = "Your Token"

                        serverUrlField.text = (savedUrl ? savedUrl : defaultUrl);
                        accessTokenField.text = (savedToken ? savedToken : defaultToken);
                        console.log("savedUrl:"+savedUrl)
                        if(!savedUrl){
                            console.log("defaultUrl:"+defaultUrl)
                            datSet.saveCredentials("serverUrl", defaultUrl);
                        }
                        if(!savedToken){
                            datSet.saveCredentials("accessToken", defaultToken);
                        }
                        
                    }
                }

                // ROW 0
                Container {
                    id: row0
                    visible: false
                    layout: DockLayout {}
                    horizontalAlignment: HorizontalAlignment.Fill
                    minHeight: 120.0 // Slightly taller for better touch targets
                    Label { 
                        id: lab0
                        verticalAlignment: VerticalAlignment.Center 
                        horizontalAlignment: HorizontalAlignment.Left
                    }
                    ToggleButton {
                        id: tog0
                        horizontalAlignment: HorizontalAlignment.Right
                        verticalAlignment: VerticalAlignment.Center
                        onCheckedChanged: { 
                            console.log("[SETTINGS-QML] tog0 toggled: " + lab0.text +"-"+accs[0]+ " = " + checked);
                            datSet.updateSetting(accs[0], checked); 
                        }
                    }
                    Divider { verticalAlignment: VerticalAlignment.Bottom }
                }
                
                // ROW 1
                Container {
                    id: row1
                    visible: false
                    layout: DockLayout {}
                    horizontalAlignment: HorizontalAlignment.Fill
                    minHeight: 120.0
                    Label { 
                        id: lab1
                        verticalAlignment: VerticalAlignment.Center 
                        horizontalAlignment: HorizontalAlignment.Left
                    }
                    ToggleButton {
                        id: tog1
                        horizontalAlignment: HorizontalAlignment.Right
                        verticalAlignment: VerticalAlignment.Center
                        onCheckedChanged: { 
                            console.log("[SETTINGS-QML] tog1 toggled: " + lab1.text + " = " + checked);
                            datSet.updateSetting(accs[1], checked); 
                        }
                    }
                    Divider { verticalAlignment: VerticalAlignment.Bottom }
                }
                
                // ROW 2
                Container {
                    id: row2
                    visible: false
                    layout: DockLayout {}
                    horizontalAlignment: HorizontalAlignment.Fill
                    minHeight: 120.0
                    Label { 
                        id: lab2
                        verticalAlignment: VerticalAlignment.Center 
                        horizontalAlignment: HorizontalAlignment.Left
                    }
                    ToggleButton {
                        id: tog2
                        horizontalAlignment: HorizontalAlignment.Right
                        verticalAlignment: VerticalAlignment.Center
                        onCheckedChanged: { datSet.updateSetting(accs[2], checked); }
                    }
                    Divider { verticalAlignment: VerticalAlignment.Bottom }
                }
                
                // ROW 3
                Container {
                    id: row3
                    visible: false
                    layout: DockLayout {}
                    horizontalAlignment: HorizontalAlignment.Fill
                    minHeight: 120.0
                    Label { 
                        id: lab3
                        verticalAlignment: VerticalAlignment.Center 
                        horizontalAlignment: HorizontalAlignment.Left
                    }
                    ToggleButton {
                        id: tog3
                        horizontalAlignment: HorizontalAlignment.Right
                        verticalAlignment: VerticalAlignment.Center
                        onCheckedChanged: { datSet.updateSetting(accs[3], checked); }
                    }
                    Divider { verticalAlignment: VerticalAlignment.Bottom }
                }
                
                // ROW 4
                Container {
                    id: row4
                    visible: false
                    layout: DockLayout {}
                    horizontalAlignment: HorizontalAlignment.Fill
                    minHeight: 120.0
                    Label { 
                        id: lab4
                        verticalAlignment: VerticalAlignment.Center 
                        horizontalAlignment: HorizontalAlignment.Left
                    }
                    ToggleButton {
                        id: tog4
                        horizontalAlignment: HorizontalAlignment.Right
                        verticalAlignment: VerticalAlignment.Center
                        onCheckedChanged: { datSet.updateSetting(accs[4], checked); }
                    }
                    Divider { verticalAlignment: VerticalAlignment.Bottom }
                }
                
                // ROW 5
                Container {
                    id: row5
                    visible: false
                    layout: DockLayout {}
                    horizontalAlignment: HorizontalAlignment.Fill
                    minHeight: 120.0
                    Label { 
                        id: lab5
                        verticalAlignment: VerticalAlignment.Center 
                        horizontalAlignment: HorizontalAlignment.Left
                    }
                    ToggleButton {
                        id: tog5
                        horizontalAlignment: HorizontalAlignment.Right
                        verticalAlignment: VerticalAlignment.Center
                        onCheckedChanged: { datSet.updateSetting(accs[5], checked); }
                    }
                    Divider { verticalAlignment: VerticalAlignment.Bottom }
                }
                
                // ROW 6
                Container {
                    id: row6
                    visible: false
                    layout: DockLayout {}
                    horizontalAlignment: HorizontalAlignment.Fill
                    minHeight: 120.0
                    Label { 
                        id: lab6
                        verticalAlignment: VerticalAlignment.Center 
                        horizontalAlignment: HorizontalAlignment.Left
                    }
                    ToggleButton {
                        id: tog6
                        horizontalAlignment: HorizontalAlignment.Right
                        verticalAlignment: VerticalAlignment.Center
                        onCheckedChanged: { datSet.updateSetting(accs[6], checked); }
                    }
                    Divider { verticalAlignment: VerticalAlignment.Bottom }
                }
                
                // ROW 7
                Container {
                    id: row7
                    visible: false
                    layout: DockLayout {}
                    horizontalAlignment: HorizontalAlignment.Fill
                    minHeight: 120.0
                    Label { 
                        id: lab7
                        verticalAlignment: VerticalAlignment.Center 
                        horizontalAlignment: HorizontalAlignment.Left
                    }
                    ToggleButton {
                        id: tog7
                        horizontalAlignment: HorizontalAlignment.Right
                        verticalAlignment: VerticalAlignment.Center
                        onCheckedChanged: { datSet.updateSetting(accs[7], checked); }
                    }
                    Divider { verticalAlignment: VerticalAlignment.Bottom }
                }
                
                // ROW 8
                Container {
                    id: row8
                    visible: false
                    layout: DockLayout {}
                    horizontalAlignment: HorizontalAlignment.Fill
                    minHeight: 120.0
                    Label { 
                        id: lab8
                        verticalAlignment: VerticalAlignment.Center 
                        horizontalAlignment: HorizontalAlignment.Left
                    }
                    ToggleButton {
                        id: tog8
                        horizontalAlignment: HorizontalAlignment.Right
                        verticalAlignment: VerticalAlignment.Center
                        onCheckedChanged: { datSet.updateSetting(accs[8], checked); }
                    }
                    Divider { verticalAlignment: VerticalAlignment.Bottom }
                }
                
                // ROW 9
                Container {
                    id: row9
                    visible: false
                    layout: DockLayout {}
                    horizontalAlignment: HorizontalAlignment.Fill
                    minHeight: 120.0
                    Label { 
                        id: lab9
                        verticalAlignment: VerticalAlignment.Center 
                        horizontalAlignment: HorizontalAlignment.Left
                    }
                    ToggleButton {
                        id: tog9
                        horizontalAlignment: HorizontalAlignment.Right
                        verticalAlignment: VerticalAlignment.Center
                        onCheckedChanged: { datSet.updateSetting(accs[9], checked); }
                    }
                    Divider { verticalAlignment: VerticalAlignment.Bottom }
                }
                
                Container {
                    horizontalAlignment: HorizontalAlignment.Fill
                    topPadding: ui.du(3.0)
                    bottomPadding: ui.du(6.0)
                    rightPadding: ui.du(3.0)
                    leftPadding: ui.du(3.0)
                    topMargin: ui.du(1.0)
                    layout: StackLayout {
                        orientation: LayoutOrientation.TopToBottom
                    }

                    ActivityIndicator {
                        id: fetchAct
                        running: false
                        minHeight: ui.du(8.0)
                        horizontalAlignment: HorizontalAlignment.Center      
                                          
                    }
                    Label {
                        id: syncStatusLabel
                        text: "Select accounts above and click Initialize"
                        textStyle.fontSize: FontSize.Small
                        textStyle.color: Color.create("#666666")
                        horizontalAlignment: HorizontalAlignment.Center
                        multiline: true
                        bottomMargin: 20.0
                        visible: false
                    }
                       
                    // YENİ: İlerleme Çubuğu
                    ProgressIndicator {
                      id: syncProgressIndicator
                      visible: false
                      fromValue: 0
                      toValue: 100.0
                      horizontalAlignment: HorizontalAlignment.Fill
                      topMargin: ui.du(1.0)
                      bottomMargin: ui.du(2.0)
                    
                    
                    }
                    
                    Button {
                      id: initButton
                      text: "Initialize & Download Messages"
                      horizontalAlignment: HorizontalAlignment.Center
                      visible: false
                      onClicked: {
                          console.log("[QML] Initialize button clicked");
                          syncStatusLabel.text = "Starting initialization...";
                          syncProgressIndicator.visible = true;
                          syncProgressIndicator.value = 0; // Sıfırla
                          initButton.enabled = false; // Tıklanabilirliği kapat, butonu tamamen yok etme ki süreç izlensin
                          datSet.initializeDatabaseSync();
                      }
                      topMargin: ui.sdu(5.0)
                    }
                    
                    Button {
                      id: closeButton
                      text: "Close & Continue"
                      visible: false
                      horizontalAlignment: HorizontalAlignment.Center
                      onClicked: {
                          console.log("[QML] Closing settings sheet");
                          setupSheet.close();
                          datSet.setInitRun(true);
                      }
                      topMargin: ui.sdu(5.0)
                    }
                }
            }
            
            
        }
        
        }
    }
    attachedObjects: [
        ComponentDefinition {
            id: helpPage
            source: "help.qml"
        }
    ]
}