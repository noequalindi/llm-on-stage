#include "PosterSpec.h"
#include <cassert>
#include <iostream>
int main() {
    nlohmann::json good={{"titulo","Río azul"},{"animo","calma"},{"ritmo","lento"},{"paleta","mar"},{"motivo","Texto de prueba"}};
    assert(PosterSpec::parse(good.dump()).paleta=="mar");
    auto rejected=[](const std::string& text) { try { PosterSpec::parse(text); return false; } catch (...) { return true; } };
    assert(rejected("```json\n"+good.dump()+"\n```"));
    for (auto field:{"titulo","animo","ritmo","paleta","motivo"}) {
        auto bad=good; bad.erase(field); assert(rejected(bad.dump()));
        bad=good; bad[field]=42; assert(rejected(bad.dump()));
    }
    auto bad=good; bad["animo"]="furia"; assert(rejected(bad.dump()));
    bad=good; bad["extra"]="execute"; assert(rejected(bad.dump()));
    bad=good; bad["titulo"]=std::string(61,'a'); assert(rejected(bad.dump()));
    bad=good; bad["titulo"]="dos\nlineas"; assert(rejected(bad.dump()));
    bad=good; bad["motivo"]=std::string(201,'a'); assert(rejected(bad.dump()));
    std::cout<<"Poster contract: valid UTF-8, enums, fields, types, bounds and invalid model output OK\n";
}
