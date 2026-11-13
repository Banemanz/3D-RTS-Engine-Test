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

struct AppState {
    RTSCamera camera;
    SelectionManager selectionManager;
    RTSSkirmish game;
    RTSProject project=RTSProject::defaults();
    EditorSession editor;
    std::string projectPath="projects/working.rtsproject.json";
    bool mouseStroke=false;
    bool hoveredTerrain=false;
    Vec3 hoveredWorld{};
    void reloadFromProject(){
        game.loadProject(project);
        camera.terrain=&project.terrain;
        hasMoveMarker=false;
    }
    void saveProject(){
        try{
            TriggerEngine::validate(project);
            project.saveFile(projectPath);
            editor.modified=false;editor.message="PROJECT SAVED";
        }
        catch(const std::exception& e){editor.message=std::string("SAVE ERROR: ")+e.what();}
    }
    enum class Mode { Normal, AttackMove, BuildBarracks } mode = Mode::Normal;
    bool keyA = false, keyB = false, keyT = false, keyC = false;
    bool keyX = false, keyR = false;
    std::array<std::vector<int>,10> controlGroups{};
    std::array<bool,10> digitWasDown{};
    int framebufferWidth = 1280, framebufferHeight = 720;

    Vec3 lastClickedWorldPos{0.0f, 0.0f, 0.0f};
    bool hasMoveMarker = false;
    int windowWidth = 1280;
    int windowHeight = 720;
};

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
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state) {
        if(state->editor.panel==EditorSession::Panel::Terrain &&
           glfwGetKey(window,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS) {
            state->editor.brushRadius=std::clamp(state->editor.brushRadius+
                float(yoffset),.5f,32.0f);
        }else state->camera.onMouseScroll(yoffset);
    }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
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
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state) return;

    double mouseX = 0.0, mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

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
                state->game.units
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
        if (state->mode == AppState::Mode::AttackMove) {
            state->game.orderMove(hit, true, (mods & GLFW_MOD_SHIFT) != 0);
            state->mode = AppState::Mode::Normal;
            return;
        }
        Unit* enemy = state->selectionManager.raycastEnemyUnit(
            mouseX, mouseY, state->camera,
            state->windowWidth, state->windowHeight, state->game.units);
        if (enemy) {
            state->game.orderAttack(enemy->id);
        } else if (!state->game.orderGather(hit)) {
            state->game.orderMove(hit, false, (mods & GLFW_MOD_SHIFT) != 0);
        }
    }
}


void characterCallback(GLFWwindow* window,unsigned int codepoint) {
    auto* s=static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if(s && (s->editor.panel==EditorSession::Panel::Data ||
             s->editor.panel==EditorSession::Panel::Triggers))
        s->editor.addChar(codepoint);
}
void editorKeyCallback(GLFWwindow* window,int key,int /*scancode*/,int action,int mods) {
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
                RTSProject backup=s->project;
                const bool wasModified=edit.modified;
                try {
                    const bool changed=edit.panel==EditorSession::Panel::Triggers
                        ? edit.finishTriggerEdit(s->project):edit.finishEdit(s->project);
                    if(changed && !edit.textEditing){
                        TriggerEngine::validate(s->project);
                        s->reloadFromProject();
                    }
                }catch(const std::exception& e){
                    s->project=std::move(backup);
                    edit.discardRejectedChange();
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
}

int main(int argc, char** argv) {
    int smokeFrames = -1;
    std::string screenshotPath;
    std::string inputProjectPath;
    bool startEditor=false;
    bool startTriggerEditor=false;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--smoke-frames" && i + 1 < argc)
            smokeFrames = (std::max)(1, std::atoi(argv[++i]));
        else if (arg == "--screenshot" && i + 1 < argc)
            screenshotPath = argv[++i];
        else if(arg == "--project" && i+1<argc)inputProjectPath=argv[++i];
        else if(arg == "--editor")startEditor=true;
        else if(arg == "--trigger-editor")startTriggerEditor=true;
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
        app.reloadFromProject();
        if(startEditor || startTriggerEditor) {
            app.editor.panel=startTriggerEditor ? EditorSession::Panel::Triggers :
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

        // The skirmish owns unit lifetimes and initial spawning; see RTSSkirmish.

        // Editable heightfield mesh: five floats per vertex, updated after brush strokes.
        std::vector<float> terrainVertices=app.project.terrain.buildMesh();
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

        while (!glfwWindowShouldClose(window)) {
            double currentTime = glfwGetTime();
            float deltaTime = std::clamp(static_cast<float>(currentTime - lastTime), 0.0f, 0.25f);
            lastTime = currentTime;

            auto justPressed = [&](int key, bool& previous) {
                bool down = glfwGetKey(window, key) == GLFW_PRESS;
                bool fired = down && !previous;
                previous = down;
                return fired;
            };
            if(app.editor.panel==EditorSession::Panel::Play) {
            if (justPressed(GLFW_KEY_A, app.keyA)) app.mode = AppState::Mode::AttackMove;
            if (justPressed(GLFW_KEY_B, app.keyB)) app.mode = AppState::Mode::BuildBarracks;
            if (justPressed(GLFW_KEY_T, app.keyT)) app.game.queueMarine();
            if (justPressed(GLFW_KEY_C, app.keyC)) app.game.queueWorker();
            if (justPressed(GLFW_KEY_X, app.keyX)) app.game.orderStop();
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
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) app.camera.yawDegrees -= 75.0f * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) app.camera.yawDegrees += 75.0f * deltaTime;

            if(app.editor.panel!=EditorSession::Panel::Data || !app.editor.textEditing)
                app.camera.update(deltaTime, window, app.windowWidth, app.windowHeight);
            if(app.editor.panel==EditorSession::Panel::Play) {
                accumulator += deltaTime;
                int steps = 0;
                while (accumulator >= fixedStep && steps < 8) {
                    app.game.update(fixedStep);
                    accumulator -= fixedStep;
                    ++steps;
                }
                if (steps == 8 && accumulator >= fixedStep)
                    accumulator = std::fmod(accumulator, fixedStep);
            }else accumulator=0.0f; // editor never advances gameplay

            if (currentTime - lastTitleUpdate > 0.35) {
                const char* mode = app.mode == AppState::Mode::BuildBarracks ? "PLACE BARRACKS: RMB" :
                    app.mode == AppState::Mode::AttackMove ? "ATTACK MOVE: RMB" : "COMMAND";
                std::string title = app.editor.panel==EditorSession::Panel::Play
                    ? std::string("3D RTS | ")+app.game.statusLine()+" | "+mode
                    : std::string("3D RTS AUTHORING | ")+app.project.name+
                      " | "+app.editor.panelName()+(app.editor.modified?" *":"");
                glfwSetWindowTitle(window, title.c_str());
                lastTitleUpdate = currentTime;
            }

            if(app.project.terrain.dirty()) {
                terrainVertices=app.project.terrain.buildMesh();
                terrainVertexCount=static_cast<GLsizei>(terrainVertices.size()/5);
                glBindVertexArray(terrainVAO);
                glBindBuffer(GL_ARRAY_BUFFER,terrainVBO);
                glBufferData(GL_ARRAY_BUFFER,terrainVertices.size()*sizeof(float),
                             terrainVertices.data(),GL_DYNAMIC_DRAW);
                app.project.terrain.clearDirty();
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
                if (node.remaining <= 0) continue;
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
                ActorVisual visual=resolvePlacedDoodad(app.project,placed);
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

            // Screen-space economy and command HUD.
            if (app.windowWidth > 0 && app.windowHeight > 0) {
                const float w = static_cast<float>(app.windowWidth);
                const float h = static_cast<float>(app.windowHeight);
                const float bg[] = {
                    -1.f, 1.f,  1.f, 1.f,  1.f, 1.f-240.f/h,
                    -1.f, 1.f,  1.f, 1.f-240.f/h, -1.f, 1.f-240.f/h
                };
                glUseProgram(hudBoxShader);
                glUniform4f(glGetUniformLocation(hudBoxShader, "uColor"),.035f,.065f,.10f,.75f);
                glBindVertexArray(hudVAO);
                glBindBuffer(GL_ARRAY_BUFFER,hudVBO);
                glBufferData(GL_ARRAY_BUFFER,sizeof(bg),bg,GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES,0,6);
                const std::string economy = "MINERALS " + std::to_string(app.game.playerMinerals)
                    + "  SUPPLY " + std::to_string(app.game.supplyUsed) + "/"
                    + std::to_string(app.game.supplyCap) + "  WORKERS "
                    + std::to_string(app.game.activeWorkerCount())
                    + "  BARRACKS " + std::to_string(app.game.completedBarracksCount());
                drawHUDText(hudBoxShader,hudVAO,hudVBO,app.windowWidth,app.windowHeight,
                            14,12,2.6f,economy,.45f,.95f,.85f);
                drawHUDText(hudBoxShader,hudVAO,hudVBO,app.windowWidth,app.windowHeight,
                            14,40,2.0f,"LMB SELECT  RMB COMMAND  A ATTACK MOVE  B BUILD  T MARINE  C WORKER  SHIFT+RMB QUEUE",
                            .9f,.94f,.97f);
                drawHUDText(hudBoxShader,hudVAO,hudVBO,app.windowWidth,app.windowHeight,
                            14,60,2.0f,"Q/E ROTATE  X STOP  R RESTART  CTRL+1-9 SAVE  1-9 RECALL",
                            .7f,.8f,.9f);
                std::string selectionLabel;
                int selectedCount=0;
                for(const auto& u:app.game.units)if(u.isSelected && u.isAlive() && u.ownerId==0) {
                    ++selectedCount;
                    if(selectedCount==1)
                        selectionLabel = u.instanceName + " HP " + std::to_string(int(u.currentHp))
                            + "/" + std::to_string(int(u.data.max_hp));
                }
                if(selectedCount>1)selectionLabel=std::to_string(selectedCount)+" UNITS SELECTED";
                if(!selectionLabel.empty())
                    drawHUDText(hudBoxShader,hudVAO,hudVBO,app.windowWidth,app.windowHeight,
                                14,84,2.2f,selectionLabel,1.0f,.92f,.53f);
                if(app.mode==AppState::Mode::BuildBarracks)
                    drawHUDText(hudBoxShader,hudVAO,hudVBO,app.windowWidth,app.windowHeight,
                                14,108,2.4f,"PLACE BARRACKS WITH RIGHT CLICK  COST 150",1.f,.8f,.35f);
                else if(app.mode==AppState::Mode::AttackMove)
                    drawHUDText(hudBoxShader,hudVAO,hudVBO,app.windowWidth,app.windowHeight,
                                14,108,2.4f,"RIGHT CLICK DESTINATION TO ATTACK MOVE",1.f,.8f,.35f);
                if(!app.game.triggerMessage.empty() &&
                   app.editor.panel==EditorSession::Panel::Play)
                    drawHUDText(hudBoxShader,hudVAO,hudVBO,
                                app.windowWidth,app.windowHeight,
                                14,139,2.0f,app.game.triggerMessage,
                                1.f,.76f,.32f);
                if(app.game.result != MatchResult::Playing)
                    drawHUDText(hudBoxShader,hudVAO,hudVBO,app.windowWidth,app.windowHeight,
                                w*.32f,h*.46f,5.0f,
                                app.game.result==MatchResult::Victory ? "VICTORY - PRESS R" : "DEFEAT - PRESS R",
                                1.f,.82f,.32f);
            }
            // Editor UI is intentionally drawn in the same OpenGL window:
            // project assets and game logic both use this exact document.
            if(app.editor.panel!=EditorSession::Panel::Play &&
               app.windowWidth>0 && app.windowHeight>0) {
                const bool dataPanel=app.editor.panel==EditorSession::Panel::Data ||
                                     app.editor.panel==EditorSession::Panel::Triggers;
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
                    line(40,2.0f,"F7 PLAY  F5 TERRAIN  F9 TEST MAP  CTRL+S SAVE  TAB NEXT OBJECT  SHIFT+TAB PREVIOUS");
                    line(62,2.0f,"UP/DOWN PROPERTY  ENTER EDIT JSON  CTRL+N ADD FIELD  CTRL+D DERIVE OBJECT  CTRL+BACKSPACE REMOVE");
                    if(entry) {
                        line(92,2.7f,entry->type+" / "+entry->id+
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
                            line(124+float(idx-first)*25,2.1f,
                                 std::string(highlight?"> ":"  ")+key+" = "+value,
                                 highlight?1.0f:.68f,highlight?.87f:.81f,
                                 highlight?.41f:.92f);
                        }
                        if(app.editor.textEditing)
                            line(float(app.windowHeight)-58,2.25f,
                                 "EDIT: "+app.editor.editText,1.f,.81f,.40f);
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

        glfwDestroyWindow(window);
        glfwTerminate();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}