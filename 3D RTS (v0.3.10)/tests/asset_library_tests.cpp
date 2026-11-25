#include "EditorSession.hpp"
#include "RTSAssetLibrary.hpp"
#include <algorithm>
#include <cassert>
#if defined(NDEBUG)
#error "Asset authoring tests need Release assertions"
#endif
#include <filesystem>
#include <iostream>
#include <string>
int main(int argc,char** argv){
    assert(argc==3);
    const std::filesystem::path file=argv[1];
    const auto assets=rtsDiscoverStaticAssets(file);
    const auto found=std::find_if(assets.begin(),assets.end(),
        [](const RTSAssetRecord& a){return a.reference=="assets/beacon.rtsmesh";});
    assert(found!=assets.end() && found->valid());
    assert(found->vertices==32 && found->triangles==16);
    assert(found->textureWidth==2 && found->textureHeight==2);
    assert(found->imageBytes==16 && found->texturedSurfaces==1);
    assert(found->maximum[1]>found->minimum[1]);
    assert(rtsAssetMatches(*found,"BEACON"));
    assert(!rtsAssetMatches(*found,"missing_name"));
    const auto dual=std::find_if(assets.begin(),assets.end(),
        [](const RTSAssetRecord& a){return a.reference=="assets/beacon_dual.rtsmesh";});
    assert(dual!=assets.end() && dual->valid());
    assert(dual->surfaces==2 && dual->texturedSurfaces==1 && dual->imageBytes==16);
    assert(dual->maximum[1]>found->maximum[1]);
    assert(rtsDiscoverStaticAssets(file,0).empty());
    RTSProject p=RTSProject::defaults();
    EditorSession e;
    e.panel=EditorSession::Panel::Data;
    const auto before=p.toJson().serialize();
    assert(!e.importStaticMesh(p,file.string(),"assets/../escape.rtsmesh"));
    assert(!e.importStaticMesh(p,file.string(),"assets/missing.rtsmesh"));
    assert(p.toJson().serialize()==before && !e.modified);
    assert(e.importStaticMesh(p,file.string(),"assets/beacon.rtsmesh"));
    const auto imported=p.toJson().serialize();
    assert(imported!=before);
    const auto* actor=e.selected(p);
    assert(actor && actor->type=="Actor");
    const auto actorId=actor->id;
    const auto fields=p.catalog.resolvedFields(actorId);
    const auto modelId=fields.at("model").get<std::string>();
    assert(p.catalog.find(modelId)->type=="Model");
    assert(p.catalog.resolvedFields(modelId).at("mesh")
        .get<std::string>()=="assets/beacon.rtsmesh");
    assert(!e.importStaticMesh(p,file.string(),"assets/beacon.rtsmesh"));
    assert(imported==p.toJson().serialize());
    assert(e.undo(p) && p.toJson().serialize()==before);
    assert(e.redo(p) && p.toJson().serialize()==imported);
    e.panel=EditorSession::Panel::Scene;
    const auto count=p.placements.size();
    assert(!e.placeSceneDoodad(p,modelId,Vec3(0,0,0)));
    assert(!e.placeSceneDoodad(p,actorId,Vec3(100000,0,0)));
    assert(p.placements.size()==count);
    assert(e.placeSceneDoodad(p,actorId,Vec3(-4,0,6)));
    assert(p.placements.size()==count+1);
    assert(p.placements.back().catalogId==actorId);
    assert(!e.selectedPlacementId.empty());
    p.validate();
    assert(e.undo(p) && p.placements.size()==count);
    assert(e.redo(p) && p.placements.size()==count+1);
    assert(e.undo(p) && e.undo(p));
    assert(p.toJson().serialize()==before);
    std::cout<<"asset library: validated scan, safe paths, atomic Model+Actor "
                "registration, idempotence, F8 placement and undo/redo passed\n";
}
