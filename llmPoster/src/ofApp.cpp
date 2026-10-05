#include "ofApp.h"

// ============================================================================
// MAPA DEL EJEMPLO (leer en clase en este orden)
//
//   texto del alumno
//     -> 2. send()      arma el pedido JSON (system + historial + user)
//     -> ofxLocalAI    POST HTTP a Ollama (http://127.0.0.1:11434/api/chat)
//                       en otro hilo: la ventana nunca se congela
//     -> 3. update()    llm.update() entrega la respuesta al hilo de OF
//     -> 4. response()  valida el JSON con el contrato (PosterSpec / ChatSession)
//     -> 5. draw()      drawPoster() convierte datos en movimiento (PosterView.h)
//
// Ollama ejecuta Qwen. OF NO ejecuta Python ni el modelo: solo pregunta por HTTP.
// El modelo propone palabras ("calma", "sol"); NUESTRO código decide qué significan.
// ============================================================================

// Interfaz del ejemplo. Coordenadas y helpers de dibujo: se pueden reemplazar.
namespace {
const ofRectangle editor(28,660,850,70),sendButton(900,660,250,70);
const ofRectangle chatButton(702,43,96,30),posterButton(808,43,96,30),resetButton(914,43,126,30),jsonButton(1050,43,100,30);

// Tres puntos que "respiran" mientras esperamos. Solo es decoración:
// no mide progreso, porque no sabemos cuánto va a tardar el modelo.
void activityDots(float x,float y,double seconds) {
    ofPushStyle(); ofFill();
    for (int i=0;i<3;++i) {
        const float pulse=.5f+.5f*std::sin(seconds*5.0-i*.9);
        ofSetColor(142,214,255,100+155*pulse);
        ofDrawCircle(x+i*13,y-3*pulse,3+1.2f*pulse);
    }
    ofPopStyle();
}

// Corta un texto en renglones de N columnas. Recorre CARACTERES (UTF-8), no bytes:
// así una "ñ" o una "á" cuentan como una letra y nunca se parten por la mitad.
std::vector<std::string> lines(const std::string& text,int columns) {
    std::vector<std::string> result(1); int column=0;
    for (auto cp:ofUTF8Iterator(text)) {
        if (cp=='\r') continue;
        if (cp=='\n' || column>=columns) {
            result.emplace_back(); column=0; if (cp=='\n') continue;
        }
        ofUTF8Append(result.back(),cp=='\t' ? ' ' : cp); ++column;
    }
    return result;
}

// Igual que lines(), pero devuelve un solo texto y corta con "..." si hay demasiados renglones.
std::string wrap(const std::string& text,int columns,int maxLines) {
    std::string output; int column=0,lines=1;
    for (auto cp:ofUTF8Iterator(text)) {
        if (cp=='\n' || column>=columns) {
            if (++lines>maxLines) return output+"...";
            output+='\n'; column=0; if (cp=='\n') continue;
        }
        ofUTF8Append(output,cp); ++column;
    }
    return output;
}
}

// ----------------------------------------------------------------------------
// 1. PREPARAR: datos del ejemplo, URL/modelo y eventos de red.
// El addon no instala Ollama. La URL y el modelo están en bin/data/connection.json.
// ----------------------------------------------------------------------------
void ofApp::setup() {
    chatMode=!check;
    ofSetFrameRate(60); ofSetVerticalSync(true); ofSetEscapeQuitsApp(false);
    ofSetWindowTitle("26 / LLM local + openFrameworks / Afiche vivo");
    text.setup(); // Fuente con ñ y tildes: antes de cualquier draw().
    try {
        // Arrancamos con un afiche GRABADO (escrito a mano): se puede diseñar
        // y probar el dibujo sin esperar al modelo.
        apply(ofBufferFromFile("recorded.json").getText(),true);
        if (!preview.empty()) return;

        // Configuración fuera del código: cambiar de modelo no obliga a recompilar.
        auto config=ofLoadJson("connection.json");
        url=config.at("ollama_url").get<std::string>(); modelId=config.at("model").get<std::string>();

        // Los prompts son archivos de texto: se editan y se prueban reiniciando la app.
        //   system-prompt.txt -> modo AFICHE (5 campos)
        //   chat-prompt.txt   -> modo CHAT (respuesta + visual)
        systemPrompt=ofBufferFromFile("system-prompt.txt").getText();
        if (systemPrompt.empty()) throw std::runtime_error("Falta system-prompt.txt");
        chatPrompt=ofBufferFromFile("chat-prompt.txt").getText();
        if (chatPrompt.empty()) throw std::runtime_error("Falta chat-prompt.txt: copiar bin/data actualizado");
        loadChatConfig(); // memoria y num_ctx: se cambian sin recompilar.

        // EVENTOS: no esperamos la respuesta "parados". Le decimos al addon
        // "cuando llegue algo, llamá a response() o a error()". Como un timbre.
        ofAddListener(llm.response,this,&ofApp::response); ofAddListener(llm.error,this,&ofApp::error);
        llm.setup(url); // HTTP directo a Ollama, sin iniciar un proceso Python.
        initialized=true;
        if (check) send(); else connect();
    } catch (const std::exception& exc) { detail=exc.what(); status="Revisar archivos en bin/data. E copia el error."; if (check) ofExit(1); }
}

// ----------------------------------------------------------------------------
// 2. ENVIAR: aquí los alumnos deciden QUÉ mandar y CUÁNDO consultarlo.
// Llamamos una vez por interacción (Enter o botón), NUNCA una vez por frame:
// serían 60 consultas por segundo a un modelo que tarda segundos en responder.
// ----------------------------------------------------------------------------
void ofApp::send() {
    if (typing.active()) { typing.finish(); return; } // Enter también permite ver todo.
    if (!initialized || busy()) return;
    if (input.find_first_not_of(" \r\n\t")==std::string::npos) { status="Escribi un texto primero"; return; }

    // El pedido es un JSON con el formato de la API de Ollama (/api/chat):
    //   model       qué modelo usar (de connection.json)
    //   stream      false = recibir la respuesta completa de una vez
    //   format      "json" = Ollama obliga al modelo a devolver JSON bien formado
    //   temperature cuánto azar al elegir cada palabra: 0 = siempre lo más probable.
    //               AFICHE usa 0 (consistencia); CHAT usa .4 (algo de variedad).
    //   num_predict tope de tokens de la respuesta (si se corta, llega un error)
    //   num_ctx     ventana de contexto en tokens: instrucciones + historial + mensaje
    //               + respuesta tienen que entrar acá. Si no entran, Ollama descarta
    //               lo más viejo sin avisar. Se configura en chat-config.json.
    //   messages    la conversación: system = instrucciones, user = pedido.
    //               En CHAT, ChatSession agrega el historial (la "memoria").
    ofJson request={{"model",modelId},{"stream",false},{"format","json"},
        {"options",{{"temperature",chatMode ? .4 : 0.},{"num_predict",chatMode ? 768 : 256},{"num_ctx",numCtx}}},
        {"messages",chatMode ? conversation.messages(chatPrompt,input) :
            ofJson::array({{{"role","system"},{"content",systemPrompt}},{{"role","user"},{"content",input}}})}};
    detail.clear();

    // chat() devuelve ENSEGUIDA: el HTTP sigue en otro hilo. true = pedido enviado;
    // false = ya había uno en curso (el addon no hace fila, atiende de a uno).
    pending=llm.chat(request);
    if (pending) { sentInput=input; pendingSince=ofGetElapsedTimef(); chatScroll=0; }
    status=pending ? "Generando con Qwen... el dibujo sigue funcionando" : "Ollama ya tiene una consulta pendiente";
}

// ----------------------------------------------------------------------------
// 3. ACTUALIZAR: recibir eventos y avanzar la animación sin bloquear el dibujo.
// ----------------------------------------------------------------------------
void ofApp::update() {
    // Revisa si el hilo de red terminó. Si terminó, dispara response() o error()
    // acá, en el hilo principal: por eso es seguro tocar la escena desde ellos.
    llm.update();
    typing.update(ofGetElapsedTimef()); // Animación local, independiente de los FPS.
    if (check && ofGetElapsedTimef()>190) { ofLogError()<<"Timeout de comprobacion"; ofExit(1); }
}

// VALIDAR ANTES DE USAR (modo AFICHE y afiche grabado).
// Si el JSON no cumple el contrato, parse() lanza un error y el afiche
// anterior queda intacto: una mala respuesta nunca rompe la obra.
void ofApp::apply(const std::string& value,bool sample) {
    raw=value;
    try {
        const auto candidate=PosterSpec::parse(raw); // Validar todo antes de reemplazar la escena.
        poster=candidate; recorded=sample; detail.clear();
        status=sample ? "RESPUESTA GRABADA / sin inferencia" : "JSON valido / afiche actualizado por el LLM";
        if (check && !sample) { ofLogNotice()<<"POSTER_CONTRACT_OK"; ofExit(0); }
    } catch (const std::exception& exc) {
        detail=exc.what(); status="JSON rechazado. Se conserva el ultimo afiche. E copia el detalle.";
        if (check) { ofLogError()<<detail; ofExit(1); }
    }
}

// ----------------------------------------------------------------------------
// 4. RECIBIR Y VALIDAR: message.content contiene el texto generado por Ollama.
// Este es el punto de entrada para conectar una respuesta con otra experiencia.
// ----------------------------------------------------------------------------
void ofApp::response(ofJson& event) {
    pending=false;
    const auto& data=event.at("body"); // La respuesta de Ollama, tal cual llegó.

    // Respuesta a la tecla L (GET /api/tags): ¿está instalado nuestro modelo?
    if (event.at("request")=="models") {
        bool found=false;
        for (const auto& model:data.at("models"))
            if (model.is_object() && model.value("name",std::string{})==modelId) found=true;
        status=found ? "Ollama conectado / Qwen disponible. Escribi y pulsa Interpretar" : "Ollama conectado, falta el modelo. Repetir el instalador";
        return;
    }

    // El modelo SIEMPRE devuelve texto. Nosotros le pedimos que ese texto sea JSON.
    const auto content=data.at("message").at("content").get<std::string>();
    // Ollama informa cuántos tokens ocupó el pedido completo (system + historial + mensaje).
    promptTokens=data.value("prompt_eval_count",0);
    if (!chatMode) { apply(content,false); return; }
    try {
        // accept() valida {respuesta, visual} y recién entonces guarda el turno.
        poster=conversation.accept(sentInput,content);
        // El historial ya tiene la respuesta COMPLETA. Solo su vista se anima
        // ("Escribiendo..."): es un efecto local, el modelo ya terminó.
        // Cambiar 40 por otro valor para ajustar los caracteres por segundo.
        typing.start(conversation.turns.back().answer,ofGetElapsedTimef(),40);
        raw=content; recorded=false; detail.clear(); chatScroll=0;
        if (input==sentInput) input.clear(); // Conservar un nuevo borrador escrito durante la espera.
        selectAll=false; editing=true;
        status=promptTokens>numCtx*9/10 ? "Contexto casi lleno: C borra el contexto o subi num_ctx en chat-config.json"
                                        : "Respuesta recibida / el tono anima el afiche";
    } catch (const std::exception& exc) {
        // Rechazada: no se agrega al historial ni cambia el afiche.
        detail=std::string(exc.what())+"\nRespuesta recibida:\n"+content;
        status="Respuesta rechazada: se conservan conversacion y afiche. E copia detalle";
    }
}

// Falla de red o de Ollama (cerrado, modelo faltante, timeout).
// Qué hacer si falla también es una decisión de diseño: acá, avisar y seguir.
void ofApp::error(std::string& message) {
    pending=false; detail=message;
    status="No se pudo consultar Ollama. E copia detalle; comprobar Ollama y modelo";
    if (check) { ofLogError()<<detail; ofExit(1); }
}

// CONTEXTO parametrizable: bin/data/chat-config.json (opcional; sin archivo, valores por defecto).
// Ejercicio: probar memoria 0 (cada mensaje es independiente) o 1, y comparar.
void ofApp::loadChatConfig() {
    const auto file=ofToDataPath("chat-config.json");
    if (!ofFile::doesFileExist(file)) return;
    const auto config=ofLoadJson(file);
    conversation.setLimit(config.value("memoria",4));                 // 0 a 20 intercambios
    numCtx=std::clamp(config.value("num_ctx",4096),512,32768);         // Qwen2.5 1.5B admite hasta 32768
}

// Tecla L: pregunta a Ollama qué modelos tiene (GET /api/tags). No usa el modelo.
void ofApp::connect() {
    if (!initialized || busy()) return;
    detail.clear(); sentInput.clear(); pending=llm.listModels();
    if (pending) pendingSince=ofGetElapsedTimef();
    status=pending ? "Conectando directamente con Ollama..." : "Hay una consulta pendiente";
}

// ----------------------------------------------------------------------------
// 5. REPRESENTAR: drawPoster() y esta interfaz son decisiones de nuestra obra.
// Se pueden cambiar sin reescribir el transporte HTTP de ofxLocalAI.
// draw() corre 60 veces por segundo, esté o no esperando al modelo.
// ----------------------------------------------------------------------------
void ofApp::draw() {
    ofBackground(13,22,39); ofSetColor(233,240,255);
    text.draw("UNA / INFORMATICA APLICADA 2                 26 / LLM LOCAL + OPENFRAMEWORKS",28,30);
    text.draw(chatMode ? "CHAT VIVO / conversacion + tono + movimiento" : "AFICHE VIVO / del texto al JSON, del JSON a la imagen",28,57);
    auto button=[this](const ofRectangle& rect,const std::string& label,bool selected) {
        ofSetColor(selected ? ofColor(47,91,153) : ofColor(28,43,66)); ofDrawRectRounded(rect,6);
        ofSetColor(233,240,255); text.draw(label,rect.x+10,rect.y+20);
    };
    button(chatButton,"CHAT",chatMode); button(posterButton,"AFICHE",!chatMode);
    button(resetButton,"NUEVO CHAT",false); button(jsonButton,"JSON",showJson);

    // EL AFICHE: acá los datos validados se vuelven imagen. Ver PosterView.h.
    drawPoster(poster,{28,90,650,510},ofGetElapsedTimef(),text);

    // Panel derecho: el JSON crudo (qué dijo el modelo) o la conversación.
    // Mirar el JSON al lado del afiche muestra dónde termina el modelo y empieza el código.
    ofSetColor(225,235,253);
    text.draw(recorded ? "FUENTE: GRABADA / SIN INFERENCIA" : "FUENTE: LLM / "+modelId,702,115);
    if (!chatMode || showJson) {
        text.draw("ULTIMA SALIDA (JSON)",702,150);
        text.draw(wrap(raw,51,(pending || typing.active()) ? 22 : 27),702,176);
    } else {
        // Cuánto contexto usamos: intercambios guardados y tokens del último pedido.
        text.draw("CONVERSACION  memoria "+ofToString(conversation.turns.size())+"/"+ofToString(conversation.limit)+
                  "  tokens "+ofToString(promptTokens)+"/"+ofToString(numCtx),702,150);
        std::vector<std::string> transcript;
        auto append=[&](const std::string& value) {
            const auto wrapped=lines(value,51); transcript.insert(transcript.end(),wrapped.begin(),wrapped.end());
        };
        if (conversation.turns.empty()) append("Escribi una pregunta, pedi una historia o imagina una experiencia. La respuesta cambiara el tono del afiche.");
        for (const auto& turn:conversation.turns) {
            append("VOS: "+turn.user); append("");
            // La última respuesta se muestra de a poco; las anteriores, completas.
            const bool revealing=typing.active() && &turn==&conversation.turns.back();
            const auto cursor=revealing && std::fmod(ofGetElapsedTimef(),1.f)<.5f ? "|" : "";
            append("LLM: "+(revealing ? typing.visible() : turn.answer)+cursor); append("");
        }
        if (pending && !sentInput.empty()) { append("VOS: "+sentInput); append(""); }
        // Scroll con la rueda: mostramos solo los renglones que entran en el panel.
        const int visibleLines=(pending || typing.active()) ? 22 : 25;
        const int maxScroll=std::max(0,static_cast<int>(transcript.size())-visibleLines);
        chatScroll=ofClamp(chatScroll,0,maxScroll);
        const int first=maxScroll-chatScroll;
        for (int i=first;i<std::min(first+visibleLines,static_cast<int>(transcript.size()));++i)
            text.draw(transcript[i],702,176+(i-first)*16);
        ofSetColor(160,185,220); text.draw("Rueda: historial | JSON: ver datos del modelo",702,606);
    }

    // Indicadores de estado: "Pensando..." = esperando HTTP; "Escribiendo..." = ya llegó.
    if (pending) {
        const double elapsed=std::max(0.0,static_cast<double>(ofGetElapsedTimef())-pendingSince);
        ofSetColor(28,43,66); ofDrawRectRounded(702,536,448,54,10);
        activityDots(721,556,elapsed);
        ofSetColor(233,240,255);
        text.draw(sentInput.empty() ? "Conectando con Ollama..." : "Pensando...",764,556);
        ofSetColor(160,185,220);
        text.draw(ofToString(static_cast<int>(elapsed))+" s / "+
            (sentInput.empty() ? "comprobando conexion" : "preparando respuesta"),764,577);
    }
    if (!pending && typing.active()) {
        ofSetColor(28,43,66); ofDrawRectRounded(702,536,448,54,10);
        activityDots(721,556,ofGetElapsedTimef());
        ofSetColor(233,240,255); text.draw("Escribiendo...",764,556);
        ofSetColor(160,185,220); text.draw("Enter / VER COMPLETA para adelantar",764,577);
    }

    // Editor de texto y botón de envío.
    ofSetColor(225,235,253);
    text.draw("animo: "+poster.animo+" / ritmo: "+poster.ritmo+" / paleta: "+poster.paleta,28,627);
    ofSetColor(editing ? ofColor(42,65,100) : ofColor(28,43,66)); ofDrawRectRounded(editor,10);
    ofSetColor(selectAll ? ofColor(255,209,139) : ofColor(234,241,255)); text.draw(wrap(input+(editing ? "|" : ""),100,3),editor.x+14,editor.y+24);
    ofSetColor(busy() ? ofColor(51,67,90) : ofColor(47,91,153)); ofDrawRectRounded(sendButton,10);
    ofSetColor(255); text.draw(busy() ? (sentInput.empty() ? "CONECTANDO" : "PENSANDO") : typing.active() ? "VER COMPLETA" : chatMode ? "ENVIAR / ENTER" : "INTERPRETAR / ENTER",sendButton.x+24,sendButton.y+40);
    if (pending) activityDots(sendButton.x+182,sendButton.y+38,ofGetElapsedTimef()-pendingSince);
    ofSetColor(211,222,239); text.draw(wrap(status,140,1),28,752);
    text.draw("Click: editar | ESC: salir del editor | L: Ollama | R: grabada | E: detalle | C: borrar contexto | +/-: memoria",28,780);
    if (!preview.empty() && ofGetFrameNum()==4) { ofSaveScreen(preview); ofExit(); }
}

// ----------------------------------------------------------------------------
// INTERACCIÓN: mouse y teclado. Nada de esto forma parte de la conexión al modelo;
// es la parte que cada obra reemplaza por su propia forma de entrada.
// ----------------------------------------------------------------------------
void ofApp::mouseScrolled(int x,int y,float scrollX,float scrollY) {
    if (chatMode && !showJson && x>=702 && y>=150 && y<=610)
        chatScroll=std::max(0,chatScroll+static_cast<int>(scrollY*3));
}
void ofApp::mousePressed(int x,int y,int button) {
    if (jsonButton.inside(x,y)) { showJson=!showJson; return; }
    if (chatButton.inside(x,y) || posterButton.inside(x,y) || resetButton.inside(x,y)) {
        if (busy()) { status="Espera la respuesta antes de cambiar de modo o borrar el chat"; return; }
        if (resetButton.inside(x,y)) {
            // NUEVO CHAT: tirar el "cuaderno". El modelo no cambia; solo borramos
            // lo que le reenviábamos en cada consulta.
            typing.clear();
            conversation.clear(); sentInput.clear(); input.clear(); chatScroll=0; promptTokens=0;
            apply(ofBufferFromFile("recorded.json").getText(),true);
            status="Nueva conversacion / memoria borrada"; chatMode=true;
        } else { typing.finish(); chatMode=chatButton.inside(x,y); }
        showJson=false; editing=true; selectAll=!input.empty(); return;
    }
    editing=editor.inside(x,y); selectAll=false;
    if (sendButton.inside(x,y)) send();
}
// Entrada de teclado de la interfaz: no forma parte de la conexión al modelo.
void ofApp::keyPressed(int key) {
    if (check || !preview.empty()) return;
    if (key==OF_KEY_RETURN) { send(); return; }
    if (editing) {
        if (key==OF_KEY_ESC) { editing=false; selectAll=false; return; }
        const bool command=ofGetKeyPressed(OF_KEY_COMMAND) || ofGetKeyPressed(OF_KEY_CONTROL);
        if (command && (key=='a' || key=='A')) { selectAll=true; return; }
        if (command && (key=='v' || key=='V')) {
            // Pegar: descartamos caracteres de control y limitamos el largo.
            auto pasted=ofGetClipboardString(); std::string result=selectAll ? "" : input;
            for (auto cp:ofUTF8Iterator(pasted)) {
                if (result.size()>950) break;
                if (cp>=32) ofUTF8Append(result,cp);
            }
            input=result; selectAll=false; return;
        }
        if (key==OF_KEY_BACKSPACE || key==OF_KEY_DEL) {
            // Borrar un CARÁCTER completo: una "ñ" ocupa 2 bytes en UTF-8.
            if (selectAll) input.clear();
            else if (!input.empty()) { size_t i=input.size()-1; while(i>0 && (static_cast<unsigned char>(input[i])&0xc0)==0x80) --i; input.erase(i); }
            selectAll=false; return;
        }
        if (!command && key>=32 && key<0x110000 && !(key>=0xe00 && key<=0xfff)) {
            if (selectAll) input.clear(); selectAll=false;
            if (input.size()<950) ofUTF8Append(input,key);
        }
        return;
    }
    // Atajos (fuera del editor: primero Esc).
    if (key=='l' || key=='L') connect();
    // CONTEXTO en vivo: C borra la memoria (sin tocar el afiche); + / - cambian cuántos intercambios se reenvían.
    if ((key=='c' || key=='C') && !busy()) {
        conversation.clear(); promptTokens=0; chatScroll=0;
        status="Contexto borrado: el proximo mensaje llega sin historial";
    }
    if ((key=='+' || key=='=' || key=='-') && !busy()) {
        conversation.setLimit(conversation.limit+(key=='-' ? -1 : 1));
        status="Memoria: "+ofToString(conversation.limit)+" intercambios (0 = sin memoria). Para fijarlo, editar chat-config.json";
    }
    if ((key=='r' || key=='R') && !busy()) apply(ofBufferFromFile("recorded.json").getText(),true);
    if (key=='e' || key=='E') ofSetClipboardString(status+"\n"+detail+"\n"+raw); // Diagnóstico al portapapeles.
}

// ----------------------------------------------------------------------------
// 6. CERRAR: liberar el worker y los listeners antes de destruir la aplicación.
// ----------------------------------------------------------------------------
void ofApp::exit() {
    llm.close();
    ofRemoveListener(llm.response,this,&ofApp::response); ofRemoveListener(llm.error,this,&ofApp::error);
}
