# Direct3D 12 Feature Audit (2025-10-12)

| Feature / API | Hardware Tier | Current Integration | Gaps / Next Actions |
|---------------|---------------|---------------------|---------------------|
| **Resource Binding Tier 3** | Maxwell+/GCN+ | Descriptor heap schema in `BackendD3D12` preallocates 128 slots, SRV/UAV tables and sampler tables wired. | Audit descriptor reuse for compute path; add residency tracking for atlas textures. |
| **Explicit Multi-Queue** | DX12 base | Graphics/Compute/Copy queues created with individual fences/events (`BackendD3D12.cpp:223`). | Copy queue not yet leveraged for streamed glyph uploads (DirectStorage path currently copy-through compute queue). |
| **Variable Rate Shading (VRS)** | Tier ≥1 (DX12 Ultimate) | Capability query recorded in `_features.variableShadingRateTier`. Runtime flag toggles exposed. | Implement per-draw shading rate image + rate combiners when hardware supports. |
| **Sampler Feedback** | Tier ≥0.9 (DX12 Ultimate) | Capability detection stored; pipeline has SRV/UAV descriptors reserved. | Generate feedback map for glyph atlas to drive DirectStorage residency and implement eviction heuristics. |
| **Mesh Shaders / Amplification** | Mesh Shader Tier (DX12 Ultimate) | Capability stored; no mesh shader PSO yet. | Prototype instanced quad amplification shader to offload vertex duplication. |
| **Work Graphs** | WDDM 3.2+ | Detection stub toggles flag when available. | Investigate GPU-driven draw call building for glyph batches. |
| **Sampler Heap Feedback** | Hardware dependent | Descriptor schema ready. | Need instrumentation to monitor descriptor heap pressure at runtime. |
| **DirectStorage 1.2+** | NVMe (T10/T13) | Factory/queue created, remote 9P cache with telemetry, glyph streaming path implemented. | Integrate copy queue, add asynchronous decompression for future GPU formats. |
| **DXR 1.1 (Raytracing)** | DXR GPUs | Not integrated. | Evaluate optional raytraced CRT/blur effects; provide feature toggle. |
| **Residency Management** | Tier 2+ | Placeholder; atlas uses committed resources. | Add `ID3D12Device3::EnqueueMakeResident` for large atlas textures and sampler feedback results. |
| **Frame Presentation (DXGI 1.6)** | WDDM 2.7+ | Flip model swap chain; tearing enabled via settings. | Add HDR metadata negotiation and VRR telemetry. |
| **Vendor APIs (NVAPI, AGS)** | Vendor-specific | Detection scaffolding + telemetry; toggles pending. | Invoke Reflex/Anti-Lag APIs when flags set; capture driver error codes. |
| **Compute Shaders** | DX12 base | Stub PSO placeholder; glyph compute path not yet migrated. | Port glyph rasterization + background grid generation to D3D12 compute queue. |
| **Telemetry** | All | DirectStorage + Vendor events logged. | Add PIX GPU markers for VRS/Sampler feedback path, integrate into HUD. |

## External References
- DirectX 12 Ultimate overview: https://devblogs.microsoft.com/directx/directx12-ultimate-getting-started
- Variable Rate Shading: https://microsoft.github.io/DirectX-Specs/d3d/VRS.html
- Work Graphs: https://devblogs.microsoft.com/directx/work-graphs-in-directx-12/
- DirectStorage 1.2 release: https://devblogs.microsoft.com/directx/directstorage-1-2
- NVAPI SDK: https://developer.nvidia.com/nvapi
- AMD AGS SDK: https://gpuopen.com/architecture-graphics-sdk
