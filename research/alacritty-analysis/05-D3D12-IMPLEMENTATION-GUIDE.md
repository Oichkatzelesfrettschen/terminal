# Implementing Alacritty's Techniques in Direct3D 12

## Overview

This guide provides a complete implementation roadmap for bringing Alacritty's rendering optimizations to Windows Terminal using Direct3D 12.

## Architecture Comparison

### Alacritty (OpenGL 3.3)

```
Glyph Cache (HashMap)
    |
    v
Texture Atlas (1024x1024 RGBA)
    |
    v
Instance Buffer (65,536 instances)
    |
    v
Vertex Shader (gl_VertexID + instancing)
    |
    v
Fragment Shader (dual-source blending)
    |
    v
2 Draw Calls (background + text)
```

### Windows Terminal (D3D12 Proposed)

```
Glyph Cache (unordered_map)
    |
    v
Texture Atlas (1024x1024 R8G8B8A8_UNORM)
    |
    v
Structured Buffer (65,536 instances)
    |
    v
Vertex Shader (SV_VertexID + SV_InstanceID)
    |
    v
Pixel Shader (dual-source blending)
    |
    v
2 DrawIndexedInstanced calls
```

## Phase 1: Glyph Cache Implementation

### Data Structures

```cpp
// GlyphCacheKey.h
struct GlyphCacheKey {
    uint64_t fontFace;      // Font face handle
    uint32_t codepoint;     // Unicode codepoint
    uint32_t sizeInPixels;  // Font size
    uint8_t style;          // Bold, italic, etc.

    bool operator==(const GlyphCacheKey& other) const {
        return fontFace == other.fontFace &&
               codepoint == other.codepoint &&
               sizeInPixels == other.sizeInPixels &&
               style == other.style;
    }

    struct Hash {
        size_t operator()(const GlyphCacheKey& k) const {
            // Use xxHash for fast hashing
            return XXH64(&k, sizeof(k), 0);
        }
    };
};

// GlyphCacheValue.h
struct GlyphCacheValue {
    uint32_t atlasIndex;        // Which atlas texture
    uint16_t x, y;              // Pixel coordinates in atlas
    uint16_t width, height;     // Glyph dimensions in pixels
    int16_t bearingX, bearingY; // Glyph bearing (positioning offset)
    float uvLeft, uvTop;        // Normalized UV coordinates [0,1]
    float uvWidth, uvHeight;    // Normalized UV size [0,1]
    bool isColored;             // RGBA vs grayscale
};

// GlyphCache.h
class GlyphCache {
private:
    std::unordered_map<GlyphCacheKey, GlyphCacheValue, GlyphCacheKey::Hash> cache;
    IFontRasterizer* rasterizer;

public:
    GlyphCacheValue GetOrLoad(const GlyphCacheKey& key);
    void PreCacheCommonGlyphs();
    void Clear();
};
```

### Pre-Caching Implementation

```cpp
void GlyphCache::PreCacheCommonGlyphs() {
    // Cache ASCII printable characters for all font styles
    const uint8_t styles[] = {
        FontStyle::Regular,
        FontStyle::Bold,
        FontStyle::Italic,
        FontStyle::BoldItalic
    };

    for (auto style : styles) {
        for (uint32_t c = 32; c <= 126; ++c) {
            GlyphCacheKey key{
                currentFontFace,
                c,
                currentFontSize,
                style
            };
            GetOrLoad(key);
        }
    }
}

GlyphCacheValue GlyphCache::GetOrLoad(const GlyphCacheKey& key) {
    // Check cache first
    auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }

    // Rasterize glyph
    auto rasterized = rasterizer->RasterizeGlyph(key);

    if (!rasterized.has_value()) {
        // Handle missing glyph
        GlyphCacheKey missingKey = key;
        missingKey.codepoint = 0;  // U+0000 = missing glyph

        auto missingIt = cache.find(missingKey);
        if (missingIt != cache.end()) {
            return missingIt->second;
        }

        // Create missing glyph indicator
        rasterized = CreateMissingGlyphBitmap();
        auto value = UploadToAtlas(rasterized.value());
        cache[missingKey] = value;
        return value;
    }

    // Upload to atlas and cache
    auto value = UploadToAtlas(rasterized.value());
    cache[key] = value;
    return value;
}
```

## Phase 2: Texture Atlas System

### Atlas Structure

```cpp
// TextureAtlas.h
class TextureAtlas {
public:
    static constexpr uint32_t ATLAS_SIZE = 1024;

private:
    ComPtr<ID3D12Resource> texture;         // Committed resource
    ComPtr<ID3D12Resource> uploadBuffer;    // Staging buffer
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle;  // SRV descriptor

    uint32_t rowExtent = 0;      // Current X position
    uint32_t rowBaseline = 0;    // Current Y position
    uint32_t rowTallest = 0;     // Tallest glyph in current row

public:
    bool Insert(const RasterizedGlyph& glyph, GlyphCacheValue& outValue);
    void Clear();
    ID3D12Resource* GetTexture() const { return texture.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return srvHandle; }
};

// AtlasManager.h
class AtlasManager {
private:
    std::vector<std::unique_ptr<TextureAtlas>> atlases;
    size_t currentAtlas = 0;

public:
    GlyphCacheValue LoadGlyph(const RasterizedGlyph& glyph);
    void ClearAll();
};
```

### Atlas Creation

```cpp
TextureAtlas::TextureAtlas(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap, uint32_t srvIndex) {
    // Create texture resource
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = ATLAS_SIZE;
    textureDesc.Height = ATLAS_SIZE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture)
    );

    // Create upload buffer (for staging)
    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = ATLAS_SIZE * ATLAS_SIZE * 4;  // RGBA
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)
    );

    // Create SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += srvIndex * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    device->CreateShaderResourceView(texture.Get(), &srvDesc, cpuHandle);

    srvHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
    srvHandle.ptr += srvIndex * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}
```

### Row-Packing Insertion

```cpp
bool TextureAtlas::Insert(const RasterizedGlyph& glyph, GlyphCacheValue& outValue) {
    // Check if glyph fits at all
    if (glyph.width > ATLAS_SIZE || glyph.height > ATLAS_SIZE) {
        return false;  // Glyph too large
    }

    // Check if there's room in current row
    if (!HasRoomInRow(glyph)) {
        AdvanceRow();
    }

    // Still no room? Atlas is full
    if (!HasRoomInRow(glyph)) {
        return false;
    }

    // Insert the glyph
    InsertInternal(glyph, outValue);
    return true;
}

bool TextureAtlas::HasRoomInRow(const RasterizedGlyph& glyph) const {
    uint32_t nextExtent = rowExtent + glyph.width;
    bool enoughWidth = nextExtent <= ATLAS_SIZE;
    bool enoughHeight = glyph.height < (ATLAS_SIZE - rowBaseline);
    return enoughWidth && enoughHeight;
}

void TextureAtlas::AdvanceRow() {
    uint32_t advanceTo = rowBaseline + rowTallest;
    if (ATLAS_SIZE - advanceTo <= 0) {
        return;  // Atlas full
    }

    rowBaseline = advanceTo;
    rowExtent = 0;
    rowTallest = 0;
}

void TextureAtlas::InsertInternal(const RasterizedGlyph& glyph, GlyphCacheValue& outValue) {
    uint32_t offsetX = rowExtent;
    uint32_t offsetY = rowBaseline;

    // Prepare texture copy
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = texture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = uploadBuffer.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = 0;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    src.PlacedFootprint.Footprint.Width = glyph.width;
    src.PlacedFootprint.Footprint.Height = glyph.height;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = Align(glyph.width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    // Map upload buffer and copy glyph data
    void* mappedData;
    uploadBuffer->Map(0, nullptr, &mappedData);
    CopyGlyphData(mappedData, glyph, src.PlacedFootprint.Footprint.RowPitch);
    uploadBuffer->Unmap(0, nullptr);

    // Copy to atlas texture
    D3D12_BOX srcBox = {0, 0, 0, glyph.width, glyph.height, 1};
    commandList->CopyTextureRegion(&dst, offsetX, offsetY, 0, &src, &srcBox);

    // Update row state
    rowExtent = offsetX + glyph.width;
    if (glyph.height > rowTallest) {
        rowTallest = glyph.height;
    }

    // Fill output value
    outValue.atlasIndex = atlasIndex;
    outValue.x = offsetX;
    outValue.y = offsetY;
    outValue.width = glyph.width;
    outValue.height = glyph.height;
    outValue.bearingX = glyph.bearingX;
    outValue.bearingY = glyph.bearingY;
    outValue.uvLeft = static_cast<float>(offsetX) / ATLAS_SIZE;
    outValue.uvTop = static_cast<float>(offsetY) / ATLAS_SIZE;
    outValue.uvWidth = static_cast<float>(glyph.width) / ATLAS_SIZE;
    outValue.uvHeight = static_cast<float>(glyph.height) / ATLAS_SIZE;
    outValue.isColored = glyph.isColored;
}
```

## Phase 3: Batch Rendering System

### Instance Data

```cpp
// InstanceData.h
struct InstanceData {
    uint16_t col, row;           // Cell position
    int16_t bearingX, bearingY;  // Glyph bearing
    int16_t width, height;       // Glyph size
    float uvLeft, uvTop;         // UV coordinates
    float uvWidth, uvHeight;
    uint8_t fgR, fgG, fgB;       // Foreground color
    uint8_t flags;               // Colored | WideChar
    uint8_t bgR, bgG, bgB, bgA;  // Background color
};  // 48 bytes

static_assert(sizeof(InstanceData) == 48, "Instance data must be 48 bytes");
```

### Batch Class

```cpp
class GlyphBatch {
private:
    static constexpr size_t MAX_BATCH_SIZE = 65536;

    std::vector<InstanceData> instances;
    ComPtr<ID3D12Resource> instanceBuffer;
    void* mappedInstanceData = nullptr;

    ID3D12Resource* currentAtlasTexture = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE currentAtlasSRV;

public:
    void Initialize(ID3D12Device* device);
    void AddGlyph(const Cell& cell, const GlyphCacheValue& glyph, ID3D12Resource* atlasTexture, D3D12_GPU_DESCRIPTOR_HANDLE atlasSRV);
    void Flush(ID3D12GraphicsCommandList* cmdList);
    bool IsFull() const { return instances.size() >= MAX_BATCH_SIZE; }
    bool IsEmpty() const { return instances.empty(); }
};

void GlyphBatch::Initialize(ID3D12Device* device) {
    // Create instance buffer (upload heap for CPU updates)
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = MAX_BATCH_SIZE * sizeof(InstanceData);
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&instanceBuffer)
    );

    // Persistent map
    instanceBuffer->Map(0, nullptr, &mappedInstanceData);

    instances.reserve(MAX_BATCH_SIZE);
}

void GlyphBatch::AddGlyph(
    const Cell& cell,
    const GlyphCacheValue& glyph,
    ID3D12Resource* atlasTexture,
    D3D12_GPU_DESCRIPTOR_HANDLE atlasSRV)
{
    // Flush if atlas changes
    if (!IsEmpty() && currentAtlasTexture != atlasTexture) {
        Flush(commandList);
    }

    currentAtlasTexture = atlasTexture;
    currentAtlasSRV = atlasSRV;

    // Create instance data
    InstanceData inst = {};
    inst.col = static_cast<uint16_t>(cell.column);
    inst.row = static_cast<uint16_t>(cell.row);
    inst.bearingX = glyph.bearingX;
    inst.bearingY = glyph.bearingY;
    inst.width = glyph.width;
    inst.height = glyph.height;
    inst.uvLeft = glyph.uvLeft;
    inst.uvTop = glyph.uvTop;
    inst.uvWidth = glyph.uvWidth;
    inst.uvHeight = glyph.uvHeight;
    inst.fgR = cell.fg.r;
    inst.fgG = cell.fg.g;
    inst.fgB = cell.fg.b;
    inst.flags = (glyph.isColored ? 0x01 : 0x00) | (cell.isWide ? 0x02 : 0x00);
    inst.bgR = cell.bg.r;
    inst.bgG = cell.bg.g;
    inst.bgB = cell.bg.b;
    inst.bgA = cell.bg.a;

    instances.push_back(inst);

    // Flush if batch full
    if (IsFull()) {
        Flush(commandList);
    }
}

void GlyphBatch::Flush(ID3D12GraphicsCommandList* cmdList) {
    if (IsEmpty()) return;

    // Copy instance data to GPU buffer
    memcpy(mappedInstanceData, instances.data(), instances.size() * sizeof(InstanceData));

    // Set atlas SRV
    cmdList->SetGraphicsRootDescriptorTable(1, currentAtlasSRV);

    // Set instance buffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = static_cast<UINT>(instances.size());
    srvDesc.Buffer.StructureByteStride = sizeof(InstanceData);

    cmdList->SetGraphicsRootShaderResourceView(2, instanceBuffer->GetGPUVirtualAddress());

    // Draw background pass
    cmdList->SetPipelineState(backgroundPSO.Get());
    cmdList->SetGraphicsRoot32BitConstant(0, 0, 0);  // renderingPass = 0
    cmdList->DrawIndexedInstanced(6, static_cast<UINT>(instances.size()), 0, 0, 0);

    // Draw text pass
    cmdList->SetPipelineState(textPSO.Get());
    cmdList->SetGraphicsRoot32BitConstant(0, 1, 0);  // renderingPass = 1
    cmdList->DrawIndexedInstanced(6, static_cast<UINT>(instances.size()), 0, 0, 0);

    instances.clear();
}
```

## Phase 4: Shaders

### Root Signature

```cpp
// Root parameters:
// 0: 32-bit constants (cellWidth, cellHeight, projectionX, projectionY, projectionScaleX, projectionScaleY, renderingPass)
// 1: Descriptor table (atlas texture SRV)
// 2: SRV (instance buffer)

D3D12_ROOT_PARAMETER rootParams[3] = {};

// Root constants
rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
rootParams[0].Constants.ShaderRegister = 0;
rootParams[0].Constants.RegisterSpace = 0;
rootParams[0].Constants.Num32BitValues = 7;
rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

// Atlas texture
D3D12_DESCRIPTOR_RANGE descRange = {};
descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
descRange.NumDescriptors = 1;
descRange.BaseShaderRegister = 0;
descRange.RegisterSpace = 0;
descRange.OffsetInDescriptorsFromTableStart = 0;

rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
rootParams[1].DescriptorTable.pDescriptorRanges = &descRange;
rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

// Instance buffer
rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
rootParams[2].Descriptor.ShaderRegister = 1;
rootParams[2].Descriptor.RegisterSpace = 0;
rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

// Static sampler
D3D12_STATIC_SAMPLER_DESC sampler = {};
sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
sampler.MipLODBias = 0.0f;
sampler.MaxAnisotropy = 1;
sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
sampler.MinLOD = 0.0f;
sampler.MaxLOD = D3D12_FLOAT32_MAX;
sampler.ShaderRegister = 0;
sampler.RegisterSpace = 0;
sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
rootSigDesc.NumParameters = 3;
rootSigDesc.pParameters = rootParams;
rootSigDesc.NumStaticSamplers = 1;
rootSigDesc.pStaticSamplers = &sampler;
rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
```

### Vertex Shader (HLSL)

```hlsl
// TextRendering.hlsl

struct InstanceData {
    uint2 cellPos;              // col, row
    int2 bearing;               // bearingX, bearingY
    uint2 size;                 // width, height
    float2 uvPos;               // uvLeft, uvTop
    float2 uvSize;              // uvWidth, uvHeight
    uint4 colors;               // fgR, fgG, fgB, flags, bgR, bgG, bgB, bgA (packed)
};

cbuffer Constants : register(b0) {
    float cellWidth;
    float cellHeight;
    float2 projectionOffset;
    float2 projectionScale;
    uint renderingPass;
};

StructuredBuffer<InstanceData> instances : register(t1);

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 fgColor : COLOR0;
    float4 bgColor : COLOR1;
    float flags : TEXCOORD1;
};

VSOutput VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID) {
    InstanceData inst = instances[instanceID];

    // Decode colors
    float4 fg = float4(
        inst.colors.x / 255.0,
        inst.colors.y / 255.0,
        inst.colors.z / 255.0,
        inst.colors.w & 0x01  // Colored flag
    );

    float4 bg = float4(
        ((inst.colors.w >> 8) & 0xFF) / 255.0,
        ((inst.colors.w >> 16) & 0xFF) / 255.0,
        ((inst.colors.w >> 24) & 0xFF) / 255.0,
        inst.colors.w & 0x80  // Alpha
    ) / 255.0;

    uint flags = inst.colors.w & 0xFF;

    // Compute quad corner
    float2 corner;
    corner.x = (vertexID == 0 || vertexID == 1) ? 1.0 : 0.0;
    corner.y = (vertexID == 0 || vertexID == 3) ? 0.0 : 1.0;

    float2 cellPos = float2(inst.cellPos) * float2(cellWidth, cellHeight);

    VSOutput output;

    if (renderingPass == 0) {
        // Background pass
        float occupiedCells = (flags & 0x02) ? 2.0 : 1.0;
        float2 bgSize = float2(cellWidth * occupiedCells, cellHeight);
        float2 pos = cellPos + bgSize * corner;
        output.position = float4(projectionOffset + projectionScale * pos, 0.0, 1.0);
        output.texCoord = float2(0, 0);
    } else {
        // Text pass
        float2 glyphSize = float2(inst.size);
        float2 glyphOffset = float2(inst.bearing.x, cellHeight - inst.bearing.y);
        float2 pos = cellPos + glyphSize * corner + glyphOffset;
        output.position = float4(projectionOffset + projectionScale * pos, 0.0, 1.0);
        output.texCoord = inst.uvPos + corner * inst.uvSize;
    }

    output.fgColor = fg;
    output.bgColor = bg;
    output.flags = flags;

    return output;
}
```

### Pixel Shader (HLSL)

```hlsl
Texture2D glyphAtlas : register(t0);
SamplerState linearSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 fgColor : COLOR0;
    float4 bgColor : COLOR1;
    float flags : TEXCOORD1;
};

struct PSOutput {
    float4 color : SV_Target0;
};

PSOutput PSMain(PSInput input) {
    PSOutput output;

    if (renderingPass == 0) {
        // Background pass
        if (input.bgColor.a == 0.0) {
            discard;
        }
        output.color = float4(input.bgColor.rgb * input.bgColor.a, input.bgColor.a);
        return output;
    }

    // Text pass
    bool isColored = (input.flags & 0x01) != 0;

    if (isColored) {
        // Colored emoji
        float4 texColor = glyphAtlas.Sample(linearSampler, input.texCoord);
        // Revert premultiplication
        if (texColor.a > 0.0) {
            texColor.rgb /= texColor.a;
        }
        output.color = texColor;
    } else {
        // Subpixel text rendering
        float3 mask = glyphAtlas.Sample(linearSampler, input.texCoord).rgb;
        output.color = float4(input.fgColor.rgb * mask, max(max(mask.r, mask.g), mask.b));
    }

    return output;
}
```

### Blend State

```cpp
D3D12_BLEND_DESC textBlendDesc = {};
textBlendDesc.RenderTarget[0].BlendEnable = TRUE;
textBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
textBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
textBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
textBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
textBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
textBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
textBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
```

## Phase 5: Integration and Optimization

### Main Rendering Loop

```cpp
void TerminalRenderer::RenderFrame(const Terminal& terminal) {
    // Update projection
    UpdateProjection();

    // Clear
    ClearRenderTarget();

    // Start batch
    glyphBatch.Begin(commandList);

    // Iterate cells
    for (uint32_t row = 0; row < terminal.rows; ++row) {
        for (uint32_t col = 0; col < terminal.cols; ++col) {
            const Cell& cell = terminal.GetCell(row, col);

            // Get font style
            FontStyle style = GetFontStyle(cell.flags);

            // Get glyph from cache
            GlyphCacheKey key{fontFace, cell.codepoint, fontSize, style};
            GlyphCacheValue glyph = glyphCache.GetOrLoad(key);

            // Get atlas texture
            auto* atlas = atlasManager.GetAtlas(glyph.atlasIndex);

            // Add to batch
            glyphBatch.AddGlyph(cell, glyph, atlas->GetTexture(), atlas->GetSRV());
        }
    }

    // Flush remaining batch
    glyphBatch.Flush(commandList);

    // Present
    swapChain->Present(1, 0);
}
```

### Performance Monitoring

```cpp
class PerformanceStats {
public:
    void RecordFrame() {
        frameCount++;
        totalDrawCalls += batchFlushCount;
        totalInstancesRendered += instanceCount;
    }

    void PrintStats() {
        double avgDrawCalls = static_cast<double>(totalDrawCalls) / frameCount;
        double avgInstances = static_cast<double>(totalInstancesRendered) / frameCount;

        printf("Avg draw calls per frame: %.2f\n", avgDrawCalls);
        printf("Avg instances per frame: %.2f\n", avgInstances);
        printf("Cache hit rate: %.2f%%\n", cacheHitRate * 100.0);
    }
};
```

## Expected Performance Improvements

### Before (Current Windows Terminal)

- Draw calls per frame: ~1,000-10,000 (one per glyph)
- GPU state changes: High (frequent texture/buffer binding)
- CPU overhead: High (many API calls)
- Memory bandwidth: Moderate

### After (With Alacritty Techniques)

- Draw calls per frame: 2-6 (background + text, 1-3 atlases)
- GPU state changes: Minimal (batch by atlas)
- CPU overhead: Low (batch preparation only)
- Memory bandwidth: Lower (instanced rendering)

**Expected speedup: 5-10x for rendering**

## Conclusion

Implementing Alacritty's techniques in D3D12 requires:

1. Fast hash-based glyph cache
2. Dynamic texture atlas with row-packing
3. Structured buffer for instance data
4. Instanced rendering with SV_VertexID
5. Dual-pass rendering (background + text)

The API differences are minimal - most concepts translate directly from OpenGL to D3D12.

**Key insight:** The algorithms matter more than the API. Alacritty's performance comes from smart batching, not from OpenGL magic.
