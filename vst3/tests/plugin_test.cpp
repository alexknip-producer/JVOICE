/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/Plugin.cpp"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/utility/memoryibstream.h"
#include <cassert>
#include <iostream>
class EditSpy:public U::Implements<U::Directly<IComponentHandler>> {
public:
    int begins=0,edits=0,ends=0;
    tresult PLUGIN_API beginEdit(ParamID)override{++begins;return kResultOk;}
    tresult PLUGIN_API performEdit(ParamID,ParamValue v)override{assert(v>=0 && v<=1);++edits;return kResultOk;}
    tresult PLUGIN_API endEdit(ParamID)override{++ends;return kResultOk;}
    tresult PLUGIN_API restartComponent(int32)override{return kResultOk;}
};
int main(){
    using namespace voice;
    auto* p=new Plugin;assert(p->initialize(nullptr)==kResultOk);
    auto* spy=new EditSpy;p->setComponentHandler(spy);spy->release();
    ProcessSetup setup{kRealtime,kSample32,256,48000};assert(p->setupProcessing(setup)==kResultOk);
    assert(p->snapshot().language==5);
    assert(document(p->snapshot())=="pulsare quasare die sonne der mond die sterne");
    assert(p->snapshot().lines[1].original[0]==0);
    assert(p->setLanguage(4));
    assert(p->setDocument(u8"Привет, мир\r\n\r\nПой со мной"));
    assert(std::string(p->snapshot().lines[1].original.data()).empty());
    assert(document(p->snapshot())==u8"Привет, мир\n\nПой со мной");
    assert(!p->setDocument(std::string(129,'a')));
    assert(!p->setDocument(std::string(13,'\n')));
    assert(p->edit(12,u8"Тринадцатая строка"));p->pick(12);
    assert(!p->edit(13,"no"));
    for(int lang=1;lang<=9;++lang){assert(p->setLanguage(lang));assert(std::string(p->snapshot().lines[0].original.data())==u8"Привет, мир");}
    assert(p->setLanguage(4));
    for(int i=0;i<controlCount;++i){
        if(i==ReservedControl)continue;
        ParameterInfo info{};assert(p->getParameterInfoByTag(firstControlID+i,info)==kResultOk);
        assert(info.flags & ParameterInfo::kCanAutomate);
        p->gestureBegin(i);p->gestureValue(i,i==Voices ? 1:i==Unison || i==ExtInput || i==Mono ? 0:.5);p->gestureEnd(i);
    }
    assert(spy->begins>=controlCount-1 && spy->edits>=controlCount-1 && spy->begins==spy->ends);
    ParameterInfo retired{};assert(p->getParameterInfoByTag(firstControlID+ReservedControl,retired)!=kResultOk);
    ResizableMemoryIBStream stream;assert(p->getState(&stream)==kResultOk);
    auto* q=new Plugin;assert(q->initialize(nullptr)==kResultOk);stream.rewind();assert(q->setState(&stream)==kResultOk);
    auto bank=q->snapshot();assert(bank.selected==12 && bank.language==4);
    assert(std::string(bank.lines[12].original.data())==u8"Тринадцатая строка");
    for(int i=0;i<controlCount;++i)assert(p->control(i)==q->control(i));
    // Version 1 projects keep all their original rows; legacy selection maps into 13 visible rows.
    ResizableMemoryIBStream legacy;uint32_t header[]={0x31564A54,1,4,15};legacy.write(header,sizeof header,nullptr);
    for(int i=0;i<16;++i){const char* word=i==15 ? "legacy" : "la";uint32_t len=uint32_t(std::strlen(word));legacy.write(&len,4,nullptr);legacy.write(const_cast<char*>(word),len,nullptr);}
    legacy.rewind();assert(q->setState(&legacy)==kResultOk);assert(q->currentLine()==12);
    assert(std::string(q->snapshot().lines[15].original.data())=="legacy");
    for(int i=0;i<controlCount;++i)assert(q->control(i)==controlDefaults[i]);
    // Version 3 has fourteen controls; new Master Volume must default to 0 dB.
    ResizableMemoryIBStream previous;header[1]=3;previous.write(header,sizeof header,nullptr);
    for(int i=0;i<16;++i){uint32_t len=2;previous.write(&len,4,nullptr);previous.write(const_cast<char*>("la"),len,nullptr);}
    for(int i=0;i<14;++i){double value=.25;previous.write(&value,sizeof value,nullptr);}
    previous.rewind();assert(q->setState(&previous)==kResultOk);
    for(int i=0;i<14;++i)assert(q->control(i)==(i==ReservedControl ? 0:.25));
    assert(q->control(MasterVolume)==controlDefaults[MasterVolume]);
    assert(q->control(ExtGain)==controlDefaults[ExtGain]);
    assert(q->control(ModDepth)==controlDefaults[ModDepth] && q->control(ModRate)==controlDefaults[ModRate]);
    assert(q->control(Mono)==0 && q->control(Smear)==.5);
    // Version 4 gains survive; newly introduced modulation keeps its defaults.
    ResizableMemoryIBStream v4;header[1]=4;v4.write(header,sizeof header,nullptr);
    for(int i=0;i<16;++i){uint32_t len=2;v4.write(&len,4,nullptr);v4.write(const_cast<char*>("la"),len,nullptr);}
    for(int i=0;i<16;++i){double value=.25;v4.write(&value,sizeof value,nullptr);}
    v4.rewind();assert(q->setState(&v4)==kResultOk);
    assert(q->control(MasterVolume)==.25 && q->control(ExtGain)==.25);
    assert(q->control(ModDepth)==controlDefaults[ModDepth] && q->control(ModRate)==controlDefaults[ModRate]);
    assert(p->setActive(true)==kResultOk);assert(p->setProcessing(true)==kResultOk);
    float left[256]{},right[256]{};float* channels[]={left,right};AudioBusBuffers output{};
    output.numChannels=2;output.channelBuffers32=channels;EventList events;
    ProcessData data{};data.numSamples=256;data.numOutputs=1;data.outputs=&output;data.symbolicSampleSize=kSample32;data.processMode=kRealtime;data.inputEvents=&events;
    ProcessContext context{};context.sampleRate=48000;data.processContext=&context;
    Event key{};key.type=Event::kNoteOnEvent;key.noteOn.pitch=12;key.noteOn.velocity=1;events.addEvent(key);
    p->process(data);assert(p->currentLine()==12);for(float x:left)assert(x==0);
    events.clear();p->pick(0);p->process(data);p->edit(3,u8"Другая строка");p->process(data);assert(p->currentLine()==0);
    Event note{};note.type=Event::kNoteOnEvent;note.noteOn.pitch=60;note.noteOn.velocity=.8f;note.noteOn.noteId=1;
    events.addEvent(note);p->process(data);assert(p->syllablePosition()==1);events.clear();
    note.noteOn.noteId=2;events.addEvent(note);p->process(data);assert(p->syllablePosition()==2);events.clear();
    context.state=ProcessContext::kPlaying;note.noteOn.noteId=3;events.addEvent(note);p->process(data);assert(p->syllablePosition()==1);events.clear();
    p->process(data);assert(p->syllablePosition()==1); // no reset on every playing block
    context.state=0;p->process(data);context.state=ProcessContext::kPlaying;note.noteOn.noteId=4;events.addEvent(note);p->process(data);assert(p->syllablePosition()==1);events.clear();
    ParameterChanges automation;
    for(int i=0;i<controlCount;++i){int32 qi=0,pi=0;auto id=firstControlID+i;auto* queue=automation.addParameterData(id,qi);queue->addPoint(100,i==Unison || i==ExtInput || i==Mono ? 0:i==Voices ? 1:.7,pi);}
    data.inputParameterChanges=&automation;p->process(data);data.inputParameterChanges=nullptr;
    for(int i=0;i<controlCount;++i)assert(std::abs(p->control(i)-(i==Unison || i==ExtInput || i==Mono || i==ReservedControl ? 0:i==Voices ? 1:.7))<1e-9);
    double energy=0;for(int b=0;b<40;++b){p->process(data);for(float x:left){assert(std::isfinite(x));energy+=x*x;}}assert(energy>.01);
    for(int id=1;id<=4;++id){Event off{};off.type=Event::kNoteOffEvent;off.noteOff.pitch=60;off.noteOff.noteId=id;events.addEvent(off);}
    p->process(data);events.clear();for(int b=0;b<400;++b)p->process(data);for(float x:left)assert(x==0);
#ifdef _WIN32
    auto* view=p->createView(ViewType::kEditor);assert(view);
    HWND parent=CreateWindowExW(0,L"STATIC",L"JVoice test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,930,770,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(parent);assert(view->attached(parent,kPlatformTypeHWND)==kResultOk);
    HWND editor=FindWindowExW(parent,nullptr,L"JVoiceEditorV2",nullptr);assert(editor);
    HWND text=GetDlgItem(editor,1000);assert(text);assert(GetDlgItem(editor,1001)==nullptr);
    assert(GetWindowLongPtrW(text,GWL_STYLE)&ES_MULTILINE);
    SendMessageW(text,EM_SETSEL,0,-1);
    SendMessageW(text,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(L"Проверка русского текста\r\n\r\nТретья строка"));
    assert(document(p->snapshot())==u8"Проверка русского текста\n\nТретья строка");
    int third=int(SendMessageW(text,EM_LINEINDEX,2,0));SendMessageW(text,EM_SETSEL,third,third);
    SendMessageW(editor,WM_COMMAND,MAKEWPARAM(26,BN_CLICKED),0);assert(p->currentLine()==2);
    SendMessageW(editor,WM_COMMAND,MAKEWPARAM(24,BN_CLICKED),0);assert(p->currentLine()==3);
    SendMessageW(editor,WM_COMMAND,MAKEWPARAM(23,BN_CLICKED),0);assert(p->currentLine()==2);
    auto language=GetDlgItem(editor,20);SendMessageW(language,CB_SETCURSEL,1,0);
    SendMessageW(editor,WM_COMMAND,MAKEWPARAM(20,CBN_SELCHANGE),0);assert(p->snapshot().language==2);
    assert(std::string(p->snapshot().lines[0].original.data())==u8"Проверка русского текста");
    for(int i=0;i<controlCount;++i)if(i!=Unison && i!=ExtInput && i!=Mono && i!=PitchBendRange && i!=ReservedControl){auto knob=GetDlgItem(editor,2000+i);assert(knob);SendMessageW(knob,WM_KEYDOWN,VK_RIGHT,0);}
    assert(!GetDlgItem(editor,2000+ReservedControl));
    auto toggle=GetDlgItem(editor,25);SendMessageW(toggle,BM_SETCHECK,BST_CHECKED,0);SendMessageW(editor,WM_COMMAND,MAKEWPARAM(25,BN_CLICKED),0);assert(p->control(Unison)==1);
    auto monoToggle=GetDlgItem(editor,29);assert(monoToggle);SendMessageW(monoToggle,BM_SETCHECK,BST_CHECKED,0);SendMessageW(editor,WM_COMMAND,MAKEWPARAM(29,BN_CLICKED),0);assert(p->control(Mono)==1);
    ShowWindow(parent,SW_SHOW);UpdateWindow(parent);MSG msg{};
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(300);
    HDC dc=GetDC(editor),mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,900,720);auto oldBmp=SelectObject(mem,bmp);
    BitBlt(mem,0,0,900,720,dc,0,0,SRCCOPY);SelectObject(mem,oldBmp);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=900;info.bmiHeader.biHeight=-720;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    std::vector<uint8_t> pixels(900*720*4);GetDIBits(mem,bmp,0,720,pixels.data(),&info,DIB_RGB_COLORS);
    BITMAPFILEHEADER headerBmp{};headerBmp.bfType=0x4D42;headerBmp.bfOffBits=sizeof(headerBmp)+sizeof(BITMAPINFOHEADER);headerBmp.bfSize=headerBmp.bfOffBits+uint32_t(pixels.size());
    FILE* f=std::fopen("editor-preview.bmp","wb");assert(f);std::fwrite(&headerBmp,1,sizeof(headerBmp),f);std::fwrite(&info.bmiHeader,1,sizeof(BITMAPINFOHEADER),f);std::fwrite(pixels.data(),1,pixels.size(),f);std::fclose(f);
    DeleteDC(mem);ReleaseDC(editor,dc);DeleteObject(bmp);view->removed();view->release();DestroyWindow(parent);
#endif
    p->setProcessing(false);p->setActive(false);p->terminate();q->terminate();p->release();q->release();
    std::cout<<"PASS: multiline, language switching, legacy state, automation gestures/events, transport reset, keyswitches, stereo output, editor\n";
}
