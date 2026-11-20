pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Collapsible list of every connected vehicle with its link state, plus the fleet wide RTL and Land buttons
Item {
    id: root
    objectName: "flyViewFleetPanel"

    property real maximumHeight

    readonly property bool expanded: _expanded
    readonly property alias controller: fleetController

    readonly property var _guidedController: globals.guidedControllerFlyView
    readonly property string _expandedSettingsKey: "FlyViewFleetPanelExpanded"
    readonly property real _margin: ScreenTools.defaultFontPixelWidth / 2
    readonly property real _dotSize: ScreenTools.defaultFontPixelHeight * 0.6
    readonly property var _vehicles: QGroundControl.multiVehicleManager.vehicles
    readonly property int _lostCount: _countLostVehicles()
    property bool _expanded: true

    function _countLostVehicles() {
        let lost = 0
        for (let i = 0; i < _vehicles.count; i++) {
            const vehicle = _vehicles.get(i)
            if (vehicle && vehicle.vehicleLinkManager.communicationLost) {
                lost++
            }
        }
        return lost
    }

    function _setExpanded(value) {
        _expanded = value
        QGroundControl.saveBoolGlobalSetting(_expandedSettingsKey, value)
    }

    function _commandName() {
        return fleetController.activeCommand === FleetCommandController.Command.Land ? qsTr("Land") : qsTr("RTL")
    }

    function _entryFor(vehicleId) {
        return fleetController.vehicleStatus[String(vehicleId)]
    }

    function _linkText(vehicle) {
        if (!vehicle) {
            return ""
        }
        const linkManager = vehicle.vehicleLinkManager
        const linkName = linkManager.primaryLinkName
        if (linkName === "") {
            return qsTr("No link")
        }
        if (linkManager.communicationLost) {
            return qsTr("%1 · Comm Lost").arg(linkName)
        }
        return qsTr("%1 · Loss %2%").arg(linkName).arg(vehicle.mavlinkLossPercent.toFixed(0))
    }

    function _statusText(entry) {
        if (!entry) {
            return ""
        }
        const command = _commandName()
        switch (entry.state) {
        case FleetCommandController.VehicleState.Pending:
            return qsTr("%1: attempt %2 of %3").arg(command).arg(entry.attempt).arg(entry.maxAttempts)
        case FleetCommandController.VehicleState.Succeeded:
            return qsTr("%1: confirmed").arg(command)
        case FleetCommandController.VehicleState.Failed:
            return qsTr("%1: failed after %2 attempts").arg(command).arg(entry.attempt)
        case FleetCommandController.VehicleState.Unsupported:
            return qsTr("%1: not supported").arg(command)
        case FleetCommandController.VehicleState.Skipped:
            return qsTr("%1: skipped (disarmed)").arg(command)
        case FleetCommandController.VehicleState.Cancelled:
            return qsTr("%1: stopped").arg(command)
        }
        return ""
    }

    function _statusColor(entry) {
        if (!entry) {
            return qgcPal.textSecondary
        }
        switch (entry.state) {
        case FleetCommandController.VehicleState.Pending:
            return qgcPal.warningText
        case FleetCommandController.VehicleState.Succeeded:
            return qgcPal.successText
        case FleetCommandController.VehicleState.Failed:
            return qgcPal.errorText
        }
        return qgcPal.textSecondary
    }

    function _summaryText() {
        const status = fleetController.vehicleStatus
        let total = 0
        let confirmed = 0
        let failed = 0
        for (const vehicleId in status) {
            total++
            if (status[vehicleId].state === FleetCommandController.VehicleState.Succeeded) {
                confirmed++
            } else if (status[vehicleId].state === FleetCommandController.VehicleState.Failed) {
                failed++
            }
        }
        return qsTr("%1: %2 of %3 confirmed, %4 failed").arg(_commandName()).arg(confirmed).arg(total).arg(failed)
    }

    width: ScreenTools.defaultFontPixelWidth * 36
    height: Math.min(maximumHeight, column.implicitHeight + _margin * 2)
    visible: !QGroundControl.videoManager.fullScreen && _vehicles.count > 0

    QGCPalette { id: qgcPal }

    FleetCommandController { id: fleetController }

    Rectangle {
        anchors.fill: parent
        color: qgcPal.window
        radius: ScreenTools.defaultFontPixelWidth / 2
        opacity: 0.75
    }

    DeadMouseArea {
        anchors.fill: parent
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: root._margin
        spacing: root._margin

        Item {
            id: headerItem
            objectName: "fleetPanelHeader"
            Layout.fillWidth: true
            implicitHeight: headerRow.implicitHeight

            RowLayout {
                id: headerRow
                anchors.fill: parent
                spacing: root._margin

                QGCLabel {
                    text: qsTr("Fleet (%1)").arg(root._vehicles.count)
                    font.bold: true
                }

                QGCLabel {
                    visible: root._lostCount > 0
                    text: qsTr("%1 lost").arg(root._lostCount)
                    color: qgcPal.colorRed
                }

                Item {
                    Layout.fillWidth: true
                }

                QGCColoredImage {
                    objectName: "fleetPanelToggle"
                    width: ScreenTools.defaultFontPixelHeight
                    height: width
                    source: root._expanded ? "/res/chevron-double-down.svg" : "/res/chevron-double-up.svg"
                    color: qgcPal.textPrimary
                    fillMode: Image.PreserveAspectFit
                    sourceSize.height: height
                }
            }

            QGCMouseArea {
                fillItem: headerItem
                onClicked: root._setExpanded(!root._expanded)
            }
        }

        QGCListView {
            id: vehicleList
            objectName: "fleetVehicleList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.min(contentHeight, ScreenTools.defaultFontPixelHeight * 12)
            visible: root._expanded
            clip: true
            spacing: root._margin / 2
            model: QGroundControl.multiVehicleManager.vehicles

            delegate: Rectangle {
                id: rowDelegate
                objectName: rowDelegate._vehicle ? "fleetVehicleRow_" + rowDelegate._vehicle.id : ""

                required property var object

                readonly property var _vehicle: object
                readonly property var _entry: rowDelegate._vehicle ? root._entryFor(rowDelegate._vehicle.id) : undefined
                readonly property bool _commLost: rowDelegate._vehicle ? rowDelegate._vehicle.vehicleLinkManager.communicationLost : false

                width: vehicleList.width
                height: rowColumn.implicitHeight + root._margin * 2
                color: qgcPal.windowCard
                radius: root._margin

                QGCMouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (rowDelegate._vehicle) {
                            QGroundControl.multiVehicleManager.activeVehicle = rowDelegate._vehicle
                        }
                    }
                }

                ColumnLayout {
                    id: rowColumn
                    anchors.fill: parent
                    anchors.margins: root._margin
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: root._margin

                        Rectangle {
                            objectName: rowDelegate._vehicle ? "fleetLinkDot_" + rowDelegate._vehicle.id : ""
                            Layout.preferredWidth: root._dotSize
                            Layout.preferredHeight: root._dotSize
                            radius: root._dotSize / 2
                            color: rowDelegate._commLost ? qgcPal.colorRed : qgcPal.colorGreen
                        }

                        QGCLabel {
                            text: rowDelegate._vehicle ? qsTr("Vehicle %1").arg(rowDelegate._vehicle.id) : ""
                        }

                        QGCLabel {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            text: rowDelegate._vehicle
                                  ? rowDelegate._vehicle.flightMode + " · " + (rowDelegate._vehicle.armed ? qsTr("Armed") : qsTr("Disarmed"))
                                  : ""
                        }
                    }

                    QGCLabel {
                        objectName: rowDelegate._vehicle ? "fleetLinkText_" + rowDelegate._vehicle.id : ""
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.pointSize: ScreenTools.smallFontPointSize
                        color: qgcPal.textSecondary
                        text: root._linkText(rowDelegate._vehicle)
                    }

                    QGCLabel {
                        objectName: rowDelegate._vehicle ? "fleetCommandStatus_" + rowDelegate._vehicle.id : ""
                        Layout.fillWidth: true
                        visible: rowDelegate._entry !== undefined
                        elide: Text.ElideRight
                        font.pointSize: ScreenTools.smallFontPointSize
                        color: root._statusColor(rowDelegate._entry)
                        text: root._statusText(rowDelegate._entry)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root._margin
            visible: root._expanded

            QGCButton {
                objectName: "fleetRtlAllButton"
                Layout.fillWidth: true
                text: qsTr("RTL ALL")
                backgroundColor: qgcPal.colorYellow
                textColor: qgcPal.mapWidgetBorderDark
                onClicked: fleetController.rtlAll()
            }

            QGCButton {
                objectName: "fleetLandAllButton"
                Layout.fillWidth: true
                text: qsTr("LAND ALL")
                backgroundColor: qgcPal.colorRed
                textColor: qgcPal.mapWidgetBorderLight
                onClicked: root._guidedController.confirmAction(root._guidedController.actionFleetLand, fleetController)
            }
        }

        QGCButton {
            objectName: "fleetStopButton"
            Layout.fillWidth: true
            visible: fleetController.busy
            text: qsTr("Stop retries")
            onClicked: fleetController.cancel()
        }

        QGCLabel {
            objectName: "fleetSummary"
            Layout.fillWidth: true
            visible: fleetController.activeCommand !== FleetCommandController.Command.None
            elide: Text.ElideRight
            text: root._summaryText()
        }
    }

    Component.onCompleted: _expanded = QGroundControl.loadBoolGlobalSetting(_expandedSettingsKey, true)
}
