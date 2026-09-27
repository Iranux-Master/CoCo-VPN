import QtQuick

import Style 1.0

Text {
    // Explicit alignment mirrors with the window; natural alignment does not.
    horizontalAlignment: Text.AlignLeft
    lineHeight: 30 + LanguageUiController.getLineHeightAppend()
    lineHeightMode: Text.FixedHeight

    color: AmneziaStyle.color.paleGray
    font.pixelSize: 25
    font.weight: 700
    font.family: AmneziaStyle.uiFontFamily

    wrapMode: Text.WordWrap
}
