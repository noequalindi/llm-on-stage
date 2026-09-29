#include "ofxLocalLLM.h"
#include <curl/curl.h>
#include <limits>
#include <stdexcept>

ofxLocalLLM::ofxLocalLLM() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        throw std::runtime_error("No se pudo iniciar HTTP");
}
ofxLocalLLM::~ofxLocalLLM() { close(); curl_global_cleanup(); }
void ofxLocalLLM::setup(const std::string& url) {
    close();
    if (url.rfind("http://",0)!=0 && url.rfind("https://",0)!=0)
        throw std::invalid_argument("URL de Ollama invalida");
    baseUrl=url;
    while (!baseUrl.empty() && baseUrl.back()=='/') baseUrl.pop_back();
    active=true;
}
bool ofxLocalLLM::chat(const ofJson& request) {
    if (!request.is_object()) return false;
    auto body=request;
    body["stream"]=false; // This introductory client receives one complete reply.
    return start("/api/chat","chat",&body);
}
bool ofxLocalLLM::listModels() { return start("/api/tags","models",nullptr); }

bool ofxLocalLLM::start(const std::string& path,const std::string& kind,const ofJson* body) {
    if (!active || inFlight) return false;
    const auto url=baseUrl+path, payload=body ? body->dump() : std::string{};
    inFlight=true; stopping=false;
    try {
        worker=std::thread([this,url,payload,kind] {
            ofJson event;
            std::string problem;
            CURL* curl=nullptr;
            curl_slist* headers=nullptr;
            try {
                curl=curl_easy_init();
                if (!curl) throw std::runtime_error("No se pudo preparar HTTP");
                struct State { std::string text; std::atomic<bool>* stop; bool tooLarge=false; } state{{},&stopping};
                curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
                curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
                curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);
                curl_easy_setopt(curl,CURLOPT_TIMEOUT,kind=="chat" ? 180L : 10L);
                curl_easy_setopt(curl,CURLOPT_NOPROXY,"127.0.0.1,localhost,::1");
                curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
                curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&stopping);
                curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,+[](void* p,curl_off_t,curl_off_t,curl_off_t,curl_off_t)->int {
                    return static_cast<std::atomic<bool>*>(p)->load() ? 1 : 0;
                });
                curl_easy_setopt(curl,CURLOPT_WRITEDATA,&state);
                curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,+[](char* bytes,size_t size,size_t count,void* ptr)->size_t {
                    auto& s=*static_cast<State*>(ptr);
                    if (s.stop->load() || (count && size>std::numeric_limits<size_t>::max()/count)) return 0;
                    const auto n=size*count;
                    if (n>1024*1024-s.text.size()) { s.tooLarge=true; return 0; }
                    try { s.text.append(bytes,n); return n; } catch (...) { return 0; }
                });
                if (!payload.empty()) {
                    headers=curl_slist_append(nullptr,"Content-Type: application/json");
                    if (!headers) throw std::runtime_error("No se pudo preparar HTTP");
                    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
                    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,payload.c_str());
                    curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,static_cast<long>(payload.size()));
                }
                const auto code=curl_easy_perform(curl);
                long status=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
                if (state.tooLarge) throw std::runtime_error("Respuesta de Ollama demasiado grande");
                if (code!=CURLE_OK) throw std::runtime_error(std::string("Ollama: ")+curl_easy_strerror(code)+". Abrir Ollama y revisar la URL.");
                auto data=ofJson::parse(state.text,nullptr,false);
                if (status<200 || status>=300) {
                    std::string detail=state.text.substr(0,512);
                    if (data.is_object() && data.contains("error") && data["error"].is_string()) detail=data["error"].get<std::string>().substr(0,512);
                    throw std::runtime_error("Ollama HTTP "+std::to_string(status)+": "+detail);
                }
                if (!data.is_object()) throw std::runtime_error("Ollama no devolvio un objeto JSON");
                if (data.contains("error")) throw std::runtime_error("Ollama: "+data["error"].dump());
                if (kind=="chat") {
                    if (!data.contains("done") || data["done"]!=true || !data.contains("message") ||
                        !data["message"].is_object() || !data["message"].contains("content") ||
                        !data["message"]["content"].is_string()) throw std::runtime_error("Respuesta de chat incompleta");
                    if (data.value("done_reason",std::string{})=="length") throw std::runtime_error("Limite de tokens alcanzado; acortar pedido o aumentar num_predict");
                    if (data["message"]["content"].get<std::string>().find_first_not_of(" \t\r\n")==std::string::npos)
                        throw std::runtime_error("El modelo devolvio texto vacio");
                } else if (!data.contains("models") || !data["models"].is_array()) throw std::runtime_error("Catalogo de Ollama invalido");
                event={{"request",kind},{"body",data}};
            } catch (const std::exception& exc) { problem=exc.what(); }
            catch (...) { problem="Error inesperado de HTTP"; }
            if (headers) curl_slist_free_all(headers);
            if (curl) curl_easy_cleanup(curl);
            std::lock_guard<std::mutex> lock(mutex);
            result=std::move(event); failure=std::move(problem); completed=true;
        });
    } catch (...) { inFlight=false; throw; }
    return true;
}
void ofxLocalLLM::update() {
    ofJson event; std::string problem;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!completed) return;
        event=std::move(result); problem=std::move(failure); completed=false;
    }
    if (worker.joinable()) worker.join();
    inFlight=false; // Listeners may now send the next request.
    if (!problem.empty()) ofNotifyEvent(error,problem,this);
    else ofNotifyEvent(response,event,this);
}
void ofxLocalLLM::close() {
    stopping=true;
    if (worker.joinable()) worker.join(); // curl progress callback interrupts transfer.
    std::lock_guard<std::mutex> lock(mutex);
    active=false; inFlight=false; completed=false; result=nullptr; failure.clear();
}
