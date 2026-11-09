import QtQuick

import QGroundControl
import QGroundControl.Controls

PreFlightCheckButton {
    name:                   qsTr("Sound output")
    manualText:             qsTr("%1 audio output enabled. System audio output enabled, too?").arg(QGroundControl.appName)
    telemetryTextFailure:   qsTr("%1 audio output is disabled. Please enable it under application settings->general to hear audio warnings!").arg(QGroundControl.appName)
    telemetryFailure:       _audioMuted || _audioVolume <= 0.0

    property real _audioVolume: QGroundControl.settingsManager.appSettings.audioVolume.rawValue
    property bool _audioMuted:  QGroundControl.settingsManager.appSettings.audioMuted.rawValue
}
