#include "ofApp.h"

// ============================================================================
// MAPA DEL EJEMPLO (leer en clase en este orden)
//
//   texto del alumno ("un escenario lunar...")
//     -> 2. send()          arma el pedido: system prompt + texto + JSON Schema
//     -> ofxLocalAI        POST HTTP a Ollama (http://127.0.0.1:11434/api/chat)
//                           en otro hilo: la física nunca se congela
//     -> 3. update()        llm.update() entrega la respuesta al hilo de OF
//     -> response()/4. apply()  SceneSpec::parse() valida los 9 campos
//     -> StagePhysics::build()  crea cuerpos Box2D con esos datos
//     -> 5. draw()          dibuja la escena y la respuesta escrita
//
// Diferencia con llmPoster: acá el modelo propone NÚMEROS físicos (gravedad,
// rebote, cantidad) además de categorías. Box2D calcula el movimiento; OF dibuja.
// El modelo no dibuja, no simula y no escribe código C++.
// ============================================================================

namespace {
// Zonas de la ventana (fija de 1280 x 800, ver main.cpp).
const ofRectangle stageArea(24,96,720,520),editor(24,660,950,68),sendButton(990,660,266,68);
const ofRectangle resetButton(778,47,142,30),jsonButton(934,47,104,30),sampleButton(1052,47,204,30);

// Corta un texto en renglones de N columnas. Recorre CARACTERES (UTF-8), no bytes.
std::string wrap(const std::string& value,int columns,int maxLines) {
    std::string result; int col=0,line=1;
    for (auto cp:ofUTF8Iterator(value)) {
        if (cp=='\r') continue;
        if (cp=='\n'||col>=columns) {
            if (++line>maxLines) return result+"...";
            result+='\n';col=0;if(cp=='\n')continue;
        }
        ofUTF8Append(result,cp=='\t'?' ':cp);++col;
    }
    return result;
}

// Tres puntos animados mientras esperamos. Decoración: no mide progreso.
void dots(float x,float y,double t) {
    for (int i=0;i<3;++i) {
        float pulse=.5f+.5f*std::sin(t*5-i*.9);
        ofSetColor(144,215,255,100+155*pulse);
        ofDrawCircle(x+13*i,y-3*pulse,3+1.2f*pulse);
    }
}
}

// ----------------------------------------------------------------------------
// 1. PREPARAR. Registrar eventos antes de enviar; cargar configuración desde data.
// ----------------------------------------------------------------------------
void ofApp::setup() {
    ofSetFrameRate(60); ofSetVerticalSync(true); ofSetEscapeQuitsApp(false);
    ofSetWindowTitle("UNA / LLM STAGE / texto + Box2D");
    text.setup();           // Fuente con ñ y tildes: antes de cualquier draw().
    stage.setup(stageArea); // Mundo físico, paredes y plataformas fijas.
    try {
        // Escena GRABADA al abrir: se puede probar la física sin esperar al modelo.
        apply(ofBufferFromFile("recorded.json").getText(),true);
        if (!preview.empty()) return;

        // URL y modelo fuera del código (bin/data/connection.json).
        auto config=ofLoadJson("connection.json");
        model=config.at("model").get<std::string>();
        url=config.at("ollama_url").get<std::string>();

        // Las instrucciones para el modelo viven en un .txt editable.
        prompt=ofBufferFromFile("system-prompt.txt").getText();
        if (prompt.empty()) throw std::runtime_error("Falta system-prompt.txt");

        // EVENTOS: "cuando llegue la respuesta, llamá a response() o a error()".
        ofAddListener(llm.response,this,&ofApp::response);
        ofAddListener(llm.error,this,&ofApp::error);
        llm.setup(url);
        initialized=true;
        if (check) send();
    } catch (const std::exception& e) {
        detail=e.what(); status="Revisar bin/data. Esc y E copian detalle";
        if (check) ofExit(1);
    }
}

// ----------------------------------------------------------------------------
// 2. ENVIAR. Cambiar aquí la entrada: texto, un evento de la obra o un sensor.
// Se llama una vez por acción (Enter o botón), nunca en cada frame.
// ----------------------------------------------------------------------------
void ofApp::send() {
    if (typing.active()) {
        typing.finish(); // Enter mientras escribe muestra el texto completo.
        return;
    }
    if (!initialized || pending) return;
    if (input.find_first_not_of(" \r\n\t")==std::string::npos) {
        status="Escribi una escena primero";
        return;
    }

    // system: tarea y contrato. user: la idea de la persona.
    // format: en vez de "json" a secas (como llmPoster), mandamos un JSON Schema
    //   completo: Ollama guía al modelo para que respete campos, tipos y rangos.
    //   Es una ayuda, no una garantía: SceneSpec igualmente valida la salida.
    // temperature 0: misma idea -> misma escena (más fácil de comparar en clase).
    const ofJson request={
        {"model",model},
        {"stream",false},
        {"format",SceneSpec::schema()},
        {"options",{{"temperature",0.},{"num_predict",768}}},
        {"messages",ofJson::array({
            {{"role","system"},{"content",prompt}},
            {{"role","user"},{"content",input}}
        })}
    };
    // Sin historial: cada escena es un pedido nuevo e independiente.
    pending=llm.chat(request); // Devuelve enseguida; HTTP trabaja en otro hilo.
    if (pending) {
        sentInput=input;
        pendingSince=ofGetElapsedTimef();
        detail.clear();
        status="El modelo prepara la escena...";
    }
}

// ----------------------------------------------------------------------------
// 3. ACTUALIZAR. La física y la escritura continúan aunque HTTP esté esperando.
// ----------------------------------------------------------------------------
void ofApp::update() {
    llm.update();                       // ¿Llegó la respuesta? Si sí, dispara response().
    typing.update(ofGetElapsedTimef()); // Revela la respuesta de a poco.
    // La física recibe el mouse: mientras está presionado, atrae o repele cuerpos.
    stage.update(ofGetLastFrameTime(),ofVec2f(ofGetMouseX(),ofGetMouseY()),held);
    if (check && ofGetElapsedTimef()>190) { ofLogError()<<"Timeout de escena"; ofExit(1); }
}

// ----------------------------------------------------------------------------
// 4. VALIDAR ANTES DE ACTUAR. No destruir la escena por una respuesta inválida.
// Si parse() falla, lanza un error ANTES de tocar la escena: la anterior sigue.
// ----------------------------------------------------------------------------
void ofApp::apply(const std::string& value,bool sample) {
    const auto candidate=SceneSpec::parse(value); // Si falla, no tocar la escena.
    stage.build(candidate);                        // Datos válidos -> cuerpos físicos.
    scene=candidate;
    raw=value;
    recorded=sample;
    detail.clear();
    typing.start(scene.respuesta,ofGetElapsedTimef(),40); // 40 caracteres por segundo.
    if (sample) typing.finish();
    status=sample ? "ESCENA GRABADA / SIN INFERENCIA"
                  : "Escena generada: mantene el mouse presionado dentro del escenario";
    if (check && !sample) {
        ofLogNotice()<<"STAGE_CONTRACT_OK bodies="<<stage.bodyCount();
        ofExit(0);
    }
}

// Llegó la respuesta de Ollama. El texto del modelo está en body.message.content.
// Este es el punto para conectar la respuesta con OTRA obra (sonido, video, luces).
void ofApp::response(ofJson& event) {
    pending=false;
    // El addon entrega este evento desde llm.update(), en el hilo principal de OF.
    const std::string value=event.at("body").at("message").at("content").get<std::string>();
    try {
        apply(value,false);
        // Si alguien escribió otro borrador durante la espera, conservarlo.
        if (input==sentInput) input.clear();
        selectAll=false;
    } catch (const std::exception& e) {
        // Respuesta fuera del contrato: avisar, guardar el detalle y seguir con la escena anterior.
        detail=std::string(e.what())+"\nRespuesta recibida:\n"+value;
        status="Respuesta rechazada: se conserva la escena. Esc y E copian detalle";
        if (check) {
            ofLogError()<<detail;
            ofExit(1);
        }
    }
}

// Falla de red u Ollama (cerrado, modelo faltante, timeout). La obra sigue funcionando.
void ofApp::error(std::string& message) {
    pending=false; detail=message;
    status="No se pudo consultar Ollama. Esc y E copian detalle";
    if (check) { ofLogError()<<detail; ofExit(1); }
}

// ----------------------------------------------------------------------------
// 5. DIBUJAR. El modelo no dibuja ni ejecuta C++: StagePhysics interpreta sus datos.
// ----------------------------------------------------------------------------
void ofApp::draw() {
    ofBackground(13,22,39); ofSetColor(233,240,255);
    text.draw("UNA / INFORMATICA APLICADA 2",24,29);
    text.draw("LLM STAGE / palabras que habitan una escena fisica",24,65);
    auto button=[this](const ofRectangle& rect,const std::string& label) {
        ofSetColor(35,55,83); ofDrawRectRounded(rect,6);
        ofSetColor(233,240,255); text.draw(label,rect.x+12,rect.y+20);
    };
    button(resetButton,"REARMAR ESCENA"); button(jsonButton,"JSON"); button(sampleButton,"EJEMPLO GRABADO");

    // EL ESCENARIO: cuerpos, plataformas y colores. Ver StagePhysics::draw().
    stage.draw(text);

    // Panel derecho: la respuesta escrita o el JSON crudo (botón JSON).
    ofSetColor(225,235,253);
    text.draw(recorded?"FUENTE: GRABADA / SIN INFERENCIA":"FUENTE: LLM / "+model,778,115);
    text.draw(showJson?"DATOS DE LA ESCENA":"RESPUESTA DEL MODELO",778,154);
    const auto cursor=typing.active()&&std::fmod(ofGetElapsedTimef(),1.f)<.5f?"|":"";
    text.draw(wrap(showJson?raw:typing.visible()+cursor,57,22),778,185);

    // "Pensando..." = esperando HTTP. "Escribiendo..." = ya llegó, se revela de a poco.
    if (pending||typing.active()) {
        ofSetColor(28,43,66); ofDrawRectRounded(778,553,478,58,10);
        dots(798,576,ofGetElapsedTimef()); ofSetColor(233,240,255);
        text.draw(pending?"Pensando...":"Escribiendo...",842,578);
        text.draw(pending?ofToString(int(ofGetElapsedTimef()-pendingSince))+" s / esperando respuesta":"Enter: ver respuesta completa",842,599);
    }

    // Resumen de lo que eligió el modelo, editor y botón de envío.
    ofSetColor(160,190,225);
    text.draw("Mouse presionado: "+scene.interaccion+" | forma: "+scene.forma+" | cuerpos: "+ofToString(scene.cantidad),24,640);
    ofSetColor(editing?ofColor(42,65,100):ofColor(28,43,66)); ofDrawRectRounded(editor,10);
    ofSetColor(selectAll?ofColor(255,209,139):ofColor(234,241,255)); text.draw(wrap(input+(editing?"|":""),110,3),38,684);
    ofSetColor(pending?ofColor(51,67,90):ofColor(47,91,153)); ofDrawRectRounded(sendButton,10);
    ofSetColor(255); text.draw(pending?"PENSANDO...":typing.active()?"VER COMPLETA":"CREAR ESCENA / ENTER",1010,699);
    ofSetColor(211,222,239); text.draw(wrap(status,148,1),24,753);
    text.draw("Click: editar | Esc: salir del editor | E: copiar diagnostico | JSON: inspeccionar salida",24,780);
    if (!preview.empty()&&ofGetFrameNum()==60) { ofSaveScreen(preview); ofExit(); }
}

// ----------------------------------------------------------------------------
// INTERACCIÓN: mouse y teclado. No es parte de la conexión al modelo:
// es lo que cada obra reemplaza por su propia entrada (sensor, cámara, sonido).
// ----------------------------------------------------------------------------
void ofApp::mousePressed(int x,int y,int button) {
    if (jsonButton.inside(x,y)) { showJson=!showJson; return; }
    if (!pending&&resetButton.inside(x,y)) {
        // REARMAR: mismos datos, cuerpos en su posición inicial. No consulta al modelo.
        stage.build(scene); status="Mismos datos, simulacion reiniciada";
        return;
    }
    if (!pending&&sampleButton.inside(x,y)) {
        try { apply(ofBufferFromFile("recorded.json").getText(),true); }
        catch (const std::exception& e) { detail=e.what(); status="Error en recorded.json"; }
        return;
    }
    editing=editor.inside(x,y); selectAll=false;
    // held = "el mouse está apretado dentro del escenario": activa la fuerza en update().
    held=button==OF_MOUSE_BUTTON_LEFT&&stageArea.inside(x,y);
    if (sendButton.inside(x,y)) send();
}
void ofApp::mouseReleased(int x,int y,int button) { held=false; }

// Entrada de texto local. No forma parte del addon ni del prompt del modelo.
void ofApp::keyPressed(int key) {
    if (check||!preview.empty()) return;
    if (key==OF_KEY_RETURN) { send(); return; }
    if (!editing) {
        if (key=='e'||key=='E') ofSetClipboardString(status+"\n"+detail+"\n"+raw); // Diagnóstico.
        return;
    }
    if (key==OF_KEY_ESC) { editing=false; selectAll=false; return; }
    const bool command=ofGetKeyPressed(OF_KEY_COMMAND)||ofGetKeyPressed(OF_KEY_CONTROL);
    if (command&&(key=='a'||key=='A')) { selectAll=true; return; }
    if (command&&(key=='v'||key=='V')) {
        // Pegar: descartamos caracteres de control y limitamos el largo.
        if (selectAll) input.clear();
        selectAll=false;
        for (auto cp:ofUTF8Iterator(ofGetClipboardString())) {
            if (input.size()>950) break;
            if (cp>=32) ofUTF8Append(input,cp);
        }
        return;
    }
    if (key==OF_KEY_BACKSPACE||key==OF_KEY_DEL) {
        // Borrar un CARÁCTER completo: una "ñ" ocupa 2 bytes en UTF-8.
        if (selectAll) input.clear();
        else if (!input.empty()) {
            size_t i=input.size()-1;
            while (i>0&&(static_cast<unsigned char>(input[i])&0xc0)==0x80) --i;
            input.erase(i);
        }
        selectAll=false;
        return;
    }
    if (!command&&key>=32&&key<0x110000&&!(key>=0xe00&&key<=0xfff)) {
        if (selectAll) input.clear();
        selectAll=false;
        if (input.size()<950) ofUTF8Append(input,key);
    }
}

// ----------------------------------------------------------------------------
// 6. CERRAR. Cuerpos antes que mundo; worker antes que listeners/aplicación.
// ----------------------------------------------------------------------------
void ofApp::exit() {
    llm.close();
    ofRemoveListener(llm.response,this,&ofApp::response);
    ofRemoveListener(llm.error,this,&ofApp::error);
    stage.clear();
}
