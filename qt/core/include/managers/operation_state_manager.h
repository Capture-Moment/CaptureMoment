/**
 * @file operation_state_manager.h
 * @brief Manages the state of active image operations.
 * @author CaptureMoment Team
 * @date 2025
 */

#pragma once

#include "operations/operation_descriptor.h"
#include <vector>
#include <mutex>

#include <spdlog/spdlog.h>

namespace CaptureMoment::UI {

namespace Managers {

/**
 * @brief Manages the state of active image operations.
 *
 * This class is responsible for maintaining the list of currently active operations
 * (e.g., brightness +0.5, contrast -0.2).
 * It does NOT directly interact with PhotoEngine or ImageControllerBase.
 * Its purpose is solely to store the state of operations and provide a thread-safe
 * method to retrieve the current list of active operations.
 */
class OperationStateManager {
public:
    /**
     * @brief Constructs an OperationStateManager.
     */
    explicit OperationStateManager();

    ~OperationStateManager() = default;

    // Disable copy and assignment
    OperationStateManager(const OperationStateManager&) = delete;
    OperationStateManager& operator=(const OperationStateManager&) = delete;

    /**
     * @brief Init operations
     *
     * @param descriptors list of the operations
     */
    void initialize(const std::vector<Core::Operations::OperationDescriptor>& descriptors);

    /**
     * @brief Adds or updates an operation in the active state.
     *
     * If an operation of the same type already exists, its parameters are updated.
     * Otherwise, a new operation is added to the list.
     *
     * @param descriptor The operation descriptor to add or update.
     */
    void addOrUpdateOperation(const Core::Operations::OperationDescriptor& descriptor);

    /**
     * @brief Removes an operation of a specific type from the active state.
     *
     * If no operation of the given type exists, this method does nothing.
     *
     * @param type The type of the operation to remove.
     */
    void removeOperation(Core::Operations::OperationType type);

    /**
     * @brief Clears all active operations, resetting the state to empty (original image with no ops).
     */
    void clearAllOperations();

    /**
     * @brief Gets the current list of active operation descriptors.
     *
     * This method is thread-safe.
     * @return A copy of the vector containing the active operation descriptors.
     */
    [[nodiscard]] std::vector<Core::Operations::OperationDescriptor> getActiveOperations() const;

private:
    mutable std::mutex m_mutex;
    std::vector<Core::Operations::OperationDescriptor> m_active_operations;
};

} // namespace Managers

} // namespace CaptureMoment::UI
