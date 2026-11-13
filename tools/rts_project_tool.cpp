#include "RTSProject.hpp"
#include "TriggerEngine.hpp"
#include <algorithm>
#include <cmath>
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
      << "RTSProjectTool --rename path old-id new-id\n"
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
        TriggerEngine::validate(project);
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
        if(action=="--write-copy") {
            if(argc!=4){usage();return 2;}
            project.saveFile(argv[3]);
            std::cout<<"Copied validated project to "<<argv[3]<<"\\n";
            return 0;
        }
        if(action=="--get") {
            if(argc!=5){usage();return 2;}
            auto fields=project.catalog.resolvedFields(argv[3]);
            auto i=fields.find(argv[4]);
            if(i==fields.end())throw std::invalid_argument("Unknown property");
            std::cout<<i->second.serialize()<<"\n";return 0;
        }
        if(action=="--set-root") {
            if(argc!=5){usage();return 2;}
            const std::string name=argv[3];
            if(name=="schema_version" || name=="catalog" || name=="placements" ||
               name=="terrain" || name.empty())
                throw std::invalid_argument("Protected project root field");
            picojson::value v;
            const std::string error=picojson::parse(v,std::string(argv[4]));
            if(!error.empty())throw std::invalid_argument("Invalid JSON value: "+error);
            if(name=="name") {
                if(!v.is<std::string>())throw std::invalid_argument("Project name must be string");
                project.name=v.get<std::string>();
            } else if(name=="description") {
                if(!v.is<std::string>())throw std::invalid_argument("Description must be string");
                project.description=v.get<std::string>();
            } else if(name=="metadata") {
                if(!v.is<picojson::object>())throw std::invalid_argument("Metadata must be object");
                project.metadata=v.get<picojson::object>();
            } else project.extensions[name]=v;
        } else if(action=="--set-instance") {
            if(argc!=6){usage();return 2;}
            auto it=std::find_if(project.placements.begin(),project.placements.end(),
                [&](const ProjectPlacement& p){return p.id==argv[3];});
            if(it==project.placements.end())throw std::invalid_argument("Unknown placed ID");
            std::string field=argv[4];
            picojson::value v;
            std::string error=picojson::parse(v,std::string(argv[5]));
            if(!error.empty())throw std::invalid_argument("Invalid JSON value: "+error);
            if(field=="name") {
                if(!v.is<std::string>())throw std::invalid_argument("Instance name must be string");
                it->name=v.get<std::string>();
            } else if(field=="position" || field=="rotation" || field=="scale") {
                if(!v.is<picojson::array>() || v.get<picojson::array>().size()!=3)
                    throw std::invalid_argument("Transform requires [x,y,z]");
                const auto& a=v.get<picojson::array>();
                for(const auto& component:a)
                    if(!component.is<double>() || !std::isfinite(component.get<double>()) ||
                       std::abs(component.get<double>())>100000)
                        throw std::invalid_argument("Transform requires three finite coordinates");
                Vec3 next{float(a[0].get<double>()),float(a[1].get<double>()),float(a[2].get<double>())};
                if(field=="position")it->position=next;
                else if(field=="rotation")it->rotation=next;
                else it->scale=next;
            } else if(field=="owner" || field=="amount") {
                if(!v.is<double>() || !std::isfinite(v.get<double>()) ||
                   std::floor(v.get<double>())!=v.get<double>() ||
                   v.get<double>()< -1 || v.get<double>()>10000000)
                    throw std::invalid_argument("Owner/amount must be integer");
                if(field=="owner")it->owner=static_cast<int>(v.get<double>());
                else it->amount=static_cast<int>(v.get<double>());
            } else if(field=="overrides") {
                if(!v.is<picojson::object>())throw std::invalid_argument("overrides must be object");
                it->overrides=v.get<picojson::object>();
            } else if(field=="catalog") {
                if(!v.is<std::string>())throw std::invalid_argument("catalog must be string");
                it->catalogId=v.get<std::string>();
            } else throw std::invalid_argument("Unknown/immutable placement field: "+field);
        } else if(action=="--paint-cell") {
            if(argc!=8){usage();return 2;}
            int x=std::stoi(argv[3]),z=std::stoi(argv[4]);
            TerrainMap::Cell cell;
            cell.height=std::stof(argv[5]);
            int material=std::stoi(argv[6]);
            if(material<0 || material>7)throw std::invalid_argument("Material index must be 0..7");
            cell.material=static_cast<std::uint8_t>(material);
            const int blocked=std::stoi(argv[7]);
            if(blocked!=0 && blocked!=1)throw std::invalid_argument("blocked must be 0 or 1");
            cell.blocked=blocked==1;
            if(!project.terrain.setCell(x,z,cell))
                throw std::invalid_argument("Cell outside terrain or invalid value");
        } else if(action=="--set") {
            if(argc!=6){usage();return 2;}
            picojson::value value;
            std::string input=argv[5],error=picojson::parse(value,input);
            if(!error.empty())value=picojson::value(input);
            if(!project.catalog.setField(argv[3],argv[4],value))
                throw std::invalid_argument("Invalid catalog object or property key");
        }else if(action=="--rename") {
            if(argc!=5){usage();return 2;}
            if(!project.renameCatalogObject(argv[3],argv[4]))
                throw std::invalid_argument("Unknown, invalid or duplicate catalog ID");
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
        TriggerEngine::validate(project);
        project.saveFile(path);
        std::cout<<"Saved: "<<path<<"\n";
        return 0;
    } catch(const std::exception& e){
        std::cerr<<"RTSProjectTool: "<<e.what()<<"\n";
        return 1;
    }
}
