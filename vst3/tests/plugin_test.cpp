/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/Plugin.cpp"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/utility/memoryibstream.h"
#include <cassert>
#include <iostream>

int main() {
    auto* p = new voice::Plugin;
    assert(p->initialize(nullptr)==kResultOk);
    ProcessSetup setup{kRealtime,kSample32,256,48000};
    assert(p->setupProcessing(setup)==kResultOk);
    assert(p->edit(15,u8"Шестнадцатая строка"));
    p->pick(15);
    ResizableMemoryIBStream stream;
    assert(p->getState(&stream)==kResultOk);
    auto* q = new voice::Plugin;
    assert(q->initialize(nullptr)==kResultOk);
    stream.rewind();assert(q->setState(&stream)==kResultOk);
    auto bank=q->snapshot();
    assert(bank.selected==15 && bank.language==4);
    assert(std::string(bank.lines[15].original.data())==u8"Шестнадцатая строка");
    assert(std::string(bank.lines[15].sung.data())=="shestnadtsataya stroka");
    assert(p->setActive(true)==kResultOk);
    assert(p->setProcessing(true)==kResultOk);
    float left[256]{},right[256]{};float* channels[]={left,right};
    AudioBusBuffers output{};output.numChannels=2;output.channelBuffers32=channels;
    EventList events;
    ProcessData data{};data.numSamples=256;data.numOutputs=1;data.outputs=&output;
    data.symbolicSampleSize=kSample32;data.processMode=kRealtime;data.inputEvents=&events;
    Event key{};key.type=Event::kNoteOnEvent;key.noteOn.pitch=0;key.noteOn.velocity=1;
    events.addEvent(key);assert(p->process(data)==kResultOk);
    assert(p->currentLine()==0);
    for(float s:left)assert(s==0);
    // The chosen keyswitch must survive an edit to a different row.
    p->edit(3,u8"Другая строка");events.clear();p->process(data);assert(p->currentLine()==0);
    Event note{};note.type=Event::kNoteOnEvent;note.noteOn.pitch=60;note.noteOn.velocity=.8f;note.noteOn.noteId=1;
    events.addEvent(note);double energy=0;
    for(int b=0;b<80;++b) {
        assert(p->process(data)==kResultOk);events.clear();
        for(int i=0;i<256;++i){assert(std::isfinite(left[i]) && left[i]==right[i]);energy+=left[i]*left[i];}
    }
    assert(energy>.01);
    note.type=Event::kNoteOffEvent;note.noteOff.pitch=60;note.noteOff.noteId=1;
    events.addEvent(note);p->process(data);events.clear();
    for(int b=0;b<200;++b)p->process(data);
    for(float s:left)assert(s==0);
#ifdef _WIN32
    auto* view=p->createView(ViewType::kEditor);assert(view);
    HWND parent=CreateWindowExW(0,L"STATIC",L"JVOICE test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        0,0,790,740,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(parent);assert(view->attached(parent,kPlatformTypeHWND)==kResultOk);
    HWND editor=FindWindowExW(parent,nullptr,L"JiantVoiceEditorV1",nullptr);assert(editor);
    HWND text=GetDlgItem(editor,1000);assert(text);
    SetWindowTextW(text,L"Проверка русского текста");
    assert(std::string(p->snapshot().lines[0].original.data())==u8"Проверка русского текста");
    ShowWindow(parent,SW_SHOW);UpdateWindow(parent);
    MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    Sleep(300);
    HDC dc=GetDC(editor),mem=CreateCompatibleDC(dc);
    HBITMAP bmp=CreateCompatibleBitmap(dc,760,685);SelectObject(mem,bmp);
    BitBlt(mem,0,0,760,685,dc,0,0,SRCCOPY);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=760;
    info.bmiHeader.biHeight=-685;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    std::vector<uint8_t> pixels(760*685*4);GetDIBits(mem,bmp,0,685,pixels.data(),&info,DIB_RGB_COLORS);
    BITMAPFILEHEADER header{};header.bfType=0x4D42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
    header.bfSize=header.bfOffBits+uint32_t(pixels.size());
    FILE* f=std::fopen("editor-preview.bmp","wb");assert(f);
    std::fwrite(&header,1,sizeof(header),f);std::fwrite(&info.bmiHeader,1,sizeof(BITMAPINFOHEADER),f);
    std::fwrite(pixels.data(),1,pixels.size(),f);std::fclose(f);
    DeleteDC(mem);ReleaseDC(editor,dc);DeleteObject(bmp);
    view->removed();view->release();DestroyWindow(parent);
#endif
    p->setProcessing(false);p->setActive(false);
    p->terminate();q->terminate();p->release();q->release();
    std::cout<<"PASS: VST3 lifecycle, UTF-8 state roundtrip, keyswitch, sound, note off, editor (Windows)\n";
}
