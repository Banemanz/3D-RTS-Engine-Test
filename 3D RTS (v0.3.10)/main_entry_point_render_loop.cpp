// Build using CMake; see the repository README.md.


#include "GLCore.hpp"
#include "Math3D.hpp"
#include "RTSCamera.hpp"
#include "UnitData.hpp"
#include "Unit.hpp"
#include "SelectionManager.hpp"
#include "Shaders.hpp"
#include "RTSSkirmish.hpp"
#include "BitmapHUD.hpp"
#include "RTSHUDLayout.hpp"
#include "RTSHUD.hpp"
#include "CrashReporter.hpp"
#include "RTSProject.hpp"
#include "InstanceOverrides.hpp"
#include "EditorSession.hpp"
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <cmath>
#include <string>

#include <algorithm>
#include <array>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <exception>

struct AppState {
    RTSCamera camera;
    SelectionManager selectionManager;
    RTSSkirmish game;
    RTSProject project=RTSProject::defaults();
    EditorSession editor;
    std::unordered_map<std::string,ActorVisual> doodadVisuals;
    std::string notice;
    float noticeSeconds=0;
    std::string projectPath="projects/working.rtsproject.json";
    bool mouseStroke=false;
    bool hoveredTerrain=false;
    Vec3 hoveredWorld{};
    float terrainSlopeLimit()const {
        auto it=project.metadata.find("max_walkable_slope");
        if(it==project.metadata.end() || !it->second.is<double>() ||
           !std::isfinite(it->second.get<double>()))return 1.25f;
        return std::clamp(float(it->second.get<double>()),.05f,100.f);
    }
    void reloadFromProject(){
        TriggerEngine::validate(project);
        // Validate all actor assets once, not every rendered frame.
        std::unordered_map<std::string,ActorVisual> previews;
        for(const auto& placed:project.placements)
            if(placed.kind=="Doodad")
                previews.emplace(placed.id,resolvePlacedDoodad(project,placed));
        game.loadProject(project);
        doodadVisuals=std::move(previews);
        project.terrain.markDirty();
        camera.terrain=&project.terrain;
        hasMoveMarker=false;
    }
    void inform(const std::string& message){
        notice=message.substr(0,140);
        noticeSeconds=4.f;
    }
    void autoSave(){
        if(!editor.modified || editor.panel==EditorSession::Panel::Play)return;
        try{
            TriggerEngine::validate(project);
            project.saveFile(projectPath+".autosave");
        }catch(const std::exception& e){
            std::cerr<<"[Autosave] "<<e.what()<<"\n";
            editor.message=std::string("AUTOSAVE FAILED: ")+e.what();
        }
    }
    void recoverAutosave(){
        try {
            const std::string autosave=projectPath+".autosave";
            if(!std::filesystem::exists(autosave))return;
            if(std::filesystem::exists(projectPath) &&
               std::filesystem::last_write_time(autosave)<=std::filesystem::last_write_time(projectPath))
                return;
            RTSProject recovered;
            recovered.loadFile(autosave);
            TriggerEngine::validate(recovered);
            project=std::move(recovered);
            editor.modified=true;
            inform("RECOVERED NEWER AUTOSAVE - CTRL+S TO KEEP");
        }catch(const std::exception& e){
            std::cerr<<"[Autosave] recovery skipped: "<<e.what()<<"\n";
        }
    }
    void saveProject(){
        try{
            TriggerEngine::validate(project);
            project.saveFile(projectPath);
            std::error_code ec;
            std::filesystem::remove(projectPath+".autosave",ec);
            editor.modified=false;editor.message="PROJECT SAVED";
            inform("MAP SAVED");
        }
        catch(const std::exception& e){editor.message=std::string("SAVE ERROR: ")+e.what();}
    }
    enum class Mode { Normal, Move, AttackMove, BuildBarracks, Patrol } mode = Mode::Normal;
    bool keyA = false, keyB = false, keyT = false, keyC = false;
    bool keyX = false, keyR = false, keyM=false, keyP=false, keyV=false,
         keyF2=false, keyF3=false, keyG=false, keyH=false;
    std::array<std::vector<int>,10> controlGroups{};
    std::array<bool,10> digitWasDown{};
    int framebufferWidth = 1280, framebufferHeight = 720;

    Vec3 lastClickedWorldPos{0.0f, 0.0f, 0.0f};
    bool hasMoveMarker = false;
    int windowWidth = 1280;
    int windowHeight = 720;
    void command(HUDCommand action,double px=0,double py=0){
        if(action==HUDCommand::Minimap){
            RTSHUDLayout hud(windowWidth,windowHeight);
            const Vec3 world=hud.minimapWorld(px,py,project.terrain);
            camera.targetFocalPoint=world;
            camera.focalPoint=world;
            return;
        }
        switch(action){
            case HUDCommand::Move: mode=Mode::Move;inform("MOVE: RIGHT CLICK DESTINATION");break;
            case HUDCommand::Patrol:mode=Mode::Patrol;inform("PATROL: RIGHT CLICK DESTINATION");break;
            case HUDCommand::AttackMove:mode=Mode::AttackMove;inform("ATTACK MOVE: RIGHT CLICK");break;
            case HUDCommand::Build:mode=Mode::BuildBarracks;inform("BUILD: RIGHT CLICK CLEAR TERRAIN");break;
            case HUDCommand::Stop:game.orderStop();mode=Mode::Normal;break;
            case HUDCommand::Hold:
                game.orderHold();
                mode=Mode::Normal;
                inform("HOLD POSITION - ATTACKS WITHOUT MOVING");
                break;
            case HUDCommand::CancelQueue:
                if(!game.cancelSelectedProduction())inform("NO TRAINING TO CANCEL");
                else inform("TRAINING CANCELLED - RESOURCES REFUNDED");
                break;
            case HUDCommand::Marine:
                if(!game.queueMarine())inform("CANNOT TRAIN MARINE: CHECK MINERALS, SUPPLY AND BARRACKS");
                break;
            case HUDCommand::Worker:
                if(!game.queueWorker())inform("CANNOT TRAIN WORKER: CHECK MINERALS AND SUPPLY");
                break;
            case HUDCommand::SelectArmy:{
                int count=0;
                for(auto& unit:game.units) {
                    unit.isSelected=unit.isAlive() && unit.ownerId==0 &&
                                     !unit.isStructure && !unit.isWorker;
                    if(unit.isSelected)++count;
                }
                if(!count)inform("NO COMBAT UNITS AVAILABLE");
                break;
            }
            case HUDCommand::SelectWorkers:{
                int count=0;
                for(auto& unit:game.units){
                    unit.isSelected=unit.isAlive() && unit.ownerId==0 && unit.isWorker;
                    if(unit.isSelected)++count;
                }
                if(!count)inform("NO WORKERS AVAILABLE");
                break;
            }
            case HUDCommand::FocusBase:{
                bool found=false;
                for(auto& unit:game.units)if(unit.isAlive() && unit.ownerId==0 &&
                                           unit.id==game.playerBaseId()) {
                    for(auto& item:game.units)item.isSelected=false;
                    unit.isSelected=true;
                    camera.focalPoint=unit.position;
                    camera.targetFocalPoint=unit.position;
                    found=true;break;
                }
                if(!found)inform("NO COMMAND CENTER");
                break;
            }
            case HUDCommand::Cancel:mode=Mode::Normal;break;
            default:break;
        }
    }
};


void reportInputFailure(GLFWwindow* window,const char* source,const char* message) noexcept {
    try {
        auto* state=static_cast<AppState*>(glfwGetWindowUserPointer(window));
        if(state) {
            state->inform(std::string("EDITOR ERROR: ")+source+" - "+message);
            state->editor.message=state->notice;
            if(state->mouseStroke) {
                state->editor.finishStroke(state->project);
                state->mouseStroke=false;
            }
        }
        std::cerr<<"[Editor] "<<source<<": "<<message<<"\n";
        std::ofstream log("rts-editor-error.log",std::ios::app);
        if(log)log<<"[Editor] "<<source<<": "<<message<<"\n";
    }catch(...) {std::cerr<<"[Editor] exception while reporting callback error\n";}
}

// Exercise the REAL OpenGL editor lifecycle under CI, not just the pure
// authoring libraries. This is an in-memory harness: no writes or user input.
void exerciseEditorLifecycle(AppState& app,int frame) {
    if(frame%12!=0)return;
    auto& ed=app.editor;
    const int phase=(frame/12)%8;
    const float x=2.f+float((frame/96)%4)*3.f;
    switch(phase) {
    case 0:
        ed.panel=EditorSession::Panel::Terrain;
        ed.tool=EditorSession::Tool::Raise;
        ed.startStroke(app.project);
        ed.paint(app.project,{x,0.f,2.f});
        ed.finishStroke(app.project);
        app.reloadFromProject();
        break;
    case 1:
        ed.panel=EditorSession::Panel::Scene;
        ed.selectedPlacementId.clear();
        ed.cycleSceneSelection(app.project,1);
        ed.rotateSelectedPlacement(app.project,15.f);
        app.reloadFromProject();
        break;
    case 2:
        ed.panel=EditorSession::Panel::Triggers;
        ed.createTrigger(app.project);
        app.reloadFromProject();
        break;
    case 3:
        ed.panel=EditorSession::Panel::Triggers;
        if(ed.beginTriggerEdit(app.project)){
            // Parse and replace the selected rule, then reinitialize simulation
            // to exercise reconfiguration during a live GPU context.
            ed.finishTriggerEdit(app.project);
            app.reloadFromProject();
        }
        break;
    case 4:
        ed.panel=EditorSession::Panel::Data;
        ed.beginCatalogSearch();
        ed.editText="unit";
        ed.finishCatalogSearch(app.project);
        break;
    case 5:
        ed.panel=EditorSession::Panel::Data;
        ed.duplicateCatalogObject(app.project);
        app.reloadFromProject();
        break;
    case 6:
        if(ed.undo(app.project))app.reloadFromProject();
        break;
    case 7:
        if(ed.redo(app.project))app.reloadFromProject();
        break;
    }
}
void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state) {
        state->framebufferWidth = width;
        state->framebufferHeight = height;
    }
}

void windowSizeCallback(GLFWwindow* window, int width, int height) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state) {
        state->windowWidth = width;
        state->windowHeight = height;
    }
}
void scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    try {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state) {
        if(state->editor.panel==EditorSession::Panel::Terrain &&
           glfwGetKey(window,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS) {
            state->editor.brushRadius=std::clamp(state->editor.brushRadius+
                float(yoffset),.5f,32.0f);
        }else state->camera.onMouseScroll(yoffset);
    }
    } catch(const std::exception& e) {
        reportInputFailure(window,"scrollCallback",e.what());
    } catch(...) {
        reportInputFailure(window,"scrollCallback","unknown exception");
    }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    try {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state) {
        if(state->editor.panel!=EditorSession::Panel::Play) {
            Vec3 hit;
            state->hoveredTerrain=state->camera.screenToGroundRaycast(
                xpos,ypos,state->windowWidth,state->windowHeight,hit);
            if(state->hoveredTerrain)state->hoveredWorld=hit;
            if(state->mouseStroke && state->hoveredTerrain &&
               state->editor.panel==EditorSession::Panel::Terrain)
                state->editor.paint(state->project,hit);
        }else state->selectionManager.onMouseMove(xpos,ypos);
    }
    } catch(const std::exception& e) {
        reportInputFailure(window,"cursorPosCallback",e.what());
    } catch(...) {
        reportInputFailure(window,"cursorPosCallback","unknown exception");
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    try {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state) return;

    double mouseX = 0.0, mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    if(state->editor.panel==EditorSession::Panel::Play) {
        RTSHUDLayout hud(state->windowWidth,state->windowHeight);
        if(hud.consumes(mouseX,mouseY)){
            if(button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_RELEASE)
                state->selectionManager.isDragging=false;
            if(button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_PRESS){
                const int selectedId=hud.selectionUnitAt(mouseX,mouseY,state->game);
                if(selectedId>=0){
                    const bool additive=(mods&GLFW_MOD_SHIFT)!=0;
                    for(auto& unit:state->game.units) {
                        if(additive){
                            if(unit.id==selectedId)unit.isSelected=!unit.isSelected;
                        }else unit.isSelected=unit.isAlive() && unit.id==selectedId;
                    }
                }else state->command(hud.hit(mouseX,mouseY,state->game),mouseX,mouseY);
            }
            return; // even a HUD background click must never issue a world command
        }
    }
    if(state->editor.panel==EditorSession::Panel::Scene) {
        if(button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_PRESS) {
            Vec3 hit;
            if(state->camera.screenToGroundRaycast(mouseX,mouseY,
                   state->windowWidth,state->windowHeight,hit)) {
                if((mods & GLFW_MOD_SHIFT) && state->editor.selectedPlacement(state->project)) {
                    if(state->editor.moveSelectedPlacement(state->project,hit))
                        state->reloadFromProject();
                } else state->editor.selectNearestPlacement(state->project,hit);
            }
        }
        return;
    }
    if(state->editor.panel!=EditorSession::Panel::Play) {
        if(state->editor.panel!=EditorSession::Panel::Terrain)return;
        Vec3 hit;
        bool hits=state->camera.screenToGroundRaycast(
            mouseX,mouseY,state->windowWidth,state->windowHeight,hit);
        if(button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_PRESS && hits) {
            if(state->editor.tool==EditorSession::Tool::PlaceMineral ||
               state->editor.tool==EditorSession::Tool::PlaceUnit ||
               state->editor.tool==EditorSession::Tool::PlaceDoodad) {
                if(state->editor.place(state->project,hit))state->reloadFromProject();
            }else{
                state->editor.startStroke(state->project);
                state->mouseStroke=true;
                state->editor.paint(state->project,hit);
            }
        }else if(button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_RELEASE) {
            if(state->mouseStroke) {
                state->editor.finishStroke(state->project);
                state->mouseStroke=false;
                state->game.syncTerrain(state->project.terrain);
            }
        }else if(button==GLFW_MOUSE_BUTTON_RIGHT && action==GLFW_PRESS && hits) {
            if(state->editor.removeNearest(state->project,hit))state->reloadFromProject();
        }
        return;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            state->selectionManager.onLeftMouseDown(mouseX, mouseY);
        } else if (action == GLFW_RELEASE) {
            bool shiftHeld = (mods & GLFW_MOD_SHIFT) != 0;
            state->selectionManager.onLeftMouseUp(
                mouseX, mouseY, shiftHeld,
                state->camera, state->windowWidth, state->windowHeight,
                state->game.units,&state->game.vision
            );
        }
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        Vec3 hit;
        if (!state->camera.screenToGroundRaycast(mouseX, mouseY,
                state->windowWidth, state->windowHeight, hit)) return;
        state->lastClickedWorldPos = hit;
        state->hasMoveMarker = true;

        if (state->mode == AppState::Mode::BuildBarracks) {
            if (!state->game.buildBarracks(hit))
                std::cout << "[Build] Requires a selected worker nearby, 150 minerals and clear terrain.\n";
            state->mode = AppState::Mode::Normal;
            return;
        }
        if (state->mode == AppState::Mode::Patrol) {
            if(!state->game.orderPatrol(hit))state->inform("NO VALID PATROL ROUTE");
            state->mode=AppState::Mode::Normal;
            return;
        }
        if (state->mode == AppState::Mode::Move) {
            if(!state->game.orderMove(hit,false,(mods & GLFW_MOD_SHIFT)!=0))
                state->inform("NO VALID MOVE ROUTE");
            state->mode=AppState::Mode::Normal;
            return;
        }
        if (state->mode == AppState::Mode::AttackMove) {
            state->game.orderMove(hit, true, (mods & GLFW_MOD_SHIFT) != 0);
            state->mode = AppState::Mode::Normal;
            return;
        }
        Unit* enemy = state->selectionManager.raycastEnemyUnit(
            mouseX, mouseY, state->camera,
            state->windowWidth, state->windowHeight, state->game.units,
            &state->game.vision);
        if (enemy) {
            state->game.orderAttack(enemy->id);
        } else if(state->game.setRallyPoint(hit)) {
            state->inform("RALLY POINT UPDATED");
        } else if(!state->game.orderGather(hit)) {
            state->game.orderMove(hit,false,(mods & GLFW_MOD_SHIFT)!=0);
        }
    }
    } catch(const std::exception& e) {
        reportInputFailure(window,"mouseButtonCallback",e.what());
    } catch(...) {
        reportInputFailure(window,"mouseButtonCallback","unknown exception");
    }
}


void characterCallback(GLFWwindow* window,unsigned int codepoint) {
    try {
    auto* s=static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if(s && (s->editor.panel==EditorSession::Panel::Data ||
             s->editor.panel==EditorSession::Panel::Triggers ||
             s->editor.panel==EditorSession::Panel::Scene))
        s->editor.addChar(codepoint);
    } catch(const std::exception& e) {
        reportInputFailure(window,"characterCallback",e.what());
    } catch(...) {
        reportInputFailure(window,"characterCallback","unknown exception");
    }
}
void editorKeyCallback(GLFWwindow* window,int key,int /*scancode*/,int action,int mods) {
    try {
    if(action!=GLFW_PRESS && action!=GLFW_REPEAT)return;
    auto* s=static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if(!s)return;
    auto& edit=s->editor;
    const bool ctrl=(mods & GLFW_MOD_CONTROL)!=0;
    if(edit.textEditing) {
        if(ctrl && key==GLFW_KEY_V) {
            const char* clip=glfwGetClipboardString(window);
            if(clip)for(const unsigned char* p=reinterpret_cast<const unsigned char*>(clip);
                         *p;++p)edit.addChar(*p);
            return;
        }
        if(key==GLFW_KEY_ESCAPE){edit.cancelEdit();return;}
        if(key==GLFW_KEY_ENTER || key==GLFW_KEY_KP_ENTER) {
            if(action==GLFW_PRESS) {
                if(edit.filterEditing){
                    edit.finishCatalogSearch(s->project);
                    return;
                }
                RTSProject backup=s->project;
                const bool wasModified=edit.modified;
                bool changed=false;
                try {
                    changed=edit.panel==EditorSession::Panel::Triggers
                        ? edit.finishTriggerEdit(s->project)
                        : edit.panel==EditorSession::Panel::Scene
                        ? edit.finishSceneOverrideEdit(s->project)
                        : edit.finishEdit(s->project);
                    if(changed && !edit.textEditing){
                        TriggerEngine::validate(s->project);
                        s->reloadFromProject();
                    }
                }catch(const std::exception& e){
                    s->project=std::move(backup);
                    if(changed)edit.discardRejectedChange();
                    edit.modified=wasModified;
                    edit.message=std::string("INVALID EDIT: ")+e.what();
                    try{s->reloadFromProject();}catch(...) {}
                }
            }
            return;
        }
        if(key==GLFW_KEY_BACKSPACE){edit.backspace();return;}
        return;
    }
    if(action==GLFW_REPEAT && key!=GLFW_KEY_LEFT_BRACKET && key!=GLFW_KEY_RIGHT_BRACKET)
        return;
    if(ctrl && key==GLFW_KEY_S) {s->saveProject();return;}
    if(key==GLFW_KEY_F5) {
        edit.panel=edit.panel==EditorSession::Panel::Terrain?
            EditorSession::Panel::Play:EditorSession::Panel::Terrain;
        if(edit.panel==EditorSession::Panel::Play)s->reloadFromProject();
        return;
    }
    if(key==GLFW_KEY_F6) {
        edit.panel=edit.panel==EditorSession::Panel::Triggers?
            EditorSession::Panel::Play:EditorSession::Panel::Triggers;
        if(edit.panel==EditorSession::Panel::Play)s->reloadFromProject();
        return;
    }
    if(key==GLFW_KEY_F8) {
        edit.panel=edit.panel==EditorSession::Panel::Scene?
            EditorSession::Panel::Play:EditorSession::Panel::Scene;
        if(edit.panel==EditorSession::Panel::Play)s->reloadFromProject();
        return;
    }
    if(key==GLFW_KEY_F7) {
        edit.panel=edit.panel==EditorSession::Panel::Data?
            EditorSession::Panel::Play:EditorSession::Panel::Data;
        if(edit.panel==EditorSession::Panel::Play)s->reloadFromProject();
        return;
    }
    if(key==GLFW_KEY_F9) {edit.panel=EditorSession::Panel::Play;s->reloadFromProject();return;}
    if(key==GLFW_KEY_ESCAPE) {
        if(edit.panel!=EditorSession::Panel::Play) {
            edit.panel=EditorSession::Panel::Play;s->reloadFromProject();
        }else if(s->mode!=AppState::Mode::Normal) {
            s->mode=AppState::Mode::Normal;
            s->inform("COMMAND CANCELLED");
        }else glfwSetWindowShouldClose(window,true);
        return;
    }
    if(edit.panel==EditorSession::Panel::Play)return;
    try{
        if(ctrl && key==GLFW_KEY_Z) {
            if(edit.undo(s->project))s->reloadFromProject();
            return;
        }
        if(ctrl && key==GLFW_KEY_Y) {
            if(edit.redo(s->project))s->reloadFromProject();
            return;
        }
        if(edit.panel==EditorSession::Panel::Scene) {
            if(key==GLFW_KEY_TAB) {
                edit.cycleSceneSelection(s->project,(mods & GLFW_MOD_SHIFT)?-1:1);return;
            }
            if(ctrl && key==GLFW_KEY_D) {
                if(edit.duplicateSelectedPlacement(s->project))s->reloadFromProject();return;
            }
            if(key==GLFW_KEY_DELETE) {
                const auto* selected=edit.selectedPlacement(s->project);
                if(selected && selected->kind=="Region") {
                    auto triggers=s->project.extensions.find("triggers");
                    if(triggers!=s->project.extensions.end() &&
                       triggers->second.is<picojson::array>()) {
                        for(const auto& rule:triggers->second.get<picojson::array>()){
                            if(!rule.is<picojson::object>())continue;
                            const auto& r=rule.get<picojson::object>();
                            auto event=r.find("event");
                            if(event==r.end() || !event->second.is<picojson::object>())continue;
                            const auto& fields=event->second.get<picojson::object>();
                            auto region=fields.find("region");
                            if(region!=fields.end() && region->second.is<std::string>() &&
                               region->second.get<std::string>()==selected->id){
                                edit.message="REGION IS REFERENCED BY A TRIGGER";return;
                            }
                        }
                    }
                }
                if(edit.deleteSelectedPlacement(s->project))s->reloadFromProject();return;
            }
            if(key==GLFW_KEY_ENTER || key==GLFW_KEY_KP_ENTER) {
                edit.beginSceneOverrideEdit(s->project);return;
            }
            if(key==GLFW_KEY_P && s->hoveredTerrain) {
                if(edit.placeSceneMarker(s->project,s->hoveredWorld,false))s->reloadFromProject();
                return;
            }
            if(key==GLFW_KEY_R && s->hoveredTerrain) {
                if(edit.placeSceneMarker(s->project,s->hoveredWorld,true))s->reloadFromProject();
                return;
            }
            if(key==GLFW_KEY_G && s->hoveredTerrain) {
                if(edit.moveSelectedPlacement(s->project,s->hoveredWorld))s->reloadFromProject();
                return;
            }
            if(key==GLFW_KEY_J || key==GLFW_KEY_K) {
                if(edit.rotateSelectedPlacement(s->project,key==GLFW_KEY_J?-15.f:15.f))
                    s->reloadFromProject();
                return;
            }
            if(key==GLFW_KEY_COMMA || key==GLFW_KEY_PERIOD) {
                if(edit.scaleSelectedPlacement(s->project,key==GLFW_KEY_COMMA?.9f:1.1f))
                    s->reloadFromProject();
                return;
            }
            if(key==GLFW_KEY_O) {
                if(edit.toggleSelectedOwner(s->project))s->reloadFromProject();return;
            }
            if(key==GLFW_KEY_LEFT_BRACKET || key==GLFW_KEY_RIGHT_BRACKET) {
                if(edit.adjustSelectedRegionRadius(s->project,
                   key==GLFW_KEY_LEFT_BRACKET?-.5f:.5f))s->reloadFromProject();
                return;
            }
        }
        if(edit.panel==EditorSession::Panel::Triggers) {
            if(ctrl && key==GLFW_KEY_N) {edit.createTrigger(s->project);return;}
            if(ctrl && key==GLFW_KEY_D) {edit.duplicateTrigger(s->project);return;}
            if(ctrl && key==GLFW_KEY_C) {
                const auto json=edit.triggerJSON(s->project,edit.selectedTrigger);
                if(!json.empty())glfwSetClipboardString(window,json.c_str());
                return;
            }
            if(key==GLFW_KEY_DELETE) {edit.deleteTrigger(s->project);return;}
            if(key==GLFW_KEY_UP || key==GLFW_KEY_DOWN) {
                edit.cycleTrigger(s->project,key==GLFW_KEY_UP?-1:1);return;
            }
            if(key==GLFW_KEY_ENTER || key==GLFW_KEY_KP_ENTER) {
                edit.beginTriggerEdit(s->project);return;
            }
        }
        if(edit.panel==EditorSession::Panel::Data) {
            if(ctrl && key==GLFW_KEY_F){
                if(mods & GLFW_MOD_SHIFT)edit.clearCatalogSearch(s->project);
                else edit.beginCatalogSearch();
                return;
            }
            if(ctrl && key==GLFW_KEY_D){edit.duplicateCatalogObject(s->project);return;}
            if(ctrl && key==GLFW_KEY_N){edit.beginNewField();return;}
            if(ctrl && key==GLFW_KEY_BACKSPACE) {
                if(edit.removeLocalOverride(s->project))s->reloadFromProject();
                return;
            }
            if(key==GLFW_KEY_TAB)edit.cycleObject(s->project,
                                                    (mods & GLFW_MOD_SHIFT)?-1:1);
            else if(key==GLFW_KEY_UP)edit.cycleField(s->project,-1);
            else if(key==GLFW_KEY_DOWN)edit.cycleField(s->project,1);
            else if(key==GLFW_KEY_ENTER || key==GLFW_KEY_KP_ENTER)
                edit.beginValueEdit(s->project);
        }else if(edit.panel==EditorSession::Panel::Terrain) {
            if(key>=GLFW_KEY_1 && key<=GLFW_KEY_7)
                edit.tool=static_cast<EditorSession::Tool>(key-GLFW_KEY_1);
            else if(key==GLFW_KEY_8)edit.tool=EditorSession::Tool::PlaceMineral;
            else if(key==GLFW_KEY_9)edit.tool=EditorSession::Tool::PlaceUnit;
            else if(key==GLFW_KEY_0)edit.tool=EditorSession::Tool::PlaceDoodad;
            else if(key==GLFW_KEY_LEFT_BRACKET)
                edit.brushRadius=std::max(.5f,edit.brushRadius-.5f);
            else if(key==GLFW_KEY_RIGHT_BRACKET)
                edit.brushRadius=std::min(32.f,edit.brushRadius+.5f);
            else if(key==GLFW_KEY_MINUS)
                edit.brushStrength=std::max(.1f,edit.brushStrength-.1f);
            else if(key==GLFW_KEY_EQUAL)
                edit.brushStrength=std::min(10.f,edit.brushStrength+.1f);
            else if(key==GLFW_KEY_M)edit.materialId=(edit.materialId+1)%8;
            else if(key==GLFW_KEY_O)edit.placementOwner=1-edit.placementOwner;
            else if(key==GLFW_KEY_P)
                edit.cyclePlacementCatalog(s->project,1,(mods & GLFW_MOD_SHIFT)!=0);
            else if(key==GLFW_KEY_DELETE && s->hoveredTerrain) {
                if(edit.removeNearest(s->project,s->hoveredWorld))s->reloadFromProject();
            }
        }
    }catch(const std::exception& e) {edit.message=std::string("EDITOR ERROR: ")+e.what();}
    } catch(const std::exception& e) {
        reportInputFailure(window,"editorKeyCallback",e.what());
    } catch(...) {
        reportInputFailure(window,"editorKeyCallback","unknown exception");
    }
}

int main(int argc, char** argv) {
    RTSCrashReporter::install();
    int smokeFrames = -1;
    std::string screenshotPath;
    std::string inputProjectPath;
    bool startEditor=false;
    bool startTriggerEditor=false;
    bool startSceneEditor=false;
    bool startDataEditor=false;
    bool selectArmyForSmoke=false;
    bool stressEditor=false;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--smoke-frames" && i + 1 < argc)
            smokeFrames = (std::max)(1, std::atoi(argv[++i]));
        else if (arg == "--screenshot" && i + 1 < argc)
            screenshotPath = argv[++i];
        else if(arg == "--project" && i+1<argc)inputProjectPath=argv[++i];
        else if(arg == "--editor")startEditor=true;
        else if(arg == "--trigger-editor")startTriggerEditor=true;
        else if(arg == "--scene-editor")startSceneEditor=true;
        else if(arg == "--data-editor")startDataEditor=true;
        else if(arg == "--smoke-select-army")selectArmyForSmoke=true;
        else if(arg == "--editor-stress-smoke")stressEditor=true;
    }
    int renderedFrames = 0;
    try {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW." << std::endl;
            return -1;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        AppState app;
        if(!inputProjectPath.empty()) {
            app.project.loadFile(inputProjectPath);
            app.projectPath=inputProjectPath;
        }else if(std::filesystem::exists("projects/default.rtsproject.json")) {
            app.project.loadFile("projects/default.rtsproject.json");
        }
        app.recoverAutosave();
        app.reloadFromProject();
        if(selectArmyForSmoke && smokeFrames>0)
            app.command(HUDCommand::SelectArmy);
        if(startEditor || startTriggerEditor || startSceneEditor || startDataEditor ||
           (stressEditor && smokeFrames>0)) {
            app.editor.panel=startDataEditor ? EditorSession::Panel::Data :
                startSceneEditor ? EditorSession::Panel::Scene :
                startTriggerEditor ? EditorSession::Panel::Triggers :
                                     EditorSession::Panel::Terrain;
            auto preset=app.project.metadata.find("editor_camera");
            if(preset!=app.project.metadata.end() && preset->second.is<picojson::object>()) {
                const auto& cam=preset->second.get<picojson::object>();
                auto read=[&](const char* k,float def) {
                    auto it=cam.find(k);
                    return (it!=cam.end() && it->second.is<double>() &&
                            std::isfinite(it->second.get<double>()))
                        ? float(it->second.get<double>()) : def;
                };
                app.camera.focalPoint=Vec3(read("x",0),0,read("z",0));
                app.camera.targetFocalPoint=app.camera.focalPoint;
                app.camera.yawDegrees=read("yaw",0);
                app.camera.currentZoomDistance=
                    std::clamp(read("zoom",42),app.camera.minZoomDistance,app.camera.maxZoomDistance);
                app.camera.targetZoomDistance=app.camera.currentZoomDistance;
            }
        }
        GLFWwindow* window = glfwCreateWindow(
            app.windowWidth, app.windowHeight,
            "3D RTS Engine - Modular Architecture",
            nullptr, nullptr
        );
        if (!window) {
            std::cerr << "Failed to create GLFW window." << std::endl;
            glfwTerminate();
            return -1;
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        if (!loadOpenGLFunctions()) {
            std::cerr << "Failed to load OpenGL 3.3 Core functions." << std::endl;
            glfwTerminate();
            return -1;
        }

        glfwSetWindowUserPointer(window, &app);
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
        glfwSetWindowSizeCallback(window, windowSizeCallback);
        glfwGetWindowSize(window, &app.windowWidth, &app.windowHeight);
        glfwGetFramebufferSize(window, &app.framebufferWidth, &app.framebufferHeight);
        glViewport(0, 0, app.framebufferWidth, app.framebufferHeight);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetKeyCallback(window,editorKeyCallback);
        glfwSetCharCallback(window,characterCallback);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        GLuint terrainShader = compileShaderProgram(TERRAIN_VERTEX_SHADER, TERRAIN_FRAGMENT_SHADER);
        GLuint unitShader    = compileShaderProgram(UNIT_VERTEX_SHADER, UNIT_FRAGMENT_SHADER);
        GLuint ringShader    = compileShaderProgram(RING_VERTEX_SHADER, RING_FRAGMENT_SHADER);
        GLuint hudBoxShader  = compileShaderProgram(HUD_BOX_VERTEX_SHADER, HUD_BOX_FRAGMENT_SHADER);
        GLuint lineShader    = compileShaderProgram(LINE_VERTEX_SHADER, HUD_BOX_FRAGMENT_SHADER);

        // Streaming 1-channel vision texture, one texel per terrain cell.
        // Uploads are revision-gated; this does not rebuild the terrain VBO.
        GLuint fogTexture=0;
        glGenTextures(1,&fogTexture);
        glBindTexture(GL_TEXTURE_2D,fogTexture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        int uploadedFogWidth=0,uploadedFogHeight=0;
        std::size_t uploadedFogRevision=0;

        // The skirmish owns unit lifetimes and initial spawning; see RTSSkirmish.

        // Editable heightfield mesh: five floats per vertex, updated after brush strokes.
        std::vector<float> terrainVertices=app.project.terrain.buildMesh(app.terrainSlopeLimit());
        GLsizei terrainVertexCount=static_cast<GLsizei>(terrainVertices.size()/5);
        GLuint terrainVAO, terrainVBO;
        glGenVertexArrays(1, &terrainVAO);
        glGenBuffers(1, &terrainVBO);
        glBindVertexArray(terrainVAO);
        glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
        glBufferData(GL_ARRAY_BUFFER, terrainVertices.size()*sizeof(float),
                     terrainVertices.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(4*sizeof(float)));
        glEnableVertexAttribArray(2);
        app.project.terrain.clearDirty();

        // 2. 3D Unit Mesh Geometry
        float unitVertices[] = {
            -0.45f, 0.0f,  0.45f,  0, 0, 1,   0.45f, 0.0f,  0.45f,  0, 0, 1,   0.35f, 1.45f,  0.35f,  0, 0, 1,
            -0.45f, 0.0f,  0.45f,  0, 0, 1,   0.35f, 1.45f,  0.35f,  0, 0, 1,  -0.35f, 1.45f,  0.35f,  0, 0, 1,
             0.45f, 0.0f, -0.45f,  0, 0,-1,  -0.45f, 0.0f, -0.45f,  0, 0,-1,  -0.35f, 1.45f, -0.35f,  0, 0,-1,
             0.45f, 0.0f, -0.45f,  0, 0,-1,  -0.35f, 1.45f, -0.35f,  0, 0,-1,   0.35f, 1.45f, -0.35f,  0, 0,-1,
            -0.45f, 0.0f, -0.45f, -1, 0, 0,  -0.45f, 0.0f,  0.45f, -1, 0, 0,  -0.35f, 1.45f,  0.35f, -1, 0, 0,
            -0.45f, 0.0f, -0.45f, -1, 0, 0,  -0.35f, 1.45f,  0.35f, -1, 0, 0,  -0.35f, 1.45f, -0.35f, -1, 0, 0,
             0.45f, 0.0f,  0.45f,  1, 0, 0,   0.45f, 0.0f, -0.45f,  1, 0, 0,   0.35f, 1.45f, -0.35f,  1, 0, 0,
             0.45f, 0.0f,  0.45f,  1, 0, 0,   0.35f, 1.45f, -0.35f,  1, 0, 0,   0.35f, 1.45f,  0.35f,  1, 0, 0,
            -0.35f, 1.45f,  0.35f, 0, 1, 0,   0.35f, 1.45f,  0.35f, 0, 1, 0,   0.35f, 1.45f, -0.35f, 0, 1, 0,
            -0.35f, 1.45f,  0.35f, 0, 1, 0,   0.35f, 1.45f, -0.35f, 0, 1, 0,  -0.35f, 1.45f, -0.35f, 0, 1, 0,
            -0.22f, 1.05f,  0.38f, 0, 0, 1,   0.22f, 1.05f,  0.38f, 0, 0, 1,   0.00f, 1.25f,  0.65f, 0, 0.4f, 0.9f
        };
        GLuint unitVAO, unitVBO;
        glGenVertexArrays(1, &unitVAO);
        glGenBuffers(1, &unitVBO);
        glBindVertexArray(unitVAO);
        glBindBuffer(GL_ARRAY_BUFFER, unitVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(unitVertices), unitVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // 3. Selection Ring Quad Geometry
        float ringVertices[] = {
            -1.0f, 0.02f, -1.0f,   1.0f, 0.02f, -1.0f,   1.0f, 0.02f,  1.0f,
            -1.0f, 0.02f, -1.0f,   1.0f, 0.02f,  1.0f,  -1.0f, 0.02f,  1.0f
        };
        GLuint ringVAO, ringVBO;
        glGenVertexArrays(1, &ringVAO);
        glGenBuffers(1, &ringVBO);
        glBindVertexArray(ringVAO);
        glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(ringVertices), ringVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // 4. Dynamic 2D HUD Drag-Selection Box VAO/VBO
        GLuint hudVAO, hudVBO;
        glGenVertexArrays(1, &hudVAO);
        glGenBuffers(1, &hudVBO);
        glBindVertexArray(hudVAO);
        glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
        glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // 5. Dynamic 3D Attack Tracer Line VAO/VBO
        GLuint lineVAO, lineVBO;
        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);
        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, 2 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        std::cout << "RTS Skirmish: LMB select, RMB move/attack/gather, A then RMB attack-move,\n"
                  << "B then RMB place Barracks (worker needed), T train Marine, C train Worker,\n"
                  << "X stop, Q/E rotate, arrows/W/S/D camera, R restart, Esc quit.\n";
        float accumulator = 0.0f;
        const float fixedStep = 1.0f / 60.0f;
        double lastTitleUpdate = 0.0;
        double lastTime = glfwGetTime();
        double lastAutosave=lastTime;

        while (!glfwWindowShouldClose(window)) {
            if(stressEditor && smokeFrames>0)
                exerciseEditorLifecycle(app,renderedFrames);
            double currentTime = glfwGetTime();
            float deltaTime = std::clamp(static_cast<float>(currentTime - lastTime), 0.0f, 0.25f);
            lastTime = currentTime;
            if(currentTime-lastAutosave>=25.0){
                app.autoSave();
                lastAutosave=currentTime;
            }
            if(app.noticeSeconds>0.f) {
                app.noticeSeconds=std::max(0.f,app.noticeSeconds-deltaTime);
                if(app.noticeSeconds==0.f)app.notice.clear();
            }

            auto justPressed = [&](int key, bool& previous) {
                bool down = glfwGetKey(window, key) == GLFW_PRESS;
                bool fired = down && !previous;
                previous = down;
                return fired;
            };
            if(app.editor.panel==EditorSession::Panel::Play) {
            if (justPressed(GLFW_KEY_M, app.keyM)) app.command(HUDCommand::Move);
            if (justPressed(GLFW_KEY_P, app.keyP)) app.command(HUDCommand::Patrol);
            if (justPressed(GLFW_KEY_V, app.keyV)) app.command(HUDCommand::CancelQueue);
            if (justPressed(GLFW_KEY_A, app.keyA)) app.command(HUDCommand::AttackMove);
            if (justPressed(GLFW_KEY_B, app.keyB)) app.command(HUDCommand::Build);
            if (justPressed(GLFW_KEY_T, app.keyT)) app.command(HUDCommand::Marine);
            if (justPressed(GLFW_KEY_C, app.keyC)) app.command(HUDCommand::Worker);
            if (justPressed(GLFW_KEY_X, app.keyX)) app.command(HUDCommand::Stop);
            if (justPressed(GLFW_KEY_F2, app.keyF2)) app.command(HUDCommand::SelectArmy);
            if (justPressed(GLFW_KEY_F3, app.keyF3)) app.command(HUDCommand::SelectWorkers);
            if (justPressed(GLFW_KEY_G, app.keyG)) app.command(HUDCommand::Hold);
            if (justPressed(GLFW_KEY_H, app.keyH)) app.command(HUDCommand::FocusBase);
            if (justPressed(GLFW_KEY_R, app.keyR)) {
                app.game.reset();
                app.controlGroups = {};
                app.mode = AppState::Mode::Normal;
                app.hasMoveMarker = false;
            }
            bool controlHeld = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                               glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            for(int digit=1;digit<=9;++digit) {
                if(!justPressed(GLFW_KEY_0+digit,app.digitWasDown[static_cast<size_t>(digit)]))continue;
                auto& group=app.controlGroups[static_cast<size_t>(digit)];
                if(controlHeld) {
                    group.clear();
                    for(const auto& u:app.game.units)
                        if(u.isSelected && u.isAlive() && u.ownerId==0)group.push_back(u.id);
                } else {
                    for(auto& u:app.game.units) {
                        u.isSelected=false;
                        if(u.isAlive() && u.ownerId==0 &&
                           std::find(group.begin(),group.end(),u.id)!=group.end())
                            u.isSelected=true;
                    }
                }
            }
            }
            if(!app.editor.textEditing) {
                if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) app.camera.yawDegrees -= 75.0f * deltaTime;
                if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) app.camera.yawDegrees += 75.0f * deltaTime;
            }

            if(!app.editor.textEditing)
                app.camera.update(deltaTime, window, app.windowWidth, app.windowHeight);
            if(app.editor.panel==EditorSession::Panel::Play) {
                accumulator += deltaTime;
                int steps = 0;
                while (accumulator >= fixedStep && steps < 8) {
                    try{app.game.update(fixedStep);}
                    catch(const std::exception& e) {
                        // Do not tear down the editor on malformed game scripting.
                        reportInputFailure(window,"simulation",e.what());
                        app.editor.panel=EditorSession::Panel::Scene;
                        accumulator=0.f;break;
                    }
                    accumulator -= fixedStep;
                    ++steps;
                }
                if (steps == 8 && accumulator >= fixedStep)
                    accumulator = std::fmod(accumulator, fixedStep);
            }else accumulator=0.0f; // editor never advances gameplay

            if (currentTime - lastTitleUpdate > 0.35) {
                const char* mode = app.mode == AppState::Mode::BuildBarracks ? "PLACE BARRACKS: RMB" :
                    app.mode == AppState::Mode::AttackMove ? "ATTACK MOVE: RMB" :
                    app.mode == AppState::Mode::Move ? "MOVE: RMB" :
                    app.mode == AppState::Mode::Patrol ? "PATROL: RMB" : "COMMAND";
                std::string title = app.editor.panel==EditorSession::Panel::Play
                    ? std::string("3D RTS | ")+app.game.statusLine()+" | "+mode
                    : std::string("3D RTS AUTHORING | ")+app.project.name+
                      " | "+app.editor.panelName()+(app.editor.modified?" *":"");
                glfwSetWindowTitle(window, title.c_str());
                lastTitleUpdate = currentTime;
            }

            if(app.project.terrain.dirty()) {
                terrainVertices=app.project.terrain.buildMesh(app.terrainSlopeLimit());
                terrainVertexCount=static_cast<GLsizei>(terrainVertices.size()/5);
                glBindVertexArray(terrainVAO);
                glBindBuffer(GL_ARRAY_BUFFER,terrainVBO);
                glBufferData(GL_ARRAY_BUFFER,terrainVertices.size()*sizeof(float),
                             terrainVertices.data(),GL_DYNAMIC_DRAW);
                app.project.terrain.clearDirty();
            }
            const auto& vision=app.game.vision;
            const int visW=vision.width(),visH=vision.depth();
            if(visW>0 && visH>0 &&
               (uploadedFogRevision!=vision.revision() ||
                uploadedFogWidth!=visW || uploadedFogHeight!=visH)){
                glBindTexture(GL_TEXTURE_2D,fogTexture);
                const auto& texels=vision.textureBytes();
                if(uploadedFogWidth!=visW || uploadedFogHeight!=visH){
                    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,visW,visH,0,GL_RED,
                                 GL_UNSIGNED_BYTE,texels.data());
                    uploadedFogWidth=visW;
                    uploadedFogHeight=visH;
                }else{
                    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,visW,visH,GL_RED,
                                    GL_UNSIGNED_BYTE,texels.data());
                }
                uploadedFogRevision=vision.revision();
            }
            glClearColor(0.08f, 0.11f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            float aspect = (app.windowHeight > 0)
                ? static_cast<float>(app.windowWidth) / static_cast<float>(app.windowHeight)
                : 1.0f;

            Mat4 view = app.camera.getViewMatrix();
            Mat4 proj = app.camera.getProjectionMatrix(aspect);

            // 1. Render Terrain
            glUseProgram(terrainShader);
            glBindTexture(GL_TEXTURE_2D,fogTexture);
            glUniform1i(glGetUniformLocation(terrainShader,"uFogMap"),0);
            glUniform4f(glGetUniformLocation(terrainShader,"uFogSettings"),
                app.project.terrain.minX(),app.project.terrain.minZ(),
                app.project.terrain.spacing(),
                app.editor.panel==EditorSession::Panel::Play?1.f:0.f);
            glUniform4f(glGetUniformLocation(terrainShader,"uFogSize"),
                float(visW),float(visH),0.f,0.f);
            Mat4 terrainModel;
            glUniformMatrix4fv(glGetUniformLocation(terrainShader, "uModel"), 1, GL_FALSE, terrainModel.m);
            glUniformMatrix4fv(glGetUniformLocation(terrainShader, "uView"), 1, GL_FALSE, view.m);
            glUniformMatrix4fv(glGetUniformLocation(terrainShader, "uProjection"), 1, GL_FALSE, proj.m);
            glUniform3f(glGetUniformLocation(terrainShader, "uClickMarker"),
                        app.lastClickedWorldPos.x, app.lastClickedWorldPos.y, app.lastClickedWorldPos.z);
            glUniform1f(glGetUniformLocation(terrainShader, "uMarkerPulse"), static_cast<float>(currentTime));
            glUniform1f(glGetUniformLocation(terrainShader, "uHasMarker"),
                        app.hasMoveMarker ? 1.0f : 0.0f);
            glUniform1f(glGetUniformLocation(terrainShader, "uShowPathing"),
                        app.editor.panel==EditorSession::Panel::Terrain ? 1.0f : 0.0f);
            glUniform1f(glGetUniformLocation(terrainShader, "uShowGrid"),
                        app.editor.panel==EditorSession::Panel::Terrain ? 1.0f : .35f);

            glBindVertexArray(terrainVAO);
            glDrawArrays(GL_TRIANGLES, 0, terrainVertexCount);

            // 2. Render 3D Alive Units, Rings & Weapon Tracers
            for (const auto& unit : app.game.units) {
                if (!unit.isAlive()) continue;
                if(app.editor.panel==EditorSession::Panel::Play &&
                   unit.ownerId!=0 && !app.game.vision.visible(unit.position))
                    continue;

                if (unit.isSelected) {
                    glUseProgram(ringShader);
                    float ringScale = 0.95f * unit.data.visual_scale;
                    Mat4 ringModel = Mat4::translate(unit.position) * Mat4::scale(Vec3(ringScale, 1.0f, ringScale));
                    glUniformMatrix4fv(glGetUniformLocation(ringShader, "uModel"), 1, GL_FALSE, ringModel.m);
                    glUniformMatrix4fv(glGetUniformLocation(ringShader, "uView"), 1, GL_FALSE, view.m);
                    glUniformMatrix4fv(glGetUniformLocation(ringShader, "uProjection"), 1, GL_FALSE, proj.m);
                    if (unit.getFaction() == Faction::Humanity) {
                        glUniform4f(glGetUniformLocation(ringShader, "uColor"), 0.15f, 0.98f, 0.42f, 0.92f);
                    } else {
                        glUniform4f(glGetUniformLocation(ringShader, "uColor"), 0.98f, 0.25f, 0.35f, 0.92f);
                    }

                    glBindVertexArray(ringVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }

                glUseProgram(unitShader);
                float s = unit.data.visual_scale;
                Mat4 unitModel = Mat4::translate(unit.position)
                               * Mat4::rotateY(unit.facingYawRad)
                               * Mat4::scale(Vec3(s*unit.modelScale.x,
                                                  s*unit.modelScale.y,s*unit.modelScale.z));
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uModel"), 1, GL_FALSE, unitModel.m);
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uView"), 1, GL_FALSE, view.m);
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uProjection"), 1, GL_FALSE, proj.m);
                glUniform3f(glGetUniformLocation(unitShader, "uColor"),
                            unit.data.faction_color.x, unit.data.faction_color.y, unit.data.faction_color.z);
                glUniform1f(glGetUniformLocation(unitShader, "uSelected"), unit.isSelected ? 1.0f : 0.0f);
                glUniform1f(glGetUniformLocation(unitShader, "uOpacity"),unit.actorOpacity);

                glBindVertexArray(unitVAO);
                glDrawArrays(GL_TRIANGLES, 0, 33);

                if (unit.attackFlashTimer > 0.0f) {
                    Vec3 muzzle = unit.position + Vec3(0.0f, unit.height * 0.65f, 0.0f);
                    float lineVerts[] = {
                        muzzle.x, muzzle.y, muzzle.z,
                        unit.lastAttackTargetPos.x, unit.lastAttackTargetPos.y, unit.lastAttackTargetPos.z
                    };

                    glUseProgram(lineShader);
                    glUniformMatrix4fv(glGetUniformLocation(lineShader, "uView"), 1, GL_FALSE, view.m);
                    glUniformMatrix4fv(glGetUniformLocation(lineShader, "uProjection"), 1, GL_FALSE, proj.m);
                    if (unit.getFaction() == Faction::Humanity) {
                        glUniform4f(glGetUniformLocation(lineShader, "uColor"), 1.0f, 0.88f, 0.25f, 0.95f);
                    } else {
                        glUniform4f(glGetUniformLocation(lineShader, "uColor"), 0.92f, 0.30f, 1.0f, 0.95f);
                    }

                    glBindVertexArray(lineVAO);
                    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lineVerts), lineVerts);
                    glDrawArrays(GL_LINES, 0, 2);
                }
            }

            // Mineral patches use the placeholder unit mesh until an asset pipeline exists.
            glUseProgram(unitShader);
            glUniformMatrix4fv(glGetUniformLocation(unitShader, "uView"), 1, GL_FALSE, view.m);
            glUniformMatrix4fv(glGetUniformLocation(unitShader, "uProjection"), 1, GL_FALSE, proj.m);
            glUniform3f(glGetUniformLocation(unitShader, "uColor"), 0.15f, 0.72f, 0.88f);
            glUniform1f(glGetUniformLocation(unitShader, "uSelected"), 0.0f);
            glUniform1f(glGetUniformLocation(unitShader, "uOpacity"), 1.0f);
            glBindVertexArray(unitVAO);
            for (const auto& node : app.game.minerals) {
                if(node.remaining<=0)continue;
                if(app.editor.panel==EditorSession::Panel::Play &&
                   !app.game.vision.explored(node.position))continue;
                Mat4 mesh = Mat4::translate(node.position)
                          * Mat4::scale(Vec3(1.35f, 1.1f, 1.35f));
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uModel"), 1, GL_FALSE, mesh.m);
                glDrawArrays(GL_TRIANGLES, 0, 33);
            }

            // Placed doodads resolve their actor color/scale/opacity. Imported mesh
            // filenames stay in the catalog; procedural placeholders are used until
            // a dedicated asset pipeline is introduced.
            for(const auto& placed:app.project.placements) {
                if(placed.kind!="Doodad")continue;
                if(app.editor.panel==EditorSession::Panel::Play &&
                   !app.game.vision.explored(placed.position))continue;
                const auto visualEntry=app.doodadVisuals.find(placed.id);
                if(visualEntry==app.doodadVisuals.end())continue;
                const ActorVisual& visual=visualEntry->second;
                Vec3 pos=placed.position;
                pos.y=app.project.terrain.sampleHeight(pos.x,pos.z);
                Mat4 transform=Mat4::translate(pos)
                    * Mat4::rotateY(toRadians(placed.rotation.y))
                    * Mat4::scale(Vec3(placed.scale.x*visual.scale.x,
                                       placed.scale.y*visual.scale.y,
                                       placed.scale.z*visual.scale.z));
                glUseProgram(unitShader);
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uView"),1,GL_FALSE,view.m);
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uProjection"),1,GL_FALSE,proj.m);
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uModel"),1,GL_FALSE,transform.m);
                glUniform3f(glGetUniformLocation(unitShader, "uColor"),
                            visual.tint.x,visual.tint.y,visual.tint.z);
                glUniform1f(glGetUniformLocation(unitShader, "uSelected"),0.0f);
                glUniform1f(glGetUniformLocation(unitShader, "uOpacity"),visual.opacity);
                glBindVertexArray(unitVAO);
                glDrawArrays(GL_TRIANGLES,0,33);
            }

            // Rally location marker for currently selected production buildings.
            for(const auto& building:app.game.units)if(building.isAlive() &&
                building.isSelected && building.isStructure && building.ownerId==0) {
                const auto it=app.game.rallyPoints().find(building.id);
                if(it==app.game.rallyPoints().end())continue;
                const Vec3 pos=app.project.terrain.clampToMap(it->second);
                Mat4 marker=Mat4::translate(pos+Vec3(0,.09f,0))
                    * Mat4::scale(Vec3(1.1f,1.f,1.1f));
                glUseProgram(ringShader);
                glUniformMatrix4fv(glGetUniformLocation(ringShader,"uModel"),1,GL_FALSE,marker.m);
                glUniformMatrix4fv(glGetUniformLocation(ringShader,"uView"),1,GL_FALSE,view.m);
                glUniformMatrix4fv(glGetUniformLocation(ringShader,"uProjection"),1,GL_FALSE,proj.m);
                glUniform4f(glGetUniformLocation(ringShader,"uColor"),.94f,.84f,.28f,.60f);
                glBindVertexArray(ringVAO);
                glDrawArrays(GL_TRIANGLES,0,6);
            }
            if(app.editor.panel==EditorSession::Panel::Terrain && app.hoveredTerrain) {
                glUseProgram(ringShader);
                Mat4 brush=Mat4::translate(app.hoveredWorld+Vec3(0,.06f,0))
                    * Mat4::scale(Vec3(app.editor.brushRadius,1,app.editor.brushRadius));
                glUniformMatrix4fv(glGetUniformLocation(ringShader, "uModel"),1,GL_FALSE,brush.m);
                glUniformMatrix4fv(glGetUniformLocation(ringShader, "uView"),1,GL_FALSE,view.m);
                glUniformMatrix4fv(glGetUniformLocation(ringShader, "uProjection"),1,GL_FALSE,proj.m);
                glUniform4f(glGetUniformLocation(ringShader, "uColor"),.94f,.8f,.21f,.77f);
                glBindVertexArray(ringVAO);
                glDrawArrays(GL_TRIANGLES,0,6);
            }

            // Selected scene objects and region extents. A lightweight ground
            // overlay is used until the editor has proper transform gizmos.
            if(app.editor.panel==EditorSession::Panel::Scene) {
                for(const auto& placed:app.project.placements) {
                    float radius=placed.kind=="Point"?.7f:1.4f;
                    const bool region=placed.kind=="Region";
                    if(region){
                        radius=5.0f;
                        auto it=placed.overrides.find("radius");
                        if(it!=placed.overrides.end() && it->second.is<double>())
                            radius=std::clamp(float(it->second.get<double>()),.5f,64.f);
                    }
                    const bool chosen=app.editor.selectedPlacementId==placed.id;
                    Vec3 pos=placed.position;
                    pos.y=app.project.terrain.sampleHeight(pos.x,pos.z);
                    Mat4 marker=Mat4::translate(pos+Vec3(0,.08f,0))
                        * Mat4::scale(Vec3(radius,1,radius));
                    glUseProgram(ringShader);
                    glUniformMatrix4fv(glGetUniformLocation(ringShader,"uModel"),1,GL_FALSE,marker.m);
                    glUniformMatrix4fv(glGetUniformLocation(ringShader,"uView"),1,GL_FALSE,view.m);
                    glUniformMatrix4fv(glGetUniformLocation(ringShader,"uProjection"),1,GL_FALSE,proj.m);
                    glUniform4f(glGetUniformLocation(ringShader,"uColor"),
                        chosen?1.f:region?.16f:.29f,
                        chosen?.78f:region?.78f:.66f,
                        chosen?.18f:region?1.f:.86f,
                        chosen?.55f:region?.13f:.24f);
                    glBindVertexArray(ringVAO);
                    glDrawArrays(GL_TRIANGLES,0,6);
                }
            }

            // 3. Render Floating 2D Screen-Projected Health Bars & Drag-Selection Box
            glDisable(GL_DEPTH_TEST);
            glUseProgram(hudBoxShader);
            glBindVertexArray(hudVAO);
            glBindBuffer(GL_ARRAY_BUFFER, hudVBO);

            if (app.windowWidth > 0 && app.windowHeight > 0) {
                auto toNdcX = [&](float px) { return (2.0f * px) / static_cast<float>(app.windowWidth) - 1.0f; };
                auto toNdcY = [&](float py) { return 1.0f - (2.0f * py) / static_cast<float>(app.windowHeight); };

                for (const auto& unit : app.game.units) {
                    if (!unit.isAlive()) continue;
                    if(app.editor.panel==EditorSession::Panel::Play &&
                       unit.ownerId!=0 && !app.game.vision.visible(unit.position))
                        continue;
                    Vec2 headScreen;
                    if (app.camera.worldToScreen(unit.position + Vec3(0.0f, unit.height + 0.45f, 0.0f),
                                                 app.windowWidth, app.windowHeight, headScreen)) {
                        float barW = 42.0f * unit.data.visual_scale;
                        float barH = 5.0f;
                        float hpRatio = std::clamp(unit.currentHp / unit.data.max_hp, 0.0f, 1.0f);

                        float bx0 = toNdcX(headScreen.x - barW * 0.5f);
                        float bx1 = toNdcX(headScreen.x + barW * 0.5f);
                        float by0 = toNdcY(headScreen.y - barH);
                        float by1 = toNdcY(headScreen.y);

                        float bgVerts[] = { bx0, by0, bx1, by0, bx1, by1, bx0, by0, bx1, by1, bx0, by1 };
                        glUniform4f(glGetUniformLocation(hudBoxShader, "uColor"), 0.08f, 0.08f, 0.10f, 0.85f);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(bgVerts), bgVerts);
                        glDrawArrays(GL_TRIANGLES, 0, 6);

                        float fx1 = toNdcX((headScreen.x - barW * 0.5f) + barW * hpRatio);
                        float fgVerts[] = { bx0, by0, fx1, by0, fx1, by1, bx0, by0, fx1, by1, bx0, by1 };
                        if (unit.getFaction() == Faction::Humanity) {
                            glUniform4f(glGetUniformLocation(hudBoxShader, "uColor"), 0.20f, 0.92f, 0.42f, 0.95f);
                        } else {
                            glUniform4f(glGetUniformLocation(hudBoxShader, "uColor"), 0.88f, 0.28f, 0.95f, 0.95f);
                        }
                        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fgVerts), fgVerts);
                        glDrawArrays(GL_TRIANGLES, 0, 6);
                    }
                }

                if (app.selectionManager.isDragging) {
                    float dragLen = (app.selectionManager.dragCurrentScreen - app.selectionManager.dragStartScreen).length();
                    if (dragLen >= app.selectionManager.dragThresholdPixels) {
                        float x0 = toNdcX(app.selectionManager.dragStartScreen.x);
                        float y0 = toNdcY(app.selectionManager.dragStartScreen.y);
                        float x1 = toNdcX(app.selectionManager.dragCurrentScreen.x);
                        float y1 = toNdcY(app.selectionManager.dragCurrentScreen.y);

                        float boxVerts[] = {
                            x0, y0,  x1, y0,  x1, y1,
                            x0, y0,  x1, y1,  x0, y1
                        };

                        glUniform4f(glGetUniformLocation(hudBoxShader, "uColor"), 0.15f, 0.95f, 0.45f, 0.22f);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(boxVerts), boxVerts);
                        glDrawArrays(GL_TRIANGLES, 0, 6);
                    }
                }
            }

            // Gameplay HUD: classic resource strip, tactical minimap,
            // selection dossier and clickable command grid.
            if(app.editor.panel==EditorSession::Panel::Play &&
               app.windowWidth>0 && app.windowHeight>0) {
                RTSHUDLayout layout(app.windowWidth,app.windowHeight);
                RTSHUD hud(hudBoxShader,hudVAO,hudVBO,layout);
                const int commandMode=app.mode==AppState::Mode::Move?1:
                    app.mode==AppState::Mode::AttackMove?2:
                    app.mode==AppState::Mode::BuildBarracks?3:
                    app.mode==AppState::Mode::Patrol?4:0;
                hud.draw(app.game,app.project,app.camera,commandMode,app.notice);
            }
            // Editor UI is intentionally drawn in the same OpenGL window:
            // project assets and game logic both use this exact document.
            if(app.editor.panel!=EditorSession::Panel::Play &&
               app.windowWidth>0 && app.windowHeight>0) {
                const bool dataPanel=app.editor.panel==EditorSession::Panel::Data ||
                                     app.editor.panel==EditorSession::Panel::Triggers ||
                                      app.editor.panel==EditorSession::Panel::Scene;
                const float bottom=dataPanel?-1.0f:
                    1.0f-2.0f*220.0f/float(app.windowHeight);
                float panelQuad[]={
                    -1,1, 1,1, 1,bottom,
                    -1,1, 1,bottom, -1,bottom
                };
                glUseProgram(hudBoxShader);
                glUniform4f(glGetUniformLocation(hudBoxShader,"uColor"),
                            .035f,.061f,.095f,.96f);
                glBindVertexArray(hudVAO);
                glBindBuffer(GL_ARRAY_BUFFER,hudVBO);
                glBufferData(GL_ARRAY_BUFFER,sizeof(panelQuad),panelQuad,GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES,0,6);
                const auto line=[&](float y,float size,const std::string& msg,
                                   float r=.76f,float g=.87f,float b=.95f){
                    std::string display=msg;
                    if(display.size()>115)display=display.substr(0,112)+"...";
                    drawHUDText(hudBoxShader,hudVAO,hudVBO,
                                app.windowWidth,app.windowHeight,
                                15.f,y,size,display,r,g,b);
                };
                line(12,2.8f,app.editor.panelName()+
                     (app.editor.modified?"  * UNSAVED":"  SAVED"),.31f,.94f,.67f);
                if(app.editor.panel==EditorSession::Panel::Terrain) {
                    line(38,2.0f,"F5 PLAY  F7 DATA  F9 TEST MAP  CTRL+S SAVE  CTRL+Z/Y UNDO/REDO");
                    line(60,2.0f,"1 RAISE  2 LOWER  3 SMOOTH  4 FLATTEN  5 PAINT  6 BLOCK  7 CLEAR  8 MINERAL  9 UNIT  0 DOODAD");
                    line(82,2.0f,"LMB BRUSH/PLACE  RMB DELETE NEARBY  [/] RADIUS  -/= STRENGTH  M MATERIAL  P UNIT  SHIFT+P ACTOR");
                    line(104,2.0f,"TOOL "+app.editor.toolName()+
                         "  RADIUS "+std::to_string(int(app.editor.brushRadius*10)/10.0f)+
                         "  STRENGTH "+std::to_string(app.editor.brushStrength).substr(0,4)+
                         "  MATERIAL "+std::to_string(app.editor.materialId),1.f,.80f,.37f);
                    line(126,2.0f,"PLACEMENT "+app.editor.placementArchetype+
                         "  ACTOR "+app.editor.placementDoodad+
                         "  OWNER "+std::to_string(app.editor.placementOwner),.71f,.84f,1.f);
                    line(148,2.0f,"MAP "+app.project.name+
                         "  SIZE "+std::to_string(app.project.terrain.width())+" X "+
                         std::to_string(app.project.terrain.depth())+
                         "  OBJECTS "+std::to_string(app.project.placements.size()));
                    if(app.hoveredTerrain)
                        line(171,1.8f,"CURSOR X "+
                             std::to_string(app.hoveredWorld.x).substr(0,6)+
                             " Y "+std::to_string(app.hoveredWorld.y).substr(0,6)+
                             " Z "+std::to_string(app.hoveredWorld.z).substr(0,6));
                } else if(app.editor.panel==EditorSession::Panel::Data) {
                    const CatalogEntry* entry=app.editor.selected(app.project);
                    line(40,2.0f,"F7 PLAY  F5 TERRAIN  F9 TEST MAP  CTRL+S SAVE  TAB NEXT / SHIFT+TAB PREV");
                    line(62,2.0f,"CTRL+F SEARCH  CTRL+SHIFT+F CLEAR  UP/DOWN PROPERTY  ENTER EDIT  CTRL+N ADD FIELD  CTRL+D DERIVE");
                    const std::size_t matches=app.editor.matchingCatalogCount(app.project);
                    line(87,2.0f,"OBJECTS "+std::to_string(matches)+" / "+
                         std::to_string(app.project.catalog.entries().size())+
                         "  FILTER: "+(app.editor.catalogFilter.empty()?"ALL":app.editor.catalogFilter),
                         .61f,.91f,.76f);
                    if(entry) {
                        line(118,2.7f,entry->type+" / "+entry->id+
                             (entry->parent.empty()?"":"  PARENT "+entry->parent),
                             1.f,.80f,.36f);
                        auto keys=app.editor.fields(app.project);
                        auto values=app.project.catalog.resolvedFields(entry->id);
                        int selectedIndex=keys.empty()?0:int(app.editor.selectedField%keys.size());
                        int first=std::max(0,selectedIndex-10);
                        if(int(keys.size())>18)first=std::min(first,int(keys.size())-18);
                        for(int idx=first;idx<int(keys.size()) && idx<first+18;++idx) {
                            const auto& key=keys[static_cast<std::size_t>(idx)];
                            std::string value=values.at(key).serialize();
                            if(value.size()>65)value=value.substr(0,62)+"...";
                            bool highlight=idx==selectedIndex;
                            line(153+float(idx-first)*25,2.1f,
                                 std::string(highlight?"> ":"  ")+key+" = "+value,
                                 highlight?1.0f:.68f,highlight?.87f:.81f,
                                 highlight?.41f:.92f);
                        }
                        if(app.editor.textEditing)
                            line(float(app.windowHeight)-58,2.25f,
                                 std::string(app.editor.filterEditing?"SEARCH: ":"EDIT: ")+
                                     app.editor.editText,1.f,.81f,.40f);
                    }
                } else if(app.editor.panel==EditorSession::Panel::Scene) {
                    line(40,2.0f,"F8 PLAY  F5 TERRAIN  F6 TRIGGERS  F7 DATA  CTRL+S SAVE  CTRL+Z/Y UNDO");
                    line(64,1.9f,"LMB SELECT  SHIFT+LMB MOVE  G MOVE TO CURSOR  TAB NEXT  SHIFT+TAB PREV");
                    line(87,1.9f,"P PLACE POINT  R PLACE REGION  J/K ROTATE  ,/. SCALE  O OWNER");
                    line(110,1.9f,"[/] REGION RADIUS  CTRL+D DUPLICATE  DEL DELETE  ENTER OVERRIDE JSON");
                    line(146,2.5f,"SCENE PLACEMENTS: "+std::to_string(app.project.placements.size()),
                         1.f,.79f,.38f);
                    const auto* selected=app.editor.selectedPlacement(app.project);
                    if(selected) {
                        line(185,2.5f,"SELECTED "+selected->id+" / "+selected->kind,
                             1.f,.82f,.41f);
                        line(220,2.2f,"CATALOG "+selected->catalogId+
                             "  OWNER "+std::to_string(selected->owner));
                        line(253,2.0f,"POSITION "+std::to_string(selected->position.x).substr(0,7)+
                             ", "+std::to_string(selected->position.y).substr(0,7)+
                             ", "+std::to_string(selected->position.z).substr(0,7));
                        line(286,2.0f,"ROTATION Y "+std::to_string(selected->rotation.y).substr(0,7)+
                             "  SCALE "+std::to_string(selected->scale.x).substr(0,6));
                        if(selected->kind=="Region") {
                            const auto rad=selected->overrides.find("radius");
                            const double radius=rad!=selected->overrides.end() &&
                                                rad->second.is<double>()
                                                ? rad->second.get<double>():5.0;
                            line(318,2.0f,"REGION RADIUS "+std::to_string(radius).substr(0,7),
                                 .55f,.9f,.98f);
                        }
                        std::string overrides=picojson::value(selected->overrides).serialize();
                        if(overrides.size()>155)overrides=overrides.substr(0,152)+"...";
                        line(360,1.8f,"OVERRIDES "+overrides,.76f,.9f,.66f);
                    } else line(190,2.0f,"CLICK A PLACED OBJECT OR PRESS TAB TO CHOOSE ONE.");
                    if(app.editor.textEditing) {
                        line(600,2.1f,"EDIT INSTANCE OVERRIDES JSON; CTRL+V PASTE",
                             1.f,.75f,.32f);
                        std::string snippet=app.editor.editText;
                        if(snippet.size()>160)snippet=snippet.substr(0,157)+"...";
                        line(635,1.7f,snippet,1.f,.88f,.65f);
                    }
                } else if(app.editor.panel==EditorSession::Panel::Triggers) {
                    line(40,2.0f,"F6 PLAY  F5 TERRAIN  F7 DATA  F9 TEST MAP  CTRL+S SAVE");
                    line(64,2.0f,"UP/DOWN TRIGGERS  ENTER EDIT JSON  CTRL+V PASTE  CTRL+C COPY  CTRL+N NEW");
                    line(88,2.0f,"CTRL+D DUPLICATE  DELETE REMOVE  ENTER APPLY EDIT  ESC CANCEL");
                    const std::size_t count=app.editor.triggerCount(app.project);
                    line(120,2.6f,"MAP LOGIC: "+std::to_string(count)+" EVENT-CONDITION-ACTION RULES",
                         1.f,.80f,.35f);
                    if(count==0)line(160,2.0f,"NO TRIGGERS. CTRL+N TO ADD A MAP START EVENT.");
                    const std::size_t current=count?app.editor.selectedTrigger%count:0;
                    const std::size_t first=count && current>10?current-10:0;
                    for(std::size_t idx=first;idx<count && idx<first+14;++idx) {
                        const bool active=idx==current;
                        line(158+float(idx-first)*28,2.0f,
                             std::string(active?"> ":"  ")+
                             app.editor.triggerLabel(app.project,idx),
                             active?1.f:.72f,active?.84f:.83f,active?.40f:.95f);
                    }
                    if(count>0) {
                        std::string json=app.editor.triggerJSON(app.project,current);
                        if(json.size()>150)json=json.substr(0,147)+"...";
                        line(565,1.8f,"SELECTED: "+json,.61f,.88f,.72f);
                    }
                    if(app.editor.textEditing) {
                        line(614,2.1f,"RAW JSON EDIT (CTRL+V TO PASTE):",1.f,.77f,.38f);
                        std::string src=app.editor.editText;
                        if(src.size()>190)src=src.substr(0,187)+"...";
                        line(643,1.8f,src,1.f,.89f,.63f);
                    }
                }
                if(!app.editor.message.empty())
                    line(float(app.windowHeight)-(dataPanel?24:float(app.windowHeight)-199),
                         1.8f,app.editor.message,1.f,.63f,.33f);
            }
            glEnable(GL_DEPTH_TEST);

            if (smokeFrames > 0 && ++renderedFrames >= smokeFrames) {
                if (!screenshotPath.empty() && app.framebufferWidth > 0 && app.framebufferHeight > 0) {
                    const int width = app.framebufferWidth, height = app.framebufferHeight;
                    std::vector<unsigned char> pixels(static_cast<size_t>(width)*height*3);
                    glPixelStorei(GL_PACK_ALIGNMENT,1);
                    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
                    std::ofstream out(screenshotPath,std::ios::binary);
                    out<<"P6\n"<<width<<" "<<height<<"\n255\n";
                    for(int y=height-1;y>=0;--y)
                        out.write(reinterpret_cast<const char*>(pixels.data()+static_cast<size_t>(y)*width*3),
                                  static_cast<std::streamsize>(width*3));
                    if(!out)std::cerr<<"Failed to save screenshot: "<<screenshotPath<<"\n";
                }
                glfwSetWindowShouldClose(window,true);
            }
            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glDeleteTextures(1,&fogTexture);
        glfwDestroyWindow(window);
        glfwTerminate();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}