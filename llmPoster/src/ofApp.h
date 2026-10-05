#pragma once
#include "ofMain.h"
#include "ofxLocalAI.h"
#include "PosterView.h"
#include "ChatSession.h"
#include "ofxTextReveal.h"
#include "TextStyle.h"

// Recorrido para clase: setup -> send -> update -> response -> draw (ver ofApp.cpp).
// Tres capas separadas a propósito, para poder cambiar una sin tocar las otras:
//   CONEXIÓN  ofxLocalAI            habla HTTP con Ollama (reutilizable en otra obra)
//   CONTRATO  PosterSpec/ChatSession qué datos aceptamos y cuáles rechazamos
//   OBRA      PosterView.h           cómo se ven y se mueven esos datos
class ofApp:public ofBaseApp {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void mousePressed(int x,int y,int button) override;
    void exit() override;
    void mouseScrolled(int x,int y,float scrollX,float scrollY) override;
    void send();
    void response(ofJson& event);
    void error(std::string& message);
    void apply(const std::string& raw,bool recorded);
    void connect();
    void loadChatConfig();
    bool check=false;
    std::string preview;
private:
    // CONEXIÓN: este cliente se puede reutilizar en otra obra de OF.
    ofxLocalAI llm;
    // PRESENTACIÓN: cambia la velocidad en response(), sin tocar el modelo.
    ofxTextReveal typing;
    TextStyle text;
    PosterSpec poster;
    // DATOS: historial completo y validado; no depende del texto visible animado.
    ChatSession conversation;
    // chatPrompt: instrucciones del modo CHAT. sentInput: lo último que enviamos.
    std::string chatPrompt,sentInput;
    bool chatMode=true,showJson=false;
    int chatScroll=0;
    // CONTEXTO (bin/data/chat-config.json): numCtx = ventana de Ollama en tokens.
    // promptTokens = cuántos tokens ocupó el último pedido (lo informa Ollama).
    int numCtx=4096,promptTokens=0;
    std::string input="Hola! Me ayudas a imaginar una experiencia interactiva?",raw,systemPrompt;
    std::string modelId,url,status="Escribi una idea y pulsa Interpretar",detail;
    // pending = esperando HTTP; typing.active() = respuesta recibida que se revela.
    bool pending=false,recorded=true,initialized=false;
    double pendingSince=0;
    bool editing=false,selectAll=false;
    bool busy() const { return pending; }
};
