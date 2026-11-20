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

void FlyViewFleetPanelUITest::_testPanelListsVehiclesAndCollapses()
{
    // The panel starts expanded when nothing was saved; leave the stored state as found
    const QVariant savedExpanded = fleetPanelStoredExpanded();
    const auto restoreExpanded = qScopeGuard([savedExpanded] { fleetPanelStoreExpanded(savedExpanded); });
    fleetPanelStoreExpanded(QVariant());

    // The gimbal probe goes unanswered once the link is cut, and the queue gives up on it
    ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Giving up sending command after max retries: "
                                                       "MAV_CMD_REQUEST_MESSAGE message: GIMBAL_MANAGER_INFORMATION")));

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
                    });
}

void FlyViewFleetPanelUITest::_testLandAllRequiresConfirmation()
{
    runWithMockLink(
        [] { return MockLink::startPX4MockLink(); },
        [this](QPointer<MockLink>, Vehicle* vehicle) {
            vehicle->setArmed(true, false /* showError */);
            QTRY_VERIFY_WITH_TIMEOUT(vehicle->armed(), TestTimeout::mediumMs());

            QVERIFY(clickButton(QStringLiteral("fleetLandAllButton")));

            // Confirm by emitting the delay button's activated signal instead of simulating press-and-hold
            QQuickItem* const confirmButton =
                findVisibleItem(_rootItem, QStringLiteral("guidedActionConfirmButton"), TestTimeout::mediumMs());
            QVERIFY2(confirmButton, "LAND ALL did not ask for a confirmation");

            // Nothing is sent before the confirmation: the vehicle never reaches Land and no command status appears
            const QString statusName = _vehicleObjectName(QStringLiteral("fleetCommandStatus_"), vehicle);
            QVERIFY2(!QTest::qWaitFor([vehicle] { return vehicle->flightMode() == vehicle->landFlightMode(); },
                                      TestTimeout::shortMs()),
                     "Land was sent before the confirmation");
            QVERIFY(!findVisibleItem(_rootItem, statusName, 0));

            QVERIFY(QMetaObject::invokeMethod(confirmButton, "activated"));

            QTRY_COMPARE_WITH_TIMEOUT(vehicle->flightMode(), vehicle->landFlightMode(), TestTimeout::longMs());
            QTRY_COMPARE_WITH_TIMEOUT(_visibleItemText(statusName), QStringLiteral("Land: confirmed"),
                                      TestTimeout::longMs());
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

    runWithMockLink([] { return MockLink::startPX4MockLink(); },
                    [this](QPointer<MockLink> mockLink, Vehicle* vehicle) {
                        vehicle->setArmed(true, false /* showError */);
                        QTRY_VERIFY_WITH_TIMEOUT(vehicle->armed(), TestTimeout::mediumMs());

                        QQuickItem* const rtlButton =
                            findVisibleItem(_rootItem, QStringLiteral("fleetRtlAllButton"), TestTimeout::longMs());
                        QVERIFY2(rtlButton, "RTL ALL button not visible");
                        QQuickItem* const landButton =
                            findVisibleItem(_rootItem, QStringLiteral("fleetLandAllButton"), TestTimeout::longMs());
                        QVERIFY2(landButton, "LAND ALL button not visible");
                        const qreal rtlTop = fleetPanelSceneRect(rtlButton).top();
                        const qreal landTop = fleetPanelSceneRect(landButton).top();

                        // Without a link the command keeps retrying, which brings up the Stop button and the summary
                        mockLink->setCommLost(true);
                        QVERIFY(clickButton(QStringLiteral("fleetRtlAllButton")));
                        QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("fleetStopButton"), TestTimeout::mediumMs()),
                                 "Stop retries not shown while retrying");
                        QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("fleetSummary"), TestTimeout::mediumMs()),
                                 "Summary not shown while retrying");

                        // A second tap on the same spot must hit RTL ALL again, not Stop retries
                        QTRY_VERIFY2_WITH_TIMEOUT(
                            qFuzzyCompare(fleetPanelSceneRect(rtlButton).top(), rtlTop) &&
                                qFuzzyCompare(fleetPanelSceneRect(landButton).top(), landTop),
                            qPrintable(QStringLiteral("RTL ALL moved from %1 to %2, LAND ALL from %3 to %4")
                                           .arg(rtlTop)
                                           .arg(fleetPanelSceneRect(rtlButton).top())
                                           .arg(landTop)
                                           .arg(fleetPanelSceneRect(landButton).top())),
                            TestTimeout::shortMs());

                        QVERIFY(clickButton(QStringLiteral("fleetStopButton")));
                    });
}
