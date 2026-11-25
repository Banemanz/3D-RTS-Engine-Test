#pragma once
#include "RTSStaticMesh.hpp"
// Native offline compiler. No GL context, external process or Python required.
RTSStaticMesh rtsCompileGltf(const std::filesystem::path& source);
// Publishes a new project-local asset; existing files are never overwritten.
void rtsImportGltf(const std::filesystem::path& source,
                   const std::filesystem::path& project,
                   const std::string& reference);
// Deliberate replacement of a valid existing cache. Preserve unique sockets.
void rtsReimportGltf(const std::filesystem::path& source,
                    const std::filesystem::path& project,
                    const std::string& reference);

// Explicit, portable source receipts. Runtime assets and replay data are unchanged.
struct RTSAssetSourceStatus {
    std::string state,source,detail;
    std::vector<std::string> changed;
    std::size_t dependencies=0;
};
void rtsTrackGltf(const std::filesystem::path& source,
                  const std::filesystem::path& project,const std::string& reference);
RTSAssetSourceStatus rtsInspectTrackedGltf(const std::filesystem::path& project,
                                         const std::string& reference);
void rtsRebuildTrackedGltf(const std::filesystem::path& project,const std::string& reference);
