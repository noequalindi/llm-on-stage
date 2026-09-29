#pragma once
#include "ofMain.h"
#include "ofxBox2d.h"
#include "SceneSpec.h"
#include "TextStyle.h"

// ESCENOGRAFÍA: todo el dibujo y la física están aquí, separados de HTTP.
// Modificar build() para cambiar los objetos; draw() para cambiar su apariencia.
//
// Box2D es un motor de física 2D: nosotros creamos cuerpos con forma, masa,
// rebote y fricción, y él calcula caídas y choques. Analogía: el LLM es el
// escenógrafo que escribe "cajas pesadas que rebotan poco"; Box2D es la
// gravedad del teatro; draw() decide el vestuario de cada actor.
//
// Qué dato del modelo usa cada parte:
//   gravedad    -> world.setGravity()      (negativa sube, 0 flota, positiva cae)
//   rebote      -> setPhysics(densidad, REBOTE, fricción)
//   cantidad    -> cuántos cuerpos crea build()
//   forma       -> ofxBox2dCircle o ofxBox2dRect
//   palabras    -> etiqueta de cada cuerpo (se repiten si hay más cuerpos)
//   interaccion -> signo de la fuerza del mouse en update()
//   paleta      -> colores en draw() ("fuego": rojo, el fondo late y tiembla)
class StagePhysics {
public:
    void setup(const ofRectangle& area) {
        bounds=area; world.init(); world.setFPS(60);
        world.createBounds(area.x,area.y,area.width,area.height);
        // Plataformas estáticas: densidad cero. Son parte del escenario de la obra.
        for (int i=0;i<2;++i) {
            auto p=std::make_shared<ofxBox2dRect>();
            p->setPhysics(0,.3f,.4f);
            p->setup(world.getWorld(),area.x+area.width*(i ? .72f : .28f),area.y+area.height*(i ? .76f : .62f),180,14,i ? -12 : 12);
            platforms.push_back(p);
        }
    }
    // Crear los cuerpos a partir de datos YA validados por SceneSpec::parse().
    void build(const SceneSpec& spec) {
        // Los shared_ptr destruyen los cuerpos anteriores antes de crear nuevos.
        actors.clear(); current=spec; accumulator=0;
        world.setGravity(0,spec.gravedad);
        for (int i=0;i<spec.cantidad;++i) {
            Actor a; a.label=spec.palabras[i%spec.palabras.size()];
            // Posición inicial en grilla de 6 columnas.
            const float x=bounds.x+70+(i%6)*115;
            const float y=bounds.y+78+(i/6)*74;
            if (spec.forma=="circulos") {
                auto shape=std::make_shared<ofxBox2dCircle>();
                shape->setPhysics(1,spec.rebote,.25f); shape->setup(world.getWorld(),x,y,34);
                a.body=shape;
            } else {
                auto shape=std::make_shared<ofxBox2dRect>();
                shape->setPhysics(1,spec.rebote,.25f); shape->setup(world.getWorld(),x,y,92,48);
                a.body=shape;
            }
            // Damping = "rozamiento con el aire": frena de a poco el movimiento y el giro.
            a.body->setLinearDamping(.18f); a.body->setAngularDamping(.3f);
            a.body->setVelocity((i%3-1)*1.5f,-1.f); actors.push_back(a);
        }
    }
    void update(float delta,const ofVec2f& mouse,bool held) {
        // Paso fijo: la simulación no cambia de velocidad con cada fluctuación de FPS.
        accumulator+=ofClamp(delta,0.f,.1f);
        while (accumulator>=1.f/60.f) {
            // FUERZA DEL MOUSE: con el botón apretado, cada cuerpo a menos de 280 px
            // recibe un empujón hacia el mouse (atraer) o en contra (repeler).
            // Más cerca = más fuerza. Ejercicio: cambiar 280 (radio) o 20 (intensidad).
            if (held && bounds.inside(mouse)) {
                for (auto& a:actors) {
                    ofVec2f direction=mouse-a.body->getPosition();
                    const float distance=direction.length();
                    if (distance<1 || distance>280) continue;
                    direction/=distance;
                    const float sign=current.interaccion=="atraer" ? 1.f : -1.f;
                    // Fuerza proporcional a masa: cuerpos de distintos tamaños responden parecido.
                    const float force=a.body->body->GetMass()*20.f*(1.f-distance/280.f)*sign;
                    a.body->body->ApplyForceToCenter(b2Vec2(direction.x*force,direction.y*force),true);
                }
            }
            world.update(); accumulator-=1.f/60.f;
        }
    }
    // APARIENCIA: el lugar más fácil para empezar a modificar la obra.
    void draw(const TextStyle& text) {
        ofPushStyle();
        // paleta -> colores. "fuego" es el escenario rojo del enojo.
        const bool fuego=current.paleta=="fuego";
        const ofColor ink=current.paleta=="sol" ? ofColor(255,211,126) : current.paleta=="noche" ? ofColor(208,182,255) : fuego ? ofColor(255,120,95) : ofColor(144,215,255);
        const ofColor bg=current.paleta=="sol" ? ofColor(66,37,42) : current.paleta=="noche" ? ofColor(30,24,58) : fuego ? ofColor(78,14,18) : ofColor(17,40,73);
        ofSetColor(bg); ofDrawRectRounded(bounds,16);
        // Con "fuego" el fondo late y tiembla, como un pulso acelerado.
        // Es una decisión de la obra: el modelo solo eligió la palabra "fuego".
        const float t=ofGetElapsedTimef();
        const float shake=fuego ? 2.5f : 0.f;
        // Fondo de escenografía. Se puede reemplazar por texturas, luces o video propio.
        ofSetColor(ink,fuego ? 35+20*std::sin(t*6) : 25);
        ofDrawCircle(bounds.getRight()-105,bounds.y+96,current.paleta=="noche" ? 50 : fuego ? 75+6*std::sin(t*6) : 75);
        for (int i=0;i<7;++i) {
            const float dy=shake*std::sin(t*23+i*1.7f);
            ofDrawLine(bounds.x+18,bounds.y+80+i*64+dy,bounds.getRight()-18,bounds.y+80+i*64-dy);
        }
        ofSetColor(ink,100); for (auto& p:platforms) p->draw();
        for (auto& a:actors) {
            // Dibujar desde la transformación física; así podemos sustituir la forma
            // por una imagen sin cambiar las colisiones. Evitamos la línea de debug
            // que dibuja ofxBox2dCircle sobre la palabra.
            ofPushMatrix();
            ofTranslate(a.body->getPosition());
            ofRotateDeg(a.body->getRotation());
            ofSetColor(ink,215);
            if (current.forma=="circulos") ofDrawCircle(0,0,34);
            else ofDrawRectRounded(-46,-24,92,48,5);
            const auto length=std::count_if(a.label.begin(),a.label.end(),[](unsigned char c){return (c&0xc0)!=0x80;});
            ofSetColor(bg); text.drawSmall(a.label,-3.2f*length,3.2f); // Centrada: 6.4 px por carácter.
            ofPopMatrix();
        }
        ofSetColor(ink); text.draw(current.titulo,bounds.x+18,bounds.y+25);
        ofPopStyle();
    }
    void clear() { actors.clear(); platforms.clear(); } // Destruir formas antes del mundo.
    int bodyCount() const { return static_cast<int>(actors.size()); }
private:
    // El mundo se declara antes que los cuerpos para que se destruya después.
    ofxBox2d world;
    struct Actor { std::shared_ptr<ofxBox2dBaseShape> body; std::string label; };
    std::vector<Actor> actors;
    std::vector<std::shared_ptr<ofxBox2dRect>> platforms;
    ofRectangle bounds;
    SceneSpec current;
    float accumulator=0;
};
