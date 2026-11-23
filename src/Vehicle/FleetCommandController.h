#pragma once

#include <QtCore/QLoggingCategory>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QVariantMap>
#include <QtQmlIntegration/QtQmlIntegration>

class QTimer;
class Vehicle;

Q_DECLARE_LOGGING_CATEGORY(FleetCommandControllerLog)

/// Sends RTL, Land, Takeoff or Reboot to every connected vehicle and tracks the result per vehicle.
class FleetCommandController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(Command activeCommand READ activeCommand NOTIFY activeCommandChanged)
    Q_PROPERTY(QVariantMap vehicleStatus READ vehicleStatus NOTIFY vehicleStatusChanged)
    Q_PROPERTY(double takeoffAltitude READ takeoffAltitude CONSTANT)

public:
    explicit FleetCommandController(QObject* parent = nullptr);
    ~FleetCommandController() override;

    enum class Command
    {
        None,
        RTL,
        Land,
        Takeoff,
        Reboot
    };
    Q_ENUM(Command)

    enum class VehicleState
    {
        Pending,
        Succeeded,
        Failed,
        Unsupported,
        Skipped,
        Cancelled
    };
    Q_ENUM(VehicleState)

    /// Where a takeoff is, or where it stopped when it failed
    enum class TakeoffStep
    {
        None,
        Mode,
        Arm,
        Climb
    };
    Q_ENUM(TakeoffStep)

    Q_INVOKABLE void rtlAll();
    Q_INVOKABLE void landAll();
    Q_INVOKABLE void takeoffAll();
    Q_INVOKABLE void rebootAll();
    Q_INVOKABLE void cancel();

    bool busy() const { return _busy; }

    Command activeCommand() const { return _activeCommand; }

    QVariantMap vehicleStatus() const { return _vehicleStatus; }

    double takeoffAltitude() const { return kTakeoffAltitudeMeters; }

    static constexpr int kAttemptTimeoutMs = 2000;
    static constexpr int kMinAttempts = 1;
    static constexpr int kMaxAttempts = 20;
    static constexpr int kMinIntervalSec = 1;
    static constexpr int kMaxIntervalSec = 60;
    static constexpr double kTakeoffAltitudeMeters = 5.0;
    static constexpr int kTakeoffStepTimeoutMs = 5000;
    static constexpr int kTakeoffClimbTimeoutMs = 15000;
    static constexpr int kRebootTimeoutMs = 6000;

signals:
    void busyChanged();
    void activeCommandChanged();
    void vehicleStatusChanged();
    void finished(FleetCommandController::Command command);

private:
    struct VehicleEntry
    {
        QPointer<Vehicle> vehicle;
        QString targetMode;
        VehicleState state = VehicleState::Pending;
        int attempt = 0;
        QMetaObject::Connection modeConnection;
    };

    void _start(Command command);
    void _sendAttempt(quint32 id);
    void _attemptWindowElapsed();
    void _flightModeChanged(quint32 id, const QString& mode);
    void _vehicleRemoved(Vehicle* vehicle);
    void _resolve(quint32 id, VehicleState state);
    void _settle();
    void _publish();
    void _updateBusy();
    void _releaseEntries();

    bool _busy = false;
    Command _activeCommand = Command::None;
    QVariantMap _vehicleStatus;
    QMap<quint32, VehicleEntry> _entries;
    QTimer* _attemptTimer = nullptr;
    int _maxAttempts = 0;
};
