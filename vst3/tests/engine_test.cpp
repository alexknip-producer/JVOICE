/* SPDX-License-Identifier: GPL-3.0-only */
#include "TextBank.h"
#include "VoiceEngine.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    voice::Line line;
    assert(voice::makeLine(u8"Привет, ёжик! Щука и любовь",4,line));
    assert(std::string(line.original.data()) == u8"Привет, ёжик! Щука и любовь");
    assert(std::string(line.sung.data()) == "privet, yozhik! shchuka i lyubov");
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
        for(int i=0;i<JV_BLOCK;++i) { assert(x[i]==y[i]); assert(std::isfinite(x[i]) && std::abs(x[i])<=1); energy+=x[i]*x[i]; }
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
    jv_destroy(a); jv_destroy(b);
    std::cout << "PASS: text limits, RU/DE/FR, 9 languages, 8 voices, isolation, note IDs, release, panic\n";
}
