import QtQuick

import Style 1.0

Text {
    // Explicit alignment mirrors with the window; natural alignment does not.
    horizontalAlignment: Text.AlignLeft
    lineHeight: 18
    lineHeightMode: Text.FixedHeight

    color: AmneziaStyle.color.textTertiary
    font.pixelSize: 14
    font.weight: 400
    font.family: AmneziaStyle.uiFontFamily
    font.letterSpacing: 0

    wrapMode: Text.WordWrap
}
