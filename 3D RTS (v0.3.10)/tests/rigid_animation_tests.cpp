#include "RTSProject.hpp"
#include "EditorSession.hpp"
#include "RTSAssetAudit.hpp"
#include "RTSGltfImport.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
static bool near(float a,float b){return std::abs(a-b)<2e-4f;}
int main(int argc,char** argv){
    assert(argc==3);
    auto m=rtsCompileGltf(argv[1]);
    assert(m.nodes.size()==4);
    RTSRigidRig rig(m.nodes);assert(rig.valid);
    for(std::size_t j=0;j<m.nodes.size();++j){
        const auto id=rtsNodeMatrix(m.nodes[j].model)*rig.inverseBind[j];
        for(int k=0;k<16;++k)assert(near(id.m[k],k%5==0?1:0));
    }
    RTSRigidClip clip;clip.duration=4;
    RTSRigidTrack track;track.node="Cap support";
    track.keys={{0,{}, {},{1,1,1}}, {2,{1,0,0},{0,180,0},{1,1,1}},
                {4,{}, {0,360,0},{1,1,1}}};
    clip.tracks={track};
    assert(near(rtsClipTime(clip,5),1));
    assert(near(rtsClipTime(clip,-1),3));
    auto pose=rtsSampleRigidPose(m.nodes,rig,clip,1);
    assert(pose.valid && near(pose.model[2].m[12],.85f));
    assert(near(pose.model[3].m[12],.85f));
    auto point=pose.surfaceDelta[1]*Vec4(m.nodes[3].model[12],m.nodes[3].model[13],m.nodes[3].model[14],1);
    assert(near(point.x,pose.model[3].m[12]) && near(point.y,pose.model[3].m[13]));
    for(int k=0;k<16;++k)assert(pose.surfaceDelta[0].m[k]==(k%5==0?1:0));
    RTSModelAttachment a;a.socket="Amber cap";a.offset=Vec3(1,0,0);
    auto posed=rtsPosedAttachment(m.nodes,&pose,Mat4(),a);
    auto expected=pose.model[3]*Vec4(1,0,0,1);
    assert(posed && near(posed->m[12],expected.x)&&near(posed->m[14],expected.z));
    auto rest=rtsSampleRigidPose(m.nodes,rig,clip,4);
    for(int k=0;k<16;++k)assert(near(rest.surfaceDelta[1].m[k],k%5==0?1:0));
    clip.loop=false;assert(near(rtsClipTime(clip,7),4));
    auto bad=clip;bad.tracks[0].node="missing";
    assert(!rtsSampleRigidPose(m.nodes,rig,bad,1).valid);
    RTSProject project;project.loadFile(argv[2]);
    EditorSession editor;editor.panel=EditorSession::Panel::Data;
    assert(editor.selectCatalogId(project,"dual_beacon_actor"));
    assert(editor.setCatalogField(project,"rigid_clip",serializeRigidClip(clip)));
    const auto compiled=actorFromCatalog(project.catalog,"dual_beacon_actor").rigidClip;
    assert(compiled.tracks.size()==1 && compiled.tracks[0].keys.size()==3);
    assert(rtsAuditProjectAssets(project,argv[2]).valid());
    auto invalid=clip;invalid.tracks[0].keys[1].time=0;
    assert(!editor.setCatalogField(project,"rigid_clip",serializeRigidClip(invalid)));
    assert(editor.undo(project));assert(actorFromCatalog(project.catalog,"dual_beacon_actor").rigidClip.tracks.empty());
    assert(editor.redo(project));assert(editor.setCatalogField(project,"rigid_clip",serializeRigidClip(bad)));
    assert(!rtsAuditProjectAssets(project,argv[2]).valid());
    std::cout<<"Rigid poses, bind inverse, child propagation, socket synchronization, clip validation and undo passed\n";
}
