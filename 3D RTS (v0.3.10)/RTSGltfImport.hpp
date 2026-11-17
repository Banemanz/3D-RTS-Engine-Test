#pragma once
#include "RTSStaticMesh.hpp"
// Native offline compiler. No GL context, external process or Python required.
RTSStaticMesh rtsCompileGltf(const std::filesystem::path& source);
// Publishes a new project-local asset; existing files are never overwritten.
void rtsImportGltf(const std::filesystem::path& source,
                   const std::filesystem::path& project,
                   const std::string& reference);
