#pragma once
#include "UnitData.hpp"
#include "third_party/picojson.h"
#include <stdexcept>
#include <string>
#include <vector>

// A catalog is an extensible typed object graph. Fields deliberately retain
// arbitrary JSON values, including future mod-defined fields not yet understood
// by the game runtime.
struct CatalogEntry {
    std::string id;
    std::string type;
    std::string parent;
    picojson::object fields;
};

class DataCatalog {
public:
    const std::vector<CatalogEntry>& entries() const { return entries_; }
    std::vector<CatalogEntry>& entries() { return entries_; }
    const CatalogEntry* find(const std::string& id) const;
    CatalogEntry* find(const std::string& id);
    bool add(CatalogEntry entry,std::string* error=nullptr);
    bool cloneDerived(const std::string& original,const std::string& newId);
    bool setField(const std::string& id,const std::string& key,
                  const picojson::value& value);
    picojson::object resolvedFields(const std::string& id) const;
    void validate() const;
    picojson::value serialize() const;
    static DataCatalog deserialize(const picojson::value& root);
    static DataCatalog defaults();
private:
    std::vector<CatalogEntry> entries_;
    void resolveInto(const std::string& id,picojson::object& out,
                     std::vector<std::string>& stack) const;
};

// Typed runtime adapters. Unknown catalog fields are not deleted or discarded.
UnitData unitFromCatalog(const DataCatalog& catalog,const std::string& id);
struct ActorVisual {
    Vec3 tint{1.f,1.f,1.f};
    Vec3 scale{1.f,1.f,1.f};
    float opacity=1.f;
    std::string mesh="procedural.unit";
    std::string texture;
    std::string animation;
    std::string sound;
};
ActorVisual actorFromCatalog(const DataCatalog& catalog,const std::string& actorId);
