#include "FleetCommandControllerTest.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QRegularExpression>
#include <QtTest/QSignalSpy>

#include "Fact.h"
#include "FleetCommandController.h"
#include "FlyViewSettings.h"
#include "MockLink.h"
#include "MultiVehicleManager.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "Vehicle.h"

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
}

void FleetCommandControllerTest::cleanup()
{
    dumpFailureContextIfTestFailed(QStringLiteral("before FleetCommandControllerTest teardown"));
    _disconnectLinks();
    SettingsManager::instance()->flyViewSettings()->fleetCommandAttempts()->setRawValue(_savedAttempts);
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
                              3 * FleetCommandController::kAttemptTimeoutMs + TestTimeout::shortMs());
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

    QTRY_VERIFY_WITH_TIMEOUT(_attempt(controller, vehicle) >= 2,
                             3 * FleetCommandController::kAttemptTimeoutMs + TestTimeout::shortMs());
    QCOMPARE(_state(controller, vehicle), int(FleetCommandController::VehicleState::Pending));

    _setCommLost(link, false);

    QTRY_COMPARE_WITH_TIMEOUT(_state(controller, vehicle), int(FleetCommandController::VehicleState::Succeeded),
                              TestTimeout::longMs());
    QVERIFY(_attempt(controller, vehicle) <= 5);
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
    QVERIFY_NO_SIGNAL_WAIT(flightModeSpy, 2 * FleetCommandController::kAttemptTimeoutMs);

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
    QVERIFY_NO_SIGNAL_WAIT(statusSpy, 2 * FleetCommandController::kAttemptTimeoutMs);
    QCOMPARE(link->receivedMavCommandCount(MAV_CMD_DO_SET_MODE), 0);
}

UT_REGISTER_TEST(FleetCommandControllerTest, TestLabel::Integration, TestLabel::Vehicle)
