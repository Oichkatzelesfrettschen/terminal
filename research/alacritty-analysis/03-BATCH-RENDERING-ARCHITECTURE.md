# Alacritty Batch Rendering Architecture - Deep Dive

## Overview

Alacritty achieves exceptional rendering performance through aggressive batching. Instead of one draw call per glyph, it batches up to 65,536 glyphs into a single draw call.

## Source Files

- `/alacritty/src/renderer/text/glsl3.rs` - OpenGL 3.3 renderer
- `/alacritty/src/renderer/text/gles2.rs` - OpenGL ES 2.0 renderer
- `/alacritty/src/renderer/text/mod.rs` - Shared rendering logic

## Two Renderer Paths

Alacritty has two rendering backends:

1. **GLSL3:** OpenGL 3.3+ (modern desktop)
2. **GLES2:** OpenGL ES 2.0 (compatibility mode)

### Renderer Selection

```rust
let (use_glsl3, allow_dsb) = match renderer_preference {
    Some(RendererPreference::Glsl3) => (true, true),
    Some(RendererPreference::Gles2) => (false, true),
    Some(RendererPreference::Gles2Pure) => (false, false),
    None => (shader_version.as_ref() >= "3.3" && !is_gles_context, true),
};
```

**Default:** GLSL3 if shader version >= 3.3 and not in GLES context

## GLSL3 Renderer (Instanced Rendering)

### Batch Configuration

```rust
/// Maximum items to be drawn in a batch.
const BATCH_MAX: usize = 0x1_0000;  // 65,536 instances
```

**Why 65,536?**
- Large enough for multiple full screens (typical screen: ~2,000-10,000 cells)
- Power of 2 (good for GPU)
- Fits well in GPU memory
- Rarely needs multiple batches

### Instance Data Structure

```rust
#[derive(Debug)]
#[repr(C)]
struct InstanceData {
    // Cell position (2 bytes)
    col: u16,                   // Column index
    row: u16,                   // Row index

    // Glyph metrics (8 bytes)
    left: i16,                  // Bearing X
    top: i16,                   // Bearing Y
    width: i16,                 // Glyph width
    height: i16,                // Glyph height

    // UV coordinates (16 bytes)
    uv_left: f32,               // UV left
    uv_bot: f32,                // UV bottom
    uv_width: f32,              // UV width
    uv_height: f32,             // UV height

    // Colors (7 bytes)
    r: u8,                      // Foreground red
    g: u8,                      // Foreground green
    b: u8,                      // Foreground blue
    cell_flags: RenderingGlyphFlags,  // Flags (1 byte)

    // Background color (4 bytes)
    bg_r: u8,                   // Background red
    bg_g: u8,                   // Background green
    bg_b: u8,                   // Background blue
    bg_a: u8,                   // Background alpha
}

// Total: 48 bytes per instance
```

**Memory for max batch:** 65,536 * 48 = 3,145,728 bytes (~3 MB)

### Rendering Cell Flags

```rust
bitflags! {
    #[repr(C)]
    #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
    struct RenderingGlyphFlags: u8 {
        const COLORED   = 0b0000_0001;  // Is colored emoji
        const WIDE_CHAR = 0b0000_0010;  // Is wide character (CJK, etc.)
    }
}
```

**Packed in single byte** - saves memory and bandwidth

### OpenGL Setup

```rust
unsafe {
    gl::Enable(gl::BLEND);
    gl::BlendFunc(gl::SRC1_COLOR, gl::ONE_MINUS_SRC1_COLOR);

    // Disable depth test (not needed for 2D rendering)
    gl::DepthMask(gl::FALSE);

    gl::GenVertexArrays(1, &mut vao);
    gl::GenBuffers(1, &mut ebo);
    gl::GenBuffers(1, &mut vbo_instance);
    gl::BindVertexArray(vao);

    // Element buffer (shared quad indices)
    let indices: [u32; 6] = [0, 1, 3, 1, 2, 3];
    gl::BindBuffer(gl::ELEMENT_ARRAY_BUFFER, ebo);
    gl::BufferData(
        gl::ELEMENT_ARRAY_BUFFER,
        (6 * size_of::<u32>()) as isize,
        indices.as_ptr() as *const _,
        gl::STATIC_DRAW,
    );

    // Instance buffer
    gl::BindBuffer(gl::ARRAY_BUFFER, vbo_instance);
    gl::BufferData(
        gl::ARRAY_BUFFER,
        (BATCH_MAX * size_of::<InstanceData>()) as isize,
        ptr::null(),
        gl::STREAM_DRAW,  // Hint: Data changes every frame
    );

    // Setup vertex attributes
    setup_instance_attributes();
}
```

### Vertex Attribute Setup

```rust
let mut index = 0;
let mut size = 0;

macro_rules! add_attr {
    ($count:expr, $gl_type:expr, $type:ty) => {
        gl::VertexAttribPointer(
            index,
            $count,
            $gl_type,
            gl::FALSE,
            size_of::<InstanceData>() as i32,
            size as *const _,
        );
        gl::EnableVertexAttribArray(index);
        gl::VertexAttribDivisor(index, 1);  // Advance per instance

        size += $count * size_of::<$type>();
        index += 1;
    };
}

// Coords (2x u16)
add_attr!(2, gl::UNSIGNED_SHORT, u16);

// Glyph offset and size (4x i16)
add_attr!(4, gl::SHORT, i16);

// UV offset (4x f32)
add_attr!(4, gl::FLOAT, f32);

// Color and cell flags (4x u8)
add_attr!(4, gl::UNSIGNED_BYTE, u8);

// Background color (4x u8)
add_attr!(4, gl::UNSIGNED_BYTE, u8);
```

**Key:** `glVertexAttribDivisor(index, 1)` - Each instance gets its own attribute data

### Batch Building

```rust
impl Batch {
    fn add_item(&mut self, cell: &RenderableCell, glyph: &Glyph, _: &SizeInfo) {
        if self.is_empty() {
            self.tex = glyph.tex_id;
        }

        let mut cell_flags = RenderingGlyphFlags::empty();
        cell_flags.set(RenderingGlyphFlags::COLORED, glyph.multicolor);
        cell_flags.set(RenderingGlyphFlags::WIDE_CHAR, cell.flags.contains(Flags::WIDE_CHAR));

        self.instances.push(InstanceData {
            col: cell.point.column.0 as u16,
            row: cell.point.line as u16,
            top: glyph.top,
            left: glyph.left,
            width: glyph.width,
            height: glyph.height,
            uv_bot: glyph.uv_bot,
            uv_left: glyph.uv_left,
            uv_width: glyph.uv_width,
            uv_height: glyph.uv_height,
            r: cell.fg.r,
            g: cell.fg.g,
            b: cell.fg.b,
            cell_flags,
            bg_r: cell.bg.r,
            bg_g: cell.bg.g,
            bg_b: cell.bg.b,
            bg_a: (cell.bg_alpha * 255.0) as u8,
        });
    }
}
```

### Flushing Logic

```rust
#[inline]
fn add_render_item(&mut self, cell: &RenderableCell, glyph: &Glyph, size_info: &SizeInfo) {
    // Flush batch if texture changing
    if !self.batch().is_empty() && self.batch().tex() != glyph.tex_id {
        self.render_batch();
    }

    self.batch().add_item(cell, glyph, size_info);

    // Render batch and clear if it's full
    if self.batch().full() {
        self.render_batch();
    }
}
```

**Flush triggers:**
1. Texture atlas change (different atlas)
2. Batch full (65,536 instances)

### Rendering the Batch

```rust
fn render_batch(&mut self) {
    unsafe {
        // Upload instance data to GPU
        gl::BufferSubData(
            gl::ARRAY_BUFFER,
            0,
            self.batch.size() as isize,
            self.batch.instances.as_ptr() as *const _,
        );
    }

    // Bind texture if necessary
    if *self.active_tex != self.batch.tex() {
        unsafe {
            gl::BindTexture(gl::TEXTURE_2D, self.batch.tex());
        }
        *self.active_tex = self.batch.tex();
    }

    unsafe {
        // Pass 1: Draw backgrounds
        self.program.set_rendering_pass(RenderingPass::Background);
        gl::DrawElementsInstanced(
            gl::TRIANGLES,
            6,                              // 6 indices per quad
            gl::UNSIGNED_INT,
            ptr::null(),
            self.batch.len() as GLsizei,    // Number of instances
        );

        // Pass 2: Draw text glyphs
        self.program.set_rendering_pass(RenderingPass::SubpixelPass1);
        gl::DrawElementsInstanced(
            gl::TRIANGLES,
            6,
            gl::UNSIGNED_INT,
            ptr::null(),
            self.batch.len() as GLsizei,
        );
    }

    self.batch.clear();
}
```

**Two draw calls per batch:**
1. Background pass
2. Text pass

**Total GPU calls for full batch:**
- 1x glBufferSubData (~3 MB)
- 0-1x glBindTexture (only if changed)
- 2x glDrawElementsInstanced

### Rendering Passes

```rust
#[repr(u8)]
enum RenderingPass {
    Background = 0,     // Draw cell backgrounds
    SubpixelPass1 = 1,  // Draw text glyphs
    SubpixelPass2 = 2,  // GLES2 only
    SubpixelPass3 = 3,  // GLES2 only
}
```

## GLES2 Renderer (Vertex-Based)

### Batch Configuration

```rust
const BATCH_MAX: usize = (u16::MAX - u16::MAX % 4) as usize;
// = 65,532 vertices (4 vertices per glyph)
// = 16,383 glyphs max
```

**Why u16 limit?**
- GLES2 only supports 16-bit indices
- Must be divisible by 4 (4 vertices per glyph)

### Vertex Data Structure

```rust
#[repr(C)]
#[derive(Debug, Copy, Clone)]
struct TextVertex {
    // Cell coordinates (4 bytes)
    x: i16,
    y: i16,

    // Glyph coordinates (4 bytes)
    glyph_x: i16,
    glyph_y: i16,

    // UV coordinates (8 bytes)
    u: f32,
    v: f32,

    // Foreground color (3 bytes)
    r: u8,
    g: u8,
    b: u8,

    // Flags (1 byte)
    colored: RenderingGlyphFlags,

    // Background color (4 bytes)
    bg_r: u8,
    bg_g: u8,
    bg_b: u8,
    bg_a: u8,
}

// Total: 32 bytes per vertex
// 4 vertices per glyph = 128 bytes per glyph
```

**Memory for max batch:** 65,532 * 32 = 2,097,024 bytes (~2 MB)

### Batch Building (GLES2)

```rust
fn add_item(&mut self, cell: &RenderableCell, glyph: &Glyph, size_info: &SizeInfo) {
    if self.is_empty() {
        self.tex = glyph.tex_id;
    }

    // Calculate cell position
    let x = cell.point.column.0 as i16 * size_info.cell_width() as i16;
    let y = cell.point.line as i16 * size_info.cell_height() as i16;

    // Calculate glyph position
    let glyph_x = cell.point.column.0 as i16 * size_info.cell_width() as i16 + glyph.left;
    let glyph_y = (cell.point.line + 1) as i16 * size_info.cell_height() as i16 - glyph.top;

    let colored = if glyph.multicolor {
        RenderingGlyphFlags::COLORED
    } else {
        RenderingGlyphFlags::empty()
    };

    let is_wide = if cell.flags.contains(Flags::WIDE_CHAR) { 2 } else { 1 };

    // Create 4 vertices for quad (bottom-left, top-left, top-right, bottom-right)
    let mut vertex = TextVertex { /* ... */ };

    // Bottom-left
    self.vertices.push(vertex);

    // Top-left
    vertex.y = y;
    vertex.glyph_y = glyph_y;
    vertex.v = glyph.uv_bot;
    self.vertices.push(vertex);

    // Top-right
    vertex.x = x + is_wide * size_info.cell_width() as i16;
    vertex.glyph_x = glyph_x + glyph.width;
    vertex.u = glyph.uv_left + glyph.uv_width;
    self.vertices.push(vertex);

    // Bottom-right
    vertex.y = y + size_info.cell_height() as i16;
    vertex.glyph_y = glyph_y + glyph.height;
    vertex.v = glyph.uv_bot + glyph.uv_height;
    self.vertices.push(vertex);
}
```

### Rendering (GLES2)

```rust
fn render_batch(&mut self) {
    unsafe {
        gl::BufferSubData(
            gl::ARRAY_BUFFER,
            0,
            self.batch.size() as isize,
            self.batch.vertices.as_ptr() as *const _,
        );
    }

    if *self.active_tex != self.batch.tex() {
        unsafe {
            gl::BindTexture(gl::TEXTURE_2D, self.batch.tex());
        }
        *self.active_tex = self.batch.tex();
    }

    unsafe {
        let num_indices = (self.batch.len() / 4 * 6) as i32;

        // Background pass
        self.program.set_rendering_pass(RenderingPass::Background);
        gl::BlendFunc(gl::ONE, gl::ZERO);
        gl::DrawElements(gl::TRIANGLES, num_indices, gl::UNSIGNED_SHORT, ptr::null());

        if self.dual_source_blending {
            // Single text pass with dual-source blending
            self.program.set_rendering_pass(RenderingPass::SubpixelPass1);
            gl::BlendFunc(gl::SRC1_COLOR, gl::ONE_MINUS_SRC1_COLOR);
            gl::DrawElements(gl::TRIANGLES, num_indices, gl::UNSIGNED_SHORT, ptr::null());
        } else {
            // Three-pass subpixel rendering
            // Pass 1: Subtract text from background
            self.program.set_rendering_pass(RenderingPass::SubpixelPass1);
            gl::BlendFuncSeparate(gl::ZERO, gl::ONE_MINUS_SRC_COLOR, gl::ZERO, gl::ONE);
            gl::DrawElements(gl::TRIANGLES, num_indices, gl::UNSIGNED_SHORT, ptr::null());

            // Pass 2: Add inverted alpha
            self.program.set_rendering_pass(RenderingPass::SubpixelPass2);
            gl::BlendFuncSeparate(gl::ONE_MINUS_DST_ALPHA, gl::ONE, gl::ZERO, gl::ONE);
            gl::DrawElements(gl::TRIANGLES, num_indices, gl::UNSIGNED_SHORT, ptr::null());

            // Pass 3: Blend text color
            self.program.set_rendering_pass(RenderingPass::SubpixelPass3);
            gl::BlendFuncSeparate(gl::ONE, gl::ONE, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
            gl::DrawElements(gl::TRIANGLES, num_indices, gl::UNSIGNED_SHORT, ptr::null());
        }
    }

    self.batch.clear();
}
```

**GLES2 draw calls per batch:**
- With dual-source blending: 2 passes (background + text)
- Without dual-source blending: 4 passes (background + 3 text passes)

## Performance Comparison

### GLSL3 vs GLES2

| Metric | GLSL3 | GLES2 |
|--------|-------|-------|
| **Max glyphs per batch** | 65,536 | 16,383 |
| **Data per glyph** | 48 bytes | 128 bytes |
| **Max batch size** | ~3 MB | ~2 MB |
| **Draw calls per batch** | 2 | 2-4 |
| **GPU efficiency** | Higher (instancing) | Lower (vertex duplication) |
| **Compatibility** | Modern GPUs | All GPUs |

**GLSL3 wins:**
- 4x more glyphs per batch
- 2.67x less data per glyph
- Fewer draw calls
- Better GPU utilization

## Typical Rendering Scenario

### Full Screen Redraw (80x24 terminal)

**Setup:**
- Screen size: 80 columns x 24 rows = 1,920 cells
- Assume all cells visible and unique

**GLSL3:**
```
Total glyphs: 1,920
Batches: 1 (fits in 65,536 limit)
Draw calls: 2 (background + text)
Data uploaded: 1,920 * 48 = 92,160 bytes (~90 KB)
Texture bindings: 1 (assuming single atlas)
```

**GLES2:**
```
Total glyphs: 1,920
Batches: 1 (fits in 16,383 limit)
Draw calls: 2-4 (depending on dual-source blending)
Data uploaded: 1,920 * 128 = 245,760 bytes (~240 KB)
Texture bindings: 1
```

### Large Terminal (200x60)

**Setup:**
- Screen size: 200 columns x 60 rows = 12,000 cells

**GLSL3:**
```
Total glyphs: 12,000
Batches: 1
Draw calls: 2
Data uploaded: 12,000 * 48 = 576,000 bytes (~563 KB)
```

**GLES2:**
```
Total glyphs: 12,000
Batches: 1
Draw calls: 2-4
Data uploaded: 12,000 * 128 = 1,536,000 bytes (~1.5 MB)
```

### Extreme Case (Multiple Atlases)

**Setup:**
- Screen size: 200x60 = 12,000 cells
- Using 3 different atlases (rare)

**GLSL3:**
```
Total glyphs: 12,000
Batches: 3 (one per atlas)
Draw calls: 6 (2 per batch)
Data uploaded: ~563 KB (split across batches)
Texture bindings: 3
```

## Transferable Concepts for D3D12

### 1. Structured Buffer for Instances

```cpp
struct InstanceData {
    uint16_t col, row;           // 4 bytes
    int16_t left, top;           // 4 bytes
    int16_t width, height;       // 4 bytes
    float uvLeft, uvBot;         // 8 bytes
    float uvWidth, uvHeight;     // 8 bytes
    uint8_t r, g, b, flags;      // 4 bytes
    uint8_t bgR, bgG, bgB, bgA;  // 4 bytes
};  // 48 bytes total

// Create structured buffer
D3D12_RESOURCE_DESC bufferDesc = {};
bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
bufferDesc.Width = 65536 * sizeof(InstanceData);
bufferDesc.Height = 1;
bufferDesc.DepthOrArraySize = 1;
bufferDesc.MipLevels = 1;
bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
bufferDesc.SampleDesc.Count = 1;
bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

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
```

### 2. Instanced Drawing

```cpp
// Upload instance data
void* mappedData;
instanceBuffer->Map(0, nullptr, &mappedData);
memcpy(mappedData, instances.data(), instances.size() * sizeof(InstanceData));
instanceBuffer->Unmap(0, nullptr);

// Set structured buffer SRV
commandList->SetGraphicsRootShaderResourceView(0, instanceBuffer->GetGPUVirtualAddress());

// Draw instanced
commandList->DrawIndexedInstanced(
    6,                      // IndexCountPerInstance (quad = 2 triangles = 6 indices)
    instances.size(),       // InstanceCount
    0,                      // StartIndexLocation
    0,                      // BaseVertexLocation
    0                       // StartInstanceLocation
);
```

### 3. Vertex Shader (HLSL)

```hlsl
struct InstanceData {
    uint2 cellPos;      // col, row
    int4 glyphMetrics;  // left, top, width, height
    float4 uvRect;      // uvLeft, uvBot, uvWidth, uvHeight
    uint4 colors;       // Packed RGBA colors + flags
};

StructuredBuffer<InstanceData> instances : register(t0);

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    float4 bgColor : COLOR1;
};

VSOutput main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID) {
    InstanceData inst = instances[instanceID];

    // Compute quad corner from vertexID
    float2 corner;
    corner.x = (vertexID == 0 || vertexID == 1) ? 1.0 : 0.0;
    corner.y = (vertexID == 0 || vertexID == 3) ? 0.0 : 1.0;

    // Compute position
    float2 cellPos = float2(inst.cellPos) * cellDim;
    float2 glyphSize = float2(inst.glyphMetrics.zw);
    float2 glyphOffset = float2(inst.glyphMetrics.xy);

    float2 pos = cellPos + glyphSize * corner + glyphOffset;

    VSOutput output;
    output.position = float4(projection.xy + projection.zw * pos, 0.0, 1.0);
    output.texCoord = inst.uvRect.xy + corner * inst.uvRect.zw;
    output.color = UnpackColor(inst.colors.rgb);
    output.bgColor = UnpackColor(inst.colors.a);

    return output;
}
```

### 4. Dual-Pass Rendering

```cpp
// Pass 1: Backgrounds
commandList->SetPipelineState(backgroundPSO);
commandList->SetGraphicsRoot32BitConstant(0, 0, 0); // renderingPass = 0
commandList->DrawIndexedInstanced(6, batchSize, 0, 0, 0);

// Pass 2: Text
commandList->SetPipelineState(textPSO);
commandList->SetGraphicsRoot32BitConstant(0, 1, 0); // renderingPass = 1
commandList->DrawIndexedInstanced(6, batchSize, 0, 0, 0);
```

### 5. Batch Flushing

```cpp
class GlyphBatch {
private:
    std::vector<InstanceData> instances;
    ID3D12Resource* currentAtlas;
    static constexpr size_t MAX_BATCH_SIZE = 65536;

public:
    void AddGlyph(const Cell& cell, const Glyph& glyph) {
        // Flush if texture changes
        if (!instances.empty() && currentAtlas != glyph.atlasTexture) {
            Flush();
        }

        instances.push_back(CreateInstanceData(cell, glyph));

        // Flush if batch full
        if (instances.size() >= MAX_BATCH_SIZE) {
            Flush();
        }
    }

    void Flush() {
        if (instances.empty()) return;

        // Upload and draw
        UploadInstances();
        Draw();

        instances.clear();
    }
};
```

## Performance Best Practices

### 1. Minimize State Changes

```cpp
// BAD: Change texture for every glyph
for (auto& cell : cells) {
    BindTexture(cell.glyph.atlas);
    DrawGlyph(cell);
}

// GOOD: Batch by atlas
for (auto& atlas : atlases) {
    BindTexture(atlas);
    for (auto& cell : cellsUsingAtlas[atlas]) {
        AddToBatch(cell);
    }
    FlushBatch();
}
```

### 2. Pre-Allocate Buffers

```cpp
// BAD: Allocate every frame
auto buffer = CreateBuffer(batchSize);
UploadData(buffer, data);
Draw(buffer);
buffer.Release();

// GOOD: Reuse across frames
// Initialization
auto uploadBuffer = CreateBuffer(MAX_BATCH_SIZE);

// Per frame
UploadData(uploadBuffer, data);
Draw(uploadBuffer);
// Keep buffer alive for next frame
```

### 3. Use Structured Buffers

```cpp
// BAD: Multiple vertex buffers
SetVertexBuffer(0, positionsBuffer);
SetVertexBuffer(1, uvsBuffer);
SetVertexBuffer(2, colorsBuffer);

// GOOD: Single structured buffer
SetShaderResourceView(0, instanceBuffer);
```

## Conclusion

Alacritty's batch rendering achieves exceptional performance through:

1. **Large batches** - Up to 65,536 glyphs per draw call
2. **Instanced rendering** - Minimal data duplication
3. **Smart flushing** - Only on texture change or batch full
4. **Efficient data packing** - 48 bytes per instance
5. **Dual-pass rendering** - Background + text in 2 draw calls

For D3D12:
- Use structured buffers for instance data
- DrawIndexedInstanced with SV_InstanceID
- Flush batches on resource binding changes
- Target 50,000+ instances per batch

**Key insight:** The fewer times you talk to the GPU, the faster you go.
