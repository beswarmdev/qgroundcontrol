#pragma once

#include <QtCore/QVariant>

#include "QmlUITestBase.h"

class Vehicle;

/// UI test for the Fly View fleet panel: vehicle list with link status, persisted collapse state, RTL ALL and
/// TAKEOFF ALL sending at once, LAND ALL and REBOOT ALL waiting for the confirmation overlay in the middle of the
/// screen, the editable resend fields, and the layout next to the multi-vehicle panel.
class FlyViewFleetPanelUITest : public QmlUITestBase
{
    Q_OBJECT

private slots:
    void init() override;
    void cleanup() override;

    void _testPanelListsVehiclesAndCollapses();
    void _testRtlAllSendsWithoutConfirmation();
    void _testLandAllConfirmsInCenterOverlay();
    void _testLandOverlayCancelSendsNothing();
    void _testTakeoffAllSendsWithoutConfirmation();
    void _testRebootNeedsTwoConfirmations();
    void _testResendFieldsEditSettings();
    void _testConfirmOverlayClosesWhenFleetEmpty();
    void _testFleetPanelLayoutWithMultiVehiclePanel();
    void _testFleetPanelStaysBelowCameraControls();
    void _testActionButtonsStayPutWhileRetrying();

private:
    /// Text of the visible item with \a objectName, or an empty string while the item is hidden or missing.
    QString _visibleItemText(const QString& objectName);

    /// objectName of a per-vehicle panel item, such as "fleetCommandStatus_<id>".
    static QString _vehicleObjectName(const QString& prefix, const Vehicle* vehicle);

    /// Takeoff makes the vehicle airborne, which creates QGCPressure and warns on hosts without a pressure backend.
    void _ignorePressureSensorNoise();

    QVariant _savedAttempts;
    QVariant _savedInterval;
};
