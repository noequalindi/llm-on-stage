#include "SceneSpec.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
    if(argc>1) {
        std::string raw((std::istreambuf_iterator<char>(std::cin)),{});
        auto scene=SceneSpec::parse(raw);
        std::cout<<"SCENE_CONTRACT_OK bodies="<<scene.cantidad<<"\n";return 0;
    }
    using nlohmann::json;
    json good={{"respuesta","Flotamos."},{"titulo","Luna"},{"paleta","noche"},
        {"gravedad",-1},{"rebote",.5},{"cantidad",12},{"forma","circulos"},
        {"interaccion","atraer"},{"palabras",json::array({"luna",u8"órbita"})}};
    auto scene=SceneSpec::parse(good.dump());
    assert(scene.cantidad==12 && scene.palabras.size()==2 && scene.gravedad==-1);
    auto reject=[](const std::string& raw){
        bool failed=false;try{SceneSpec::parse(raw);}catch(...){failed=true;}assert(failed);
    };
    for(auto it=good.begin();it!=good.end();++it){auto bad=good;bad.erase(it.key());reject(bad.dump());}
    auto bad=good;bad["extra"]="execute";reject(bad.dump());
    for(auto count: {json(0),json(25),json(12.5),json("12"),json(true),json(18446744073709551615ULL)}){
        bad=good;bad["cantidad"]=count;reject(bad.dump());
    }
    for(auto field:{"gravedad","rebote"}){
        for(auto value:{json(-10),json(16),json("1"),json(true),json(nullptr)}){
            bad=good;bad[field]=value;reject(bad.dump());
        }
    }
    bad=good;bad["paleta"]="fuego";assert(SceneSpec::parse(bad.dump()).paleta=="fuego");
    for(auto field:{"forma","paleta","interaccion"}){bad=good;bad[field]="desconocido";reject(bad.dump());}
    for(auto words:{json::array(),json("luna"),json::array({""}),json::array({1}),json::array({"01234567890"}),json::array({"a","b","c","d","e","f","g"})}){
        bad=good;bad["palabras"]=words;reject(bad.dump());
    }
    for(auto value:{std::string(" "),std::string(701,'x'),std::string("bad\x01")}){bad=good;bad["respuesta"]=value;reject(bad.dump());}
    bad=good;bad["titulo"]="dos\nlineas";reject(bad.dump());
    reject("```json\n"+good.dump()+"\n```");reject(std::string(8193,'x'));
    for(auto gravity:{-5,15})for(auto bounce:{0,1})for(auto count:{6,24}){
        auto edge=good;edge["gravedad"]=gravity;edge["rebote"]=bounce;edge["cantidad"]=count;
        assert(SceneSpec::parse(edge.dump()).cantidad==count);
    }
    std::cout<<"Scene: required fields, types, ranges, UTF-8 and unsafe output rejection OK\n";
}
