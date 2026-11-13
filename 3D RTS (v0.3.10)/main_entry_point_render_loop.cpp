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
#include <cstdlib>
#include <fstream>
#include <cmath>
#include <string>

#include <algorithm>
#include <iostream>
#include <vector>

struct AppState {
    RTSCamera camera;
    SelectionManager selectionManager;
    RTSSkirmish game;
    enum class Mode { Normal, AttackMove, BuildBarracks } mode = Mode::Normal;
    bool keyA = false, keyB = false, keyT = false, keyC = false;
    bool keyX = false, keyR = false;
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
        state->camera.onMouseScroll(yoffset);
    }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state) {
        state->selectionManager.onMouseMove(xpos, ypos);
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state) return;

    double mouseX = 0.0, mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

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
            state->game.orderMove(hit, true);
            state->mode = AppState::Mode::Normal;
            return;
        }
        Unit* enemy = state->selectionManager.raycastEnemyUnit(
            mouseX, mouseY, state->camera,
            state->windowWidth, state->windowHeight, state->game.units);
        if (enemy) {
            state->game.orderAttack(enemy->id);
        } else if (!state->game.orderGather(hit)) {
            state->game.orderMove(hit, false);
        }
    }
}

int main(int argc, char** argv) {
    int smokeFrames = -1;
    std::string screenshotPath;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--smoke-frames" && i + 1 < argc)
            smokeFrames = (std::max)(1, std::atoi(argv[++i]));
        else if (arg == "--screenshot" && i + 1 < argc)
            screenshotPath = argv[++i];
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

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        GLuint terrainShader = compileShaderProgram(TERRAIN_VERTEX_SHADER, TERRAIN_FRAGMENT_SHADER);
        GLuint unitShader    = compileShaderProgram(UNIT_VERTEX_SHADER, UNIT_FRAGMENT_SHADER);
        GLuint ringShader    = compileShaderProgram(RING_VERTEX_SHADER, RING_FRAGMENT_SHADER);
        GLuint hudBoxShader  = compileShaderProgram(HUD_BOX_VERTEX_SHADER, HUD_BOX_FRAGMENT_SHADER);
        GLuint lineShader    = compileShaderProgram(LINE_VERTEX_SHADER, HUD_BOX_FRAGMENT_SHADER);

        // The skirmish owns unit lifetimes and initial spawning; see RTSSkirmish.

        // 1. Terrain Geometry
        const float terrainSize = 120.0f;
        float terrainVertices[] = {
            -terrainSize, 0.0f, -terrainSize,  terrainSize, 0.0f, -terrainSize,  terrainSize, 0.0f,  terrainSize,
            -terrainSize, 0.0f, -terrainSize,  terrainSize, 0.0f,  terrainSize, -terrainSize, 0.0f,  terrainSize
        };
        GLuint terrainVAO, terrainVBO;
        glGenVertexArrays(1, &terrainVAO);
        glGenBuffers(1, &terrainVBO);
        glBindVertexArray(terrainVAO);
        glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(terrainVertices), terrainVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

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
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);
            if (justPressed(GLFW_KEY_A, app.keyA)) app.mode = AppState::Mode::AttackMove;
            if (justPressed(GLFW_KEY_B, app.keyB)) app.mode = AppState::Mode::BuildBarracks;
            if (justPressed(GLFW_KEY_T, app.keyT)) app.game.queueMarine();
            if (justPressed(GLFW_KEY_C, app.keyC)) app.game.queueWorker();
            if (justPressed(GLFW_KEY_X, app.keyX)) app.game.orderStop();
            if (justPressed(GLFW_KEY_R, app.keyR)) {
                app.game.reset();
                app.mode = AppState::Mode::Normal;
                app.hasMoveMarker = false;
            }
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) app.camera.yawDegrees -= 75.0f * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) app.camera.yawDegrees += 75.0f * deltaTime;

            app.camera.update(deltaTime, window, app.windowWidth, app.windowHeight);
            accumulator += deltaTime;
            int steps = 0;
            while (accumulator >= fixedStep && steps < 8) {
                app.game.update(fixedStep);
                accumulator -= fixedStep;
                ++steps;
            }
            if (steps == 8 && accumulator >= fixedStep)
                accumulator = std::fmod(accumulator, fixedStep); // discard severe stalls, not unbounded catch-up

            if (currentTime - lastTitleUpdate > 0.35) {
                const char* mode = app.mode == AppState::Mode::BuildBarracks ? "PLACE BARRACKS: RMB" :
                    app.mode == AppState::Mode::AttackMove ? "ATTACK MOVE: RMB" : "COMMAND";
                std::string title = std::string("3D RTS | ") + app.game.statusLine() + " | " + mode;
                glfwSetWindowTitle(window, title.c_str());
                lastTitleUpdate = currentTime;
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
            glUniform1f(glGetUniformLocation(terrainShader, "uHasMarker"), app.hasMoveMarker ? 1.0f : 0.0f);

            glBindVertexArray(terrainVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

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
                               * Mat4::scale(Vec3(s, s, s));
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uModel"), 1, GL_FALSE, unitModel.m);
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uView"), 1, GL_FALSE, view.m);
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uProjection"), 1, GL_FALSE, proj.m);
                glUniform3f(glGetUniformLocation(unitShader, "uColor"),
                            unit.data.faction_color.x, unit.data.faction_color.y, unit.data.faction_color.z);
                glUniform1f(glGetUniformLocation(unitShader, "uSelected"), unit.isSelected ? 1.0f : 0.0f);

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
            glBindVertexArray(unitVAO);
            for (const auto& node : app.game.minerals) {
                if (node.remaining <= 0) continue;
                Mat4 mesh = Mat4::translate(node.position)
                          * Mat4::scale(Vec3(1.35f, 1.1f, 1.35f));
                glUniformMatrix4fv(glGetUniformLocation(unitShader, "uModel"), 1, GL_FALSE, mesh.m);
                glDrawArrays(GL_TRIANGLES, 0, 33);
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
                            14,40,2.0f,"LMB SELECT  RMB COMMAND  A ATTACK MOVE  B BUILD  T MARINE  C WORKER",
                            .9f,.94f,.97f);
                drawHUDText(hudBoxShader,hudVAO,hudVBO,app.windowWidth,app.windowHeight,
                            14,60,2.0f,"Q/E ROTATE  X STOP  R RESTART  ARROWS/W/S/D PAN",
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
                if(app.game.result != MatchResult::Playing)
                    drawHUDText(hudBoxShader,hudVAO,hudVBO,app.windowWidth,app.windowHeight,
                                w*.32f,h*.46f,5.0f,
                                app.game.result==MatchResult::Victory ? "VICTORY - PRESS R" : "DEFEAT - PRESS R",
                                1.f,.82f,.32f);
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