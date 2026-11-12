/**
 * @file contrast_model.cpp
 * @brief Implementation of ContrastModel inheriting from BaseAdjustmentModel
 * @author CaptureMoment Team
 * @date 2025
 */

#include "models/operations/basic_adjustment_models/contrast_model.h"
#include <spdlog/spdlog.h>

namespace CaptureMoment::UI::Models::Operations {

ContrastModel::ContrastModel(QObject* parent)
    : BaseAdjustmentModel(parent)
{
    m_params.value = Core::Operations::OperationRanges::getContrastDefaultValue();
    spdlog::debug("[ContrastModel::ContrastModel]: (inherits default value from BaseAdjustmentModel)");
}

Core::Operations::OperationDescriptor ContrastModel::getDescriptor() const
{
    Core::Operations::OperationDescriptor descriptor;
    descriptor.id = m_operation_id;
    descriptor.type = Core::Operations::OperationType::Contrast;
    descriptor.name = "Contrast (" + std::to_string(static_cast<int>(m_params.value * 100)) + "%)";
    descriptor.enabled = true;
    descriptor.setParam<float>("value", m_params.value);
    return descriptor;
}

void ContrastModel::reset()
{
    spdlog::debug("[ContrastModel::reset]: Resetting to default value ({})", Core::Operations::OperationRanges::getContrastDefaultValue());
    setValue(Core::Operations::OperationRanges::getContrastDefaultValue());
}

// Handles successful operation completion from ImageControllerBase.
void ContrastModel::onOperationCompleted()
{
    spdlog::debug("[ContrastModel::onOperationCompleted]: Operation Completed successfully");
    emit operationApplied();
}

// Handles operation failure from ImageControllerBase.
void ContrastModel::onOperationFailed(const QString& error)
{
    spdlog::error("[ContrastModel::onOperationFailed]: Operation failed - {}", error.toStdString());
    emit operationFailed(error);
}

} // namespace CaptureMoment::UI::Models::Operations
