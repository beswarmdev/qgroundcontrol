#include "FlyViewFleetPanelUITest.h"

#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtCore/QSettings>
#include <QtCore/QVariant>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

#include "AppSettings.h"
#include "Fact.h"
#include "FleetCommandController.h"
#include "FlyViewSettings.h"
#include "LinkManager.h"
#include "MockLink.h"
#include "MultiVehicleManager.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "Vehicle.h"

UT_REGISTER_TEST(FlyViewFleetPanelUITest, TestLabel::Integration)

namespace {

constexpr int kFleetPanelWindowWidth = 1280;
constexpr int kFleetPanelWindowHeight = 720;
constexpr int kFleetPanelCompactWindowHeight = 560;
constexpr int kFleetPanelCameraControlsMinHeight = 100;

// Where QGroundControl.saveBoolGlobalSetting() keeps the panel's expanded state (QGroundControlQmlGlobal, private)
constexpr const char* kFleetPanelSettingsGroup = "QGCQml";
constexpr const char* kFleetPanelExpandedKey = "FlyViewFleetPanelExpanded";

/// Raw stored value: invalid while the panel never saved its state.
QVariant fleetPanelStoredExpanded()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kFleetPanelSettingsGroup));
    return settings.value(QLatin1String(kFleetPanelExpandedKey));
}

/// Stores \a value, or removes the key when \a value is invalid.
void fleetPanelStoreExpanded(const QVariant& value)
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kFleetPanelSettingsGroup));
    if (value.isValid()) {
        settings.setValue(QLatin1String(kFleetPanelExpandedKey), value);
    } else {
        settings.remove(QLatin1String(kFleetPanelExpandedKey));
    }
}

/// The state the panel restores on its next start (expanded when nothing was saved).
bool fleetPanelPersistedExpanded()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kFleetPanelSettingsGroup));
    return settings.value(QLatin1String(kFleetPanelExpandedKey), true).toBool();
}

/// Position and size of \a item in window coordinates, after its layout has been polished.
QRectF fleetPanelSceneRect(QQuickItem* item)
{
    for (QQuickItem* ancestor = item; ancestor; ancestor = ancestor->parentItem()) {
        ancestor->ensurePolished();
    }
    return QRectF(item->mapToScene(QPointF(0, 0)), QSizeF(item->width(), item->height()));
}

}  // namespace

QString FlyViewFleetPanelUITest::_visibleItemText(const QString& objectName)
{
    const QQuickItem* const item = findVisibleItem(_rootItem, objectName, 0);
    return item ? item->property("text").toString() : QString();
}

QString FlyViewFleetPanelUITest::_vehicleObjectName(const QString& prefix, const Vehicle* vehicle)
{
    return prefix + QString::number(vehicle->id());
}

void FlyViewFleetPanelUITest::_ignorePressureSensorNoise()
{
    ignoreLogMessage("Utilities.QGCSensors", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Failed to connect to pressure backend")));
    ignoreLogMessage("Utilities.QGCSensors", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Error Initializing Pressure Sensor")));
}

void FlyViewFleetPanelUITest::init()
{
    QmlUITestBase::init();

    // One send per vehicle, one second apart, so a command reaches its final status within a second
    FlyViewSettings* const settings = SettingsManager::instance()->flyViewSettings();
    _savedAttempts = settings->fleetCommandAttempts()->rawValue();
    _savedInterval = settings->fleetCommandIntervalSec()->rawValue();
    settings->fleetCommandAttempts()->setRawValue(1);
    settings->fleetCommandIntervalSec()->setRawValue(1);
}

void FlyViewFleetPanelUITest::cleanup()
{
    QmlUITestBase::cleanup();

    FlyViewSettings* const settings = SettingsManager::instance()->flyViewSettings();
    settings->fleetCommandAttempts()->setRawValue(_savedAttempts);
    settings->fleetCommandIntervalSec()->setRawValue(_savedInterval);
}

void FlyViewFleetPanelUITest::_testPanelListsVehiclesAndCollapses()
{
    // The panel starts expanded when nothing was saved; leave the stored state as found
    const QVariant savedExpanded = fleetPanelStoredExpanded();
    const auto restoreExpanded = qScopeGuard([savedExpanded] { fleetPanelStoreExpanded(savedExpanded); });
    fleetPanelStoreExpanded(QVariant());

    // The gimbal probe goes unanswered once the link is cut, and the queue gives up on it. So does the request for
    // the available modes when the link is cut before the vehicle has answered it.
    ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Giving up sending command after max retries: "
                                                       "MAV_CMD_REQUEST_MESSAGE message: GIMBAL_MANAGER_INFORMATION")));
    ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Giving up sending command after max retries: "
                                                       "MAV_CMD_REQUEST_MESSAGE message: AVAILABLE_MODES")));
    ignoreLogMessage("Vehicle.StandardModes", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Failed to retrieve available modes")));

    runWithMockLink(
        [] { return MockLink::startPX4MockLink(); },
        [this](QPointer<MockLink> mockLink, Vehicle* vehicle) {
            QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("flyViewFleetPanel"), TestTimeout::longMs()),
                     "Fleet panel not visible");
            QVERIFY2(findVisibleItem(_rootItem, _vehicleObjectName(QStringLiteral("fleetVehicleRow_"), vehicle),
                                     TestTimeout::longMs()),
                     "Vehicle row not listed");

            const QString linkTextName = _vehicleObjectName(QStringLiteral("fleetLinkText_"), vehicle);
            QVERIFY2(findVisibleItem(_rootItem, linkTextName, TestTimeout::longMs()), "Link text not visible");
            QVERIFY(!_visibleItemText(linkTextName).isEmpty());
            QVERIFY(!_visibleItemText(linkTextName).contains(QStringLiteral("Comm Lost")));

            mockLink->setCommLost(true);
            QTRY_VERIFY_WITH_TIMEOUT(_visibleItemText(linkTextName).contains(QStringLiteral("Comm Lost")),
                                     TestTimeout::longMs());

            QVERIFY(clickButton(QStringLiteral("fleetPanelToggle")));
            QTRY_VERIFY_WITH_TIMEOUT(!findVisibleItem(_rootItem, QStringLiteral("fleetVehicleList"), 0),
                                     TestTimeout::shortMs());
            QVERIFY(findVisibleItem(_rootItem, QStringLiteral("fleetPanelHeader"), 0));
            QVERIFY(!fleetPanelPersistedExpanded());

            QVERIFY(clickButton(QStringLiteral("fleetPanelToggle")));
            QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("fleetVehicleList"), TestTimeout::shortMs()),
                     "Vehicle list not shown after expanding");
            QVERIFY(fleetPanelPersistedExpanded());
        });
}

void FlyViewFleetPanelUITest::_testRtlAllSendsWithoutConfirmation()
{
    runWithMockLink([] { return MockLink::startPX4MockLink(); },
                    [this](QPointer<MockLink>, Vehicle* vehicle) {
                        vehicle->setArmed(true, false /* showError */);
                        QTRY_VERIFY_WITH_TIMEOUT(vehicle->armed(), TestTimeout::mediumMs());
                        QVERIFY(vehicle->flightMode() != vehicle->rtlFlightMode());

                        // No click on the confirmation slider: RTL ALL must go out on its own
                        QVERIFY(clickButton(QStringLiteral("fleetRtlAllButton")));

                        QTRY_COMPARE_WITH_TIMEOUT(vehicle->flightMode(), vehicle->rtlFlightMode(),
                                                  TestTimeout::longMs());
                        const QString statusName = _vehicleObjectName(QStringLiteral("fleetCommandStatus_"), vehicle);
                        QTRY_COMPARE_WITH_TIMEOUT(_visibleItemText(statusName), QStringLiteral("RTL: confirmed"),
                                                  TestTimeout::longMs());
                        QVERIFY2(!findVisibleItem(_rootItem, QStringLiteral("guidedActionConfirmButton"), 0),
                                 "RTL ALL asked for a confirmation");
                        QVERIFY2(!findVisibleItem(_rootItem, QStringLiteral("fleetConfirmOverlay"), 0),
                                 "RTL ALL opened the confirmation overlay");
                    });
}

void FlyViewFleetPanelUITest::_testLandAllConfirmsInCenterOverlay()
{
    runWithMockLink(
        [] { return MockLink::startPX4MockLink(); },
        [this](QPointer<MockLink>, Vehicle* vehicle) {
            vehicle->setArmed(true, false /* showError */);
            QTRY_VERIFY_WITH_TIMEOUT(vehicle->armed(), TestTimeout::mediumMs());

            QVERIFY(clickButton(QStringLiteral("fleetLandAllButton")));

            QQuickItem* const overlay =
                findVisibleItem(_rootItem, QStringLiteral("fleetConfirmOverlay"), TestTimeout::mediumMs());
            QVERIFY2(overlay, "LAND ALL did not open the confirmation overlay");
            QQuickItem* const card =
                findVisibleItem(_rootItem, QStringLiteral("fleetConfirmCard"), TestTimeout::mediumMs());
            QVERIFY2(card, "Confirmation card not visible");

            // The card sits in the middle of the screen
            const QPointF overlayCenter = fleetPanelSceneRect(overlay).center();
            const QPointF cardCenter = fleetPanelSceneRect(card).center();
            QVERIFY2(qAbs(cardCenter.x() - overlayCenter.x()) <= 1.0 && qAbs(cardCenter.y() - overlayCenter.y()) <= 1.0,
                     qPrintable(QStringLiteral("Card centered at %1,%2 instead of %3,%4")
                                    .arg(cardCenter.x())
                                    .arg(cardCenter.y())
                                    .arg(overlayCenter.x())
                                    .arg(overlayCenter.y())));
            QCOMPARE(_visibleItemText(QStringLiteral("fleetConfirmTitle")), QStringLiteral("LAND ALL VEHICLES"));
            QVERIFY(
                _visibleItemText(QStringLiteral("fleetConfirmMessage")).startsWith(QStringLiteral("1 armed vehicles")));

            // Nothing is sent before the confirmation: the vehicle never reaches Land and no command status appears
            const QString statusName = _vehicleObjectName(QStringLiteral("fleetCommandStatus_"), vehicle);
            QVERIFY2(!QTest::qWaitFor([vehicle] { return vehicle->flightMode() == vehicle->landFlightMode(); },
                                      TestTimeout::shortMs()),
                     "Land was sent before the confirmation");
            QVERIFY(!findVisibleItem(_rootItem, statusName, 0));

            QVERIFY(clickButton(QStringLiteral("fleetConfirmAcceptButton")));

            QTRY_VERIFY_WITH_TIMEOUT(!findVisibleItem(_rootItem, QStringLiteral("fleetConfirmOverlay"), 0),
                                     TestTimeout::shortMs());
            QTRY_COMPARE_WITH_TIMEOUT(vehicle->flightMode(), vehicle->landFlightMode(), TestTimeout::longMs());
            QTRY_COMPARE_WITH_TIMEOUT(_visibleItemText(statusName), QStringLiteral("Land: confirmed"),
                                      TestTimeout::longMs());
        });
}

void FlyViewFleetPanelUITest::_testLandOverlayCancelSendsNothing()
{
    runWithMockLink(
        [] { return MockLink::startPX4MockLink(); },
        [this](QPointer<MockLink>, Vehicle* vehicle) {
            vehicle->setArmed(true, false /* showError */);
            QTRY_VERIFY_WITH_TIMEOUT(vehicle->armed(), TestTimeout::mediumMs());

            QVERIFY(clickButton(QStringLiteral("fleetLandAllButton")));
            QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("fleetConfirmOverlay"), TestTimeout::mediumMs()),
                     "LAND ALL did not open the confirmation overlay");

            QVERIFY(clickButton(QStringLiteral("fleetConfirmCancelButton")));
            QTRY_VERIFY_WITH_TIMEOUT(!findVisibleItem(_rootItem, QStringLiteral("fleetConfirmOverlay"), 0),
                                     TestTimeout::shortMs());

            QVERIFY2(!QTest::qWaitFor([vehicle] { return vehicle->flightMode() == vehicle->landFlightMode(); },
                                      TestTimeout::shortMs()),
                     "Land was sent after the confirmation was cancelled");
            QVERIFY(!findVisibleItem(_rootItem, _vehicleObjectName(QStringLiteral("fleetCommandStatus_"), vehicle), 0));
        });
}

void FlyViewFleetPanelUITest::_testTakeoffAllSendsWithoutConfirmation()
{
    _ignorePressureSensorNoise();

    runWithMockLink([] { return MockLink::startPX4MockLink(); },
                    [this](QPointer<MockLink>, Vehicle* vehicle) {
                        QVERIFY(!vehicle->armed());

                        // No overlay and no slider: TAKEOFF ALL goes out on its own
                        QVERIFY(clickButton(QStringLiteral("fleetTakeoffAllButton")));
                        QVERIFY2(!findVisibleItem(_rootItem, QStringLiteral("fleetConfirmOverlay"), 0),
                                 "TAKEOFF ALL opened the confirmation overlay");
                        QVERIFY2(!findVisibleItem(_rootItem, QStringLiteral("guidedActionConfirmButton"), 0),
                                 "TAKEOFF ALL asked for a confirmation");

                        const int timeoutMs = 2 * FleetCommandController::kTakeoffStepTimeoutMs +
                                              FleetCommandController::kTakeoffClimbTimeoutMs;
                        QTRY_VERIFY_WITH_TIMEOUT(vehicle->armed() && vehicle->airborne(), timeoutMs);
                        const QString statusName = _vehicleObjectName(QStringLiteral("fleetCommandStatus_"), vehicle);
                        QTRY_COMPARE_WITH_TIMEOUT(_visibleItemText(statusName), QStringLiteral("Takeoff: airborne"),
                                                  TestTimeout::mediumMs());
                    });
}

void FlyViewFleetPanelUITest::_testRebootNeedsTwoConfirmations()
{
    runWithMockLink(
        [] { return MockLink::startPX4MockLink(); },
        [this](QPointer<MockLink> mockLink, Vehicle* vehicle) {
            // The reboot ack is handled inside MockLink and makes the vehicle close its link, which drops the last
            // reference to the MockLink while the MockLink is still running
            const SharedLinkInterfacePtr keepLinkAlive =
                LinkManager::instance()->sharedLinkInterfacePointerForLink(mockLink.data());
            QVERIFY(keepLinkAlive);

            vehicle->setArmed(true, false /* showError */);
            QTRY_VERIFY_WITH_TIMEOUT(vehicle->armed(), TestTimeout::mediumMs());

            QVERIFY(clickButton(QStringLiteral("fleetRebootAllButton")));
            QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("fleetConfirmOverlay"), TestTimeout::mediumMs()),
                     "REBOOT ALL did not open the confirmation overlay");
            QCOMPARE(_visibleItemText(QStringLiteral("fleetConfirmTitle")), QStringLiteral("REBOOT ALL AUTOPILOTS"));
            QCOMPARE(_visibleItemText(QStringLiteral("fleetConfirmStep")), QStringLiteral("Step 1 of 2"));
            QVERIFY(_visibleItemText(QStringLiteral("fleetConfirmMessage"))
                        .contains(QStringLiteral("1 of them are ARMED")));

            // The first confirmation only brings up the second one
            QVERIFY(clickButton(QStringLiteral("fleetConfirmAcceptButton")));
            QTRY_COMPARE_WITH_TIMEOUT(_visibleItemText(QStringLiteral("fleetConfirmTitle")),
                                      QStringLiteral("CONFIRM REBOOT"), TestTimeout::shortMs());
            QCOMPARE(_visibleItemText(QStringLiteral("fleetConfirmStep")), QStringLiteral("Step 2 of 2"));
            QQuickItem* const acceptButton = findVisibleItem(_rootItem, QStringLiteral("fleetConfirmAcceptButton"), 0);
            QVERIFY(acceptButton);
            QVERIFY2(!acceptButton->isEnabled(), "Second step accepts right away");

            // A tap on the disabled button changes nothing
            QVERIFY(clickButton(QStringLiteral("fleetConfirmAcceptButton")));
            QCOMPARE(_visibleItemText(QStringLiteral("fleetConfirmTitle")), QStringLiteral("CONFIRM REBOOT"));
            QVERIFY(MultiVehicleManager::instance()->activeVehicle() != nullptr);

            // After the delay the second step accepts, and the vehicle is restarted
            QTRY_VERIFY_WITH_TIMEOUT(acceptButton->isEnabled(), 1500 + TestTimeout::shortMs());
            QVERIFY(clickButton(QStringLiteral("fleetConfirmAcceptButton")));
            QTRY_VERIFY_WITH_TIMEOUT(!findVisibleItem(_rootItem, QStringLiteral("fleetConfirmOverlay"), 0),
                                     TestTimeout::shortMs());
            QTRY_VERIFY_WITH_TIMEOUT(MultiVehicleManager::instance()->activeVehicle() == nullptr,
                                     TestTimeout::longMs());
        });
}

void FlyViewFleetPanelUITest::_testResendFieldsEditSettings()
{
    runWithMockLink([] { return MockLink::startPX4MockLink(); },
                    [this](QPointer<MockLink>, Vehicle*) {
                        FlyViewSettings* const settings = SettingsManager::instance()->flyViewSettings();
                        QQuickItem* const attemptsField =
                            findVisibleItem(_rootItem, QStringLiteral("fleetAttemptsField"), TestTimeout::longMs());
                        QVERIFY2(attemptsField, "Sends field not visible");
                        QQuickItem* const intervalField =
                            findVisibleItem(_rootItem, QStringLiteral("fleetIntervalField"), TestTimeout::longMs());
                        QVERIFY2(intervalField, "Interval field not visible");

                        const auto edit = [](QQuickItem* field, const QString& text) {
                            field->setProperty("text", text);
                            return QMetaObject::invokeMethod(field, "editingFinished");
                        };

                        // A valid value is stored, and the fields refuse anything outside the Fact's range
                        QVERIFY(edit(attemptsField, QStringLiteral("5")));
                        QTRY_COMPARE_WITH_TIMEOUT(settings->fleetCommandAttempts()->rawValue().toInt(), 5,
                                                  TestTimeout::shortMs());
                        for (const QString& invalid : {QStringLiteral("0"), QStringLiteral("21")}) {
                            QVERIFY(edit(attemptsField, invalid));
                            QCOMPARE(settings->fleetCommandAttempts()->rawValue().toInt(), 5);
                        }

                        QVERIFY(edit(intervalField, QStringLiteral("7")));
                        QTRY_COMPARE_WITH_TIMEOUT(settings->fleetCommandIntervalSec()->rawValue().toInt(), 7,
                                                  TestTimeout::shortMs());
                        for (const QString& invalid : {QStringLiteral("0"), QStringLiteral("61")}) {
                            QVERIFY(edit(intervalField, invalid));
                            QCOMPARE(settings->fleetCommandIntervalSec()->rawValue().toInt(), 7);
                        }
                    });
}

void FlyViewFleetPanelUITest::_testConfirmOverlayClosesWhenFleetEmpty()
{
    runWithMockLink([] { return MockLink::startPX4MockLink(); },
                    [this](QPointer<MockLink> mockLink, Vehicle* vehicle) {
                        vehicle->setArmed(true, false /* showError */);
                        QTRY_VERIFY_WITH_TIMEOUT(vehicle->armed(), TestTimeout::mediumMs());

                        QVERIFY(clickButton(QStringLiteral("fleetLandAllButton")));
                        const QPointer<QQuickItem> overlay =
                            findVisibleItem(_rootItem, QStringLiteral("fleetConfirmOverlay"), TestTimeout::mediumMs());
                        QVERIFY2(overlay, "LAND ALL did not open the confirmation overlay");

                        // The last vehicle goes away while the overlay is open: it closes without running the command
                        disconnectMockLink(mockLink);
                        QTRY_VERIFY_WITH_TIMEOUT(!findVisibleItem(_rootItem, QStringLiteral("fleetConfirmOverlay"), 0),
                                                 TestTimeout::longMs());
                        QCOMPARE(overlay->property("currentStep").toInt(), 0);
                    });
}

void FlyViewFleetPanelUITest::_testFleetPanelLayoutWithMultiVehiclePanel()
{
    Fact* const multiVehiclePanel = SettingsManager::instance()->appSettings()->enableMultiVehiclePanel();
    const QVariant savedMultiVehiclePanel = multiVehiclePanel->rawValue();
    const auto restoreMultiVehiclePanel = qScopeGuard(
        [multiVehiclePanel, savedMultiVehiclePanel] { multiVehiclePanel->setRawValue(savedMultiVehiclePanel); });
    multiVehiclePanel->setRawValue(true);

    runWithMockLink(
        [] { return MockLink::startPX4MockLink(); },
        [this](QPointer<MockLink>, Vehicle* firstVehicle) {
            // MainWindowSavedState sets a minimum size that onscreen platforms enforce
            _window->setMinimumWidth(0);
            _window->setMinimumHeight(0);
            _window->resize(kFleetPanelWindowWidth, kFleetPanelWindowHeight);
            QTRY_COMPARE_WITH_TIMEOUT(_rootItem->width(), qreal(kFleetPanelWindowWidth), TestTimeout::shortMs());
            QTRY_COMPARE_WITH_TIMEOUT(_rootItem->height(), qreal(kFleetPanelWindowHeight), TestTimeout::shortMs());

            // The multi-vehicle panel only shows with at least two vehicles
            expectAppMessage(QRegularExpression(QStringLiteral("Connected to Vehicle [0-9]+")));
            const QPointer<MockLink> secondLink = MockLink::startPX4MockLink();
            QVERIFY(secondLink);
            const auto disconnectSecondLink = qScopeGuard([secondLink] {
                if (secondLink) {
                    secondLink->disconnect();
                }
                QVERIFY2(QTest::qWaitFor([] { return MultiVehicleManager::instance()->vehicles()->count() == 1; },
                                         TestTimeout::mediumMs()),
                         "Timeout waiting for the second vehicle to disconnect");
            });
            QTRY_VERIFY_WITH_TIMEOUT(MultiVehicleManager::instance()->getVehicleById(secondLink->vehicleId()),
                                     TestTimeout::longMs());
            verifyExpectedLogMessage();
            Vehicle* const secondVehicle = MultiVehicleManager::instance()->getVehicleById(secondLink->vehicleId());
            QVERIFY(secondVehicle != firstVehicle);
            QTRY_VERIFY_WITH_TIMEOUT(secondVehicle->isInitialConnectComplete(), TestTimeout::longMs());
            QCOMPARE(MultiVehicleManager::instance()->vehicles()->count(), 2);

            QQuickItem* const topRightPanel =
                findVisibleItem(_rootItem, QStringLiteral("flyViewTopRightPanel"), TestTimeout::longMs());
            QVERIFY2(topRightPanel, "Multi-vehicle panel not visible with two vehicles");
            QQuickItem* const fleetPanel =
                findVisibleItem(_rootItem, QStringLiteral("flyViewFleetPanel"), TestTimeout::longMs());
            QVERIFY2(fleetPanel, "Fleet panel not visible");
            QQuickItem* const bottomRightRow =
                findVisibleItem(_rootItem, QStringLiteral("flyViewBottomRightRowLayout"), TestTimeout::longMs());
            QVERIFY2(bottomRightRow, "Bottom right instrument row not visible");

            const auto sceneRect = [](QQuickItem* item) {
                for (QQuickItem* ancestor = item; ancestor; ancestor = ancestor->parentItem()) {
                    ancestor->ensurePolished();
                }
                return QRectF(item->mapToScene(QPointF(0, 0)), QSizeF(item->width(), item->height()));
            };
            const auto windowRect = [this] { return QRectF(0, 0, _rootItem->width(), _rootItem->height()); };
            const auto panelsAreClear = [&] {
                const QRectF topRight = sceneRect(topRightPanel);
                const QRectF fleet = sceneRect(fleetPanel);
                const QRectF bottomRow = sceneRect(bottomRightRow);
                return topRight.bottom() <= fleet.top() && fleet.bottom() <= bottomRow.top() &&
                       windowRect().contains(topRight) && windowRect().contains(fleet) &&
                       windowRect().contains(bottomRow);
            };
            const auto panelsDescription = [&] {
                const auto text = [](const QRectF& rect) {
                    return QStringLiteral("(%1, %2, %3 x %4)")
                        .arg(rect.x())
                        .arg(rect.y())
                        .arg(rect.width())
                        .arg(rect.height());
                };
                return QStringLiteral("window %1, top right %2, fleet %3, bottom right %4")
                    .arg(text(windowRect()), text(sceneRect(topRightPanel)), text(sceneRect(fleetPanel)),
                         text(sceneRect(bottomRightRow)));
            };
            QTRY_VERIFY2_WITH_TIMEOUT(panelsAreClear(), qPrintable(panelsDescription()), TestTimeout::mediumMs());
        });
}

void FlyViewFleetPanelUITest::_testFleetPanelStaysBelowCameraControls()
{
    // With one vehicle the top right column holds the camera controls instead of the multi-vehicle panel
    runWithMockLink(
        [] { return MockLink::startPX4MockLink(MockConfiguration::OptionEnableCamera); },
        [this](QPointer<MockLink>, Vehicle* vehicle) {
            _window->setMinimumWidth(0);
            _window->setMinimumHeight(0);
            _window->resize(kFleetPanelWindowWidth, kFleetPanelCompactWindowHeight);
            QTRY_COMPARE_WITH_TIMEOUT(_rootItem->height(), qreal(kFleetPanelCompactWindowHeight),
                                      TestTimeout::shortMs());

            QQuickItem* const column =
                findVisibleItem(_rootItem, QStringLiteral("flyViewTopRightColumn"), TestTimeout::longMs());
            QVERIFY2(column, "Top right column not visible with one vehicle");
            QQuickItem* const fleetPanel =
                findVisibleItem(_rootItem, QStringLiteral("flyViewFleetPanel"), TestTimeout::longMs());
            QVERIFY2(fleetPanel, "Fleet panel not visible");

            QVERIFY(vehicle->cameraManager());
            QTRY_VERIFY_WITH_TIMEOUT(fleetPanelSceneRect(column).height() > kFleetPanelCameraControlsMinHeight,
                                     TestTimeout::longMs());

            const auto description = [&] {
                const auto text = [](const QRectF& rect) {
                    return QStringLiteral("(%1, %2, %3 x %4)")
                        .arg(rect.x())
                        .arg(rect.y())
                        .arg(rect.width())
                        .arg(rect.height());
                };
                return QStringLiteral("top right column %1, fleet panel %2")
                    .arg(text(fleetPanelSceneRect(column)), text(fleetPanelSceneRect(fleetPanel)));
            };
            QTRY_VERIFY2_WITH_TIMEOUT(!fleetPanelSceneRect(column).intersects(fleetPanelSceneRect(fleetPanel)),
                                      qPrintable(description()), TestTimeout::mediumMs());
        });
}

void FlyViewFleetPanelUITest::_testActionButtonsStayPutWhileRetrying()
{
    // The link is cut below, and the queue gives up on the unanswered gimbal probe
    ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Giving up sending command after max retries: "
                                                       "MAV_CMD_REQUEST_MESSAGE message: GIMBAL_MANAGER_INFORMATION")));

    // Five sends keep the command busy long enough to look at the panel
    SettingsManager::instance()->flyViewSettings()->fleetCommandAttempts()->setRawValue(5);

    runWithMockLink([] { return MockLink::startPX4MockLink(); },
                    [this](QPointer<MockLink> mockLink, Vehicle* vehicle) {
                        vehicle->setArmed(true, false /* showError */);
                        QTRY_VERIFY_WITH_TIMEOUT(vehicle->armed(), TestTimeout::mediumMs());

                        QList<QQuickItem*> buttons;
                        QList<qreal> tops;
                        for (const QString& name :
                             {QStringLiteral("fleetTakeoffAllButton"), QStringLiteral("fleetRebootAllButton"),
                              QStringLiteral("fleetRtlAllButton"), QStringLiteral("fleetLandAllButton")}) {
                            QQuickItem* const button = findVisibleItem(_rootItem, name, TestTimeout::longMs());
                            QVERIFY2(button, qPrintable(name + QStringLiteral(" not visible")));
                            buttons.append(button);
                            tops.append(fleetPanelSceneRect(button).top());
                        }

                        // Without a link the command keeps retrying, which brings up the Stop button and the summary
                        mockLink->setCommLost(true);
                        QVERIFY(clickButton(QStringLiteral("fleetRtlAllButton")));
                        QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("fleetStopButton"), TestTimeout::mediumMs()),
                                 "Stop retries not shown while retrying");
                        QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("fleetSummary"), TestTimeout::mediumMs()),
                                 "Summary not shown while retrying");

                        // A second tap on the same spot must hit the same action again, not Stop
                        // The layout rounds fractional heights, which moves a button by half a pixel at most
                        const auto buttonsStayPut = [&buttons, &tops] {
                            for (int i = 0; i < buttons.count(); i++) {
                                if (qAbs(fleetPanelSceneRect(buttons[i]).top() - tops[i]) > 1.0) {
                                    return false;
                                }
                            }
                            return true;
                        };
                        const auto describeButtons = [&buttons, &tops] {
                            QStringList moves;
                            for (int i = 0; i < buttons.count(); i++) {
                                moves.append(QStringLiteral("%1 from %2 to %3")
                                                 .arg(buttons[i]->objectName())
                                                 .arg(tops[i])
                                                 .arg(fleetPanelSceneRect(buttons[i]).top()));
                            }
                            return moves.join(QStringLiteral(", "));
                        };
                        QTRY_VERIFY2_WITH_TIMEOUT(buttonsStayPut(), qPrintable(describeButtons()),
                                                  TestTimeout::shortMs());

                        QVERIFY(clickButton(QStringLiteral("fleetStopButton")));
                    });
}
