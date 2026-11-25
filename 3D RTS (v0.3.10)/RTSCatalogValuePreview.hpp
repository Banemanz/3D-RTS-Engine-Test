#pragma once
#include "DataCatalog.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

// Presentation-only: do not round actual authoring data or change project
// serialization. Repeatedly writing a rounded float would damage replays.
inline std::string rtsFormatCatalogPreview(const picojson::value& value){
    if(!value.is<double>())return value.serialize();
    const double n=value.get<double>();
    if(!std::isfinite(n))return value.serialize();
    if(n==0.)return "0";
    std::ostringstream stream;
    if(std::fabs(n)>=1.e7 || std::fabs(n)<.00001)
        stream<<std::setprecision(6)<<n;
    else {
        stream<<std::fixed<<std::setprecision(5)<<n;
    }
    std::string rendered=stream.str();
    const auto pos=rendered.find('.');
    if(pos!=std::string::npos && rendered.find('e')==std::string::npos){
        while(!rendered.empty() && rendered.back()=='0')rendered.pop_back();
        if(!rendered.empty() && rendered.back()=='.')rendered.pop_back();
    }
    return rendered;
}
