#pragma once
#include "Math3D.hpp"
#include "RTSStaticMesh.hpp"

// Cosmetic child model; no collision, weapon, pathing or simulation effects.
struct RTSModelAttachment {
    std::string socket,modelId,mesh;
    Vec3 offset{0,0,0},scale{1,1,1},tint{1,1,1},modelScale{1,1,1};
    float yaw=0,opacity=1;
};
inline std::optional<Mat4> rtsAttachmentTransform(
    const std::vector<RTSModelNode>& nodes,const Mat4& parent,
    const RTSModelAttachment& attachment){
    const auto index=rtsFindUniqueModelNode(nodes,attachment.socket);
    if(!index)return std::nullopt;
    Mat4 node;
    for(int i=0;i<16;++i)node.m[i]=nodes[*index].model[i];
    return parent*node*Mat4::translate(attachment.offset)*
        Mat4::rotateY(toRadians(attachment.yaw))*
        Mat4::scale(Vec3(attachment.scale.x*attachment.modelScale.x,
                         attachment.scale.y*attachment.modelScale.y,
                         attachment.scale.z*attachment.modelScale.z));
}
