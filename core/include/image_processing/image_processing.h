/**
 * @file image_processing.h
 * @brief Umbrella header for the ImageProcessing module.
 *
 * This header provides a single entry point for the `CaptureMoment::Core::ImageProcessing` namespace.
 * Including this file grants access to all interfaces and concrete implementations
 * related to image storage (CPU/GPU), backend decision, and memory management.
 *
 * This module is responsible for:
 * - **Abstraction**: Hiding the complexity of CPU vs GPU memory access.
 * - **Backends**: providing optimized CPU (Vector/Halide) and GPU (Halide) implementations.
 * - **Selection**: Deciding which backend to use (Benchmarking, Hardware Detection).
 * - **Factory**: Creating the appropriate working image instances based on configuration.
 *
 * @author CaptureMoment Team
 * @date 2026
 */

#pragma once

// ============================================================
// 1. Interfaces
// ============================================================

/**
 * @brief Main hardware abstraction interface.
 * Defines the contract for any image buffer (CPU or GPU).
 */
#include "image_processing/interfaces/i_working_image_hardware.h"

/**
 * @brief Interface for backend selection strategies.
 */
#include "image_processing/interfaces/i_backend_decider.h"

// ============================================================
// 2. Concrete Implementations
// ============================================================

// --- Base Classes Halide ---
#include "image_processing/halide/working_image_halide.h"

// --- CPU Implementations ---
#include "image_processing/cpu/working_image_cpu_default.h"
#include "image_processing/cpu/working_image_cpu_halide.h"

/**
 * @brief Specific class for CPU-based image buffers.
 */
#include "image_processing/cpu/working_image_cpu.h"


/**
 * @brief Specific class for GPU-based image buffers.
 */
#include "image_processing/gpu/working_image_gpu.h"

// --- GPU Implementations ---
/**
 * @brief Specific class for GPU Halide-based image buffers.
 */
#include "image_processing/gpu/working_image_gpu_halide.h"

// --- Common Implementations ---
/**
 * @brief Base class providing raw buffer storage and metadata for all working image implementations.
 */
#include "image_processing/common/working_image_data.h"

/**
 * @brief Struct representing the image data view passed to hardware workers.
 * Contains spans for working and original data, as well as geometry information.
 */
#include "image_processing/common/image_view.h"

// ============================================================
// 3. Factories
// ============================================================

/**
 * @brief Factory for creating working images based on MemoryType.
 * Uses a Registry pattern to allow dynamic backend registration.
 */
#include "image_processing/factories/working_image_factory.h"

// ============================================================
// 4. Deciders
// ============================================================

/**
 * @brief Benchmark-based backend decider.
 * Runs performance tests to select the fastest available backend.
 */
#include "image_processing/deciders/benchmarking_backend_decider.h"


// ============================================================
// 5. Contexts
// ============================================================

/**
 * @brief Manages the lifecycle of a single WorkingImage instance.
 */
#include "image_processing/working_image_context.h"

// ============================================================
// 6. Registration
// ============================================================
/**
 * @brief Registers all default backends to the factory.
 * Must be called once at application startup.
 */
#include "image_processing/registration/working_image_registration.h"

// ============================================================
// 7. Helper
// ============================================================

/**
 * @brief Provides compile-time GPU backend availability per platform.
 */
#include "image_processing/helper/gpu_features.h"
