import bb.cascades 1.4

Page {
    id: genericSettingsPage
    property string accountID
    property string accountName
    property TabbedPane mainRef
    
    // Sekmeden gelen mevcut renkler
    property string tPrimaryColor: ""
    property string tChatBgColor: ""
    property string tBubbleColor: ""
    property string tUnreadBadgeColor: ""
    
    function savePreference(key, value) {
        dat.updateSetting("state_pref_" + key + "_" + accountID, value);
    }
    
    function saveThemePref(key, value) {
        app.updateSetting("theme_" + key + "_" + accountID, value);
        mainRef.updateAccountTabs();
    }
    
    titleBar: TitleBar {
        title: accountName + " Settings"
    }
    
    ScrollView {
        Container {
            layout: StackLayout {}
            topPadding: 40.0; leftPadding: 40.0; rightPadding: 40.0; bottomPadding: 60.0;
            
            // --- TEMA AYARLARI ---
            Label {
                text: "Theme Customization"
                textStyle.fontWeight: FontWeight.Bold
                textStyle.fontSize: FontSize.Medium
            }
            Divider {}
            
            Container {
                bottomPadding: 20.0
                layout: StackLayout {}
                
                // Primary Color
                Label { text: "Primary Color (HEX)" }
                Container {
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    TextField {
                        id: primaryInput
                        text: tPrimaryColor
                        layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
                    }
                    Container {
                        preferredWidth: 80; preferredHeight: 80
                        background: Color.create(primaryInput.text.length > 3 ? primaryInput.text : "#000000")
                        leftMargin: 20.0
                    }
                }
                
                // Chat Background Color
                Label { text: "Chat Background Color (HEX)"; topMargin: 20.0 }
                Container {
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    TextField {
                        id: bgInput
                        text: tChatBgColor
                        layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
                    }
                    Container {
                        preferredWidth: 80; preferredHeight: 80
                        background: Color.create(bgInput.text.length > 3 ? bgInput.text : "#000000")
                        leftMargin: 20.0
                    }
                }
                
                // Bubble Color
                Label { text: "Sender Bubble Color (HEX)"; topMargin: 20.0 }
                Container {
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    TextField {
                        id: bubbleInput
                        text: tBubbleColor
                        layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
                    }
                    Container {
                        preferredWidth: 80; preferredHeight: 80
                        background: Color.create(bubbleInput.text.length > 3 ? bubbleInput.text : "#000000")
                        leftMargin: 20.0
                    }
                }
                
                // Badge Color
                Label { text: "Unread Badge Color (HEX)"; topMargin: 20.0 }
                Container {
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    TextField {
                        id: badgeInput
                        text: tUnreadBadgeColor
                        layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
                    }
                    Container {
                        preferredWidth: 80; preferredHeight: 80
                        background: Color.create(badgeInput.text.length > 3 ? badgeInput.text : "#000000")
                        leftMargin: 20.0
                    }
                }
                
                Button {
                    text: "Save Theme"
                    horizontalAlignment: HorizontalAlignment.Center
                    topMargin: 30.0
                    onClicked: {
                        saveThemePref("primary", primaryInput.text);
                        saveThemePref("bg", bgInput.text);
                        saveThemePref("bubble", bubbleInput.text);
                        saveThemePref("badge", badgeInput.text);
                        console.log("[AccountSettings] Theme saved for " + accountID);
                    }
                }
            }
            
            // --- BİLDİRİM AYARLARI ---
            Label {
                text: "Notifications"
                textStyle.fontWeight: FontWeight.Bold
                textStyle.fontSize: FontSize.Medium
                topMargin: 40.0
            }
            
            Divider {}
            
            // ROW: Personal Chats
            Container {
                layout: DockLayout {}
                horizontalAlignment: HorizontalAlignment.Fill
                minHeight: 120.0
                Label {
                    text: "Personal Chats"
                    verticalAlignment: VerticalAlignment.Center
                    textStyle.fontSize: FontSize.Medium
                }
                ToggleButton {
                    horizontalAlignment: HorizontalAlignment.Right
                    verticalAlignment: VerticalAlignment.Center
                    checked: dat.getSetting("state_pref_personal_" + accountID, true)
                    onCheckedChanged: { 
                        savePreference("personal", checked);
                    }
                }
            }
            
            Divider {}
            
            // ROW: Group Chats
            Container {
                layout: DockLayout {}
                horizontalAlignment: HorizontalAlignment.Fill
                minHeight: 120.0
                Label {
                    text: "Groups & Communities"
                    verticalAlignment: VerticalAlignment.Center
                    textStyle.fontSize: FontSize.Medium
                }
                ToggleButton {
                    horizontalAlignment: HorizontalAlignment.Right
                    verticalAlignment: VerticalAlignment.Center
                    checked: dat.getSetting("state_pref_groups_" + accountID, true)
                    onCheckedChanged: { 
                        savePreference("groups", checked);
                        console.log("[WhatsAppSettings] Groups notified: " + checked); 
                    }
                }
            }
            Divider {}
            
            // ROW: Status Updates
            Container {
                layout: DockLayout {}
                horizontalAlignment: HorizontalAlignment.Fill
                minHeight: 120.0
                Label {
                    text: "Status Updates"
                    verticalAlignment: VerticalAlignment.Center
                    textStyle.fontSize: FontSize.Medium
                }
                ToggleButton {
                    horizontalAlignment: HorizontalAlignment.Right
                    verticalAlignment: VerticalAlignment.Center
                    checked: dat.getSetting("state_pref_status_" + accountID, false)
                    onCheckedChanged: {
                        savePreference("status", checked);
                        console.log("[WhatsAppSettings] Status notified: " + checked); 
                    }
                }
            }
            Divider {}
            
            // ROW: Channels
            Container {
                layout: DockLayout {}
                horizontalAlignment: HorizontalAlignment.Fill
                minHeight: 120.0
                Label {
                    text: "Channels"
                    verticalAlignment: VerticalAlignment.Center
                    textStyle.fontSize: FontSize.Medium
                }
                ToggleButton {
                    horizontalAlignment: HorizontalAlignment.Right
                    verticalAlignment: VerticalAlignment.Center
                    checked: dat.getSetting("state_pref_channels_" + accountID, false)
                    onCheckedChanged: { 
                        savePreference("channels", checked);
                        console.log("[WhatsAppSettings] Channels notified: " + checked); 
                    }
                }
            }
            
            Divider { bottomMargin: 40.0 }
        
        }
    }
}