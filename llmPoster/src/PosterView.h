#pragma once
#include "ofMain.h"
#include "PosterSpec.h"
#include "TextStyle.h"

// MODIFICAR AQUÍ EN CLASE: el LLM propone etiquetas; OF decide su significado visual.
// Ejercicio: reemplazar la onda de calma, cambiar colores o mapear ritmo a otra acción.
//
// Analogía: el modelo escribe la partitura ("piano", "allegro"); esta función
// es la orquesta. La misma partitura con otra orquesta suena distinta.
// Todo lo que se ve acá es código programado a mano: no es imagen generada.
inline void drawPoster(const PosterSpec& spec,const ofRectangle& rect,double seconds,const TextStyle& text) {
    ofPushStyle();
    // MAPEO 1 · paleta -> colores de fondo y tinta.
    const ofColor background=spec.paleta=="mar" ? ofColor(17,40,73) : spec.paleta=="sol" ? ofColor(84,37,24) : ofColor(31,23,60);
    const ofColor ink=spec.paleta=="mar" ? ofColor(142,214,255) : spec.paleta=="sol" ? ofColor(255,212,123) : ofColor(228,178,255);
    // MAPEO 2 · ritmo -> velocidad de todas las animaciones.
    const float speed=spec.ritmo=="lento" ? .6f : spec.ritmo=="medio" ? 1.4f : 3.f;
    // MAPEO 3 · animo -> cuánto se mueven las líneas y el título.
    const float amplitude=spec.animo=="calma" ? 3.f : spec.animo=="alegria" ? 14.f : 6.f;
    ofSetColor(background); ofDrawRectRounded(rect,18);
    // Líneas de fondo que ondulan con el tiempo (seno = movimiento de vaivén).
    ofSetColor(ink,45);
    for (int i=1;i<9;++i) {
        float y=rect.y+i*rect.height/9 + amplitude*std::sin(seconds*speed+i*.5);
        ofDrawLine(rect.x+20,y,rect.getRight()-20,y);
    }
    // Cada tono tiene un gesto distinto, dibujado por OF; no es video generado.
    ofNoFill(); ofSetLineWidth(2); ofSetColor(ink,90);
    const float cx=rect.x+rect.width*.5f, cy=rect.y+rect.height*.76f;
    if (spec.animo=="calma") {
        // calma: tres elipses que se agrandan y achican, como una respiración.
        const float breath=8.f*std::sin(seconds*speed);
        for (int i=0;i<3;++i) ofDrawEllipse(cx,cy,100+i*55+breath,28+i*15+breath*.3f);
    } else if (spec.animo=="alegria") {
        // alegria: cinco círculos que rebotan (abs(sin) = rebote siempre hacia arriba).
        for (int i=0;i<5;++i) {
            const float bounce=std::abs(std::sin(seconds*speed+i*.7f));
            ofDrawCircle(cx+(i-2)*65,cy-45*bounce,12+8*bounce);
        }
    } else {
        // tension: una línea en zigzag que vibra.
        ofPolyline zigzag;
        for (int i=0;i<=16;++i)
            zigzag.addVertex(cx-160+i*20,cy+(i%2 ? -1 : 1)*(12+8*std::sin(seconds*speed+i)));
        zigzag.draw();
    }
    ofFill();
    // TÍTULO: cortar en renglones de hasta 20 caracteres sin partir palabras.
    // Fuente incluida en bin/data (TextStyle.h): no depende de las fuentes de cada equipo.
    std::string wrapped; int column=0;
    for (const auto& word:ofSplitString(spec.titulo," ",true,true)) {
        const auto length=std::count_if(word.begin(),word.end(),[](unsigned char c){return (c&0xc0)!=0x80;});
        if (column && column+1+length>20) { wrapped+='\n'; column=0; }
        if (column) { wrapped+=' '; ++column; }
        for (auto cp:ofUTF8Iterator(word)) {
            if (column++==20) { wrapped+='\n'; column=1; }
            ofUTF8Append(wrapped,cp);
        }
    }
    // El título también flota con el ánimo; con "tension" además se inclina.
    ofPushMatrix();
    ofTranslate(rect.x+36,rect.y+rect.height*.42+amplitude*std::sin(seconds*speed));
    if (spec.animo=="tension") ofRotateDeg(2*std::sin(seconds*speed));
    ofSetColor(ink); text.drawTitle(wrapped);
    ofPopMatrix(); ofPopStyle();
}
