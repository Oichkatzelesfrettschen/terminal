#ifndef ATLAS_SHADER_TYPES_INCLUDED
#define ATLAS_SHADER_TYPES_INCLUDED

#include "shaders/hlsl/common/defines.hlsl"

#ifndef ATLAS_VERTEX_SEMANTIC
#define ATLAS_VERTEX_SEMANTIC SV_Position
#endif

#ifndef ATLAS_SHADINGTYPE_SEMANTIC
#define ATLAS_SHADINGTYPE_SEMANTIC shadingType
#endif

#ifndef ATLAS_RENDITIONSCALE_SEMANTIC
#define ATLAS_RENDITIONSCALE_SEMANTIC renditionScale
#endif

#ifndef ATLAS_INSTANCE_POSITION_SEMANTIC
#define ATLAS_INSTANCE_POSITION_SEMANTIC position
#endif

#ifndef ATLAS_SIZE_SEMANTIC
#define ATLAS_SIZE_SEMANTIC size
#endif

#ifndef ATLAS_TEXCOORD_SEMANTIC
#define ATLAS_TEXCOORD_SEMANTIC texcoord
#endif

#ifndef ATLAS_COLOR_SEMANTIC
#define ATLAS_COLOR_SEMANTIC color
#endif

struct VSData
{
    float2 vertex : ATLAS_VERTEX_SEMANTIC;
    uint shadingType : ATLAS_SHADINGTYPE_SEMANTIC;
    uint2 renditionScale : ATLAS_RENDITIONSCALE_SEMANTIC;
    int2 position : ATLAS_INSTANCE_POSITION_SEMANTIC;
    uint2 size : ATLAS_SIZE_SEMANTIC;
    uint2 texcoord : ATLAS_TEXCOORD_SEMANTIC;
    float4 color : ATLAS_COLOR_SEMANTIC;
};

struct PSData
{
    float4 position : SV_Position;
    float2 texcoord : ATLAS_TEXCOORD_SEMANTIC;
    nointerpolation uint shadingType : ATLAS_SHADINGTYPE_SEMANTIC;
    nointerpolation float2 renditionScale : ATLAS_RENDITIONSCALE_SEMANTIC;
    nointerpolation float4 color : ATLAS_COLOR_SEMANTIC;
};

#endif // ATLAS_SHADER_TYPES_INCLUDED
