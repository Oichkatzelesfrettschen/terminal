# Alacritty Shader Techniques - Deep Dive

## Overview

Alacritty's shaders are designed for maximum performance with minimal complexity. They leverage modern GPU features while maintaining compatibility with older hardware.

## Shader Files

- `/alacritty/res/glsl3/text.v.glsl` - GLSL 3.3 vertex shader
- `/alacritty/res/glsl3/text.f.glsl` - GLSL 3.3 fragment shader
- `/alacritty/res/gles2/text.v.glsl` - GLES 2.0 vertex shader
- `/alacritty/res/gles2/text.f.glsl` - GLES 2.0 fragment shader
- `/alacritty/res/rect.v.glsl` - Rectangle rendering vertex shader
- `/alacritty/res/rect.f.glsl` - Rectangle rendering fragment shader

## GLSL 3.3 Vertex Shader

### Full Source

```glsl
#version 330 core

// Cell properties.
layout(location = 0) in vec2 gridCoords;

// Glyph properties.
layout(location = 1) in vec4 glyph;

// uv mapping.
layout(location = 2) in vec4 uv;

// Text foreground rgb packed together with cell flags
layout(location = 3) in vec4 textColor;

// Background color.
layout(location = 4) in vec4 backgroundColor;

out vec2 TexCoords;
flat out vec4 fg;
flat out vec4 bg;

// Terminal properties
uniform vec2 cellDim;
uniform vec4 projection;
uniform int renderingPass;

#define WIDE_CHAR 2

void main() {
    vec2 projectionOffset = projection.xy;
    vec2 projectionScale = projection.zw;

    // Compute vertex corner position
    vec2 position;
    position.x = (gl_VertexID == 0 || gl_VertexID == 1) ? 1. : 0.;
    position.y = (gl_VertexID == 0 || gl_VertexID == 3) ? 0. : 1.;

    // Position of cell from top-left
    vec2 cellPosition = cellDim * gridCoords;

    fg = vec4(textColor.rgb / 255.0, textColor.a);
    bg = backgroundColor / 255.0;

    float occupiedCells = 1;
    if ((int(fg.a) >= WIDE_CHAR)) {
        // Update wide char x dimension so it'll cover the following spacer.
        occupiedCells = 2;

        // Subtract wide char bits keeping only colored flag.
        fg.a = round(fg.a - WIDE_CHAR);
    }

    if (renderingPass == 0) {
        // Background pass
        vec2 backgroundDim = cellDim;
        backgroundDim.x *= occupiedCells;

        vec2 finalPosition = cellPosition + backgroundDim * position;
        gl_Position = vec4(projectionOffset + projectionScale * finalPosition, 0.0, 1.0);

        TexCoords = vec2(0, 0);
    } else {
        // Text pass
        vec2 glyphSize = glyph.zw;
        vec2 glyphOffset = glyph.xy;
        glyphOffset.y = cellDim.y - glyphOffset.y;

        vec2 finalPosition = cellPosition + glyphSize * position + glyphOffset;
        gl_Position = vec4(projectionOffset + projectionScale * finalPosition, 0.0, 1.0);

        vec2 uvOffset = uv.xy;
        vec2 uvSize = uv.zw;
        TexCoords = uvOffset + position * uvSize;
    }
}
```

### Key Techniques

#### 1. gl_VertexID for Quad Generation

```glsl
// Instead of passing 4 vertex positions, compute from ID
vec2 position;
position.x = (gl_VertexID == 0 || gl_VertexID == 1) ? 1. : 0.;
position.y = (gl_VertexID == 0 || gl_VertexID == 3) ? 0. : 1.;

// Quad layout:
// 0: (1, 0) - bottom-left
// 1: (1, 1) - top-left
// 2: (0, 1) - top-right
// 3: (0, 0) - bottom-right
```

**Benefit:** No vertex position attribute needed, saves bandwidth

**Element buffer (shared across all instances):**
```rust
let indices: [u32; 6] = [0, 1, 3, 1, 2, 3];
```

#### 2. Dual-Purpose Shader (Background + Text)

```glsl
uniform int renderingPass;

if (renderingPass == 0) {
    // Background pass: Stretch cell background
    vec2 backgroundDim = cellDim;
    backgroundDim.x *= occupiedCells;  // Double width for wide chars
    vec2 finalPosition = cellPosition + backgroundDim * position;
} else {
    // Text pass: Position glyph with offset
    vec2 glyphSize = glyph.zw;
    vec2 glyphOffset = glyph.xy;
    vec2 finalPosition = cellPosition + glyphSize * position + glyphOffset;
}
```

**Benefit:** Single shader handles both passes, reduces PSO switches

#### 3. Wide Character Handling

```glsl
float occupiedCells = 1;
if ((int(fg.a) >= WIDE_CHAR)) {
    occupiedCells = 2;  // CJK characters occupy 2 cells
    fg.a = round(fg.a - WIDE_CHAR);  // Remove flag, keep colored flag
}
```

**Encoding:** Flags are packed in alpha channel
- Bit 0: Colored flag (0x01)
- Bit 1: Wide char flag (0x02)

#### 4. Projection Transform in Vertex Shader

```glsl
uniform vec4 projection;  // (offsetX, offsetY, scaleX, scaleY)

// Convert from pixel space to NDC [-1, 1]
gl_Position = vec4(projectionOffset + projectionScale * finalPosition, 0.0, 1.0);
```

**CPU-side calculation:**
```rust
let scale_x = 2. / (width - 2. * padding_x);
let scale_y = -2. / (height - 2. * padding_y);  // Negative for Y-down
let offset_x = -1.;
let offset_y = 1.;

gl::Uniform4f(u_projection, offset_x, offset_y, scale_x, scale_y);
```

**Benefit:** No matrix multiply needed, just MAD (multiply-add)

## GLSL 3.3 Fragment Shader

### Full Source

```glsl
#version 330 core

#if defined(GLES2_RENDERER)
#extension GL_EXT_blend_func_extended: require
// ... GLES2 definitions ...
#else

in vec2 TexCoords;
flat in vec4 fg;
flat in vec4 bg;

layout(location = 0, index = 0) out vec4 color;
layout(location = 0, index = 1) out vec4 alphaMask;

#define FRAG_COLOR color
#define ALPHA_MASK alphaMask
#endif

#define COLORED 1

uniform int renderingPass;
uniform sampler2D mask;

void main() {
    if (renderingPass == 0) {
        // Background pass
        if (bg.a == 0.0) {
            discard;
        }

        ALPHA_MASK = vec4(1.0);
        // Premultiply background color by alpha
        FRAG_COLOR = vec4(bg.rgb * bg.a, bg.a);
        return;
    }

    float colored = fg.a;

    if (int(colored) == COLORED) {
        // Color glyphs (emojis)
        FRAG_COLOR = texture(mask, TexCoords);
        ALPHA_MASK = vec4(FRAG_COLOR.a);

        // Revert alpha premultiplication
        if (FRAG_COLOR.a != 0.0) {
            FRAG_COLOR.rgb = vec3(FRAG_COLOR.rgb / FRAG_COLOR.a);
        }

        FRAG_COLOR = vec4(FRAG_COLOR.rgb, 1.0);
    } else {
        // Regular text glyphs (subpixel rendering)
        vec3 textColor = texture(mask, TexCoords).rgb;
        ALPHA_MASK = vec4(textColor, textColor.r);
        FRAG_COLOR = vec4(fg.rgb, 1.0);
    }
}
```

### Key Techniques

#### 1. Dual-Source Blending

```glsl
layout(location = 0, index = 0) out vec4 color;
layout(location = 0, index = 1) out vec4 alphaMask;
```

**OpenGL blending setup:**
```rust
gl::BlendFunc(gl::SRC1_COLOR, gl::ONE_MINUS_SRC1_COLOR);
```

**Blending formula:**
```
final = color * alphaMask + dst * (1 - alphaMask)
```

**Why dual-source?**
- Subpixel text rendering in single pass
- No read-modify-write cycles
- Proper gamma-correct blending

#### 2. Subpixel Rendering

```glsl
// For monochrome glyphs
vec3 textColor = texture(mask, TexCoords).rgb;
ALPHA_MASK = vec4(textColor, textColor.r);
FRAG_COLOR = vec4(fg.rgb, 1.0);

// Blending:
// final.rgb = fg.rgb * textColor.rgb + dst.rgb * (1 - textColor.rgb)
// final.a   = 1.0 * textColor.r + dst.a * (1 - textColor.r)
```

**RGB subpixel components:**
- Red channel: Left subpixel
- Green channel: Center subpixel
- Blue channel: Right subpixel

**Result:** Sharper text on LCD displays

#### 3. Colored Emoji Support

```glsl
if (int(colored) == COLORED) {
    FRAG_COLOR = texture(mask, TexCoords);
    ALPHA_MASK = vec4(FRAG_COLOR.a);

    // Revert premultiplied alpha from texture
    if (FRAG_COLOR.a != 0.0) {
        FRAG_COLOR.rgb = vec3(FRAG_COLOR.rgb / FRAG_COLOR.a);
    }

    FRAG_COLOR = vec4(FRAG_COLOR.rgb, 1.0);
}
```

**Why revert premultiplication?**
- Font rasterizer outputs premultiplied RGBA
- Dual-source blending expects non-premultiplied
- Divide by alpha to revert, then multiply in blending

#### 4. Background Pass Optimization

```glsl
if (renderingPass == 0) {
    if (bg.a == 0.0) {
        discard;  // Skip transparent backgrounds
    }

    ALPHA_MASK = vec4(1.0);
    FRAG_COLOR = vec4(bg.rgb * bg.a, bg.a);  // Premultiply
    return;
}
```

**Early discard:** Saves fragment processing for transparent cells

**Premultiplication:** Correct alpha blending for semi-transparent backgrounds

## GLES 2.0 Fallback (Without Dual-Source Blending)

### Fragment Shader (3-Pass)

```glsl
#version 100

varying mediump vec2 TexCoords;
varying mediump vec3 fg;
varying highp float colored;
varying mediump vec4 bg;

uniform highp int renderingPass;
uniform sampler2D mask;

#define COLORED 1

mediump float max_rgb(mediump vec3 mask) {
    return max(max(mask.r, mask.g), mask.b);
}

void render_text() {
    mediump vec4 mask = texture2D(mask, TexCoords);
    mediump float m_rgb = max_rgb(mask.rgb);

    if (renderingPass == 1) {
        // Pass 1: Output mask
        gl_FragColor = vec4(mask.rgb, m_rgb);
    } else if (renderingPass == 2) {
        // Pass 2: Background * (1 - mask)
        gl_FragColor = bg * (vec4(m_rgb) - vec4(mask.rgb, m_rgb));
    } else {
        // Pass 3: Foreground * mask
        gl_FragColor = vec4(fg, 1.) * vec4(mask.rgb, m_rgb);
    }
}

void render_bitmap() {
    if (renderingPass == 2) {
        discard;  // Skip background pass
    }
    mediump vec4 mask = texture2D(mask, TexCoords);
    if (renderingPass == 1) {
        gl_FragColor = mask.aaaa;  // Alpha only
    } else {
        gl_FragColor = mask;  // Full color
    }
}

void main() {
    if (renderingPass == 0) {
        // Background pass
        if (bg.a == 0.0) {
            discard;
        }
        gl_FragColor = vec4(bg.rgb * bg.a, bg.a);
        return;
    }

    if (int(colored) == COLORED) {
        render_bitmap();
    } else {
        render_text();
    }
}
```

### Blending Setup (3-Pass)

```rust
// Pass 1: Subtract text from background
gl::BlendFuncSeparate(gl::ZERO, gl::ONE_MINUS_SRC_COLOR, gl::ZERO, gl::ONE);
gl::DrawElements(...);

// Pass 2: Add inverted alpha
gl::BlendFuncSeparate(gl::ONE_MINUS_DST_ALPHA, gl::ONE, gl::ZERO, gl::ONE);
gl::DrawElements(...);

// Pass 3: Blend text color
gl::BlendFuncSeparate(gl::ONE, gl::ONE, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
gl::DrawElements(...);
```

**Based on WebRender's text rendering algorithm:**
https://github.com/servo/webrender/blob/master/webrender/doc/text-rendering.md

## Rectangle Shader (Underlines, Cursor)

### Vertex Shader

```glsl
#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;

out vec4 color;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    color = aColor;
}
```

**Simple pass-through** - rectangles are already in NDC space

### Fragment Shader (Undercurl)

```glsl
#version 330 core

#if defined(DRAW_UNDERCURL)

uniform float cellWidth;
uniform float cellHeight;
uniform float undercurlPosition;

in vec4 color;
out vec4 fragColor;

void main() {
    // Generate sine wave for undercurl
    float x = gl_FragCoord.x;
    float y = gl_FragCoord.y;

    float wave = sin(x / cellWidth * 3.14159 * 2.0) * (cellHeight / 4.0);
    float baseline = undercurlPosition;

    if (abs(y - baseline - wave) > 1.0) {
        discard;
    }

    fragColor = color;
}

#elif defined(DRAW_DOTTED)

uniform float cellWidth;
uniform float paddingX;

in vec4 color;
out vec4 fragColor;

void main() {
    float x = gl_FragCoord.x - paddingX;
    float cellFraction = mod(x, cellWidth) / cellWidth;

    // Create dots
    if (cellFraction > 0.5) {
        discard;
    }

    fragColor = color;
}

#else

// Normal rectangle
in vec4 color;
out vec4 fragColor;

void main() {
    fragColor = color;
}

#endif
```

**Procedural generation:** Undercurl/dotted patterns generated in shader

## Shader Compilation

### Shader Creation

```rust
pub fn new(
    shader_version: ShaderVersion,
    shader_header: Option<&str>,
    vertex_shader: &'static str,
    fragment_shader: &'static str,
) -> Result<Self, ShaderError> {
    let vertex_shader =
        Shader::new(shader_version, shader_header, gl::VERTEX_SHADER, vertex_shader)?;
    let fragment_shader =
        Shader::new(shader_version, shader_header, gl::FRAGMENT_SHADER, fragment_shader)?;

    let program = unsafe { Self(gl::CreateProgram()) };

    unsafe {
        gl::AttachShader(program.id(), vertex_shader.id());
        gl::AttachShader(program.id(), fragment_shader.id());
        gl::LinkProgram(program.id());

        let mut success: GLint = 0;
        gl::GetProgramiv(program.id(), gl::LINK_STATUS, &mut success);

        if success != i32::from(gl::TRUE) {
            return Err(ShaderError::Link(get_program_info_log(program.id())));
        }
    }

    Ok(program)
}
```

### Version Header Injection

```rust
fn shader_header(&self) -> &'static str {
    match self {
        Self::Glsl3 => "#version 330 core\n",
        Self::Gles2 => "#version 100\n#define GLES2_RENDERER\n",
    }
}

// Shader source concatenation:
// 1. Version header
// 2. Optional custom header (e.g., "#define DRAW_UNDERCURL\n")
// 3. Main shader source
```

## Transferable Concepts for D3D12

### 1. Vertex Shader with SV_VertexID

```hlsl
struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    float4 bgColor : COLOR1;
};

cbuffer Constants : register(b0) {
    float2 cellDim;
    float4 projection;
    uint renderingPass;
};

StructuredBuffer<InstanceData> instances : register(t0);

VSOutput main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID) {
    InstanceData inst = instances[instanceID];

    // Compute quad corner from vertexID
    float2 corner;
    corner.x = (vertexID == 0 || vertexID == 1) ? 1.0 : 0.0;
    corner.y = (vertexID == 0 || vertexID == 3) ? 0.0 : 1.0;

    float2 cellPos = float2(inst.col, inst.row) * cellDim;

    VSOutput output;

    if (renderingPass == 0) {
        // Background pass
        float occupiedCells = (inst.flags & 0x02) ? 2.0 : 1.0;
        float2 bgDim = cellDim;
        bgDim.x *= occupiedCells;

        float2 pos = cellPos + bgDim * corner;
        output.position = float4(projection.xy + projection.zw * pos, 0.0, 1.0);
        output.texCoord = float2(0, 0);
    } else {
        // Text pass
        float2 glyphSize = float2(inst.width, inst.height);
        float2 glyphOffset = float2(inst.left, cellDim.y - inst.top);

        float2 pos = cellPos + glyphSize * corner + glyphOffset;
        output.position = float4(projection.xy + projection.zw * pos, 0.0, 1.0);
        output.texCoord = inst.uvRect.xy + corner * inst.uvRect.zw;
    }

    output.color = UnpackColor(inst.fgColor);
    output.bgColor = UnpackColor(inst.bgColor);

    return output;
}
```

### 2. Dual-Source Blending (Pixel Shader)

```hlsl
Texture2D glyphTexture : register(t0);
SamplerState linearSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    float4 bgColor : COLOR1;
};

struct PSOutput {
    float4 color : SV_Target0;
    float4 alphaMask : SV_Target1;  // Dual-source blend
};

cbuffer Constants : register(b0) {
    uint renderingPass;
};

PSOutput main(PSInput input) {
    PSOutput output;

    if (renderingPass == 0) {
        // Background pass
        if (input.bgColor.a == 0.0) {
            discard;
        }
        output.color = float4(input.bgColor.rgb * input.bgColor.a, input.bgColor.a);
        output.alphaMask = float4(1, 1, 1, 1);
        return output;
    }

    bool isColored = (input.color.a >= 0.5);

    if (isColored) {
        // Colored emoji
        float4 texColor = glyphTexture.Sample(linearSampler, input.texCoord);
        output.alphaMask = texColor.aaaa;

        // Revert premultiplication
        if (texColor.a > 0.0) {
            texColor.rgb /= texColor.a;
        }
        output.color = float4(texColor.rgb, 1.0);
    } else {
        // Subpixel text
        float3 mask = glyphTexture.Sample(linearSampler, input.texCoord).rgb;
        output.alphaMask = float4(mask, mask.r);
        output.color = float4(input.color.rgb, 1.0);
    }

    return output;
}
```

### 3. Blend State Setup (D3D12)

```cpp
D3D12_BLEND_DESC blendDesc = {};
blendDesc.RenderTarget[0].BlendEnable = TRUE;
blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC1_COLOR;
blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC1_COLOR;
blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC1_ALPHA;
blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC1_ALPHA;
blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
```

## Performance Analysis

### Shader Complexity

| Shader | ALU Ops | Texture Samples | Branches |
|--------|---------|-----------------|----------|
| GLSL3 VS | ~20 | 0 | 1 (if renderingPass) |
| GLSL3 FS | ~15 | 1 | 2 (if bg.a, if colored) |
| GLES2 VS | ~15 | 0 | 1 |
| GLES2 FS | ~25 | 1 | 3 |

**Very lightweight** - Modern GPUs can handle thousands of these per frame

### Optimization Notes

1. **Flat interpolation:** `flat out vec4 fg` - no interpolation needed, saves GPU cycles
2. **Early discard:** Background pass discards transparent fragments early
3. **No branching in inner loops:** Branches are uniform across instances
4. **Minimal texture samples:** One sample per fragment
5. **No complex math:** Just MAD operations, no trig or exp

## Conclusion

Alacritty's shaders achieve high performance through:

1. **Instanced rendering** with SV_VertexID/gl_VertexID
2. **Dual-source blending** for single-pass subpixel rendering
3. **Dual-purpose shaders** to minimize PSO switches
4. **Efficient data packing** (flags in alpha channel)
5. **Early fragment discard** for transparent backgrounds

All techniques translate directly to D3D12/HLSL with minimal changes.

**Key insight:** Keep shaders simple. Let batching and instancing do the heavy lifting.
