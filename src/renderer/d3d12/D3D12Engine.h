/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- D3D12Engine.h

Abstract:
- This module provides the declaration for the D3D12 rendering engine.

Author(s):
- Your Name (your-github-alias) 14-Oct-2025

--*/

#pragma once

#include "../inc/IRenderEngine.hpp"

namespace Microsoft::Console::Render
{
    class D3D12Engine : public IRenderEngine
    {
    public:
        D3D12Engine();
        ~D3D12Engine();

        [[nodiscard]] HRESULT Initialize();

        [[nodiscard]] HRESULT SetHwnd(HWND hwnd);
        void SetBackgroundColor(COLORREF color);

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
        [[nodiscard]] HRESULT _createRootSignature();
        [[nodiscard]] HRESULT _createPipelineState();
        [[nodiscard]] HRESULT _createVertexBuffer();
        [[nodiscard]] HRESULT _createShaderResources();
        void _setRenderTarget();
        void _setViewport();
        void _drawQuads(const std::span<const D3D12_VERTEX_BUFFER_VIEW> vertexBuffers, const std::span<const D3D12_INDEX_BUFFER_VIEW> indexBuffers);
        HWND _hwnd;
        Microsoft::WRL::ComPtr<ID3D12Resource> _vertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;
        Microsoft::WRL::ComPtr<ID3D12Resource> _indexBuffer;
        D3D12_INDEX_BUFFER_VIEW _indexBufferView;
        COLORREF _backgroundColor;
        static const UINT FrameCount = 2;

        Microsoft::WRL::ComPtr<ID3D12Device> _device;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> _commandQueue;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> _swapChain;
        Microsoft::WRL::ComPtr<ID3D12Resource> _renderTargets[FrameCount];
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _commandAllocator;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _commandList;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtvHeap;
        UINT _rtvDescriptorSize;

        UINT _frameIndex;
        HANDLE _fenceEvent;
        Microsoft::WRL::ComPtr<ID3D12Fence> _fence;
        UINT64 _fenceValue;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvHeap;
        Microsoft::WRL::ComPtr<ID3D12Resource> _constantBuffer;
        UINT8* _pCbvDataBegin;

        D3D12_VIEWPORT _viewport;
        D3D12_RECT _scissorRect;
    };
}
