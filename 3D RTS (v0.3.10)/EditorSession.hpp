#pragma once
#include "RTSProject.hpp"
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

// Pure data/editing state: renderer and GLFW only translate input into these
// commands. A whole project can be edited and saved without any GPU.
class EditorSession {
public:
    enum class Panel { Play, Terrain, Data, Triggers, Scene };
    enum class Tool { Raise, Lower, Smooth, Flatten, Paint, Block, Unblock,
                      PlaceMineral, PlaceUnit, PlaceDoodad };
    Panel panel=Panel::Play;
    Tool tool=Tool::Raise;
    float brushRadius=4.0f;
    float brushStrength=0.8f;
    int materialId=1;
    int placementOwner=0;
    std::string placementArchetype="marine";
    std::string placementDoodad="marine_actor";
    std::string message;

    std::size_t selectedObject=0;
    std::size_t selectedField=0;
    std::size_t selectedTrigger=0;
    std::string selectedPlacementId;
    bool textEditing=false;
    bool newFieldEditing=false;
    std::string editText;
    bool modified=false;

    void startStroke(RTSProject& project);
    bool paint(RTSProject& project,const Vec3& worldPos);
    void finishStroke(RTSProject& project);
    bool undo(RTSProject& project);
    bool redo(RTSProject& project);
    // Used only to discard a tentative edit rejected by the runtime validator.
    void discardRejectedChange();

    bool place(RTSProject& project,const Vec3& worldPos);
    bool removeNearest(RTSProject& project,const Vec3& worldPos,float radius=3.0f);
    bool cyclePlacementCatalog(const RTSProject& project,int direction,bool doodad=false);

    // Scene editor uses stable IDs rather than indices that move when deleting
    // and inserting scene entities.
    const ProjectPlacement* selectedPlacement(const RTSProject& project) const;
    bool selectNearestPlacement(const RTSProject& project,const Vec3& world,float radius=3.5f);
    bool cycleSceneSelection(const RTSProject& project,int direction);
    bool moveSelectedPlacement(RTSProject& project,const Vec3& world);
    bool rotateSelectedPlacement(RTSProject& project,float degrees);
    bool scaleSelectedPlacement(RTSProject& project,float factor);
    bool toggleSelectedOwner(RTSProject& project);
    bool adjustSelectedRegionRadius(RTSProject& project,float delta);
    bool duplicateSelectedPlacement(RTSProject& project);
    bool deleteSelectedPlacement(RTSProject& project);
    bool placeSceneMarker(RTSProject& project,const Vec3& world,bool region);
    bool beginSceneOverrideEdit(const RTSProject& project);
    bool finishSceneOverrideEdit(RTSProject& project);

    void cycleObject(const RTSProject& project,int delta);
    void cycleField(const RTSProject& project,int delta);
    std::vector<std::string> fields(const RTSProject& project) const;
    const CatalogEntry* selected(const RTSProject& project) const;
    std::string selectedFieldName(const RTSProject& project) const;
    bool beginValueEdit(const RTSProject& project);
    void beginNewField();
    void addChar(unsigned int codepoint);
    void backspace();
    void cancelEdit();
    bool finishEdit(RTSProject& project);
    bool removeLocalOverride(RTSProject& project);
    bool duplicateCatalogObject(RTSProject& project);

    // First trigger authoring panel: raw JSON rule inspector/editor. A future
    // node-graph can use the same serialized ECA object model.
    std::size_t triggerCount(const RTSProject& project) const;
    std::string triggerLabel(const RTSProject& project,std::size_t index) const;
    std::string triggerJSON(const RTSProject& project,std::size_t index) const;
    void cycleTrigger(const RTSProject& project,int delta);
    bool createTrigger(RTSProject& project);
    bool duplicateTrigger(RTSProject& project);
    bool deleteTrigger(RTSProject& project);
    bool beginTriggerEdit(const RTSProject& project);
    bool finishTriggerEdit(RTSProject& project);
    std::string panelName() const;
    std::string toolName() const;

private:
    bool painting_=false;
    bool strokeActionRecorded_=false;
    std::unique_ptr<RTSProject> strokeBefore_;
    // One chronological undo stack across terrain, instances, catalogs and triggers.
    // Snapshots never embed TerrainMap's own undo history.
    std::deque<RTSProject> undo_,redo_;
    std::string pendingNewField_;
    std::size_t maxHistory_=32;
    void remember(const RTSProject& project);
    void rememberPlacement(const RTSProject& project) { remember(project); }
};
