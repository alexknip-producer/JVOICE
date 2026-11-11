/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/Plugin.cpp"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/utility/memoryibstream.h"
#include <cassert>
#include <future>
#include <iostream>
using namespace voice;
struct Spy:U::Implements<U::Directly<IComponentHandler>> {
    int changes=0;
    tresult PLUGIN_API beginEdit(ParamID)override{return kResultOk;}
    tresult PLUGIN_API performEdit(ParamID,ParamValue)override{++changes;return kResultOk;}
    tresult PLUGIN_API endEdit(ParamID)override{return kResultOk;}
    tresult PLUGIN_API restartComponent(int32)override{return kResultOk;}
};
struct Run {
    Plugin* p;float l[256]{},r[256]{},inL[256]{},inR[256]{};float* out[2]{l,r};float* in[2]{inL,inR};
    AudioBusBuffers bus{},input{};EventList events;ProcessData data{};
    explicit Run(uint32_t seed=123):p(new Plugin(seed)){
        assert(p->initialize(nullptr)==kResultOk);ProcessSetup setup{kRealtime,kSample32,256,48000};assert(p->setupProcessing(setup)==kResultOk);
        bus.numChannels=2;bus.channelBuffers32=out;input.numChannels=2;input.channelBuffers32=in;
        data.numSamples=256;data.numOutputs=1;data.outputs=&bus;data.symbolicSampleSize=kSample32;data.inputEvents=&events;
    }
    void start(){assert(p->setActive(true)==kResultOk);assert(p->setProcessing(true)==kResultOk);}
    void note(int ch=0){Event e{};e.type=Event::kNoteOnEvent;e.noteOn.channel=ch;e.noteOn.pitch=60;e.noteOn.velocity=.8;e.noteOn.noteId=1;events.addEvent(e);}
    void render(){assert(p->process(data)==kResultOk);events.clear();}
    void automate(int control,double value){ParameterChanges q;int32 ix=0,point=0;auto id=firstControlID+control;q.addParameterData(id,ix)->addPoint(0,value,point);data.inputParameterChanges=&q;render();data.inputParameterChanges=nullptr;}
    ~Run(){p->setProcessing(false);p->setActive(false);p->terminate();p->release();}
};
int main(){
    assert(plainControl(SpeechRate,1)==600 && plainControl(SpeechRate,controlDefaults[SpeechRate])==100);
    assert(plainControl(PitchBendRange,0)==2 && plainControl(PitchBendRange,1)==12);
    // Copy/paste state must never copy the editor's host handler or any live DSP storage.
    Run a,b,reference;auto* sa=new Spy;auto* sb=new Spy;a.p->setComponentHandler(sa);b.p->setComponentHandler(sb);
    a.p->setDocument("la luna");a.p->setParamNormalized(firstControlID+Shift,.3);
    ResizableMemoryIBStream state;a.p->getState(&state);state.rewind();assert(b.p->setComponentState(&state)==kResultOk);
    state.rewind();assert(reference.p->setEditorState(&state)==kResultOk);
    a.p->gestureBegin(Shift);a.p->gestureValue(Shift,.9);a.p->gestureEnd(Shift);
    assert(sa->changes==1 && sb->changes==0);assert(b.p->control(Shift)==.3);
    a.start();b.start();reference.start();a.note();b.note();reference.note();
    a.automate(Shift,.1);b.render();reference.render();
    for(int k=0;k<200;++k){a.render();b.render();reference.render();for(int i=0;i<256;++i){assert(b.l[i]==reference.l[i]);assert(b.r[i]==reference.r[i]);}}
    assert(b.p->control(Shift)==.3 && reference.p->control(Shift)==.3);
    ParamID bendID=0;assert(a.p->getMidiControllerAssignment(0,3,kPitchBend,bendID)==kResultOk && bendID==4003);
    // Same cloned states processed concurrently must remain independent.
    auto future=std::async(std::launch::async,[&](){std::vector<float> samples;for(int k=0;k<40;++k){b.render();samples.insert(samples.end(),b.l,b.l+256);}return samples;});
    std::vector<float> expected;for(int k=0;k<40;++k){reference.render();expected.insert(expected.end(),reference.l,reference.l+256);}assert(future.get()==expected);
#ifdef _WIN32
    HWND parents[2]{};IPlugView* views[2]{};HWND editors[2]{};
    Run* instances[2]={&a,&b};
    for(int i=0;i<2;++i){parents[i]=CreateWindowExW(0,L"STATIC",L"Two instances",WS_OVERLAPPEDWINDOW,0,0,940,780,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);views[i]=instances[i]->p->createView(ViewType::kEditor);assert(views[i]->attached(parents[i],kPlatformTypeHWND)==kResultOk);editors[i]=FindWindowExW(parents[i],nullptr,L"JVoiceEditorV2",nullptr);assert(editors[i]);}
    int beforeA=sa->changes,beforeB=sb->changes;double oldB=b.p->control(Shift);
    SendMessageW(GetDlgItem(editors[0],2000+Shift),WM_KEYDOWN,VK_UP,0);
    assert(sa->changes==beforeA+1 && sb->changes==beforeB && b.p->control(Shift)==oldB);
    // An unchanged timer tick must not invalidate every knob.
    SendMessageW(editors[0],WM_TIMER,1,0);auto knob=GetDlgItem(editors[0],2000+Shift);ValidateRect(knob,nullptr);
    SendMessageW(editors[0],WM_TIMER,1,0);assert(!GetUpdateRect(knob,nullptr,FALSE));
    for(int i=0;i<2;++i){views[i]->removed();views[i]->release();DestroyWindow(parents[i]);}
#endif
    sa->release();sb->release();
    // Native clock and interpolation: preserve 1kHz and stereo at common host rates.
    for(double rate:{22050.,44100.,48000.,88200.,96000.,192000.}){
        FixedRateStream stream;stream.reset(rate);double energy=0;int crossings=0;float previous=0;
        for(int i=0;i<int(rate);++i){float x=float(std::sin(2*3.141592653589793*1000*i/rate));auto out=stream.tick(x,-x,[](const float* l,const float* r,float* ol,float* orr){std::copy(l,l+JV_BLOCK,ol);std::copy(r,r+JV_BLOCK,orr);});
            assert(std::isfinite(out[0]) && std::abs(out[0]+out[1])<1e-6);if(i>rate*.2){if(previous<=0 && out[0]>0)++crossings;energy+=out[0]*out[0];}previous=out[0];}
        assert(std::abs(crossings-800)<=2);assert(energy/rate>.3);assert(std::abs(double(stream.nativeFrames)-44100)<2*JV_BLOCK);
    }
    // Stereo sidechain replaces voiced excitation only: left-only input must stay left.
    auto* e=jv_create();jv_control(e,ExtInput,1);jv_control(e,Breath,0);jv_control(e,Buzz,1);jv_text(e,"aaaa",2);jv_note_on(e,0,60,100,0,1);
    float il[JV_BLOCK],ir[JV_BLOCK]{},ol[JV_BLOCK],orr[JV_BLOCK];double leftEnergy=0,rightEnergy=0;
    for(int k=0;k<200;++k){for(int i=0;i<JV_BLOCK;++i)il[i]=float(.4*std::sin(2*3.141592653589793*220*(k*JV_BLOCK+i)/JV_SAMPLE_RATE));jv_render_input(e,il,ir,ol,orr);for(int i=0;i<JV_BLOCK;++i){leftEnergy+=ol[i]*ol[i];rightEnergy+=orr[i]*orr[i];}}
    assert(leftEnergy>.001 && rightEnergy<1e-9);
    // The host's optional stereo aux bus must reach the same carrier path.
    Run routed;routed.p->setDocument("aaaa");routed.p->setParamNormalized(firstControlID+ExtInput,1);routed.p->setParamNormalized(firstControlID+Breath,0);
    SpeakerArrangement stereo=SpeakerArr::kStereo;assert(routed.p->setBusArrangements(&stereo,1,&stereo,1)==kResultOk);
    routed.data.numInputs=1;routed.data.inputs=&routed.input;routed.start();routed.note();double hostL=0,hostR=0;
    for(int k=0;k<80;++k){for(int i=0;i<256;++i)routed.inL[i]=float(.5*std::sin(2*3.141592653589793*220*(k*256+i)/48000));routed.render();for(int i=0;i<256;++i){hostL+=routed.l[i]*routed.l[i];hostR+=routed.r[i]*routed.r[i];}}
    assert(hostL>.001 && hostR<1e-9);
    jv_reset(e);jv_control(e,ExtInput,1);jv_control(e,Breath,0);jv_text(e,"sa",1);jv_note_on(e,0,60,100,0,1);double hiss=0;
    for(int k=0;k<100;++k){jv_render_input(e,ir,ir,ol,orr);for(float x:ol)hiss+=x*x;}assert(hiss>.001);
    // Release extremes and polyphony headroom without a master saturation stage.
    jv_reset(e);jv_control(e,Release,0);jv_text(e,"la",1);jv_note_on(e,0,60,127,0,1);for(int k=0;k<10;++k)jv_render(e,ol);jv_note_off(e,0,60,1);for(int k=0;k<20;++k)jv_render(e,ol);assert(jv_active(e)==0);
    jv_reset(e);jv_control(e,Release,1);jv_text(e,"la",1);jv_note_on(e,0,60,127,0,1);for(int k=0;k<10;++k)jv_render(e,ol);jv_note_off(e,0,60,1);for(int k=0;k<20;++k)jv_render(e,ol);assert(jv_active(e)==1);
    jv_reset(e);jv_control(e,Buzz,1);jv_control(e,Breath,.05);jv_control(e,Resonance,1);jv_text(e,"aaaa",2);for(int n=0;n<8;++n)jv_note_on(e,0,60,127,n>0,n);
    float peak=0;for(int k=0;k<1000;++k){jv_render_stereo(e,ol,orr);for(int i=0;i<JV_BLOCK;++i){assert(std::isfinite(ol[i]));peak=std::max(peak,std::abs(ol[i]));}}assert(peak<1);
    auto* other=jv_create();jv_reset(e);
    for(auto* engine:{e,other}){jv_control(engine,ExtInput,1);jv_control(engine,Breath,0);jv_text(engine,"sa",1);jv_note_on(engine,0,60,100,0,1);}
    jv_control(e,SibilanceRandom,1);double sibilanceDifference=0;
    for(int k=0;k<100;++k){jv_render(e,ol);jv_render(other,orr);for(int i=0;i<JV_BLOCK;++i)sibilanceDifference+=std::abs(ol[i]-orr[i]);}assert(sibilanceDifference>.001);
    jv_reset(e);jv_reset(other);
    for(auto* engine:{e,other}){jv_control(engine,Buzz,1);jv_control(engine,Breath,0);jv_text(engine,"aaaa",2);jv_note_on(engine,1,69,100,0,1);}
    jv_bend(e,0,1);
    for(int k=0;k<100;++k){jv_render(e,ol);jv_render(other,orr);for(int i=0;i<JV_BLOCK;++i)assert(ol[i]==orr[i]);}
    jv_control(e,PitchBendRange,1);jv_bend(e,1,1);double bendDifference=0;
    for(int k=0;k<100;++k){jv_render(e,ol);jv_render(other,orr);for(int i=0;i<JV_BLOCK;++i)bendDifference+=std::abs(ol[i]-orr[i]);}assert(bendDifference>.01);
    jv_destroy(other);
    jv_destroy(e);
    std::cout<<"PASS: duplicated-instance automation/UI/DSP isolation, fixed 44.1kHz clock, stereo carrier, retained hiss, release and headroom\n";
}
