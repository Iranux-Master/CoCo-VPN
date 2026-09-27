import QtQuick

import Style 1.0

Text {
    // Explicit alignment mirrors with the window; natural alignment does not.
    horizontalAlignment: Text.AlignLeft
    lineHeight: 16 + LanguageUiController.getLineHeightAppend()
    lineHeightMode: Text.FixedHeight

    color: AmneziaStyle.color.midnightBlack
    font.pixelSize: 13
    font.weight: 400
    font.family: AmneziaStyle.uiFontFamily
    font.letterSpacing: AmneziaStyle.persianUi ? 0 : 0.02

    wrapMode: Text.Wrap
}
