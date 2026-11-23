#include "FleetCommandController.h"

#include <QtCore/QTimer>

#include "Fact.h"
#include "FlyViewSettings.h"
#include "MultiVehicleManager.h"
#include "QGCLoggingCategory.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "Vehicle.h"

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
    qCDebug(FleetCommandControllerLog) << "not implemented";
}

void FleetCommandController::rebootAll()
{
    qCDebug(FleetCommandControllerLog) << "not implemented";
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
            QObject::disconnect(entry.modeConnection);
        }
    }

    _publish();
    _updateBusy();
}

void FleetCommandController::_start(Command command)
{
    // A command in progress is replaced without reporting it as finished
    _releaseEntries();

    FlyViewSettings* const settings = SettingsManager::instance()->flyViewSettings();
    _maxAttempts = settings ? qBound(kMinAttempts, settings->fleetCommandAttempts()->rawValue().toInt(), kMaxAttempts)
                            : kMinAttempts;
    const int intervalSec =
        settings ? qBound(kMinIntervalSec, settings->fleetCommandIntervalSec()->rawValue().toInt(), kMaxIntervalSec)
                 : kMinIntervalSec;
    _attemptTimer->setInterval(intervalSec * 1000);

    const QmlObjectListModel* const vehicles = MultiVehicleManager::instance()->vehicles();
    for (int i = 0; vehicles && (i < vehicles->count()); i++) {
        Vehicle* const vehicle = vehicles->value<Vehicle*>(i);
        if (!vehicle) {
            continue;
        }

        const quint32 id = vehicle->id();
        VehicleEntry entry;
        entry.vehicle = vehicle;
        entry.targetMode = (command == Command::Land) ? vehicle->landFlightMode() : vehicle->rtlFlightMode();
        if (entry.targetMode.isEmpty()) {
            entry.state = VehicleState::Unsupported;
        } else if (!vehicle->armed()) {
            entry.state = VehicleState::Skipped;
        } else {
            // A vehicle that is already in the target mode still gets every send
            entry.confirmed = (vehicle->flightMode() == entry.targetMode);
            entry.modeConnection = connect(vehicle, &Vehicle::flightModeChanged, this,
                                           [this, id](const QString& mode) { _flightModeChanged(id, mode); });
        }
        _entries.insert(id, entry);
    }

    const bool commandChanged = (_activeCommand != command);
    _activeCommand = command;

    const QList<quint32> ids = _entries.keys();
    for (const quint32 id : ids) {
        const auto it = _entries.constFind(id);
        if ((it != _entries.constEnd()) && (it->state == VehicleState::Pending)) {
            _sendAttempt(id);
        }
    }

    _publish();
    if (commandChanged) {
        emit activeCommandChanged();
    }
    _updateBusy();

    if (_busy) {
        _attemptTimer->start();
    } else {
        emit finished(command);
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

void FleetCommandController::_vehicleRemoved(Vehicle* vehicle)
{
    if (!vehicle) {
        return;
    }

    const auto it = _entries.find(vehicle->id());
    if ((it == _entries.end()) || (it->vehicle != vehicle)) {
        return;
    }

    QObject::disconnect(it->modeConnection);
    _entries.erase(it);
    _settle();
}

void FleetCommandController::_resolve(quint32 id, VehicleState state)
{
    const auto it = _entries.find(id);
    if (it == _entries.end()) {
        return;
    }

    it->state = state;
    QObject::disconnect(it->modeConnection);
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
    } else if (!_attemptTimer->isActive()) {
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
                                                     {QStringLiteral("step"), static_cast<int>(TakeoffStep::None)},
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
    for (const VehicleEntry& entry : std::as_const(_entries)) {
        QObject::disconnect(entry.modeConnection);
    }
    _entries.clear();
}
