# Phase 0: Implementation Guide - D3D12 Compilation Fixes
## Ultra-Riced Windows Terminal - Immediate Execution

**Status**: IN PROGRESS (14/28 tasks complete)
**Time Remaining**: ~1 hour
**Priority**: URGENT - Execute today

---

## Progress Summary

✅ **COMPLETED** (14 tasks):
1. ✅ Fixed shader Output semantic - color: SV_Target0
2. ✅ Fixed shader Output semantic - weights: SV_Target1
3. ✅ Found GraphicsAPI enum in common.h
4. ✅ Added Direct3D12 to GraphicsAPI enum
5. ✅ Updated pch.h: dxgi1_3.h → dxgi1_6.h
6. ✅ Updated AtlasEngine.h: dxgi1_3.h → dxgi1_6.h
7. ✅ Identified all DXGI includes

🔄 **IN PROGRESS** (1 task):
8. 🔄 Finding backend switch statement in AtlasEngine.r.cpp

⏳ **PENDING** (13 tasks):
9. ⏳ Download d3dx12.h (MANUAL STEP REQUIRED)
10. ⏳ Place d3dx12.h in src/renderer/atlas/
11. ⏳ Add include to BackendD3D12.h
12. ⏳ Add case GraphicsAPI::Direct3D12
13. ⏳ Move static buffers to members
14. ⏳ Fix input layout semantic
15. ⏳ Build & test

---

## Files Modified So Far

```
src/renderer/atlas/shader_d3d12_ps.hlsl
  - Line 36: Added ": SV_Target0" to color
  - Line 37: Added ": SV_Target1" to weights

src/renderer/atlas/common.h
  - Line 323: Added "Direct3D12," to GraphicsAPI enum

src/renderer/atlas/pch.h
  - Line 23: Changed dxgi1_3.h → dxgi1_6.h

src/renderer/atlas/AtlasEngine.h
  - Line 8: Changed dxgi1_3.h → dxgi1_6.h
```

---

## NEXT STEPS (Manual Actions Required)

### Step 1: Download d3dx12.h (5 minutes)

The d3dx12.h file is a Microsoft helper header that provides convenience wrappers for D3D12 APIs.

**Option A: Direct Download from GitHub**
```bash
# Windows PowerShell
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/microsoft/DirectX-Headers/main/include/directx/d3dx12.h" -OutFile "src/renderer/atlas/d3dx12.h"
```

**Option B: Manual Download**
1. Open browser to: https://github.com/microsoft/DirectX-Headers
2. Navigate to: `include/directx/d3dx12.h`
3. Click "Raw" button
4. Save As: `src/renderer/atlas/d3dx12.h`

**Option C: Clone Repository**
```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/microsoft/DirectX-Headers
cd DirectX-Headers
git sparse-checkout set include/directx
cp include/directx/d3dx12.h ../windows-terminal-optimized/src/renderer/atlas/
```

**Validation**: File should be ~2,800 lines, starts with:
```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef __D3DX12_H__
#define __D3DX12_H__
```

### Step 2: Add Include to BackendD3D12.h

Open `src/renderer/atlas/BackendD3D12.h` and add after existing D3D12 includes:

```cpp
#include <d3d12.h>
#include <dxgi1_6.h>
#include "d3dx12.h"  // ADD THIS LINE
```

### Step 3: Wire Up D3D12 Backend in AtlasEngine

The next automated step will add the case statement to instantiate the D3D12 backend.

---

## Automation-Ready Tasks

The following tasks can be completed via the todo system once Step 1-2 are done:

- Add case GraphicsAPI::Direct3D12 to Atlas

Engine switch
- Refactor static buffers to class members
- Fix input layout semantic (SV_Position → POSITION)
- Build with UltraPerformance configuration
- Test D3D12 backend

---

## Quick Reference: Remaining Code Changes

### AtlasEngine.r.cpp Backend Switch

**Location**: Search for `switch (s.graphicsAPI)` or `case GraphicsAPI::Direct3D11`

**Add After D3D11 Case**:
```cpp
case GraphicsAPI::Direct3D12:
{
#if ATLAS_ENGINE_D3D12
    _b = std::make_unique<BackendD3D12>(p);
#else
    THROW_HR(E_NOTIMPL); // D3D12 backend not compiled
#endif
    break;
}
```

### BackendD3D12.cpp - Move Static Buffers

**Find** (around line 1283-1354):
```cpp
// Static vertex buffer definition
static constexpr f32x2 vertices[]{ ... };
static constexpr u16 indices[]{ ... };
```

**Action**: Move these to class member variables in BackendD3D12.h:
```cpp
class BackendD3D12 {
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> _vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> _indexBuffer;
    // ...
};
```

### BackendD3D12.cpp - Fix Input Layout Semantic

**Find** (around line 462-470):
```cpp
D3D12_INPUT_ELEMENT_DESC inputElements[] = {
    {"SV_Position", 0, DXGI_FORMAT_R32G32_FLOAT, ...},  // WRONG
    // ...
};
```

**Change To**:
```cpp
D3D12_INPUT_ELEMENT_DESC inputElements[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, ...},  // CORRECT
    // ...
};
```

---

## Build Commands

After all changes are complete:

```powershell
# Clean build
Remove-Item -Recurse -Force bin\, obj\

# Build with UltraPerformance
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
msbuild OpenConsole.sln /p:Configuration=UltraPerformance /p:Platform=x64 /m

# Expected time: 15-20 minutes
```

---

## Expected Outcome

After Phase 0 completion:
- ✅ D3D12 backend compiles without errors
- ✅ Terminal launches with D3D12 backend selected
- ✅ Renders colored background quad (NO TEXT YET - that's Phase 3)
- ✅ No crashes or hangs

**Visual Verification**: Terminal window opens, shows solid color background (probably black or the backgroundColor constant). This confirms D3D12 is working at the GPU level.

---

## Troubleshooting

### Error: "d3dx12.h not found"
- **Cause**: Step 1 not completed or file in wrong location
- **Fix**: Verify file is at `src/renderer/atlas/d3dx12.h`

### Error: "GraphicsAPI::Direct3D12 is not a member"
- **Cause**: Clean build needed after enum change
- **Fix**: Run clean build command above

### Error: "Cannot convert DXGI_FORMAT to D3D12_..."
- **Cause**: Mixing D3D11 and D3D12 types
- **Fix**: Check all includes use D3D12 types, not D3D11

### Crash on Launch with D3D12
- **Cause**: Static buffers not properly initialized
- **Fix**: Verify buffer refactoring was done correctly

### Black screen but no crash
- **Success!** This is expected - text rendering comes in Phase 3

---

## Time Estimate Breakdown

- ✅ Completed so far: 1 hour
- ⏳ Manual downloads (Steps 1-2): 10 minutes
- ⏳ Remaining automated tasks: 20 minutes
- ⏳ Build time: 15 minutes
- ⏳ Testing: 5 minutes
- **Total Phase 0**: 2 hours ✅ ON TRACK

---

## Next Phase Preview

**Phase 1 (Week 1)**: SPIR-V Toolchain Setup
- Install Vulkan SDK (DXC, spirv-opt, spirv-val)
- Build SPIRV-Cross
- Refactor shaders into modular structure
- Create compilation scripts

**Estimated Start**: Tomorrow after Phase 0 completion

---

**Document Status**: Living Document - Auto-updated by todo system
**Last Updated**: 2025-10-11 (automated)
