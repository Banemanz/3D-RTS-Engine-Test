#pragma once
#include "DataCatalog.hpp"
#include "EditorSession.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <algorithm>
#include <cmath>

namespace CreatorRigidClip {
struct Draft {
    std::string key,notice;
    RTSRigidClip clip;
    std::vector<RTSModelNode> nodes;
    std::size_t track=0;
};
inline bool editVec(const char* label,Vec3& value){
    float v[]={value.x,value.y,value.z};
    if(!ImGui::InputFloat3(label,v))return false;
    value=Vec3(v[0],v[1],v[2]);return true;
}
inline bool draw(EditorSession& ed,RTSProject& project,const std::string& projectPath,const std::string& actorId){
    static Draft draft;
    if(!ImGui::CollapsingHeader("Rigid part animation | keyframes"))return false;
    const auto reload=[&]{
        draft=Draft{};
        draft.key=projectPath+"|"+actorId;
        draft.clip=compileRigidClip(project.catalog,actorId);
        try{
            const auto visual=actorFromCatalog(project.catalog,actorId);
            draft.nodes=rtsReadStaticMesh(rtsResolveMeshReference(projectPath,visual.mesh)).nodes;
        }catch(const std::exception& ex){draft.notice=ex.what();}
    };
    if(draft.key!=projectPath+"|"+actorId)reload();
    auto& clip=draft.clip;
    ImGui::TextWrapped("Local offsets, XYZ Euler rotations and scale animate retained model parts. Descendants and attached models follow the pose. Pure presentation.");
    ImGui::InputFloat("Clip duration (seconds)",&clip.duration,.25f,1.f,"%.2f");
    ImGui::Checkbox("Loop clip",&clip.loop);
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / 16 tracks",clip.tracks.size());
    if(draft.nodes.empty())ImGui::TextWrapped("A compiled multi-node RTSMESH3 model is needed. Reload after import.");
    if(clip.tracks.size()<16 && ImGui::SmallButton("Add part track")){
        RTSRigidTrack t;
        for(const auto& n:draft.nodes)
            if(rtsFindUniqueModelNode(draft.nodes,n.name) &&
               std::none_of(clip.tracks.begin(),clip.tracks.end(),[&](const RTSRigidTrack& v){return v.node==n.name;})){
                t.node=n.name;break;
            }
        t.keys.emplace_back();
        clip.tracks.push_back(std::move(t));
        draft.track=clip.tracks.size()-1;
    }
    if(!clip.tracks.empty()){
        draft.track=std::min(draft.track,clip.tracks.size()-1);
        if(ImGui::BeginCombo("Track",clip.tracks[draft.track].node.empty()?"Select a node":clip.tracks[draft.track].node.c_str())){
            for(std::size_t i=0;i<clip.tracks.size();++i){
                ImGui::PushID(int(i));
                if(ImGui::Selectable(clip.tracks[i].node.empty()?"(unassigned)":clip.tracks[i].node.c_str(),draft.track==i))draft.track=i;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        auto& t=clip.tracks[draft.track];
        if(ImGui::BeginCombo("Target model node",t.node.empty()?"Select unique node":t.node.c_str())){
            for(const auto& n:draft.nodes)
                if(rtsFindUniqueModelNode(draft.nodes,n.name) &&
                   ImGui::Selectable(n.name.c_str(),t.node==n.name))t.node=n.name;
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("%zu / 64 keys | increasing seconds",t.keys.size());
        if(t.keys.size()<64 && ImGui::SmallButton("Add keyframe")){
            RTSRigidKey k;
            if(!t.keys.empty()){
                k=t.keys.back();
                k.time=std::min(clip.duration,k.time+.25f);
                if(k.time<=t.keys.back().time){
                    float best=0;std::size_t at=0;
                    for(std::size_t i=1;i<t.keys.size();++i)
                        if(t.keys[i].time-t.keys[i-1].time>best){best=t.keys[i].time-t.keys[i-1].time;at=i;}
                    if(at){
                        k=rtsSampleRigidTrack(t,(t.keys[at-1].time+t.keys[at].time)*.5f);
                        t.keys.insert(t.keys.begin()+static_cast<std::ptrdiff_t>(at),k);
                    }else if(k.time>0){
                        k.time=0;t.keys.insert(t.keys.begin(),k);
                    }
                }else t.keys.push_back(k);
            }else t.keys.push_back(k);
        }
        ImGui::SameLine();
        if(ImGui::SmallButton("Sort keys"))
            std::stable_sort(t.keys.begin(),t.keys.end(),[](const RTSRigidKey& a,const RTSRigidKey& b){return a.time<b.time;});
        ImGui::BeginChild("##rigidKeys",ImVec2(0,std::min(310.f,140.f+float(t.keys.size())*55.f)),true);
        for(std::size_t i=0;i<t.keys.size();++i){
            ImGui::PushID(int(i));
            auto& k=t.keys[i];
            ImGui::SeparatorText(("Key "+std::to_string(i+1)).c_str());
            ImGui::InputFloat("Time",&k.time,0,0,"%.3f");
            editVec("Local offset",k.offset);editVec("Rotation XYZ (deg)",k.rotation);editVec("Local scale",k.scale);
            if(ImGui::SmallButton("Remove key")){
                t.keys.erase(t.keys.begin()+static_cast<std::ptrdiff_t>(i));
                ImGui::PopID();break;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        if(ImGui::SmallButton("Remove track")){
            clip.tracks.erase(clip.tracks.begin()+static_cast<std::ptrdiff_t>(draft.track));draft.track=0;
        }
    }
    if(ImGui::Button("Apply rigid clip")){
        // File-aware node validation here; catalog validation remains
        // independent of asset files so headless map editing stays possible.
        bool nodesValid=true;
        for(const auto& t:clip.tracks)if(!rtsFindUniqueModelNode(draft.nodes,t.node)){
            nodesValid=false;draft.notice="Missing/ambiguous model node: "+t.node;break;
        }
        if(nodesValid && ed.setCatalogField(project,"rigid_clip",serializeRigidClip(clip))){
            draft.key.clear();return true;
        }
    }
    ImGui::SameLine();if(ImGui::SmallButton("Reload clip / nodes"))reload();
    if(!draft.notice.empty())ImGui::TextWrapped("%s",draft.notice.c_str());
    ImGui::TextWrapped("Apply, then open this asset's Model Viewer and select the Actor to preview the clip.");
    return false;
}
}
