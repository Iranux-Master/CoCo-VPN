import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import SortFilterProxyModel 0.2

import PageEnum 1.0
import ContainerProps 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Components"
import "../Config"


PageType {
    id: root

    enum ConfigType {
        AmneziaConnection,
        OpenVpn,
        WireGuard,
        Awg,
        Xray
    }

    Connections {
        target: ExportController

        function onRevokeConfigFinished() {
            PageController.showBusyIndicator(false)
            PageController.showNotificationMessage(qsTr("Config revoked"))
        }

        function onGenerateConfig(type) {
            PageController.showBusyIndicator(true)

            var configCaption
            var configExtension
            var configFileName

            var containerIndex = ServersUiController.processedContainerIndex
            var serverId = ServersUiController.processedServerId

            switch (type) {
            case PageShare.ConfigType.AmneziaConnection: {
                ExportController.generateConnectionConfig(serverId, containerIndex, clientNameTextField.textField.text);
                configCaption = qsTr("Save CoCo VPN config")
                configExtension = ".vpn"
                configFileName = "amnezia_config"
                break;
            }
            case PageShare.ConfigType.OpenVpn: {
                ExportController.generateOpenVpnConfig(serverId, clientNameTextField.textField.text)
                configCaption = qsTr("Save OpenVPN config")
                configExtension = ".ovpn"
                configFileName = "amnezia_for_openvpn"
                break
            }
            case PageShare.ConfigType.WireGuard: {
                ExportController.generateWireGuardConfig(serverId, clientNameTextField.textField.text)
                configCaption = qsTr("Save WireGuard config")
                configExtension = ".conf"
                configFileName = "amnezia_for_wireguard"
                break
            }
            case PageShare.ConfigType.Awg: {
                ExportController.generateAwgConfig(serverId, containerIndex, clientNameTextField.textField.text)
                configCaption = qsTr("Save AmneziaWG config")
                configExtension = ".conf"
                configFileName = "amnezia_for_awg"
                break
            }
            case PageShare.ConfigType.Xray: {
                ExportController.generateXrayConfig(serverId, clientNameTextField.textField.text)
                configCaption = qsTr("Save XRay config")
                configExtension = ".json"
                configFileName = "amnezia_for_xray"
                break
            }
            }

            PageController.showBusyIndicator(false)
            
            var headerText = qsTr("Connection to ") + serverSelector.text
            var configContentHeaderText = qsTr("File with connection settings to ") + serverSelector.text
            PageController.goToShareConnectionPage(headerText, configContentHeaderText, configCaption, configExtension, configFileName)
        }

        function onExportErrorOccurred(error) {
            PageController.showBusyIndicator(false)
            PageController.showErrorMessage(error)
        }

        function onAccountBatchFinished(succeeded, failed) {
            PageController.showBusyIndicator(false)
            PageController.showNotificationMessage(qsTr("Account creation finished: %1 succeeded, %2 failed").arg(succeeded).arg(failed))
        }
    }

    property bool isSearchBarVisible: false
    property bool showContent: false
    property bool shareButtonEnabled: true
    property var selectedBatchProtocols: []
    property var selectedAccountIds: []
    property string sharePreview: ""
    property list<QtObject> connectionTypesModel: [
        amneziaConnectionFormat
    ]

    QtObject {
        id: amneziaConnectionFormat
        readonly property string name: qsTr("For the CoCo VPN app")
        readonly property int type: PageShare.ConfigType.AmneziaConnection
    }
    QtObject {
        id: openVpnConnectionFormat
        readonly property string name: qsTr("OpenVPN native format")
        readonly property int type: PageShare.ConfigType.OpenVpn
    }
    QtObject {
        id: wireGuardConnectionFormat
        readonly property string name: qsTr("WireGuard native format")
        readonly property int type: PageShare.ConfigType.WireGuard
    }
    QtObject {
        id: awgConnectionFormat
        readonly property string name: qsTr("AmneziaWG native format")
        readonly property int type: PageShare.ConfigType.Awg
    }
    QtObject {
        id: xrayConnectionFormat
        readonly property string name: qsTr("XRay native format")
        readonly property int type: PageShare.ConfigType.Xray
    }

    FlickableType {
        id: a

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        contentHeight: content.height + 10

        ColumnLayout {
            id: content

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            anchors.rightMargin: 16
            anchors.leftMargin: 16

            spacing: 0

            HeaderTypeWithButton {
                id: header
                Layout.fillWidth: true
                Layout.topMargin: 24 + PageController.safeAreaTopMargin

                headerText: qsTr("Share VPN Access")

                actionButtonImage: "qrc:/images/controls/more-vertical.svg"
                actionButtonFunction: function() {
                    shareFullAccessDrawer.openTriggered()
                }

                DrawerType2 {
                    id: shareFullAccessDrawer

                    parent: root

                    anchors.fill: parent
                    expandedHeight: root.height

                    expandedStateContent: ColumnLayout {
                        id: shareFullAccessDrawerContent
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.topMargin: 16

                        spacing: 0

                        onImplicitHeightChanged: {
                            shareFullAccessDrawer.expandedHeight = shareFullAccessDrawerContent.implicitHeight + 32
                        }

                        Header2Type {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 16
                            Layout.leftMargin: 16
                            Layout.rightMargin: 16

                            headerText: qsTr("Share full access to the server and VPN")
                            descriptionText: qsTr("Use for your own devices, or share with those you trust to manage the server.")
                        }

                        LabelWithButtonType {
                            id: shareFullAccessButton
                            Layout.fillWidth: true

                            text: qsTr("Share")
                            rightImageSource: "qrc:/images/controls/chevron-right.svg"

                            clickedFunction: function() {
                                PageController.goToPage(PageEnum.PageShareFullAccess)
                                shareFullAccessDrawer.closeTriggered()
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: accessTypeSelector

                property int currentIndex

                Layout.topMargin: 32

                implicitWidth: accessTypeSelectorContent.implicitWidth
                implicitHeight: accessTypeSelectorContent.implicitHeight

                color: AmneziaStyle.color.onyxBlack
                radius: 16

                RowLayout {
                    id: accessTypeSelectorContent

                    spacing: 0

                    HorizontalRadioButton {
                        id: connectionRadioButton
                        checked: accessTypeSelector.currentIndex === 0

                        implicitWidth: (root.width - 32) / 3
                        text: qsTr("Connection")

                        onClicked: {
                            accessTypeSelector.currentIndex = 0
                        }

                        Keys.onEnterPressed: this.clicked()
                        Keys.onReturnPressed: this.clicked()
                    }

                    HorizontalRadioButton {
                        id: usersRadioButton
                        checked: accessTypeSelector.currentIndex === 1

                        implicitWidth: (root.width - 32) / 3
                        text: qsTr("Users")

                        onClicked: {
                            accessTypeSelector.currentIndex = 1
                            PageController.showBusyIndicator(true)
                            ExportController.updateClientManagementModel(ServersUiController.processedServerId,
                                                                         ServersUiController.processedContainerIndex)
                            PageController.showBusyIndicator(false)
                        }

                        Keys.onEnterPressed: this.clicked()
                        Keys.onReturnPressed: this.clicked()
                    }
                    HorizontalRadioButton {
                        checked: accessTypeSelector.currentIndex === 2
                        implicitWidth: (root.width - 32) / 3
                        text: qsTr("Accounts")
                        onClicked: accessTypeSelector.currentIndex = 2
                    }
                }
            }

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.bottomMargin: 24

                visible: accessTypeSelector.currentIndex === 0

                text: qsTr("Share VPN access without the ability to manage the server")
                color: AmneziaStyle.color.mutedGray
            }

            TextFieldWithHeaderType {
                id: clientNameTextField
                Layout.fillWidth: true
                Layout.topMargin: 16

                visible: accessTypeSelector.currentIndex === 0

                headerText: qsTr("User name")
                textField.text: "New client"
                textField.maximumLength: 20

                checkEmptyText: true
            }

            DropDownType {
                id: serverSelector

                signal serverSelectorIndexChanged
                property int currentIndex: -1

                Layout.fillWidth: true
                Layout.topMargin: 16

                drawerHeight: 0.4375
                drawerParent: root

                descriptionText: qsTr("Server")
                headerText: qsTr("Server")

                listView: ListViewWithRadioButtonType {
                    id: serverSelectorListView
                    rootWidth: root.width
                    imageSource: "qrc:/images/controls/check.svg"

                    model: SortFilterProxyModel {
                        id: proxyServersModel
                        sourceModel: ServersModel
                        filters: [
                            ValueFilter {
                                roleName: "hasWriteAccess"
                                value: true
                            },
                            ValueFilter {
                                roleName: "hasInstalledContainers"
                                value: true
                            }
                        ]
                    }

                    clickedFunction: function() {
                        handler()

                        if (serverSelector.currentIndex !== serverSelectorListView.selectedIndex) {
                            serverSelector.currentIndex = serverSelectorListView.selectedIndex
                            serverSelector.serverSelectorIndexChanged()
                        }

                        serverSelector.closeTriggered()
                    }

                    Component.onCompleted: {
                        if (ServersUiController.isServerHasWriteAccess(ServersUiController.defaultServerId)
                            && ServersUiController.serverHasInstalledContainers(ServersUiController.defaultServerId)) {
                            serverSelectorListView.selectedIndex =
                                proxyServersModel.mapFromSource(ServersUiController.getServerIndexById(ServersUiController.defaultServerId))
                        } else {
                            serverSelectorListView.selectedIndex = 0
                        }

                        serverSelectorListView.positionViewAtIndex(selectedIndex, ListView.Beginning)
                        serverSelectorListView.triggerCurrentItem()
                    }

                    function handler() {
                        serverSelector.text = selectedText
                        ServersUiController.setProcessedServerId(ServersUiController.getServerId(proxyServersModel.mapToSource(selectedIndex)))
                    }
                }
            }

            DropDownType {
                id: containerSelector

                signal containerSelectorTextChanged

                Layout.fillWidth: true
                Layout.topMargin: 16

                drawerHeight: 0.5
                drawerParent: root

                descriptionText: qsTr("Protocol")
                headerText: qsTr("Protocol")

                listView: ListViewWithRadioButtonType {
                    id: containerSelectorListView

                    rootWidth: root.width
                    imageSource: "qrc:/images/controls/check.svg"

                    model: SortFilterProxyModel {
                        id: proxyContainersModel
                        sourceModel: ContainersModel
                        filters: [
                            ValueFilter {
                                roleName: "isInstalled"
                                value: true
                            },
                            ValueFilter {
                                roleName: "isShareable"
                                value: true
                            },
                            ValueFilter {
                                roleName: "isUnsupportedContainer"
                                value: false
                            }
                        ]
                    }

                    clickedFunction: function() {
                        handler()

                        containerSelector.closeTriggered()
                    }

                    Connections {
                        target: serverSelector

                        function onServerSelectorIndexChanged() {
                            if (!proxyContainersModel.count) {
                                root.shareButtonEnabled = false
                                return
                            }

                            var defaultContainer = proxyContainersModel.mapFromSource(
                                        ServersUiController.serverDefaultContainer(ServersUiController.processedServerId))
                            if (defaultContainer < 0) {
                                defaultContainer = 0
                            }

                            containerSelectorListView.selectedIndex = defaultContainer
                            containerSelectorListView.positionViewAtIndex(defaultContainer, ListView.Beginning)
                            containerSelectorListView.triggerCurrentItem()
                        }
                    }

                    function handler() {
                        if (!proxyContainersModel.count) {
                            root.shareButtonEnabled = false
                            return
                        } else {
                            root.shareButtonEnabled = true
                        }

                        containerSelector.text = selectedText

                        ServersUiController.processedContainerIndex = proxyContainersModel.mapToSource(selectedIndex)

                        fillConnectionTypeModel()

                        if (accessTypeSelector.currentIndex === 1) {
                            PageController.showBusyIndicator(true)
                            ExportController.updateClientManagementModel(ServersUiController.processedServerId,
                                                                         ServersUiController.processedContainerIndex)
                            PageController.showBusyIndicator(false)
                        }

                        containerSelector.containerSelectorTextChanged()
                    }

                    function fillConnectionTypeModel() {
                        root.connectionTypesModel = [amneziaConnectionFormat]

                        var index = proxyContainersModel.mapToSource(selectedIndex)

                        if (index === ContainerProps.containerFromString("amnezia-openvpn")) {
                            root.connectionTypesModel.push(openVpnConnectionFormat)
                        } else if (index === ContainerProps.containerFromString("amnezia-wireguard")) {
                            root.connectionTypesModel.push(wireGuardConnectionFormat)
                        } else if (index === ContainerProps.containerFromString("amnezia-awg")) {
                            root.connectionTypesModel.push(awgConnectionFormat)
                        } else if (index === ContainerProps.containerFromString("amnezia-awg2")) {
                            root.connectionTypesModel.push(awgConnectionFormat)
                        } else if (index === ContainerProps.containerFromString("amnezia-xray")) {
                            root.connectionTypesModel.push(xrayConnectionFormat)
                        }
                    }
                }
            }

            DropDownType {
                id: exportTypeSelector

                property int currentIndex: 0

                Layout.fillWidth: true
                Layout.topMargin: 16

                drawerHeight: 0.4375
                drawerParent: root

                visible: accessTypeSelector.currentIndex === 0
                enabled: root.connectionTypesModel.length > 1

                descriptionText: qsTr("Connection format")
                headerText: qsTr("Connection format")

                listView: ListViewWithRadioButtonType {
                    id: exportTypeSelectorListView

                    onCurrentIndexChanged: {
                        exportTypeSelector.currentIndex = exportTypeSelectorListView.selectedIndex
                        exportTypeSelector.text = exportTypeSelectorListView.selectedText
                    }

                    onModelChanged: {
                        if (exportTypeSelector.currentIndex >= model.length || exportTypeSelector.currentIndex < 0) {
                            exportTypeSelector.currentIndex = 0
                        }
                        selectedIndex = exportTypeSelector.currentIndex
                        if (model.length > 0 && model[selectedIndex] && model[selectedIndex].name !== undefined) {
                            exportTypeSelectorListView.selectedText = model[selectedIndex].name
                            exportTypeSelector.text = model[selectedIndex].name
                        } else {
                            exportTypeSelectorListView.selectedText = ""
                            exportTypeSelector.text = ""
                        }
                    }

                    rootWidth: root.width

                    imageSource: "qrc:/images/controls/check.svg"

                    model: root.connectionTypesModel
                    currentIndex: 0

                    Connections {
                        target: containerSelector

                        function onContainerSelectorTextChanged() {
                            if (exportTypeSelector.currentIndex >= root.connectionTypesModel.length) {
                                exportTypeSelectorListView.selectedIndex = 0
                                exportTypeSelector.currentIndex = 0
                                exportTypeSelector.text = root.connectionTypesModel[0].name
                            }
                        }
                    }

                    clickedFunction: function() {
                        exportTypeSelector.text = exportTypeSelectorListView.selectedText
                        exportTypeSelector.currentIndex = exportTypeSelectorListView.selectedIndex
                        exportTypeSelector.closeTriggered()
                    }
                }
            }

            ColumnLayout {
                id: accountsPane
                visible: accessTypeSelector.currentIndex === 2
                Layout.fillWidth: true
                Layout.topMargin: 24
                spacing: 12

                Header2Type {
                    Layout.fillWidth: true
                    headerText: qsTr("Create account groups")
                    descriptionText: qsTr("Each account gets separate credentials for every selected connection method.")
                }
                TextFieldWithHeaderType {
                    id: batchNameField
                    Layout.fillWidth: true
                    headerText: qsTr("Account name")
                    textField.text: qsTr("Client")
                    textField.maximumLength: 20
                    textField.inputMethodHints: Qt.ImhLatinOnly
                }
                TextFieldWithHeaderType {
                    id: batchCountField
                    Layout.fillWidth: true
                    headerText: qsTr("Number of accounts (1–100)")
                    textField.text: "1"
                    textField.inputMethodHints: Qt.ImhDigitsOnly
                }
                ParagraphTextType {
                    Layout.fillWidth: true
                    text: qsTr("Select installed VPN methods")
                    color: AmneziaStyle.color.paleGray
                }
                Repeater {
                    model: proxyContainersModel
                    delegate: CheckBox {
                        required property int index
                        required property string name
                        visible: isVpnContainer
                        text: name
                        checked: root.selectedBatchProtocols.indexOf(proxyContainersModel.mapToSource(index)) >= 0
                        onToggled: {
                            var values = root.selectedBatchProtocols.slice()
                            var sourceIndex = proxyContainersModel.mapToSource(index)
                            if (checked && values.indexOf(sourceIndex) < 0) values.push(sourceIndex)
                            if (!checked) values = values.filter(function(value) { return value !== sourceIndex })
                            root.selectedBatchProtocols = values
                        }
                    }
                }
                BasicButtonType {
                    Layout.fillWidth: true
                    enabled: !ExportController.batchRunning && root.selectedBatchProtocols.length > 0
                    text: ExportController.batchRunning
                          ? qsTr("Creating accounts: %1 of %2").arg(ExportController.batchProgress).arg(ExportController.batchTotal)
                          : qsTr("Create accounts")
                    clickedFunc: function() {
                        var count = parseInt(batchCountField.textField.text)
                        if (!count || count < 1 || count > 100 || batchNameField.textField.text.trim() === "") return
                        PageController.showBusyIndicator(true)
                        ExportController.startAccountBatch(ServersUiController.processedServerId, serverSelector.text,
                                                           batchNameField.textField.text, count, root.selectedBatchProtocols)
                    }
                }
                Header2Type {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    headerText: qsTr("Created accounts")
                    descriptionText: qsTr("Select one or more accounts to prepare a share message.")
                }
                Repeater {
                    model: ExportController.accountGroups
                    delegate: CheckBox {
                        required property var modelData
                        text: modelData.name + " · " + modelData.serverName + " · " + modelData.methods.length + " " + qsTr("methods")
                              + (modelData.status === "partial" ? " · " + qsTr("Some methods failed") : "")
                        checked: root.selectedAccountIds.indexOf(modelData.id) >= 0
                        onToggled: {
                            var ids = root.selectedAccountIds.slice()
                            if (checked && ids.indexOf(modelData.id) < 0) ids.push(modelData.id)
                            if (!checked) ids = ids.filter(function(value) { return value !== modelData.id })
                            root.selectedAccountIds = ids
                        }
                    }
                }
                ComboBox {
                    id: savedTemplatesBox
                    Layout.fillWidth: true
                    model: ExportController.shareTemplates
                    textRole: "name"
                    displayText: currentIndex >= 0 ? currentText : qsTr("Choose a saved template")
                    onActivated: {
                        if (currentIndex >= 0) templateTextArea.text = model[currentIndex].body
                    }
                }
                TextArea {
                    id: templateTextArea
                    Layout.fillWidth: true
                    Layout.preferredHeight: 140
                    wrapMode: TextEdit.Wrap
                    text: "{{NAME}}\nسرور: {{SERVER}}\nروش‌های اتصال: {{PROTOCOLS}}\n{{CONFIGS}}\n{{QR}}"
                    placeholderText: qsTr("Tags: {{NAME}}, {{SERVER}}, {{PROTOCOLS}}, {{CONFIGS}}, {{QR}}")
                }
                RowLayout {
                    Layout.fillWidth: true
                    TextField { id: templateNameField; Layout.fillWidth: true; placeholderText: qsTr("Template name") }
                    BasicButtonType {
                        text: qsTr("Save template")
                        clickedFunc: function() {
                            ExportController.saveShareTemplate(templateNameField.text, templateTextArea.text)
                            templateNameField.text = ""
                        }
                    }
                }
                BasicButtonType {
                    Layout.fillWidth: true
                    enabled: root.selectedAccountIds.length > 0
                    text: qsTr("Preview")
                    clickedFunc: function() {
                        root.sharePreview = ExportController.renderAccountsTemplate(root.selectedAccountIds, templateTextArea.text)
                    }
                }
                TextArea {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.RichText
                    text: root.sharePreview
                    visible: root.sharePreview.length > 0
                }
                BasicButtonType {
                    Layout.fillWidth: true
                    enabled: root.sharePreview.length > 0
                    text: qsTr("Share prepared message")
                    leftImageSource: "qrc:/images/controls/share-2.svg"
                    clickedFunc: function() {
                        ExportController.setConfigFromString(root.sharePreview, "cocovpn_accounts.html")
                    }
                }
                ParagraphTextType {
                    Layout.fillWidth: true
                    text: qsTr("The {{QR}} tag places QR codes in the preview. Connection settings grant access, so send them only to people you trust.")
                    color: AmneziaStyle.color.mutedGray
                }
            }

            BasicButtonType {
                id: shareButton

                Layout.fillWidth: true
                Layout.topMargin: 40
                Layout.bottomMargin: 32

                enabled: shareButtonEnabled
                visible: accessTypeSelector.currentIndex === 0

                text: qsTr("Share")
                leftImageSource: "qrc:/images/controls/share-2.svg"

                clickedFunc: function(){
                    if (clientNameTextField.textField.text !== "") {
                        ExportController.generateConfig(root.connectionTypesModel[exportTypeSelector.currentIndex].type)
                    }
                }
            }

            Header2Type {
                id: usersHeader
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.bottomMargin: 16

                visible: accessTypeSelector.currentIndex === 1 && !root.isSearchBarVisible

                headerText: qsTr("Users")
                actionButtonImage: "qrc:/images/controls/search.svg"
                actionButtonFunction: function() {
                    root.isSearchBarVisible = true
                }
            }

            RowLayout {
                Layout.topMargin: 24
                Layout.bottomMargin: 16
                visible: accessTypeSelector.currentIndex === 1 && root.isSearchBarVisible

                TextFieldWithHeaderType {
                    id: searchTextField
                    Layout.fillWidth: true

                    textField.placeholderText: qsTr("Search")

                    Keys.onEscapePressed: {
                        searchTextField.textField.text = ""
                        root.isSearchBarVisible = false
                    }

                    function navigateTo() {
                        if (searchTextField.textField.text === "") {
                            root.isSearchBarVisible = false
                        }
                    }

                    Keys.onTabPressed: { navigateTo() }
                    Keys.onEnterPressed: { navigateTo() }
                    Keys.onReturnPressed: { navigateTo() }
                }

                ImageButtonType {
                    id: closeSearchButton
                    image: "qrc:/images/controls/close.svg"
                    imageColor: AmneziaStyle.color.paleGray

                    function clickedFunc() {
                        searchTextField.textField.text = ""
                        root.isSearchBarVisible = false
                    }

                    onClicked: clickedFunc()
                    Keys.onEnterPressed: clickedFunc()
                    Keys.onReturnPressed: clickedFunc()
                }
            }

            ListView {
                id: clientsListView
                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight

                visible: accessTypeSelector.currentIndex === 1

                function escapeRe(s) { return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') }

                property bool isFocusable: true
                property bool freezeFilter: false

                model: SortFilterProxyModel {
                    id: proxyClientManagementModel
                    sourceModel: ClientManagementModel
                    filters: RegExpFilter {
                        roleName: "clientName"
                        enabled: !clientsListView.freezeFilter
                        pattern: ".*" + clientsListView.escapeRe(searchTextField.textField.text) + ".*"
                        caseSensitivity: Qt.CaseInsensitive
                    }
                }

                clip: true
                interactive: false
                reuseItems: true

                delegate: Item {
                    implicitWidth: clientsListView.width
                    implicitHeight: delegateContent.implicitHeight

                    ColumnLayout {
                        id: delegateContent

                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right

                        anchors.rightMargin: -16
                        anchors.leftMargin: -16

                        LabelWithButtonType {
                            id: clientFocusItem
                            Layout.fillWidth: true

                            text: clientName
                            rightImageSource: "qrc:/images/controls/chevron-right.svg"

                            clickedFunction: function() {
                                clientInfoDrawer.openTriggered()
                            }
                        }

                        DividerType {}

                        DrawerType2 {
                            id: clientInfoDrawer

                            parent: root

                            width: root.width
                            height: root.height

                            expandedStateContent: ColumnLayout {
                                id: expandedStateContent
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.topMargin: 16
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16

                                onImplicitHeightChanged: {
                                    clientInfoDrawer.expandedHeight = expandedStateContent.implicitHeight + 32
                                }

                                Header2TextType {
                                    Layout.maximumWidth: parent.width
                                    Layout.bottomMargin: 24

                                    text: clientName
                                    maximumLineCount: 2
                                    wrapMode: Text.Wrap
                                    elide: Qt.ElideRight
                                }

                                ParagraphTextType {
                                    color: AmneziaStyle.color.mutedGray
                                    visible: creationDate
                                    Layout.maximumWidth: parent.width

                                    maximumLineCount: 2
                                    wrapMode: Text.Wrap
                                    elide: Qt.ElideRight

                                    text: qsTr("Creation date: %1").arg(creationDate)
                                }

                                ParagraphTextType {
                                    color: AmneziaStyle.color.mutedGray
                                    visible: latestHandshake
                                    Layout.maximumWidth: parent.width

                                    maximumLineCount: 2
                                    wrapMode: Text.Wrap
                                    elide: Qt.ElideRight

                                    text: qsTr("Latest handshake: %1").arg(latestHandshake)
                                }

                                ParagraphTextType {
                                    color: AmneziaStyle.color.mutedGray
                                    visible: dataReceived
                                    Layout.maximumWidth: parent.width

                                    maximumLineCount: 2
                                    wrapMode: Text.Wrap
                                    elide: Qt.ElideRight

                                    text: qsTr("Data received: %1").arg(dataReceived)
                                }

                                ParagraphTextType {
                                    color: AmneziaStyle.color.mutedGray
                                    visible: dataSent
                                    Layout.maximumWidth: parent.width

                                    maximumLineCount: 2
                                    wrapMode: Text.Wrap
                                    elide: Qt.ElideRight

                                    text: qsTr("Data sent: %1").arg(dataSent)
                                }

                                ParagraphTextType {
                                    color: AmneziaStyle.color.mutedGray
                                    visible: allowedIps
                                    Layout.maximumWidth: parent.width

                                    wrapMode: Text.Wrap

                                    text: qsTr("Allowed IPs: %1").arg(allowedIps)
                                }

                                BasicButtonType {
                                    id: renameButton
                                    Layout.fillWidth: true
                                    Layout.topMargin: 24

                                    defaultColor: AmneziaStyle.color.transparent
                                    hoveredColor: AmneziaStyle.color.translucentWhite
                                    pressedColor: AmneziaStyle.color.sheerWhite
                                    disabledColor: AmneziaStyle.color.mutedGray
                                    textColor: AmneziaStyle.color.paleGray
                                    borderWidth: 1

                                    text: qsTr("Rename")

                                    clickedFunc: function() {
                                        clientNameEditDrawer.openTriggered()
                                    }

                                    DrawerType2 {
                                        id: clientNameEditDrawer

                                        parent: root

                                        anchors.fill: parent
                                        expandedHeight: root.height * 0.35

                                        expandedStateContent: ColumnLayout {
                                            anchors.top: parent.top
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.topMargin: 32
                                            anchors.leftMargin: 16
                                            anchors.rightMargin: 16

                                            TextFieldWithHeaderType {
                                                id: clientNameEditor
                                                Layout.fillWidth: true
                                                headerText: qsTr("Client name")
                                                textField.text: clientName
                                                textField.maximumLength: 20
                                                checkEmptyText: true
                                            }

                                            BasicButtonType {
                                                id: saveButton

                                                Layout.fillWidth: true

                                                text: qsTr("Save")

                                                clickedFunc: function() {
                                                    if (clientNameEditor.textField.text === "") {
                                                        return
                                                    }

                                                    if (clientNameEditor.textField.text !== clientName) {
                                                        clientsListView.freezeFilter = true
                                                        PageController.showBusyIndicator(true)
                                                        ExportController.renameClient(proxyClientManagementModel.mapToSource(index),
                                                                                          clientNameEditor.textField.text,
                                                                                          ServersUiController.processedServerId,
                                                                                          ServersUiController.processedContainerIndex)
                                                        PageController.showBusyIndicator(false)
                                                        Qt.callLater(function(){ clientsListView.freezeFilter = false })
                                                        clientNameEditDrawer.closeTriggered()
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                BasicButtonType {
                                    id: revokeButton
                                    Layout.fillWidth: true
                                    Layout.topMargin: 8

                                    defaultColor: AmneziaStyle.color.transparent
                                    hoveredColor: AmneziaStyle.color.translucentWhite
                                    pressedColor: AmneziaStyle.color.sheerWhite
                                    disabledColor: AmneziaStyle.color.mutedGray
                                    textColor: AmneziaStyle.color.paleGray
                                    borderWidth: 1

                                    text: qsTr("Revoke")

                                    clickedFunc: function() {
                                        var headerText = qsTr("Revoke the config for a user - %1?").arg(clientName)
                                        var descriptionText = qsTr("The user will no longer be able to connect to your server.")
                                        var yesButtonText = qsTr("Continue")
                                        var noButtonText = qsTr("Cancel")

                                        var yesButtonFunction = function() {
                                            clientInfoDrawer.closeTriggered()
                                            PageController.showBusyIndicator(true)
                                            ExportController.revokeConfig(proxyClientManagementModel.mapToSource(index),
                                                                              ServersUiController.processedServerId,
                                                                              ServersUiController.processedContainerIndex)
                                        }
                                        var noButtonFunction = function() {
                                        }

                                        if (ConnectionController.isRevokeBlockedDuringActiveConnection(
                                                ServersUiController.processedServerId,
                                                ServersUiController.processedContainerIndex,
                                                clientId)) {
                                            PageController.showNotificationMessage("Unable to revoke current config during active connection")
                                        } else {
                                            showQuestionDrawer(headerText, descriptionText, yesButtonText, noButtonText, yesButtonFunction, noButtonFunction)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

}
