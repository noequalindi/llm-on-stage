#include "ofApp.h"
// Punto de entrada. --preview guarda una captura y --check prueba el contrato
// con el modelo real (los usan los scripts de validación, no hace falta en clase).
int main(int argc,char** argv) {
    auto app=std::make_shared<ofApp>();
    if (argc==3 && std::string(argv[1])=="--preview") app->preview=argv[2];
    if (argc==3 && std::string(argv[1])=="--check") {
        app->check=true; ofSetDataPathRoot(std::filesystem::absolute(argv[2]).string()+"/");
    }
    ofSetupOpenGL(1180,800,OF_WINDOW);
    return ofRunApp(app);
}
