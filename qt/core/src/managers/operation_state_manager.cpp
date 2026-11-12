/**
 * @file operation_state_manager.cpp
 * @brief Implementation of OperationStateManager
 * @author CaptureMoment Team
 * @date 2025
 */

#include "managers/operation_state_manager.h"
#include "operations/operation_descriptor.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace CaptureMoment::UI::Managers {

OperationStateManager::OperationStateManager()
{
    spdlog::debug("[OperationStateManager::OperationStateManager]: Constructed.");
}

void OperationStateManager::addOrUpdateOperation(const Core::Operations::OperationDescriptor& descriptor)
{
    std::scoped_lock lock(m_mutex);
    spdlog::debug("[OperationStateManager::addOrUpdateOperation]: Adding/updating operation '{}'.", descriptor.name);

    // Find if an operation of the same type already exists
    auto it { std::find_if(m_active_operations.begin(), m_active_operations.end(),
                           [&descriptor](const auto& op) { return op.type == descriptor.type; }) };

    if (it != m_active_operations.end()) {
        // Update existing operation
        *it = descriptor;
        spdlog::debug("[OperationStateManager::addOrUpdateOperation]: Updated operation '{}'.", descriptor.name);
    } else {
        // Add new operation
        m_active_operations.push_back(descriptor);
        spdlog::debug("[OperationStateManager::addOrUpdateOperation]: Added new operation '{}'.", descriptor.name);
    }
}

void OperationStateManager::removeOperation(Core::Operations::OperationType type)
{
    std::scoped_lock lock(m_mutex);
    spdlog::debug("[OperationStateManager::removeOperation]: Removing operation type '{}'.", static_cast<int>(type));

    // Remove operation of the specified type
    m_active_operations.erase(
        std::remove_if(m_active_operations.begin(), m_active_operations.end(),
                       [type](const auto& op) { return op.type == type; }),
        m_active_operations.end()
        );

    spdlog::debug("[OperationStateManager::removeOperation]: Operation type '{}' removed (if it existed).", static_cast<int>(type));
    // Note: applyCurrentOperationsToEngine() is no longer called here.
    // That responsibility belongs to ImageControllerBase.
}

void OperationStateManager::clearAllOperations()
{
    std::scoped_lock lock(m_mutex);
    spdlog::debug("[OperationStateManager::clearAllOperations]: Clearing all operations.");

    m_active_operations.clear();
    spdlog::debug("[OperationStateManager::clearAllOperations]: All operations cleared.");
}

std::vector<Core::Operations::OperationDescriptor> OperationStateManager::getActiveOperations() const
{
    std::scoped_lock lock(m_mutex);
    // Return a copy of the vector, safe for concurrent access by the caller
    return m_active_operations;
}

} // namespace CaptureMoment::UI::Managers
