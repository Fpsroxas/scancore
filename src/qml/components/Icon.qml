import QtQuick
import Scancore

// A Lucide icon tinted to a colour.
Image {
    id: icon
    property string name
    property color color: Theme.text
    property int size: 18

    width: size
    height: size
    source: name ? App.icon(name, color.toString()) : ""
    sourceSize: Qt.size(Math.ceil(size * Screen.devicePixelRatio), Math.ceil(size * Screen.devicePixelRatio))
    smooth: true
    asynchronous: false
    cache: true
}
