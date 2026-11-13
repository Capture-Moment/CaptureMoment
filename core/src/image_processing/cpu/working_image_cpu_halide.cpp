/**
 * @file working_image_cpu_halide.cpp
 * @brief Implementation of WorkingImageCPU_Halide
 * @author CaptureMoment Team
 * @date 2025
 */

#include "image_processing/cpu/working_image_cpu_halide.h"

#include <spdlog/spdlog.h>

namespace CaptureMoment::Core::ImageProcessing {

bool WorkingImageCPU_Halide::bindView(const ImageView& view)
{
    // 1. Bind the view to the CPU base class (stores spans and geometry)
    if (!WorkingImageCPU::bindView(view)) {
        return false;
    }

    // 2. Check the view data pointer before initializing Halide
    if (m_view_data_image.working_data.data() == nullptr) {
        spdlog::error("[WorkingImageCPU_Halide::bindView]: View data pointer is null.");
        return false;
    }

    initializeHalide(
        m_view_data_image.working_data,
        static_cast<int>(m_view_data_image.width),
        static_cast<int>(m_view_data_image.height),
        static_cast<int>(m_view_data_image.channels)
        );

    if (!m_halide_buffer.defined()) {
        spdlog::error("[WorkingImageCPU_Halide::bindView]: Failed to initialize Halide buffer.");
        return false;
    }

    m_halide_original_buffer = Halide::Buffer<float>::make_interleaved(
        const_cast<float*>(m_view_data_image.original_data.data()),
        static_cast<int>(m_view_data_image.width),
        static_cast<int>(m_view_data_image.height),
        static_cast<int>(m_view_data_image.channels)
        );

    spdlog::debug("[WorkingImageCPU_Halide::bindView]: Bound and initialized Halide ({}x{}, {} ch).",
                  m_view_data_image.width, m_view_data_image.height, m_view_data_image.channels);
    return true;
}

} // namespace CaptureMoment::Core::ImageProcessing
