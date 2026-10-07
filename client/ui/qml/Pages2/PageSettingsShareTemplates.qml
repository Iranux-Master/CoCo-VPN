import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"

PageType {
    id: root

    property string editingTemplateId: ""
    property string editingProtocol: "general"
    property string editingTemplateName: ""
    property string editingTemplateBody: ""
    property bool editingIsSystem: false

    Connections {
        target: ExportController
        enabled: root.visible
        function onExportErrorOccurred(error) { PageController.showErrorMessage(error) }
    }
    property var templateTags: ["{{NAME}}", "{{SERVER}}", "{{PROTOCOLS}}", "{{CONFIGS}}", "{{QR}}"]
    property var protocolOptions: [
        {"key": "general", "name": qsTr("General")},
        {"key": "openvpn", "name": "OpenVPN"},
        {"key": "wireguard", "name": "WireGuard"},
        {"key": "awg", "name": "AWG"},
        {"key": "xray", "name": "XRay"},
        {"key": "ikev2", "name": "IKEv2"}
    ]

    function protocolName(key) {
        for (var i = 0; i < protocolOptions.length; ++i) {
            if (protocolOptions[i].key === key) return protocolOptions[i].name
        }
        return qsTr("General")
    }

    function templateDisplayName(item) {
        if (!item) return ""
        if (!item.isSystem) return item.name || ""
        if (item.id === "system-general") return qsTr("General")
        if (item.id === "system-openvpn") return "OpenVPN"
        if (item.id === "system-wireguard") return "WireGuard"
        if (item.id === "system-awg") return "AWG"
        if (item.id === "system-xray") return "XRay"
        if (item.id === "system-ikev2") return "IKEv2"
        return item.name || ""
    }

    function templateDisplayNameById(id) {
        return root.templateDisplayName(ExportController.shareTemplate(id))
    }

    function templateOptionsForProtocol(protocol) {
        var source = ExportController.templatesForProtocol(protocol)
        var result = []
        for (var i = 0; i < source.length; ++i) {
            var item = source[i]
            result.push({"id": item.id, "name": root.templateDisplayName(item),
                         "protocol": item.protocol, "isSystem": item.isSystem})
        }
        return result
    }

    function openNewTemplate() {
        editingTemplateId = ""
        editingProtocol = "general"
        editingTemplateName = ""
        editingTemplateBody = "سلام {{NAME}}،\nسرور: {{SERVER}}\nروش اتصال: {{PROTOCOLS}}\n{{CONFIGS}}\n{{QR}}"
        editingIsSystem = false
        templateEditor.openTriggered()
    }

    function previewTemplate(id) {
        if (!ExportController.openHtmlPreview(ExportController.renderShareTemplatePreview(id)))
            PageController.showNotificationMessage(qsTr("Could not open HTML preview"))
    }

    function downloadPreview(id) {
        var html = ExportController.renderShareTemplatePreview(id)
        if (html.length === 0) return
        var fileName = SystemController.getFileName(qsTr("Save HTML preview"), qsTr("HTML files (*.html)"),
                StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/CoCoVPN-template-preview.html",
                true, "html")
        if (fileName !== "" && ExportController.setConfigFromString(html, fileName))
            PageController.showNotificationMessage(qsTr("Sharing message saved"))
    }

    function openTemplate(item) {
        if (item.isSystem && Qt.platform.os === "windows") {
            root.previewTemplate(item.id)
            return
        }
        editingTemplateId = item.id
        editingProtocol = item.protocol || "general"
        editingTemplateName = root.templateDisplayName(item)
        editingTemplateBody = item.body || ""
        editingIsSystem = item.isSystem === true
        templateEditor.openTriggered()
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
                headerText: qsTr("Sharing templates")
                descriptionText: qsTr("Create reusable messages and choose the default template for each VPN protocol.")
            }

            Header2Type {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                headerText: qsTr("Default templates")
                descriptionText: qsTr("The matching template is selected automatically when accounts are shared.")
            }

            Repeater {
                model: root.protocolOptions

                DropDownType {
                    id: defaultSelector
                    required property var modelData
                    property string protocolKey: modelData.key
                    property var availableTemplates: {
                        var revision = ExportController.shareTemplates
                        return root.templateOptionsForProtocol(protocolKey)
                    }
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    drawerParent: root
                    fitContent: true
                    drawerHeight: 0.55
                    descriptionText: modelData.name
                    headerText: qsTr("Default template for %1").arg(modelData.name)
                    text: {
                        var revision = ExportController.shareTemplates
                        return root.templateDisplayNameById(ExportController.defaultShareTemplateId(protocolKey))
                    }

                    listView: ListViewWithRadioButtonType {
                        rootWidth: root.width
                        model: defaultSelector.availableTemplates
                        selectedIndex: {
                            var revision = ExportController.shareTemplates
                            var selectedId = ExportController.defaultShareTemplateId(defaultSelector.protocolKey)
                            for (var i = 0; i < defaultSelector.availableTemplates.length; ++i)
                                if (defaultSelector.availableTemplates[i].id === selectedId) return i
                            return 0
                        }
                        clickedFunction: function() {
                            var item = defaultSelector.availableTemplates[selectedIndex]
                            if (!item) return
                            ExportController.setDefaultShareTemplate(defaultSelector.protocolKey, item.id)
                            defaultSelector.closeTriggered()
                        }
                    }
                }
            }

            Header2Type {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                headerText: qsTr("Available templates")
                descriptionText: qsTr("System templates are protected. You can create, edit and delete your own templates.")
            }

            BasicButtonType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: qsTr("Create template")
                leftImageSource: "qrc:/images/controls/plus.svg"
                clickedFunc: function() { root.openNewTemplate() }
            }

            Repeater {
                model: ExportController.shareTemplates

                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 0

                    LabelWithButtonType {
                        Layout.fillWidth: true
                        text: root.templateDisplayName(modelData)
                        descriptionText: root.protocolName(modelData.protocol)
                                         + " · " + (modelData.isSystem ? qsTr("System template") : qsTr("Custom template"))
                        leftImageSource: modelData.isSystem
                                         ? "qrc:/images/controls/file-check-2.svg"
                                         : "qrc:/images/controls/edit-3.svg"
                        rightImageSource: modelData.isSystem
                                          ? "qrc:/images/controls/eye.svg"
                                          : "qrc:/images/controls/edit-3.svg"
                        clickedFunction: function() { root.openTemplate(modelData) }
                    }

                    BasicButtonType {
                        Layout.alignment: Qt.AlignRight
                        Layout.rightMargin: 16
                        Layout.bottomMargin: 12
                        implicitHeight: 44
                        visible: Qt.platform.os === "windows"
                        text: qsTr("Save HTML preview")
                        clickedFunc: function() { root.downloadPreview(modelData.id) }
                    }

                    DividerType {}
                }
            }
        }
    }

    DrawerType2 {
        id: templateEditor
        parent: root
        anchors.fill: parent
        expandedHeight: root.height

        expandedStateContent: Item {
            // DrawerType2's Loader derives its height from this content.
            implicitHeight: templateEditor.expandedHeight

            Connections {
                target: templateEditor
                function onAboutToShow() {
                    templateNameField.textField.text = root.editingTemplateName
                    templateBodyField.textArea.text = root.editingTemplateBody
                    templateProtocolSelector.text = root.protocolName(root.editingProtocol)
                }
            }

            BackButtonType {
                id: editorBackButton
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: 16
                backButtonFunction: function() { templateEditor.closeTriggered() }
            }

            Header2Type {
                id: editorHeader
                anchors.top: editorBackButton.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                headerText: root.editingTemplateId === ""
                            ? qsTr("Create template")
                            : (root.editingIsSystem ? qsTr("Template preview") : qsTr("Edit template"))
                descriptionText: root.editingIsSystem
                                 ? qsTr("System templates are read-only.")
                                 : qsTr("Click a tag to insert it at the current cursor position.")
            }

            FlickableType {
                id: editorScroll
                anchors.top: editorHeader.bottom
                anchors.topMargin: 16
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                contentHeight: editorContent.implicitHeight + 32

                ColumnLayout {
                    id: editorContent
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    TextFieldWithHeaderType {
                        id: templateNameField
                        Layout.fillWidth: true
                        textFieldEditable: !root.editingIsSystem
                        headerText: qsTr("Template name")
                        placeholderText: qsTr("Enter a name for this template")
                    }

                    DropDownType {
                        id: templateProtocolSelector
                        Layout.fillWidth: true
                        enabled: !root.editingIsSystem
                        drawerParent: root
                        fitContent: true
                        drawerHeight: 0.5
                        headerText: qsTr("Template protocol")
                        descriptionText: qsTr("Protocol")
                        text: root.protocolName(root.editingProtocol)
                        listView: ListViewWithRadioButtonType {
                            rootWidth: root.width
                            model: root.protocolOptions
                            currentValue: root.protocolName(root.editingProtocol)
                            clickedFunction: function() {
                                var item = root.protocolOptions[selectedIndex]
                                root.editingProtocol = item.key
                                templateProtocolSelector.text = item.name
                                templateProtocolSelector.closeTriggered()
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        visible: !root.editingIsSystem
                        spacing: 8
                        Repeater {
                            model: root.templateTags
                            BasicButtonType {
                                required property string modelData
                                implicitHeight: 44
                                width: Math.max(buttonTextLabel.implicitWidth + 32, 84)
                                defaultColor: AmneziaStyle.color.transparent
                                hoveredColor: AmneziaStyle.color.translucentWhite
                                pressedColor: AmneziaStyle.color.sheerWhite
                                textColor: AmneziaStyle.color.paleGray
                                borderColor: AmneziaStyle.color.slateGray
                                borderWidth: 1
                                text: modelData
                                clickedFunc: function() {
                                    var field = templateBodyField.textArea
                                    var first = Math.min(field.selectionStart, field.selectionEnd)
                                    var last = Math.max(field.selectionStart, field.selectionEnd)
                                    if (first !== last) field.remove(first, last)
                                    field.insert(first, modelData)
                                    field.forceActiveFocus()
                                }
                            }
                        }
                    }

                    TextAreaType {
                        id: templateBodyField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 240
                        placeholderText: qsTr("Write a sharing message")
                        textArea.readOnly: root.editingIsSystem
                        textArea.wrapMode: TextEdit.Wrap
                    }

                    BasicButtonType {
                        Layout.fillWidth: true
                        visible: !root.editingIsSystem
                        enabled: templateNameField.textField.text.trim().length > 0
                                 && templateBodyField.textArea.text.trim().length > 0
                        text: qsTr("Save template")
                        clickedFunc: function() {
                            ExportController.upsertShareTemplate(root.editingTemplateId,
                                                                 templateNameField.textField.text,
                                                                 templateBodyField.textArea.text,
                                                                 root.editingProtocol)
                            PageController.showNotificationMessage(qsTr("Template saved"))
                            templateEditor.closeTriggered()
                        }
                    }

                    BasicButtonType {
                        Layout.fillWidth: true
                        visible: Qt.platform.os === "windows"
                        text: qsTr("Open HTML preview")
                        clickedFunc: function() {
                            var html = ExportController.renderShareTemplateDraft(templateNameField.textField.text,
                                          templateBodyField.textArea.text, root.editingProtocol)
                            if (!ExportController.openHtmlPreview(html))
                                PageController.showNotificationMessage(qsTr("Could not open HTML preview"))
                        }
                    }

                    BasicButtonType {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 16
                        visible: root.editingTemplateId !== "" && !root.editingIsSystem
                        defaultColor: AmneziaStyle.color.transparent
                        hoveredColor: AmneziaStyle.color.translucentWhite
                        pressedColor: AmneziaStyle.color.sheerWhite
                        textColor: AmneziaStyle.color.paleGray
                        borderWidth: 1
                        text: qsTr("Delete template")
                        leftImageSource: "qrc:/images/controls/trash.svg"
                        clickedFunc: function() {
                            var templateIdToDelete = root.editingTemplateId
                            showQuestionDrawer(qsTr("Delete this template?"),
                                                    qsTr("This action cannot be undone."),
                                                    qsTr("Delete"), qsTr("Cancel"),
                                                    function() {
                                                        ExportController.deleteShareTemplate(templateIdToDelete)
                                                        templateEditor.closeTriggered()
                                                    }, function() {})
                        }
                    }
                }
            }
        }
    }
}
