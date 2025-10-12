// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// OpenGL GLSL Common Definitions
// Target: GLSL 3.30 Core Profile
//
// This file contains common definitions shared between vertex and fragment shaders
// for the OpenGL backend of the Windows Terminal Atlas renderer.

// Shading type constants
#define SHADING_TYPE_TEXT_BACKGROUND    0
#define SHADING_TYPE_TEXT_GRAYSCALE     1
#define SHADING_TYPE_TEXT_CLEARTYPE     2
#define SHADING_TYPE_TEXT_BUILTIN_GLYPH 3
#define SHADING_TYPE_TEXT_PASSTHROUGH   4
#define SHADING_TYPE_DOTTED_LINE        5
#define SHADING_TYPE_DASHED_LINE        6
#define SHADING_TYPE_CURLY_LINE         7
#define SHADING_TYPE_SOLID_LINE         8
#define SHADING_TYPE_CURSOR             9
#define SHADING_TYPE_FILLED_RECT       10

// Helper functions for color manipulation

vec4 premultiplyColor(vec4 color)
{
    return vec4(color.rgb * color.a, color.a);
}

vec4 alphaBlendPremultiplied(vec4 bottom, vec4 top)
{
    return bottom * (1.0 - top.a) + top;
}

vec4 decodeRGBA(uint i)
{
    vec4 result;
    result.r = float((i >> 0u) & 0xFFu) / 255.0;
    result.g = float((i >> 8u) & 0xFFu) / 255.0;
    result.b = float((i >> 16u) & 0xFFu) / 255.0;
    result.a = float((i >> 24u) & 0xFFu) / 255.0;
    return result;
}

// DirectWrite-compatible text rendering functions
// These replicate the DirectWrite gamma correction and contrast enhancement
// algorithms for consistent text rendering across D3D and OpenGL backends.

float DWrite_CalcColorIntensity(vec3 color)
{
    // Luminance calculation using standard weights
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float DWrite_ApplyLightOnDarkContrastAdjustment(float enhancedContrast, vec3 color)
{
    // Apply contrast adjustment based on whether we're rendering light text on dark background
    float intensity = DWrite_CalcColorIntensity(color);
    return enhancedContrast * (1.0 - intensity);
}

float DWrite_EnhanceContrast(float alpha, float enhancedContrast)
{
    // Enhanced contrast using a non-linear curve
    // This makes thin text more readable by enhancing edge contrast
    if (enhancedContrast <= 0.0) {
        return alpha;
    }

    float ec_half = enhancedContrast * 0.5;
    float ec_half_sqr = ec_half * ec_half;
    float alpha_half = alpha - 0.5;

    if (alpha_half > 0.0) {
        return alpha_half / sqrt(ec_half_sqr + alpha_half * alpha_half) * ec_half + 0.5;
    } else {
        return -(-alpha_half / sqrt(ec_half_sqr + alpha_half * alpha_half) * ec_half) + 0.5;
    }
}

vec3 DWrite_EnhanceContrast3(vec3 alpha, float enhancedContrast)
{
    return vec3(
        DWrite_EnhanceContrast(alpha.r, enhancedContrast),
        DWrite_EnhanceContrast(alpha.g, enhancedContrast),
        DWrite_EnhanceContrast(alpha.b, enhancedContrast)
    );
}

float DWrite_ApplyAlphaCorrection(float alpha, float intensity, vec4 gammaRatios)
{
    // Apply gamma correction based on color intensity
    // This ensures consistent text weight across different background colors
    float gamma = mix(gammaRatios.y, gammaRatios.x, intensity);
    return pow(alpha, gamma);
}

vec3 DWrite_ApplyAlphaCorrection3(vec3 alpha, vec3 color, vec4 gammaRatios)
{
    // Apply per-channel gamma correction for ClearType rendering
    float intensity = DWrite_CalcColorIntensity(color);
    float gamma = mix(gammaRatios.y, gammaRatios.x, intensity);
    return pow(alpha, vec3(gamma));
}

// ClearType-specific blending
// ClearType uses subpixel rendering where each RGB channel represents a different
// subpixel position. This requires special blending to avoid color fringing.
vec4 DWrite_ClearTypeBlend(vec3 glyphRGB, vec3 foregroundColor, float foregroundAlpha)
{
    // Weight the glyph samples by the foreground color
    vec3 weighted = glyphRGB * foregroundColor * foregroundAlpha;

    // The final alpha is the average of the RGB channels
    // This prevents color fringing at subpixel boundaries
    float alpha = (weighted.r + weighted.g + weighted.b) * 0.333333;

    return vec4(weighted, alpha);
}

// Helper for builtin glyph pattern generation
float BuiltinGlyph_CheckerboardPattern(vec2 pos, float stretch, float invert, float fill, float alpha)
{
    // Generate a checkerboard pattern for shaded glyphs
    // The 'stretch' parameter controls horizontal stretching (0=normal, 1=wide)
    // The 'invert' parameter inverts the pattern (0=normal, 1=inverted)
    // The 'fill' parameter controls solid vs patterned (0=pattern, 1=solid)

    // Calculate the base pattern
    float xFactor = stretch * -0.25 + 0.5;
    float yFactor = 0.5;
    float pattern = step(fract(dot(pos, vec2(xFactor, yFactor))), 0.0);

    // Apply alpha from texture
    float stretched = pattern * alpha;

    // Apply invert
    float inverted = abs(invert - stretched);

    // Apply fill (solid vs pattern)
    return max(fill, inverted);
}
