import QtQuick

import Style 1.0

Text {
    // Explicit alignment mirrors with the window; natural alignment does not.
    horizontalAlignment: Text.AlignLeft
    color: AmneziaStyle.color.textStaticWhite
    font.pixelSize: 36
    font.weight: 500
    font.family: AmneziaStyle.uiFontFamily
    font.letterSpacing: AmneziaStyle.persianUi ? 0 : -1.8
}
