/**
 * @file halide_color_space_ops.h
 * @brief Pure mathematical Halide functions for color space conversions.
 *
 * @author CaptureMoment Team
 * @date 2026
 */

#pragma once

#include <Halide.h>

namespace CaptureMoment::Core::Pipeline {

namespace ColorSpace {

/**
 * @brief Converts RGB Halide::Func to Oklab.
 *
 * The Alpha channel (c == 3) is left completely untouched.
 *
 * @param input The input function providing RGB(A) data.
 * @param x Halide Var for width.
 * @param y Halide Var for height.
 * @param c Halide Var for channels.
 * @return Halide::Func A new function containing L, a, b, Alpha.
 */
inline Halide::Func rgbToOklab(const Halide::Func& input, const Halide::Var& x, const Halide::Var& y, const Halide::Var& c)
{
    Halide::Func oklab_func("rgb_to_oklab");

    // 1. Read linear RGB (assumes input is already linear, if sRGB, add gamma removal here)
    Halide::Expr r { input(x, y, 0) };
    Halide::Expr g { input(x, y, 1) };
    Halide::Expr b { input(x, y, 2) };

    // 2. Linear RGB to LMS (Cone responses)
    Halide::Expr l { 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b };
    Halide::Expr m { 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b };
    Halide::Expr s { 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b };

    // 3. Cube root of LMS (Sign-aware to prevent NaN on negative values)
    // Halide::select is evaluated per-pixel at compile time to generate branching-less SIMD code.
    Halide::Expr l_ { select(l >= 0.0f, pow(l, 1.0f / 3.0f), -pow(-l, 1.0f / 3.0f)) };
    Halide::Expr m_ { select(m >= 0.0f, pow(m, 1.0f / 3.0f), -pow(-m, 1.0f / 3.0f)) };
    Halide::Expr s_ { select(s >= 0.0f, pow(s, 1.0f / 3.0f), -pow(-s, 1.0f / 3.0f)) };

    // 4. LMS' to Oklab using M2 matrix
    Halide::Expr L { 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_ };
    Halide::Expr a { 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_ };
    Halide::Expr b_val { 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_ };

    // 5. Define the output, preserving Alpha on channel 3
    oklab_func(x, y, c) = select(
        c == 0, L,
        c == 1, a,
        c == 2, b_val,
        input(x, y, c) // c == 3 (Alpha)
    );

    return oklab_func;
}

/**
 * @brief Converts a Chunky Oklab Halide::Func back to linear RGB.
 *
 * The Alpha channel (c == 3) is left completely untouched.
 *
 * @param input The input function providing Lab(A) data.
 * @param x Halide Var for width.
 * @param y Halide Var for height.
 * @param c Halide Var for channels.
 * @return Halide::Func A new function containing R, G, B, Alpha.
 */
inline Halide::Func oklabToRgb(const Halide::Func& input, const Halide::Var& x, const Halide::Var& y, const Halide::Var& c)
{

    Halide::Func rgb_func("oklab_to_rgb");

    // 1. Read Oklab
    Halide::Expr L { input(x, y, 0) };
    Halide::Expr a { input(x, y, 1) };
    Halide::Expr b_val { input(x, y, 2) };

    // 2. Oklab to LMS' (Inverse M2 matrix)
    Halide::Expr l_ { L + 0.3963377774f * a + 0.2158037573f * b_val };
    Halide::Expr m_ { L - 0.1055613458f * a - 0.0638541728f * b_val };
    Halide::Expr s_ { L - 0.0894841775f * a - 1.2914855480f * b_val };

    // 3. Cube LMS' (Inverse perceptual non-linearity)
    // No need for select() here, multiplying a negative number by itself stays safe.
    Halide::Expr l { l_ * l_ * l_ };
    Halide::Expr m { m_ * m_ * m_ };
    Halide::Expr s { s_ * s_ * s_ };

    // 4. LMS to Linear RGB (Inverse M1 matrix)
    Halide::Expr r { +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s };
    Halide::Expr g { -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s };
    Halide::Expr b_out { -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s };

    // 5. Define the output, preserving Alpha
    rgb_func(x, y, c) = select(
        c == 0, r,
        c == 1, g,
        c == 2, b_out,
        input(x, y, c) // c == 3 (Alpha)
    );

    return rgb_func;
}

/**
 * @brief Converts linear ProPhoto (ROMM RGB, D50) Halide::Func to Oklab.
 *
 * @param input The input function providing linear ProPhoto RGBA data.
 * @param x Halide Var for width.
 * @param y Halide Var for height.
 * @param c Halide Var for channels.
 * @return Halide::Func A new function containing L, a, b, Alpha.
 */
inline Halide::Func proPhotoLinearToOklab(const Halide::Func& input, const Halide::Var& x, const Halide::Var& y, const Halide::Var& c)
{
    Halide::Func oklab_func("prophoto_linear_to_oklab");

    // 1. Read linear ProPhoto. No gamma removal here: ROMM data is expected
    //    already linear (produced by prophoto_linear OIIO color transform).
    Halide::Expr r { input(x, y, 0) };
    Halide::Expr g { input(x, y, 1) };
    Halide::Expr b { input(x, y, 2) };

    // 2. Linear ProPhoto to LMS' (M1 composed with Bradford D50->D65,
    //    see file header). White point normalized: (1,1,1) -> LMS' (1,1,1).
    Halide::Expr l { 0.7153289120f * r + 0.3529100770f * g - 0.0682558242f * b };
    Halide::Expr m { 0.2743542906f * r + 0.6678509662f * g + 0.0577945291f * b };
    Halide::Expr s { 0.1097541052f * r + 0.1862089904f * g + 0.7040358683f * b };

    // 3. Cube root of LMS (Sign-aware to handle out-of-gamut negative values
    //    without producing NaN).
    Halide::Expr l_ { select(l >= 0.0f, pow(l, 1.0f / 3.0f), -pow(-l, 1.0f / 3.0f)) };
    Halide::Expr m_ { select(m >= 0.0f, pow(m, 1.0f / 3.0f), -pow(-m, 1.0f / 3.0f)) };
    Halide::Expr s_ { select(s >= 0.0f, pow(s, 1.0f / 3.0f), -pow(-s, 1.0f / 3.0f)) };

    // 4. LMS' to Oklab using M2 matrix (unchanged from the reference Oklab:
    //    M2 operates on LMS', it does not depend on the input primaries).
    Halide::Expr L { 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_ };
    Halide::Expr a { 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_ };
    Halide::Expr b_val { 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_ };

    // 5. Define the output, preserving Alpha on channel 3.
    oklab_func(x, y, c) = select(
        c == 0, L,
        c == 1, a,
        c == 2, b_val,
        input(x, y, c) // c == 3 (Alpha)
    );

    return oklab_func;
}

/**
 * @brief Converts a Chunky Oklab Halide::Func back to linear ProPhoto (ROMM RGB, D50).
 *
 * @param input The input function providing Lab(A) data.
 * @param x Halide Var for width.
 * @param y Halide Var for height.
 * @param c Halide Var for channels.
 * @return Halide::Func A new function containing R, G, B, Alpha in linear ProPhoto.
 */
inline Halide::Func oklabToProPhotoLinear(const Halide::Func& input, const Halide::Var& x, const Halide::Var& y, const Halide::Var& c)
{
    Halide::Func rgb_func("oklab_to_prophoto_linear");

    // 1. Read Oklab
    Halide::Expr L { input(x, y, 0) };
    Halide::Expr a { input(x, y, 1) };
    Halide::Expr b_val { input(x, y, 2) };

    // 2. Oklab to LMS' (Inverse M2 matrix, unchanged from the reference).
    Halide::Expr l_ { L + 0.3963377774f * a + 0.2158037573f * b_val };
    Halide::Expr m_ { L - 0.1055613458f * a - 0.0638541728f * b_val };
    Halide::Expr s_ { L - 0.0894841775f * a - 1.2914855480f * b_val };

    // 3. Cube LMS' (Inverse perceptual non-linearity).
    //    No need for select() here, cubing a negative number stays safe.
    Halide::Expr l { l_ * l_ * l_ };
    Halide::Expr m { m_ * m_ * m_ };
    Halide::Expr s { s_ * s_ * s_ };

    // 4. LMS to linear ProPhoto (inverse of the composed M1, see file header).
    Halide::Expr r { +1.7387392019f * l - 0.9884191746f * m + 0.2497092918f * s };
    Halide::Expr g { -0.7070022878f * l + 1.9343231884f * m - 0.2273326246f * s };
    Halide::Expr b_out { -0.0840633067f * l - 0.3575177308f * m + 1.4415810392f * s };

    // 5. Define the output, preserving Alpha.
    rgb_func(x, y, c) = select(
        c == 0, r,
        c == 1, g,
        c == 2, b_out,
        input(x, y, c) // c == 3 (Alpha)
    );

    return rgb_func;

} // namespace ColorSpace

} // namespace CaptureMoment::Core::Pipeline
