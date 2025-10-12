# Build, Tooling, and Validation Strategy

## 1. Toolchain Checklist (Windows Host)
| Tool | Version | Purpose | Install Command |
|------|---------|---------|-----------------|
| Visual Studio | 2022 17.11+ | MSVC, Windows SDK, PIX integration | VS Installer (Desktop + Game workloads) |
| Windows SDK | 10.0.26100+ | DirectX Headers, DirectStorage libs | Bundled with VS |
| DirectXShaderCompiler | 1.8.2505 | DXIL + SPIR-V compilation | `winget install "Microsoft.DirectXShaderCompiler"` |
| DirectStorage Runtime | 1.2.3+ | GPU IO pipeline | Download from Microsoft Game Dev |
| NVAPI SDK | R560+ | NVIDIA feature hooks | Manual download |
| AMD AGS SDK | 6.2+ | RDNA feature control | GPUOpen download |
| Intel GPA | 2024.3+ | Intel profiling | Intel download |
| Vulkan SDK | 1.3.290+ | SPIR-V toolchain | `winget install Khronos.Group.VulkanSDK` |
| glslang / spirv-cross | Latest | GLSL/SPIR-V conversion | Provided in Vulkan SDK |
| PIX for Windows | 2310+ | GPU capture & analysis | Microsoft Store/Download |
| GPUView | Latest WPT | Present & queue analysis | Windows Performance Toolkit |

## 2. Build Configurations
- `UltraPerformance`: Baseline high-performance configuration (DX11/12).
- `UltraPerformance.DX12`: Enables D3D12 advanced feature flags (VRS, Mesh Shader, DirectStorage, vendor hooks).
- `UltraPerformance.DX12.Shipping`: Release-optimized with telemetry off by default.
- `UltraPerformance.GL`: Builds OpenGL 4.6 backend and SPIR-V artifacts.
- `Compatibility.GL33`: Forces OpenGL 3.3 shaders + legacy path.
- `Fallback.CPU`: Builds CPU raster backend (uses ISPC when available).

### PowerShell Entry Points
```powershell
# Configure developer environment
pwsh -File tools\Setup-RenderingToolchain.ps1

# Build all shader permutations (DXIL + SPIR-V)
pwsh -File tools\Invoke-DXCBuild.ps1 -Configuration UltraPerformance.DX12

# Pack glyph assets for DirectStorage
pwsh -File tools\Invoke-DirectStoragePack.ps1 -Input assets\glyphs -Output build\dstorage

# Full solution build
devshell> msbuild OpenConsole.sln /m /p:Configuration=UltraPerformance.DX12 /p:Platform=x64
```

## 3. Automated Validation Pipelines
### 3.1 Continuous Integration
- **Stage 1 – Build & Unit**
  - Run DXC compile for all shaders; fail build if warnings.
  - Execute shader reflection unit tests (struct layout, semantics).
- **Stage 2 – Renderer Smoke Tests**
  - Launch headless test harness `RendererTestHost.exe` to render scripted frames per backend/resolution pair.
  - Capture GPU timings via `ID3D12CommandQueue::GetClockCalibration` and output JSON metrics.
- **Stage 3 – Artifact Packaging**
  - Publish DXIL, SPIR-V, DirectStorage bundles, and telemetry symbol files.

### 3.2 Nightly Performance Runs
- Use dedicated RTX 5090 / RX 8900 / Intel Arc B580 machines.
- Script: `tools\Invoke-PerfSuite.ps1`
  - Iterates over 4K120, 1440p240, 1080p360, ultra-wide combos.
  - Toggles VRR, HDR, tearing according to matrix.
  - Records ETW traces + GPUView sessions.
  - Aggregates metrics into `perf_reports\YYYYMMDD.json`.

### 3.3 Vendor Validation
- **NVIDIA**: Run Nsight Systems/Graphics automation, ensure Reflex latencies under 10 ms input-to-photon at 360 Hz.
- **AMD**: Use Radeon GPU Profiler for wave occupancy, ensure AGS toggles map to expected wave modes.
- **Intel**: GPA capture to validate EU occupancy and memory bandwidth.

## 4. Manual Test Playbook
1. **Functional Smoke**
   - Launch Terminal, switch backend between DX12/DX11/GL/CPU via settings.
   - Enable/disable DirectStorage from UI; monitor glyph loading times.
   - Toggle VRR/tearing/hdr; verify present path updates.
2. **Stress Tests**
   - 10,000 tab glyph stress with waterfall effect.
   - Animated shader effect (CRT) at 360 Hz, ensure GPU utilization < 75%.
   - Force flush/resident loops to exercise memory pressure.
3. **Failure Injection**
   - Simulate DirectStorage queue saturation; ensure fallback to Win32 I/O.
   - Drop vendor DLLs (NVAPI/AGS) to ensure graceful degradation.
   - Force compute queue timeout (debug layer) to validate recovery.

## 5. Telemetry & Reporting
- All builds emit `RendererPerf.log` w/ JSON entries per frame.
- Telemetry aggregator `tools\Merge-PerfMetrics.ps1` merges per-backend data for dashboards.
- Grafana/PowerBI dashboards ingest metrics for weekly regression analysis.

## 6. Documentation & Training
- Maintain `docs/RenderingFeatureMaxPlan.md` (high-level).
- Update `docs/RenderPipelineSetup.md` with step-by-step developer setup.
- Record PIX/GPUView capture walkthrough videos for onboarding.

