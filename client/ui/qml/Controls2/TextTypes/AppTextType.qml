import QtQuick

import Style 1.0

Text {
    // Explicit alignment mirrors with the window; natural alignment does not.
    horizontalAlignment: Text.AlignLeft
    lineHeight: 24
    lineHeightMode: Text.FixedHeight

    color: AmneziaStyle.color.textPrimary
    font.pixelSize: 16
    font.weight: 400
    font.family: AmneziaStyle.uiFontFamily
    font.letterSpacing: AmneziaStyle.persianUi ? 0 : -0.4

    wrapMode: Text.WordWrap
}
