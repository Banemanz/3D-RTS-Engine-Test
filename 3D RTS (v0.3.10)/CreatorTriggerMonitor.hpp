#pragma once
#include "TriggerEngine.hpp"
#include "imgui.h"
#include <string>

// Editor presentation only. No game-state mutations, no simulation callbacks:
// the trace is generated headlessly in the fixed-step TriggerEngine.
namespace CreatorTriggerMonitor {
template<class App>
void draw(App& app){
    if(!app.triggerTraceVisible)return;
    ImGui::SetNextWindowPos(ImVec2(348.f,65.f),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(490.f,500.f),ImGuiCond_FirstUseEver);
    bool opened=app.triggerTraceVisible;
    if(!ImGui::Begin("Creator | Trigger Monitor",&opened)){
        app.triggerTraceVisible=opened;ImGui::End();return;
    }
    app.triggerTraceVisible=opened;
    auto& engine=app.game.triggerDiagnostics();
    bool capture=app.triggerTracingEnabled;
    if(ImGui::Checkbox("Capture trigger events",&capture)){
        app.triggerTracingEnabled=capture;
        app.game.setTriggerTracing(capture);
    }
    ImGui::SameLine();
    if(ImGui::Button("Clear event log"))engine.clearTrace();
    ImGui::TextDisabled("Playtest %.2fs | %zu / %zu recent events",
                         engine.lastSimulationSeconds(),engine.trace().size(),
                         TriggerEngine::TraceCapacity);
    ImGui::TextWrapped("Run F9 to play, then F6 to inspect. Editing map data restarts the preview.");
    static ImGuiTextFilter search;
    search.Draw("Search rules / event details");
    ImGui::SeparatorText("Rule execution counters");
    const auto& rules=engine.rules();
    if(ImGui::BeginTable("##triggerStats",4,
        ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_SizingStretchProp,
        ImVec2(0.f,145.f))){
        ImGui::TableSetupColumn("Rule",ImGuiTableColumnFlags_WidthStretch,2.f);
        ImGui::TableSetupColumn("Tried");
        ImGui::TableSetupColumn("Fired");
        ImGui::TableSetupColumn("Blocked");
        ImGui::TableHeadersRow();
        for(std::size_t i=0;i<rules.size();++i){
            const auto& rule=rules[i];
            if(!search.PassFilter(rule.id.c_str()))continue;
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if(ImGui::Selectable(rule.id.c_str(),false,
                                 ImGuiSelectableFlags_SpanAllColumns)){
                app.editor.finishStroke(app.project);
                if(app.editor.textEditing)app.editor.cancelEdit();
                app.editor.selectedTrigger=i;
                app.editor.panel=EditorSession::Panel::Triggers;
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%llu",static_cast<unsigned long long>(rule.attempts));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%llu",static_cast<unsigned long long>(rule.executions));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%llu",static_cast<unsigned long long>(rule.blocked));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Recent execution and condition failures");
    if(ImGui::BeginChild("##triggerHistory",ImVec2(0.f,0.f),true)){
        const auto& events=engine.trace();
        if(events.empty())ImGui::TextDisabled("No captured events. Play a map with authored triggers.");
        for(auto it=events.rbegin();it!=events.rend();++it){
            const auto& entry=*it;
            const std::string searchable=entry.ruleId+" "+entry.event+" "+entry.detail;
            if(!search.PassFilter(searchable.c_str()))continue;
            ImGui::PushID(static_cast<int>(entry.sequence));
            const char* state=entry.status==TriggerEngine::TraceStatus::Executed?"FIRED":
                entry.status==TriggerEngine::TraceStatus::ConditionBlocked?"BLOCKED":"ERROR";
            const ImVec4 color=entry.status==TriggerEngine::TraceStatus::Executed
                ?ImVec4(.40f,.83f,.62f,1.f)
                :entry.status==TriggerEngine::TraceStatus::ConditionBlocked
                    ?ImVec4(.96f,.74f,.36f,1.f):ImVec4(.94f,.39f,.38f,1.f);
            ImGui::TextColored(color,"[%s]",state);
            ImGui::SameLine();
            ImGui::Text("%.2fs | %s",entry.simulationSeconds,entry.ruleId.c_str());
            if(entry.subjectId>=0)
                ImGui::TextDisabled("Subject unit #%d",entry.subjectId);
            ImGui::TextWrapped("%s (%s)",entry.detail.c_str(),entry.event.c_str());
            if(ImGui::SmallButton("Open rule")){
                for(std::size_t i=0;i<rules.size();++i)
                    if(rules[i].id==entry.ruleId){
                        app.editor.finishStroke(app.project);
                        if(app.editor.textEditing)app.editor.cancelEdit();
                        app.editor.selectedTrigger=i;
                        app.editor.panel=EditorSession::Panel::Triggers;
                        break;
                    }
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}
} // namespace CreatorTriggerMonitor
