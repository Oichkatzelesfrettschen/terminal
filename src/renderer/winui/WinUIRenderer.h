/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WinUIRenderer.h

Abstract:
- This module provides the declaration for the WinUI renderer.

Author(s):
- Your Name (your-github-alias) 14-Oct-2025

--*/

#pragma once

#include "../inc/IRenderEngine.hpp"

namespace Microsoft::Console::Render
{
    class WinUIRenderer : public IRenderEngine
    {
    public:
        WinUIRenderer();
        ~WinUIRenderer();

        [[nodiscard]] HRESULT Initialize(HWND hwnd);

        // IRenderEngine methods
        [[nodiscard]] HRESULT StartPaint() override;
        [[nodiscard]] HRESULT EndPaint() override;
        [[nodiscard]] bool RequiresContinuousRedraw() override;
        void WaitUntilCanRender() override;
        [[nodiscard]] HRESULT PaintBackground() override;
        [[nodiscard]] HRESULT PaintBufferLine(const std::span<const Cluster> clusters, const til::point coord, const bool fTrimLeft, const bool lineWrapped) override;
        [[nodiscard]] HRESULT PaintBufferGridLines(const GridLineSet lines, const COLORREF color, const size_t cchLine, const til::point coordTarget) override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) override;
        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) override;
        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes, const RenderSettings& renderSettings, const gsl::not_null<ID2D1RenderTarget*> d2dRenderTarget, const bool isErase, const bool isSettingDefaultBrushes) override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfo& fontInfo, FontInfo& fontInfoActual) override;
        [[nodiscard]] HRESULT UpdateDpi(const int iDpi) override;
        [[nodiscard]] HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) override;
        [[nodiscard]] HRESULT GetProposedFont(const FontInfo& fontInfo, FontInfo& fontInfoActual) override;
        [[nodiscard]] HRESULT GetDirtyArea(std::span<const til::rect>& area) override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ til::size& pFontSize) override;
        [[nodiscard]] HRESULT IsGlyphWide(const std::wstring_view glyph, _Out_ bool* const pIsWide) override;
        [[nodiscard]] HRESULT ScrollFrame() override;
        [[nodiscard]] HRESULT InvalidateTitle(const std::wstring_view proposedTitle) override;
        [[nodiscard]] HRESULT UpdateTitle(const std::wstring_view newTitle) override;

    private:
        HWND _hwnd;
        std::unique_ptr<D3D12Engine> _d3d12Engine;
    };
}
