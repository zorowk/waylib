// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Waylib.Server
import Live

Item {
    id :root

    Shortcut {
        sequences: [StandardKey.Quit]
        context: Qt.ApplicationShortcut
        onActivated: {
            Qt.quit()
        }
    }

    OutputRenderWindow {
        id: renderWindow

        width: outputsContainer.implicitWidth
        height: outputsContainer.implicitHeight

        Row {
            id: outputsContainer

            anchors.fill: parent

            DynamicCreatorComponent {
                id: outputDelegateCreator
                creator: Helper.outputCreator

                OutputItem {
                    id: rootOutputItem
                    required property WaylandOutput waylandOutput
                    readonly property OutputViewport onscreenViewport: outputViewport

                    output: waylandOutput
                    devicePixelRatio: waylandOutput.scale
                    layout: outputLayout
                    cursorDelegate: Item {
                        required property OutputCursor cursor

                        visible: cursor.visible && !cursor.isHardwareCursor
                        width: cursor.size.width
                        height: cursor.size.height

                        Image {
                            id: cursorImage
                            source: cursor.imageSource
                            x: -cursor.hotspot.x
                            y: -cursor.hotspot.y
                            cache: false
                            width: cursor.size.width
                            height: cursor.size.height
                            sourceClipRect: cursor.sourceRect
                        }
                    }

                    OutputViewport {
                        id: outputViewport
                        input: contents
                        output: waylandOutput
                        anchors.centerIn: parent
                    }

                    Item {
                        id: contents
                        anchors.fill: parent
                        Rectangle {
                            anchors.fill: parent
                            color: "black"
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "'Ctrl+Q' quit"
                            font.pointSize: 40
                            color: "white"
                        }
                        DynamicCreatorComponent {
                            id: toplevelComponent
                            creator: Helper.xdgShellCreator
                            chooserRole: "type"
                            chooserRoleValue: "toplevel"
                            XdgSurfaceItem {
                                id: toplevelSurfaceItem
                                required property WaylandXdgSurface waylandSurface
                                shellSurface: waylandSurface
                                Button {
                                    id: button
                                    text: "Freeze"
                                    onClicked: {
                                        const freeze = (text === "Freeze")
                                        toplevelSurfaceItem.contentItem.live = !freeze
                                        if (freeze) {
                                            text = "Unfreeze"
                                        } else {
                                            text = "Freeze"
                                        }
                                    }
                                }
                                topPadding: button.height
                                anchors.centerIn: parent
                            }
                        }
                    }
                }
            }
        }
    }
}
