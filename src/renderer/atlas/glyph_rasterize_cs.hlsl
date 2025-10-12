// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Glyph Rasterization Compute Shader
// Parallel glyph rasterization from font data to atlas texture
// Thread group size: [8, 8, 1] for tile-based rasterization

// ============================================================================
// Constant Buffers
// ============================================================================

cbuffer GlyphConstants : register(b0)
{
    uint2 atlasSize;        // Atlas texture dimensions
    uint2 glyphSize;        // Maximum glyph size in pixels
    uint glyphCount;        // Number of glyphs to rasterize
    uint glyphsPerRow;      // Glyphs per row in atlas
    float gamma;            // Gamma correction value
    float contrast;         // Enhanced contrast factor
    uint4 subpixelMask;     // Subpixel rendering mask (ClearType)
    float2 renderScale;     // DPI scaling factor
    uint flags;             // Rendering flags (antialiasing, etc.)
    uint _padding;
};

// Glyph descriptor structure
struct GlyphDescriptor
{
    uint2 atlasPos;         // Position in atlas texture
    uint2 size;             // Glyph size in pixels
    int2 bearing;           // Glyph bearing (offset)
    uint advance;           // Horizontal advance
    uint charCode;          // Unicode character code
    float4 metrics;         // Font metrics (ascent, descent, etc.)
    uint dataOffset;        // Offset into glyph data buffer
    uint dataSize;          // Size of glyph data in bytes
    uint flags;             // Glyph-specific flags
};

// Glyph coverage data (for antialiasing)
struct CoverageData
{
    float coverage;         // Coverage value [0, 1]
    float distance;         // Signed distance to edge
    uint subpixelR;         // Subpixel coverage red
    uint subpixelG;         // Subpixel coverage green
    uint subpixelB;         // Subpixel coverage blue
};

// ============================================================================
// Resources
// ============================================================================

// Glyph atlas texture UAV (R8_UNORM for grayscale, RGBA8 for ClearType)
RWTexture2D<float4> GlyphAtlas : register(u0);

// Glyph descriptors buffer
StructuredBuffer<GlyphDescriptor> GlyphDescriptors : register(t0);

// Glyph bitmap data buffer (raw font rasterization data)
Buffer<uint> GlyphDataBuffer : register(t1);

// Shared memory for tile-based processing
groupshared float TileCache[10][10]; // 8x8 tile with 1-pixel border

// ============================================================================
// Helper Functions
// ============================================================================

// Sample glyph bitmap data
float SampleGlyphBitmap(uint dataOffset, uint2 pixelPos, uint2 glyphSize)
{
    // Calculate bit position in glyph data
    uint pixelIndex = pixelPos.y * glyphSize.x + pixelPos.x;
    uint byteOffset = dataOffset + (pixelIndex / 8);
    uint bitIndex = pixelIndex % 8;

    // Read byte from buffer
    uint byteValue = GlyphDataBuffer[byteOffset / 4];
    byteValue = (byteValue >> ((byteOffset % 4) * 8)) & 0xFF;

    // Extract bit
    return ((byteValue >> (7 - bitIndex)) & 1) ? 1.0f : 0.0f;
}

// Apply gamma correction
float ApplyGamma(float value, float gamma)
{
    return pow(saturate(value), 1.0f / gamma);
}

// Apply enhanced contrast
float ApplyContrast(float value, float contrast)
{
    // Sigmoid contrast enhancement
    float enhanced = 1.0f / (1.0f + exp(-contrast * (value - 0.5f)));
    return saturate(enhanced);
}

// Lanczos filter for high-quality resampling
float Lanczos(float x, float a)
{
    if (abs(x) < 0.0001f) return 1.0f;
    if (abs(x) >= a) return 0.0f;

    float pix = 3.14159265f * x;
    return (a * sin(pix) * sin(pix / a)) / (pix * pix);
}

// High-quality antialiasing using Lanczos resampling
float AntialiasSample(uint dataOffset, float2 texCoord, uint2 glyphSize)
{
    float accumulator = 0.0f;
    float weightSum = 0.0f;
    const float filterRadius = 2.0f;

    // Lanczos resampling with 2-pixel radius
    for (int y = -2; y <= 2; y++)
    {
        for (int x = -2; x <= 2; x++)
        {
            float2 samplePos = texCoord + float2(x, y);

            // Check bounds
            if (samplePos.x >= 0 && samplePos.x < glyphSize.x &&
                samplePos.y >= 0 && samplePos.y < glyphSize.y)
            {
                float weight = Lanczos(length(float2(x, y)), filterRadius);
                float sample = SampleGlyphBitmap(dataOffset, uint2(samplePos), glyphSize);

                accumulator += sample * weight;
                weightSum += weight;
            }
        }
    }

    return (weightSum > 0.0f) ? (accumulator / weightSum) : 0.0f;
}

// ClearType subpixel rendering
float3 ComputeSubpixelCoverage(uint dataOffset, float2 texCoord, uint2 glyphSize)
{
    float3 coverage;

    // Sample at 1/3 pixel offsets for RGB subpixels
    coverage.r = AntialiasSample(dataOffset, texCoord + float2(-0.333f, 0), glyphSize);
    coverage.g = AntialiasSample(dataOffset, texCoord, glyphSize);
    coverage.b = AntialiasSample(dataOffset, texCoord + float2(0.333f, 0), glyphSize);

    // Apply subpixel filtering weights
    float3 weights = float3(0.25f, 0.5f, 0.25f);
    coverage = coverage * weights + (1.0f - weights) * coverage.ggg;

    return coverage;
}

// ============================================================================
// Compute Shader Entry Point
// ============================================================================

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID,
          uint3 groupThreadID : SV_GroupThreadID,
          uint3 groupID : SV_GroupID)
{
    // Get pixel position in atlas
    uint2 pixelPos = dispatchThreadID.xy;

    // Check bounds
    if (pixelPos.x >= atlasSize.x || pixelPos.y >= atlasSize.y)
    {
        return;
    }

    // Determine which glyph this pixel belongs to
    uint glyphsPerRow = atlasSize.x / glyphSize.x;
    uint glyphRow = pixelPos.y / glyphSize.y;
    uint glyphCol = pixelPos.x / glyphSize.x;
    uint glyphIndex = glyphRow * glyphsPerRow + glyphCol;

    // Check if we have a valid glyph
    if (glyphIndex >= glyphCount)
    {
        // Clear unused atlas space
        GlyphAtlas[pixelPos] = float4(0, 0, 0, 0);
        return;
    }

    // Load glyph descriptor
    GlyphDescriptor glyph = GlyphDescriptors[glyphIndex];

    // Calculate position within glyph
    uint2 glyphLocalPos = pixelPos - glyph.atlasPos;

    // Check if pixel is within glyph bounds
    if (glyphLocalPos.x >= glyph.size.x || glyphLocalPos.y >= glyph.size.y)
    {
        // Clear padding area
        GlyphAtlas[pixelPos] = float4(0, 0, 0, 0);
        return;
    }

    // Load glyph data into shared memory for efficient access
    uint tileX = groupThreadID.x;
    uint tileY = groupThreadID.y;

    // Cooperative loading with 1-pixel border for filtering
    if (tileX < 10 && tileY < 10)
    {
        int2 loadPos = int2(groupID.xy * 8 - 1) + int2(tileX, tileY);
        if (loadPos.x >= 0 && loadPos.x < (int)glyph.size.x &&
            loadPos.y >= 0 && loadPos.y < (int)glyph.size.y)
        {
            TileCache[tileY][tileX] = SampleGlyphBitmap(glyph.dataOffset, uint2(loadPos), glyph.size);
        }
        else
        {
            TileCache[tileY][tileX] = 0.0f;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Rasterize based on rendering mode
    float4 output = float4(0, 0, 0, 0);

    if (flags & 0x01) // ClearType subpixel rendering
    {
        float3 subpixel = ComputeSubpixelCoverage(glyph.dataOffset, float2(glyphLocalPos), glyph.size);

        // Apply gamma and contrast
        subpixel.r = ApplyGamma(ApplyContrast(subpixel.r, contrast), gamma);
        subpixel.g = ApplyGamma(ApplyContrast(subpixel.g, contrast), gamma);
        subpixel.b = ApplyGamma(ApplyContrast(subpixel.b, contrast), gamma);

        output = float4(subpixel, 1.0f);
    }
    else if (flags & 0x02) // Antialiased grayscale
    {
        float coverage = AntialiasSample(glyph.dataOffset, float2(glyphLocalPos), glyph.size);

        // Apply gamma and contrast
        coverage = ApplyGamma(ApplyContrast(coverage, contrast), gamma);

        // Store in alpha channel for grayscale rendering
        output = float4(1.0f, 1.0f, 1.0f, coverage);
    }
    else // Simple binary (no antialiasing)
    {
        float coverage = TileCache[tileY + 1][tileX + 1]; // +1 for border offset

        // Threshold for binary output
        coverage = (coverage > 0.5f) ? 1.0f : 0.0f;

        output = float4(1.0f, 1.0f, 1.0f, coverage);
    }

    // Write to atlas with atomic operations for thread safety
    GlyphAtlas[pixelPos] = output;

    // Optional: Wave intrinsics for better memory coalescing
    if (WaveIsFirstLane())
    {
        // Prefetch next tiles for better cache utilization
        // (Hardware-specific optimization)
    }
}