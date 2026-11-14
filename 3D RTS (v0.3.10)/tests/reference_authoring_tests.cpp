#include "EditorSession.hpp"
#include <cassert>
#include <iostream>

int main(){
    RTSProject project=RTSProject::defaults();
    EditorSession editor;editor.panel=EditorSession::Panel::Data;
    assert(editor.selectCatalogId(project,"barracks"));
    const picojson::array defaultTraining=project.catalog.resolvedFields("barracks")
        .at("trainable_units").get<picojson::array>();
    assert(defaultTraining.size()==1);
    picojson::array training=defaultTraining;
    training.emplace_back("worker");
    assert(editor.setCatalogField(project,"trainable_units",picojson::value(training)));
    assert(project.catalog.resolvedFields("barracks").at("trainable_units")
        .get<picojson::array>().size()==2);
    const std::string beforeInvalid=project.catalog.resolvedFields("barracks")
        .at("trainable_units").serialize();
    auto invalid=training;invalid.emplace_back("worker");
    assert(!editor.setCatalogField(project,"trainable_units",picojson::value(invalid)));
    assert(project.catalog.resolvedFields("barracks").at("trainable_units").serialize()==beforeInvalid);
    invalid=training;invalid.emplace_back("command_center"); // structures cannot be trained
    assert(!editor.setCatalogField(project,"trainable_units",picojson::value(invalid)));
    assert(editor.undo(project));
    assert(project.catalog.resolvedFields("barracks").at("trainable_units")
        .get<picojson::array>()==defaultTraining);
    assert(editor.redo(project));
    assert(project.catalog.resolvedFields("barracks").at("trainable_units")
        .get<picojson::array>()==training);

    // Effect sequences share the same editable reference-array transaction
    // without a hardcoded whitelist of authored IDs.
    CatalogEntry sequence;
    sequence.type="Effect";sequence.id="custom_sequence";
    sequence.fields["kind"]=picojson::value("Sequence");
    sequence.fields["children"]=picojson::value(picojson::array{
        picojson::value("kinetic_pulse")});
    assert(project.catalog.add(sequence));
    project.validate();
    assert(editor.selectCatalogId(project,"custom_sequence"));
    const picojson::array expanded{picojson::value("kinetic_pulse"),
                                   picojson::value("restorative_pulse")};
    assert(editor.setCatalogField(project,"children",picojson::value(expanded)));
    const picojson::array reversed{picojson::value("restorative_pulse"),
                                   picojson::value("kinetic_pulse")};
    assert(editor.setCatalogField(project,"children",picojson::value(reversed)));
    assert(!editor.setCatalogField(project,"children",picojson::value(
        picojson::array{picojson::value("custom_sequence")}))); // cycle
    assert(!editor.setCatalogField(project,"children",picojson::value(picojson::array{})));
    assert(project.catalog.resolvedFields("custom_sequence").at("children")
        .get<picojson::array>()==reversed);
    assert(editor.undo(project));
    assert(project.catalog.resolvedFields("custom_sequence").at("children")
        .get<picojson::array>()==expanded);
    assert(editor.redo(project));
    assert(project.catalog.resolvedFields("custom_sequence").at("children")
        .get<picojson::array>()==reversed);
    RTSProject copy=RTSProject::fromJson(project.toJson());
    assert(copy.catalog.resolvedFields("custom_sequence").at("children")
        .get<picojson::array>()==reversed);
    std::cout<<"creator reference lists: production/effect editing and atomic rollback passed\n";
}
