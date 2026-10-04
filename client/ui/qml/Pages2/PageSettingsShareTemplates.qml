import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"

PageType {
    id: root

    property string editingTemplateId: ""
    property string editingProtocol: "general"
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

    function openNewTemplate() {
        editingTemplateId = ""
        editingProtocol = "general"
        templateNameField.textField.text = ""
        templateBodyField.textArea.text = "سلام {{NAME}}،\nسرور: {{SERVER}}\nروش اتصال: {{PROTOCOLS}}\n{{CONFIGS}}\n{{QR}}"
        templateProtocolSelector.text = protocolName(editingProtocol)
        templateEditor.openTriggered()
    }

    function openTemplate(item) {
        if (item.isSystem) return
        editingTemplateId = item.id
        editingProtocol = item.protocol || "general"
        templateNameField.textField.text = item.name
        templateBodyField.textArea.text = item.body
        templateProtocolSelector.text = protocolName(editingProtocol)
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
                    property var availableTemplates: ExportController.templatesForProtocol(protocolKey)
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    drawerParent: root
                    fitContent: true
                    drawerHeight: 0.55
                    descriptionText: modelData.name
                    headerText: qsTr("Default template for %1").arg(modelData.name)
                    text: ExportController.shareTemplateName(ExportController.defaultShareTemplateId(protocolKey))

                    listView: ListViewWithRadioButtonType {
                        rootWidth: root.width
                        model: defaultSelector.availableTemplates
                        currentValue: ExportController.shareTemplateName(
                                          ExportController.defaultShareTemplateId(defaultSelector.protocolKey))
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
                clickedFunc: root.openNewTemplate
            }

            Repeater {
                model: ExportController.shareTemplates

                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 0

                    LabelWithButtonType {
                        Layout.fillWidth: true
                        text: modelData.name
                        descriptionText: root.protocolName(modelData.protocol)
                                         + " · " + (modelData.isSystem ? qsTr("System template") : qsTr("Custom template"))
                        leftImageSource: modelData.isSystem
                                         ? "qrc:/images/controls/file-check-2.svg"
                                         : "qrc:/images/controls/edit-3.svg"
                        rightImageSource: modelData.isSystem ? "" : "qrc:/images/controls/chevron-right.svg"
                        clickedFunction: function() { root.openTemplate(modelData) }
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

        expandedStateContent: ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 16
            spacing: 0

            BackButtonType {
                Layout.leftMargin: 16
                backButtonFunction: function() { templateEditor.closeTriggered() }
            }

            Header2Type {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16
                headerText: root.editingTemplateId === "" ? qsTr("Create template") : qsTr("Edit template")
                descriptionText: qsTr("Click a tag to insert it at the current cursor position.")
            }

            ScrollView {
                id: editorScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                contentWidth: availableWidth
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: editorScroll.availableWidth
                    spacing: 12

                    TextFieldWithHeaderType {
                        id: templateNameField
                        Layout.fillWidth: true
                        headerText: qsTr("Template name")
                        placeholderText: qsTr("Enter a name for this template")
                    }

                    DropDownType {
                        id: templateProtocolSelector
                        Layout.fillWidth: true
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
                        textArea.wrapMode: TextEdit.Wrap
                    }

                    BasicButtonType {
                        Layout.fillWidth: true
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
                        Layout.bottomMargin: 16
                        visible: root.editingTemplateId !== ""
                        defaultColor: AmneziaStyle.color.transparent
                        hoveredColor: AmneziaStyle.color.translucentWhite
                        pressedColor: AmneziaStyle.color.sheerWhite
                        textColor: AmneziaStyle.color.paleGray
                        borderWidth: 1
                        text: qsTr("Delete template")
                        leftImageSource: "qrc:/images/controls/trash.svg"
                        clickedFunc: function() {
                            root.showQuestionDrawer(qsTr("Delete this template?"),
                                                    qsTr("This action cannot be undone."),
                                                    qsTr("Delete"), qsTr("Cancel"),
                                                    function() {
                                                        ExportController.deleteShareTemplate(root.editingTemplateId)
                                                        templateEditor.closeTriggered()
                                                    }, function() {})
                        }
                    }
                }
            }
        }
    }
}
