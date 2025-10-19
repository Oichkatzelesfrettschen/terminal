#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>

namespace Microsoft::Console::Render
{
    struct ShapedRowD3D12
    {
        std::vector<uint16_t> glyphIndices;
        std::vector<float> glyphAdvances;
        std::vector<DWRITE_GLYPH_OFFSET> glyphOffsets;
        std::vector<uint32_t> colors;
    };
}
