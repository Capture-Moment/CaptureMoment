#pragma once
/**
 * @file gpu_features.h
 * @brief Compile-time GPU backend availability per platform.
 *
 * Single source of truth for which Halide GPU backends make sense on the
 * current platform. Kept as constexpr so the compiler eliminates the
 * "unavailable" branches entirely — no runtime cost, no #ifdef at call sites.
 * 
 * @author CaptureMoment Team
 * @date 2026
 */

#include <Halide.h>

namespace CaptureMoment::Core {
namespace ImageProcessing {

#if defined(_WIN32)
inline constexpr bool k_has_d3d12 = true;
#else
inline constexpr bool k_has_d3d12 = false;
#endif

#if defined(__APPLE__)
inline constexpr bool k_has_metal = true;
// Apple dropped CUDA driver support after macOS 10.13.
inline constexpr bool k_has_cuda = false;
#else
inline constexpr bool k_has_metal = false;
inline constexpr bool k_has_cuda = true;
#endif

/**
 * @brief Whether probing this GPU backend can possibly succeed on this platform.
 */
inline bool platform_supports(Halide::Target::Feature feature)
{
    switch (feature)
    {
    case Halide::Target::Metal:         return k_has_metal;
    case Halide::Target::D3D12Compute:  return k_has_d3d12;
    case Halide::Target::CUDA:          return k_has_cuda;
    case Halide::Target::Vulkan:
    case Halide::Target::OpenCL:
        return true;
    default:
        return false;
    }
}

} // namespace ImageProcessing
} // namespace CaptureMoment::Core
