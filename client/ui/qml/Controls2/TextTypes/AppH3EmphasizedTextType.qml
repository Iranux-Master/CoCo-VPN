import QtQuick

import Style 1.0

Text {
    // Explicit alignment mirrors with the window; natural alignment does not.
    horizontalAlignment: Text.AlignLeft
    lineHeight: 24
    lineHeightMode: Text.FixedHeight

    color: AmneziaStyle.color.textPrimary
    font.pixelSize: 18
    font.weight: 600
    font.family: AmneziaStyle.uiFontFamily
    font.letterSpacing: 0

    wrapMode: Text.WordWrap
}
