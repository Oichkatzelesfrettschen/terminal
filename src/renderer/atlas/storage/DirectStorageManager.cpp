#include "pch.h"
#include "DirectStorageManager.h"

#include <Windows.h>
#include <ShlObj.h>
#include <wil/result.h>

#include "../telemetry/TelemetryReporter.h"

using namespace Microsoft::Console::Render::Atlas::Storage;

#if ATLAS_HAVE_DIRECTSTORAGE
bool DirectStorageManager::_ensureCacheDirectory()
{
    if (_cacheReady)
    {
        return true;
    }

    wil::unique_cotaskmem_string localAppData;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, localAppData.put())) || !localAppData)
    {
        _setStatus(L"Failed to locate LocalAppData for DirectStorage cache");
        return false;
    }

    try
    {
        _cacheDirectory = std::filesystem::path(localAppData.get()) / L"Atlas" / L"DirectStorageCache";
        std::filesystem::create_directories(_cacheDirectory);
        _cacheReady = true;
    }
    catch (const std::exception& ex)
    {
        _setStatus(wil::str_printf<std::wstring>(L"Cache directory error: %hs", ex.what()));
        _cacheReady = false;
    }

    return _cacheReady;
}

bool DirectStorageManager::_isRemotePath(std::wstring_view path) const noexcept
{
    auto begins_with_ci = [](std::wstring_view haystack, std::wstring_view needle) noexcept
    {
        if (haystack.size() < needle.size())
        {
            return false;
        }
        for (size_t i = 0; i < needle.size(); ++i)
        {
            if (towlower(haystack[i]) != towlower(needle[i]))
            {
                return false;
            }
        }
        return true;
    };

    return begins_with_ci(path, L"\\\\wsl$") || begins_with_ci(path, L"\\\\9p\\");
}

std::filesystem::path DirectStorageManager::_stageRemoteFile(std::wstring_view path)
{
    if (!_ensureCacheDirectory())
    {
        return {};
    }

    const auto hashed = wil::str_printf<std::wstring>(L"%016llx.bin", static_cast<unsigned long long>(std::hash<std::wstring>{}(std::wstring(path))));
    std::filesystem::path cacheFile = _cacheDirectory / hashed;

    const auto startTime = std::chrono::high_resolution_clock::now();

    try
    {
        std::filesystem::path source(path);
        bool copyRequired = true;
        if (_cacheEnabled && std::filesystem::exists(cacheFile))
        {
            std::error_code ec;
            auto srcTime = std::filesystem::last_write_time(source, ec);
            auto dstTime = std::filesystem::last_write_time(cacheFile, ec);
            if (!ec && dstTime >= srcTime)
            {
                copyRequired = false;
                ++_cacheHits;
            }
        }

        if (copyRequired)
        {
            std::filesystem::copy_file(source, cacheFile, std::filesystem::copy_options::overwrite_existing);
            ++_cacheMisses;
            const auto endTime = std::chrono::high_resolution_clock::now();
            _lastCopyMilliseconds = std::chrono::duration<double, std::milli>(endTime - startTime).count();

            if (!_cacheEnabled)
            {
                _tempFiles.push_back(cacheFile);
            }
        }
        else
        {
            _lastCopyMilliseconds = 0.0;
        }
    }
    catch (const std::exception& ex)
    {
        _setStatus(wil::str_printf<std::wstring>(L"Cache copy failed: %hs", ex.what()));
        return {};
    }

    return cacheFile;
}

void DirectStorageManager::_setStatus(std::wstring status) noexcept
{
    _status = std::move(status);
}
#endif

DirectStorageManager::~DirectStorageManager()
{
    Shutdown();
}

void DirectStorageManager::Shutdown() noexcept
{
#if ATLAS_HAVE_DIRECTSTORAGE
    if (_fileQueue)
    {
        _fileQueue->CancelRequestsWithTag(nullptr, 0);
        _fileQueue->Close();
    }

    _fileQueue.Reset();
    _factory.Reset();
    _createFactory = nullptr;
    _module.reset();
    _pendingFiles.clear();
    _tempFiles.clear();
    _queueEvent.reset();
    _cacheReady = false;
#endif
    _available = false;
}

bool DirectStorageManager::Initialize(ID3D12Device* device)
{
    Shutdown();

    if (!device)
    {
        _status = L"Initialize called with null device";
        return false;
    }

#if ATLAS_HAVE_DIRECTSTORAGE
    _module.reset(LoadLibraryW(L"dstorage.dll"));
    if (!_module)
    {
        _setStatus(L"DirectStorage runtime not found (dstorage.dll missing)");
        return false;
    }

    _createFactory = reinterpret_cast<PFN_DStorageCreateFactory>(GetProcAddress(_module.get(), "DStorageCreateFactory"));
    if (!_createFactory)
    {
        _setStatus(L"DirectStorage factory export not available");
        return false;
    }

    Microsoft::WRL::ComPtr<IDStorageFactory> factory;
    HRESULT hr = _createFactory(DSTORAGE_FACTORY_FLAG_NONE, IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr))
    {
        wil::unique_cotaskmem_string message;
        if (SUCCEEDED(wil::GetFailureLogString(hr, message.put())))
        {
            _setStatus(std::wstring(L"DStorageCreateFactory failed: ") + message.get());
        }
        else
        {
            wchar_t buffer[64];
            swprintf_s(buffer, L"DStorageCreateFactory failed: 0x%08X", static_cast<unsigned int>(hr));
            _setStatus(buffer);
        }
        return false;
    }

    _factory = std::move(factory);
    _ensureCacheDirectory();

    DSTORAGE_QUEUE_DESC queueDesc{};
    queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    queueDesc.Device = device;
    queueDesc.Priority = DSTORAGE_QUEUE_PRIORITY_NORMAL;
    queueDesc.Capacity = DSTORAGE_MIN_QUEUE_CAPACITY;
    queueDesc.Name = L"AtlasGlyphDirectStorageQueue";

    hr = _factory->CreateQueue(&queueDesc, IID_PPV_ARGS(&_fileQueue));
    if (FAILED(hr))
    {
        wchar_t buffer[64];
        swprintf_s(buffer, L"CreateQueue failed: 0x%08X", static_cast<unsigned int>(hr));
        _setStatus(buffer);
        _fileQueue.Reset();
        return false;
    }

    _queueEvent.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    if (!_queueEvent)
    {
        _setStatus(L"Failed to create DirectStorage queue event");
        return false;
    }

    THROW_IF_FAILED(_fileQueue->SetEventNotification(_queueEvent.get()));

    _available = true;
    _setStatus(L"DirectStorage queue ready");
#else
    _setStatus(L"DirectStorage headers unavailable at build time");
#endif

    return _available;
}

bool DirectStorageManager::EnqueueFileRead(std::wstring_view path,
                                           ID3D12Resource* destination,
                                           UINT64 destinationOffset,
                                           UINT64 size,
                                           UINT64 fileOffset)
{
#if ATLAS_HAVE_DIRECTSTORAGE
    if (!_available || !_fileQueue)
    {
        return false;
    }

    std::wstring widePath(path);

    if (_isRemotePath(widePath))
    {
        const auto staged = _stageRemoteFile(widePath);
        if (!staged.empty())
        {
            widePath = staged.wstring();
        }
    }

    Microsoft::WRL::ComPtr<IDStorageFile> file;
    const HRESULT openHr = _factory->OpenFile(widePath.c_str(), IID_PPV_ARGS(&file));
    if (FAILED(openHr))
    {
        wchar_t buffer[128];
        swprintf_s(buffer, L"OpenFile failed (%ls): 0x%08X", widePath.c_str(), static_cast<unsigned int>(openHr));
        _setStatus(buffer);
        return false;
    }

    DSTORAGE_REQUEST request{};
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_BUFFER;
    request.Options.Flags = DSTORAGE_REQUEST_FLAGS_NONE;
    request.Source.File.Source = file.Get();
    request.Source.File.Offset = fileOffset;
    request.Destination.Buffer.Resource = destination;
    request.Destination.Buffer.Offset = destinationOffset;
    request.Size = size;
    request.UncompressedSize = size;

    const HRESULT enqueueHr = _fileQueue->EnqueueRequest(&request);
    if (FAILED(enqueueHr))
    {
        wchar_t buffer[128];
        swprintf_s(buffer, L"EnqueueRequest failed: 0x%08X", static_cast<unsigned int>(enqueueHr));
        _setStatus(buffer);
        return false;
    }

    _pendingFiles.emplace_back(std::move(file));
    return true;
#else
    (void)path;
    (void)destination;
    (void)destinationOffset;
    (void)size;
    (void)fileOffset;
    return false;
#endif
}

void DirectStorageManager::Submit() noexcept
{
#if ATLAS_HAVE_DIRECTSTORAGE
    if (_fileQueue)
    {
        _fileQueue->Submit();
        _setStatus(L"DirectStorage queue submitted");
    }
#endif
}

void DirectStorageManager::WaitForIdle() noexcept
{
#if ATLAS_HAVE_DIRECTSTORAGE
    if (_fileQueue)
    {
        _fileQueue->WaitForIdle();
        _setStatus(L"DirectStorage queue idle");
    }
    _pendingFiles.clear();

    Telemetry::ReportDirectStorageCacheEvent({ _status, _cacheHits, _cacheMisses, _lastCopyMilliseconds });

    if (!_tempFiles.empty())
    {
        std::error_code ec;
        for (auto& temp : _tempFiles)
        {
            std::filesystem::remove(temp, ec);
        }
        _tempFiles.clear();
    }
#endif
}

bool DirectStorageManager::SetCacheEnabled(bool enabled) noexcept
{
    _cacheEnabled = enabled;
#if ATLAS_HAVE_DIRECTSTORAGE
    if (!_available)
    {
        _setStatus(L"DirectStorage unavailable");
        return true;
    }

    if (!enabled)
    {
        _cacheHits = 0;
        _cacheMisses = 0;
        _lastCopyMilliseconds = 0.0;
    }

    _setStatus(enabled ? L"DirectStorage cache enabled" : L"DirectStorage cache disabled");
    Telemetry::ReportDirectStorageCacheEvent({ _status, _cacheHits, _cacheMisses, _lastCopyMilliseconds });
    return true;
#else
    (void)enabled;
    _setStatus(L"DirectStorage unavailable");
    Telemetry::ReportDirectStorageCacheEvent({ _status, 0, 0, 0.0 });
    return true;
#endif
}

bool DirectStorageManager::ClearCache() noexcept
{
    const bool cleared = ClearPersistentCache();
#if ATLAS_HAVE_DIRECTSTORAGE
    if (cleared)
    {
        _cacheHits = 0;
        _cacheMisses = 0;
        _lastCopyMilliseconds = 0.0;
        _setStatus(L"DirectStorage cache cleared");
        _tempFiles.clear();
    }
    else
    {
        _setStatus(L"Failed to clear DirectStorage cache");
    }
    Telemetry::ReportDirectStorageCacheEvent({ _status, _cacheHits, _cacheMisses, _lastCopyMilliseconds });
#else
    if (!cleared)
    {
        _setStatus(L"Failed to clear DirectStorage cache");
    }
    _tempFiles.clear();
    Telemetry::ReportDirectStorageCacheEvent({ _status, 0, 0, 0.0 });
#endif
    return cleared;
}

bool DirectStorageManager::ClearPersistentCache() noexcept
{
    try
    {
        wil::unique_cotaskmem_string localAppData;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, localAppData.put())))
        {
            return false;
        }

        std::filesystem::path cacheDir = std::filesystem::path(localAppData.get()) / L"Atlas" / L"DirectStorageCache";
        std::error_code ec;
        if (!std::filesystem::exists(cacheDir, ec))
        {
            return !ec;
        }

        for (const auto& entry : std::filesystem::directory_iterator(cacheDir, ec))
        {
            if (ec)
            {
                return false;
            }

            std::filesystem::remove_all(entry.path(), ec);
            if (ec)
            {
                return false;
            }
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}

#if !ATLAS_HAVE_DIRECTSTORAGE
void DirectStorageManager::_setStatus(std::wstring status) noexcept
{
    _status = std::move(status);
}
#endif
