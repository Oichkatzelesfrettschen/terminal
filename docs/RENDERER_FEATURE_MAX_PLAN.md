# Renderer Feature-Maximization Roadmap

## 0. Vision & Performance Targets
- **Platforms**: Windows 11 (24H2+) with latest WDDM, AMD/NVIDIA/Intel GPUs, Ryzen/Intel CPUs with AVX2+/AVX-512 where available.
- **Refresh & Resolution Goals**:
  - 3840x2160 @ 120 Hz (HDR + 10-bit pipeline)
  - 2560x1440 @ 240 Hz (ultra-wide variants up to 3440x1440 @ 240 Hz)
  - 1920x1080 @ 360 Hz (low-latency focus)
- **Latency Targets**: < 4 ms GPU frame, < 2 ms CPU submission budget, < 1 ms storage-to-GPU glyph upload for 95th percentile.
- **Rendering Paradigm**: GPU-first batching, DirectStorage-fed glyph pipelines, CPU SIMD fallback for WARP/CPU-only scenarios, shared shader source across DirectX & OpenGL.

## 1. Direct3D 12 Feature Stack
### 1.1 Core Rendering Pipeline
- Explicit multi-queue design: Graphics (present), Compute (grid/glyph), Copy (DirectStorage + uploads).
- Triple-buffered frame resources with fence-per-backbuffer, frame-latency capped at 1.
- Descriptor heap strategy:
  - Global shader-visible CBV/SRV/UAV heap (tier-aware, 2K descriptors budget).
  - Static sampler heap (point, linear, anisotropic).
- Root signature revisions:
  - Slot 0: VS constants (frame data).
  - Slot 1: PS constants (color tables, gamma).
  - Slot 2: SRV descriptor table (glyph atlas + dynamic textures).
  - Slot 3: UAV table (glyph compute outputs, debug overlays).
  - Slot 4: Sampler table.
- Pipeline State Objects:
  - Background/text/cursor/line PSOs with dual-source blending for ClearType.
  - Mesh Shader PSO prototype for future instancing experiments (conditional via feature flag).
  - Compute PSOs for grid generation, glyph rasterization, shader-based post-effects (CRT, noise, etc.).
  - Optional RTPSO if DXR-backed effects (e.g., CRT glow) are explored.

### 1.2 Advanced D3D12 Features
| Feature | Hardware Requirement | Integration Plan |
|---------|----------------------|------------------|
| **Variable Rate Shading (VRS)** | Tier 2+ capable GPUs | Use NVAPI/AGS for per-vendor tuning. Apply coarse shading on smooth backgrounds, full rate on glyph edges. |
| **Sampler Feedback** | Tier 1+ (DX12U) | Track glyph residency, drive DirectStorage streaming of rarely-used glyph pages. |
| **Mesh Shaders** | DX12 Ultimate | Prototype glyph instancing via amplification shader to reduce CPU vertex work. |
| **Raytracing (DXR 1.1)** | DXR-capable GPUs | Optional: accelerate glow/shadow effects while keeping fallback path. |
| **Work Graphs** | WDDM 3.2+ | Async generation of glyph draw commands on GPU. |
| **Residency Management** | Tier 2+ | Manually manage heaps for atlas + background textures; use `ID3D12Device3::EnqueueMakeResident`. |
| **Frame Presentation** | DXGI 1.6 | Use `Present1` with dirty rectangles, allow tearing for >240 Hz with `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`. |

### 1.3 DirectStorage Integration
- Storage queues:
  - `DSTORAGE_REQUEST_SOURCE_FILE` pipeline for glyph bitmaps, shader blobs, background images.
  - GPU decompression (GDeflate) for atlas updates; fall back to CPU on unsupported hardware.
- Copy queue synergy: feed Copy queue with DirectStorage outputs (alias same command queue where supported to reduce synchronization).
- Batch updates: accumulate glyph requests per frame, submit via DirectStorage `EnqueueRequest`, fence on compute queue before PSO uses new data.
- File packaging: adopt packed glyph cache (NVTT/BC7 for tinted backgrounds) with DirectStorage-friendly alignment.
- Telemetry: integrate DirectStorage runtime stats into ETW + custom overlay.

### 1.4 Vendor Extensions
- **NVIDIA (NVAPI)**:
  - Enable `NVAPI_D3D_SetSleepMode` / Reflex-style low-latency hints for 360 Hz path.
  - Use shader extension slots for warp-level intrinsics where beneficial.
  - Hook into `NVAPI_D3D12_SetFeatureHints` for overclock/perf states.
- **AMD (AGS/NVAPI-equivalent)**:
  - Query RDNA Wave64/Wave32 preferences; align shader compile choices.
  - Access SAM (Smart Access Memory) state for atlas residency planning.
- **Intel**:
  - Utilize Intel GPA instrumentation, consider `INTC_D3D12_GetCustomExtension` for perf counters.
- **CUDA / GPU Interop**:
  - Provide optional CUDA interop path for GPU compute heavy filters (e.g., path to run NV-accelerated blur for CRT shader).
  - Use `cudaExternalMemory_t` & `cudaExternalSemaphore_t` for zero-copy atlas updates.

## 2. Direct3D 11 Enhancement Track
- Keep D3D11 backend feature parity via shared shader permutations (DXIL compiled via DXC, fallback to FXC where necessary).
- Integrate `ID3D11On12Device` bridge to leverage D3D12 features when D3D11 is selected but hardware supports DX12.
- Use driver hints (`IDXGIDevice4::OfferResources`) for glyph atlas trimming.
- Optional vendor features: NVAPI, AGS for driver-level overrides (flip model, low-latency mode).
- Optimize instancing using `ID3D11DeviceContext4::BeginEventInt` instrumentation for telemetry.

## 3. OpenGL 4.6 Path (Modern)
- Adopt AZDO techniques:
  - Persistent mapped buffers for instance data.
  - Bindless textures for glyph atlas (via `GL_ARB_bindless_texture`).
  - Multi-draw-indirect to mirror D3D12 batching.
- SPIR-V ingestion via `GL_ARB_gl_spirv` using DXC SPIR-V backend.
- Synchronize shader source via single HLSL repository → compile to SPIR-V → cross-compile to GLSL (fallback glslang) when `GL_ARB_gl_spirv` absent.
- Use `GL_NV_gpu_multicast` / `GL_AMD_sparse_texture` where available.
- Present via `wglSwapIntervalEXT(0)` with latency markers; integrate `GLX_NV_delay_before_swap` equivalent on supported drivers.

## 4. OpenGL 3.3 Compatibility Path
- Legacy shader pipeline translation using `spirv-cross` to GLSL 330.
- Replace bindless features with texture arrays + UBOs.
- Instance data via dynamic VBO updates, double-buffered for sync.
- Frame pacing through `WGL_EXT_swap_control_tear` where available.
- Provide optional ARB_clip_control + viewport arrays for ultrawide pixel-perfect layout.

## 5. CPU-Only / WARP Fallback
- Multi-threaded renderer built on:
  - `Direct2D` (existing) + SIMD extension path using `DirectXMath` or `ISPC` for rasterization.
  - Glyph atlas CPU rasterizer leveraging AVX2/AVX-512 copying (64-byte wide stores).
- Use `til::` concurrency helpers + `std::barrier` for frame sync.
- Shared shader logic mirrored in scalar C++ (auto-generated via ISPC or LLVM for CPU pass where feasible).
- Integrate DirectStorage CPU fallback (synchronous read + software decompression via Oodle/BCN decoders if license permits).

## 6. Shader Toolchain Strategy
- **Primary compiler**: DXC 1.8+ for HLSL → DXIL (D3D12/D3D11) and HLSL → SPIR-V (GL/Vulkan/CPU compute).
- **Secondary tools**: glslangValidator, spirv-cross, fxc (for legacy compatibility testing).
- Unified shader module layout:
  - `shader_common.hlsl`: semantics macros, shared structs.
  - Backend-specific wrappers define semantics (`POSITION` vs `SV_Position`, vendor semantics).
  - Build scripts spit out DXIL/CSO/GLSL headers per backend.
- Shader tests: integrate unit tests via `dxc --dumpbin /extract` + `spirv-val` to ensure feature-level compatibility.

## 7. Frame Timing & Presentation Strategy
- Frame pacing library:
  - Use `IDXGISwapChain4::SetHDRMetaData` & `CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING)` to switch between VSync/VFR.
  - Integrate `QPC`-based CPU timers + `ID3D12CommandQueue::GetTimestampFrequency` for GPU metrics.
  - Implement adaptive resolution cell scaling for 360 Hz fallback (reduce vertical resolution gracefully).
- Support monitor feature detection (VRR, HDR) via DXGI output queries; tie into settings UI.

## 8. Storage & Asset Pipeline
- Asset packaging:
  - Glyph atlas shards stored as DirectStorage-friendly `.dstorage` bundles.
  - Shader binaries packaged per architecture (DXIL, SPIR-V, CPU).
- Build integration:
  - MSBuild/PowerShell tasks to preprocess assets, run DXC, pack DirectStorage bundles.
  - Optional integration with `Microsoft.GameCore.DirectStorage` CLI for verifying dataset.

## 9. Telemetry & Diagnostics
- Performance HUD overlay toggled via settings:
  - Frame time, GPU queue utilization, DirectStorage throughput.
  - VRS shading rate visualization, glyph residency heatmap.
- Logging pipeline using ETW providers:
  - `Microsoft-Windows-Direct3D12`, `Microsoft-DirectStorage`, vendor-specific providers.
- PIX/RenderDoc capture scripts; integrate to CI gating for regression detection.

## 10. Testing Matrix
| Resolution / Hz | Backend | GPU Vendors | Storage Path | Notes |
|-----------------|---------|-------------|--------------|-------|
| 4K @ 120 | D3D12 | NVIDIA Ada, AMD RDNA3, Intel Arc | NVMe DirectStorage + Fallback | HDR + VRS + Mesh Shader smoke tests |
| 1440p @ 240 | D3D12 | Same | DirectStorage | Focus on VRR + low-latency present |
| 1080p @ 360 | D3D12 | NVIDIA RTX (Reflex), AMD (AFMF) | DirectStorage | Tearing allowed, Present1 dirty rects |
| 4K @ 120 | D3D11 | All | Win32 async file IO | Parity validation |
| 4K @ 120 | OpenGL 4.6 | NVIDIA + AMD | Win32 file IO | SPIR-V path |
| 4K @ 60 | OpenGL 3.3 | All | Win32 file IO | Legacy fallback |
| 4K @ 60 | CPU | Any | Memory mapped | AVX2 raster pipeline |

## 11. Implementation Phasing (TODO Backlog)
1. **Phase A – D3D12 Feature Lift**
   - [ ] Finalize root signature + descriptor heap schema.
   - [ ] Implement multi-queue command submission + fence orchestration.
   - [ ] Integrate DXC pipeline + regenerate shader headers.
   - [ ] Add VRS tier detection + shading rate resources.
   - [ ] Wire Sampler Feedback for glyph residency tracking.
2. **Phase B – DirectStorage Pipeline**
   - [ ] Introduce DirectStorage manager (queue creation, request batching).
   - [ ] Adapt glyph atlas loader to DirectStorage data path.
   - [ ] Add GPU decompression fallback logic.
   - [ ] Telemetry + debugging hooks (ETW, overlay).
3. **Phase C – Vendor Extensions**
   - [ ] Wrap NVAPI/AGS/Intel APIs with runtime detection.
   - [ ] Expose settings toggles for low-latency/VRR overrides.
   - [ ] Validate on each vendor hardware tier.
4. **Phase D – Cross-Backend Shader Harmonization**
   - [ ] DXC SPIR-V output integration.
   - [ ] Build-time conversion to GLSL 4.6 / GLSL 3.3.
   - [ ] CPU raster fallback via ISPC or LLVM JIT.
5. **Phase E – Testing & Automation**
   - [ ] Establish automated benchmark harness (present-based timing + GPU counters).
   - [ ] Add CI smoke tests for each backend/resolution pair (headless where possible).
   - [ ] Capture PIX/RenderDoc/NSight scripts for manual deep dives.

## 12. Tooling & Build Integration
- PowerShell build scripts:
  - `Invoke-DXCBuild.ps1`: compiles HLSL to DXIL/SPIR-V/headers.
  - `Invoke-DirectStoragePack.ps1`: packs glyph assets.
  - `Invoke-GPUValidation.ps1`: runs D3D12 debug layer, NVAPI/AGS diagnostics.
- MSBuild configurations:
  - `UltraPerformance` (existing) → base.
  - `UltraPerformance.DStorage` → toggles DirectStorage + VRS + Mesh Shader.
  - `LegacyCompatibility` → disables advanced features for older hardware.
- Dependency management: use vcpkg or NuGet for NVAPI, AGS SDK, DirectStorage runtime redistributables.

## 13. Risk & Mitigation
- **Driver Fragmentation**: Maintain feature toggles + safe fallbacks, detect unsupported features at runtime.
- **Shader Complexity**: Keep shared HLSL modular, rely on automated tests to validate permutations.
- **Storage Availability**: Provide fallback to Win32 async IO when DirectStorage not installed.
- **Profiling Overhead**: Make telemetry optional to avoid perf impact in release builds.

## 14. Next Actions (Immediate)
1. Implement DXC-based shader generation flow and commit generated headers for D3D12 path.
2. Prototype DirectStorage loader for glyph atlas with CPU fallback, integrate into BackendD3D12 resource init.
3. Add runtime detection stubs for VRS, Sampler Feedback, and vendor extensions to drive capabilities UI.
4. Update Atlas settings schema to expose backend selection + feature toggles (VRR, tearing, DirectStorage, vendor fast paths).

