import bb.cascades 1.4
import bb.system 1.2

Page {
    id: settingsPage
    property TabbedPane mainRef
    
    titleBar: TitleBar {
        title: "Settings"
        scrollBehavior: TitleBarScrollBehavior.Sticky
    }
    
    ScrollView {
        content: Container {
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }
            leftPadding: ui.du(3.0)
            rightPadding: ui.du(3.0)
            topPadding: ui.du(3.0)
            bottomPadding: ui.du(3.0)
            
            
            
            // --- 2. DARK MODE SETTING ---
            Container {
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }
                horizontalAlignment: HorizontalAlignment.Fill
                
                Container {
                    layoutProperties: StackLayoutProperties {
                        spaceQuota: 1.0
                    }
                    verticalAlignment: VerticalAlignment.Center
                    
                    Label {
                        text: "Dark Mode"
                        textStyle.fontWeight: FontWeight.W600
                        textStyle.fontSize: FontSize.Medium
                    }
                    Label {
                        text: "Enables dark theme across the application."
                        textStyle.fontSize: FontSize.XSmall
                        textStyle.color: Color.DarkGray
                        multiline: true
                    }
                }
                
                ToggleButton {
                    id: darkModeToggle
                    verticalAlignment: VerticalAlignment.Center
                    checked: app.darkMode
                    onCheckedChanged: {
                        if (app.darkMode !== checked) {
                            app.darkMode = checked;
                            //restartToast.show();
                            restartDialog.show();
                            }                       
                    }
                }      
                
            }
            
            Divider {
                
            }
            
            // --- 1. TRACKPAD FOCUS OVERLAY SETTING ---
            Container {
                visible: app.isClassic
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }
                horizontalAlignment: HorizontalAlignment.Fill
                bottomMargin: ui.du(4.0)
                
                Container {
                    layoutProperties: StackLayoutProperties {
                        spaceQuota: 1.0
                    }
                    verticalAlignment: VerticalAlignment.Center
                    
                    Label {
                        text: "Disable Trackpad Focus Overlay"
                        textStyle.fontWeight: FontWeight.W600
                        textStyle.fontSize: FontSize.Medium
                        multiline: true
                    }
                    Label {
                        text: "Disables the system highlight overlay when navigating messages on BlackBerry Classic devices."
                        textStyle.fontSize: FontSize.XSmall
                        textStyle.color: Color.DarkGray
                        multiline: true
                    }
                }
                
                ToggleButton {
                    verticalAlignment: VerticalAlignment.Center
                    checked: app.disableTrackpadFocus
                    onCheckedChanged: {
                        app.disableTrackpadFocus = checked;
                    }
                }
            }
            
        }
    }
    
    attachedObjects: [
        SystemToast {
            id: setToast
            body: ""
        },
        SystemDialog {
            id: restartDialog
            title: "Restart Required!"
            body: "Close and reopen the app to apply changes."
            
            confirmButton.label: "Close Now"
            cancelButton.label: "Cancel"
            
            onFinished: {
                if (result == SystemUiResult.ConfirmButtonSelection) {
                    // "Close Now" seçildiyse uygulamayı güvenli şekilde kapat
                    app.exitApp(); 
                } else {
                    // "Cancel" seçildiyse veya dialog dışına basıldıysa toggle'ı eski haline geri al
                    app.darkMode = !app.darkMode; // Tercihi geri çeviriyoruz
                    darkModeToggle.checked = app.darkMode; // Toggle butonunu görsel olarak güncelliyoruz
                }
            }
        }
    ]
    
    
}