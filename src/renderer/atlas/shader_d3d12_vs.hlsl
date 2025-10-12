// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// D3D12 Vertex Shader for Atlas Renderer
// Matches BackendD3D12 root signature with explicit register bindings

#define ATLAS_VERTEX_SEMANTIC POSITION
#define ATLAS_SHADINGTYPE_SEMANTIC SHADINGTYPE
#define ATLAS_RENDITIONSCALE_SEMANTIC RENDITIONSCALE
#define ATLAS_INSTANCE_POSITION_SEMANTIC INSTANCE_POSITION
#define ATLAS_SIZE_SEMANTIC INSTANCE_SIZE
#define ATLAS_TEXCOORD_SEMANTIC INSTANCE_TEXCOORD
#define ATLAS_COLOR_SEMANTIC INSTANCE_COLOR

#include "shader_common.hlsl"

#undef ATLAS_VERTEX_SEMANTIC
#undef ATLAS_SHADINGTYPE_SEMANTIC
#undef ATLAS_RENDITIONSCALE_SEMANTIC
#undef ATLAS_INSTANCE_POSITION_SEMANTIC
#undef ATLAS_SIZE_SEMANTIC
#undef ATLAS_TEXCOORD_SEMANTIC
#undef ATLAS_COLOR_SEMANTIC

cbuffer VSConstBuffer : register(b0)
{
    float2 positionScale;
}

// clang-format off
PSData main(VSData data)
// clang-format on
{
    PSData output;
    output.color = data.color;
    output.shadingType = data.shadingType;
    output.renditionScale = data.renditionScale;

    // Transform from pixel space to NDC space
    // positionScale is (2.0f / width, -2.0f / height)
    output.position.xy = (data.position + data.vertex.xy * data.size) * positionScale + float2(-1.0f, 1.0f);
    output.position.zw = float2(0, 1);
    output.texcoord = data.texcoord + data.vertex.xy * data.size;

    return output;
}
