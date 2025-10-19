# SCRATCHPAD: Refactoring to a Pure Windows 11 25H2 Native Application

## 1. Project Goal

Refactor the existing "Ultra-Riced Windows Terminal" project into a pure Windows 11 25H2 native application. This involves a complete shift away from cross-platform technologies like OpenGL and SPIR-V, and a deep embrace of the modern Windows development stack.

## 2. Core Technologies

*   **UI Framework:** WinUI 3 with the Windows App SDK 1.6. This will provide a modern, Fluent Design-based UI that is decoupled from the OS, allowing for faster updates and more flexible deployment.
*   **Graphics API:** DirectX 12 Ultimate. This will be the sole graphics API, allowing for the implementation of cutting-edge rendering features.
*   **Machine Learning:** Windows ML (powered by DirectML). This will be used for any future AI-powered features.
*   **Storage:** DirectStorage. This will be used to accelerate the loading of assets like fonts, themes, and shaders.

## 3. Key DirectX 12 Ultimate Features to Leverage

*   **DirectX Raytracing (DXR):** While not a primary feature for a terminal, DXR can be used for advanced visual effects like acrylic and mica materials, and other transparency and reflection effects.
*   **Variable Rate Shading (VRS):** This will be a key performance optimization, allowing the renderer to focus shading on the text glyphs themselves, while using a lower shading rate for the background and other less important areas.
*   **Mesh Shaders:** The mesh shader pipeline can be used to create a highly efficient text rendering engine, where each glyph is processed as a small meshlet.
*   **Sampler Feedback:** This can be used to optimize the glyph atlas, only loading the glyphs that are actually needed for the current view.

## 4. Build Environment

*   **Visual Studio:** Visual Studio 2022 (latest version as of October 2025).
*   **Windows SDK:** 10.0.26100 or later.
*   **C++ Standard:** C++20. While C++23 is available in preview, C++20 will be used for its stability and full feature support.
*   **HLSL:** HLSL 2021 (Shader Model 6.8). Shaders will be compiled using the DirectX Shader Compiler (DXC) included with the Windows SDK, as Visual Studio's built-in compiler may not support the latest shader models.

## 5. Removed Technologies

The following technologies will be removed from the project:

*   OpenGL
*   SPIR-V
*   Any other cross-platform libraries or frameworks

## 6. New `README.md` Section

A new section will be added to the `README.md` file, titled "Project Refactoring: A Pure Windows 11 Future". This section will outline the new direction of the project and provide a high-level overview of the new architecture and technology stack.
