import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PageEnum 1.0
import Style 1.0
import "./"
import "../Controls2"
import "../Controls2/TextTypes"

PageType {
    id: root
    property string selectedKind: "wireguard"
    property string selectedLanguage: ExportController.shareLanguage
    property string selectedTemplateId: ExportController.defaultShareTemplateId(selectedKind, selectedLanguage)
    property var kinds: ExportController.shareTemplateKinds
    property var templateOptions: ExportController.shareTemplates(selectedKind, selectedLanguage)

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
        anchors.left: parent.left
        anchors.right: parent.right
        contentHeight: content.implicitHeight + 24

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 14

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.topMargin: 18
                headerText: qsTr("Share Content Templates / قالب‌های محتوای اشتراک‌گذاری")
                descriptionText: qsTr("Choose the default language and edit the built-in guide for each access type. The guide is included when you save a sharing file.")
            }

            DropDownType {
                id: kindSelector
                Layout.fillWidth: true
                drawerParent: root
                drawerHeight: 0.55
                headerText: qsTr("Access type / protocol / service")
                text: root.kindName(root.selectedKind)
                listView: ListViewWithRadioButtonType {
                    rootWidth: root.width
                    model: root.kinds
                    selectedIndex: root.kindIndex(root.selectedKind)
                    clickedFunction: function() {
                        root.selectedKind = root.kinds[selectedIndex].id
                        root.loadSelectedTemplate()
                        kindSelector.text = root.kindName(root.selectedKind)
                        kindSelector.closeTriggered()
                    }
                }
            }

            DropDownType {
                id: templateSelector
                Layout.fillWidth: true
                drawerParent: root
                drawerHeight: 0.45
                headerText: qsTr("Default template")
                descriptionText: qsTr("Choose which template will be used for this access type and language.")
                text: root.templateName(root.selectedTemplateId)
                listView: ListViewWithRadioButtonType {
                    rootWidth: root.width
                    model: root.templateOptions
                    selectedIndex: root.templateIndex(root.selectedTemplateId)
                    clickedFunction: function() {
                        var chosen = root.templateOptions[selectedIndex]
                        root.selectedTemplateId = chosen.id
                        ExportController.setDefaultShareTemplate(root.selectedKind, root.selectedLanguage, chosen.id)
                        templateNameField.textField.text = chosen.builtIn ? "" : chosen.name
                        templateEditor.textArea.text = chosen.body
                        templateSelector.text = chosen.name
                        templateSelector.closeTriggered()
                    }
                }
            }

            Header2Type {
                Layout.fillWidth: true
                headerText: qsTr("Default sharing language")
                descriptionText: qsTr("This language is used for new HTML and TXT share files.")
            }
            RowLayout {
                Layout.fillWidth: true
                HorizontalRadioButton {
                    Layout.fillWidth: true
                    checked: root.selectedLanguage === "fa"
                    text: qsTr("فارسی")
                    onClicked: root.changeLanguage("fa")
                }
                HorizontalRadioButton {
                    Layout.fillWidth: true
                    checked: root.selectedLanguage === "en"
                    text: qsTr("English")
                    onClicked: root.changeLanguage("en")
                }
            }

            ParagraphTextType {
                Layout.fillWidth: true
                text: qsTr("Tags: {{PROTOCOL}}, {{APP_LINK}}, {{SERVER}}, {{NAME}}, {{CREATED}}, {{CONFIG}}, {{QR}}")
                color: AmneziaStyle.color.mutedGray
            }

            TextAreaType {
                id: templateEditor
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                textArea.wrapMode: TextEdit.Wrap
                textArea.text: ExportController.shareTemplate(root.selectedKind, root.selectedLanguage)
            }

            TextFieldWithHeaderType {
                id: templateNameField
                Layout.fillWidth: true
                headerText: qsTr("Your template name")
                placeholderText: qsTr("Name a personal template")
                textField.text: root.templateName(root.selectedTemplateId) === qsTr("Built-in default") ? "" : root.templateName(root.selectedTemplateId)
            }

            BasicButtonType {
                Layout.fillWidth: true
                text: qsTr("Save template")
                leftImageSource: "qrc:/images/controls/save.svg"
                clickedFunc: function() {
                    if (root.selectedTemplateId === "builtin") {
                        ExportController.saveShareTemplate(root.selectedKind, root.selectedLanguage, templateEditor.textArea.text)
                    } else {
                        var id = ExportController.saveCustomShareTemplate(root.selectedKind, root.selectedLanguage,
                                    root.selectedTemplateId, templateNameField.textField.text, templateEditor.textArea.text)
                        if (id !== "") {
                            root.selectedTemplateId = id
                            root.templateOptions = ExportController.shareTemplates(root.selectedKind, root.selectedLanguage)
                            templateSelector.text = root.templateName(id)
                        }
                    }
                    PageController.showNotificationMessage(qsTr("Template saved"))
                }
            }
            BasicButtonType {
                Layout.fillWidth: true
                enabled: templateNameField.textField.text.trim().length > 0 && templateEditor.textArea.text.trim().length > 0
                text: qsTr("Save as a new personal template")
                clickedFunc: function() {
                    var id = ExportController.saveCustomShareTemplate(root.selectedKind, root.selectedLanguage,
                                "", templateNameField.textField.text, templateEditor.textArea.text)
                    if (id === "") return
                    root.selectedTemplateId = id
                    ExportController.setDefaultShareTemplate(root.selectedKind, root.selectedLanguage, id)
                    root.templateOptions = ExportController.shareTemplates(root.selectedKind, root.selectedLanguage)
                    templateSelector.text = root.templateName(id)
                    PageController.showNotificationMessage(qsTr("Personal template saved and selected as default"))
                }
            }
            BasicButtonType {
                Layout.fillWidth: true
                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderWidth: 1
                text: qsTr("Restore built-in template")
                clickedFunc: function() {
                    ExportController.resetShareTemplate(root.selectedKind, root.selectedLanguage)
                    ExportController.setDefaultShareTemplate(root.selectedKind, root.selectedLanguage, "builtin")
                    root.loadSelectedTemplate()
                }
            }
        }
    }

    function kindIndex(id) {
        for (var i = 0; i < kinds.length; ++i) if (kinds[i].id === id) return i
        return 0
    }
    function kindName(id) {
        for (var i = 0; i < kinds.length; ++i) if (kinds[i].id === id) return kinds[i].name
        return "WireGuard"
    }
    function templateIndex(id) {
        for (var i = 0; i < templateOptions.length; ++i) if (templateOptions[i].id === id) return i
        return 0
    }
    function templateName(id) {
        for (var i = 0; i < templateOptions.length; ++i) if (templateOptions[i].id === id) return templateOptions[i].name
        return qsTr("Built-in default")
    }
    function loadSelectedTemplate() {
        templateOptions = ExportController.shareTemplates(selectedKind, selectedLanguage)
        selectedTemplateId = ExportController.defaultShareTemplateId(selectedKind, selectedLanguage)
        var selected = templateOptions[templateIndex(selectedTemplateId)]
        if (!selected) return
        templateEditor.textArea.text = selected.body
        templateNameField.textField.text = selected.builtIn ? "" : selected.name
        templateSelector.text = selected.name
    }
    function changeLanguage(language) {
        selectedLanguage = language
        ExportController.setShareLanguage(language)
        root.loadSelectedTemplate()
    }
}
