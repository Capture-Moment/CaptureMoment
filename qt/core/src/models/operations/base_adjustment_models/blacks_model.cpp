/**
 * @file blacks_model.cpp
 * @brief Implementation of BlacksModel inheriting from BaseAdjustmentModel
 * @author CaptureMoment Team
 * @date 2025
 */

#include "models/operations/basic_adjustment_models/blacks_model.h"
#include <spdlog/spdlog.h>

namespace CaptureMoment::UI::Models::Operations {

BlacksModel::BlacksModel(QObject* parent)
    : BaseAdjustmentModel(parent)
{
    m_params.value = Core::Operations::OperationRanges::getBlacksDefaultValue();
    spdlog::debug("[BlacksModel::BlacksModel]: Created (inherits default value from BaseAdjustmentModel)");
}

Core::Operations::OperationDescriptor BlacksModel::getDescriptor() const
{
    Core::Operations::OperationDescriptor descriptor;
    descriptor.id = m_operation_id;
    descriptor.type = Core::Operations::OperationType::Blacks;
    descriptor.name = "Blacks (" + std::to_string(static_cast<int>(m_params.value * 100)) + "%)";
    descriptor.enabled = true;
    descriptor.setParam<float>("value", m_params.value);
    return descriptor;
}

void BlacksModel::reset()
{
    spdlog::debug("[BlacksModel::reset]: Resetting to default value ({})", Core::Operations::OperationRanges::getBlacksDefaultValue());
    setValue(Core::Operations::OperationRanges::getBlacksDefaultValue());
}

// Handles successful operation completion from ImageControllerBase.
void BlacksModel::onOperationCompleted()
{
    spdlog::debug("[BlacksModel::onOperationCompleted]: Operation Completed successfully");
    emit operationApplied();
}

// Handles operation failure from ImageControllerBase.
void BlacksModel::onOperationFailed(const QString& error)
{
    spdlog::error("[BlacksModel::onOperationFailed]: Operation failed - {}", error.toStdString());
    emit operationFailed(error);
}

} // namespace CaptureMoment::UI::Models::Operations
