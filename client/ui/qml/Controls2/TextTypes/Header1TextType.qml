import QtQuick

import Style 1.0

Text {
    // Explicit alignment mirrors with the window; natural alignment does not.
    horizontalAlignment: Text.AlignLeft
    lineHeight: 38 + LanguageUiController.getLineHeightAppend()
    lineHeightMode: Text.FixedHeight

    color: AmneziaStyle.color.paleGray
    font.pixelSize: 32
    font.weight: 700
    font.family: AmneziaStyle.uiFontFamily
    font.letterSpacing: AmneziaStyle.persianUi ? 0 : -1.0

    wrapMode: Text.WordWrap
}

