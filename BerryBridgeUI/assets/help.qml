import bb.cascades 1.4

Page {
    titleBar: TitleBar {
        title: "Help"
    }
    Container {
    horizontalAlignment: HorizontalAlignment.Fill
    verticalAlignment: VerticalAlignment.Fill

        ScrollView {
            Container {
            topPadding: ui.du(3.0)
            leftPadding: ui.du(3.0)
            rightPadding: ui.du(3.0)
            Label {
                text: "Berry Beeper is a native Beeper client that syncs your accounts via the Beeper Desktop API. To use this application, you must first set up the Beeper Desktop Application on a PC or server. For detailed guides and setup instructions, visit the <a href=\"https://github.com/adem-zengin/BerryBeeper\">https://github.com/adem-zengin/BerryBeeper</a>"
                multiline: true
                textStyle.textAlign: TextAlign.Justify
                textFormat: TextFormat.Html
            }
            }
        }
    }
}
