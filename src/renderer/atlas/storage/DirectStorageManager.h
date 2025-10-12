#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <chrono>
#include <d3d12.h>
#include <wil/resource.h>
#include <wil/com.h>

#if defined(__has_include)
#    if __has_include(<dstorage.h>)
#        ifndef ATLAS_HAVE_DIRECTSTORAGE
#            define ATLAS_HAVE_DIRECTSTORAGE 1
#        endif
#        include <dstorage.h>
#    else
#        ifndef ATLAS_HAVE_DIRECTSTORAGE
#            define ATLAS_HAVE_DIRECTSTORAGE 0
#        endif
#    endif
#else
#    ifndef ATLAS_HAVE_DIRECTSTORAGE
#        define ATLAS_HAVE_DIRECTSTORAGE 0
#    endif
#endif

namespace Microsoft::Console::Render::Atlas::Storage
{
    class DirectStorageManager
    {
    public:
        DirectStorageManager() = default;
        ~DirectStorageManager();

        bool Initialize(ID3D12Device* device);
        void Shutdown() noexcept;

        bool EnqueueFileRead(std::wstring_view path,
                              ID3D12Resource* destination,
                              UINT64 destinationOffset,
                              UINT64 size,
                              UINT64 fileOffset = 0);
        void Submit() noexcept;
        void WaitForIdle() noexcept;
        bool SetCacheEnabled(bool enabled) noexcept;
        bool ClearCache() noexcept;
        static bool ClearPersistentCache() noexcept;

        [[nodiscard]] bool IsAvailable() const noexcept { return _available; }
        [[nodiscard]] std::wstring_view Status() const noexcept { return _status; }
        [[nodiscard]] bool CacheEnabled() const noexcept { return _cacheEnabled; }

    private:
        bool _available = false;
        std::wstring _status;
        bool _cacheEnabled = true;

#if ATLAS_HAVE_DIRECTSTORAGE
        using PFN_DStorageCreateFactory = HRESULT(WINAPI*)(DSTORAGE_FACTORY_FLAGS, REFIID, void**);

        PFN_DStorageCreateFactory _createFactory = nullptr;
        wil::unique_hmodule _module;
        Microsoft::WRL::ComPtr<IDStorageFactory> _factory;
        Microsoft::WRL::ComPtr<IDStorageQueue> _fileQueue;
        wil::unique_handle _queueEvent;
        std::vector<Microsoft::WRL::ComPtr<IDStorageFile>> _pendingFiles;
        std::vector<std::filesystem::path> _tempFiles;
        std::filesystem::path _cacheDirectory;
        bool _cacheReady = false;
        uint64_t _cacheHits = 0;
        uint64_t _cacheMisses = 0;
        double _lastCopyMilliseconds = 0.0;

        bool _ensureCacheDirectory();
        bool _isRemotePath(std::wstring_view path) const noexcept;
        std::filesystem::path _stageRemoteFile(std::wstring_view path);
#endif

        void _setStatus(std::wstring status) noexcept;
    };
}
