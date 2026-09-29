#pragma once
#include "PosterSpec.h"
#include <vector>

// CONTRATO DEL CHAT: guarda pares completos y válidos. No dibuja ni hace HTTP.
// Para agregar nuevos datos, cambiar este validador y bin/data/chat-prompt.txt.
struct ChatTurn { std::string user, answer, json; };
struct ChatSession {
    std::vector<ChatTurn> turns;
    // CONTEXTO: cuántos intercambios se reenvían. Se lee de bin/data/chat-config.json
    // ("memoria") y se puede cambiar con + / - mientras corre la app. 0 = sin memoria.
    int limit=4;
    void setLimit(int value) {
        limit=std::clamp(value,0,20);
        while (static_cast<int>(turns.size())>limit) turns.erase(turns.begin()); // Olvidar los más viejos.
    }
    void clear() { turns.clear(); } // Borrar el contexto: el modelo no cambia, solo lo que le reenviamos.
    // La memoria consiste en reenviar mensajes, no en modificar pesos del LLM.
    // El modelo no recuerda nada entre consultas (cada pedido HTTP es independiente).
    // Analogía: Dory o "Memento". En cada mensaje le pasamos el cuaderno completo:
    //   [system] instrucciones  +  [user/assistant] x N turnos  +  [user] mensaje nuevo
    nlohmann::json messages(const std::string& system,const std::string& input) const {
        auto result=nlohmann::json::array({{{"role","system"},{"content",system}}});
        for (const auto& turn:turns) {
            result.push_back({{"role","user"},{"content",turn.user}});
            result.push_back({{"role","assistant"},{"content",turn.json}});
        }
        result.push_back({{"role","user"},{"content",input}});
        return result;
    }
    // Valida la respuesta del modelo y, SOLO si es válida, la guarda en el historial.
    // Devuelve la parte "visual" para que la app actualice el afiche.
    PosterSpec accept(const std::string& user,const std::string& raw) {
        if (raw.size()>8192) throw std::runtime_error("Respuesta de chat demasiado larga");
        const auto data=nlohmann::json::parse(raw);
        if (!data.is_object() || data.size()!=2) throw std::runtime_error("Chat requiere respuesta y visual");
        const auto answer=data.at("respuesta").get<std::string>();
        const auto length=std::count_if(answer.begin(),answer.end(),[](unsigned char c){return (c&0xc0)!=0x80;});
        if (answer.find_first_not_of(" \r\n\t")==std::string::npos || length>700)
            throw std::runtime_error("Respuesta: entre 1 y 700 caracteres");
        for (unsigned char c:answer)
            if (c<32 && c!='\n' && c!='\t' && c!='\r') throw std::runtime_error("Caracter de control en respuesta");
        // La parte visual reutiliza el mismo contrato que el modo AFICHE.
        const auto visual=PosterSpec::parse(data.at("visual").dump());
        turns.push_back({user,answer,raw});
        // Más historial = más texto por consulta = respuestas más lentas y más contexto ocupado.
        setLimit(limit); // Conservar solo los últimos "limit" intercambios, en RAM.
        return visual;
    }
};
