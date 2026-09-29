#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <stdexcept>

// CONTRATO DEL AFICHE: recibimos datos, nunca código ejecutable.
// Un prompt no garantiza valores correctos: comprobar antes de usar en la escena.
//
// Analogía: un formulario con casilleros en vez de una carta libre. El modelo
// "completa el formulario" y parse() es el control de calidad en la puerta:
// si falta un campo o un valor no está en la lista, se rechaza TODO.
// Ojo: JSON válido no es lo mismo que interpretación correcta. parse() controla
// el formato; si "alegria" le queda bien a un texto triste, lo juzgamos nosotros.
//
// Para agregar un campo: cambiarlo acá, en los prompts de bin/data y en recorded.json.
struct PosterSpec {
    // Valores por defecto: lo que se ve si nunca llegó una respuesta válida.
    std::string titulo="ESCRIBI UNA IDEA",animo="calma",ritmo="lento",paleta="mar",motivo="";
    static PosterSpec parse(const std::string& raw) {
        if (raw.size()>4096) throw std::runtime_error("Respuesta demasiado larga");
        const auto json=nlohmann::json::parse(raw); // Rechazar Markdown y texto fuera del JSON.
        if (!json.is_object() || json.size()!=5) throw std::runtime_error("Se necesitan exactamente cinco campos");
        PosterSpec value;
        value.titulo=json.at("titulo").get<std::string>();
        value.animo=json.at("animo").get<std::string>();
        value.ritmo=json.at("ritmo").get<std::string>();
        value.paleta=json.at("paleta").get<std::string>();
        value.motivo=json.at("motivo").get<std::string>();
        // Largo en CARACTERES (UTF-8), no en bytes: "ñ" cuenta como 1.
        auto length=[](const std::string& text) { return std::count_if(text.begin(),text.end(),[](unsigned char c){return (c&0xc0)!=0x80;}); };
        if (length(value.titulo)<1 || length(value.titulo)>60 || length(value.motivo)>200 ||
            value.titulo.find_first_of("\r\n\t")!=std::string::npos)
            throw std::runtime_error("Titulo: 1-60 caracteres en una linea; motivo: hasta 200");
        // Listas cerradas: el modelo solo puede elegir entre estas opciones.
        if (value.animo!="calma" && value.animo!="alegria" && value.animo!="tension") throw std::runtime_error("Animo fuera del contrato");
        if (value.ritmo!="lento" && value.ritmo!="medio" && value.ritmo!="rapido") throw std::runtime_error("Ritmo fuera del contrato");
        if (value.paleta!="mar" && value.paleta!="sol" && value.paleta!="noche") throw std::runtime_error("Paleta fuera del contrato");
        return value;
    }
};
