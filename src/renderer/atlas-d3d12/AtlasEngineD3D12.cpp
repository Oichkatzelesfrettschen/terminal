#include "pch.h"
#include "AtlasEngineD3D12.h"

using namespace Microsoft::Console::Render;

AtlasEngineD3D12::AtlasEngineD3D12() : 
    _device(nullptr),
    _swapChain(nullptr),
    _commandQueue(nullptr),
    _commandAllocator(nullptr)
{
#if defined(_DEBUG)
    Microsoft::WRL::ComPtr<ID3D12Debug>
 debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif

    THROW_IF_FAILED(D3D12CreateDevice(
        nullptr, // default adapter
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&_device)
    ));

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    THROW_IF_FAILED(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_commandQueue)));

    THROW_IF_FAILED(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator)));

    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(_p.dwriteFactory.addressof())));
    _p.dwriteFactory4 = _p.dwriteFactory.try_query<IDWriteFactory4>();

    wil::com_ptr<IDWriteTextAnalyzer> textAnalyzer;
    THROW_IF_FAILED(_p.dwriteFactory->CreateTextAnalyzer(textAnalyzer.addressof()));
    _p.textAnalyzer = textAnalyzer.query<IDWriteTextAnalyzer1>();
}

AtlasEngineD3D12::~AtlasEngineD3D12()
{
}

[[nodiscard]] HRESULT AtlasEngineD3D12::SetHwnd(HWND hwnd) noexcept
{
    _hwnd = hwnd;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngineD3D12::StartPaint() noexcept
{
    CreateSwapChain();
    return S_OK;
}

void AtlasEngineD3D12::CreateSwapChain()
{
    if (_swapChain)
    {
        return;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
    THROW_IF_FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = 0;
    swapChainDesc.Height = 0;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    THROW_IF_FAILED(dxgiFactory->CreateSwapChainForHwnd(
        _commandQueue.Get(),
        _hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));

    THROW_IF_FAILED(swapChain.As(&_swapChain));
}

[[nodiscard]] HRESULT AtlasEngineD3D12::UpdateFont(const FontInfoDesired& fontInfoDesired, _Out_ FontInfo& fontInfo) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngineD3D12::_updateFont(const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, const std::unordered_map<std::wstring_view, float>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept
{
    std::vector<DWRITE_FONT_FEATURE> fontFeatures;
    if (!features.empty())
    {
        fontFeatures.reserve(features.size() + 3);

        fontFeatures.emplace_back(DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1);
        fontFeatures.emplace_back(DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES, 1);
        fontFeatures.emplace_back(DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES, 1);

        for (const auto& p : features)
        {
            if (p.first.size() == 4)
            {
                const auto s = p.first.data();
                const auto v = static_cast<UINT32>(std::max(0l, lrintf(p.second)));
                switch (const auto tag = DWRITE_MAKE_FONT_FEATURE_TAG(s[0], s[1], s[2], s[3]))
                {
                case DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES:
                    fontFeatures[0].parameter = v;
                    break;
                case DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES:
                    fontFeatures[1].parameter = v;
                    break;
                case DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES:
                    fontFeatures[2].parameter = v;
                    break;
                default:
                    fontFeatures.emplace_back(tag, v);
                    break;
                }
            }
        }
    }

    std::vector<DWRITE_FONT_AXIS_VALUE> fontAxisValues;
    if (!axes.empty())
    {
        fontAxisValues.reserve(axes.size() + 3);

        fontAxisValues.emplace_back(DWRITE_FONT_AXIS_TAG_WEIGHT, -1.0f);
        fontAxisValues.emplace_back(DWRITE_FONT_AXIS_TAG_ITALIC, -1.0f);
        fontAxisValues.emplace_back(DWRITE_FONT_AXIS_TAG_SLANT, -1.0f);

        for (const auto& p : axes)
        {
            if (p.first.size() == 4)
            {
                const auto s = p.first.data();
                switch (const auto tag = DWRITE_MAKE_FONT_AXIS_TAG(s[0], s[1], s[2], s[3]))
                {
                case DWRITE_FONT_AXIS_TAG_WEIGHT:
                    fontAxisValues[0].value = p.second;
                    break;
                case DWRITE_FONT_AXIS_TAG_ITALIC:
                    fontAxisValues[1].value = p.second;
                    break;
                case DWRITE_FONT_AXIS_TAG_SLANT:
                    fontAxisValues[2].value = p.second;
                    break;
                default:
                    fontAxisValues.emplace_back(tag, p.second);
                    break;
                }
            }
        }
    }

    const auto font = _p.s.write()->font.write();
    _resolveFontMetrics(fontInfoDesired, fontInfo, font);
    font->fontFeatures = std::move(fontFeatures);
    font->fontAxisValues = std::move(fontAxisValues);

    return S_OK;
}

[[nodiscard]] bool AtlasEngineD3D12::_updateWithNearbyFontCollection() noexcept
{
    wil::com_ptr<IDWriteFontCollection> collection;
    try
    {
        collection = FontCache::GetCached();
    }
    CATCH_LOG();

    if (!collection || _p.font->fontCollection == collection)
    {
        return false;
    }

    _p.font->fontCollection = std::move(collection);
    return true;
}

void AtlasEngineD3D12::_resolveFontMetrics(const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, FontSettings* fontMetrics)
{
    const auto& faceName = fontInfoDesired.GetFaceName();
    const auto requestedFamily = fontInfoDesired.GetFamily();
    auto requestedWeight = fontInfoDesired.GetWeight();
    auto fontSize = std::clamp(fontInfoDesired.GetFontSize(), 1.0f, 100.0f);
    auto requestedSize = fontInfoDesired.GetEngineSize();

    if (!requestedSize.height)
    {
        fontSize = 12.0f;
        requestedSize = { 0, 12 };
    }
    if (!requestedWeight)
    {
        requestedWeight = DWRITE_FONT_WEIGHT_NORMAL;
    }

    auto fontCollection = _p.font->fontCollection;
    if (!fontCollection)
    {
        THROW_IF_FAILED(_p.dwriteFactory->GetSystemFontCollection(fontCollection.addressof(), FALSE));
    }

    std::wstring primaryFontName;
    std::wstring missingFontNames;
    wil::com_ptr<IDWriteFontFamily> primaryFontFamily;
    wil::com_ptr<IDWriteFontFallbackBuilder> fontFallbackBuilder;

    til::iterate_font_families(faceName, [&](std::wstring&& fontName) {
        u32 index = 0;
        BOOL exists = false;
        THROW_IF_FAILED(fontCollection->FindFamilyName(fontName.c_str(), &index, &exists));

        if (!exists && _updateWithNearbyFontCollection())
        {
            fontCollection = _p.font->fontCollection;
            THROW_IF_FAILED(fontCollection->FindFamilyName(fontName.c_str(), &index, &exists));
        }

        if (!exists)
        {
            if (!missingFontNames.empty())
            {
                missingFontNames.append(L", ");
            }
            missingFontNames.append(fontName);
            return;
        }

        if (!primaryFontFamily)
        {
            primaryFontName = std::move(fontName);
            THROW_IF_FAILED(fontCollection->GetFontFamily(index, primaryFontFamily.addressof()));
        }
        else
        {
            if (!fontFallbackBuilder)
            {
                THROW_IF_FAILED(_p.dwriteFactory->CreateFontFallbackBuilder(fontFallbackBuilder.addressof()));
            }

            static constexpr DWRITE_UNICODE_RANGE fullRange{ 0, 0x10FFFF };
            auto fontNamePtr = fontName.c_str();
            THROW_IF_FAILED(fontFallbackBuilder->AddMapping(
                /* ranges                 */ &fullRange,
                /* rangesCount            */ 1,
                /* targetFamilyNames      */ &fontNamePtr,
                /* targetFamilyNamesCount */ 1,
                /* fontCollection         */ fontCollection.get(),
                /* localeName             */ nullptr,
                /* baseFamilyName         */ nullptr,
                /* scale                  */ 1.0f));
        }
    });

    // if (!missingFontNames.empty() && _p.warningCallback)
    // {
    //     _p.warningCallback(DWRITE_E_NOFONT, missingFontNames);
    // }

    if (!primaryFontFamily)
    {
        primaryFontName = L"Consolas";

        u32 index = 0;
        BOOL exists = false;
        THROW_IF_FAILED(fontCollection->FindFamilyName(primaryFontName.c_str(), &index, &exists));
        THROW_HR_IF(DWRITE_E_NOFONT, !exists);

        THROW_IF_FAILED(fontCollection->GetFontFamily(index, primaryFontFamily.addressof()));
    }

    auto fontFallback = _api.systemFontFallback;
    if (fontFallbackBuilder)
    {
        THROW_IF_FAILED(fontFallbackBuilder->AddMappings(_api.systemFontFallback.get()));
        THROW_IF_FAILED(fontFallbackBuilder->CreateFontFallback(fontFallback.put()));
    }

    wil::com_ptr<IDWriteFont> primaryFont;
    THROW_IF_FAILED(primaryFontFamily->GetFirstMatchingFont(static_cast<DWRITE_FONT_WEIGHT>(requestedWeight), DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, primaryFont.addressof()));

    wil::com_ptr<IDWriteFontFace> primaryFontFace;
    THROW_IF_FAILED(primaryFont->CreateFontFace(primaryFontFace.addressof()));

    DWRITE_FONT_METRICS metrics{};
    primaryFontFace->GetMetrics(&metrics);

    const auto dpi = static_cast<f32>(_p.font->dpi);
    const auto fontSizeInPx = fontSize / 72.0f * dpi;

    const auto designUnitsPerPx = fontSizeInPx / static_cast<f32>(metrics.designUnitsPerEm);
    const auto ascent = static_cast<f32>(metrics.ascent) * designUnitsPerPx;
    const auto descent = static_cast<f32>(metrics.descent) * designUnitsPerPx;
    const auto lineGap = static_cast<f32>(metrics.lineGap) * designUnitsPerPx;
    const auto underlinePosition = static_cast<f32>(-metrics.underlinePosition) * designUnitsPerPx;
    const auto underlineThickness = static_cast<f32>(metrics.underlineThickness) * designUnitsPerPx;
    const auto strikethroughPosition = static_cast<f32>(-metrics.strikethroughPosition) * designUnitsPerPx;
    const auto strikethroughThickness = static_cast<f32>(metrics.strikethroughThickness) * designUnitsPerPx;
    const auto advanceHeight = ascent + descent + lineGap;

    auto advanceWidth = 0.5f * fontSizeInPx;
    {
        static constexpr u32 codePoint = '0';

        u16 glyphIndex;
        THROW_IF_FAILED(primaryFontFace->GetGlyphIndicesW(&codePoint, 1, &glyphIndex));

        if (glyphIndex)
        {
            DWRITE_GLYPH_METRICS glyphMetrics{};
            THROW_IF_FAILED(primaryFontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE));
            advanceWidth = static_cast<f32>(glyphMetrics.advanceWidth) * designUnitsPerPx;
        }
    }

    auto adjustedWidth = std::roundf(fontInfoDesired.GetCellWidth().Resolve(advanceWidth, dpi, fontSizeInPx, advanceWidth));
    auto adjustedHeight = std::roundf(fontInfoDesired.GetCellHeight().Resolve(advanceHeight, dpi, fontSizeInPx, advanceWidth));

    adjustedWidth = std::max(1.0f, adjustedWidth);
    adjustedHeight = std::max(1.0f, adjustedHeight);

    const auto baseline = std::roundf(ascent + (lineGap + adjustedHeight - advanceHeight) / 2.0f);
    const auto underlinePos = std::roundf(baseline + underlinePosition);
    const auto underlineWidth = std::max(1.0f, std::roundf(underlineThickness));
    const auto strikethroughPos = std::roundf(baseline + strikethroughPosition);
    const auto strikethroughWidth = std::max(1.0f, std::roundf(strikethroughThickness));
    const auto doubleUnderlineWidth = std::max(1.0f, std::roundf(underlineThickness / 2.0f));
    const auto thinLineWidth = std::max(1.0f, std::roundf(std::max(adjustedWidth / 16.0f, adjustedHeight / 32.0f)));

    auto doubleUnderlinePosBottom = underlinePos + underlineWidth - doubleUnderlineWidth;
    auto doubleUnderlinePosTop = std::roundf((baseline + doubleUnderlinePosBottom - doubleUnderlineWidth) / 2.0f);
    doubleUnderlinePosTop = std::max(doubleUnderlinePosTop, baseline + doubleUnderlineWidth);
    const auto doubleUnderlineGap = std::max(1.0f, std::roundf(1.2f / 72.0f * dpi));
    doubleUnderlinePosBottom = std::max(doubleUnderlinePosBottom, doubleUnderlinePosTop + doubleUnderlineGap + doubleUnderlineWidth);
    doubleUnderlinePosBottom = std::min(doubleUnderlinePosBottom, adjustedHeight - doubleUnderlineWidth);

    const auto cellWidth = gsl::narrow<u16>(lrintf(adjustedWidth));
    const auto cellHeight = gsl::narrow<u16>(lrintf(adjustedHeight));

    {
        til::size coordSize;
        coordSize.width = cellWidth;
        coordSize.height = cellHeight;

        if (requestedSize.width == 0)
        {
            requestedSize.width = gsl::narrow_cast<til::CoordType>(lrintf(fontSize / cellHeight * cellWidth));
        }

        fontInfo.SetFromEngine(primaryFontName, requestedFamily, requestedWeight, false, coordSize, requestedSize);
    }

    if (fontMetrics)
    {
        const auto fontWeightU16 = gsl::narrow_cast<u16>(requestedWeight);
        const auto advanceWidthU16 = gsl::narrow_cast<u16>(lrintf(advanceWidth));
        const auto baselineU16 = gsl::narrow_cast<u16>(lrintf(baseline));
        const auto descenderU16 = gsl::narrow_cast<u16>(cellHeight - baselineU16);
        const auto thinLineWidthU16 = gsl::narrow_cast<u16>(lrintf(thinLineWidth));

        const auto gridBottomPositionU16 = gsl::narrow_cast<u16>(cellHeight - thinLineWidth);
        const auto gridRightPositionU16 = gsl::narrow_cast<u16>(cellWidth - thinLineWidth);

        const auto underlinePosU16 = gsl::narrow_cast<u16>(lrintf(underlinePos));
        const auto underlineWidthU16 = gsl::narrow_cast<u16>(lrintf(underlineWidth));
        const auto strikethroughPosU16 = gsl::narrow_cast<u16>(lrintf(strikethroughPos));
        const auto strikethroughWidthU16 = gsl::narrow_cast<u16>(lrintf(strikethroughWidth));
        const auto doubleUnderlinePosTopU16 = gsl::narrow_cast<u16>(lrintf(doubleUnderlinePosTop));
        const auto doubleUnderlinePosBottomU16 = gsl::narrow_cast<u16>(lrintf(doubleUnderlinePosBottom));
        const auto doubleUnderlineWidthU16 = gsl::narrow_cast<u16>(lrintf(doubleUnderlineWidth));

        fontMetrics->fontCollection = std::move(fontCollection);
        fontMetrics->fontFallback = std::move(fontFallback);
        fontMetrics->fontFallback.try_query_to(fontMetrics->fontFallback1.put());
        fontMetrics->fontName = std::move(primaryFontName);
        fontMetrics->fontSize = fontSizeInPx;
        fontMetrics->cellSize = { cellWidth, cellHeight };
        fontMetrics->fontWeight = fontWeightU16;
        fontMetrics->advanceWidth = advanceWidthU16;
        fontMetrics->baseline = baselineU16;
        fontMetrics->descender = descenderU16;
        fontMetrics->thinLineWidth = thinLineWidthU16;

        fontMetrics->gridTop = { 0, thinLineWidthU16 };
        fontMetrics->gridBottom = { gridBottomPositionU16, thinLineWidthU16 };
        fontMetrics->gridLeft = { 0, thinLineWidthU16 };
        fontMetrics->gridRight = { gridRightPositionU16, thinLineWidthU16 };

        fontMetrics->underline = { underlinePosU16, underlineWidthU16 };
        fontMetrics->strikethrough = { strikethroughPosU16, strikethroughWidthU16 };
        fontMetrics->doubleUnderline[0] = { doubleUnderlinePosTopU16, doubleUnderlineWidthU16 };
        fontMetrics->doubleUnderline[1] = { doubleUnderlinePosBottomU16, doubleUnderlineWidthU16 };
        fontMetrics->overline = { 0, underlineWidthU16 };

        fontMetrics->builtinGlyphs = fontInfoDesired.GetEnableBuiltinGlyphs();
        fontMetrics->colorGlyphs = fontInfoDesired.GetEnableColorGlyphs();
    }
}

void AtlasEngineD3D12::_mapReplacementCharacter(u32 from, u32 to, ShapedRowD3D12& row)
{
    if (!_api.replacementCharacterLookedUp)
    {
        bool succeeded = false;

        u32 mappedLength = 0;
        _mapCharacters(L"\uFFFD", 1, &mappedLength, _api.replacementCharacterFontFace.put());

        if (mappedLength == 1)
        {
            static constexpr u32 codepoint = 0xFFFD;
            succeeded = SUCCEEDED(_api.replacementCharacterFontFace->GetGlyphIndicesW(&codepoint, 1, &_api.replacementCharacterGlyphIndex));
        }

        if (!succeeded)
        {
            _api.replacementCharacterFontFace.reset();
            _api.replacementCharacterGlyphIndex = 0;
        }

        _api.replacementCharacterLookedUp = true;
    }

    if (!_api.replacementCharacterFontFace)
    {
        return;
    }

    auto pos = from;
    auto col1 = _api.bufferLineColumn[from];
    auto initialIndicesCount = row.glyphIndices.size();
    // const auto shift = gsl::narrow_cast<u8>(row.lineRendition != LineRendition::SingleWidth);
    // const auto colors = _p.foregroundBitmap.begin() + _p.colorBitmapRowStride * _api.lastPaintBufferLineCoord.y;

    while (pos < to)
    {
        const auto col2 = _api.bufferLineColumn[++pos];
        if (col1 == col2)
        {
            continue;
        }

        row.glyphIndices.emplace_back(_api.replacementCharacterGlyphIndex);
        row.glyphAdvances.emplace_back(static_cast<f32>((col2 - col1) * _p.s->font->cellSize.x));
        row.glyphOffsets.emplace_back();
        // row.colors.emplace_back(colors[static_cast<size_t>(col1) << shift]);

        col1 = col2;
    }

    {
        const auto indicesCount = row.glyphIndices.size();
        const auto fontFace = _api.replacementCharacterFontFace.get();

        if (indicesCount > initialIndicesCount)
        {
            // row.mappings.emplace_back(fontFace, gsl::narrow_cast<u32>(initialIndicesCount), gsl::narrow_cast<u32>(indicesCount));
        }
    }
}

void AtlasEngineD3D12::_mapComplex(IDWriteFontFace2* mappedFontFace, u32 idx, u32 length, ShapedRowD3D12& row)
{
    _api.analysisResults.clear();

    TextAnalysisSource analysisSource{ _p.userLocaleName.c_str(), _api.bufferLine.data(), gsl::narrow<UINT32>(_api.bufferLine.size()) };
    TextAnalysisSink analysisSink{ _api.analysisResults };
    THROW_IF_FAILED(_p.textAnalyzer->AnalyzeScript(&analysisSource, idx, length, &analysisSink));

    for (const auto& a : _api.analysisResults)
    {
        u32 actualGlyphCount = 0;

#pragma warning(push)
#pragma warning(disable : 26494) // Variable '...' is uninitialized. Always initialize an object (type.5).
        DWRITE_TYPOGRAPHIC_FEATURES feature;
        const DWRITE_TYPOGRAPHIC_FEATURES* features;
        u32 featureRangeLengths;
#pragma warning(pop)
        u32 featureRanges = 0;

        if (!_p.font->fontFeatures.empty())
        {
            feature.features = const_cast<DWRITE_FONT_FEATURE*>(_p.font->fontFeatures.data());
            feature.featureCount = gsl::narrow_cast<u32>(_p.font->fontFeatures.size());
            features = &feature;
            featureRangeLengths = a.textLength;
            featureRanges = 1;
        }

        if (_api.clusterMap.size() <= a.textLength)
        {
            _api.clusterMap = Buffer<u16>{ static_cast<size_t>(a.textLength) + 1 };
            _api.textProps = Buffer<DWRITE_SHAPING_TEXT_PROPERTIES>{ a.textLength };
        }

        for (auto retry = 0;;)
        {
            const auto hr = _p.textAnalyzer->GetGlyphs(
                /* textString          */ _api.bufferLine.data() + a.textPosition,
                /* textLength          */ a.textLength,
                /* fontFace            */ mappedFontFace,
                /* isSideways          */ false,
                /* isRightToLeft       */ 0,
                /* scriptAnalysis      */ &a.analysis,
                /* localeName          */ _p.userLocaleName.c_str(),
                /* numberSubstitution  */ nullptr,
                /* features            */ &features,
                /* featureRangeLengths */ &featureRangeLengths,
                /* featureRanges       */ featureRanges,
                /* maxGlyphCount       */ gsl::narrow_cast<u32>(_api.glyphIndices.size()),
                /* clusterMap          */ _api.clusterMap.data(),
                /* textProps           */ _api.textProps.data(),
                /* glyphIndices        */ _api.glyphIndices.data(),
                /* glyphProps          */ _api.glyphProps.data(),
                /* actualGlyphCount    */ &actualGlyphCount);

            if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && ++retry < 8)
            {
                auto size = _api.glyphIndices.size();
                size = size + (size >> 1);
                Expects(size > _api.glyphIndices.size());
                _api.glyphIndices = Buffer<u16>{ size };
                _api.glyphProps = Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES>{ size };
                continue;
            }

            THROW_IF_FAILED(hr);
            break;
        }

        if (_api.glyphAdvances.size() < actualGlyphCount)
        {
            auto size = _api.glyphAdvances.size();
            size = size + (size >> 1);
            size = std::max<size_t>(size, actualGlyphCount);
            _api.glyphAdvances = Buffer<f32>{ size };
            _api.glyphOffsets = Buffer<DWRITE_GLYPH_OFFSET>{ size };
        }

        THROW_IF_FAILED(_p.textAnalyzer->GetGlyphPlacements(
            /* textString          */ _api.bufferLine.data() + a.textPosition,
            /* clusterMap          */ _api.clusterMap.data(),
            /* textProps           */ _api.textProps.data(),
            /* textLength          */ a.textLength,
            /* glyphIndices        */ _api.glyphIndices.data(),
            /* glyphProps          */ _api.glyphProps.data(),
            /* glyphCount          */ actualGlyphCount,
            /* fontFace            */ mappedFontFace,
            /* fontEmSize          */ _p.font->fontSize,
            /* isSideways          */ false,
            /* isRightToLeft       */ 0,
            /* scriptAnalysis      */ &a.analysis,
            /* localeName          */ _p.userLocaleName.c_str(),
            /* features            */ &features,
            /* featureRangeLengths */ &featureRangeLengths,
            /* featureRanges       */ featureRanges,
            /* glyphAdvances       */ _api.glyphAdvances.data(),
            /* glyphOffsets        */ _api.glyphOffsets.data()));

        _api.clusterMap[a.textLength] = gsl::narrow_cast<u16>(actualGlyphCount);

        // const auto shift = gsl::narrow_cast<u8>(row.lineRendition != LineRendition::SingleWidth);
        // const auto colors = _p.foregroundBitmap.begin() + _p.colorBitmapRowStride * _api.lastPaintBufferLineCoord.y;
        auto prevCluster = _api.clusterMap[0];
        size_t beg = 0;

        for (size_t i = 1; i <= a.textLength; ++i)
        {
            const auto nextCluster = _api.clusterMap[i];
            if (prevCluster == nextCluster)
            {
                continue;
            }

            const size_t col1 = _api.bufferLineColumn[a.textPosition + beg];
            const size_t col2 = _api.bufferLineColumn[a.textPosition + i];
            // const auto fg = colors[col1 << shift];

            const auto expectedAdvance = (col2 - col1) * _p.font->cellSize.x;
            f32 actualAdvance = 0;
            for (auto j = prevCluster; j < nextCluster; ++j)
            {
                actualAdvance += _api.glyphAdvances[j];
            }
            _api.glyphAdvances[nextCluster - 1] += expectedAdvance - actualAdvance;

            // row.colors.insert(row.colors.end(), nextCluster - prevCluster, fg);

            prevCluster = nextCluster;
            beg = i;
        }

        row.glyphIndices.insert(row.glyphIndices.end(), _api.glyphIndices.begin(), _api.glyphIndices.begin() + actualGlyphCount);
        row.glyphAdvances.insert(row.glyphAdvances.end(), _api.glyphAdvances.begin(), _api.glyphAdvances.begin() + actualGlyphCount);
        row.glyphOffsets.insert(row.glyphOffsets.end(), _api.glyphOffsets.begin(), _api.glyphOffsets.begin() + actualGlyphCount);
    }
}

void AtlasEngineD3D12::_mapCharacters(const wchar_t* text, u32 textLength, u32* mappedLength, IDWriteFontFace2** mappedFontFace)
{
    TextAnalysisSource analysisSource{ _p.userLocaleName.c_str(), text, textLength };
    // const auto& textFormatAxis = _api.textFormatAxes[static_cast<size_t>(_api.attributes)];

    // We don't read from scale anyways.
#pragma warning(suppress : 26494) // Variable 'scale' is uninitialized. Always initialize an object (type.5).
    f32 scale;

    // if (textFormatAxis)
    // {
    //     THROW_IF_FAILED(_p.s->font->fontFallback1->MapCharacters(
    //         /* analysisSource     */ &analysisSource,
    //         /* textPosition       */ 0,
    //         /* textLength         */ textLength,
    //         /* baseFontCollection */ _p.s->font->fontCollection.get(),
    //         /* baseFamilyName     */ _p.s->font->fontName.c_str(),
    //         /* fontAxisValues     */ textFormatAxis.data(),
    //         /* fontAxisValueCount */ gsl::narrow_cast<u32>(textFormatAxis.size()),
    //         /* mappedLength       */ mappedLength,
    //         /* scale              */ &scale,
    //         /* mappedFontFace     */ reinterpret_cast<IDWriteFontFace5**>(mappedFontFace)));
    // }
    // else
    // {
        const auto baseWeight = WI_IsFlagSet(_api.attributes, FontRelevantAttributes::Bold) ? DWRITE_FONT_WEIGHT_BOLD : static_cast<DWRITE_FONT_WEIGHT>(_p.font->fontWeight);
        const auto baseStyle = WI_IsFlagSet(_api.attributes, FontRelevantAttributes::Italic) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
        wil::com_ptr<IDWriteFont> font;

        THROW_IF_FAILED(_p.font->fontFallback->MapCharacters(
            /* analysisSource     */ &analysisSource,
            /* textPosition       */ 0,
            /* textLength         */ textLength,
            /* baseFontCollection */ _p.font->fontCollection.get(),
            /* baseFamilyName     */ _p.font->fontName.c_str(),
            /* baseWeight         */ baseWeight,
            /* baseStyle          */ baseStyle,
            /* baseStretch        */ DWRITE_FONT_STRETCH_NORMAL,
            /* mappedLength       */ mappedLength,
            /* mappedFont         */ font.addressof(),
            /* scale              */ &scale));

        if (font)
        {
            THROW_IF_FAILED(font->CreateFontFace(reinterpret_cast<IDWriteFontFace**>(mappedFontFace)));
        }
    // }

    // // Oh wow! You found a case where scale isn't 1! I tried every font and none
    // // returned something besides 1. I just couldn't figure out why this exists.
    // assert(scale == 1);
}
void AtlasEngineD3D12::_mapRegularText(size_t offBeg, size_t offEnd)
{
    auto& row = _p.rows[_api.lastPaintBufferLineCoord.y];

    for (u32 idx = gsl::narrow_cast<u32>(offBeg), mappedEnd = 0; idx < offEnd; idx = mappedEnd)
    {
        u32 mappedLength = 0;
        wil::com_ptr<IDWriteFontFace2> mappedFontFace;
        _mapCharacters(_api.bufferLine.data() + idx, gsl::narrow_cast<u32>(offEnd - idx), &mappedLength, mappedFontFace.addressof());
        mappedEnd = idx + mappedLength;

        if (!mappedFontFace)
        {
            // _mapReplacementCharacter(idx, mappedEnd, row);
            continue;
        }

        const auto initialIndicesCount = row.glyphIndices.size();

        if (mappedLength > 0)
        {
            // if (_p.s->font->fontFeatures.empty())
            // {
            //     for (u32 complexityLength = 0; idx < mappedEnd; idx += complexityLength)
            //     {
            //         BOOL isTextSimple = FALSE;
            //         THROW_IF_FAILED(_p.textAnalyzer->GetTextComplexity(_api.bufferLine.data() + idx, mappedEnd - idx, mappedFontFace.get(), &isTextSimple, &complexityLength, _api.glyphIndices.data()));

            //         if (isTextSimple)
            //         {
            //             const auto shift = gsl::narrow_cast<u8>(row.lineRendition != LineRendition::SingleWidth);
            //             const auto colors = _p.foregroundBitmap.begin() + _p.colorBitmapRowStride * _api.lastPaintBufferLineCoord.y;

            //             for (size_t i = 0; i < complexityLength; ++i)
            //             {
            //                 const auto col1 = _api.bufferLineColumn[idx + i + 0];
            //                 const auto col2 = _api.bufferLineColumn[idx + i + 1];
            //                 const auto glyphAdvance = (col2 - col1) * _p.s->font->cellSize.x;
            //                 const auto fg = colors[static_cast<size_t>(col1) << shift];
            //                 row.glyphIndices.emplace_back(_api.glyphIndices[i]);
            //                 row.glyphAdvances.emplace_back(static_cast<f32>(glyphAdvance));
            //                 row.glyphOffsets.emplace_back();
            //                 row.colors.emplace_back(fg);
            //             }
            //         }
            //         else
            //         {
            //             // _mapComplex(mappedFontFace.get(), idx, complexityLength, row);
            //         }
            //     }
            // }
            // else
            // {
            //     // _mapComplex(mappedFontFace.get(), idx, mappedLength, row);
            // }
        }

        const auto indicesCount = row.glyphIndices.size();
        if (indicesCount > initialIndicesCount)
        {
            // if (row.mappings.empty() || row.mappings.back().fontFace != mappedFontFace)
            // {
            //     row.mappings.emplace_back(std::move(mappedFontFace), gsl::narrow_cast<u32>(initialIndicesCount), gsl::narrow_cast<u32>(indicesCount));
            // }
            // else
            // {
            //     row.mappings.back().glyphsTo = gsl::narrow_cast<u32>(indicesCount);
            // }
        }
    }
}

void AtlasEngineD3D12::_mapBuiltinGlyphs(size_t offBeg, size_t offEnd)
{
    auto& row = _p.rows[_api.lastPaintBufferLineCoord.y];
    auto initialIndicesCount = row.glyphIndices.size();
    // const auto shift = gsl::narrow_cast<u8>(row.lineRendition != LineRendition::SingleWidth);
    // const auto colors = _p.foregroundBitmap.begin() + _p.colorBitmapRowStride * _api.lastPaintBufferLineCoord.y;
    const auto base = reinterpret_cast<const u16*>(_api.bufferLine.data());
    const auto len = offEnd - offBeg;

    row.glyphIndices.insert(row.glyphIndices.end(), base + offBeg, base + offEnd);
    // row.glyphAdvances.insert(row.glyphAdvances.end(), len, static_cast<f32>(_p.s->font->cellSize.x));
    row.glyphOffsets.insert(row.glyphOffsets.end(), len, {});

    for (size_t i = offBeg; i < offEnd; ++i)
    {
        const auto col = _api.bufferLineColumn[i];
        // row.colors.emplace_back(colors[static_cast<size_t>(col) << shift]);
    }

    // row.mappings.emplace_back(nullptr, gsl::narrow_cast<u32>(initialIndicesCount), gsl::narrow_cast<u32>(row.glyphIndices.size()));
}

void AtlasEngineD3D12::_flushBufferLine()
{
    if (_api.bufferLine.empty())
    {
        return;
    }

    const auto cleanup = wil::scope_exit([this]() noexcept {
        _api.bufferLine.clear();
        _api.bufferLineColumn.clear();
    });

    // This would seriously blow us up otherwise.
    Expects(_api.bufferLineColumn.size() == _api.bufferLine.size() + 1);

    const auto builtinGlyphs = _p.s->font->builtinGlyphs;
    const auto beg = _api.bufferLine.data();
    const auto len = _api.bufferLine.size();
    size_t segmentBeg = 0;
    size_t segmentEnd = 0;
    bool custom = false;

    while (segmentBeg < len)
    {
        segmentEnd = segmentBeg;
        do
        {
            auto i = segmentEnd;
            char32_t codepoint = beg[i++];
            if (til::is_leading_surrogate(codepoint) && i < len)
            {
                codepoint = til::combine_surrogates(codepoint, beg[i++]);
            }

            const auto c = (builtinGlyphs && BuiltinGlyphs::IsBuiltinGlyph(codepoint)) || BuiltinGlyphs::IsSoftFontChar(codepoint);
            if (custom != c)
            {
                break;
            }

            segmentEnd = i;
        } while (segmentEnd < len);

        if (segmentBeg != segmentEnd)
        {
            if (custom)
            {
                // _mapBuiltinGlyphs(segmentBeg, segmentEnd);
            }
            else
            {
                // _mapRegularText(segmentBeg, segmentEnd);
            }
        }

        segmentBeg = segmentEnd;
        custom = !custom;
    }
}

[[nodiscard]] HRESULT AtlasEngineD3D12::EndPaint() noexcept
{
    return S_OK;
}

// Other IRenderEngine methods (to be implemented)

[[nodiscard]] bool AtlasEngineD3D12::RequiresContinuousRedraw() noexcept { return false; }
void AtlasEngineD3D12::WaitUntilCanRender() noexcept { }
[[nodiscard]] HRESULT AtlasEngineD3D12::Present() noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::ScrollFrame() noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::Invalidate(const til::rect* psrRegion) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::InvalidateCursor(const til::rect* psrRegion) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::InvalidateSystem(const til::rect* prcDirtyClient) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::InvalidateSelection(std::span<const til::rect> rectangles) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::InvalidateScroll(const til::point* pcoordDelta) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::InvalidateAll() noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::InvalidateTitle(std::wstring_view proposedTitle) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::PrepareRenderInfo(const RenderFrameInfo& info) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::ResetLineTransform() noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::PrepareLineTransform(LineRendition lineRendition, til::CoordType targetRow, til::CoordType viewportLeft) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::PaintBackground() noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::PaintBufferLine(std::span<const Cluster> clusters, til::point coord, bool fTrimLeft, bool lineWrapped) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::PaintBufferGridLines(GridLineSet lines, COLORREF color, size_t cchLine, til::point coordTarget) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::PaintSelection(const til::rect& rect) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::PaintCursor(const CursorOptions& options) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::UpdateDrawingBrushes(const TextAttribute& textAttributes, const RenderSettings& renderSettings, gsl::not_null<IRenderData*> pData, bool usingSoftFont, bool isSettingDefaultBrushes) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::UpdateFont(const FontInfoDesired& FontInfoDesired, FontInfo& FontInfo) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::UpdateSoftFont(std::span<const uint16_t> bitPattern, til::size cellSize, size_t centeringHint) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::UpdateDpi(int iDpi) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::GetProposedFont(const FontInfoDesired& FontInfoDesired, FontInfo& FontInfo, int iDpi) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::GetDirtyArea(std::span<const til::rect>& area) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::GetFontSize(til::size* pFontSize) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::IsGlyphWideByFont(std::wstring_view glyph, bool* pResult) noexcept { return E_NOTIMPL; }
[[nodiscard]] HRESULT AtlasEngineD3D12::UpdateTitle(std::wstring_view newTitle) noexcept { return E_NOTIMPL; }
