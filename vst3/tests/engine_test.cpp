/* SPDX-License-Identifier: GPL-3.0-only */
#include "TextBank.h"
#include "VoiceEngine.h"
#include "VoiceParameters.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    voice::Line line;
    assert(voice::makeLine(u8"Привет, ёжик! Щука и любовь",4,line));
    assert(std::string(line.original.data()) == u8"Привет, ёжик! Щука и любовь");
    assert(std::string(line.sung.data()) == "privet, yozhik! shchuka i lyubov'");
    assert(voice::makeLine(u8"И Ы Е Э вечер эхо",4,line));
    assert(std::string(line.sung.data()) == "i y e eh vecher ehkho");
    assert(voice::makeLine(u8"Эхо",5,line));assert(std::string(line.sung.data())=="ekho");
    assert(voice::makeLine(u8"Über schöne Grüße",5,line));
    assert(std::string(line.sung.data()) == "ueber schoene gruesse");
    assert(voice::makeLine(u8"Été français",6,line));
    assert(std::string(line.sung.data()) == "eitei fra~sais");
    std::string longRussian;
    for (int i=0;i<128;++i) longRussian += u8"щ";
    assert(voice::makeLine(longRussian,4,line));
    assert(std::strlen(line.sung.data()) == 512);
    assert(!voice::makeLine(longRussian+u8"а",4,line));
    assert(!voice::makeLine("\xC0\x80",4,line));
    jv_engine* a = jv_create();
    jv_engine* b = jv_create();
    assert(a && b);
    jv_text(a,"privet mir",4);
    jv_text(b,"privet mir",4);
    for (int i=0;i<8;++i) {
        jv_note_on(a,0,48+i*3,100,i>0,i);
        jv_note_on(b,0,48+i*3,100,i>0,i);
    }
    assert(jv_active(a)==8);
    float x[JV_BLOCK],y[JV_BLOCK]; double energy=0;
    for (int block=0;block<300;++block) {
        jv_render(a,x); jv_render(b,y);
        for(int i=0;i<JV_BLOCK;++i) { assert(x[i]==y[i]); assert(std::isfinite(x[i])); energy+=x[i]*x[i]; }
    }
    assert(energy>0.01);
    // Editing the cursor must leave existing voices untouched.
    jv_text(a,"drugaya stroka",4);
    for(int block=0;block<100;++block) {
        jv_render(a,x);jv_render(b,y);
        for(int i=0;i<JV_BLOCK;++i) assert(x[i]==y[i]);
    }
    for(int i=0;i<8;++i) jv_note_off(a,0,48+i*3,i);
    for(int block=0;block<1000;++block) jv_render(a,x);
    assert(jv_active(a)==0);
    jv_note_on(a,0,60,100,0,101); jv_note_on(a,0,60,100,1,102);
    jv_note_off(a,0,60,101);
    for(int block=0;block<1000;++block) jv_render(a,x);
    assert(jv_active(a)==1);
    jv_panic(a); assert(jv_active(a)==0);
    // Every preview language, including the extended phonetic readers.
    for(int lang=1;lang<=9;++lang) {
        jv_text(a,lang==5 ? "liebe herz" : lang==6 ? "amour chanson" : "la luna",lang);
        jv_note_on(a,0,60,100,0,1);
        for(int block=0;block<80;++block) {jv_render(a,x);for(float v:x) assert(std::isfinite(v));}
        jv_panic(a);
    }
    // Every requested timbre control must affect the rendered signal.
    for(int control=0;control<6;++control) {
        jv_reset(a);jv_reset(b);
        for(int i=0;i<voice::controlCount;++i){jv_control(a,i,voice::controlDefaults[i]);jv_control(b,i,voice::controlDefaults[i]);}
        jv_control(b,control,control==voice::Buzz || control==voice::Artic ? 0:1);
        jv_text(a,"shchuka zhizn",4);jv_text(b,"shchuka zhizn",4);
        jv_note_on(a,0,60,100,0,1);jv_note_on(b,0,60,100,0,1);
        double difference=0;
        for(int block=0;block<300;++block){jv_render(a,x);jv_render(b,y);for(int i=0;i<JV_BLOCK;++i){assert(std::isfinite(y[i]));difference+=std::abs(x[i]-y[i]);}}
        assert(difference>.001);
    }
    // A single MIDI note creates the configured unison stack; Note Off releases all layers.
    // Grain keeps the zero setting unchanged and continuously changes sibilance.
    for(double amount:{.3,.7,1.0}){
        jv_seed(a,1234);jv_seed(b,1234);jv_reset(a);jv_reset(b);
        jv_control(b,voice::Grain,amount);jv_text(a,"sh",4);jv_text(b,"sh",4);
        jv_note_on(a,0,60,100,0,1);jv_note_on(b,0,60,100,0,1);double difference=0,energy=0,base=0;
        for(int k=0;k<700;++k){jv_render(a,x);jv_render(b,y);for(int i=0;i<JV_BLOCK;++i){assert(std::isfinite(y[i]));difference+=(x[i]-y[i])*(x[i]-y[i]);base+=x[i]*x[i];energy+=y[i]*y[i];}}
        assert(difference>base*(amount==1.0 ? .1:.02) && energy>.0001);
    }
    for(int nv=1;nv<=8;++nv) {
        jv_reset(a);jv_text(a,"la",2);jv_control(a,voice::Voices,(nv-1)/31.0);jv_control(a,voice::Unison,1);
        jv_note_on(a,0,60,100,0,77);assert(jv_active(a)==nv);assert(jv_position(a)==1);
        jv_note_off(a,0,60,77);for(int k=0;k<1000;++k)jv_render(a,x);assert(jv_active(a)==0);
    }
    jv_reset(a);jv_reset(b);jv_text(a,"la",2);jv_text(b,"la",2);
    jv_note_on(a,0,60,100,0,1);jv_note_on(b,0,60,100,0,1);
    jv_control(a,voice::Image,1);jv_control(b,voice::Detune,1);
    float left[JV_BLOCK],right[JV_BLOCK];double stereo=0,detune=0;
    for(int k=0;k<100;++k){jv_render_stereo(a,left,right);jv_render(b,y);for(int i=0;i<JV_BLOCK;++i){stereo+=std::abs(left[i]-right[i]);detune+=std::abs(left[i]-y[i]);}}
    assert(stereo>.01);
    jv_reset(a);jv_reset(b);jv_text(a,"la",2);jv_text(b,"la",2);
    jv_note_on(a,0,60,100,0,1);jv_note_on(b,0,60,100,0,1);jv_control(b,voice::Detune,1);
    detune=0;
    for(int k=0;k<100;++k){jv_render(a,x);jv_render(b,y);for(int i=0;i<JV_BLOCK;++i)detune+=std::abs(x[i]-y[i]);}
    assert(detune>.01);
    jv_control(a,voice::Voices,0);jv_note_on(a,0,62,100,0,2);assert(jv_active(a)==1);
    jv_destroy(a); jv_destroy(b);
    std::cout << "PASS: text limits, RU/DE/FR, 9 languages, 8 voices, isolation, note IDs, release, panic\n";
}
