#pragma once

#include "QmlUITestBase.h"

class Vehicle;

/// UI test for the Fly View fleet panel: vehicle list with link status, persisted collapse state, RTL ALL sending
/// at once, LAND ALL waiting for the confirmation slider, and the layout next to the multi-vehicle panel.
class FlyViewFleetPanelUITest : public QmlUITestBase
{
    Q_OBJECT

private slots:
    void _testPanelListsVehiclesAndCollapses();
    void _testRtlAllSendsWithoutConfirmation();
    void _testLandAllRequiresConfirmation();
    void _testFleetPanelLayoutWithMultiVehiclePanel();

private:
    /// Text of the visible item with \a objectName, or an empty string while the item is hidden or missing.
    QString _visibleItemText(const QString& objectName);

    /// objectName of a per-vehicle panel item, such as "fleetCommandStatus_<id>".
    static QString _vehicleObjectName(const QString& prefix, const Vehicle* vehicle);
};
