import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"

// CoCo: buy a server from webpack.ir. Signs in, lists plans in the wallet currency, pays from the
// wallet or opens the gateway in the browser for the shortfall, and follows the build.
PageType {
    id: root

    property int planIndex: 0
    property int regionIndex: 0
    property int systemIndex: 0
    property int gatewayIndex: 0

    readonly property var plan: WebpackController.plans.length > planIndex ? WebpackController.plans[planIndex] : null
    readonly property var region: plan && plan.regions.length > regionIndex ? plan.regions[regionIndex] : null
    readonly property var system: plan && plan.systems.length > systemIndex ? plan.systems[systemIndex] : null

    function gatewayOptions() {
        var result = []
        for (var i = 0; i < WebpackController.gateways.length; ++i)
            result.push({"name": WebpackController.gateways[i]})
        return result
    }

    Component.onCompleted: {
        if (WebpackController.signedIn)
            WebpackController.loadShop()
    }

    Connections {
        target: WebpackController
        enabled: root.visible
        function onErrorOccurred(message) { PageController.showErrorMessage(message) }
        function onNotificationShown(message) { PageController.showNotificationMessage(message) }
        function onSignInFinished(ok) { if (ok) WebpackController.loadShop() }
        function onBusyChanged() { PageController.showBusyIndicator(WebpackController.busy) }
        function onPlansChanged() { root.planIndex = 0; root.regionIndex = 0; root.systemIndex = 0 }
        function onOrderPlaced() { PageController.goToPage(PageEnum.PageWebpackServers) }
    }

    BackButtonType {
        id: backButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin
    }

    FlickableType {
        anchors.top: backButton.bottom
        anchors.bottom: parent.bottom
        contentHeight: content.implicitHeight + 32

        ColumnLayout {
            id: content
            width: parent.width
            spacing: 16

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                headerText: qsTr("Buy a server from webpack")
                descriptionText: qsTr("Pick a plan and pay. CoCo adds the server and sets up the VPN with one press.")
            }

            // Sign-in
            ColumnLayout {
                visible: !WebpackController.signedIn
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 12

                ParagraphTextType {
                    Layout.fillWidth: true
                    text: qsTr("Sign in to your webpack.ir account. CoCo keeps only a renewable sign-in on this device, never your password.")
                }

                TextFieldWithHeaderType {
                    id: emailField
                    Layout.fillWidth: true
                    headerText: qsTr("Email")
                    textField.inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                }

                TextFieldWithHeaderType {
                    id: passwordField
                    Layout.fillWidth: true
                    headerText: qsTr("Password")
                    textField.echoMode: TextInput.Password
                }

                BasicButtonType {
                    Layout.fillWidth: true
                    text: qsTr("Sign in")
                    clickedFunc: function() {
                        WebpackController.signIn(emailField.textField.text, passwordField.textField.text)
                        passwordField.textField.text = ""
                    }
                }

                BasicButtonType {
                    Layout.fillWidth: true
                    defaultColor: AmneziaStyle.color.transparent
                    hoveredColor: AmneziaStyle.color.translucentWhite
                    pressedColor: AmneziaStyle.color.sheerWhite
                    textColor: AmneziaStyle.color.paleGray
                    borderWidth: 1
                    text: qsTr("Sign in with Google")
                    clickedFunc: function() { WebpackController.signInWithGoogle() }
                }

                LabelTextType {
                    Layout.fillWidth: true
                    text: qsTr("No account? Create one on <a href=\"https://webpack.ir\">webpack.ir</a>.")
                    textFormat: Text.RichText
                    onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                }
            }

            // Account and wallet
            LabelWithButtonType {
                visible: WebpackController.signedIn
                Layout.fillWidth: true
                text: WebpackController.accountEmail
                descriptionText: WebpackController.offline
                                 ? qsTr("Offline: webpack.ir cannot be reached")
                                 : (WebpackController.walletText !== "" ? qsTr("Wallet: %1").arg(WebpackController.walletText) : "")
                rightImageSource: "qrc:/images/controls/chevron-right.svg"
                clickedFunction: function() { WebpackController.openAccountOnWebsite() }
            }

            // Waiting for the payment in the browser
            ColumnLayout {
                visible: WebpackController.waitingForPayment
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 8

                WarningType {
                    Layout.fillWidth: true
                    backGroundColor: AmneziaStyle.color.translucentWhite
                    iconPath: "qrc:/images/controls/alert-circle.svg"
                    textString: WebpackController.paymentText
                }

                BasicButtonType {
                    Layout.fillWidth: true
                    text: qsTr("I paid, check now")
                    clickedFunc: function() { WebpackController.checkPaymentNow() }
                }

                BasicButtonType {
                    Layout.fillWidth: true
                    defaultColor: AmneziaStyle.color.transparent
                    hoveredColor: AmneziaStyle.color.translucentWhite
                    pressedColor: AmneziaStyle.color.sheerWhite
                    textColor: AmneziaStyle.color.paleGray
                    borderWidth: 1
                    text: qsTr("Stop waiting")
                    clickedFunc: function() { WebpackController.stopWaitingForPayment() }
                }
            }

            // Plan choice
            ColumnLayout {
                visible: WebpackController.signedIn && !WebpackController.waitingForPayment
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 12

                ParagraphTextType {
                    visible: WebpackController.plans.length === 0
                    Layout.fillWidth: true
                    text: qsTr("Loading plans...")
                }

                DropDownType {
                    id: planSelector
                    visible: WebpackController.plans.length > 0
                    Layout.fillWidth: true
                    drawerParent: root
                    fitContent: true
                    drawerHeight: 0.6
                    headerText: qsTr("Plan")
                    descriptionText: qsTr("Plan")
                    text: root.plan ? root.plan.name : ""
                    listView: ListViewWithRadioButtonType {
                        rootWidth: root.width
                        model: WebpackController.plans
                        selectedIndex: root.planIndex
                        clickedFunction: function() {
                            root.planIndex = selectedIndex
                            root.regionIndex = 0
                            root.systemIndex = 0
                            planSelector.closeTriggered()
                        }
                    }
                }

                ParagraphTextType {
                    visible: root.plan !== null
                    Layout.fillWidth: true
                    text: root.plan ? root.plan.specifications + "\n" + root.plan.priceText : ""
                }

                DropDownType {
                    id: regionSelector
                    visible: root.plan !== null && root.plan.regions.length > 0
                    Layout.fillWidth: true
                    drawerParent: root
                    fitContent: true
                    drawerHeight: 0.5
                    headerText: qsTr("Location")
                    descriptionText: qsTr("Location")
                    text: root.region ? root.region.name : ""
                    listView: ListViewWithRadioButtonType {
                        rootWidth: root.width
                        model: root.plan ? root.plan.regions : []
                        selectedIndex: root.regionIndex
                        clickedFunction: function() {
                            root.regionIndex = selectedIndex
                            regionSelector.closeTriggered()
                        }
                    }
                }

                DropDownType {
                    id: systemSelector
                    visible: root.plan !== null && root.plan.systems.length > 1
                    Layout.fillWidth: true
                    drawerParent: root
                    fitContent: true
                    drawerHeight: 0.5
                    headerText: qsTr("Operating system")
                    descriptionText: qsTr("Operating system")
                    text: root.system ? root.system.name : ""
                    listView: ListViewWithRadioButtonType {
                        rootWidth: root.width
                        model: root.plan ? root.plan.systems : []
                        selectedIndex: root.systemIndex
                        clickedFunction: function() {
                            root.systemIndex = selectedIndex
                            systemSelector.closeTriggered()
                        }
                    }
                }

                DropDownType {
                    id: gatewaySelector
                    visible: WebpackController.gateways.length > 1
                    Layout.fillWidth: true
                    drawerParent: root
                    fitContent: true
                    drawerHeight: 0.4
                    headerText: qsTr("If your wallet is short, pay with")
                    descriptionText: qsTr("Payment method")
                    text: WebpackController.gateways.length > root.gatewayIndex ? WebpackController.gateways[root.gatewayIndex] : ""
                    listView: ListViewWithRadioButtonType {
                        rootWidth: root.width
                        model: root.gatewayOptions()
                        selectedIndex: root.gatewayIndex
                        clickedFunction: function() {
                            root.gatewayIndex = selectedIndex
                            gatewaySelector.closeTriggered()
                        }
                    }
                }

                CaptionTextType {
                    visible: root.plan !== null
                    Layout.fillWidth: true
                    color: AmneziaStyle.color.mutedGray
                    text: root.plan ? root.plan.termsText : ""
                }

                WarningType {
                    visible: WebpackController.hasPendingOrder
                    Layout.fillWidth: true
                    backGroundColor: AmneziaStyle.color.translucentWhite
                    iconPath: "qrc:/images/controls/alert-circle.svg"
                    textString: qsTr("An earlier order did not get a clear answer. Buy sends that same order again, so you are never charged twice.")
                }

                BasicButtonType {
                    visible: root.plan !== null
                    enabled: root.plan !== null && root.plan.sellable && !WebpackController.busy
                    Layout.fillWidth: true
                    text: qsTr("Buy")
                    clickedFunc: function() {
                        WebpackController.buy(root.plan.id, root.region ? root.region.id : "",
                                              root.system ? root.system.id : "", root.gatewayIndex)
                    }
                }
            }

            BasicButtonType {
                visible: WebpackController.signedIn
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderWidth: 1
                text: qsTr("Servers bought from webpack")
                clickedFunc: function() { PageController.goToPage(PageEnum.PageWebpackServers) }
            }

            BasicButtonType {
                visible: WebpackController.signedIn
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.mutedGray
                text: qsTr("Sign out of webpack")
                clickedFunc: function() { WebpackController.signOut() }
            }
        }
    }
}
