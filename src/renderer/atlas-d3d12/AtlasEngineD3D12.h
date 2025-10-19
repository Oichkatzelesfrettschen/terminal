#pragma once

#include "..\..\..\inc\IRenderEngine.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

namespace Microsoft::Console::Render
{
    class AtlasEngineD3D12 : public IRenderEngine
    {
    public:
        AtlasEngineD3D12();
        ~AtlasEngineD3D12();

        // IRenderEngine methods
        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        [[nodiscard]] bool RequiresContinuousRedraw() noexcept override;
        void WaitUntilCanRender() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;
        [[nodiscard]] HRESULT ScrollFrame() noexcept override;
        [[nodiscard]] HRESULT Invalidate(const til::rect* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const til::rect* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const til::rect* prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(std::span<const til::rect> rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const til::point* pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateTitle(std::wstring_view proposedTitle) noexcept override;
        [[nodiscard]] HRESULT PrepareRenderInfo(const RenderFrameInfo& info) noexcept override;
        [[nodiscard]] HRESULT ResetLineTransform() noexcept override;
        [[nodiscard]] HRESULT PrepareLineTransform(LineRendition lineRendition, til::CoordType targetRow, til::CoordType viewportLeft) noexcept override;
        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(std::span<const Cluster> clusters, til::point coord, bool fTrimLeft, bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(GridLineSet lines, COLORREF color, size_t cchLine, til::point coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) noexcept override;
        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;
        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes, const RenderSettings& renderSettings, gsl::not_null<IRenderData*> pData, bool usingSoftFont, bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired, FontInfo& FontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateSoftFont(std::span<const uint16_t> bitPattern, til::size cellSize, size_t centeringHint) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept override;
        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& FontInfoDesired, FontInfo& FontInfo, int iDpi) noexcept override;
        [[nodiscard]] HRESULT GetDirtyArea(std::span<const til::rect>& area) noexcept override;
        [[nodiscard]] HRESULT GetFontSize(til::size* pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(std::wstring_view glyph, bool* pResult) noexcept override;
        [[nodiscard]] HRESULT UpdateTitle(std::wstring_view newTitle) noexcept override;

        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo) noexcept override;

        [[nodiscard]] HRESULT SetHwnd(HWND hwnd) noexcept;

    private:
        void _mapReplacementCharacter(u32 from, u32 to, ShapedRowD3D12& row);
        [[nodiscard]] bool _updateWithNearbyFontCollection() noexcept;
        [[nodiscard]] HRESULT _updateFont(const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, const std::unordered_map<std::wstring_view, float>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept;
        void _resolveFontMetrics(const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, FontSettings* fontMetrics);
        void _mapComplex(IDWriteFontFace2* mappedFontFace, u32 idx, u32 length, ShapedRowD3D12& row);
        void _mapCharacters(const wchar_t* text, u32 textLength, u32* mappedLength, IDWriteFontFace2** mappedFontFace);
        void _mapRegularText(size_t offBeg, size_t offEnd);
        void _mapBuiltinGlyphs(size_t offBeg, size_t offEnd);
                                                        struct RenderingPayload
                                                        {
                                                            std::vector<ShapedRowD3D12> rows;
                                                            FontInfo* font;
                                                            std::wstring userLocaleName;
                                                            wil::com_ptr<IDWriteFactory> dwriteFactory;
                                                            wil::com_ptr<IDWriteFactory4> dwriteFactory4;
                                                            wil::com_ptr<IDWriteTextAnalyzer1> textAnalyzer;
                                                            GenerationalSettings s;
                                                            std::function<void(HRESULT, wil::zwstring_view)> warningCallback;
                                                        } _p;
                                                
                                                        struct ApiState
                                                        {
                                                            std::vector<wchar_t> bufferLine;
                                                            std::vector<uint16_t> bufferLineColumn;
                                                            til::point lastPaintBufferLineCoord;
                                                            FontRelevantAttributes attributes;
                                                            std::vector<TextAnalysisSinkResult> analysisResults;
                                                            Buffer<uint16_t> clusterMap;
                                                            Buffer<DWRITE_SHAPING_TEXT_PROPERTIES> textProps;
                                                            Buffer<uint16_t> glyphIndices;
                                                            Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProps;
                                                                        Buffer<float> glyphAdvances;
                                                                        Buffer<DWRITE_GLYPH_OFFSET> glyphOffsets;
                                                                        wil::com_ptr<IDWriteFontFallback> systemFontFallback;
                                                                        wil::com_ptr<IDWriteFontFace2> replacementCharacterFontFace;
                                                                        u16 replacementCharacterGlyphIndex = 0;
                                                                        bool replacementCharacterLookedUp = false;
                                                                    } _api;        void _flushBufferLine();
        HWND _hwnd;
        void CreateSwapChain();
        Microsoft::WRL::ComPtr<ID3D12Device> _device;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> _swapChain;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> _commandQueue;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _commandAllocator;
    };
}
