# Welcome to the Ultra-Riced Windows Terminal

![terminal-logos](https://github.com/microsoft/terminal/assets/91625426/333ddc76-8ab2-4eb4-a8c0-4d7b953b1179)

This repository is a fork of the official Microsoft Windows Terminal, with a focus on high performance and aesthetic customization. The project's vision is to create a "riced" version of the terminal with a new DirectX 12 renderer, SIMD optimizations, an OpenGL fallback, and a curated set of themes and fonts.

## Current Status

**This project is currently in a pre-alpha state and is not yet ready for general use.**

While a significant amount of planning and architectural design has been completed, the implementation is still in its early stages. The D3D12 backend, which is the core of the performance improvements, is **not yet functional** and cannot render text.

**Key Issues:**

*   **D3D12 backend is incomplete:** The D3D12 backend is missing critical features, including text rendering, glyph atlas management, and cursor rendering.
*   **Compilation Blockers:** There are several compilation blockers that prevent the D3D12 backend from being built.
*   **Feature Gaps:** There is a significant gap between the features of the D3D11 backend and the D3D12 backend.

A detailed breakdown of the current status can be found in the [Comprehensive Audit Summary](COMPREHENSIVE_AUDIT_SUMMARY.md).

## The Vision

The vision for this project is to create the ultimate "riced" Windows Terminal, with a focus on three key areas:

1.  **Maximum Performance:** Achieve 5-10x rendering performance improvement over the stock Windows Terminal by leveraging a modern D3D12 rendering engine, Alacritty-inspired rendering techniques, and x86-64-v3 SIMD optimizations.
2.  **Beautiful Aesthetics:** Provide a curated set of 11 beautiful themes, including popular choices like Catppuccin, Dracula, and Nord, as well as modern font support with Spline Sans Mono and Nerd Fonts integration.
3.  **Modern Features:** Implement a host of modern features, including a cross-platform OpenGL 3.3+ fallback renderer, and a production-ready, fully implemented feature set with zero placeholders.

The complete vision for the project is detailed in the [Ultra-Riced Windows Terminal Master Plan](ULTRA_RICED_TERMINAL_MASTER_PLAN.md).

## The Journey So Far (A Tale of Agents)

This project has been a collaborative effort between human developers and several AI agents, including Claude and Gemini. Each agent has contributed to different aspects of the project, from research and planning to implementation.

This multi-agent approach has been a great learning experience, but it has also led to some challenges. The initial phases of the project were characterized by a flurry of activity, with different agents working on different parts of the codebase in parallel. This resulted in a large number of design documents and roadmaps, but also a lack of a single, unified vision.

The key lesson learned from this experience is the importance of having a single, coherent plan and a clear "source of truth" for the project. This unified `README.md` is an attempt to rectify this and provide a single, clear vision for the future of the project.

## Project Refactoring: A Pure Windows 11 Future

After a thorough audit of the project's goals and the current state of the Windows development ecosystem, the decision has been made to refactor the project into a **pure Windows 11 25H2 native application**. This represents a shift away from the original cross-platform ambitions and a deep embrace of the modern Windows development stack.

### Rationale

The primary motivation for this change is to leverage the full power of the Windows platform and deliver a truly next-generation terminal experience. By focusing on a single platform, we can take advantage of the latest Windows technologies, including DirectX 12 Ultimate, DirectML, and DirectStorage, without the compromises required by a cross-platform approach.

### Core Technologies

The refactored project will be built on the following core technologies:

*   **UI Framework:** WinUI 3 with the Windows App SDK 1.6. This will provide a modern, Fluent Design-based UI that is decoupled from the OS, allowing for faster updates and more flexible deployment.
*   **Graphics API:** DirectX 12 Ultimate. This will be the sole graphics API, allowing for the implementation of cutting-edge rendering features.
*   **Machine Learning:** Windows ML (powered by DirectML). This will be used for any future AI-powered features.
*   **Storage:** DirectStorage. This will be used to accelerate the loading of assets like fonts, themes, and shaders.

### Key DirectX 12 Ultimate Features to Leverage

*   **DirectX Raytracing (DXR):** While not a primary feature for a terminal, DXR can be used for advanced visual effects like acrylic and mica materials, and other transparency and reflection effects.
*   **Variable Rate Shading (VRS):** This will be a key performance optimization, allowing the renderer to focus shading on the text glyphs themselves, while using a lower shading rate for the background and other less important areas.
*   **Mesh Shaders:** The mesh shader pipeline can be used to create a highly efficient text rendering engine, where each glyph is processed as a small meshlet.
*   **Sampler Feedback:** This can be used to optimize the glyph atlas, only loading the glyphs that are actually needed for the current view.

### Build Environment

*   **Visual Studio:** Visual Studio 2022 (latest version as of October 2025).
*   **Windows SDK:** 10.0.26100 or later.
*   **C++ Standard:** C++20. While C++23 is available in preview, C++20 will be used for its stability and full feature support.
*   **HLSL:** HLSL 2021 (Shader Model 6.8). Shaders will be compiled using the DirectX Shader Compiler (DXC) included with the Windows SDK, as Visual Studio's built-in compiler may not support the latest shader models.

### Removed Technologies

The following technologies will be removed from the project:

*   OpenGL
*   SPIR-V
*   Any other cross-platform libraries or frameworks

## The Path Forward (Unified Roadmap)

The following is a unified roadmap that synthesizes the various roadmaps and plans that have been created for the project. It prioritizes the critical path to a functional and feature-complete terminal.

**Phase 0: Critical Fixes (IMMEDIATE)**

The immediate priority is to fix the compilation blockers in the D3D12 backend and get it to a state where it can render a basic scene. This is a prerequisite for any further development.

**Phase 1: Core Text Rendering (2-3 weeks)**

The next step is to implement the core text rendering functionality in the D3D12 backend. This includes:

*   Glyph atlas system
*   DirectWrite/Direct2D integration
*   Text rendering pipeline
*   Background system

**Phase 2: Terminal Features (1-2 weeks)**

Once the core text rendering is in place, the next step is to implement the remaining terminal features, including:

*   Cursor system
*   Underlines and decorations
*   Custom shaders

**Phase 3: Modern D3D12 Features (1-2 weeks)**

With a feature-complete D3D12 backend, the focus will shift to implementing modern D3D12 features to maximize performance. This includes:

*   Ring buffer upload heaps
*   Enhanced Barriers
*   PIX + DRED
*   ExecuteIndirect
*   Variable Rate Shading

**Phase 4: WinUI 3 Integration (3-4 weeks)**

The final phase of the project will be to integrate the D3D12 renderer with a new WinUI 3-based application, replacing the existing XAML-based UI.

A more detailed breakdown of the roadmap can be found in the [Master Implementation Roadmap](MASTER_IMPLEMENTATION_ROADMAP.md).

## Key Architectural Decisions

With the project's refactoring to a pure Windows 11 native application, the following key architectural decisions have been made:

*   **DirectX 12 Ultimate as the Sole Graphics API:** The project will exclusively use DirectX 12 Ultimate, enabling the use of advanced features like Raytracing, Variable Rate Shading, Mesh Shaders, and Sampler Feedback to create a high-performance, visually rich terminal experience.
*   **WinUI 3 for the UI Framework:** The user interface will be built using WinUI 3 and the Windows App SDK, ensuring a modern, Fluent Design-based aesthetic and a clean separation between the UI and the core rendering engine.
*   **DirectStorage for Asset Loading:** All assets, including fonts, themes, and shaders, will be loaded using the DirectStorage API, providing near-instantaneous loading times and reducing CPU overhead.
*   **HLSL Shader Model 6.0+:** Shaders will be written in the latest version of HLSL, taking full advantage of the features of DirectX 12 Ultimate and the latest GPU hardware.
*   **C++20 as the Core Language:** The project will be built using C++20, leveraging modern language features like modules, coroutines, and concepts to create a more robust, maintainable, and performant codebase.

## Building and Running

**Prerequisites:**

*   Windows 11 25H2 (build >= 10.0.26100.0) or later
*   Developer Mode enabled in Windows Settings
*   PowerShell 7 or later
*   Windows SDK (10.0.26100.0) or later
*   Visual Studio 2022 (latest version) with the following workloads:
    *   Desktop Development with C++
    *   Universal Windows Platform Development
    *   C++ (v143) Universal Windows Platform Tools
*   Windows App SDK 1.6 or later

**Build Commands:**

```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild
```

**Running & Debugging:**

To debug the Windows Terminal in VS, right click on `CascadiaPackage` (in the Solution Explorer) and go to properties. In the Debug menu, change "Application process" and "Background task process" to "Native Only".

You should then be able to build & debug the Terminal project by hitting F5. Make sure to select either the "x64" or the "x86" platform.

## Contributing

We are excited to work alongside you, our amazing community, to build and enhance Windows Terminal!

***BEFORE you start work on a feature/fix***, please read & follow our [Contributor's Guide](CONTRIBUTING.md) to help avoid any wasted or duplicate effort.