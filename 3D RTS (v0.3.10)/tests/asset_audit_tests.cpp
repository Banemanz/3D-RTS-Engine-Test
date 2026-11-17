#include "RTSAssetAudit.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Asset audit requires assertions in Release tests"
#endif
#include <filesystem>
#include <iostream>
#include <string>
int main(int argc,char** argv){
    assert(argc==2);
    const std::filesystem::path file=argv[1];
    RTSProject project;
    project.loadFile(file.string());
    auto result=rtsAuditProjectAssets(project,file);
    assert(result.valid());
    assert(result.models>=3);
    assert(result.importedModels==1);
    assert(result.uniqueFiles==1);
    assert(result.uniqueTriangles==16);
    assert(result.triangleInstances==16);
    assert(result.rows.size()==1);
    assert(result.rows.front().vertices==32);
    const auto original=project.toJson().serialize();
    auto model=project.catalog.find("imported_beacon_model");
    assert(model && model->type=="Model");
    model->fields["mesh"]=picojson::value("assets/missing_asset.rtsmesh");
    result=rtsAuditProjectAssets(project,file);
    assert(!result.valid() && result.rows.size()==1);
    assert(!result.rows.front().error.empty());
    model->fields["mesh"]=picojson::value("../private.rtsmesh");
    result=rtsAuditProjectAssets(project,file);
    assert(!result.valid());
    assert(result.rows.front().error.find("unsafe")!=std::string::npos);
    model->fields["mesh"]=picojson::value(77.0);
    result=rtsAuditProjectAssets(project,file);
    assert(!result.valid());
    assert(result.rows.front().error.find("string")!=std::string::npos);
    // Asset audit never edits the map; restoring the source round-trips.
    model->fields["mesh"]=picojson::value("assets/beacon.rtsmesh");
    assert(project.toJson().serialize()==original);
    assert(rtsAuditProjectAssets(project,file).valid());
    std::cout<<"Asset auditing: referenced models, deduplication, corruption "
                "and unsafe external references passed\n";
}
