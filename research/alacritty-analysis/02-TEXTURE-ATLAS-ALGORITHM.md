# Alacritty Texture Atlas Algorithm - Deep Dive

## Overview

The texture atlas is where all rasterized glyphs are stored on the GPU. Alacritty uses a row-packing algorithm with dynamic expansion.

## Source File

`/alacritty/src/renderer/text/atlas.rs`

## Atlas Structure

```rust
pub struct Atlas {
    /// Texture id for this atlas.
    id: GLuint,

    /// Width of atlas (always 1024).
    width: i32,

    /// Height of atlas (always 1024).
    height: i32,

    /// Left-most free pixel in current row.
    row_extent: i32,

    /// Baseline Y position for current row.
    row_baseline: i32,

    /// Tallest glyph in current row.
    row_tallest: i32,

    /// Whether running in GLES context.
    is_gles_context: bool,
}
```

## Atlas Constants

```rust
pub const ATLAS_SIZE: i32 = 1024;
```

**Size:** 1024x1024 pixels
**Format:** RGBA8 (4 bytes per pixel)
**Memory:** 4,194,304 bytes (4 MB) per atlas

## Row-Packing Algorithm

### Visual Layout

```
(0,0)
  +------------------------------------------------------------------+ (1024,0)
  | Glyph 1   | Glyph 2    | Glyph 3   | Glyph 4  | ...            |
  | 10x12     | 8x12       | 12x12     | 6x8      |                |
  +-----------+------------+-----------+----------+                |
  |                                                                 |
  | Glyph 5       | Glyph 6   | ...                                |
  | 20x16         | 15x16     |                                    |
  +---------------+-----------+                                    |
  |                                                                 |
  | Glyph 7          | Glyph 8     | Glyph 9  | ...                |
  | 25x18            | 10x14       | 8x12     |                    |
  +------------------+-------------+----------+                    |
  |                                                                 |
  ...
  |                                                                 |
  +------------------------------------------------------------------+ (1024,1024)
                                                               (row_extent, row_baseline)

Row 1: row_baseline=0,  row_tallest=12, row_extent=36
Row 2: row_baseline=12, row_tallest=16, row_extent=35
Row 3: row_baseline=28, row_tallest=18, row_extent=43
```

### Packing Logic

```rust
pub fn insert(
    &mut self,
    glyph: &RasterizedGlyph,
    active_tex: &mut u32,
) -> Result<Glyph, AtlasInsertError> {
    // 1. Check if glyph fits in atlas at all
    if glyph.width > self.width || glyph.height > self.height {
        return Err(AtlasInsertError::GlyphTooLarge);
    }

    // 2. Check if there's room in current row
    if !self.room_in_row(glyph) {
        self.advance_row()?;  // Move to next row
    }

    // 3. Still no room? Atlas is full
    if !self.room_in_row(glyph) {
        return Err(AtlasInsertError::Full);
    }

    // 4. Insert the glyph
    Ok(self.insert_inner(glyph, active_tex))
}

fn room_in_row(&self, raw: &RasterizedGlyph) -> bool {
    let next_extent = self.row_extent + raw.width;
    let enough_width = next_extent <= self.width;
    let enough_height = raw.height < (self.height - self.row_baseline);

    enough_width && enough_height
}

fn advance_row(&mut self) -> Result<(), AtlasInsertError> {
    let advance_to = self.row_baseline + self.row_tallest;
    if self.height - advance_to <= 0 {
        return Err(AtlasInsertError::Full);
    }

    self.row_baseline = advance_to;
    self.row_extent = 0;
    self.row_tallest = 0;

    Ok(())
}
```

### Insertion Process

```rust
fn insert_inner(&mut self, glyph: &RasterizedGlyph, active_tex: &mut u32) -> Glyph {
    let offset_y = self.row_baseline;
    let offset_x = self.row_extent;
    let height = glyph.height;
    let width = glyph.width;
    let multicolor;

    unsafe {
        gl::BindTexture(gl::TEXTURE_2D, self.id);

        // Handle RGB vs RGBA buffers
        let (format, buffer) = match &glyph.buffer {
            BitmapBuffer::Rgb(buffer) => {
                multicolor = false;
                if self.is_gles_context {
                    // GLES requires RGBA, convert RGB to RGBA
                    let mut new_buffer = Vec::with_capacity(buffer.len() / 3 * 4);
                    for rgb in buffer.chunks_exact(3) {
                        new_buffer.push(rgb[0]);
                        new_buffer.push(rgb[1]);
                        new_buffer.push(rgb[2]);
                        new_buffer.push(u8::MAX);  // Full alpha
                    }
                    (gl::RGBA, Cow::Owned(new_buffer))
                } else {
                    (gl::RGB, Cow::Borrowed(buffer))
                }
            },
            BitmapBuffer::Rgba(buffer) => {
                multicolor = true;
                (gl::RGBA, Cow::Borrowed(buffer))
            },
        };

        // Upload glyph to texture atlas
        gl::TexSubImage2D(
            gl::TEXTURE_2D,
            0,                  // mip level
            offset_x,           // x offset
            offset_y,           // y offset
            width,              // width
            height,             // height
            format,             // format (RGB or RGBA)
            gl::UNSIGNED_BYTE,  // type
            buffer.as_ptr() as *const _,
        );

        gl::BindTexture(gl::TEXTURE_2D, 0);
        *active_tex = 0;
    }

    // Update row state
    self.row_extent = offset_x + width;
    if height > self.row_tallest {
        self.row_tallest = height;
    }

    // Generate UV coordinates (normalized to [0,1])
    let uv_bot = offset_y as f32 / self.height as f32;
    let uv_left = offset_x as f32 / self.width as f32;
    let uv_height = height as f32 / self.height as f32;
    let uv_width = width as f32 / self.width as f32;

    Glyph {
        tex_id: self.id,
        multicolor,
        top: glyph.top as i16,
        left: glyph.left as i16,
        width: width as i16,
        height: height as i16,
        uv_bot,
        uv_left,
        uv_width,
        uv_height,
    }
}
```

## Multi-Atlas Support

```rust
pub fn load_glyph(
    active_tex: &mut GLuint,
    atlas: &mut Vec<Atlas>,
    current_atlas: &mut usize,
    rasterized: &RasterizedGlyph,
) -> Glyph {
    // Try to insert into current atlas
    match atlas[*current_atlas].insert(rasterized, active_tex) {
        Ok(glyph) => glyph,
        Err(AtlasInsertError::Full) => {
            // Current atlas is full, try next one
            let is_gles_context = atlas[*current_atlas].is_gles_context;

            *current_atlas += 1;
            if *current_atlas == atlas.len() {
                // No next atlas exists, create new one
                let new = Atlas::new(ATLAS_SIZE, is_gles_context);
                *active_tex = 0;
                atlas.push(new);
            }

            // Recursively insert into next atlas
            Atlas::load_glyph(active_tex, atlas, current_atlas, rasterized)
        },
        Err(AtlasInsertError::GlyphTooLarge) => {
            // Glyph is too large, return empty glyph
            Glyph {
                tex_id: atlas[*current_atlas].id,
                multicolor: false,
                top: 0,
                left: 0,
                width: 0,
                height: 0,
                uv_bot: 0.,
                uv_left: 0.,
                uv_width: 0.,
                uv_height: 0.,
            }
        },
    }
}
```

**Atlas Expansion:**
1. Start with single 1024x1024 atlas
2. When full, create new atlas
3. No limit on number of atlases
4. Each atlas is a separate GL texture

## Atlas Initialization

```rust
pub fn new(size: i32, is_gles_context: bool) -> Self {
    let mut id: GLuint = 0;
    unsafe {
        // Set pixel unpack alignment to 1 byte (for non-aligned rows)
        gl::PixelStorei(gl::UNPACK_ALIGNMENT, 1);

        // Create texture
        gl::GenTextures(1, &mut id);
        gl::BindTexture(gl::TEXTURE_2D, id);

        // Allocate texture storage (RGBA, no initial data)
        gl::TexImage2D(
            gl::TEXTURE_2D,
            0,                  // mip level
            gl::RGBA as i32,    // internal format
            size,               // width
            size,               // height
            0,                  // border (must be 0)
            gl::RGBA,           // format
            gl::UNSIGNED_BYTE,  // type
            ptr::null(),        // no initial data
        );

        // Set texture parameters
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE as i32);
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE as i32);
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, gl::LINEAR as i32);
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, gl::LINEAR as i32);

        gl::BindTexture(gl::TEXTURE_2D, 0);
    }

    Self {
        id,
        width: size,
        height: size,
        row_extent: 0,
        row_baseline: 0,
        row_tallest: 0,
        is_gles_context,
    }
}
```

**Texture Parameters:**
- `GL_CLAMP_TO_EDGE` - Prevents texture bleeding at edges
- `GL_LINEAR` - Bilinear filtering for smooth scaling
- `UNPACK_ALIGNMENT = 1` - Handle non-4-byte-aligned rows

## Atlas Clearing

```rust
pub fn clear(&mut self) {
    self.row_extent = 0;
    self.row_baseline = 0;
    self.row_tallest = 0;
}

pub fn clear_atlas(atlas: &mut [Atlas], current_atlas: &mut usize) {
    for atlas in atlas.iter_mut() {
        atlas.clear();
    }
    *current_atlas = 0;
}
```

**Note:** Texture memory is NOT cleared - just packing state reset. Old glyphs are overwritten as needed.

## Texture Format Handling

### Desktop OpenGL (Core Profile)

```rust
// Can upload RGB directly to RGBA texture
gl::TexSubImage2D(..., gl::RGB, gl::UNSIGNED_BYTE, ...);
```

**Driver converts RGB to RGBA automatically**

### OpenGL ES 2.0

```rust
// Must manually convert RGB to RGBA
if self.is_gles_context {
    let mut new_buffer = Vec::with_capacity(buffer.len() / 3 * 4);
    for rgb in buffer.chunks_exact(3) {
        new_buffer.push(rgb[0]);  // R
        new_buffer.push(rgb[1]);  // G
        new_buffer.push(rgb[2]);  // B
        new_buffer.push(u8::MAX); // A = 255
    }
    (gl::RGBA, Cow::Owned(new_buffer))
}
```

**GLES requires format match between texture and upload**

## UV Coordinate Generation

```rust
// Generate normalized UV coordinates [0, 1]
let uv_bot = offset_y as f32 / self.height as f32;
let uv_left = offset_x as f32 / self.width as f32;
let uv_height = height as f32 / self.height as f32;
let uv_width = width as f32 / self.width as f32;

// Example for 10x12 glyph at (100, 50) in 1024x1024 atlas:
// uv_bot    = 50 / 1024.0  = 0.0488
// uv_left   = 100 / 1024.0 = 0.0977
// uv_width  = 10 / 1024.0  = 0.0098
// uv_height = 12 / 1024.0  = 0.0117

// In shader:
// TexCoords = uvOffset + position * uvSize
// position is [0,1] for quad corners
```

## Space Efficiency Analysis

### Best Case (Uniform Glyphs)

```
All glyphs 10x12 pixels:
- Glyphs per row: 1024 / 10 = 102 glyphs
- Rows: 1024 / 12 = 85 rows
- Total glyphs: 102 * 85 = 8,670 glyphs
- Efficiency: 8,670 * 120 pixels / (1024*1024) = 100%
```

### Typical Case (Variable Glyphs)

```
Average glyph: 8x14 pixels
- Estimated glyphs: ~7,000
- Wasted space: ~20% (row fragmentation)
- Efficiency: ~80%
```

### Worst Case (Extreme Variation)

```
Mix of 4x6 and 30x40 glyphs:
- Severe row fragmentation
- Efficiency: ~60%
- Still acceptable for typical terminal use
```

## Performance Characteristics

### Time Complexity

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Insert glyph | O(1) | Constant time append to row |
| Advance row | O(1) | Simple pointer arithmetic |
| Atlas creation | O(n^2) | n = atlas size, but done once |
| Atlas clear | O(1) | Just reset pointers |

### Space Complexity

| Structure | Size | Notes |
|-----------|------|-------|
| Atlas metadata | ~48 bytes | Per atlas |
| Texture memory | 4 MB | Per 1024x1024 RGBA atlas |
| Typical usage | 4-8 MB | 1-2 atlases for most terminals |

## Cache Locality

**Good:**
- Sequential writes within a row (CPU cache friendly)
- GPU texture cache benefits from spatial locality
- Commonly used glyphs (ASCII) packed together at start

**Bad:**
- Random access during rendering (texture sampling)
- But GPU texture cache handles this well

## Transferable Concepts for D3D12

### 1. Atlas Structure

```cpp
struct TextureAtlas {
    ID3D12Resource* texture;        // Committed resource
    UINT width;                     // 1024
    UINT height;                    // 1024
    UINT rowExtent;                 // Current X position
    UINT rowBaseline;               // Current Y position
    UINT rowTallest;                // Tallest glyph in row
};

std::vector<TextureAtlas> atlases;
size_t currentAtlas = 0;
```

### 2. Row-Packing Algorithm

```cpp
bool HasRoomInRow(const RasterizedGlyph& glyph) {
    auto& atlas = atlases[currentAtlas];
    UINT nextExtent = atlas.rowExtent + glyph.width;
    bool enoughWidth = nextExtent <= atlas.width;
    bool enoughHeight = glyph.height < (atlas.height - atlas.rowBaseline);
    return enoughWidth && enoughHeight;
}

void AdvanceRow() {
    auto& atlas = atlases[currentAtlas];
    UINT advanceTo = atlas.rowBaseline + atlas.rowTallest;

    if (atlas.height - advanceTo <= 0) {
        // Atlas full, create new one
        CreateNewAtlas();
        return;
    }

    atlas.rowBaseline = advanceTo;
    atlas.rowExtent = 0;
    atlas.rowTallest = 0;
}
```

### 3. Texture Creation

```cpp
void CreateAtlas() {
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = 1024;
    desc.Height = 1024;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    ID3D12Resource* texture;
    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture)
    );

    TextureAtlas atlas;
    atlas.texture = texture;
    atlas.width = 1024;
    atlas.height = 1024;
    atlas.rowExtent = 0;
    atlas.rowBaseline = 0;
    atlas.rowTallest = 0;

    atlases.push_back(atlas);
}
```

### 4. Glyph Upload

```cpp
void UploadGlyph(const RasterizedGlyph& glyph) {
    auto& atlas = atlases[currentAtlas];

    // Create upload buffer
    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = glyph.width * glyph.height * 4; // RGBA
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    ID3D12Resource* uploadBuffer;
    device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)
    );

    // Map and copy data
    void* data;
    uploadBuffer->Map(0, nullptr, &data);
    memcpy(data, glyph.buffer, glyph.width * glyph.height * 4);
    uploadBuffer->Unmap(0, nullptr);

    // Copy to atlas texture
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = atlas.texture;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = uploadBuffer;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = 0;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    src.PlacedFootprint.Footprint.Width = glyph.width;
    src.PlacedFootprint.Footprint.Height = glyph.height;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = glyph.width * 4;

    D3D12_BOX srcBox = {0, 0, 0, glyph.width, glyph.height, 1};

    commandList->CopyTextureRegion(
        &dst,
        atlas.rowExtent,    // DstX
        atlas.rowBaseline,  // DstY
        0,                  // DstZ
        &src,
        &srcBox
    );

    // Update row state
    atlas.rowExtent += glyph.width;
    if (glyph.height > atlas.rowTallest) {
        atlas.rowTallest = glyph.height;
    }
}
```

### 5. UV Coordinate Generation

```cpp
struct UVRect {
    float left, top, width, height;
};

UVRect GenerateUV(UINT x, UINT y, UINT w, UINT h) {
    const float atlasSize = 1024.0f;
    return {
        x / atlasSize,
        y / atlasSize,
        w / atlasSize,
        h / atlasSize
    };
}
```

## Optimization Notes

### Why 1024x1024?

1. **Power of two:** Better GPU cache alignment
2. **Not too small:** Fits ~7,000 typical glyphs
3. **Not too large:** 4MB is reasonable memory footprint
4. **Common size:** Well-supported across all GPUs

### Why Row-Packing vs. Other Algorithms?

**Alternatives:**
1. **Shelf packing:** Similar to row-packing (used by Alacritty)
2. **Skyline packing:** More complex, marginally better space efficiency
3. **Guillotine:** Faster but worse space efficiency
4. **Binary tree:** Good for varying sizes but more complex

**Alacritty chooses row-packing because:**
- Simple to implement
- Good enough efficiency (~80%)
- Fast O(1) insertion
- No rebalancing needed

### Why No Compaction?

When atlas is full, create new atlas instead of compacting existing:

**Pros of no compaction:**
- Simple implementation
- No GPU->CPU readback
- No texture copying
- Fast atlas creation

**Cons:**
- Wasted space in old atlases
- More texture bindings during rendering

**Verdict:** Memory is cheap, simplicity wins

## Conclusion

Alacritty's texture atlas algorithm achieves:

1. **Simple** - Row-packing with minimal state
2. **Fast** - O(1) insertion, no compaction
3. **Scalable** - Unlimited atlases
4. **Efficient** - ~80% space utilization
5. **Portable** - Works on all OpenGL implementations

Direct translation to D3D12 is straightforward with committed resources and texture regions.
