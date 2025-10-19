/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WinUIRenderer.cpp

Abstract:
- This module provides the implementation for the WinUI renderer.

Author(s):
- Your Name (your-github-alias) 14-Oct-2025

--*/

#include "precomp.h"
#include "WinUIRenderer.h"
#include "../d3d12/D3D12Engine.h"

using namespace Microsoft::Console::Render;

WinUIRenderer::WinUIRenderer()
{
    _d3d12Engine = std::make_unique<D3D12Engine>();
}

WinUIRenderer::~WinUIRenderer()
{
}

[[nodiscard]] HRESULT WinUIRenderer::Initialize(HWND hwnd)
{
    _hwnd = hwnd;
    RETURN_IF_FAILED(_d3d12Engine->SetHwnd(hwnd));
    RETURN_IF_FAILED(_d3d12Engine->Initialize());
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::StartPaint()
{
    return _d3d12Engine->StartPaint();
}

[[nodiscard]] HRESULT WinUIRenderer::EndPaint()
{
    return S_OK;
}

[[nodiscard]] bool WinUIRenderer::RequiresContinuousRedraw()
{
    return false;
}

void WinUIRenderer::WaitUntilCanRender()
{
}

[[nodiscard]] HRESULT WinUIRenderer::PaintBackground()
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::PaintBufferLine(const std::span<const Cluster> /*clusters*/, const til::point /*coord*/, const bool /*fTrimLeft*/, const bool /*lineWrapped*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::PaintBufferGridLines(const GridLineSet /*lines*/, const COLORREF /*color*/, const size_t /*cchLine*/, const til::point /*coordTarget*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::PaintSelection(const til::rect& /*rect*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::PaintCursor(const CursorOptions& /*options*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::UpdateDrawingBrushes(const TextAttribute& /*textAttributes*/, const RenderSettings& /*renderSettings*/, const gsl::not_null<ID2D1RenderTarget*> /*d2dRenderTarget*/, const bool /*isErase*/, const bool /*isSettingDefaultBrushes*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::UpdateFont(const FontInfo& /*fontInfo*/, FontInfo& /*fontInfoActual*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::UpdateDpi(const int /*iDpi*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::UpdateViewport(const til::inclusive_rect& /*srNewViewport*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::GetProposedFont(const FontInfo& /*fontInfo*/, FontInfo& /*fontInfoActual*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::GetDirtyArea(std::span<const til::rect>& /*area*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::GetFontSize(_Out_ til::size& /*pFontSize*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::IsGlyphWide(const std::wstring_view /*glyph*/, _Out_ bool* const /*pIsWide*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::ScrollFrame()
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::InvalidateTitle(const std::wstring_view /*proposedTitle*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT WinUIRenderer::UpdateTitle(const std::wstring_view /*newTitle*/)
{
    return S_OK;
}
