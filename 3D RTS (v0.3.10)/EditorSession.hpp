#pragma once
#include "RTSProject.hpp"
#include "RTSRequirements.hpp"
#include "RTSUpgrades.hpp"
#include "TriggerTree.hpp"
#include <cstddef>
#include <deque>
#include <functional>
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
    std::string placementArchetype="ranger";
    std::string placementDoodad="ranger_actor";
    std::string message;

    std::size_t selectedObject=0;
    std::string catalogFilter;
    // Filter by catalog family (empty means all). Not tied to a particular UI.
    std::string catalogTypeFilter;
    bool filterEditing=false;
    bool renamingCatalog=false;
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
    bool setCatalogTypeFilter(const RTSProject& project,const std::string& type);
    // Stable-ID navigation survives catalog filters and derives; no GPU/UI dependency.
    bool selectCatalogId(const RTSProject& project,const std::string& id);
    // Validated, atomic, undoable typed field editing for property widgets.
    bool setCatalogField(RTSProject& project,const std::string& key,
                         const picojson::value& value);
    // Atomic, undoable project-side AI policy settings. Invalid edits are
    // rejected before the runtime/project is reloaded.
    bool setAIResearchSetting(RTSProject& project,const std::string& key,
                              const picojson::value& value);
    void beginCatalogSearch();
    void clearCatalogSearch(const RTSProject& project);
    bool finishCatalogSearch(const RTSProject& project);
    std::size_t matchingCatalogCount(const RTSProject& project) const;
    bool catalogMatches(const CatalogEntry& entry) const;
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
    // Create a fully valid original Unit/Actor/Model/Effect starter object.
    // All presets use the same transactional catalog/undo API as F7 editing.
    bool createCatalogObject(RTSProject& project,const std::string& type,
                             const std::string& preset);
    bool convertEffectKind(RTSProject& project,const std::string& kind);
    // Ground targeting and the grouped AoE inspector use full transactional
    // mutations, keeping the map valid through Self<->Ground conversion.
    bool setAbilityTargetPolicy(RTSProject& project,const std::string& target);
    bool setGroundAbilitySettings(RTSProject& project,float radius,
                                  const std::string& team,int delayTicks,
                                  const std::string& shape="Circle",
                                  float angle=90.f,float width=2.f);
    // Commit a grouped Behavior inspector as one validated undo record.
    bool setBehaviorDefinition(RTSProject& project,const BehaviorDefinition& values);
    bool setProjectileDefinition(RTSProject& project,const ProjectileDefinition& values);
    bool setWeaponDefinition(RTSProject& project,const WeaponDefinition& values);
    bool setUpgradeDefinition(RTSProject& project,
                              const RTSUpgradeDefinition& values);
    bool setValidatorDefinition(RTSProject& project,
                                const RTSValidatorDefinition& values);
    bool setRequirementDefinition(RTSProject& project,
                                 RTSRequirementKind kind,
                                 const std::string& validatorId,
                                 const std::vector<std::string>& children);
    // One undoable Actor event authoring transaction; strict typed schema
    // validation applies equally in the headless project compiler.
    bool setActorCues(RTSProject& project,
                      const std::vector<RTSActorCueDefinition>& values);
    bool duplicateCatalogObject(RTSProject& project);
    bool beginCatalogRename(const RTSProject& project);
    bool finishCatalogRename(RTSProject& project);
    bool deleteCatalogObject(RTSProject& project);

    // Named global integers shared by every trigger in one map. Changes are
    // validated, undoable transactions and refuse dangling rule references.
    bool addTriggerVariable(RTSProject& project,const std::string& name,int initial=0);
    bool setTriggerVariable(RTSProject& project,const std::string& name,int value);
    bool removeTriggerVariable(RTSProject& project,const std::string& name);
    bool renameTriggerVariable(RTSProject& project,const std::string& oldName,
                               const std::string& newName);
    // First trigger authoring panel: raw JSON rule inspector/editor. A future
    // node-graph can use the same serialized ECA object model.
    std::size_t triggerCount(const RTSProject& project) const;
    std::string triggerLabel(const RTSProject& project,std::size_t index) const;
    std::string triggerJSON(const RTSProject& project,std::size_t index) const;
    void cycleTrigger(const RTSProject& project,int delta);
    bool createTrigger(RTSProject& project);
    bool duplicateTrigger(RTSProject& project);
    bool deleteTrigger(RTSProject& project);
    // Same typed trigger commands serve desktop GUI and headless tools.
    picojson::object triggerObject(const RTSProject& project,std::size_t index) const;
    bool setTriggerEvent(RTSProject& project,const std::string& type);
    bool setTriggerOnce(RTSProject& project,bool once);
    bool setTriggerEventField(RTSProject& project,const std::string& key,
                              const picojson::value& value);
    bool addTriggerNode(RTSProject& project,bool condition,const std::string& type);
    bool removeTriggerNode(RTSProject& project,bool condition,std::size_t index);
    bool moveTriggerNode(RTSProject& project,bool condition,std::size_t index,int direction);
    bool setTriggerNodeField(RTSProject& project,bool condition,std::size_t index,
                             const std::string& key,const picojson::value& value);
    // Switch a numerical ECA operand between a literal and a globally
    // referenced expression. Uses the same transaction and validation gate.
    bool setTriggerNodeOperand(RTSProject& project,bool condition,
                               std::size_t index,bool useExpression,
                               const std::string& variableId="");
    // All paths are rooted at the selected trigger; a path step names a typed
    // ECA list and index, including recursive IfThenElse/Repeat children.
    bool addTriggerTreeNode(RTSProject& project,const TriggerTreePath& parent,
                            const std::string& list,const std::string& type);
    bool removeTriggerTreeNode(RTSProject& project,const TriggerTreePath& path);
    bool duplicateTriggerTreeNode(RTSProject& project,const TriggerTreePath& path);
    bool moveTriggerTreeNode(RTSProject& project,const TriggerTreePath& path,int direction);
    bool setTriggerTreeField(RTSProject& project,const TriggerTreePath& path,
                             const std::string& field,const picojson::value& value);
    bool setTriggerTreeOperand(RTSProject& project,const TriggerTreePath& path,
                               bool expression,const std::string& variableId="");
    bool setTriggerExpressionNode(RTSProject& project,const TriggerTreePath& path,
                                  const std::string& field,
                                  const std::vector<std::string>& expressionPath,
                                  const picojson::value& expression);
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
    bool mutateTriggerVariables(RTSProject& project,
        const std::function<void(picojson::object&)>& change,
        const std::string& status);
    bool mutateTrigger(RTSProject& project,
                       const std::function<void(picojson::object&)>& change,
                       const std::string& status);
    void remember(const RTSProject& project);
    void rememberPlacement(const RTSProject& project) { remember(project); }
};
