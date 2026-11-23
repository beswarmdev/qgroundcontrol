#pragma once

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QVariant>

#include "MAVLinkLib.h"
#include "UnitTest.h"

class FleetCommandController;
class MockLink;
class Vehicle;

/// Drives FleetCommandController against several MockLink vehicles that the test connects and disconnects itself.
class FleetCommandControllerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void init() override;
    void cleanup() override;

    void _testCommandAllSucceeds_data();
    void _testCommandAllSucceeds();
    void _testDisarmedVehicleSkipped();
    void _testUnsupportedVehicle();
    void _testRetriesThenFails();
    void _testSucceedsWhenLinkReturns();
    void _testLandOverridesRtl();
    void _testVehicleRemovedWhileRetrying();
    void _testCancelStopsResending();
    void _testSendsEveryAttemptAfterConfirmation();
    void _testReassertsTargetMode();
    void _testSettingsSnapshotPerCommand();
    void _testIntervalFromSettings();
    void _testTakeoffAll_data();
    void _testTakeoffAll();
    void _testTakeoffSkipsAirborneAndUnsupported();
    void _testTakeoffStepTimeoutFails();
    void _testLandCancelsTakeoff();

private:
    MockLink* _startLink(MAV_AUTOPILOT autopilot);
    Vehicle* _connectVehicle(MockLink* link, bool arm);
    void _setCommLost(MockLink* link, bool lost);
    void _setAttempts(int attempts);
    void _setInterval(int seconds);
    void _ignoreUnansweredCommandNoise();
    void _ignorePressureSensorNoise();
    void _disconnectLinks();

    static int _intervalMs();
    static int _state(const FleetCommandController& controller, const Vehicle* vehicle);
    static int _attempt(const FleetCommandController& controller, const Vehicle* vehicle);
    static int _maxAttempts(const FleetCommandController& controller, const Vehicle* vehicle);
    static int _step(const FleetCommandController& controller, const Vehicle* vehicle);
    static bool _confirmed(const FleetCommandController& controller, const Vehicle* vehicle);
    static QVariantMap _entry(const FleetCommandController& controller, const Vehicle* vehicle);

    QList<QPointer<MockLink>> _links;
    QVariant _savedAttempts;
    QVariant _savedInterval;
};
