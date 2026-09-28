import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Config"
import "../Controls2/TextTypes"
import "../Components"

PageType {
    id: root

    Connections {
        target: UpdateController

        function onUpdateNotFound() {
            PageController.showNotificationMessage(qsTr("You have the latest version of CoCo VPN"))
        }

        function onUpdateCheckFailed() {
            PageController.showNotificationMessage(qsTr("Failed to check for updates"))
        }
    }

    BackButtonType {
        id: backButton

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin

        onActiveFocusChanged: {
            if(backButton.enabled && backButton.activeFocus) {
                listView.positionViewAtBeginning()
            }
        }
    }

    ListViewType {
        id: listView

        anchors.top: backButton.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.left: parent.left

        header: ColumnLayout {
            width: listView.width

            Image {
                id: image
                source: "qrc:/images/cocoVpnLogo.png"
                fillMode: Image.PreserveAspectFit

                Layout.alignment: Qt.AlignCenter
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.preferredWidth: 291
                Layout.preferredHeight: 104
            }

            Header2TextType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: qsTr("About CoCo VPN")
                horizontalAlignment: Text.AlignHCenter
            }

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                horizontalAlignment: Text.AlignHCenter

                font.pixelSize: 14

                text: qsTr("CoCo VPN is based on the open-source AmneziaVPN project.")
                color: AmneziaStyle.color.paleGray
            }

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: qsTr("Source code")
            }
        }

        model: contacts

        delegate: ColumnLayout {
            width: listView.width

            LabelWithButtonType {
                Layout.fillWidth: true
                Layout.topMargin: 6

                text: title
                descriptionText: description
                leftImageSource: imageSource

                clickedFunction: handler
            }

            DividerType {}

        }

        footer: ColumnLayout {
            width: listView.width

            CaptionTextType {
                Layout.fillWidth: true
                Layout.topMargin: 40

                horizontalAlignment: Text.AlignHCenter

                text: qsTr("Software version: %1").arg(SettingsController.getAppVersion())
                color: AmneziaStyle.color.mutedGray

                MouseArea {
                    property int clickCount: 0
                    anchors.fill: parent
                    onClicked: {
                        if (clickCount > 10) {
                            SettingsController.enableDevMode()
                        } else {
                            clickCount++
                        }
                    }
                }
            }

            BasicButtonType {
                id: checkUpdatesButton
                visible: false // No CoCo VPN update feed is configured yet.

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                Layout.bottomMargin: 16
                implicitHeight: 48

                defaultColor: AmneziaStyle.color.surfaceBase
                hoveredColor: AmneziaStyle.color.surfaceHovered
                pressedColor: AmneziaStyle.color.surfacePressed
                disabledColor: AmneziaStyle.color.surfaceBase
                textColor: AmneziaStyle.color.surfaceInverse
                borderWidth: 1
                borderColor: AmneziaStyle.color.borderSoft

                enabled: !UpdateController.isCheckRunning

                text: UpdateController.isCheckRunning ? qsTr("Checking...") : qsTr("Check for updates")

                clickedFunc: function() {
                    UpdateController.checkForUpdates()
                }
            }

            BasicButtonType {
                id: privacyPolicyButton
                visible: false // The upstream privacy policy does not describe this fork.

                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 16
                Layout.topMargin: -15
                implicitHeight: 25

                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                disabledColor: AmneziaStyle.color.mutedGray
                textColor: AmneziaStyle.color.goldenApricot

                text: qsTr("Privacy Policy")

                clickedFunc: function() {
                    Qt.openUrlExternally(LanguageUiController.getCurrentSiteUrl("policy"))
                }
            }
        }
    }
    
    property list<QtObject> contacts: [ github, upstream ]

    QtObject {
        id: github

        readonly property string title: qsTr("GitHub")
        readonly property string description: qsTr("CoCo VPN source code")
        readonly property string imageSource: "qrc:/images/controls/github.svg"
        readonly property var handler: function() {
            Qt.openUrlExternally("https://github.com/Iranux-Master/CoCo-VPN")
        }
    }

    QtObject {
        id: upstream

        readonly property string title: qsTr("Original project")
        readonly property string description: qsTr("AmneziaVPN open-source project")
        readonly property string imageSource: "qrc:/images/controls/github.svg"
        readonly property var handler: function() {
            Qt.openUrlExternally("https://github.com/amnezia-vpn/amnezia-client")
        }
    }

}
