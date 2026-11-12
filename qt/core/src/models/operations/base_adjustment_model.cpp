/**
 * @file base_adjustment_model.cpp
 * @brief Implementation of BaseAdjustmentModel
 * @author CaptureMoment Team
 * @date 2025
 */

#include "models/operations/base_adjustment_model.h"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace CaptureMoment::UI::Models::Operations
{

BaseAdjustmentModel::BaseAdjustmentModel(QObject* parent)
    : OperationProvider(parent), m_params{}
{
    // Initialize the parameter structure's value.
    m_params.value = 0.0f; // Default: no adjustment
    spdlog::debug("[BaseAdjustmentModel::BaseAdjustmentModel] Created with default value {}", m_params.value);
}

void BaseAdjustmentModel::setValue(float val)
{
    // Clamp the incoming value to the allowed range [-1.0, 1.0] (or whatever minimum()/maximum() returns)
    float clamped_value { std::clamp(val, minimum(), maximum()) };

    // Check if the value has actually changed to avoid unnecessary updates/signals.
    if (qFuzzyCompare(m_params.value, clamped_value)) {
        return; // No change, exit early.
    }

    bool was_active { m_params.isActive() };

    // Update the internal parameter structure.
    m_params.value = clamped_value;

    bool is_now_active { isActive() };
    emit valueChanged(m_params.value); // Emit inherited signal

    if (was_active != is_now_active) {
        emit isActiveChanged(); // Emit inherited signal from OperationProvider
        spdlog::debug("[BaseAdjustmentModel::setValue]: Activity state changed to {}", is_now_active);
    }
}

} // namespace CaptureMoment::UI::Models::Operations
