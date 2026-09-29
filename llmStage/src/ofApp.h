#pragma once
#include "ofMain.h"
#include "ofxLocalLLM.h"
#include "ofxTextReveal.h"
#include "StagePhysics.h"
#include "TextStyle.h"

// Recorrido para clase: setup -> send -> response -> SceneSpec -> StagePhysics.
// Tres capas separadas a propósito, para poder cambiar una sin tocar las otras:
//   CONEXIÓN  ofxLocalLLM   habla HTTP con Ollama (la misma que usa llmPoster)
//   CONTRATO  SceneSpec     qué datos aceptamos y en qué rangos
//   OBRA      StagePhysics  física Box2D y dibujo; no sabe que existe un LLM
class ofApp:public ofBaseApp {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;
    void keyPressed(int key) override;
    void mousePressed(int x,int y,int button) override;
    void mouseReleased(int x,int y,int button) override;
    void send();
    void response(ofJson& event);
    void error(std::string& message);
    void apply(const std::string& value,bool sample);
    bool check=false;
    std::string preview;
private:
    ofxLocalLLM llm;        // Conexión reutilizable.
    ofxTextReveal typing;   // Presentación progresiva, después de recibir HTTP.
    StagePhysics stage;    // Física y dibujo; no conoce Ollama.
    SceneSpec scene;       // Datos validados que enlazan modelo y escena.
    TextStyle text;        // Fuente con ñ y tildes (ver TextStyle.h).
    std::string model,url,prompt,raw,detail,sentInput;
    std::string input="Un escenario lunar con palabras flotando y el mouse como iman";
    std::string status="Escribi una escena";
    // pending: esperando HTTP. held: mouse apretado dentro del escenario.
    bool pending=false,initialized=false,recorded=true,editing=false,selectAll=false,held=false,showJson=false;
    double pendingSince=0;
};
