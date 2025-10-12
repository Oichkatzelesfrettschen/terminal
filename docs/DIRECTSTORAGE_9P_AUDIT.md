# DirectStorage & 9P Integration Audit

## Background
- Windows 11 WSL2 exposes Linux files via the 9P (Plan9) protocol mounted under `\wsl$\Distribution\`.
- DirectStorage 1.x requires NT filesystem handles that support `FILE_FLAG_NO_BUFFERING` and the DirectStorage runtime APIs.
- The current Atlas pipeline pulls glyphs from local NTFS paths; remote/WFS (e.g., OneDrive/9P) fall back to Win32 async I/O.

## Findings
1. **9P mounts are remote UNC paths** and are **not enumerated as local NVMe volumes**; DirectStorage rejects handles opened on them.
2. The runtime currently initializes DirectStorage with `DSTORAGE_REQUEST_SOURCE_FILE` assuming local NT paths.
3. There is no fallback to stage 9P assets into a temporary NVMe cache before enqueuing to DirectStorage.

## Integration Plan
- Detect 9P/UNC paths and stage requested assets into a DirectStorage cache folder (e.g., `%LOCALAPPDATA%\Atlas\Cache`).
- Use DirectStorage for the cached file while maintaining a change-tracking map (mtime/hash) to invalidate cache entries on subsequent 9P writes.
- Provide telemetry events (`TelemetryReporter`) differentiating between `DSCacheHit`, `DSCacheMiss`, and `9PBypass` for performance analysis.
- Long-term: investigate Project Nucleus / Windows AppFileSystem which can expose cloud/remote storage through DirectStorage-friendly APIs.

## Next Steps
1. Implement 9P detector: treat any path starting with `\\wsl$\` or containing `\9p\` as remote.
2. Add staging layer in `DirectStorageManager` that copies remote assets to cache prior to `EnqueueFileRead`.
3. Surface cache metrics in telemetry overlay and settings UI for user control (clear cache, disable staging).
