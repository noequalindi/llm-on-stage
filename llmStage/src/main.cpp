#include "ofApp.h"
// Ventana fija para que las coordenadas del escenario coincidan con los cuerpos.
int main(int argc,char** argv) {
    auto app=std::make_shared<ofApp>();
    if(argc==3&&std::string(argv[1])=="--preview")app->preview=argv[2];
    if(argc==3&&std::string(argv[1])=="--check"){
        app->check=true;ofSetDataPathRoot(std::filesystem::absolute(argv[2]).string()+"/");
    }
    ofGLFWWindowSettings settings;
    settings.setSize(1280,800);
    settings.resizable=false;
    auto window=ofCreateWindow(settings);
    ofRunApp(window,app);
    return ofRunMainLoop();
}
