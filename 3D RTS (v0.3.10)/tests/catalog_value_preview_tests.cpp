#include "RTSCatalogValuePreview.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Catalog preview tests require Release assertions"
#endif
#include <iostream>
int main(){
    assert(rtsFormatCatalogPreview(picojson::value(.69999999999999996))=="0.7");
    assert(rtsFormatCatalogPreview(picojson::value(0.))=="0");
    assert(rtsFormatCatalogPreview(picojson::value(-0.))=="0");
    assert(rtsFormatCatalogPreview(picojson::value(123.45678))=="123.45678");
    assert(rtsFormatCatalogPreview(picojson::value(-8.500000000001))=="-8.5");
    assert(rtsFormatCatalogPreview(picojson::value(true))=="true");
    assert(rtsFormatCatalogPreview(picojson::value("piercer_rifle"))==
           "\"piercer_rifle\"");
    const auto raw=picojson::value(.69999999999999996);
    assert(raw.get<double>()==.69999999999999996);
    assert(raw.serialize()==picojson::value(.69999999999999996).serialize());
    std::cout<<"Readable F7 properties preserve exact authored JSON numeric values\n";
}
