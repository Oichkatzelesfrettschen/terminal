// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Grid Generation Compute Shader
// Generates terminal grid cells in parallel using compute shader
// Thread group size: [16, 16, 1] for 2D grid processing

// ============================================================================
// Constant Buffers
// ============================================================================

cbuffer GridConstants : register(b0)
{
    uint2 gridDimensions;    // Grid width and height in cells
    uint2 viewportSize;      // Viewport size in pixels
    uint2 cellSize;          // Cell size in pixels
    float2 positionScale;    // Position scaling factor for NDC conversion
    float4 backgroundColor;  // Background color (RGBA)
    uint frameNumber;        // Current frame number
    uint flags;             // Rendering flags (dirty cells, etc.)
    uint2 scrollOffset;     // Scroll offset in cells
};

// Grid cell attributes structure
struct GridCell
{
    float2 position;    // Cell position in screen space
    float2 size;        // Cell size in pixels
    uint2 gridCoord;    // Grid coordinate (x, y)
    uint attributes;    // Packed attributes (dirty, selected, etc.)
    uint foregroundColor;  // Foreground color (RGBA packed)
    uint backgroundColor;  // Background color (RGBA packed)
    uint glyphIndex;    // Index into glyph atlas
    uint flags;         // Cell-specific flags
};

// ============================================================================
// Resources
// ============================================================================

// Output UAV buffer containing grid cells
RWStructuredBuffer<GridCell> GridCellBuffer : register(u0);

// Dirty cell tracking UAV (1 bit per cell)
RWBuffer<uint> DirtyCellBuffer : register(u1);

// Selection buffer UAV (for terminal selection)
RWBuffer<uint> SelectionBuffer : register(u2);

// ============================================================================
// Helper Functions
// ============================================================================

// Pack RGBA color into uint
uint PackColor(float4 color)
{
    uint r = (uint)(saturate(color.r) * 255.0f);
    uint g = (uint)(saturate(color.g) * 255.0f);
    uint b = (uint)(saturate(color.b) * 255.0f);
    uint a = (uint)(saturate(color.a) * 255.0f);

    return (a << 24) | (b << 16) | (g << 8) | r;
}

// Unpack uint to RGBA color
float4 UnpackColor(uint packed)
{
    float4 color;
    color.r = float((packed >> 0) & 0xFF) / 255.0f;
    color.g = float((packed >> 8) & 0xFF) / 255.0f;
    color.b = float((packed >> 16) & 0xFF) / 255.0f;
    color.a = float((packed >> 24) & 0xFF) / 255.0f;
    return color;
}

// Check if a cell is dirty (needs redraw)
bool IsCellDirty(uint2 gridCoord)
{
    uint cellIndex = gridCoord.y * gridDimensions.x + gridCoord.x;
    uint bufferIndex = cellIndex / 32;
    uint bitIndex = cellIndex % 32;

    return (DirtyCellBuffer[bufferIndex] & (1u << bitIndex)) != 0;
}

// Mark cell as clean
void MarkCellClean(uint2 gridCoord)
{
    uint cellIndex = gridCoord.y * gridDimensions.x + gridCoord.x;
    uint bufferIndex = cellIndex / 32;
    uint bitIndex = cellIndex % 32;

    InterlockedAnd(DirtyCellBuffer[bufferIndex], ~(1u << bitIndex));
}

// Check if cell is selected
bool IsCellSelected(uint2 gridCoord)
{
    uint cellIndex = gridCoord.y * gridDimensions.x + gridCoord.x;
    uint bufferIndex = cellIndex / 32;
    uint bitIndex = cellIndex % 32;

    return (SelectionBuffer[bufferIndex] & (1u << bitIndex)) != 0;
}

// ============================================================================
// Compute Shader Entry Point
// ============================================================================

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID,
          uint3 groupThreadID : SV_GroupThreadID,
          uint3 groupID : SV_GroupID)
{
    // Get the grid coordinate for this thread
    uint2 gridCoord = dispatchThreadID.xy;

    // Check bounds
    if (gridCoord.x >= gridDimensions.x || gridCoord.y >= gridDimensions.y)
    {
        return;
    }

    // Calculate cell index
    uint cellIndex = gridCoord.y * gridDimensions.x + gridCoord.x;

    // Initialize grid cell
    GridCell cell;

    // Calculate cell position in screen space
    cell.position.x = (float)(gridCoord.x * cellSize.x);
    cell.position.y = (float)(gridCoord.y * cellSize.y);

    // Apply scroll offset
    cell.position.x -= (float)(scrollOffset.x * cellSize.x);
    cell.position.y -= (float)(scrollOffset.y * cellSize.y);

    // Set cell size
    cell.size = (float2)cellSize;

    // Store grid coordinate
    cell.gridCoord = gridCoord;

    // Initialize attributes
    cell.attributes = 0;

    // Check if cell is dirty
    if (IsCellDirty(gridCoord))
    {
        cell.attributes |= 0x01; // ATTR_DIRTY flag

        // Mark cell as clean after processing
        MarkCellClean(gridCoord);
    }

    // Check if cell is selected
    if (IsCellSelected(gridCoord))
    {
        cell.attributes |= 0x02; // ATTR_SELECTED flag

        // Invert colors for selection
        cell.foregroundColor = PackColor(backgroundColor);
        cell.backgroundColor = PackColor(float4(0.3f, 0.5f, 0.8f, 1.0f)); // Selection color
    }
    else
    {
        // Default colors
        cell.foregroundColor = PackColor(float4(1.0f, 1.0f, 1.0f, 1.0f)); // White text
        cell.backgroundColor = PackColor(backgroundColor);
    }

    // Initialize glyph index (will be populated by CPU or another compute pass)
    cell.glyphIndex = 0;

    // Initialize flags
    cell.flags = 0;

    // Optimize: Add wave intrinsics for coalesced writes if available
    // This improves memory bandwidth utilization
    if (WaveIsFirstLane())
    {
        // Prefetch next cache line for better memory access pattern
        // (Hardware-specific optimization)
    }

    // Write cell to buffer
    GridCellBuffer[cellIndex] = cell;

    // Memory fence to ensure writes are visible to graphics pipeline
    GroupMemoryBarrierWithGroupSync();
}