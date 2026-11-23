pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls

/// Collapsible list of every connected vehicle with its link state, plus the fleet wide Takeoff, Reboot, RTL and Land buttons
Item {
    id: root
    objectName: "flyViewFleetPanel"

    property real maximumHeight
    /// FleetConfirmOverlay which asks for the confirmation of Land and Reboot
    property var confirmOverlay

    readonly property bool expanded: _expanded
    readonly property alias controller: fleetController

    readonly property string _expandedSettingsKey: "FlyViewFleetPanelExpanded"
    readonly property real _margin: ScreenTools.defaultFontPixelWidth / 2
    readonly property real _dotSize: ScreenTools.defaultFontPixelHeight * 0.6
    readonly property real _fieldWidth: ScreenTools.defaultFontPixelWidth * 6
    readonly property var _vehicles: QGroundControl.multiVehicleManager.vehicles
    readonly property var _flyViewSettings: QGroundControl.settingsManager.flyViewSettings
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

    function _countArmedVehicles() {
        let armed = 0
        for (let i = 0; i < _vehicles.count; i++) {
            const vehicle = _vehicles.get(i)
            if (vehicle && vehicle.armed) {
                armed++
            }
        }
        return armed
    }

    function _setExpanded(value) {
        _expanded = value
        QGroundControl.saveBoolGlobalSetting(_expandedSettingsKey, value)
    }

    function _confirmLand() {
        if (!confirmOverlay) {
            return
        }
        confirmOverlay.show([{
            title: qsTr("LAND ALL VEHICLES"),
            message: qsTr("%1 armed vehicles will land at their current position.").arg(_countArmedVehicles()),
            confirmText: qsTr("LAND ALL"),
            accentColor: qgcPal.colorRed
        }], function() { fleetController.landAll() })
    }

    function _confirmReboot() {
        if (!confirmOverlay) {
            return
        }
        const count = _vehicles.count
        const armed = _countArmedVehicles()
        let message = qsTr("%1 vehicles will reboot their flight controller and lose their link until it restarts.").arg(count)
        if (armed > 0) {
            message += "\n" + qsTr("WARNING: %1 of them are ARMED.").arg(armed)
        }
        confirmOverlay.show([{
            title: qsTr("REBOOT ALL AUTOPILOTS"),
            message: message,
            confirmText: qsTr("REBOOT ALL"),
            accentColor: qgcPal.colorOrange
        }, {
            title: qsTr("CONFIRM REBOOT"),
            message: qsTr("Second confirmation: reboot %1 autopilots now?").arg(count),
            confirmText: qsTr("YES, REBOOT NOW"),
            accentColor: qgcPal.colorRed
        }], function() { fleetController.rebootAll() })
    }

    function _commandName() {
        switch (fleetController.activeCommand) {
        case FleetCommandController.Command.Land:
            return qsTr("Land")
        case FleetCommandController.Command.Takeoff:
            return qsTr("Takeoff")
        case FleetCommandController.Command.Reboot:
            return qsTr("Reboot")
        }
        return qsTr("RTL")
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

    function _resendStatusText(entry) {
        const command = _commandName()
        switch (entry.state) {
        case FleetCommandController.VehicleState.Pending:
            return entry.confirmed
                    ? qsTr("%1: sent %2 of %3, confirmed").arg(command).arg(entry.attempt).arg(entry.maxAttempts)
                    : qsTr("%1: sent %2 of %3").arg(command).arg(entry.attempt).arg(entry.maxAttempts)
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

    function _takeoffStatusText(entry) {
        switch (entry.state) {
        case FleetCommandController.VehicleState.Pending:
            switch (entry.step) {
            case FleetCommandController.TakeoffStep.Mode:
                return qsTr("Takeoff: setting mode")
            case FleetCommandController.TakeoffStep.Arm:
                return qsTr("Takeoff: arming")
            }
            return qsTr("Takeoff: climbing to %1 m").arg(fleetController.takeoffAltitude)
        case FleetCommandController.VehicleState.Succeeded:
            return qsTr("Takeoff: airborne")
        case FleetCommandController.VehicleState.Failed:
            switch (entry.step) {
            case FleetCommandController.TakeoffStep.Mode:
                return qsTr("Takeoff: failed (mode)")
            case FleetCommandController.TakeoffStep.Arm:
                return qsTr("Takeoff: failed (arming)")
            }
            return qsTr("Takeoff: failed (no climb)")
        case FleetCommandController.VehicleState.Unsupported:
            return qsTr("Takeoff: not supported")
        case FleetCommandController.VehicleState.Skipped:
            return qsTr("Takeoff: skipped (airborne)")
        case FleetCommandController.VehicleState.Cancelled:
            return qsTr("Takeoff: stopped")
        }
        return ""
    }

    function _rebootStatusText(entry) {
        switch (entry.state) {
        case FleetCommandController.VehicleState.Pending:
            return qsTr("Reboot: sent")
        case FleetCommandController.VehicleState.Succeeded:
            return qsTr("Reboot: accepted")
        case FleetCommandController.VehicleState.Failed:
            return qsTr("Reboot: failed")
        case FleetCommandController.VehicleState.Cancelled:
            return qsTr("Reboot: stopped")
        }
        return ""
    }

    function _statusText(entry) {
        if (!entry) {
            return ""
        }
        switch (fleetController.activeCommand) {
        case FleetCommandController.Command.Takeoff:
            return _takeoffStatusText(entry)
        case FleetCommandController.Command.Reboot:
            return _rebootStatusText(entry)
        }
        return _resendStatusText(entry)
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

    Connections {
        target: root._vehicles

        function onCountChanged() {
            // Nothing is left to command, so a pending confirmation has no meaning
            if (root._vehicles.count === 0 && root.confirmOverlay) {
                root.confirmOverlay.cancel()
            }
        }
    }

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

        // Above the action rows: the panel grows upwards, so the buttons stay where the finger last was
        QGCLabel {
            objectName: "fleetSummary"
            Layout.fillWidth: true
            visible: fleetController.activeCommand !== FleetCommandController.Command.None
            elide: Text.ElideRight
            text: root._summaryText()
        }

        QGCButton {
            objectName: "fleetStopButton"
            Layout.fillWidth: true
            visible: fleetController.busy
            text: qsTr("Stop")
            onClicked: fleetController.cancel()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root._margin
            visible: root._expanded

            QGCButton {
                objectName: "fleetTakeoffAllButton"
                Layout.fillWidth: true
                text: qsTr("TAKEOFF ALL %1 m").arg(fleetController.takeoffAltitude)
                backgroundColor: qgcPal.colorGreen
                textColor: qgcPal.mapWidgetBorderDark
                onClicked: fleetController.takeoffAll()
            }

            QGCButton {
                objectName: "fleetRebootAllButton"
                Layout.fillWidth: true
                text: qsTr("REBOOT ALL")
                backgroundColor: qgcPal.colorOrange
                textColor: qgcPal.mapWidgetBorderDark
                onClicked: root._confirmReboot()
            }
        }

        RowLayout {
            objectName: "fleetResendRow"
            Layout.fillWidth: true
            spacing: root._margin
            visible: root._expanded

            QGCLabel {
                text: qsTr("Send")
            }

            FactTextField {
                objectName: "fleetAttemptsField"
                Layout.preferredWidth: root._fieldWidth
                fact: root._flyViewSettings.fleetCommandAttempts
            }

            QGCLabel {
                text: qsTr("times, every")
            }

            FactTextField {
                objectName: "fleetIntervalField"
                Layout.preferredWidth: root._fieldWidth
                fact: root._flyViewSettings.fleetCommandIntervalSec
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
                onClicked: root._confirmLand()
            }
        }
    }

    Component.onCompleted: _expanded = QGroundControl.loadBoolGlobalSetting(_expandedSettingsKey, true)
}
