/**
 * @file brightness_model.cpp
 * @brief Implementation of BrightnessModel inheriting from BaseAdjustmentModel
 * @author CaptureMoment Team
 * @date 2025
 */

#include "models/operations/basic_adjustment_models/brightness_model.h"
#include <spdlog/spdlog.h>

namespace CaptureMoment::UI::Models::Operations {

BrightnessModel::BrightnessModel(QObject* parent)
    : BaseAdjustmentModel(parent)
{
    m_params.value = Core::Operations::OperationRanges::getBrightnessDefaultValue();
    spdlog::debug("[BrightnessModel::BrightnessModel]: Created (inherits default value from BaseAdjustmentModel)");
}

Core::Operations::OperationDescriptor BrightnessModel::getDescriptor() const
{
    Core::Operations::OperationDescriptor descriptor;
    descriptor.id = m_operation_id;
    descriptor.type = Core::Operations::OperationType::Brightness;
    descriptor.name = "Brightness (" + std::to_string(static_cast<int>(m_params.value * 100)) + "%)";
    descriptor.enabled = true;
    descriptor.setParam<float>("value", m_params.value);
    return descriptor;
}

void BrightnessModel::reset()
{
    spdlog::debug("[BrightnessModel::reset]: Resetting to default value ({})", Core::Operations::OperationRanges::getBrightnessDefaultValue());
    setValue(Core::Operations::OperationRanges::getBrightnessDefaultValue());
}

// Handles successful operation completion from ImageControllerBase.
void BrightnessModel::onOperationCompleted()
{
    spdlog::debug("[BrightnessModel::onOperationCompleted]: Operation Completed successfully");
    emit operationApplied();
}

// Handles operation failure from ImageControllerBase.
void BrightnessModel::onOperationFailed(const QString& error)
{
    spdlog::error("[BrightnessModel::onOperationFailed]: Operation failed - {}", error.toStdString());
    emit operationFailed(error);
}

} // namespace CaptureMoment::UI::Models::Operations
