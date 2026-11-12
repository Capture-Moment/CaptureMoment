/**
 * @file highlights_model.cpp
 * @brief Implementation of HighlightsModel inheriting from BaseAdjustmentModel
 * @author CaptureMoment Team
 * @date 2025
 */

#include "models/operations/basic_adjustment_models/highlights_model.h"
#include <spdlog/spdlog.h>

namespace CaptureMoment::UI::Models::Operations {

HighlightsModel::HighlightsModel(QObject* parent)
    : BaseAdjustmentModel(parent)
{
    m_params.value = Core::Operations::OperationRanges::getHighlightsDefaultValue();
    spdlog::debug("[HighlightsModel::HighlightsModel]: Created (inherits default value from BaseAdjustmentModel)");
}

Core::Operations::OperationDescriptor HighlightsModel::getDescriptor() const
{
    Core::Operations::OperationDescriptor descriptor;
    descriptor.id = m_operation_id;
    descriptor.type = Core::Operations::OperationType::Highlights;
    descriptor.name = "Highlights (" + std::to_string(static_cast<int>(m_params.value * 100)) + "%)";
    descriptor.enabled = true;
    descriptor.setParam<float>("value", m_params.value);
    return descriptor;
}

void HighlightsModel::reset()
{
    spdlog::debug("[HighlightsModel::reset]: Resetting to default value ({})", Core::Operations::OperationRanges::getHighlightsDefaultValue());
    setValue(Core::Operations::OperationRanges::getHighlightsDefaultValue());
}

// Handles successful operation completion from ImageControllerBase.
void HighlightsModel::onOperationCompleted()
{
    spdlog::debug("[HighlightsModel::onOperationCompleted]: Operation Completed successfully");
    emit operationApplied();
}

// Handles operation failure from ImageControllerBase.
void HighlightsModel::onOperationFailed(const QString& error)
{
    spdlog::error("[HighlightsModel::onOperationFailed]: Operation failed - {}", error.toStdString());
    emit operationFailed(error);
}

} // namespace CaptureMoment::UI::Models::Operations
