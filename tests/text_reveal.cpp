#include "ofxTextReveal.h"
#include <cassert>
#include <iostream>
int main() {
    ofxTextReveal text;
    assert(!text.active() && text.visible().empty());
    text.start(u8"añ🙂!",10,2);
    assert(text.active() && text.visible().empty());
    text.update(9); assert(text.visible().empty());
    text.update(10.5); assert(text.visible()=="a");
    text.update(11); assert(text.visible()==u8"añ");
    text.update(11.5); assert(text.visible()==u8"añ🙂");
    text.update(10); assert(text.visible()==u8"añ🙂");
    text.update(12); assert(text.visible()==u8"añ🙂!" && !text.active());
    text.start("nuevo",20); assert(text.visible().empty());
    text.finish(); text.update(20); assert(text.visible()=="nuevo" && !text.active());
    text.clear(); assert(!text.active() && text.visible().empty());
    text.start("",30); text.update(100); assert(!text.active());
    text.start("abc",0,0);text.update(1);assert(text.visible()=="a");
    text.start("abc",0,9999);text.update(1);assert(text.visible()=="abc");
    std::cout<<"Text reveal: UTF-8 boundaries, elapsed time, finish, restart and clear OK\n";
}
