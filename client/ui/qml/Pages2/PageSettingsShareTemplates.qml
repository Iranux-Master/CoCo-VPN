import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import PageEnum 1.0
import Style 1.0
import "./"
import "../Controls2"
import "../Controls2/TextTypes"

PageType {
    id: root
    property string selectedKind: "wireguard"
    property string selectedLanguage: LanguageUiController.persianUi ? "fa" : "en"
    property string selectedTemplateId: ExportController.defaultShareTemplateId(selectedKind, selectedLanguage)
    property var kinds: ExportController.shareTemplateKinds
    property var templateOptions: ExportController.shareTemplates(selectedKind, selectedLanguage)

    ListModel { id: kindMenuModel }
    ListModel { id: templateMenuModel }

    Component.onCompleted: {
        syncKindMenu()
        syncTemplateMenu()
        ExportController.setShareLanguage(selectedLanguage)
    }

    Connections {
        target: LanguageUiController
        function onTranslationsUpdated() {
            root.selectedLanguage = LanguageUiController.persianUi ? "fa" : "en"
            ExportController.setShareLanguage(root.selectedLanguage)
            root.loadSelectedTemplate()
        }
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
                headerText: qsTr("Share Content Templates")
                descriptionText: qsTr("Edit the sharing guide used for each access type. The guide follows the app language and is included in HTML and TXT files.")
            }

            DropDownType {
                id: kindSelector
                Layout.fillWidth: true
                drawerParent: root
                drawerHeight: 0.55
                headerText: qsTr("Access type, protocol, or service")
                text: root.kindName(root.selectedKind)
                listView: ListViewWithRadioButtonType {
                    rootWidth: root.width
                    model: kindMenuModel
                    selectedIndex: root.kindIndex(root.selectedKind)
                    clickedFunction: function() {
                        root.selectedKind = kindMenuModel.get(selectedIndex).kindId
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
                descriptionText: qsTr("Choose the template used for this access type.")
                text: root.templateName(root.selectedTemplateId)
                listView: ListViewWithRadioButtonType {
                    rootWidth: root.width
                    model: templateMenuModel
                    selectedIndex: root.templateIndex(root.selectedTemplateId)
                    clickedFunction: function() {
                        var chosen = templateMenuModel.get(selectedIndex)
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
                headerText: qsTr("Insert a tag")
                descriptionText: qsTr("Click a tag to insert it at the cursor position in your template.")
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: [
                        { label: qsTr("Protocol"), tag: "{{PROTOCOL}}" },
                        { label: qsTr("App link"), tag: "{{APP_LINK}}" },
                        { label: qsTr("Server"), tag: "{{SERVER}}" },
                        { label: qsTr("Account name"), tag: "{{NAME}}" },
                        { label: qsTr("Creation date"), tag: "{{CREATED}}" },
                        { label: qsTr("Configuration"), tag: "{{CONFIG}}" },
                        { label: qsTr("QR code"), tag: "{{QR}}" }
                    ]

                    delegate: BasicButtonType {
                        text: modelData.label
                        defaultColor: AmneziaStyle.color.transparent
                        hoveredColor: AmneziaStyle.color.translucentWhite
                        pressedColor: AmneziaStyle.color.sheerWhite
                        borderWidth: 1
                        clickedFunc: function() { root.insertTemplateTag(modelData.tag) }
                    }
                }
            }

            TextAreaType {
                id: templateEditor
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                textArea.wrapMode: TextEdit.Wrap
                textArea.text: ExportController.shareTemplate(root.selectedKind, root.selectedLanguage)
            }

            Header2Type {
                Layout.fillWidth: true
                headerText: qsTr("Download a preview")
                descriptionText: qsTr("Preview files use sample values. Real account details and QR codes are added when you share an account.")
            }

            BasicButtonType {
                Layout.fillWidth: true
                text: qsTr("Download HTML preview")
                leftImageSource: "qrc:/images/controls/save.svg"
                clickedFunc: function() { root.downloadTemplatePreview(true) }
            }

            BasicButtonType {
                Layout.fillWidth: true
                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                borderWidth: 1
                text: qsTr("Download TXT preview")
                clickedFunc: function() { root.downloadTemplatePreview(false) }
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
                            root.syncTemplateMenu()
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
                    root.syncTemplateMenu()
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
        syncTemplateMenu()
        selectedTemplateId = ExportController.defaultShareTemplateId(selectedKind, selectedLanguage)
        var selected = templateOptions[templateIndex(selectedTemplateId)]
        if (!selected) return
        templateEditor.textArea.text = selected.body
        templateNameField.textField.text = selected.builtIn ? "" : selected.name
        templateSelector.text = selected.name
    }
    function syncKindMenu() {
        kindMenuModel.clear()
        for (var i = 0; i < kinds.length; ++i)
            kindMenuModel.append({ kindId: kinds[i].id, name: kinds[i].name })
    }
    function syncTemplateMenu() {
        templateMenuModel.clear()
        for (var i = 0; i < templateOptions.length; ++i)
            templateMenuModel.append({ id: templateOptions[i].id, name: templateOptions[i].name,
                                       body: templateOptions[i].body, builtIn: templateOptions[i].builtIn })
    }
    function insertTemplateTag(tag) {
        var editor = templateEditor.textArea
        var position = editor.cursorPosition
        editor.insert(position, tag)
        editor.cursorPosition = position + tag.length
        editor.forceActiveFocus()
    }
    function downloadTemplatePreview(asHtml) {
        var extension = asHtml ? "html" : "txt"
        var filter = asHtml ? qsTr("HTML files (*.html)") : qsTr("Text files (*.txt)")
        var fileName = SystemController.getFileName(qsTr("Save template preview"), filter,
            StandardPaths.standardLocations(StandardPaths.DocumentsLocation) + "/CoCoVPN_" + selectedKind + "_template_preview." + extension,
            true, extension)
        if (fileName === "") return
        if (ExportController.saveShareTemplatePreview(fileName, selectedKind, selectedLanguage,
                                                       templateEditor.textArea.text, asHtml))
            PageController.showNotificationMessage(qsTr("Template preview saved"))
        else
            PageController.showNotificationMessage(qsTr("Could not save the template preview"))
    }
}
