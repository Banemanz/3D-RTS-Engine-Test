#pragma once
#include "DataCatalog.hpp"
#include "TerrainMap.hpp"
#include <string>
#include <vector>

struct ProjectPlacement {
    std::string id;         // stable editor GUID-like name (human-readable)
    std::string kind;       // Unit, Mineral, Doodad, Point, Region
    std::string catalogId;  // linked catalog ID when applicable
    std::string name;
    int owner=0;
    Vec3 position{0,0,0};
    Vec3 rotation{0,0,0};   // Euler degrees for authoring; render support is partial
    Vec3 scale{1,1,1};
    int amount=0;           // amount for Mineral deposits
    picojson::object overrides; // arbitrary per-instance fields (round-tripped)
};

class RTSProject {
public:
    int schemaVersion=1;
    std::string name="Untitled RTS Project";
    std::string description;
    TerrainMap terrain;
    DataCatalog catalog;
    std::vector<ProjectPlacement> placements;
    picojson::object metadata; // project-defined authoring metadata
    picojson::object extensions; // unknown top-level properties preserved losslessly

    static RTSProject defaults();
    static RTSProject fromJson(const picojson::value& root);
    picojson::value toJson() const;
    void validate() const;
    // Transactional: migrates recognized engine-facing catalog references.
    bool renameCatalogObject(const std::string& oldId,const std::string& newId);
    void loadFile(const std::string& filename);
    void saveFile(const std::string& filename) const;
};
