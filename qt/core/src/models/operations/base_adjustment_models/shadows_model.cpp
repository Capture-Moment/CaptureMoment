/**
 * @file shadows_model.cpp
 * @brief Implementation of ShadowsModel inheriting from BaseAdjustmentModel
 * @author CaptureMoment Team
 * @date 2025
 */

#include "models/operations/basic_adjustment_models/shadows_model.h"
#include <spdlog/spdlog.h>

namespace CaptureMoment::UI::Models::Operations {

ShadowsModel::ShadowsModel(QObject* parent)
    : BaseAdjustmentModel(parent)
{
    m_params.value = Core::Operations::OperationRanges::getShadowsDefaultValue();
    spdlog::debug("[ShadowsModel::ShadowsModel]: Created (inherits default value from BaseAdjustmentModel)");
}

Core::Operations::OperationDescriptor ShadowsModel::getDescriptor() const
{
    Core::Operations::OperationDescriptor descriptor;
    descriptor.id = m_operation_id;
    descriptor.type = Core::Operations::OperationType::Shadows;
    descriptor.name = "Shadows (" + std::to_string(static_cast<int>(m_params.value * 100)) + "%)";
    descriptor.enabled = true;
    descriptor.setParam<float>("value", m_params.value);
    return descriptor;
}

void ShadowsModel::reset()
{
    spdlog::debug("[ShadowsModel::reset]: Resetting to default value ({})", Core::Operations::OperationRanges::getShadowsDefaultValue());
    setValue(Core::Operations::OperationRanges::getShadowsDefaultValue());
}

// Handles successful operation completion from ImageControllerBase.
void ShadowsModel::onOperationCompleted()
{
    spdlog::debug("[ShadowsModel::onOperationCompleted]: Operation Completed successfully");
    emit operationApplied();
}

// Handles operation failure from ImageControllerBase.
void ShadowsModel::onOperationFailed(const QString& error)
{
    spdlog::error("[ShadowsModel::onOperationFailed]: Operation failed - {}", error.toStdString());
    emit operationFailed(error);
}

} // namespace CaptureMoment::UI::Models::Operations
