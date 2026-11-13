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
 * @brief Row-major 3x3 matrix of float coefficients.
 *
 * Used to parameterize the color space conversion cores below: the only
 * difference between the sRGB and ProPhoto variants of a conversion is the
 * matrix mapping linear RGB to (or from) the LMS cone responses.
 */
struct Matrix3 {
    float a11, a12, a13;
    float a21, a22, a23;
    float a31, a32, a33;
};

/**
 * @brief Converts a Chunky linear RGB Halide::Func to Oklab.
 *
 * The Alpha channel (c == 3) is left completely untouched.
 *
 * The matrix m maps the linear RGB of the source color space to the LMS
 * cone responses (Ottosson's M1 for sRGB; M1 composed with the Bradford
 * D50->D65 adaptation for ProPhoto). The cube root and the M2 matrix are
 * independent of the source primaries and are shared by every conversion.
 *
 * @param input The input function providing linear RGB(A) data.
 * @param x Halide Var for width.
 * @param y Halide Var for height.
 * @param c Halide Var for channels.
 * @param m RGB-to-LMS matrix of the source color space.
 * @param name Debug name given to the resulting Halide::Func.
 * @return Halide::Func A new function containing L, a, b, Alpha.
 */
inline Halide::Func linearRgbToOklab(const Halide::Func& input,
                                     const Halide::Var& x, const Halide::Var& y,
                                     const Halide::Var& c,
                                     const Matrix3& m, const char* name)
{
    Halide::Func oklab_func(name);
    // 1. Read linear RGB. No gamma removal here: the data is expected
    //    already linear (e.g. produced by a linear OIIO color transform).
    Halide::Expr r { input(x, y, 0) };
    Halide::Expr g { input(x, y, 1) };
    Halide::Expr b { input(x, y, 2) };
    // 2. Linear RGB to LMS (Cone responses), matrix of the source space.
    //    White point normalized: (1,1,1) -> LMS' (1,1,1).
    Halide::Expr l { m.a11 * r + m.a12 * g + m.a13 * b };
    Halide::Expr mm { m.a21 * r + m.a22 * g + m.a23 * b };
    Halide::Expr s { m.a31 * r + m.a32 * g + m.a33 * b };
    // 3. Cube root of LMS (Sign-aware to prevent NaN on negative values,
    //    i.e. out-of-gamut colors).
    // Halide::select is evaluated per-pixel at compile time to generate
    // branching-less SIMD code.
    Halide::Expr l_ { select(l >= 0.0f, pow(l, 1.0f / 3.0f), -pow(-l, 1.0f / 3.0f)) };
    Halide::Expr m_ { select(mm >= 0.0f, pow(mm, 1.0f / 3.0f), -pow(-mm, 1.0f / 3.0f)) };
    Halide::Expr s_ { select(s >= 0.0f, pow(s, 1.0f / 3.0f), -pow(-s, 1.0f / 3.0f)) };
    // 4. LMS' to Oklab using M2 matrix (independent of the source primaries).
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
 * @brief Converts a Chunky Oklab Halide::Func back to a linear RGB space.
 *
 * The Alpha channel (c == 3) is left completely untouched.
 *
 * The matrix inv is the inverse of the forward RGB-to-LMS matrix of the
 * target color space. The inverse M2 step and the cube are independent of
 * the target primaries and are shared by every conversion.
 *
 * @param input The input function providing Lab(A) data.
 * @param x Halide Var for width.
 * @param y Halide Var for height.
 * @param c Halide Var for channels.
 * @param inv LMS-to-RGB matrix of the target color space.
 * @param name Debug name given to the resulting Halide::Func.
 * @return Halide::Func A new function containing R, G, B, Alpha in linear RGB.
 */
inline Halide::Func oklabToLinearRgb(const Halide::Func& input,
                                     const Halide::Var& x, const Halide::Var& y,
                                     const Halide::Var& c,
                                     const Matrix3& inv, const char* name)
{
    Halide::Func rgb_func(name);
    // 1. Read Oklab.
    Halide::Expr L { input(x, y, 0) };
    Halide::Expr a { input(x, y, 1) };
    Halide::Expr b_val { input(x, y, 2) };
    // 2. Oklab to LMS' (Inverse M2 matrix, independent of the target primaries).
    Halide::Expr l_ { L + 0.3963377774f * a + 0.2158037573f * b_val };
    Halide::Expr m_ { L - 0.1055613458f * a - 0.0638541728f * b_val };
    Halide::Expr s_ { L - 0.0894841775f * a - 1.2914855480f * b_val };
    // 3. Cube LMS' (Inverse perceptual non-linearity).
    //    No need for select() here, cubing a negative number stays safe.
    Halide::Expr l { l_ * l_ * l_ };
    Halide::Expr m { m_ * m_ * m_ };
    Halide::Expr s { s_ * s_ * s_ };
    // 4. LMS to linear RGB of the target space (inverse of the forward matrix).
    Halide::Expr r { +inv.a11 * l + inv.a12 * m + inv.a13 * s };
    Halide::Expr g { +inv.a21 * l + inv.a22 * m + inv.a23 * s };
    Halide::Expr b_out { +inv.a31 * l + inv.a32 * m + inv.a33 * s };
    // 5. Define the output, preserving Alpha.
    rgb_func(x, y, c) = select(
        c == 0, r,
        c == 1, g,
        c == 2, b_out,
        input(x, y, c) // c == 3 (Alpha)
    );
    return rgb_func;
}

/**
 * @brief Converts a Chunky linear sRGB Halide::Func to Oklab.
 *
 * The Alpha channel (c == 3) is left completely untouched.
 *
 * @param input The input function providing linear RGB(A) data.
 * @param x Halide Var for width.
 * @param y Halide Var for height.
 * @param c Halide Var for channels.
 * @return Halide::Func A new function containing L, a, b, Alpha.
 */
inline Halide::Func rgbToOklab(const Halide::Func& input, const Halide::Var& x,
                               const Halide::Var& y, const Halide::Var& c)
{
    return linearRgbToOklab(input, x, y, c,
        // Ottosson's M1: linear sRGB (D65) -> LMS.
        {0.4122214708f, 0.5363325363f, 0.0514459929f,
         0.2119034982f, 0.6806995451f, 0.1073969566f,
         0.0883024619f, 0.2817188376f, 0.6299787005f},
        "rgb_to_oklab");
}

/**
 * @brief Converts a Chunky Oklab Halide::Func back to linear sRGB.
 *
 * The Alpha channel (c == 3) is left completely untouched.
 *
 * @param input The input function providing Lab(A) data.
 * @param x Halide Var for width.
 * @param y Halide Var for height.
 * @param c Halide Var for channels.
 * @return Halide::Func A new function containing R, G, B, Alpha in linear sRGB.
 */
inline Halide::Func oklabToRgb(const Halide::Func& input, const Halide::Var& x,
                               const Halide::Var& y, const Halide::Var& c)
{
    return oklabToLinearRgb(input, x, y, c,
        // Inverse of Ottosson's M1: LMS -> linear sRGB (D65).
        {+4.0767416621f, -3.3077115913f, +0.2309699292f,
         -1.2684380046f, +2.6097574011f, -0.3413193965f,
         -0.0041960863f, -0.7034186147f, +1.7076147010f},
        "oklab_to_rgb");
}

/**
 * @brief Converts a Chunky linear ProPhoto (ROMM RGB, D50) Halide::Func to Oklab.
 *
 * The Alpha channel (c == 3) is left completely untouched.
 *
 * @param input The input function providing linear ProPhoto RGBA data.
 * @param x Halide Var for width.
 * @param y Halide Var for height.
 * @param c Halide Var for channels.
 * @return Halide::Func A new function containing L, a, b, Alpha.
 */
inline Halide::Func proPhotoLinearToOklab(const Halide::Func& input,
                                          const Halide::Var& x,
                                          const Halide::Var& y,
                                          const Halide::Var& c)
{
    return linearRgbToOklab(input, x, y, c,
        // M1 composed with the Bradford D50->D65 adaptation:
        // linear ProPhoto (D50) -> LMS, white point normalized.
        {0.7153289120f, 0.3529100770f, -0.0682558242f,
         0.2743542906f, 0.6678509662f, +0.0577945291f,
         0.1097541052f, 0.1862089904f, +0.7040358683f},
        "prophoto_linear_to_oklab");
}

/**
 * @brief Converts a Chunky Oklab Halide::Func back to linear ProPhoto (ROMM RGB, D50).
 *
 * The Alpha channel (c == 3) is left completely untouched.
 *
 * @param input The input function providing Lab(A) data.
 * @param x Halide Var for width.
 * @param y Halide Var for height.
 * @param c Halide Var for channels.
 * @return Halide::Func A new function containing R, G, B, Alpha in linear ProPhoto.
 */
inline Halide::Func oklabToProPhotoLinear(const Halide::Func& input,
                                          const Halide::Var& x,
                                          const Halide::Var& y,
                                          const Halide::Var& c)
{
    return oklabToLinearRgb(input, x, y, c,
        // Inverse of the composed ProPhoto M1: LMS -> linear ProPhoto (D50).
        {+1.7387392019f, -0.9884191746f, +0.2497092918f,
         -0.7070022878f, +1.9343231884f, -0.2273326246f,
         -0.0840633067f, -0.3575177308f, +1.4415810392f},
        "oklab_to_prophoto_linear");
}

} // namespace ColorSpace
} // namespace CaptureMoment::Core::Pipeline
