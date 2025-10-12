# Renderer Feature Matrix (DX12 / DX11 / OpenGL / CPU)

| Capability | D3D12 (Target) | D3D11 (Enhanced) | OpenGL 4.6 | OpenGL 3.3 | CPU / WARP |
|------------|----------------|------------------|------------|------------|-------------|
| Glyph Batching | Mesh/DrawIndirect (<=65k quads per batch) | Multi-draw instancing | Multi-draw-indirect + persistent VBOs | Instanced arrays (limited) | AVX2 tile rasterizer |
| Texture Residency | Residency Manager + Sampler Feedback | Tiled resource API | Sparse textures (ARB_sparse_texture) | Manual paging | CPU-managed atlas |
| Storage Path | DirectStorage + GPU decompression | Win32 async I/O (future: DStorage fallback) | Win32 async I/O | Win32 async I/O | Memory mapped |
| Shader Toolchain | HLSL → DXIL (DXC), optional Mesh/Compute | HLSL → DXIL (DXC) / legacy FXC backup | HLSL → SPIR-V (DXC) → GL via GL_ARB_gl_spirv | HLSL → SPIR-V → GLSL 330 (spirv-cross) | HLSL → ISPC/LLVM (SIMD) |
| Advanced Features | VRS, Mesh Shaders, Work Graphs, DXR (optional), Sampler Feedback, NVAPI/AGS | Conservative Raster, Driver hints, On12 bridging | Bindless textures, AZDO | Double-buffered VBO, limited extensions | Vectorized loops + multi-thread command buffers |
| Presentation | DXGI 1.6 Flip Model, tearing, VRR, HDR | DXGI Flip Model (tier 3) | WGL_EXT_swap_control_tear, NV_delay_before_swap | Swap control (vsync only) | GDI / D2D present |
| Telemetry | PIX markers, GPU counters, DirectStorage stats | PerfHUD/NVAPI counters | KHR_debug markers | ARB_debug_output | CPU perf counters |
| Vendor Hooks | NVAPI Reflex, AGS Wave tuning, CUDA interop | NVAPI/AGS hints | NV_bindless (NVIDIA), AMD_sparse | Minimal | CPU only |
| Testing Harness | PIX, GPUView, Nsight, Radeon GPU Profiler | PIX, GPUView, vendor tools | RenderDoc, apitrace | RenderDoc | CPU profilers (VTune, WPA) |

## Cross-Backend TODO (High Priority)
1. **Shader Harmonization**
   - [ ] Verify DXC SPIR-V output for every shader permutation.
   - [ ] Add automated diff tool comparing DXIL vs SPIR-V outputs for struct packing.
2. **Telemetry Consistency**
   - [ ] Define unified event schema (frame begin/end, queue submit, DirectStorage batch).
   - [ ] Implement backend-specific adapters that feed shared overlay.
3. **Settings Surface**
   - [ ] Extend graphics settings to expose: backend selection, DirectStorage toggle, VRR / tearing, vendor features.
   - [ ] Persist per-display overrides (4K120 vs 1080p360) for dynamic tuning.
4. **Benchmark Harness**
   - [ ] Headless mode that replays recorded command traces to benchmark GPU/CPU across backends.
   - [ ] Collect ETW traces automatically and reduce to JSON for dashboards.

## Resource Links (Reference)
- Direct3D 12 Ultimate feature set: https://devblogs.microsoft.com/directx/directx12-ultimate-getting-started
- DirectStorage 1.2 overview: https://devblogs.microsoft.com/directx/directstorage-1-2
- NVIDIA NVAPI SDK: https://developer.nvidia.com/nvapi
- AMD AGS SDK: https://gpuopen.com/architecture-graphics-sdk
- Intel Graphics Analyzer: https://www.intel.com/content/www/us/en/developer/tools/graphics-performance-analyzers/overview.html
- GL_ARB_gl_spirv spec: https://registry.khronos.org/OpenGL/extensions/ARB/ARB_gl_spirv.txt
