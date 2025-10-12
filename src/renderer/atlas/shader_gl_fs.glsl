// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// OpenGL Fragment Shader
// Target: GLSL 3.30 Core Profile
//
// This fragment shader implements all shading types for terminal rendering:
// - Text rendering (grayscale, ClearType, builtin glyphs)
// - Background rendering
// - Line rendering (solid, dotted, dashed, curly)
// - Cursor and selection rendering

#version 330 core

// Include common definitions and helper functions
// Note: In practice, this would be concatenated during shader compilation
// #include "shader_gl_common.glsl"

// For standalone compilation, we need to redefine constants
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

// ============================================================================
// Inputs from Vertex Shader
// ============================================================================

in vec2 v_texcoord;
flat in uint v_shadingType;
flat in vec2 v_renditionScale;
flat in vec4 v_color;

// ============================================================================
// Uniform Buffer (Constant Buffer)
// ============================================================================

layout(std140) uniform PSConstants
{
    vec4 backgroundColor;        // Default background color
    vec2 backgroundCellSize;     // Size of each cell in the background texture (pixels)
    vec2 backgroundCellCount;    // Number of cells in the background texture
    vec4 gammaRatios;           // Gamma correction ratios [dark, light, ?, ?]
    float enhancedContrast;     // Text contrast enhancement factor
    float underlineWidth;       // Width of underline in pixels
    float doubleUnderlineWidth; // Width of double underline in pixels
    float curlyLineHalfHeight;  // Half-height of curly underline wave
    float shadedGlyphDotSize;   // Dot size for shaded/patterned glyphs
} ps_cb;

// ============================================================================
// Texture Samplers
// ============================================================================

uniform sampler2D background;   // Background color texture
uniform sampler2D glyphAtlas;   // Glyph atlas texture

// ============================================================================
// Output
// ============================================================================

out vec4 fragColor;

// ============================================================================
// Helper Functions (from shader_gl_common.glsl)
// ============================================================================

vec4 premultiplyColor(vec4 color)
{
    return vec4(color.rgb * color.a, color.a);
}

vec4 alphaBlendPremultiplied(vec4 bottom, vec4 top)
{
    return bottom * (1.0 - top.a) + top;
}

float DWrite_CalcColorIntensity(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float DWrite_ApplyLightOnDarkContrastAdjustment(float enhancedContrast, vec3 color)
{
    float intensity = DWrite_CalcColorIntensity(color);
    return enhancedContrast * (1.0 - intensity);
}

float DWrite_EnhanceContrast(float alpha, float enhancedContrast)
{
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
    float gamma = mix(gammaRatios.y, gammaRatios.x, intensity);
    return pow(alpha, gamma);
}

vec3 DWrite_ApplyAlphaCorrection3(vec3 alpha, vec3 color, vec4 gammaRatios)
{
    float intensity = DWrite_CalcColorIntensity(color);
    float gamma = mix(gammaRatios.y, gammaRatios.x, intensity);
    return pow(alpha, vec3(gamma));
}

// ============================================================================
// Main Fragment Shader
// ============================================================================

void main()
{
    vec4 outputColor;

    // Switch on shading type to determine rendering method
    // Using if-else chain instead of switch for better compatibility
    if (v_shadingType == SHADING_TYPE_TEXT_BACKGROUND)
    {
        // Render background from texture or use solid color
        vec2 cell = gl_FragCoord.xy / ps_cb.backgroundCellSize;

        if (all(lessThan(cell, ps_cb.backgroundCellCount)))
        {
            // Sample from background texture
            vec2 uv = cell / ps_cb.backgroundCellCount;
            outputColor = texture(background, uv);
        }
        else
        {
            // Outside valid region, use solid background color
            outputColor = ps_cb.backgroundColor;
        }
    }
    else if (v_shadingType == SHADING_TYPE_TEXT_GRAYSCALE)
    {
        // Grayscale text rendering (LCD-off or monochrome displays)
        // Sample glyph alpha from red channel (R8 texture format)
        float glyphAlpha = texture(glyphAtlas, v_texcoord).r;

        // Apply DirectWrite contrast enhancement
        float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(
            ps_cb.enhancedContrast, v_color.rgb);
        float contrasted = DWrite_EnhanceContrast(glyphAlpha, blendEnhancedContrast);

        // Apply gamma correction based on color intensity
        float intensity = DWrite_CalcColorIntensity(v_color.rgb);
        float alphaCorrected = DWrite_ApplyAlphaCorrection(contrasted, intensity, ps_cb.gammaRatios);

        // Blend with foreground color
        vec4 foreground = premultiplyColor(v_color);
        outputColor = foreground * alphaCorrected;
    }
    else if (v_shadingType == SHADING_TYPE_TEXT_CLEARTYPE)
    {
        // ClearType subpixel text rendering
        // Sample RGB channels for subpixel antialiasing
        vec3 glyphRGB = texture(glyphAtlas, v_texcoord).rgb;

        // Apply contrast enhancement per channel
        float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(
            ps_cb.enhancedContrast, v_color.rgb);
        vec3 contrasted = DWrite_EnhanceContrast3(glyphRGB, blendEnhancedContrast);

        // Apply gamma correction per channel
        vec3 alphaCorrected = DWrite_ApplyAlphaCorrection3(contrasted, v_color.rgb, ps_cb.gammaRatios);

        // Compute weighted alpha (average of RGB for proper blending)
        vec4 weights = vec4(alphaCorrected * v_color.a, 1.0);
        outputColor = weights * v_color;
    }
    else if (v_shadingType == SHADING_TYPE_TEXT_BUILTIN_GLYPH)
    {
        // Built-in glyphs with procedural patterns (shaded characters)
        vec4 glyph = texture(glyphAtlas, v_texcoord);
        vec2 pos = floor(gl_FragCoord.xy / (ps_cb.shadedGlyphDotSize * v_renditionScale));

        // Generate checkerboard/pattern based on glyph RGB controls
        // R channel: stretch (0=normal, 1=wide spacing)
        // G channel: invert (0=normal, 1=inverted pattern)
        // B channel: fill (0=pattern, 1=solid)
        // A channel: base alpha mask

        float xFactor = glyph.r * -0.25 + 0.5;
        float yFactor = 0.5;
        float pattern = step(fract(dot(pos, vec2(xFactor, yFactor))), 0.0);
        float stretched = pattern * glyph.a;
        float inverted = abs(glyph.g - stretched);
        float filled = max(glyph.b, inverted);

        outputColor = premultiplyColor(v_color) * filled;
    }
    else if (v_shadingType == SHADING_TYPE_TEXT_PASSTHROUGH)
    {
        // Passthrough rendering (colored emoji, images)
        vec4 texel = texture(glyphAtlas, v_texcoord);
        outputColor = texel * v_color;
    }
    else if (v_shadingType == SHADING_TYPE_DOTTED_LINE)
    {
        // Dotted line pattern
        float pattern = step(0.5, fract(gl_FragCoord.x * 0.25));
        outputColor = premultiplyColor(v_color) * pattern;
    }
    else if (v_shadingType == SHADING_TYPE_DASHED_LINE)
    {
        // Dashed line pattern (longer dashes than dotted)
        float pattern = step(0.5, fract(gl_FragCoord.x * 0.125));
        outputColor = premultiplyColor(v_color) * pattern;
    }
    else if (v_shadingType == SHADING_TYPE_CURLY_LINE)
    {
        // Curly/wavy underline
        float offset = v_texcoord.y - 0.5;
        float curve = sin(v_texcoord.x * 3.14159265 * 4.0) * ps_cb.curlyLineHalfHeight;
        float dist = abs(offset - curve);
        float alpha = 1.0 - smoothstep(0.0, ps_cb.underlineWidth, dist);
        outputColor = premultiplyColor(v_color) * alpha;
    }
    else if (v_shadingType == SHADING_TYPE_SOLID_LINE ||
             v_shadingType == SHADING_TYPE_CURSOR ||
             v_shadingType == SHADING_TYPE_FILLED_RECT)
    {
        // Solid color rendering (lines, cursor, selection)
        outputColor = premultiplyColor(v_color);
    }
    else
    {
        // Unknown shading type - render as magenta for debugging
        outputColor = vec4(1.0, 0.0, 1.0, 1.0);
    }

    fragColor = outputColor;
}
