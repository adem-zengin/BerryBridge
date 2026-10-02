import bb.cascades 1.4
import my.location 1.0
import bb.device 1.4

Page {
    id: mapPage
    property string accountID
    property string chatID
    property double currentLat: 0.0
    property double currentLng: 0.0
    property bool hasLocation: false
    property bool isWebLoaded: false
    actionBarVisibility: ChromeVisibility.Hidden
    
    function updateMapIfReady() {
        if (hasLocation && isWebLoaded) {
            //debugText.text = "Harita çiziliyor...";
            console.log("Harita çiziliyor...");
            var jsCode = "setLocation(" + currentLat + ", " + currentLng + ");";
            webView.evaluateJavaScript(jsCode);
            
            loadingIndicator.stop();
            loadingContainer.visible = false;
        }
    }
    
    titleBar: TitleBar {
        title: "Location"
        
        // 1. Sol taraftaki Eylem (Cancel)
        dismissAction: ActionItem {
            title: "Cancel"
            onTriggered: {
                // İptal butonuna basıldığında yapılacak işlemler (örn: sayfayı kapatma)
                console.log("Cancel tıklandı");
                navigationPane.pop(); // Eğer bir NavigationPane kullanıyorsanız
            }
        }
        
        // 2. Sağ taraftaki Eylem (Send)
        acceptAction: ActionItem {
            title: "Send"
            onTriggered: {
                // Gönder butonuna basıldığında koordinatları kullanma
                console.log("Send tıklandı, Koordinatlar: " + currentLat + ", " + currentLng);
                loadingIndicator.start();
                loadingContainer.visible = true;
                dat.sendMessage(mapPage.accountID, mapPage.chatID, "pendingLocationMsgID", "https://maps.google.com/?q="+currentLat+","+currentLng);
                navigationPane.pop();
                navigationPane.pop(attachmentPage);
            }
        }
    }
    
    attachedObjects: [
        LocationSession {
            id: locationSession
            
            // Yeni sinyal ismi (onLocationFound) ve parametreler (lat, lng)
            onLocationFound: {
                //debugText.text = "GPS Alındı: " + lat.toFixed(4) + ", " + lng.toFixed(4);
                console.log("GPS Alındı: " + lat.toFixed(4) + ", " + lng.toFixed(4))
                mapPage.currentLat = lat;
                mapPage.currentLng = lng;
                mapPage.hasLocation = true;
                
                mapPage.updateMapIfReady();
            }
        },
        DisplayInfo {
            id: displayInfo
        }
    ]
    
    onCreationCompleted: {
        loadingIndicator.start();
        locationSession.startUpdates();
        //debugText.text = "Bağlantılar bekleniyor...";
        console.log("Bağlantılar bekleniyor...")
    }
    
    Container {
        layout: DockLayout {}
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        
        WebView {
            id: webView
            url: "local:///assets/html/map.html"
            preferredWidth: displayInfo.pixelSize.width
            preferredHeight: displayInfo.pixelSize.height
            
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            
            onLoadingChanged: {
                if (loadRequest.status == WebLoadStatus.Started) {
                    //debugText.text = "HTML Yükleniyor...";
                    console.log("HTML Yükleniyor...")
                } else if (loadRequest.status == WebLoadStatus.Succeeded) {
                    //debugText.text = "HTML Yüklendi, GPS bekleniyor...";
                    console.log("HTML Yüklendi, GPS bekleniyor...")
                    mapPage.isWebLoaded = true;
                    mapPage.updateMapIfReady();
                } else if (loadRequest.status == WebLoadStatus.Failed) {
                    //debugText.text = "HATA: HTML dosyası bulunamadı!";
                    console.log("HATA: HTML dosyası bulunamadı!")
                }
            }
        }
        
        // Yükleme ve Debug Ekranı
        Container {
            id: loadingContainer
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            background: Color.White
            
            layout: DockLayout {}
            
            Container {
                horizontalAlignment: HorizontalAlignment.Center
                verticalAlignment: VerticalAlignment.Center
                
                layout: StackLayout {
                    orientation: LayoutOrientation.TopToBottom
                }
                
                ActivityIndicator {
                    id: loadingIndicator
                    preferredWidth: ui.du(15.0)
                    preferredHeight: ui.du(15.0)
                    horizontalAlignment: HorizontalAlignment.Center
                }
                
                Label {
                    text: "Fetching location..."
                    topMargin: ui.du(2.0)
                    horizontalAlignment: HorizontalAlignment.Center
                }
                
                // Ekranda durumu gösterecek debug metni
                /*Label {
                    id: debugText
                    text: ""
                    topMargin: ui.du(2.0)
                    horizontalAlignment: HorizontalAlignment.Center
                    textStyle.color: Color.Red
                    textStyle.fontWeight: FontWeight.Bold
                }*/
            }
        }
    }
}