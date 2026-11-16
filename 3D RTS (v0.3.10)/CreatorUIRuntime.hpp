#pragma once
#include "CreatorUISettings.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

// Small, shared Windows/Linux UI compositor adapter. Font atlas and all
// padding are rebuilt from an ABSOLUTE base style, avoiding the familiar
// cumulative ScaleAllSizes() runaway when a slider or monitor DPI changes.
class CreatorUIRuntime {
public:
    void initialize(const CreatorUISettings& settings,GLFWwindow* gameWindow){
        ImGui::StyleColorsDark();
        ImGuiStyle& s=ImGui::GetStyle();
        auto& c=s.Colors;
        const auto rgb=[](int r,int g,int b,float a=1.f){
            return ImVec4(r/255.f,g/255.f,b/255.f,a);
        };
        c[ImGuiCol_WindowBg]=rgb(13,25,43,.98f);
        c[ImGuiCol_ChildBg]=rgb(15,33,55,.95f);
        c[ImGuiCol_PopupBg]=rgb(15,33,55,.99f);
        c[ImGuiCol_TitleBg]=rgb(19,41,67);
        c[ImGuiCol_TitleBgActive]=rgb(27,65,99);
        c[ImGuiCol_MenuBarBg]=rgb(19,42,69);
        c[ImGuiCol_FrameBg]=rgb(28,59,91);
        c[ImGuiCol_FrameBgHovered]=rgb(39,88,123);
        c[ImGuiCol_FrameBgActive]=rgb(51,114,151);
        c[ImGuiCol_Button]=rgb(28,67,102);
        c[ImGuiCol_ButtonHovered]=rgb(39,100,144);
        c[ImGuiCol_ButtonActive]=rgb(51,122,163);
        c[ImGuiCol_Header]=rgb(33,72,110);
        c[ImGuiCol_HeaderHovered]=rgb(49,102,146);
        c[ImGuiCol_HeaderActive]=rgb(60,125,166);
        c[ImGuiCol_CheckMark]=rgb(98,215,220);
        c[ImGuiCol_SliderGrab]=rgb(75,185,205);
        c[ImGuiCol_SliderGrabActive]=rgb(102,224,233);
        c[ImGuiCol_Text]=rgb(229,241,251);
        c[ImGuiCol_TextDisabled]=rgb(151,177,195);
        c[ImGuiCol_Border]=rgb(76,116,151,.75f);
        c[ImGuiCol_Separator]=rgb(57,103,138);
        c[ImGuiCol_ResizeGrip]=rgb(66,150,192,.6f);
        c[ImGuiCol_ResizeGripHovered]=rgb(104,211,225);
        c[ImGuiCol_TableHeaderBg]=rgb(27,58,88);
        c[ImGuiCol_TableBorderStrong]=rgb(72,123,165);
        c[ImGuiCol_NavHighlight]=rgb(112,210,226);
        s.WindowRounding=7.f;
        s.ChildRounding=6.f;
        s.FrameRounding=5.f;
        s.PopupRounding=5.f;
        s.WindowBorderSize=1.f;
        s.FrameBorderSize=1.f;
        s.WindowPadding=ImVec2(11.f,11.f);
        s.FramePadding=ImVec2(8.f,5.f);
        s.ItemSpacing=ImVec2(9.f,7.f);
        s.ScrollbarSize=16.f;
        s.GrabMinSize=14.f;
        base_=s;
        float dpi=windowDpi(gameWindow);
        apply(settings,dpi,true);
    }
    static float windowDpi(GLFWwindow* primary){
        float x=1.f,y=1.f;
        if(primary)glfwGetWindowContentScale(primary,&x,&y);
        // If a detached editor module has focus, honor THAT monitor, not
        // the primary game's monitor. Global ImGui font/style is per-context.
        if(ImGui::GetCurrentContext()){
            for(const auto* vp:ImGui::GetPlatformIO().Viewports){
                if(!vp->PlatformHandle)continue;
                auto* platform=static_cast<GLFWwindow*>(vp->PlatformHandle);
                if(glfwGetWindowAttrib(platform,GLFW_FOCUSED)==GLFW_TRUE){
                    glfwGetWindowContentScale(platform,&x,&y);
                    break;
                }
            }
        }
        if(!std::isfinite(x) || x<=0.f)x=1.f;
        if(!std::isfinite(y) || y<=0.f)y=1.f;
        return std::clamp(std::max(x,y),.75f,4.f);
    }
    bool apply(const CreatorUISettings& settings,float dpi,bool force=false){
        const float scale=static_cast<float>(settings.effective(dpi));
        if(!force && std::fabs(scale-appliedScale_)<.025f &&
           appliedContrast_==settings.enhancedContrast)return false;
        appliedScale_=scale;
        appliedContrast_=settings.enhancedContrast;
        ImGuiStyle style=base_;
        if(settings.enhancedContrast){
            style.Colors[ImGuiCol_WindowBg]=ImVec4(.025f,.065f,.13f,1.f);
            style.Colors[ImGuiCol_ChildBg]=ImVec4(.04f,.10f,.18f,1.f);
            style.Colors[ImGuiCol_FrameBg]=ImVec4(.10f,.23f,.37f,1.f);
            style.Colors[ImGuiCol_Text]=ImVec4(1.f,1.f,1.f,1.f);
            style.Colors[ImGuiCol_TextDisabled]=ImVec4(.78f,.87f,.95f,1.f);
            style.Colors[ImGuiCol_Border]=ImVec4(.52f,.72f,.88f,1.f);
            style.Colors[ImGuiCol_CheckMark]=ImVec4(.48f,1.f,1.f,1.f);
            style.FrameBorderSize=1.5f;
            style.WindowBorderSize=1.5f;
        }
        style.ScaleAllSizes(scale);
        // Multi-viewports demand opaque floating OS windows, otherwise the
        // desktop can bleed through onto small text.
        if(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable){
            style.WindowRounding=0.f;
            style.Colors[ImGuiCol_WindowBg].w=1.f;
        }
        ImGui::GetStyle()=style;
        auto& io=ImGui::GetIO();
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        io.FontDefault=nullptr;
        io.Fonts->Clear();
        const float fontPx=std::clamp(16.5f*scale,13.f,70.f);
        // Use OS fonts when available, without bundling proprietary or
        // system font files. ImGui's built-in face remains a safe fallback.
        const char* candidates[]={
#ifdef _WIN32
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
#else
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
#endif
        };
        for(const char* path:candidates){
            if(!std::filesystem::exists(path))continue;
            if(auto* font=io.Fonts->AddFontFromFileTTF(path,fontPx)){
                io.FontDefault=font;break;
            }
        }
        if(!io.FontDefault){
            ImFontConfig config;
            config.SizePixels=fontPx;
            io.FontDefault=io.Fonts->AddFontDefault(&config);
        }
        ImGui_ImplOpenGL3_CreateFontsTexture();
        return true;
    }
    float scale()const{return appliedScale_;}
private:
    ImGuiStyle base_{};
    float appliedScale_=0.f;
    bool appliedContrast_=false;
};
