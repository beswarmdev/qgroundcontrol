import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Confirmation for the fleet wide commands: a dimmed backdrop over the whole Fly View and a card with a blinking
/// frame in the middle of the screen. Steps are accepted one after the other, and the callback runs after the last.
Item {
    id: root
    objectName: "fleetConfirmOverlay"

    readonly property int currentStep: _currentStep
    readonly property int stepCount: _steps.length
    readonly property int secondStepDelayMs: 1500

    property var _steps: []
    property int _currentStep: 0
    property var _onAccepted: null
    property bool _acceptEnabled: true

    readonly property var _step: _currentStep > 0 ? _steps[_currentStep - 1] : undefined
    readonly property color _accentColor: _step ? _step.accentColor : qgcPal.colorRed
    readonly property real _frameWidth: ScreenTools.defaultFontPixelWidth * 0.6
    readonly property real _margin: ScreenTools.defaultFontPixelWidth * 2

    /// steps: array of 1 or 2 { title, message, confirmText, accentColor }. onAccepted runs once, after the last step.
    function show(steps, onAccepted) {
        unlockTimer.stop()
        _acceptEnabled = true
        _steps = steps
        _onAccepted = onAccepted
        _currentStep = steps.length > 0 ? 1 : 0
    }

    function cancel() {
        unlockTimer.stop()
        _currentStep = 0
        _steps = []
        _onAccepted = null
    }

    function _accept() {
        if (_currentStep < _steps.length) {
            // A double tap must not go through both steps
            _acceptEnabled = false
            _currentStep++
            unlockTimer.restart()
            return
        }

        const callback = _onAccepted
        cancel()
        if (callback) {
            callback()
        }
    }

    visible: _currentStep > 0

    QGCPalette { id: qgcPal }

    Timer {
        id: unlockTimer
        interval: root.secondStepDelayMs
        onTriggered: root._acceptEnabled = true
    }

    Rectangle {
        anchors.fill: parent
        color: qgcPal.window
        opacity: 0.7
    }

    DeadMouseArea {
        anchors.fill: parent
    }

    Rectangle {
        id: card
        objectName: "fleetConfirmCard"
        anchors.centerIn: parent
        width: Math.min(root.width * 0.8, ScreenTools.defaultFontPixelWidth * 60)
        height: cardColumn.implicitHeight + root._margin * 2 + root._frameWidth * 2
        color: qgcPal.window
        radius: ScreenTools.defaultFontPixelWidth

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            radius: card.radius
            border.width: root._frameWidth
            border.color: root._accentColor

            SequentialAnimation on opacity {
                running: root.visible
                loops: Animation.Infinite
                NumberAnimation { from: 1; to: 0.35; duration: 600 }
                NumberAnimation { from: 0.35; to: 1; duration: 600 }
            }
        }

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: root._margin + root._frameWidth
            spacing: root._margin

            QGCLabel {
                objectName: "fleetConfirmTitle"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.bold: true
                font.pointSize: ScreenTools.largeFontPointSize * 1.5
                color: root._accentColor
                text: root._step ? root._step.title : ""
            }

            QGCLabel {
                objectName: "fleetConfirmMessage"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: root._step ? root._step.message : ""
            }

            QGCLabel {
                objectName: "fleetConfirmStep"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                visible: root.stepCount === 2
                color: qgcPal.textSecondary
                text: qsTr("Step %1 of %2").arg(root.currentStep).arg(root.stepCount)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: root._margin

                QGCButton {
                    objectName: "fleetConfirmCancelButton"
                    Layout.fillWidth: true
                    pointSize: ScreenTools.largeFontPointSize
                    text: qsTr("Cancel")
                    onClicked: root.cancel()
                }

                QGCButton {
                    objectName: "fleetConfirmAcceptButton"
                    Layout.fillWidth: true
                    pointSize: ScreenTools.largeFontPointSize
                    enabled: root._acceptEnabled
                    opacity: enabled ? 1 : 0.4
                    backgroundColor: root._accentColor
                    textColor: root._accentColor.hslLightness > 0.5 ? qgcPal.mapWidgetBorderDark : qgcPal.mapWidgetBorderLight
                    text: root._acceptEnabled ? (root._step ? root._step.confirmText : "") : qsTr("Wait")
                    onClicked: root._accept()
                }
            }
        }
    }
}
