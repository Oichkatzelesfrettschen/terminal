# Font Rendering Optimization for Nerd Fonts

**Research Date:** 2025-10-11
**Focus:** Glyph Atlas Strategies, DirectWrite Integration, Performance Optimization

---

## Overview

Rendering Nerd Fonts efficiently requires careful management of glyph atlases, caching strategies, and memory usage. With 3,000+ additional icon glyphs, standard text rendering approaches must be optimized to prevent performance degradation and memory exhaustion.

---

## Glyph Atlas Architecture

### What is a Glyph Atlas?

A **glyph atlas** (also called texture atlas or glyph cache) is a GPU texture containing rasterized bitmaps of glyphs. Instead of re-rasterizing glyphs on every frame, the renderer:

1. **Rasterizes glyph once** (CPU-intensive)
2. **Stores bitmap in GPU texture** (atlas)
3. **Copies from atlas to framebuffer** (GPU-fast)

### Benefits
- **Performance**: GPU texture copy is 10-100x faster than CPU rasterization
- **Consistency**: Same glyph always looks identical
- **Efficiency**: Reduces CPU load and DirectWrite API calls

### Challenges with Nerd Fonts
- **3,000+ additional glyphs** increase atlas memory requirements
- **Sparse usage**: Most icons used rarely, but must be ready on-demand
- **Memory limits**: GPU texture atlases have size constraints

---

## Windows Terminal's AtlasEngine

### Architecture Overview

Microsoft Terminal's **AtlasEngine** is a DirectWrite + Direct3D-based renderer that uses texture atlases for glyph caching.

**Key Components:**
1. **DirectWrite**: Rasterizes glyphs to bitmaps
2. **Direct2D**: Optional glyph rendering path
3. **Direct3D 11**: GPU texture management and compositing
4. **HLSL Shaders**: Blending and special effects

### Atlas Memory Management

**Current Limitations (as of 2024-2025):**
- **Maximum Atlas Size**: 256 MB
- **Approximate Glyph Capacity**: ~20,000 glyphs
- **Growth Strategy**: Grow-only (no shrinking)
- **Overflow Behavior**: Text rendering breaks after limit exceeded

**Calculation:**
```
Atlas Size = Glyph Count * (Glyph Width * Glyph Height * Bytes Per Pixel + Overhead)

Example (12pt font, 1.5x scaling):
- Glyph Size: ~24x36 pixels
- Bytes Per Pixel: 4 (RGBA)
- Per-Glyph Size: 24 * 36 * 4 = 3,456 bytes = 3.4 KB
- 20,000 glyphs: 3.4 KB * 20,000 = 68 MB (actual: ~256 MB due to padding/overhead)
```

### Performance Characteristics

**CPU Usage:**
- AtlasEngine uses **~50% less CPU** than legacy DxEngine
- **Glyph hashing consumes ~33%** of remaining CPU time
- Bottleneck shifts from rendering to cache key computation

**GPU Usage:**
- Minimal GPU load for cached glyphs (texture copy)
- First render of new glyph triggers CPU rasterization

---

## Glyph Caching Strategies

### 1. Lazy Cache Fill (Recommended)

**Strategy:** Only rasterize glyphs when they appear on screen.

**Implementation:**
```cpp
struct GlyphCacheKey {
    uint32_t fontID;
    uint32_t glyphID;
    uint16_t fontSize;
    uint16_t dpi;
    uint8_t  weight;
    uint8_t  style;
};

bool GlyphCache::TryGetGlyph(const GlyphCacheKey& key, GlyphAtlasEntry& entry) {
    auto it = cache.find(key);
    if (it != cache.end()) {
        entry = it->second;
        return true;  // Cache hit
    }

    // Cache miss: rasterize and insert
    entry = RasterizeGlyph(key);
    cache[key] = entry;
    return false;
}
```

**Pros:**
- Minimal memory usage (only used glyphs cached)
- Fast startup time

**Cons:**
- First render of new glyph has latency spike
- Cache misses on rapid scrolling

---

### 2. Preload Common Glyphs

**Strategy:** Preload ASCII + frequently used icons on startup.

**Implementation:**
```cpp
void GlyphCache::PreloadCommonGlyphs() {
    // ASCII printable characters (32-126)
    for (uint32_t codepoint = 32; codepoint <= 126; ++codepoint) {
        PreloadGlyph(codepoint);
    }

    // Common Nerd Font icons
    PreloadGlyph(0xE0B0);  // Powerline separator
    PreloadGlyph(0xE0A0);  // Branch
    PreloadGlyph(0xE5FA);  // Folder icon
    PreloadGlyph(0xF07B);  // Folder (Font Awesome)
    PreloadGlyph(0xF15B);  // File icon
    // ... add more based on telemetry
}
```

**Pros:**
- Smooth rendering for common characters
- Predictable memory usage

**Cons:**
- Longer startup time
- May preload unused glyphs

---

### 3. LRU (Least Recently Used) Eviction

**Strategy:** Evict least recently used glyphs when atlas fills.

**Implementation:**
```cpp
class LRUGlyphCache {
    std::unordered_map<GlyphCacheKey, AtlasEntry> cache;
    std::list<GlyphCacheKey> lruList;
    size_t maxCacheSize = 10000;  // ~20 MB for typical font

    void TouchGlyph(const GlyphCacheKey& key) {
        lruList.remove(key);
        lruList.push_front(key);  // Move to front (most recent)
    }

    void EvictOldestGlyph() {
        if (cache.size() >= maxCacheSize) {
            GlyphCacheKey oldest = lruList.back();
            lruList.pop_back();
            cache.erase(oldest);
        }
    }
};
```

**Pros:**
- Bounded memory usage
- Automatic adaptation to usage patterns

**Cons:**
- Complexity (LRU list maintenance)
- Potential re-rasterization of evicted glyphs

---

### 4. Word-Based Shaping Cache

**Strategy:** Cache text shaping results by word boundaries.

**Implementation:**
```cpp
struct ShapingCacheEntry {
    std::vector<uint16_t> glyphIndices;
    std::vector<float> glyphPositions;
};

std::unordered_map<std::wstring, ShapingCacheEntry> shapingCache;

ShapingCacheEntry ShapeText(const std::wstring& word) {
    auto it = shapingCache.find(word);
    if (it != shapingCache.end()) {
        return it->second;  // Cache hit
    }

    // Cache miss: perform DirectWrite text shaping
    ShapingCacheEntry entry = PerformTextShaping(word);
    shapingCache[word] = entry;
    return entry;
}
```

**Pros:**
- Reduces DirectWrite API calls (expensive)
- Effective for repetitive text (code keywords, variable names)

**Cons:**
- Memory usage scales with unique word count
- Less effective for natural language text

---

## Atlas Texture Management

### Texture Layout Strategies

#### 1. Single Large Texture (Simple)

**Layout:**
```
+----------------------------------+
| Glyph 1 | Glyph 2 | Glyph 3 | ...
+----------------------------------+
| Glyph 4 | Glyph 5 | Glyph 6 | ...
+----------------------------------+
```

**Pros:**
- Simple implementation
- Single GPU bind per frame

**Cons:**
- Fixed maximum size (texture size limit)
- Cannot shrink or reorganize

---

#### 2. Multiple Smaller Textures (Windows Terminal Approach)

**Layout:**
```
Atlas 0 (256x256):  [ASCII + Common]
Atlas 1 (256x256):  [Extended Latin + Icons Set 1]
Atlas 2 (256x256):  [Icons Set 2 + 3]
...
```

**Pros:**
- Scalable (add new textures as needed)
- Can unload unused atlases

**Cons:**
- Multiple GPU binds per frame
- More complex glyph location tracking

---

#### 3. Sparse Texture (Advanced)

**Layout:**
```
Virtual Texture (4096x4096):
  [Pages allocated on-demand]
  [Unmapped pages use zero memory]
```

**Pros:**
- Efficient memory usage (only allocated pages use RAM/VRAM)
- Large virtual address space

**Cons:**
- Requires D3D11.1+ or OpenGL 4.4+
- Complex implementation

---

### Packing Algorithms

#### Shelf Packing (Simple)
```cpp
struct Shelf {
    int y;
    int currentX;
    int height;
};

std::vector<Shelf> shelves;

Rect PackGlyph(int width, int height) {
    // Find shelf with enough space
    for (auto& shelf : shelves) {
        if (shelf.currentX + width <= atlasWidth && height <= shelf.height) {
            Rect rect = {shelf.currentX, shelf.y, width, height};
            shelf.currentX += width;
            return rect;
        }
    }

    // Create new shelf
    int y = shelves.empty() ? 0 : shelves.back().y + shelves.back().height;
    shelves.push_back({y, width, height});
    return {0, y, width, height};
}
```

**Pros:**
- Fast insertion (O(1) amortized)
- Simple to implement

**Cons:**
- Wasted space at shelf ends
- No reorganization

---

#### Rectangle Packing (Optimal)
```cpp
// Use established algorithms:
// - MaxRects (Jukka Jylanki)
// - Skyline (Jukka Jylanki)
// - Guillotine (Jukka Jylanki)

// Library recommendation: rectpack2D
// https://github.com/TeamHypersomnia/rectpack2D
```

**Pros:**
- Better space utilization (80-95% fill rate)
- Optimal for static atlases

**Cons:**
- Slower insertion (O(n))
- Complex implementation

---

## DirectWrite Integration

### Font Fallback Chain

**Strategy:** Use DirectWrite's font fallback API to support PUA glyphs.

**Implementation:**
```cpp
#include <dwrite_3.h>

// Create custom font fallback
IDWriteFontFallbackBuilder* fallbackBuilder;
factory->CreateFontFallbackBuilder(&fallbackBuilder);

// Add Nerd Font as primary for PUA range
DWRITE_UNICODE_RANGE puaRange = {0xE000, 0xF8FF};
WCHAR const* nerdFont = L"JetBrainsMono Nerd Font Mono";

fallbackBuilder->AddMapping(
    &puaRange, 1,                    // Unicode ranges
    &nerdFont, 1,                    // Target font families
    nullptr, 0,                      // Collection
    L"",                             // Locale
    L"",                             // Base family
    1.0f                             // Scale
);

// System fallback for other characters
fallbackBuilder->AddMappings(systemFontFallback);

// Build final fallback chain
IDWriteFontFallback* fontFallback;
fallbackBuilder->CreateFontFallback(&fontFallback);
```

**Result:** PUA glyphs route to Nerd Font, other characters use system fallback.

---

### Composite Font Sets (DirectWrite 3)

**Strategy:** Create composite font set with Nerd Font + system fonts.

**Implementation:**
```cpp
#include <dwrite_3.h>

IDWriteFontSetBuilder2* builder;
factory->CreateFontSetBuilder(&builder);

// Add Nerd Font with PUA axis
DWRITE_FONT_AXIS_RANGE puaAxis[] = {
    {DWRITE_FONT_AXIS_TAG_UNICODE_RANGE, 0xE000, 0xF8FF}
};

builder->AddFontFile(L"JetBrainsMonoNerdFont-Regular.ttf");
builder->SetAxisRanges(puaAxis, 1);

// Add system fonts
builder->AddFontSet(systemFontSet);

// Create composite font set
IDWriteFontSet2* compositeFontSet;
builder->CreateFontSet(&compositeFontSet);
```

**Pros:**
- Clean API design
- Automatic fallback handling

**Cons:**
- Requires Windows 10 20H1+ (DirectWrite 3)

---

### Custom Text Renderer

**Strategy:** Implement `IDWriteTextRenderer` for full rendering control.

**Implementation:**
```cpp
class CustomTextRenderer : public IDWriteTextRenderer {
public:
    HRESULT DrawGlyphRun(
        void* clientDrawingContext,
        FLOAT baselineOriginX,
        FLOAT baselineOriginY,
        DWRITE_MEASURING_MODE measuringMode,
        DWRITE_GLYPH_RUN const* glyphRun,
        DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
        IUnknown* clientDrawingEffect) override
    {
        // Check if glyph is in Nerd Font PUA range
        for (UINT32 i = 0; i < glyphRun->glyphCount; ++i) {
            UINT32 codepoint = glyphRunDescription->textString[i];

            if (codepoint >= 0xE000 && codepoint <= 0xF8FF) {
                // Route to Nerd Font glyph atlas
                RenderFromAtlas(glyphRun->glyphIndices[i], baselineOriginX, baselineOriginY);
            } else {
                // Route to standard glyph rendering
                RenderStandardGlyph(codepoint, baselineOriginX, baselineOriginY);
            }
        }
        return S_OK;
    }

    // ... implement other IDWriteTextRenderer methods
};
```

**Pros:**
- Complete rendering control
- Can optimize per-glyph

**Cons:**
- Complex implementation
- Must handle all rendering scenarios

---

## Performance Optimization Techniques

### 1. Glyph Hash Optimization

**Problem:** Hash computation consumes ~33% of AtlasEngine CPU time.

**Solution:** Use faster hash function for cache keys.

```cpp
// Instead of std::hash (slower)
struct GlyphCacheKeyHash {
    size_t operator()(const GlyphCacheKey& key) const noexcept {
        // FNV-1a hash (fast and good distribution)
        size_t hash = 14695981039346656037ULL;
        hash ^= key.fontID; hash *= 1099511628211ULL;
        hash ^= key.glyphID; hash *= 1099511628211ULL;
        hash ^= key.fontSize; hash *= 1099511628211ULL;
        hash ^= key.dpi; hash *= 1099511628211ULL;
        return hash;
    }
};

std::unordered_map<GlyphCacheKey, AtlasEntry, GlyphCacheKeyHash> cache;
```

**Impact:** 10-20% reduction in hash computation time.

---

### 2. Batch Glyph Rasterization

**Problem:** Rasterizing glyphs one-by-one is inefficient.

**Solution:** Batch multiple glyphs in single DirectWrite call.

```cpp
void RasterizeBatch(const std::vector<GlyphCacheKey>& keys) {
    // Create glyph run with all glyphs
    std::vector<UINT16> glyphIndices;
    std::vector<FLOAT> glyphAdvances;

    for (const auto& key : keys) {
        glyphIndices.push_back(key.glyphID);
        glyphAdvances.push_back(GetGlyphAdvance(key));
    }

    DWRITE_GLYPH_RUN glyphRun = {
        fontFace,
        fontSize,
        (UINT32)glyphIndices.size(),
        glyphIndices.data(),
        glyphAdvances.data(),
        nullptr, // glyphOffsets
        FALSE,   // isSideways
        0        // bidiLevel
    };

    // Render entire batch to texture
    d2dContext->DrawGlyphRun(baselineOrigin, &glyphRun, foregroundBrush);
}
```

**Impact:** 30-50% faster rasterization for cache misses.

---

### 3. SIMD Glyph Copying

**Problem:** Copying glyphs from atlas to framebuffer is bandwidth-intensive.

**Solution:** Use SIMD instructions for fast memory copy.

```cpp
#include <emmintrin.h>  // SSE2
#include <immintrin.h>  // AVX2

void CopyGlyphSSE2(const uint32_t* src, uint32_t* dst, size_t pixelCount) {
    size_t i = 0;

    // Process 4 pixels (16 bytes) at a time
    for (; i + 4 <= pixelCount; i += 4) {
        __m128i data = _mm_loadu_si128((__m128i*)&src[i]);
        _mm_storeu_si128((__m128i*)&dst[i], data);
    }

    // Handle remaining pixels
    for (; i < pixelCount; ++i) {
        dst[i] = src[i];
    }
}

void CopyGlyphAVX2(const uint32_t* src, uint32_t* dst, size_t pixelCount) {
    size_t i = 0;

    // Process 8 pixels (32 bytes) at a time
    for (; i + 8 <= pixelCount; i += 8) {
        __m256i data = _mm256_loadu_si256((__m256i*)&src[i]);
        _mm256_storeu_si256((__m256i*)&dst[i], data);
    }

    // Handle remaining pixels
    for (; i < pixelCount; ++i) {
        dst[i] = src[i];
    }
}
```

**Impact:** 2-4x faster glyph copy on modern CPUs.

---

### 4. GPU Shader Optimization

**Problem:** Per-glyph shader overhead.

**Solution:** Use instanced rendering for multiple glyphs.

```hlsl
// Vertex Shader
struct VSInput {
    float2 position : POSITION;
    float2 texCoord : TEXCOORD0;
    uint instanceID : SV_InstanceID;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer PerInstanceData : register(b0) {
    struct GlyphInstance {
        float2 screenPosition;
        float2 atlasPosition;
        float2 glyphSize;
    } instances[1024];
};

VSOutput main(VSInput input) {
    GlyphInstance inst = instances[input.instanceID];

    VSOutput output;
    output.position = float4(
        inst.screenPosition + input.position * inst.glyphSize,
        0.0,
        1.0
    );
    output.texCoord = inst.atlasPosition + input.texCoord * inst.glyphSize;
    return output;
}

// Pixel Shader
Texture2D glyphAtlas : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VSOutput input) : SV_TARGET {
    return glyphAtlas.Sample(linearSampler, input.texCoord);
}
```

**Draw Call:**
```cpp
// Render 1024 glyphs in single draw call
context->DrawInstanced(6, glyphCount, 0, 0);  // 6 vertices (2 triangles) per glyph
```

**Impact:** 10-100x reduction in draw call overhead.

---

## Memory Management Best Practices

### 1. Monitor Atlas Memory Usage

```cpp
class GlyphAtlas {
    size_t currentMemoryUsage = 0;
    size_t memoryLimit = 256 * 1024 * 1024;  // 256 MB

    bool CanAddGlyph(size_t glyphSize) const {
        return (currentMemoryUsage + glyphSize) <= memoryLimit;
    }

    void LogMemoryUsage() {
        float usagePercent = (float)currentMemoryUsage / memoryLimit * 100.0f;
        if (usagePercent > 80.0f) {
            LOG_WARNING("Glyph atlas at " << usagePercent << "% capacity");
        }
    }
};
```

---

### 2. Implement Atlas Eviction

```cpp
void EvictUnusedGlyphs() {
    auto now = std::chrono::steady_clock::now();

    for (auto it = cache.begin(); it != cache.end(); ) {
        auto timeSinceAccess = now - it->second.lastAccessTime;

        if (timeSinceAccess > std::chrono::minutes(5)) {
            currentMemoryUsage -= it->second.size;
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
}
```

---

### 3. Use Different Atlas Tiers

```cpp
enum class AtlasTier {
    Hot,      // Frequently accessed (kept in VRAM)
    Warm,     // Occasionally accessed (kept in RAM, upload on demand)
    Cold      // Rarely accessed (rasterize on demand)
};

class TieredGlyphCache {
    GlyphAtlas hotAtlas;    // 64 MB - GPU resident
    GlyphAtlas warmAtlas;   // 128 MB - System RAM

    void PromoteGlyph(GlyphID id) {
        if (warmAtlas.Contains(id)) {
            auto data = warmAtlas.GetGlyph(id);
            hotAtlas.Insert(id, data);
            warmAtlas.Remove(id);
        }
    }
};
```

---

## Nerd Fonts Specific Optimizations

### 1. Separate Atlas for Icons

**Strategy:** Keep ASCII and icons in separate atlases.

```cpp
class DualAtlasRenderer {
    GlyphAtlas asciiAtlas;   // ~1000 glyphs (ASCII + extended Latin)
    GlyphAtlas iconAtlas;    // ~3000 glyphs (Nerd Font icons)

    void RenderGlyph(uint32_t codepoint) {
        if (codepoint < 0xE000) {
            RenderFromAtlas(asciiAtlas, codepoint);
        } else {
            RenderFromAtlas(iconAtlas, codepoint);
        }
    }
};
```

**Benefits:**
- Icon atlas can use different eviction policy
- Can unload icon atlas when not needed

---

### 2. Icon Glyph Preloading by Category

**Strategy:** Preload icon sets based on user's shell prompt.

```cpp
void PreloadPowerlineIcons() {
    // Powerline Symbols (e0a0-e0d7)
    for (uint32_t cp = 0xE0A0; cp <= 0xE0D7; ++cp) {
        PreloadGlyph(cp);
    }
}

void PreloadDevicons() {
    // Devicons (e700-e8ef)
    for (uint32_t cp = 0xE700; cp <= 0xE8EF; ++cp) {
        PreloadGlyph(cp);
    }
}

// Call based on user configuration
if (userUsesStarship || userUsesPowerlevel10k) {
    PreloadPowerlineIcons();
}

if (userUsesLSD || userUsesExa) {
    PreloadDevicons();  // For file icons
}
```

---

### 3. Lazy Icon Set Loading

**Strategy:** Load icon sets on-demand.

```cpp
enum class IconSet {
    Powerline,
    FontAwesome,
    Devicons,
    MaterialDesign,
    // ...
};

class LazyIconSetLoader {
    std::unordered_map<IconSet, bool> loadedSets;

    void EnsureIconSetLoaded(IconSet set) {
        if (loadedSets[set]) return;

        switch (set) {
            case IconSet::Powerline:
                LoadIconRange(0xE0A0, 0xE0D7);
                break;
            case IconSet::FontAwesome:
                LoadIconRange(0xED00, 0xF2FF);
                break;
            // ... other sets
        }

        loadedSets[set] = true;
    }

    IconSet DetectIconSet(uint32_t codepoint) {
        if (codepoint >= 0xE0A0 && codepoint <= 0xE0D7) return IconSet::Powerline;
        if (codepoint >= 0xED00 && codepoint <= 0xF2FF) return IconSet::FontAwesome;
        // ... other ranges
        return IconSet::Unknown;
    }
};
```

---

## Testing and Profiling

### Performance Metrics to Track

```cpp
struct RenderingMetrics {
    // Timing
    double avgFrameTime;
    double avgGlyphRasterTime;
    double avgCacheLookupTime;

    // Cache statistics
    uint64_t cacheHits;
    uint64_t cacheMisses;
    double cacheHitRate;

    // Memory
    size_t atlasMemoryUsage;
    uint32_t cachedGlyphCount;

    // GPU
    uint32_t drawCallCount;
    uint32_t textureBindCount;
};

void LogMetrics() {
    LOG_INFO("Frame time: " << metrics.avgFrameTime << "ms");
    LOG_INFO("Cache hit rate: " << (metrics.cacheHitRate * 100) << "%");
    LOG_INFO("Atlas memory: " << (metrics.atlasMemoryUsage / 1024 / 1024) << "MB");
    LOG_INFO("Glyphs cached: " << metrics.cachedGlyphCount);
}
```

---

### Stress Tests

```cpp
// Test 1: Rapid scrolling through mixed ASCII + icons
void StressTest_ScrollMixedContent() {
    for (int i = 0; i < 10000; ++i) {
        RenderLine(GenerateRandomMixedLine());
        ScrollDown(1);
    }
}

// Test 2: Display all Nerd Font icons
void StressTest_AllIcons() {
    for (uint32_t cp = 0xE000; cp <= 0xF8FF; ++cp) {
        RenderGlyph(cp);
    }
    for (uint32_t cp = 0xF0000; cp <= 0xF1AF0; ++cp) {
        RenderGlyph(cp);
    }
}

// Test 3: Memory pressure test
void StressTest_MemoryPressure() {
    // Render unique glyphs until atlas is full
    for (int size = 8; size <= 72; size += 2) {
        for (uint32_t cp = 32; cp <= 126; ++cp) {
            RenderGlyphAtSize(cp, size);
        }
    }
}
```

---

## References

- **Windows Terminal AtlasEngine:** https://github.com/microsoft/terminal/pull/11623
- **Warp Glyph Atlas:** https://www.warp.dev/blog/adventures-text-rendering-kerning-glyph-atlases
- **DirectWrite Documentation:** https://learn.microsoft.com/en-us/windows/win32/directwrite/
- **Rectangle Packing Algorithms:** https://github.com/TeamHypersomnia/rectpack2D
- **fontstash (Reference Implementation):** https://github.com/memononen/fontstash

---

**Document Created:** 2025-10-11
**Last Updated:** 2025-10-11
**For:** Windows Terminal Optimization Project
