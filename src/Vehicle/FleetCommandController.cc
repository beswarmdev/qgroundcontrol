#include "FleetCommandController.h"

#include <QtCore/QSet>
#include <QtCore/QTimer>

#include "Fact.h"
#include "FlyViewSettings.h"
#include "MultiVehicleManager.h"
#include "QGCLoggingCategory.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "Vehicle.h"
#include "VehicleReadiness.h"
#include "VehicleSupports.h"

QGC_LOGGING_CATEGORY(FleetCommandControllerLog, "Vehicle.FleetCommandController")

FleetCommandController::FleetCommandController(QObject* parent)
    : QObject(parent)
    , _attemptTimer(new QTimer(this))
{
    _attemptTimer->setSingleShot(true);
    (void) connect(_attemptTimer, &QTimer::timeout, this, &FleetCommandController::_attemptWindowElapsed);
    (void) connect(MultiVehicleManager::instance(), &MultiVehicleManager::vehicleRemoved, this,
                   &FleetCommandController::_vehicleRemoved);
}

FleetCommandController::~FleetCommandController()
{
    _releaseEntries();
}

void FleetCommandController::rtlAll()
{
    _start(Command::RTL);
}

void FleetCommandController::landAll()
{
    _start(Command::Land);
}

void FleetCommandController::takeoffAll()
{
    _start(Command::Takeoff);
}

void FleetCommandController::rebootAll()
{
    _start(Command::Reboot);
}

void FleetCommandController::cancel()
{
    if (!_busy) {
        return;
    }

    _attemptTimer->stop();
    for (VehicleEntry& entry : _entries) {
        if (entry.state == VehicleState::Pending) {
            entry.state = VehicleState::Cancelled;
            _disconnectEntry(entry);
        }
    }

    _publish();
    _updateBusy();
}

void FleetCommandController::_start(Command command)
{
    // A takeoff which gets replaced may have sent the arming already, which a still disarmed vehicle does not show yet
    QSet<quint32> arming;
    for (auto it = _entries.cbegin(); it != _entries.cend(); ++it) {
        const bool armingSent = (it->step == TakeoffStep::Arm) || (it->step == TakeoffStep::Climb);
        if (armingSent && ((it->state == VehicleState::Pending) || (it->state == VehicleState::Cancelled))) {
            arming.insert(it.key());
        }
    }

    // A command in progress is replaced without reporting it as finished
    _releaseEntries();

    const bool resends = ((command == Command::RTL) || (command == Command::Land));
    _maxAttempts = 1;
    if (resends) {
        FlyViewSettings* const settings = SettingsManager::instance()->flyViewSettings();
        _maxAttempts = settings
                           ? qBound(kMinAttempts, settings->fleetCommandAttempts()->rawValue().toInt(), kMaxAttempts)
                           : kMinAttempts;
        const int intervalSec =
            settings ? qBound(kMinIntervalSec, settings->fleetCommandIntervalSec()->rawValue().toInt(), kMaxIntervalSec)
                     : kMinIntervalSec;
        _attemptTimer->setInterval(intervalSec * 1000);
    }

    const QmlObjectListModel* const vehicles = MultiVehicleManager::instance()->vehicles();
    for (int i = 0; vehicles && (i < vehicles->count()); i++) {
        Vehicle* const vehicle = vehicles->value<Vehicle*>(i);
        if (vehicle) {
            _entries.insert(vehicle->id(), _classify(command, vehicle, arming.contains(vehicle->id())));
        }
    }

    const bool commandChanged = (_activeCommand != command);
    _activeCommand = command;

    const QList<quint32> ids = _entries.keys();
    for (const quint32 id : ids) {
        const auto it = _entries.constFind(id);
        if ((it != _entries.constEnd()) && (it->state == VehicleState::Pending)) {
            _begin(id);
        }
    }

    _publish();
    if (commandChanged) {
        emit activeCommandChanged();
    }
    _updateBusy();

    if (_busy) {
        if (resends) {
            _attemptTimer->start();
        }
    } else {
        emit finished(command);
    }
}

FleetCommandController::VehicleEntry FleetCommandController::_classify(Command command, Vehicle* vehicle,
                                                                       bool armingSent) const
{
    VehicleEntry entry;
    entry.vehicle = vehicle;

    if (command == Command::Reboot) {
        // Every vehicle gets it, armed or not
        entry.attempt = 1;
        return entry;
    }

    if (command == Command::Takeoff) {
        if (!vehicle->supports()->guidedTakeoffWithAltitude()) {
            entry.state = VehicleState::Unsupported;
        } else if (vehicle->airborne()) {
            entry.state = VehicleState::Skipped;
        } else {
            entry.attempt = 1;
        }
        return entry;
    }

    entry.targetMode = (command == Command::Land) ? vehicle->landFlightMode() : vehicle->rtlFlightMode();
    if (entry.targetMode.isEmpty()) {
        entry.state = VehicleState::Unsupported;
    } else if (!vehicle->armed() && !armingSent) {
        entry.state = VehicleState::Skipped;
    } else {
        // A vehicle that is already in the target mode still gets every send
        entry.confirmed = (vehicle->flightMode() == entry.targetMode);
    }
    return entry;
}

void FleetCommandController::_begin(quint32 id)
{
    const auto it = _entries.find(id);
    if ((it == _entries.end()) || !it->vehicle) {
        return;
    }

    Vehicle* const vehicle = it->vehicle;
    if (_activeCommand == Command::Reboot) {
        // A vehicle which accepts the reboot closes its links, which removes it from the vehicle list
        vehicle->rebootVehicle();
        _startStepTimer(id, kRebootTimeoutMs);
    } else if (_activeCommand == Command::Takeoff) {
        it->takeoffConnections.append(
            connect(vehicle, &Vehicle::flightModeChanged, this, [this, id]() { _takeoffProgress(id); }));
        it->takeoffConnections.append(
            connect(vehicle, &Vehicle::armedChanged, this, [this, id]() { _takeoffProgress(id); }));
        it->takeoffConnections.append(
            connect(vehicle, &Vehicle::airborneChanged, this, [this, id]() { _takeoffProgress(id); }));
        _enterTakeoffStep(id, TakeoffStep::Mode);
    } else {
        it->modeConnection = connect(vehicle, &Vehicle::flightModeChanged, this,
                                     [this, id](const QString& mode) { _flightModeChanged(id, mode); });
        _sendAttempt(id);
    }
}

void FleetCommandController::_sendAttempt(quint32 id)
{
    const auto it = _entries.find(id);
    if (it == _entries.end()) {
        return;
    }

    it->attempt++;
    qCDebug(FleetCommandControllerLog) << "vehicle:" << id << "command:" << _activeCommand << "attempt:" << it->attempt
                                       << "/" << _maxAttempts;

    Vehicle* const vehicle = it->vehicle;
    if (vehicle) {
        vehicle->setFlightMode(it->targetMode);
    }
}

void FleetCommandController::_attemptWindowElapsed()
{
    const QList<quint32> ids = _entries.keys();
    for (const quint32 id : ids) {
        const auto it = _entries.find(id);
        if ((it == _entries.end()) || (it->state != VehicleState::Pending)) {
            continue;
        }

        if (!it->vehicle) {
            _entries.erase(it);
            continue;
        }

        it->confirmed = (it->vehicle->flightMode() == it->targetMode);
        if (it->attempt < _maxAttempts) {
            _sendAttempt(id);
        } else if (it->confirmed) {
            _resolve(id, VehicleState::Succeeded);
        } else {
            qCWarning(FleetCommandControllerLog)
                << "vehicle:" << id << "command:" << _activeCommand << "failed after" << it->attempt << "attempts";
            _resolve(id, VehicleState::Failed);
        }
    }

    _settle();
}

void FleetCommandController::_flightModeChanged(quint32 id, const QString& mode)
{
    const auto it = _entries.find(id);
    if ((it == _entries.end()) || (it->state != VehicleState::Pending)) {
        return;
    }

    it->confirmed = (mode == it->targetMode);
    if (it->confirmed && (it->attempt >= _maxAttempts)) {
        _resolve(id, VehicleState::Succeeded);
    }
    _settle();
}

void FleetCommandController::_takeoffProgress(quint32 id)
{
    const auto it = _entries.find(id);
    if ((it == _entries.end()) || (it->state != VehicleState::Pending) || !it->vehicle) {
        return;
    }

    switch (it->step) {
        case TakeoffStep::Mode:
            if (it->vehicle->flightMode() == it->targetMode) {
                _enterTakeoffStep(id, TakeoffStep::Arm);
            }
            break;
        case TakeoffStep::Arm:
            if (it->vehicle->armed()) {
                _enterTakeoffStep(id, TakeoffStep::Climb);
            }
            break;
        case TakeoffStep::Climb:
            if (it->vehicle->airborne()) {
                _resolve(id, VehicleState::Succeeded);
            }
            break;
        case TakeoffStep::None:
            break;
    }

    _settle();
}

void FleetCommandController::_enterTakeoffStep(quint32 id, TakeoffStep step)
{
    const auto it = _entries.find(id);
    if ((it == _entries.end()) || !it->vehicle) {
        return;
    }

    Vehicle* const vehicle = it->vehicle;
    it->step = step;
    switch (step) {
        case TakeoffStep::Mode: {
            // The firmware plugin names the mode its own takeoff arms in, so nothing here depends on the autopilot
            const VehicleReadiness* const readiness = vehicle->readiness();
            const QString armMode = readiness ? readiness->takeoffArmFlightMode() : QString();
            if (armMode.isEmpty() || (vehicle->flightMode() == armMode)) {
                _enterTakeoffStep(id, TakeoffStep::Arm);
                return;
            }
            it->targetMode = armMode;
            vehicle->setFlightMode(armMode);
            _startStepTimer(id, kTakeoffStepTimeoutMs);
            break;
        }
        case TakeoffStep::Arm:
            if (vehicle->armed()) {
                _enterTakeoffStep(id, TakeoffStep::Climb);
                return;
            }
            // Arming is sent once, never retried
            vehicle->setArmed(true, false);
            _startStepTimer(id, kTakeoffStepTimeoutMs);
            break;
        case TakeoffStep::Climb:
            vehicle->guidedModeTakeoff(kTakeoffAltitudeMeters);
            _startStepTimer(id, kTakeoffClimbTimeoutMs);
            break;
        case TakeoffStep::None:
            break;
    }
}

void FleetCommandController::_startStepTimer(quint32 id, int timeoutMs)
{
    const auto it = _entries.find(id);
    if (it == _entries.end()) {
        return;
    }

    if (!it->stepTimer) {
        it->stepTimer = new QTimer(this);
        it->stepTimer->setSingleShot(true);
        (void) connect(it->stepTimer, &QTimer::timeout, this, [this, id]() { _stepTimedOut(id); });
    }
    it->stepTimer->start(timeoutMs);
}

void FleetCommandController::_stepTimedOut(quint32 id)
{
    const auto it = _entries.find(id);
    if ((it == _entries.end()) || (it->state != VehicleState::Pending)) {
        return;
    }

    if (_activeCommand == Command::Reboot) {
        qCWarning(FleetCommandControllerLog) << "vehicle:" << id << "command:" << _activeCommand
                                             << "failed: still connected after" << kRebootTimeoutMs << "ms";
    } else {
        qCWarning(FleetCommandControllerLog)
            << "vehicle:" << id << "command:" << _activeCommand << "failed at step" << it->step;
    }
    _resolve(id, VehicleState::Failed);
    _settle();
}

void FleetCommandController::_vehicleRemoved(Vehicle* vehicle)
{
    if (!vehicle) {
        return;
    }

    const auto it = _entries.find(vehicle->id());
    if ((it == _entries.end()) || (it->vehicle != vehicle)) {
        return;
    }

    if ((_activeCommand == Command::Reboot) && (it->state == VehicleState::Pending)) {
        // The vehicle comes back with the same id after the restart, so its row stays to show the result
        _resolve(it.key(), VehicleState::Succeeded);
    } else {
        _disconnectEntry(*it);
        _entries.erase(it);
    }
    _settle();
}

void FleetCommandController::_resolve(quint32 id, VehicleState state)
{
    const auto it = _entries.find(id);
    if (it == _entries.end()) {
        return;
    }

    it->state = state;
    _disconnectEntry(*it);
}

void FleetCommandController::_disconnectEntry(VehicleEntry& entry)
{
    QObject::disconnect(entry.modeConnection);
    for (const QMetaObject::Connection& connection : std::as_const(entry.takeoffConnections)) {
        QObject::disconnect(connection);
    }
    entry.takeoffConnections.clear();

    if (entry.stepTimer) {
        entry.stepTimer->stop();
        entry.stepTimer->deleteLater();
        entry.stepTimer = nullptr;
    }
}

void FleetCommandController::_settle()
{
    const bool wasBusy = _busy;

    _publish();
    _updateBusy();

    if (!_busy) {
        _attemptTimer->stop();
        if (wasBusy) {
            emit finished(_activeCommand);
        }
    } else if (((_activeCommand == Command::RTL) || (_activeCommand == Command::Land)) && !_attemptTimer->isActive()) {
        _attemptTimer->start();
    }
}

void FleetCommandController::_publish()
{
    QVariantMap status;
    for (auto it = _entries.cbegin(); it != _entries.cend(); ++it) {
        status.insert(QString::number(it.key()), QVariantMap{
                                                     {QStringLiteral("state"), static_cast<int>(it->state)},
                                                     {QStringLiteral("attempt"), it->attempt},
                                                     {QStringLiteral("maxAttempts"), _maxAttempts},
                                                     {QStringLiteral("confirmed"), it->confirmed},
                                                     {QStringLiteral("step"), static_cast<int>(it->step)},
                                                 });
    }

    _vehicleStatus = status;
    emit vehicleStatusChanged();
}

void FleetCommandController::_updateBusy()
{
    bool pending = false;
    for (const VehicleEntry& entry : std::as_const(_entries)) {
        if (entry.state == VehicleState::Pending) {
            pending = true;
            break;
        }
    }

    if (_busy != pending) {
        _busy = pending;
        emit busyChanged();
    }
}

void FleetCommandController::_releaseEntries()
{
    _attemptTimer->stop();
    for (VehicleEntry& entry : _entries) {
        _disconnectEntry(entry);
    }
    _entries.clear();
}
