#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// Utilidad opcional de presentación. No consulta al modelo ni hace streaming.
// Recibe un texto UTF-8 completo y expone un prefijo cada vez más largo.
// Separar este estado del historial evita enviar respuestas cortadas al LLM.
class ofxTextReveal {
public:
    void start(const std::string& text,double now,double charactersPerSecond=40) {
        full=text; ends.clear(); count=0; started=now;
        speed=std::clamp(charactersPerSecond,1.0,240.0);
        // Guardamos límites de caracteres UTF-8: nunca cortar en medio de una ñ,
        // vocal acentuada o emoji. Los emoji compuestos pueden aparecer por partes.
        for (std::size_t i=0;i<full.size();++i)
            if (i+1==full.size() || (static_cast<unsigned char>(full[i+1])&0xc0)!=0x80)
                ends.push_back(i+1);
    }
    void update(double now) {
        const double shown=std::min(static_cast<double>(ends.size()),std::max(0.0,(now-started)*speed));
        count=std::max(count,static_cast<std::size_t>(shown));
    }
    void finish() { count=ends.size(); }
    void clear() { full.clear(); ends.clear(); count=0; }
    bool active() const { return count<ends.size(); }
    std::string visible() const { return count ? full.substr(0,ends[count-1]) : std::string{}; }
private:
    std::string full;
    std::vector<std::size_t> ends;
    std::size_t count=0;
    double started=0,speed=40;
};
