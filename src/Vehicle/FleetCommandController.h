#pragma once

#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtQmlIntegration/QtQmlIntegration>

Q_DECLARE_LOGGING_CATEGORY(FleetCommandControllerLog)

/// Sends RTL or Land to every connected vehicle and retries until each one reports the target flight mode.
class FleetCommandController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(Command activeCommand READ activeCommand NOTIFY activeCommandChanged)
    Q_PROPERTY(QVariantMap vehicleStatus READ vehicleStatus NOTIFY vehicleStatusChanged)

public:
    explicit FleetCommandController(QObject* parent = nullptr);
    ~FleetCommandController() override;

    enum class Command
    {
        None,
        RTL,
        Land
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

    Q_INVOKABLE void rtlAll();
    Q_INVOKABLE void landAll();
    Q_INVOKABLE void cancel();

    bool busy() const { return _busy; }

    Command activeCommand() const { return _activeCommand; }

    QVariantMap vehicleStatus() const { return _vehicleStatus; }

    static constexpr int kAttemptTimeoutMs = 2000;
    static constexpr int kMinAttempts = 1;
    static constexpr int kMaxAttempts = 20;

signals:
    void busyChanged();
    void activeCommandChanged();
    void vehicleStatusChanged();
    void finished(FleetCommandController::Command command);

private:
    bool _busy = false;
    Command _activeCommand = Command::None;
    QVariantMap _vehicleStatus;
};
