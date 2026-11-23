#include "FleetCommandControllerTest.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QRegularExpression>
#include <QtTest/QSignalSpy>

#include "Fact.h"
#include "FleetCommandController.h"
#include "FlyViewSettings.h"
#include "LinkManager.h"
#include "MockLink.h"
#include "MultiVehicleManager.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "Vehicle.h"
#include "VehicleReadiness.h"

void FleetCommandControllerTest::init()
{
    UnitTest::init();

    // The ArduPilot MockLink rejects the gimbal probe that follows the initial connect, and its retries give up
    // at a time that depends on how fast the test runs
    ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Giving up sending command after max retries: "
                                                       "MAV_CMD_REQUEST_MESSAGE message: GIMBAL_MANAGER_INFORMATION")));

    MultiVehicleManager::instance()->init();
    _savedAttempts = SettingsManager::instance()->flyViewSettings()->fleetCommandAttempts()->rawValue();
    _savedInterval = SettingsManager::instance()->flyViewSettings()->fleetCommandIntervalSec()->rawValue();

    // One send per vehicle, one second apart, unless the test asks for more
    _setAttempts(1);
    _setInterval(1);
}

void FleetCommandControllerTest::cleanup()
{
    dumpFailureContextIfTestFailed(QStringLiteral("before FleetCommandControllerTest teardown"));
    _disconnectLinks();
    SettingsManager::instance()->flyViewSettings()->fleetCommandAttempts()->setRawValue(_savedAttempts);
    SettingsManager::instance()->flyViewSettings()->fleetCommandIntervalSec()->setRawValue(_savedInterval);
    UnitTest::cleanup();
}

MockLink* FleetCommandControllerTest::_startLink(MAV_AUTOPILOT autopilot)
{
    switch (autopilot) {
        case MAV_AUTOPILOT_PX4:
            return MockLink::startPX4MockLink();
        case MAV_AUTOPILOT_ARDUPILOTMEGA:
            return MockLink::startAPMArduCopterMockLink();
        default:
            return nullptr;
    }
}

Vehicle* FleetCommandControllerTest::_connectVehicle(MockLink* link, bool arm)
{
    if (!link) {
        return nullptr;
    }

    MultiVehicleManager* const manager = MultiVehicleManager::instance();

    // Every vehicle after the first announces itself with an app message.
    const bool additionalVehicle = manager->vehicles()->count() > 0;
    if (additionalVehicle) {
        expectAppMessage(QRegularExpression(QStringLiteral("Connected to Vehicle [0-9]+")));
    }

    _links.append(link);
    _sharedLinks.append(LinkManager::instance()->sharedLinkInterfacePointerForLink(link));

    const quint32 vehicleId = link->vehicleId();
    if (!waitForCondition([manager, vehicleId] { return manager->getVehicleById(vehicleId) != nullptr; },
                          TestTimeout::longMs(), QStringLiteral("vehicle added"))) {
        return nullptr;
    }
    if (additionalVehicle) {
        verifyExpectedLogMessage();
    }

    Vehicle* const vehicle = manager->getVehicleById(vehicleId);
    if (!waitForCondition([vehicle] { return vehicle->isInitialConnectComplete(); }, TestTimeout::longMs(),
                          QStringLiteral("initial connect complete"))) {
        return nullptr;
    }

    if (arm) {
        vehicle->setArmed(true, false);
        if (!waitForCondition([vehicle] { return vehicle->armed(); }, TestTimeout::longMs(),
                              QStringLiteral("vehicle armed"))) {
            return nullptr;
        }
    }

    return vehicle;
}

void FleetCommandControllerTest::_setCommLost(MockLink* link, bool lost)
{
    link->setCommLost(lost);
    if (lost) {
        // Deliver what the worker sent before the flag took effect, so no heartbeat arrives after this returns
        link->waitForWorkerIdle();
        QCoreApplication::sendPostedEvents();
    }
}

void FleetCommandControllerTest::_setAttempts(int attempts)
{
    SettingsManager::instance()->flyViewSettings()->fleetCommandAttempts()->setRawValue(attempts);
}

void FleetCommandControllerTest::_setInterval(int seconds)
{
    SettingsManager::instance()->flyViewSettings()->fleetCommandIntervalSec()->setRawValue(seconds);
}

int FleetCommandControllerTest::_intervalMs()
{
    return SettingsManager::instance()->flyViewSettings()->fleetCommandIntervalSec()->rawValue().toInt() * 1000;
}

void FleetCommandControllerTest::_ignoreUnansweredCommandNoise()
{
    // ArduPilot changes mode with MAV_CMD_DO_SET_MODE. When its link is mute the command queue gives up, and it
    // rejects a second mode change while the first one waits for its ack. Whether the first one is still waiting
    // when the test ends depends on the timing, so none of these messages is asserted.
    ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Giving up sending command after max retries: "
                                                       "MAV_CMD_DO_SET_MODE")));
    ignoreLogMessage("API.QGCApplication.AppMessage", QtDebugMsg,
                     QRegularExpression(QStringLiteral("Vehicle did not respond to command: Set flight mode")));
    ignoreLogMessage("API.QGCApplication.AppMessage", QtDebugMsg,
                     QRegularExpression(QStringLiteral("Unable to send command: Waiting on previous response")));
}

void FleetCommandControllerTest::_ignorePressureSensorNoise()
{
    // The airborne transition creates QGCPressure, which warns on hosts without a pressure backend
    ignoreLogMessage("Utilities.QGCSensors", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Failed to connect to pressure backend")));
    ignoreLogMessage("Utilities.QGCSensors", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Error Initializing Pressure Sensor")));
}

void FleetCommandControllerTest::_disconnectLinks()
{
    for (const QPointer<MockLink>& link : std::as_const(_links)) {
        if (link) {
            link->disconnect();
        }
    }
    _links.clear();

    // QSignalSpy::wait returns immediately once the test has failed, so poll
    MultiVehicleManager* const manager = MultiVehicleManager::instance();
    const bool vehiclesRemoved =
        QTest::qWaitFor([manager] { return manager->vehicles()->count() == 0; }, TestTimeout::longMs());
    _sharedLinks.clear();
    UnitTest::settleEventLoopForCleanup();
    QVERIFY2(vehiclesRemoved, "Timeout waiting for the vehicles to disconnect");
}

QVariantMap FleetCommandControllerTest::_entry(const FleetCommandController& controller, const Vehicle* vehicle)
{
    return controller.vehicleStatus().value(QString::number(vehicle->id())).toMap();
}

int FleetCommandControllerTest::_state(const FleetCommandController& controller, const Vehicle* vehicle)
{
    return _entry(controller, vehicle).value(QStringLiteral("state"), -1).toInt();
}

int FleetCommandControllerTest::_attempt(const FleetCommandController& controller, const Vehicle* vehicle)
{
    return _entry(controller, vehicle).value(QStringLiteral("attempt"), -1).toInt();
}

int FleetCommandControllerTest::_maxAttempts(const FleetCommandController& controller, const Vehicle* vehicle)
{
    return _entry(controller, vehicle).value(QStringLiteral("maxAttempts"), -1).toInt();
}

int FleetCommandControllerTest::_step(const FleetCommandController& controller, const Vehicle* vehicle)
{
    return _entry(controller, vehicle).value(QStringLiteral("step"), -1).toInt();
}

bool FleetCommandControllerTest::_confirmed(const FleetCommandController& controller, const Vehicle* vehicle)
{
    return _entry(controller, vehicle).value(QStringLiteral("confirmed"), false).toBool();
}

void FleetCommandControllerTest::_testCommandAllSucceeds_data()
{
    QTest::addColumn<MAV_AUTOPILOT>("autopilot");
    QTest::addColumn<bool>("land");

    QTest::newRow("PX4 RTL") << MAV_AUTOPILOT_PX4 << false;
    QTest::newRow("PX4 Land") << MAV_AUTOPILOT_PX4 << true;
    QTest::newRow("ArduCopter RTL") << MAV_AUTOPILOT_ARDUPILOTMEGA << false;
    QTest::newRow("ArduCopter Land") << MAV_AUTOPILOT_ARDUPILOTMEGA << true;
}

void FleetCommandControllerTest::_testCommandAllSucceeds()
{
    QFETCH(MAV_AUTOPILOT, autopilot);
    QFETCH(bool, land);

    if ((autopilot == MAV_AUTOPILOT_ARDUPILOTMEGA) && !apmFirmwareSupported()) {
        QSKIP("ArduPilot support not registered in this build");
    }

    Vehicle* const first = _connectVehicle(_startLink(autopilot), true);
    QVERIFY(first);
    Vehicle* const second = _connectVehicle(_startLink(autopilot), true);
    QVERIFY(second);

    FleetCommandController controller;
    QSignalSpy finishedSpy(&controller, &FleetCommandController::finished);
    QVERIFY(finishedSpy.isValid());

    if (land) {
        controller.landAll();
    } else {
        controller.rtlAll();
    }

    for (const Vehicle* const vehicle : {first, second}) {
        QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Succeeded),
                                  TestTimeout::longMs());
        QCOMPARE(vehicle->flightMode(), land ? vehicle->landFlightMode() : vehicle->rtlFlightMode());
    }

    QCOMPARE(controller.vehicleStatus().size(), 2);
    QVERIFY(!controller.busy());
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy[0][0].value<FleetCommandController::Command>(),
             land ? FleetCommandController::Command::Land : FleetCommandController::Command::RTL);
}

void FleetCommandControllerTest::_testDisarmedVehicleSkipped()
{
    Vehicle* const armed = _connectVehicle(_startLink(MAV_AUTOPILOT_PX4), true);
    QVERIFY(armed);
    Vehicle* const disarmed = _connectVehicle(_startLink(MAV_AUTOPILOT_PX4), false);
    QVERIFY(disarmed);
    const QString disarmedFlightMode = disarmed->flightMode();

    FleetCommandController controller;
    controller.rtlAll();

    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, armed), int(FleetCommandController::VehicleState::Succeeded),
                              TestTimeout::longMs());
    QCOMPARE(_state(controller, disarmed), int(FleetCommandController::VehicleState::Skipped));
    QCOMPARE(_attempt(controller, disarmed), 0);
    QCOMPARE(disarmed->flightMode(), disarmedFlightMode);
}

void FleetCommandControllerTest::_testUnsupportedVehicle()
{
    if (!apmFirmwareSupported()) {
        QSKIP("ArduPilot support not registered in this build");
    }

    // ArduSub has no RTL flight mode
    MockLink* const link = MockLink::startAPMArduSubMockLink();
    Vehicle* const vehicle = _connectVehicle(link, true);
    QVERIFY(vehicle);
    link->clearReceivedMavCommandCounts();

    FleetCommandController controller;
    QSignalSpy finishedSpy(&controller, &FleetCommandController::finished);
    QVERIFY(finishedSpy.isValid());

    controller.rtlAll();

    QCOMPARE(_state(controller, vehicle), int(FleetCommandController::VehicleState::Unsupported));
    QCOMPARE(_attempt(controller, vehicle), 0);
    QVERIFY(!controller.busy());
    QCOMPARE(finishedSpy.count(), 1);

    // Flush anything still queued towards the mock before checking that nothing was sent
    link->waitForWorkerIdle();
    QCOMPARE(link->receivedMavCommandCount(MAV_CMD_DO_SET_MODE), 0);
}

void FleetCommandControllerTest::_testRetriesThenFails()
{
    if (!apmFirmwareSupported()) {
        QSKIP("ArduPilot support not registered in this build");
    }

    _setAttempts(2);
    // An unanswered mode change keeps the ArduPilot command queue busy for 1.2 s, and the queue refuses a second one
    _setInterval(2);
    MockLink* const link = _startLink(MAV_AUTOPILOT_ARDUPILOTMEGA);
    Vehicle* const vehicle = _connectVehicle(link, true);
    QVERIFY(vehicle);

    FleetCommandController controller;
    QSignalSpy finishedSpy(&controller, &FleetCommandController::finished);
    QVERIFY(finishedSpy.isValid());

    _ignoreUnansweredCommandNoise();
    expectLogMessage(
        "Vehicle.FleetCommandController", QtWarningMsg,
        QRegularExpression(QStringLiteral("vehicle: %1 command: .*Land failed after 2 attempts").arg(vehicle->id())));

    link->clearReceivedMavCommandCounts();
    _setCommLost(link, true);
    controller.landAll();

    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Failed),
                              3 * _intervalMs() + TestTimeout::shortMs());
    verifyExpectedLogMessage();
    QCOMPARE(_attempt(controller, vehicle), 2);
    QCOMPARE(_maxAttempts(controller, vehicle), 2);
    QCOMPARE(link->receivedMavCommandCount(MAV_CMD_DO_SET_MODE), 2);
    QVERIFY(!controller.busy());
    QCOMPARE(finishedSpy.count(), 1);
}

void FleetCommandControllerTest::_testSucceedsWhenLinkReturns()
{
    _setAttempts(5);
    MockLink* const link = _startLink(MAV_AUTOPILOT_PX4);
    Vehicle* const vehicle = _connectVehicle(link, true);
    QVERIFY(vehicle);

    FleetCommandController controller;

    _setCommLost(link, true);
    controller.rtlAll();

    QTRY_VERIFY_WITH_TIMEOUT(_attempt(controller, vehicle) >= 2, 3 * _intervalMs() + TestTimeout::shortMs());
    QCOMPARE(_state(controller, vehicle), int(FleetCommandController::VehicleState::Pending));

    _setCommLost(link, false);

    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Succeeded),
                              6 * _intervalMs() + TestTimeout::longMs());
    QCOMPARE(_attempt(controller, vehicle), 5);
    QCOMPARE(vehicle->flightMode(), vehicle->rtlFlightMode());
}

void FleetCommandControllerTest::_testLandOverridesRtl()
{
    if (!apmFirmwareSupported()) {
        QSKIP("ArduPilot support not registered in this build");
    }

    _setAttempts(5);
    MockLink* const link = _startLink(MAV_AUTOPILOT_ARDUPILOTMEGA);
    Vehicle* const vehicle = _connectVehicle(link, true);
    QVERIFY(vehicle);

    FleetCommandController controller;
    QSignalSpy finishedSpy(&controller, &FleetCommandController::finished);
    QVERIFY(finishedSpy.isValid());

    _ignoreUnansweredCommandNoise();
    _setCommLost(link, true);
    controller.rtlAll();
    QTRY_COMPARE_WITH_TIMEOUT(_attempt(controller, vehicle), 1, TestTimeout::shortMs());

    controller.landAll();
    QCOMPARE(controller.activeCommand(), FleetCommandController::Command::Land);

    _setCommLost(link, false);
    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Succeeded),
                              TestTimeout::longMs());
    QCOMPARE(vehicle->flightMode(), vehicle->landFlightMode());

    // A leftover RTL attempt would flip the vehicle back to RTL
    QSignalSpy flightModeSpy(vehicle, &Vehicle::flightModeChanged);
    QVERIFY(flightModeSpy.isValid());
    QVERIFY_NO_SIGNAL_WAIT(flightModeSpy, 2 * _intervalMs());

    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy[0][0].value<FleetCommandController::Command>(), FleetCommandController::Command::Land);
}

void FleetCommandControllerTest::_testVehicleRemovedWhileRetrying()
{
    _setAttempts(5);
    Vehicle* const first = _connectVehicle(_startLink(MAV_AUTOPILOT_PX4), true);
    QVERIFY(first);
    MockLink* const secondLink = _startLink(MAV_AUTOPILOT_PX4);
    Vehicle* const second = _connectVehicle(secondLink, true);
    QVERIFY(second);
    const QString secondKey = QString::number(second->id());

    FleetCommandController controller;
    QSignalSpy finishedSpy(&controller, &FleetCommandController::finished);
    QVERIFY(finishedSpy.isValid());

    _setCommLost(secondLink, true);
    controller.rtlAll();
    QTRY_VERIFY_WITH_TIMEOUT(_attempt(controller, second) >= 1, TestTimeout::shortMs());

    // The second vehicle is deleted after this, so only its key is used from here on
    secondLink->disconnect();
    QTRY_COMPARE_WITH_TIMEOUT(MultiVehicleManager::instance()->vehicles()->count(), 1, TestTimeout::longMs());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.vehicleStatus().contains(secondKey), TestTimeout::longMs());

    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, first), int(FleetCommandController::VehicleState::Succeeded),
                              TestTimeout::longMs());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), TestTimeout::longMs());
    QCOMPARE(finishedSpy.count(), 1);
}

void FleetCommandControllerTest::_testCancelStopsResending()
{
    if (!apmFirmwareSupported()) {
        QSKIP("ArduPilot support not registered in this build");
    }

    _setAttempts(5);
    MockLink* const link = _startLink(MAV_AUTOPILOT_ARDUPILOTMEGA);
    Vehicle* const vehicle = _connectVehicle(link, true);
    QVERIFY(vehicle);

    FleetCommandController controller;
    QSignalSpy finishedSpy(&controller, &FleetCommandController::finished);
    QVERIFY(finishedSpy.isValid());

    _ignoreUnansweredCommandNoise();
    link->clearReceivedMavCommandCounts();
    _setCommLost(link, true);
    controller.landAll();
    QTRY_COMPARE_WITH_TIMEOUT(_attempt(controller, vehicle), 1, TestTimeout::shortMs());
    // The first attempt must have reached the mock before its counters are reset
    QTRY_COMPARE_WITH_TIMEOUT(link->receivedMavCommandCount(MAV_CMD_DO_SET_MODE), 1, TestTimeout::shortMs());

    link->clearReceivedMavCommandCounts();
    controller.cancel();

    QCOMPARE(_state(controller, vehicle), int(FleetCommandController::VehicleState::Cancelled));
    QVERIFY(!controller.busy());
    QCOMPARE(finishedSpy.count(), 0);

    // Created after cancel(), which itself emits vehicleStatusChanged
    QSignalSpy statusSpy(&controller, &FleetCommandController::vehicleStatusChanged);
    QVERIFY(statusSpy.isValid());
    QVERIFY_NO_SIGNAL_WAIT(statusSpy, 2 * _intervalMs());
    QCOMPARE(link->receivedMavCommandCount(MAV_CMD_DO_SET_MODE), 0);
}

void FleetCommandControllerTest::_testSendsEveryAttemptAfterConfirmation()
{
    if (!apmFirmwareSupported()) {
        QSKIP("ArduPilot support not registered in this build");
    }

    _setAttempts(3);
    MockLink* const link = _startLink(MAV_AUTOPILOT_ARDUPILOTMEGA);
    Vehicle* const vehicle = _connectVehicle(link, true);
    QVERIFY(vehicle);

    FleetCommandController controller;
    link->clearReceivedMavCommandCounts();
    controller.landAll();

    // The vehicle reports Land after the first send, and the remaining sends still go out
    QTRY_VERIFY_WITH_TIMEOUT(_confirmed(controller, vehicle), TestTimeout::shortMs());
    QCOMPARE(_state(controller, vehicle), int(FleetCommandController::VehicleState::Pending));

    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Succeeded),
                              4 * _intervalMs() + TestTimeout::shortMs());
    QCOMPARE(_attempt(controller, vehicle), 3);
    QCOMPARE(link->receivedMavCommandCount(MAV_CMD_DO_SET_MODE), 3);
}

void FleetCommandControllerTest::_testReassertsTargetMode()
{
    if (!apmFirmwareSupported()) {
        QSKIP("ArduPilot support not registered in this build");
    }

    _setAttempts(3);
    MockLink* const link = _startLink(MAV_AUTOPILOT_ARDUPILOTMEGA);
    Vehicle* const vehicle = _connectVehicle(link, true);
    QVERIFY(vehicle);

    FleetCommandController controller;
    _ignoreUnansweredCommandNoise();
    controller.rtlAll();
    QTRY_COMPARE_WITH_TIMEOUT(vehicle->flightMode(), vehicle->rtlFlightMode(), TestTimeout::shortMs());

    // Somebody else takes the vehicle out of RTL while the sends are still going out
    vehicle->setFlightMode(vehicle->pauseFlightMode());
    QTRY_COMPARE_WITH_TIMEOUT(vehicle->flightMode(), vehicle->pauseFlightMode(), TestTimeout::shortMs());
    QVERIFY(!_confirmed(controller, vehicle));

    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Succeeded),
                              4 * _intervalMs() + TestTimeout::longMs());
    QCOMPARE(vehicle->flightMode(), vehicle->rtlFlightMode());
}

void FleetCommandControllerTest::_testSettingsSnapshotPerCommand()
{
    _setAttempts(3);
    MockLink* const link = _startLink(MAV_AUTOPILOT_PX4);
    Vehicle* const vehicle = _connectVehicle(link, true);
    QVERIFY(vehicle);

    FleetCommandController controller;
    _setCommLost(link, true);
    controller.rtlAll();
    QCOMPARE(_attempt(controller, vehicle), 1);

    // The command that is running keeps the number of sends and the interval it started with
    _setAttempts(7);
    _setInterval(5);
    QCOMPARE(_maxAttempts(controller, vehicle), 3);
    QTRY_COMPARE_WITH_TIMEOUT(_attempt(controller, vehicle), 2, 1000 + TestTimeout::shortMs() / 2);
}

void FleetCommandControllerTest::_testIntervalFromSettings()
{
    _setAttempts(2);
    _setInterval(3);
    MockLink* const link = _startLink(MAV_AUTOPILOT_PX4);
    Vehicle* const vehicle = _connectVehicle(link, true);
    QVERIFY(vehicle);

    FleetCommandController controller;
    _setCommLost(link, true);
    controller.rtlAll();
    QCOMPARE(_attempt(controller, vehicle), 1);

    // The second send waits for the whole configured interval
    QVERIFY2(!QTest::qWaitFor([&] { return _attempt(controller, vehicle) >= 2; }, 2500),
             "Second send went out before the configured interval");
    QTRY_COMPARE_WITH_TIMEOUT(_attempt(controller, vehicle), 2, 1000 + TestTimeout::shortMs());
}

void FleetCommandControllerTest::_testTakeoffAll_data()
{
    QTest::addColumn<MAV_AUTOPILOT>("autopilot");

    QTest::newRow("PX4") << MAV_AUTOPILOT_PX4;
    QTest::newRow("ArduCopter") << MAV_AUTOPILOT_ARDUPILOTMEGA;
}

void FleetCommandControllerTest::_testTakeoffAll()
{
    QFETCH(MAV_AUTOPILOT, autopilot);

    if ((autopilot == MAV_AUTOPILOT_ARDUPILOTMEGA) && !apmFirmwareSupported()) {
        QSKIP("ArduPilot support not registered in this build");
    }

    _ignorePressureSensorNoise();
    MockLink* const firstLink = _startLink(autopilot);
    Vehicle* const first = _connectVehicle(firstLink, false);
    QVERIFY(first);
    MockLink* const secondLink = _startLink(autopilot);
    Vehicle* const second = _connectVehicle(secondLink, false);
    QVERIFY(second);

    FleetCommandController controller;
    QSignalSpy finishedSpy(&controller, &FleetCommandController::finished);
    QVERIFY(finishedSpy.isValid());
    firstLink->clearReceivedMavCommandCounts();
    secondLink->clearReceivedMavCommandCounts();

    controller.takeoffAll();
    QCOMPARE(controller.activeCommand(), FleetCommandController::Command::Takeoff);

    const int timeoutMs =
        2 * FleetCommandController::kTakeoffStepTimeoutMs + FleetCommandController::kTakeoffClimbTimeoutMs;
    const QList<Vehicle*> vehicles{first, second};
    const QList<MockLink*> links{firstLink, secondLink};
    for (int i = 0; i < vehicles.count(); i++) {
        Vehicle* const vehicle = vehicles[i];
        QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Succeeded),
                                  timeoutMs);
        QVERIFY(vehicle->armed());
        QVERIFY(vehicle->airborne());
        QCOMPARE(_step(controller, vehicle), int(FleetCommandController::TakeoffStep::Climb));
        QCOMPARE(vehicle->flightMode(), vehicle->readiness()->takeoffArmFlightMode());
        QCOMPARE(links[i]->receivedMavCommandCount(MAV_CMD_NAV_TAKEOFF), 1);
        if (autopilot == MAV_AUTOPILOT_ARDUPILOTMEGA) {
            QTRY_VERIFY_WITH_TIMEOUT(
                qAbs(vehicle->altitudeRelative()->rawValue().toDouble() - controller.takeoffAltitude()) < 0.5,
                TestTimeout::mediumMs());
        }
    }

    QVERIFY(!controller.busy());
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy[0][0].value<FleetCommandController::Command>(), FleetCommandController::Command::Takeoff);
}

void FleetCommandControllerTest::_testTakeoffSkipsAirborneAndUnsupported()
{
    if (!apmFirmwareSupported()) {
        QSKIP("ArduPilot support not registered in this build");
    }

    // ArduSub has no guided takeoff
    _ignorePressureSensorNoise();
    MockLink* const copterLink = _startLink(MAV_AUTOPILOT_ARDUPILOTMEGA);
    Vehicle* const copter = _connectVehicle(copterLink, false);
    QVERIFY(copter);
    Vehicle* const sub = _connectVehicle(MockLink::startAPMArduSubMockLink(), false);
    QVERIFY(sub);

    FleetCommandController controller;
    QSignalSpy finishedSpy(&controller, &FleetCommandController::finished);
    QVERIFY(finishedSpy.isValid());

    controller.takeoffAll();
    QTRY_COMPARE_WITH_TIMEOUT(
        _state(controller, copter), int(FleetCommandController::VehicleState::Succeeded),
        2 * FleetCommandController::kTakeoffStepTimeoutMs + FleetCommandController::kTakeoffClimbTimeoutMs);
    QCOMPARE(_state(controller, sub), int(FleetCommandController::VehicleState::Unsupported));
    QCOMPARE(_attempt(controller, sub), 0);
    QCOMPARE(finishedSpy.count(), 1);

    // The copter is in the air now: a second takeoff leaves it alone and finishes at once
    copterLink->clearReceivedMavCommandCounts();
    controller.takeoffAll();
    QCOMPARE(_state(controller, copter), int(FleetCommandController::VehicleState::Skipped));
    QCOMPARE(_attempt(controller, copter), 0);
    QVERIFY(!controller.busy());
    QCOMPARE(finishedSpy.count(), 2);

    copterLink->waitForWorkerIdle();
    QCOMPARE(copterLink->receivedMavCommandCount(MAV_CMD_NAV_TAKEOFF), 0);
}

void FleetCommandControllerTest::_testTakeoffStepTimeoutFails()
{
    MockLink* const link = _startLink(MAV_AUTOPILOT_PX4);
    Vehicle* const vehicle = _connectVehicle(link, false);
    QVERIFY(vehicle);

    FleetCommandController controller;
    expectLogMessage(
        "Vehicle.FleetCommandController", QtWarningMsg,
        QRegularExpression(QStringLiteral("vehicle: %1 command: .*Takeoff failed at step .*Mode").arg(vehicle->id())));

    link->clearReceivedMavCommandCounts();
    _setCommLost(link, true);
    controller.takeoffAll();

    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Failed),
                              FleetCommandController::kTakeoffStepTimeoutMs + TestTimeout::shortMs());
    verifyExpectedLogMessage();
    QCOMPARE(_step(controller, vehicle), int(FleetCommandController::TakeoffStep::Mode));
    QVERIFY(!controller.busy());
    QCOMPARE(link->receivedMavCommandCount(MAV_CMD_COMPONENT_ARM_DISARM), 0);
    QCOMPARE(link->receivedMavCommandCount(MAV_CMD_NAV_TAKEOFF), 0);
}

void FleetCommandControllerTest::_testLandCancelsTakeoff()
{
    if (!apmFirmwareSupported()) {
        QSKIP("ArduPilot support not registered in this build");
    }

    MockLink* const link = _startLink(MAV_AUTOPILOT_ARDUPILOTMEGA);
    Vehicle* const vehicle = _connectVehicle(link, false);
    QVERIFY(vehicle);

    FleetCommandController controller;
    _ignoreUnansweredCommandNoise();
    link->clearReceivedMavCommandCounts();
    _setCommLost(link, true);
    controller.takeoffAll();
    QTRY_COMPARE_WITH_TIMEOUT(_step(controller, vehicle), int(FleetCommandController::TakeoffStep::Mode),
                              TestTimeout::shortMs());

    // Land replaces the takeoff, and a disarmed vehicle has nothing to land
    controller.landAll();
    QCOMPARE(controller.activeCommand(), FleetCommandController::Command::Land);
    QCOMPARE(_state(controller, vehicle), int(FleetCommandController::VehicleState::Skipped));

    // With the link back, a leftover takeoff would arm the vehicle within one step timeout
    _setCommLost(link, false);
    QVERIFY2(!QTest::qWaitFor(
                 [&] {
                     return (link->receivedMavCommandCount(MAV_CMD_COMPONENT_ARM_DISARM) > 0) ||
                            (link->receivedMavCommandCount(MAV_CMD_NAV_TAKEOFF) > 0);
                 },
                 FleetCommandController::kTakeoffStepTimeoutMs + TestTimeout::shortMs()),
             "The takeoff went on after Land replaced it");
    QVERIFY(!vehicle->armed());
}

void FleetCommandControllerTest::_testLandReachesVehicleBeingArmed()
{
    _setAttempts(3);
    MockLink* const link = _startLink(MAV_AUTOPILOT_PX4);
    Vehicle* const vehicle = _connectVehicle(link, false);
    QVERIFY(vehicle);

    // Already in the mode the takeoff arms in, so the first thing it does is arm
    const QString armMode = vehicle->readiness()->takeoffArmFlightMode();
    vehicle->setFlightMode(armMode);
    QTRY_COMPARE_WITH_TIMEOUT(vehicle->flightMode(), armMode, TestTimeout::shortMs());

    FleetCommandController controller;
    _setCommLost(link, true);
    controller.takeoffAll();
    QCOMPARE(_step(controller, vehicle), int(FleetCommandController::TakeoffStep::Arm));
    QVERIFY(!vehicle->armed());

    // The arming is on its way, so the vehicle must not be skipped as if it were disarmed
    controller.landAll();
    QCOMPARE(_state(controller, vehicle), int(FleetCommandController::VehicleState::Pending));

    _setCommLost(link, false);
    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Succeeded),
                              4 * _intervalMs() + TestTimeout::longMs());
    QCOMPARE(vehicle->flightMode(), vehicle->landFlightMode());
}

void FleetCommandControllerTest::_testRebootAllIncludingArmed()
{
    Vehicle* const armed = _connectVehicle(_startLink(MAV_AUTOPILOT_PX4), true);
    QVERIFY(armed);
    Vehicle* const idle = _connectVehicle(_startLink(MAV_AUTOPILOT_PX4), false);
    QVERIFY(idle);
    const QString armedKey = QString::number(armed->id());
    const QString idleKey = QString::number(idle->id());

    FleetCommandController controller;
    QSignalSpy finishedSpy(&controller, &FleetCommandController::finished);
    QVERIFY(finishedSpy.isValid());

    controller.rebootAll();
    QCOMPARE(controller.activeCommand(), FleetCommandController::Command::Reboot);

    // The firmware accepts the reboot and the vehicle drops its links, so both vehicles go away
    QTRY_COMPARE_WITH_TIMEOUT(MultiVehicleManager::instance()->vehicles()->count(), 0, TestTimeout::longMs());
    const QVariantMap status = controller.vehicleStatus();
    QCOMPARE(status.size(), 2);
    for (const QString& key : {armedKey, idleKey}) {
        QCOMPARE(status.value(key).toMap().value(QStringLiteral("state")).toInt(),
                 int(FleetCommandController::VehicleState::Succeeded));
    }
    QVERIFY(!controller.busy());
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy[0][0].value<FleetCommandController::Command>(), FleetCommandController::Command::Reboot);
}

void FleetCommandControllerTest::_testRebootFailsWithoutAck()
{
    MockLink* const link = _startLink(MAV_AUTOPILOT_PX4);
    Vehicle* const vehicle = _connectVehicle(link, false);
    QVERIFY(vehicle);

    FleetCommandController controller;
    _setCommLost(link, true);
    ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Giving up sending command after max retries: "
                                                       "MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN")));
    // The vehicle reports its own failure first, when the command queue gives up
    expectAppMessage(QRegularExpression(QStringLiteral("Vehicle reboot failed")));
    expectLogMessage(
        "Vehicle.FleetCommandController", QtWarningMsg,
        QRegularExpression(
            QStringLiteral("vehicle: %1 command: .*Reboot failed: still connected after").arg(vehicle->id())));

    controller.rebootAll();

    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Failed),
                              FleetCommandController::kRebootTimeoutMs + TestTimeout::shortMs());
    verifyExpectedLogMessage();
    verifyExpectedLogMessage();
    QVERIFY(!controller.busy());
    QCOMPARE(MultiVehicleManager::instance()->vehicles()->count(), 1);
}

UT_REGISTER_TEST(FleetCommandControllerTest, TestLabel::Integration, TestLabel::Vehicle)
