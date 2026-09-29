#include "ChatSession.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
    ChatSession chat;
    if (argc>1) {
        std::string raw((std::istreambuf_iterator<char>(std::cin)),{});
        chat.accept("Pregunta de prueba",raw);
        std::cout<<"CHAT_CONTRACT_OK\n"; return 0;
    }
    nlohmann::json visual={{"titulo","Una idea"},{"animo","calma"},{"ritmo","lento"},{"paleta","mar"},{"motivo","Un movimiento suave"}};
    nlohmann::json good={{"respuesta","Hola! Podemos imaginar un paisaje."},{"visual",visual}};
    auto messages=chat.messages("system","Hola");
    assert(messages.size()==2 && messages[0]["role"]=="system");
    chat.accept("Hola",good.dump());
    messages=chat.messages("system","Segui esa idea");
    assert(messages.size()==4 && messages[1]["content"]=="Hola");
    assert(messages[2]["content"]==good.dump() && messages[3]["content"]=="Segui esa idea");
    auto reject=[&](const std::string& raw) {
        const auto before=chat.messages("system","next");
        bool failed=false; try { chat.accept("bad",raw); } catch (...) { failed=true; }
        assert(failed && before==chat.messages("system","next"));
    };
    reject("```json\n"+good.dump()+"\n```");
    reject(std::string(8193,'a'));
    for (auto field:{"respuesta","visual"}) {
        auto bad=good; bad.erase(field); reject(bad.dump());
        bad=good; bad[field]=42; reject(bad.dump());
    }
    for (auto answer:{std::string(" \t\n"),std::string(701,'x'),std::string("bad\x01")}) {
        auto bad=good; bad["respuesta"]=answer; reject(bad.dump());
    }
    auto bad=good; bad["extra"]="code"; reject(bad.dump());
    bad=good; bad["visual"]["animo"]="invalid"; reject(bad.dump());
    for (int i=0;i<5;++i) chat.accept(std::to_string(i),good.dump());
    assert(chat.turns.size()==4 && chat.turns.front().user=="1");
    messages=chat.messages("system","last");
    assert(messages.size()==10 && messages[1]["role"]=="user" && messages[8]["role"]=="assistant");
    chat.turns.clear(); assert(chat.messages("system","new").size()==2);
    // Memoria configurable (chat-config.json / teclas + y -): bajar el límite olvida lo más viejo.
    for (int i=0;i<4;++i) chat.accept(std::to_string(i),good.dump());
    chat.setLimit(2); assert(chat.turns.size()==2 && chat.turns.front().user=="2");
    chat.setLimit(0); chat.accept("solo",good.dump()); assert(chat.turns.empty());
    assert(chat.messages("system","nuevo").size()==2);  // sin memoria: system + mensaje nuevo
    chat.setLimit(99); assert(chat.limit==20);           // tope
    chat.accept("x",good.dump()); chat.clear(); assert(chat.turns.empty());
    std::cout<<"Chat: validation, rejection without mutation, history, bounded context and reset OK\n";
}
