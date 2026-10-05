#pragma once
#include "ofMain.h"
#include <atomic>
#include <mutex>
#include <thread>

// CONEXIÓN REUTILIZABLE: llamar setup() en setup, chat() al enviar y update()
// en cada frame. Escuchar response/error para recibir el resultado.
// Todos los métodos públicos se llaman desde el hilo principal de OF.
// Solo HTTP corre en un worker. false significa ocupado/no iniciado; no encola.
class ofxLocalAI {
public:
    ofxLocalAI();
    ~ofxLocalAI();
    void setup(const std::string& url = "http://127.0.0.1:11434");
    bool chat(const ofJson& request);
    bool listModels();
    void update();
    void close();
    bool busy() const { return inFlight; }
    ofEvent<ofJson> response; // {request: chat|models, body: respuesta nativa de Ollama}
    ofEvent<std::string> error;
private:
    bool start(const std::string& path, const std::string& kind, const ofJson* body);
    std::string baseUrl;
    bool active = false, inFlight = false, completed = false;
    std::atomic<bool> stopping{false};
    std::thread worker;
    std::mutex mutex;
    ofJson result;
    std::string failure;
};
