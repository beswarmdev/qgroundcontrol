#include "FleetCommandController.h"

#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(FleetCommandControllerLog, "Vehicle.FleetCommandController")

FleetCommandController::FleetCommandController(QObject* parent)
    : QObject(parent)
{}

FleetCommandController::~FleetCommandController() = default;

void FleetCommandController::rtlAll()
{
    qCDebug(FleetCommandControllerLog) << "not implemented";
}

void FleetCommandController::landAll()
{
    qCDebug(FleetCommandControllerLog) << "not implemented";
}

void FleetCommandController::cancel()
{
    qCDebug(FleetCommandControllerLog) << "not implemented";
}
