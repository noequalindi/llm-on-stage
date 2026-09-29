#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

// CONTRATO: el modelo propone datos para un escenario, no código C++.
// Mantener estos límites junto al prompt. Validar antes de crear cuerpos Box2D.
//
// Los límites existen para proteger la obra: sin ellos el modelo podría pedir
// 10.000 cuerpos (la compu se cuelga) o gravedad 999 (todo sale disparado).
// Nueve campos: respuesta, titulo, paleta, gravedad, rebote, cantidad,
// forma, interaccion, palabras. Ver la tabla en ../README.md.
//
// Para agregar un campo, cambiar JUNTOS: system-prompt.txt, schema(), parse()
// y recorded.json; después usarlo en StagePhysics.
struct SceneSpec {
    std::string respuesta,titulo,paleta,forma,interaccion;
    float gravedad=3,rebote=.6f;
    int cantidad=12;
    std::vector<std::string> palabras;
    // Ollama acepta un JSON Schema en format. Guía la generación, pero no
    // reemplaza parse(): validar siempre los datos que llegan por la red.
    // El esquema es la "plantilla del formulario" que viaja en el pedido HTTP.
    // Tiene los mismos límites que parse(): si cambian uno, cambien el otro.
    static nlohmann::json schema() {
        using nlohmann::json;
        return {
            {"type","object"},
            {"additionalProperties",false},
            {"required",json::array({"respuesta","titulo","paleta","gravedad","rebote","cantidad","forma","interaccion","palabras"})},
            {"properties",{
                {"respuesta",{{"type","string"},{"minLength",1},{"maxLength",400}}},
                {"titulo",{{"type","string"},{"minLength",1},{"maxLength",48}}},
                {"paleta",{{"type","string"},{"enum",json::array({"mar","sol","noche","fuego"})}}},
                {"gravedad",{{"type","number"},{"minimum",-5},{"maximum",15}}},
                {"rebote",{{"type","number"},{"minimum",0},{"maximum",1}}},
                {"cantidad",{{"type","integer"},{"minimum",6},{"maximum",24}}},
                {"forma",{{"type","string"},{"enum",json::array({"circulos","cajas"})}}},
                {"interaccion",{{"type","string"},{"enum",json::array({"atraer","repeler"})}}},
                {"palabras",{{"type","array"},{"minItems",1},{"maxItems",6},
                    {"items",{{"type","string"},{"minLength",1},{"maxLength",10}}}}}
            }}
        };
    }
    // Control de calidad en la puerta: si algo no cumple, lanza un error
    // y la app conserva la escena anterior. Todo o nada: no se aplica "a medias".
    static SceneSpec parse(const std::string& raw) {
        if (raw.size()>8192) throw std::runtime_error("Escena demasiado larga");
        const auto j=nlohmann::json::parse(raw);
        if (!j.is_object() || j.size()!=9) throw std::runtime_error("La escena requiere exactamente nueve campos");
        // Texto: no vacío, largo máximo en CARACTERES (UTF-8) y sin caracteres de control.
        auto text=[&](const nlohmann::json& value,int max,bool multiline=false) {
            const auto s=value.get<std::string>();
            const auto length=std::count_if(s.begin(),s.end(),[](unsigned char c){return (c&0xc0)!=0x80;});
            if (s.find_first_not_of(" \r\n\t")==std::string::npos || length>max)
                throw std::runtime_error("Texto vacio o demasiado largo en la escena");
            for (unsigned char c:s) if (c<32 && !(multiline && (c=='\n'||c=='\t'||c=='\r')))
                throw std::runtime_error("Caracter de control en escena");
            return s;
        };
        SceneSpec s;
        s.respuesta=text(j.at("respuesta"),700,true); s.titulo=text(j.at("titulo"),48);
        s.paleta=text(j.at("paleta"),10); s.forma=text(j.at("forma"),10); s.interaccion=text(j.at("interaccion"),10);
        // Categorías: solo valores de una lista cerrada.
        if (s.paleta!="mar" && s.paleta!="sol" && s.paleta!="noche" && s.paleta!="fuego") throw std::runtime_error("Paleta: mar, sol, noche o fuego");
        if (s.forma!="circulos" && s.forma!="cajas") throw std::runtime_error("Forma: circulos o cajas");
        if (s.interaccion!="atraer" && s.interaccion!="repeler") throw std::runtime_error("Interaccion: atraer o repeler");
        // Números: que sean números de verdad (no "5" como texto) y dentro del rango.
        auto number=[&](const char* key,double min,double max) {
            const auto& v=j.at(key);
            if (!v.is_number()) throw std::runtime_error(std::string(key)+" debe ser un numero");
            double x=v.get<double>();
            if (!std::isfinite(x)||x<min||x>max) throw std::runtime_error(std::string(key)+" fuera del rango permitido");
            return x;
        };
        s.gravedad=number("gravedad",-5,15); s.rebote=number("rebote",0,1);
        if (!j.at("cantidad").is_number_integer()) throw std::runtime_error("Cantidad debe ser entera");
        s.cantidad=static_cast<int>(number("cantidad",6,24));
        const auto& words=j.at("palabras");
        if (!words.is_array() || words.empty() || words.size()>6) throw std::runtime_error("Palabras: entre 1 y 6 etiquetas");
        for (const auto& word:words) s.palabras.push_back(text(word,10));
        return s;
    }
};
