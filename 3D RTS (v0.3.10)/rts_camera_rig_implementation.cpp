#include "RTSCamera.hpp"
#include <algorithm>
#include <cmath>

void RTSCamera::onMouseScroll(double yOffset) {
    targetZoomDistance -= static_cast<float>(yOffset) * zoomStep;
    targetZoomDistance = std::clamp(targetZoomDistance, minZoomDistance, maxZoomDistance);
}

void RTSCamera::update(float deltaTime, GLFWwindow* window, int windowWidth, int windowHeight) {
    double mouseX = 0.0, mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    Vec3 panInput(0.0f, 0.0f, 0.0f);
    bool cursorInside = (mouseX >= 0.0 && mouseX <= windowWidth &&
                         mouseY >= 0.0 && mouseY <= windowHeight);

    if (cursorInside) {
        if (mouseX <= edgeMarginPixels) panInput.x -= 1.0f;
        else if (mouseX >= windowWidth - edgeMarginPixels) panInput.x += 1.0f;

        if (mouseY <= edgeMarginPixels) panInput.z -= 1.0f;
        else if (mouseY >= windowHeight - edgeMarginPixels) panInput.z += 1.0f;
    }

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        panInput.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        panInput.x += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        panInput.z -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        panInput.z += 1.0f;

    if (panInput.length() > 0.001f) {
        panInput = panInput.normalized();
        float yawRad = toRadians(yawDegrees);
        Vec3 forward(-std::sin(yawRad), 0.0f, -std::cos(yawRad));
        Vec3 right(std::cos(yawRad), 0.0f, -std::sin(yawRad));

        float heightScale = lerp(0.65f, 1.5f, getZoomAlpha());
        Vec3 worldMove = (right * panInput.x + forward * (-panInput.z)) * (panSpeed * heightScale * deltaTime);

        targetFocalPoint = targetFocalPoint + worldMove;
        targetFocalPoint.x = std::clamp(targetFocalPoint.x, -mapBoundary, mapBoundary);
        targetFocalPoint.z = std::clamp(targetFocalPoint.z, -mapBoundary, mapBoundary);
    }

    float t = 1.0f - std::exp(-smoothingFactor * deltaTime);
    currentZoomDistance = lerp(currentZoomDistance, targetZoomDistance, t);
    focalPoint.x = lerp(focalPoint.x, targetFocalPoint.x, t);
    focalPoint.z = lerp(focalPoint.z, targetFocalPoint.z, t);
}

float RTSCamera::getZoomAlpha() const {
    return (currentZoomDistance - minZoomDistance) / (maxZoomDistance - minZoomDistance);
}

float RTSCamera::getCurrentPitchDegrees() const {
    float curvedAlpha = std::pow(getZoomAlpha(), 0.75f);
    return lerp(minPitchDegrees, maxPitchDegrees, curvedAlpha);
}

Vec3 RTSCamera::getEyePosition() const {
    float pitchRad = toRadians(getCurrentPitchDegrees());
    float yawRad   = toRadians(yawDegrees);
    float heightOffset     = currentZoomDistance * std::sin(pitchRad);
    float horizontalOffset = currentZoomDistance * std::cos(pitchRad);

    return focalPoint + Vec3(
        horizontalOffset * std::sin(yawRad),
        heightOffset,
        horizontalOffset * std::cos(yawRad)
    );
}

Mat4 RTSCamera::getViewMatrix() const {
    return Mat4::lookAt(getEyePosition(), focalPoint, Vec3(0.0f, 1.0f, 0.0f));
}

Mat4 RTSCamera::getProjectionMatrix(float aspectRatio) const {
    return Mat4::perspective(toRadians(fovDegrees), aspectRatio, nearPlane, farPlane);
}

bool RTSCamera::screenPointToRay(double mouseX, double mouseY, int screenWidth, int screenHeight, Ray& outRay) const {
    if (screenWidth <= 0 || screenHeight <= 0) return false;

    float ndcX = (2.0f * static_cast<float>(mouseX)) / static_cast<float>(screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * static_cast<float>(mouseY)) / static_cast<float>(screenHeight);

    float aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
    Mat4 invVP = (getProjectionMatrix(aspect) * getViewMatrix()).inverse();

    Vec4 nearWorld4 = invVP * Vec4(ndcX, ndcY, -1.0f, 1.0f);
    Vec4 farWorld4  = invVP * Vec4(ndcX, ndcY,  1.0f, 1.0f);

    if (std::abs(nearWorld4.w) < 1e-6f || std::abs(farWorld4.w) < 1e-6f) return false;

    outRay.origin = Vec3(nearWorld4.x / nearWorld4.w, nearWorld4.y / nearWorld4.w, nearWorld4.z / nearWorld4.w);
    Vec3 rayEnd(farWorld4.x / farWorld4.w, farWorld4.y / farWorld4.w, farWorld4.z / farWorld4.w);
    outRay.direction = (rayEnd - outRay.origin).normalized();
    return true;
}

bool RTSCamera::screenToGroundRaycast(double mouseX, double mouseY, int screenWidth, int screenHeight, Vec3& outHitPoint) const {
    Ray ray;
    if (!screenPointToRay(mouseX, mouseY, screenWidth, screenHeight, ray)) return false;
    if (terrain)return terrain->raycast(ray,outHitPoint);
    if (std::abs(ray.direction.y) < 1e-6f) return false;

    float t = -ray.origin.y / ray.direction.y;
    if (t < 0.0f) return false;

    outHitPoint = ray.origin + ray.direction * t;
    outHitPoint.y = 0.0f;
    return true;
}

bool RTSCamera::worldToScreen(const Vec3& worldPos, int screenWidth, int screenHeight, Vec2& outScreenPos) const {
    if (screenWidth <= 0 || screenHeight <= 0) return false;

    float aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
    Mat4 vp = getProjectionMatrix(aspect) * getViewMatrix();
    Vec4 clip = vp * Vec4(worldPos.x, worldPos.y, worldPos.z, 1.0f);

    if (clip.w <= 0.0001f) return false;

    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;

    outScreenPos.x = (ndcX + 1.0f) * 0.5f * static_cast<float>(screenWidth);
    outScreenPos.y = (1.0f - ndcY) * 0.5f * static_cast<float>(screenHeight);
    return true;
}