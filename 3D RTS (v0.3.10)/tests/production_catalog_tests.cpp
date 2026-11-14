#include "RTSHUDLayout.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>

static void rejectInvalid(const RTSProject& p) {
    bool rejected=false;
    try { p.validate(); }catch(const std::invalid_argument&) { rejected=true; }
    assert(rejected);
}
int main(){
    RTSProject p=RTSProject::defaults();
    assert(p.catalog.cloneDerived("ranger","medic"));
    assert(p.catalog.cloneDerived("ranger","raider"));
    assert(p.catalog.setField("medic","name",picojson::value("Field Medic")));
    assert(p.catalog.setField("medic","mineral_cost",picojson::value(65.0)));
    assert(p.catalog.setField("medic","build_time",picojson::value(1.0)));
    assert(p.catalog.setField("raider","build_time",picojson::value(1.0)));
    assert(p.catalog.setField("barracks","trainable_units",
        picojson::value(picojson::array{picojson::value("ranger"),
            picojson::value("medic"),picojson::value("worker"),picojson::value("raider")})));
    p.metadata["player_starting_minerals"]=picojson::value(750.0);
    p.metadata["enemy_spawn_interval"]=picojson::value(900.0);
    ProjectPlacement factory;
    factory.id="prototype_factory";
    factory.kind="Unit";
    factory.catalogId="barracks";
    factory.name="Prototype Barracks";
    factory.owner=0;
    factory.position={-28,0,15};
    p.placements.push_back(factory);
    p.validate();

    // Scene and project JSON carry producer catalogs and inherited training.
    const auto roundtrip=RTSProject::fromJson(p.toJson());
    assert(roundtrip.catalog.resolvedFields("barracks").at("trainable_units")
        .get<picojson::array>().size()==4);
    RTSSkirmish game;
    game.loadProject(roundtrip);
    int producer=-1;
    for(auto& u:game.units)if(u.catalogId=="barracks") {
        producer=u.id;u.isSelected=true;
    }
    assert(producer>0 && game.selectedProducerId()==producer);
    assert(game.trainingPageCount()==2 && game.trainingPage()==0);
    assert(game.selectedTrainableId(0)=="ranger");
    assert(game.selectedTrainableId(1)=="medic");
    assert(game.selectedTrainableId(2)=="worker");
    assert(game.canQueueSelectedTrainingSlot(1));
    RTSHUDLayout layout(1280,720);
    auto buttons=layout.buttons(game);
    assert(buttons[3].command==HUDCommand::TrainSlot0);
    assert(buttons[4].command==HUDCommand::TrainSlot1 && buttons[4].enabled);
    assert(layout.hit(buttons[4].bounds.x+2,buttons[4].bounds.y+2,game)==HUDCommand::TrainSlot1);
    assert(buttons[11].command==HUDCommand::TrainingPage && buttons[11].enabled);
    const int originalMinerals=game.playerMinerals;
    const int originalSupply=game.supplyUsed;
    assert(game.queueSelectedTrainingSlot(1));
    assert(game.playerMinerals==originalMinerals-65);
    assert(game.supplyUsed==originalSupply+1);
    assert(game.pendingProduction(producer)==1);
    assert(game.cancelSelectedProduction());
    assert(game.playerMinerals==originalMinerals && game.supplyUsed==originalSupply);
    assert(game.queueSelectedTrainingSlot(1));
    assert(game.nextTrainingPage() && game.trainingPage()==1);
    assert(game.selectedTrainableId(0)=="raider");
    buttons=layout.buttons(game);
    assert(buttons[3].title=="RAIDER");
    assert(!game.nextTrainingPage() || game.trainingPage()==0);
    for(int t=0;t<90;++t)game.update(1.f/60.f);
    bool medicCreated=false;
    for(const auto& u:game.units)if(u.catalogId=="medic" && u.ownerId==0)
        medicCreated=true;
    assert(medicCreated);
    assert(game.pendingProduction(producer)==0);
    // Disabled at full supply, but a built-in legacy project still loads.
    game.supplyUsed=game.supplyCap;
    assert(!game.canQueueSelectedTrainingSlot(0));
    RTSProject legacy=RTSProject::defaults();
    legacy.catalog.find("barracks")->fields.erase("trainable_units");
    legacy.catalog.find("command_center")->fields.erase("trainable_units");
    RTSSkirmish old;
    old.loadProject(legacy);
    assert(old.trainableUnits(old.playerBaseId())==std::vector<std::string>{"worker"});

    // Renaming a trainable unit migrates its typed entries, not custom prose.
    assert(p.renameCatalogObject("medic","field_medic"));
    auto ids=p.catalog.resolvedFields("barracks").at("trainable_units").get<picojson::array>();
    assert(ids[1].get<std::string>()=="field_medic");

    RTSProject bad=p;
    assert(bad.catalog.setField("barracks","trainable_units",picojson::value("ranger")));
    rejectInvalid(bad);
    assert(bad.catalog.setField("barracks","trainable_units",
        picojson::value(picojson::array{picojson::value("ranger"),picojson::value("ranger")})));
    rejectInvalid(bad);
    assert(bad.catalog.setField("barracks","trainable_units",
        picojson::value(picojson::array{picojson::value("command_center")})));
    rejectInvalid(bad);
    assert(bad.catalog.setField("barracks","trainable_units",
        picojson::value(picojson::array{picojson::value("missing_unit")})));
    rejectInvalid(bad);
    std::cout<<"training: catalog-driven units, UI pages, reservations, cancellation, migration and validation passed\n";
}
