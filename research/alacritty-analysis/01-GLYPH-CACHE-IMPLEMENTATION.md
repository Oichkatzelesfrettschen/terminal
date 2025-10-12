# Alacritty Glyph Cache Implementation - Deep Dive

## Overview

The glyph cache is the heart of Alacritty's text rendering performance. It provides O(1) lookup for rasterized glyphs and manages texture atlas allocation.

## Source Files

- `/alacritty/src/renderer/text/glyph_cache.rs` - Main cache logic
- `/alacritty/src/renderer/text/atlas.rs` - Texture atlas management
- `/alacritty/src/renderer/text/mod.rs` - Renderer integration

## Data Structures

### Glyph Cache Structure

```rust
pub struct GlyphCache {
    // HashMap with fast ahash (non-cryptographic)
    cache: HashMap<GlyphKey, Glyph, RandomState>,

    // Font rasterizer (crossfont library)
    rasterizer: Rasterizer,

    // Font keys for different styles
    pub font_key: FontKey,
    pub bold_key: FontKey,
    pub italic_key: FontKey,
    pub bold_italic_key: FontKey,

    // Font size
    pub font_size: crossfont::Size,

    // Offsets for positioning
    font_offset: Delta<i8>,
    glyph_offset: Delta<i8>,

    // Font metrics
    metrics: Metrics,

    // Whether to use built-in font for box drawing
    builtin_box_drawing: bool,
}
```

### Glyph Key (Cache Key)

```rust
// From crossfont library
pub struct GlyphKey {
    pub font_key: FontKey,    // Which font face
    pub character: char,       // Unicode character
    pub size: Size,            // Font size
}
```

**Hash Implementation:**
- Uses `ahash::RandomState` (fast, non-cryptographic hash)
- Typical lookup: O(1) with very low collision rate
- Key is Copy, so no allocations on lookup

### Glyph (Cache Value)

```rust
#[derive(Copy, Clone, Debug)]
pub struct Glyph {
    pub tex_id: GLuint,        // Which texture atlas
    pub multicolor: bool,       // Colored emoji vs. monochrome
    pub top: i16,               // Glyph bearing Y
    pub left: i16,              // Glyph bearing X
    pub width: i16,             // Glyph width in pixels
    pub height: i16,            // Glyph height in pixels
    pub uv_bot: f32,            // UV bottom coordinate
    pub uv_left: f32,           // UV left coordinate
    pub uv_width: f32,          // UV width
    pub uv_height: f32,         // UV height
}
```

**Size:** 32 bytes (fits in single cache line on most architectures)

## Caching Algorithm

### Lookup Flow

```rust
pub fn get<L>(&mut self, glyph_key: GlyphKey, loader: &mut L, show_missing: bool) -> Glyph
where
    L: LoadGlyph + ?Sized,
{
    // 1. Try cache first (O(1) hash lookup)
    if let Some(glyph) = self.cache.get(&glyph_key) {
        return *glyph;  // Copy, no heap allocation
    };

    // 2. Rasterize the glyph (CPU work)
    let rasterized = self
        .builtin_box_drawing
        .then(|| builtin_font::builtin_glyph(...))
        .flatten()
        .map_or_else(|| self.rasterizer.get_glyph(glyph_key), Ok);

    // 3. Load into texture atlas (GPU upload)
    let glyph = match rasterized {
        Ok(rasterized) => self.load_glyph(loader, rasterized),
        Err(RasterizerError::MissingGlyph(rasterized)) if show_missing => {
            // Use '\0' as "missing" glyph (cache once)
            let missing_key = GlyphKey { character: '\0', ..glyph_key };
            if let Some(glyph) = self.cache.get(&missing_key) {
                *glyph
            } else {
                let glyph = self.load_glyph(loader, rasterized);
                self.cache.insert(missing_key, glyph);
                glyph
            }
        },
        Err(_) => self.load_glyph(loader, Default::default()),
    };

    // 4. Cache the result
    *self.cache.entry(glyph_key).or_insert(glyph)
}
```

### Pre-Caching Strategy

```rust
fn load_glyphs_for_font<L: LoadGlyph>(&mut self, font: FontKey, loader: &mut L) {
    let size = self.font_size;

    // Cache all ASCII printable characters (32-126)
    for i in 32u8..=126u8 {
        self.get(
            GlyphKey { font_key: font, character: i as char, size },
            loader,
            true
        );
    }
}

pub fn load_common_glyphs<L: LoadGlyph>(&mut self, loader: &mut L) {
    // Pre-cache for all font variants
    self.load_glyphs_for_font(self.font_key, loader);       // Regular
    self.load_glyphs_for_font(self.bold_key, loader);       // Bold
    self.load_glyphs_for_font(self.italic_key, loader);     // Italic
    self.load_glyphs_for_font(self.bold_italic_key, loader); // Bold+Italic
}
```

**Pre-caching Benefits:**
- Startup: Load 95 characters x 4 fonts = 380 glyphs
- Typical terminal: 90%+ of characters are ASCII
- First frame: No rasterization or uploads
- Amortized cost: Spread across startup

## Missing Glyph Handling

**Smart Caching:**
```rust
// Instead of caching each missing character separately:
// BAD:  cache['X'] = missing, cache['Y'] = missing, cache['Z'] = missing
// GOOD: cache['\0'] = missing, all missing chars return this

let missing_key = GlyphKey { character: '\0', ..glyph_key };
if let Some(glyph) = self.cache.get(&missing_key) {
    *glyph  // Return cached missing glyph
} else {
    let glyph = self.load_glyph(loader, rasterized);
    self.cache.insert(missing_key, glyph);  // Cache once
    glyph
}
```

**Benefit:** Reduces memory usage and cache pollution

## Font Variant Handling

```rust
// Font selection based on cell flags
let font_key = match cell.flags & Flags::BOLD_ITALIC {
    Flags::BOLD_ITALIC => glyph_cache.bold_italic_key,
    Flags::ITALIC => glyph_cache.italic_key,
    Flags::BOLD => glyph_cache.bold_key,
    _ => glyph_cache.font_key,
};

let mut glyph_key = GlyphKey { font_key, size: glyph_cache.font_size, character: cell.character };
```

**Optimization:** Bitflag-based selection (no branches in common case)

## Zero-Width Character Handling

```rust
// Render visible zero-width characters (combining marks, etc.)
if let Some(zerowidth) = cell.extra.as_mut().and_then(|extra| extra.zerowidth.take().filter(|_| !hidden)) {
    for character in zerowidth {
        glyph_key.character = character;
        let glyph = glyph_cache.get(glyph_key, self, false);
        self.add_render_item(&cell, &glyph, size_info);
    }
}
```

**Positioning Logic:**
```rust
// Zero-width characters are rendered at the right edge of the preceding cell
if glyph.character.width() == Some(0) {
    glyph.left += self.metrics.average_advance as i32;
}
```

## Performance Characteristics

### Time Complexity

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Cache lookup | O(1) | ahash, expected case |
| Cache insert | O(1) | amortized |
| Rasterization | O(n) | n = pixels in glyph |
| Atlas upload | O(n) | GPU DMA transfer |
| Pre-caching | O(m) | m = number of chars (380) |

### Space Complexity

| Structure | Size | Count | Total |
|-----------|------|-------|-------|
| GlyphKey | 16 bytes | N cached | 16N |
| Glyph | 32 bytes | N cached | 32N |
| HashMap overhead | ~24 bytes | N cached | 24N |
| **Total per glyph** | **~72 bytes** | - | - |
| **Typical cache** | - | ~1,000 glyphs | ~72 KB |
| **Large cache** | - | ~10,000 glyphs | ~720 KB |

**Note:** Texture atlas memory is separate (4MB per 1024x1024 RGBA atlas)

## Cache Invalidation

### Full Reset

```rust
pub fn reset_glyph_cache<L: LoadGlyph>(&mut self, loader: &mut L) {
    loader.clear();              // Clear all atlases
    self.cache = Default::default();  // Clear hash map
    self.load_common_glyphs(loader);   // Re-cache ASCII
}
```

**Triggered by:**
- Font size change
- Font family change
- DPI change

### Partial Invalidation

**Not supported.** Alacritty uses full cache reset because:
1. Font changes are rare
2. Full reset is fast (~10ms for 380 glyphs)
3. Simplifies code (no complex invalidation logic)

## Built-in Font for Box Drawing

```rust
// For box-drawing characters (U+2500-U+257F), use built-in vector font
// Avoids font-specific rendering issues and ensures consistency
let rasterized = self
    .builtin_box_drawing
    .then(|| {
        builtin_font::builtin_glyph(
            glyph_key.character,
            &self.metrics,
            &self.font_offset,
            &self.glyph_offset,
        )
    })
    .flatten()
    .map_or_else(|| self.rasterizer.get_glyph(glyph_key), Ok);
```

**Benefits:**
- Pixel-perfect box drawing at any size
- No dependency on font containing these characters
- Consistent across all fonts

## Integration with Renderer

### Loader API

```rust
pub trait LoadGlyph {
    fn load_glyph(&mut self, rasterized: &RasterizedGlyph) -> Glyph;
    fn clear(&mut self);
}

pub struct LoaderApi<'a> {
    active_tex: &'a mut GLuint,
    atlas: &'a mut Vec<Atlas>,
    current_atlas: &'a mut usize,
}
```

**Abstraction:** Cache doesn't know about OpenGL - renderer provides loading interface

### Rendering Integration

```rust
// In text renderer
fn draw_cell(&mut self, mut cell: RenderableCell, glyph_cache: &mut GlyphCache, size_info: &SizeInfo) {
    // 1. Select font
    let font_key = match cell.flags & Flags::BOLD_ITALIC {
        Flags::BOLD_ITALIC => glyph_cache.bold_italic_key,
        Flags::ITALIC => glyph_cache.italic_key,
        Flags::BOLD => glyph_cache.bold_key,
        _ => glyph_cache.font_key,
    };

    // 2. Get glyph (cache hit or load)
    let mut glyph_key = GlyphKey { font_key, size: glyph_cache.font_size, character: cell.character };
    let glyph = glyph_cache.get(glyph_key, self, true);

    // 3. Add to batch
    self.add_render_item(&cell, &glyph, size_info);
}
```

## Transferable Concepts for Windows Terminal

### 1. Hash-Based Glyph Cache

```cpp
// D3D12 equivalent
struct GlyphCacheKey {
    uint64_t fontFace;      // Font face identifier
    uint32_t character;     // Unicode codepoint
    uint32_t size;          // Font size in pixels

    // Hash function for unordered_map
    struct Hash {
        size_t operator()(const GlyphCacheKey& k) const {
            // Use xxHash or similar fast hash
            return XXH64(&k, sizeof(k), 0);
        }
    };
};

struct GlyphCacheValue {
    uint32_t atlasIndex;    // Which atlas texture
    uint16_t x, y;          // Pixel coordinates in atlas
    uint16_t width, height; // Glyph dimensions
    int16_t left, top;      // Bearing
    float uvLeft, uvTop;    // Normalized UV coords
    float uvWidth, uvHeight;
    bool isColored;         // RGBA vs. grayscale
};

std::unordered_map<GlyphCacheKey, GlyphCacheValue, GlyphCacheKey::Hash> glyphCache;
```

### 2. Pre-Caching Strategy

```cpp
void PreCacheCommonGlyphs() {
    // ASCII printable characters
    for (uint32_t c = 32; c <= 126; ++c) {
        CacheGlyph(regularFont, c);
        CacheGlyph(boldFont, c);
        CacheGlyph(italicFont, c);
        CacheGlyph(boldItalicFont, c);
    }
}
```

### 3. Missing Glyph Caching

```cpp
GlyphCacheValue GetGlyph(uint64_t fontFace, uint32_t character, uint32_t size) {
    GlyphCacheKey key{fontFace, character, size};

    auto it = glyphCache.find(key);
    if (it != glyphCache.end()) {
        return it->second;
    }

    // Rasterize glyph
    auto rasterized = RasterizeGlyph(fontFace, character, size);

    if (!rasterized.has_value()) {
        // Cache missing glyph as U+0000
        GlyphCacheKey missingKey{fontFace, 0, size};
        auto missingIt = glyphCache.find(missingKey);

        if (missingIt != glyphCache.end()) {
            return missingIt->second;
        }

        // Create missing glyph indicator
        auto missingGlyph = CreateMissingGlyph();
        glyphCache[missingKey] = missingGlyph;
        return missingGlyph;
    }

    // Upload to atlas and cache
    auto glyph = UploadToAtlas(rasterized.value());
    glyphCache[key] = glyph;
    return glyph;
}
```

### 4. Font Variant Selection

```cpp
enum class FontStyle {
    Regular = 0,
    Bold = 1,
    Italic = 2,
    BoldItalic = 3
};

uint64_t SelectFont(FontStyle style) {
    static const uint64_t fonts[] = {
        regularFontHandle,
        boldFontHandle,
        italicFontHandle,
        boldItalicFontHandle
    };
    return fonts[static_cast<size_t>(style)];
}

// Usage
FontStyle style = FontStyle::Regular;
if (cell.flags & CellFlags::Bold) style = static_cast<FontStyle>(static_cast<int>(style) | 1);
if (cell.flags & CellFlags::Italic) style = static_cast<FontStyle>(static_cast<int>(style) | 2);

uint64_t fontFace = SelectFont(style);
```

## Optimization Notes

### Why ahash?

- **Fast:** 2-3x faster than SipHash (Rust default)
- **Good quality:** Low collision rate for hash table use
- **Not cryptographic:** Don't need crypto properties for cache keys
- **DoS resistant:** Uses random seed per HashMap instance

### Why Copy for Glyph?

- **Small:** 32 bytes fits in single cache line
- **No heap:** Avoids Arc/Rc overhead
- **Fast:** Memcpy is cheap for small sizes
- **Simple:** No lifetime management

### Why No LRU Eviction?

- **Memory is cheap:** 1,000 glyphs = 72KB
- **Cache hit rate is high:** ASCII reuse is common
- **Eviction overhead:** Adds complexity and CPU cost
- **Full reset is fast:** When needed (font change)

## Conclusion

Alacritty's glyph cache achieves high performance through:

1. **Fast hash table** with O(1) lookups
2. **Pre-caching** of common characters
3. **Smart missing glyph handling** to reduce cache size
4. **Copy semantics** for small, cache-friendly values
5. **Simple invalidation** (full reset only)

All of these concepts translate directly to D3D12 with minimal changes.
