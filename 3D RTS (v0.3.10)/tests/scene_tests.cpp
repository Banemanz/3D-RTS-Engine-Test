#include "EditorSession.hpp"
#include "InstanceOverrides.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    RTSProject p=RTSProject::defaults();
    EditorSession editor;
    editor.panel=EditorSession::Panel::Scene;
    assert(!editor.selectedPlacement(p));
    assert(editor.selectNearestPlacement(p,{-13,0,-4.5f}));
    assert(editor.selectedPlacement(p)->id=="marine_1");
    const std::string id=editor.selectedPlacementId;
    const Vec3 old=editor.selectedPlacement(p)->position;
    assert(editor.moveSelectedPlacement(p,{-9,0,8}));
    assert(editor.selectedPlacement(p)->position.x==-9.f);
    assert(editor.undo(p));
    assert(editor.selectedPlacement(p)->position.x==old.x);
    assert(editor.redo(p));
    assert(editor.selectedPlacement(p)->position.x==-9.f);

    assert(editor.rotateSelectedPlacement(p,30));
    assert(std::abs(editor.selectedPlacement(p)->rotation.y-30)<0.01f);
    assert(editor.scaleSelectedPlacement(p,1.5f));
    assert(editor.selectedPlacement(p)->scale.x>1.4f);
    assert(editor.toggleSelectedOwner(p));
    assert(editor.selectedPlacement(p)->owner==1);
    assert(editor.undo(p));
    assert(editor.selectedPlacement(p)->owner==0);
    assert(editor.redo(p));
    assert(editor.selectedPlacement(p)->owner==1);

    assert(editor.beginSceneOverrideEdit(p));
    editor.editText=R"({"unit":{"max_hp":135,"damage":22},"actor":{"tint":[0.7,0.1,0.2]}})";
    assert(editor.finishSceneOverrideEdit(p));
    const ProjectPlacement* modified=editor.selectedPlacement(p);
    assert(modified!=nullptr && modified->id==id);
    auto resolved=resolvePlacedUnit(p,*modified);
    assert(resolved.unit.max_hp==135 && resolved.unit.damage==22);
    assert(unitFromCatalog(p.catalog,"marine").damage==7);
    assert(editor.undo(p));
    assert(resolvePlacedUnit(p,*editor.selectedPlacement(p)).unit.damage==7);
    assert(editor.redo(p));
    assert(resolvePlacedUnit(p,*editor.selectedPlacement(p)).unit.damage==22);

    assert(editor.beginSceneOverrideEdit(p));
    editor.editText=R"({"unit":{"max_hp":-100}})";
    assert(!editor.finishSceneOverrideEdit(p));
    assert(resolvePlacedUnit(p,*editor.selectedPlacement(p)).unit.damage==22);
    editor.cancelEdit();
    assert(editor.duplicateSelectedPlacement(p));
    assert(p.placements.size()==14);
    assert(editor.selectedPlacement(p)->id!=id);
    assert(editor.deleteSelectedPlacement(p));
    assert(p.placements.size()==13);
    assert(editor.undo(p));
    assert(p.placements.size()==14);
    assert(editor.redo(p));
    assert(p.placements.size()==13);

    assert(editor.placeSceneMarker(p,{9,0,23},false));
    assert(editor.selectedPlacement(p)->kind=="Point");
    assert(editor.placeSceneMarker(p,{12,0,24},true));
    assert(editor.selectedPlacement(p)->kind=="Region");
    assert(editor.adjustSelectedRegionRadius(p,2.5f));
    assert(editor.selectedPlacement(p)->overrides.at("radius").get<double>()==7.5);
    assert(editor.undo(p));
    assert(editor.selectedPlacement(p)->overrides.at("radius").get<double>()==5);
    assert(editor.redo(p));
    assert(editor.selectedPlacement(p)->overrides.at("radius").get<double>()==7.5);
    assert(editor.cycleSceneSelection(p,-1));
    assert(editor.selectedPlacement(p)!=nullptr);
    p.validate();
    auto loaded=RTSProject::fromJson(p.toJson());
    assert(loaded.placements.size()==p.placements.size());
    std::cout<<"scene: selection, transforms, overrides, point/region tools and history passed\n";
}
