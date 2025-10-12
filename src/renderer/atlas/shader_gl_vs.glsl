// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// OpenGL Vertex Shader
// Target: GLSL 3.30 Core Profile
//
// This vertex shader implements the same transformation pipeline as the D3D11/D3D12
// backends, transforming quad instances from pixel space to normalized device coordinates.

#version 330 core

// ============================================================================
// Vertex Input Layout
// ============================================================================

// Per-vertex attributes (static quad vertices)
layout(location = 0) in vec2 in_vertex;

// Per-instance attributes (dynamic QuadInstance data)
layout(location = 1) in uint in_shadingType;
layout(location = 2) in uvec2 in_renditionScale;
layout(location = 3) in ivec2 in_position;
layout(location = 4) in uvec2 in_size;
layout(location = 5) in uvec2 in_texcoord;
layout(location = 6) in vec4 in_color;

// ============================================================================
// Uniform Buffer (Constant Buffer)
// ============================================================================

layout(std140) uniform VSConstants
{
    vec2 positionScale;  // Scale factor for pixel to NDC conversion
} vs_cb;

// ============================================================================
// Outputs to Fragment Shader
// ============================================================================

out vec2 v_texcoord;
flat out uint v_shadingType;
flat out vec2 v_renditionScale;
flat out vec4 v_color;

// ============================================================================
// Main Vertex Shader
// ============================================================================

void main()
{
    // Transform vertex position from pixel space to normalized device coordinates (NDC)
    //
    // positionScale is expected to be:
    //   vec2(2.0 / viewportWidth, -2.0 / viewportHeight)
    //
    // This transformation:
    // 1. Scales pixel coordinates to [0, 2] range
    // 2. Flips Y axis (OpenGL Y+ is up, screen Y+ is down)
    // 3. Translates to [-1, 1] NDC range
    //
    // For a quad at position (px, py) with size (w, h):
    //   - in_vertex is [0,0], [1,0], [1,1], or [0,1] (unit quad corners)
    //   - in_position is the top-left corner in pixels
    //   - in_size is the width and height in pixels
    //
    // The calculation expands each instance into a quad:
    //   pixel_pos = in_position + in_vertex * in_size
    //   ndc_pos = pixel_pos * positionScale + vec2(-1.0, 1.0)

    vec2 pixel_pos = vec2(in_position) + in_vertex * vec2(in_size);
    vec2 ndc_pos = pixel_pos * vs_cb.positionScale + vec2(-1.0, 1.0);

    gl_Position = vec4(ndc_pos, 0.0, 1.0);

    // Calculate texture coordinates
    // Similar to position, we expand from the texcoord base
    v_texcoord = vec2(in_texcoord) + in_vertex * vec2(in_size);

    // Pass through instance attributes (flat shading, no interpolation)
    v_shadingType = in_shadingType;
    v_renditionScale = vec2(in_renditionScale);
    v_color = in_color;
}
