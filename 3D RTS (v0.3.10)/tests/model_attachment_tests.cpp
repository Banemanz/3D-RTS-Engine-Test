#include "RTSProject.hpp"
#include "EditorSession.hpp"
#include "RTSGltfImport.hpp"
#include "RTSAssetAudit.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#if defined(NDEBUG)
#error "Attachment tests require asserts in Release"
#endif
int main(int argc,char** argv){
    assert(argc==3);
    auto mesh=rtsCompileGltf(argv[1]);
    assert(mesh.nodes.size()==4);
    RTSModelAttachment a;a.socket="Amber cap";a.offset=Vec3(0,1,0);
    a.scale=Vec3(.5f,.5f,.5f);
    auto t=rtsAttachmentTransform(mesh.nodes,Mat4::translate(Vec3(10,2,3)),a);
    assert(t && std::abs(t->m[12]-10.35f)<1e-5f && std::abs(t->m[13]-3.35f)<1e-5f);
    a.socket="not found";assert(!rtsAttachmentTransform(mesh.nodes,Mat4(),a));
    RTSProject project;project.loadFile(argv[2]);
    a.socket="Amber cap";a.modelId="imported_beacon_model";
    EditorSession e;e.panel=EditorSession::Panel::Data;
    assert(e.selectCatalogId(project,"dual_beacon_actor"));
    assert(e.setCatalogField(project,"attachments",serializeModelAttachments({a})));
    const auto visual=actorFromCatalog(project.catalog,"dual_beacon_actor");
    assert(visual.attachments.size()==1 && visual.attachments[0].mesh=="assets/beacon.rtsmesh");
    assert(!project.catalogReferences(a.modelId).empty());
    assert(!project.removeCatalogObject(a.modelId));
    assert(e.undo(project));assert(actorFromCatalog(project.catalog,"dual_beacon_actor").attachments.empty());
    assert(e.redo(project));
    assert(project.renameCatalogObject(a.modelId,"renamed_attachment_model"));
    assert(actorFromCatalog(project.catalog,"dual_beacon_actor").attachments[0].modelId=="renamed_attachment_model");
    assert(!rtsAuditProjectAssets(project,argv[2]).valid());
    std::cout<<"Attachment transform, typed catalog validation, undo, rename and socket audit passed\n";
}
