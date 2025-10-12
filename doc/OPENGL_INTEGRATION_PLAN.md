# OpenGL Integration Plan

**Goal:** Bring the OpenGL 4.6/3.3 backends to parity by consuming the new SPIR-V/GLSL shader outputs and mirroring the D3D12 batching pipeline.

## 1. Artifacts
- `build/shaders/UltraPerformance.GL/shader_vs.spv`
- `build/shaders/UltraPerformance.GL/shader_ps.spv`
- `build/shaders/UltraPerformance.GL/shader_vs.glsl`
- `build/shaders/UltraPerformance.GL/shader_ps.glsl`

## 2. Backend Work
1. Update OpenGL shader loader to prefer SPIR-V (via `GL_ARB_gl_spirv`) and fall back to GLSL source.
2. Mirror descriptor layout:
   - UBO binding 0 → VS constants
   - UBO binding 1 → PS constants
   - SSBO binding 0 → instance data
   - Sampler binding 0 → glyph atlas
3. Implement persistent mapped buffers for instance data to match the D3D12 batching throughput.
4. Integrate DirectStorage fallback path: use Win32 async IO to stage textures when SPIR-V path is enabled.
5. Add telemetry: reuse `TelemetryReporter` with OpenGL-specific tags.

## 3. Build
- CMake/MSBuild: ensure `UltraPerformance.GL` triggers `Invoke-SPIRVBuild.ps1` and copies generated GLSL/SPIR-V to output.
- Linux/macOS: add Meson/CMake equivalents.

## 4. Testing
- RenderDoc captures for 4K120 and 1080p360 profiles.
- Compare frame times against D3D12 baseline.

