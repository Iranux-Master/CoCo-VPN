import QtQuick
import Style 1.0

Text {
    // Explicit alignment mirrors with the window; natural alignment does not.
    horizontalAlignment: Text.AlignLeft
    lineHeight: 24 + LanguageUiController.getLineHeightAppend()
    lineHeightMode: Text.FixedHeight

    color: AmneziaStyle.color.paleGray
    font.pixelSize: 16
    font.weight: 400
    font.family: AmneziaStyle.uiFontFamily

    wrapMode: Text.WordWrap
}
