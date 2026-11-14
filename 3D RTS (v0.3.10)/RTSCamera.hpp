#pragma once

#include "Math3D.hpp"
#include "Ray.hpp"
#include "TerrainMap.hpp"
#include <GLFW/glfw3.h>

class RTSCamera {
public:
    const TerrainMap* terrain=nullptr; // borrowed from the currently loaded project
    Vec3 focalPoint{0.0f, 0.0f, 0.0f};
    Vec3 targetFocalPoint{0.0f, 0.0f, 0.0f};

    float currentZoomDistance = 42.0f;
    float targetZoomDistance  = 42.0f;
    float minZoomDistance     = 7.0f;
    float maxZoomDistance     = 55.0f;

    float minPitchDegrees = 28.0f;
    float maxPitchDegrees = 68.0f;
    float yawDegrees      = 0.0f;

    float panSpeed         = 32.0f;
    float edgeMarginPixels = 22.0f;
    float zoomStep         = 3.5f;
    float smoothingFactor  = 12.0f;
    float mapBoundary      = 48.0f;

    float fovDegrees = 45.0f;
    float nearPlane  = 0.5f;
    float farPlane   = 250.0f;

    void onMouseScroll(double yOffset);
    void update(float deltaTime, GLFWwindow* window, int windowWidth, int windowHeight);

    float getZoomAlpha() const;
    float getCurrentPitchDegrees() const;
    Vec3 getEyePosition() const;
    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float aspectRatio) const;

    bool screenPointToRay(double mouseX, double mouseY, int screenWidth, int screenHeight, Ray& outRay) const;
    bool screenToGroundRaycast(double mouseX, double mouseY, int screenWidth, int screenHeight, Vec3& outHitPoint) const;
    bool worldToScreen(const Vec3& worldPos, int screenWidth, int screenHeight, Vec2& outScreenPos) const;
};