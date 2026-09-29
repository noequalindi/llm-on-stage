#pragma once
#include "ofMain.h"

// TEXTO: ofDrawBitmapString solo trae caracteres ASCII, por eso no dibuja ñ, tildes ni ¿¡.
// El modelo sí los envía bien (UTF-8); el límite estaba en la fuente.
// Cargamos Liberation Mono (bin/data, licencia SIL OFL, viene con los ejemplos de OF)
// con el alfabeto latino. Es monoespaciada: los ajustes por columnas siguen valiendo.
// Si falta el archivo, vuelve a la fuente bitmap y el ejemplo sigue funcionando.
struct TextStyle {
    ofTrueTypeFont body,small,title;
    bool ready=false;
    void setup(const std::string& file="LiberationMono-Regular.ttf") {
        // Tamaños elegidos para ocupar lo mismo que la fuente bitmap (8 px por columna).
        ready=load(body,file,10,13.6f) && load(small,file,8,10.9f) && load(title,file,32,43.5f);
        if (!ready) ofLogWarning("TextStyle")<<"No se pudo cargar "<<file<<": se usa la fuente bitmap (sin tildes ni ñ)";
    }
    // Mismo uso que ofDrawBitmapString(text,x,y).
    void draw(const std::string& text,float x,float y) const {
        if (ready) body.drawString(text,x,y); else ofDrawBitmapString(text,x,y);
    }
    // Texto chico (80%): etiquetas dentro de cuerpos. Se carga a ese tamaño en lugar de
    // escalar con ofScale(), que pixela la fuente. 6.4 px por columna.
    void drawSmall(const std::string& text,float x,float y) const {
        if (ready) { small.drawString(text,x,y); return; }
        ofPushMatrix(); ofScale(.8f,.8f); ofSetDrawBitmapMode(OF_BITMAPMODE_MODEL);
        ofDrawBitmapString(text,x/.8f,y/.8f); ofPopMatrix();
    }
    // Título del afiche en el origen actual: 3.2 veces el texto común.
    void drawTitle(const std::string& text) const {
        if (ready) { title.drawString(text,0,0); return; }
        ofScale(3.2f,3.2f); ofSetDrawBitmapMode(OF_BITMAPMODE_MODEL); ofDrawBitmapString(text,0,0);
    }
private:
    static bool load(ofTrueTypeFont& font,const std::string& file,int size,float lineHeight) {
        ofTrueTypeFontSettings settings(file,size);
        settings.addRanges({ofUnicode::Latin1Supplement,ofUnicode::LatinA});
        settings.addRange(ofUnicode::GeneralPunctuation); // comillas “ ” y rayas —
        if (!font.load(settings)) return false;
        font.setLineHeight(lineHeight); return true;
    }
};
