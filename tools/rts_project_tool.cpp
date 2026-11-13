#include "RTSProject.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

// Headless editing/validation interface for automation, modding pipelines and
// users who prefer code/text edits over the live property inspector.
static void usage() {
    std::cerr
      << "RTSProjectTool --create path\n"
      << "RTSProjectTool --validate path\n"
      << "RTSProjectTool --list path [catalog-type]\n"
      << "RTSProjectTool --get path object-id field\n"
      << "RTSProjectTool --set path object-id field json-value\n"
      << "RTSProjectTool --derive path parent-id new-id\n"
      << "RTSProjectTool --place path kind catalog-id x z owner\n"
      << "RTSProjectTool --placements path\n";
}
int main(int argc,char** argv) {
    try {
        if(argc<3){usage();return 2;}
        const std::string action=argv[1],path=argv[2];
        if(action=="--create") {
            if(argc!=3){usage();return 2;}
            RTSProject::defaults().saveFile(path);
            std::cout<<"Created project: "<<path<<"\n";return 0;
        }
        RTSProject project;
        project.loadFile(path);
        if(action=="--validate") {
            if(argc!=3){usage();return 2;}
            project.validate();
            std::cout<<"Valid project: "<<project.name<<", "
                     <<project.catalog.entries().size()<<" catalog objects, "
                     <<project.placements.size()<<" placements\n";
            return 0;
        }
        if(action=="--list") {
            if(argc>4){usage();return 2;}
            std::string type=argc==4?argv[3]:"";
            for(const auto& entry:project.catalog.entries())
                if(type.empty() || entry.type==type)
                    std::cout<<entry.type<<" "<<entry.id
                             <<(entry.parent.empty()?"":" : "+entry.parent)<<"\n";
            return 0;
        }
        if(action=="--placements") {
            if(argc!=3){usage();return 2;}
            for(const auto& p:project.placements)
                std::cout<<p.id<<" "<<p.kind<<" "<<p.catalogId
                         <<" ("<<p.position.x<<","<<p.position.z<<") owner "<<p.owner<<"\n";
            return 0;
        }
        if(action=="--get") {
            if(argc!=5){usage();return 2;}
            auto fields=project.catalog.resolvedFields(argv[3]);
            auto i=fields.find(argv[4]);
            if(i==fields.end())throw std::invalid_argument("Unknown property");
            std::cout<<i->second.serialize()<<"\n";return 0;
        }
        if(action=="--set") {
            if(argc!=6){usage();return 2;}
            picojson::value value;
            std::string input=argv[5],error=picojson::parse(value,input);
            if(!error.empty())value=picojson::value(input);
            if(!project.catalog.setField(argv[3],argv[4],value))
                throw std::invalid_argument("Invalid catalog object or property key");
        }else if(action=="--derive") {
            if(argc!=5){usage();return 2;}
            if(!project.catalog.cloneDerived(argv[3],argv[4]))
                throw std::invalid_argument("Invalid parent or duplicate ID");
        }else if(action=="--place") {
            if(argc!=8){usage();return 2;}
            ProjectPlacement p;
            p.id="cli_"+std::to_string(project.placements.size()+1);
            p.kind=argv[3];p.catalogId=argv[4];p.owner=std::stoi(argv[7]);
            p.position={std::stof(argv[5]),0,std::stof(argv[6])};
            p.position.y=project.terrain.sampleHeight(p.position.x,p.position.z);
            p.name=p.catalogId.empty()?p.kind:p.catalogId;
            if(p.kind=="Mineral")p.amount=750;
            project.placements.push_back(p);
        }else{usage();return 2;}
        project.validate();
        project.saveFile(path);
        std::cout<<"Saved: "<<path<<"\n";
        return 0;
    } catch(const std::exception& e){
        std::cerr<<"RTSProjectTool: "<<e.what()<<"\n";
        return 1;
    }
}
