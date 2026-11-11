/* SPDX-License-Identifier: GPL-3.0-only */
#include "TextBank.h"
#include "VoiceEngine.h"
#include "VoiceParameters.h"
#include "FixedRateStream.h"
#include "pluginterfaces/base/ustring.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#endif

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace voice {
static_assert(controlCount==JV_CONTROL_COUNT,"Plugin and DSP parameter counts must match");
static const FUID classID(0x82B9CB47,0xC243412D,0x8F8B35EA,0x3DE56001);
static std::atomic<uint32_t> noiseSerial{0x2545F491u};
class Plugin;
#ifdef _WIN32
class View : public CPluginView {
    Plugin& owner;
    HWND window=nullptr,language=nullptr,linePick=nullptr,status=nullptr,text=nullptr,unison=nullptr,external=nullptr,bendRange=nullptr;
    HWND knobs[controlCount]{};
    HFONT font=nullptr;
    HINSTANCE module=nullptr;
    bool loading=false;
    int dragging=-1,dragY=0;double dragStart=0;
    uint32_t seenRevision=0;
    std::array<double,controlCount> lastDrawn{};
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK knobProc(HWND,UINT,WPARAM,LPARAM);
    void refresh();void command(int,int);void paintKnob(HWND,int);
public:
    explicit View(Plugin&);
    ~View() override;
    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override {return std::strcmp(type,kPlatformTypeHWND)==0 ? kResultTrue:kResultFalse;}
    tresult PLUGIN_API attached(void*,FIDString) override;
    tresult PLUGIN_API removed() override;
};
#endif

class VoiceParameter : public RangeParameter {
    int index;
public:
    explicit VoiceParameter(int i):RangeParameter(STR16(""),firstControlID+i,nullptr,
        i==MasterVolume ? -60 : i==ExtGain ? -24 : i==Shift ? -12 : i==SpeechRate ? 50 : i==Voices || i==Release ? 1 : i==PitchBendRange ? 2 : 0,
        i==ExtGain ? 24 : i==MasterVolume || i==Shift ? 12 : i==SpeechRate ? 600 : i==Detune ? 40 : i==Unison || i==ExtInput ? 1 : i==Voices ? 8 : i==Release ? 10000 : i==PitchBendRange ? 12 : 100,
        plainControl(i,controlDefaults[i]),i==Unison || i==ExtInput ? 1 : i==Voices ? 7 : i==PitchBendRange ? 10 : 0),index(i) {
        UString(info.title,128).fromAscii(controlNames[i]);
        UString(info.units,128).fromAscii(i==MasterVolume || i==ExtGain ? "dB" : i==Shift ? "st" : i==Detune ? "ct" : i==Release ? "ms" : i==PitchBendRange ? "st" : i==Voices || i==Unison || i==ExtInput ? "" : "%");
        setPrecision(i==Shift || i==Unison || i==ExtInput || i==Voices || i==Release || i==PitchBendRange ? 0:1);
        info.defaultNormalizedValue=controlDefaults[i];setNormalized(controlDefaults[i]);
    }
    ParamValue toPlain(ParamValue n) const override {return plainControl(index,n);}
    ParamValue toNormalized(ParamValue p) const override {return normalizedControl(index,p);}
};

class Plugin : public SingleComponentEffect, public IMidiMapping {
public:
    static FUnknown* create(void*) { return static_cast<IComponent*>(new Plugin); }
    explicit Plugin(uint32_t seed=0) : engine(jv_create()) {
        if(engine)jv_seed(engine,seed ? seed : noiseSerial.fetch_add(0x9E3779B9u));
        for(int i=0;i<controlCount;++i)controls[i].store(controlDefaults[i]);
        makeLine("pulsare quasare die sonne der mond die sterne",5,bank.lines[0]);
    }
    ~Plugin() override { jv_destroy(engine); }
    tresult PLUGIN_API queryInterface(const TUID id,void** obj) override {
        if (FUnknownPrivate::iidEqual(id,IMidiMapping::iid)) { *obj=static_cast<IMidiMapping*>(this);addRef();return kResultOk; }
        return SingleComponentEffect::queryInterface(id,obj);
    }
    uint32 PLUGIN_API addRef() override { return SingleComponentEffect::addRef(); }
    uint32 PLUGIN_API release() override { return SingleComponentEffect::release(); }
    tresult PLUGIN_API initialize(FUnknown* context) override {
        auto r=SingleComponentEffect::initialize(context); if(r!=kResultOk)return r;
        if(!engine)return kOutOfMemory;
        processContextRequirements.needTransportState();
        addAudioOutput(STR16("Stereo output"),SpeakerArr::kStereo);
        addAudioInput(STR16("Sidechain"),SpeakerArr::kStereo,kAux,0);
        addEventInput(STR16("MIDI input"),16);
        parameters.addParameter(STR16("Text line"),nullptr,lineCount-1,0,ParameterInfo::kCanAutomate,1);
        for(int i=0;i<controlCount;++i)if(i!=ReservedControl)parameters.addParameter(new VoiceParameter(i));
        for(int ch=0;ch<16;++ch) for(int cc : {120,123})
            parameters.addParameter(STR16("MIDI panic"),nullptr,0,0,ParameterInfo::kIsHidden,100+ch*128+cc);
        for(int ch=0;ch<16;++ch)parameters.addParameter(STR16("Pitch bend"),nullptr,0,.5,ParameterInfo::kIsHidden,4000+ch);
        return kResultOk;
    }
    tresult PLUGIN_API getMidiControllerAssignment(int32 bus,int16 channel,CtrlNumber cc,ParamID& id) override {
        if(bus==0 && channel>=0 && channel<16 && cc==kPitchBend){id=4000+channel;return kResultOk;}
        if(bus!=0 || channel<0 || channel>=16 || (cc!=120 && cc!=123))return kResultFalse;
        id=100+channel*128+cc;return kResultOk;
    }
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* in,int32 ni,SpeakerArrangement* out,int32 no) override {
        if(no!=1 || !out || out[0]!=SpeakerArr::kStereo || ni<0 || ni>1)return kResultFalse;
        if(ni && (!in || (in[0]!=SpeakerArr::kStereo && in[0]!=SpeakerArr::kMono)))return kResultFalse;
        if(ni)static_cast<AudioBus*>(audioInputs.at(0).get())->setArrangement(in[0]);
        return kResultOk;
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) override {
        return size==kSample32 || size==kSample64 ? kResultTrue : kResultFalse;
    }
    tresult PLUGIN_API setupProcessing(ProcessSetup& s) override {
        if(!std::isfinite(s.sampleRate) || s.sampleRate<8000 || s.sampleRate>384000)return kInvalidArgument;
        stream.reset(s.sampleRate);return SingleComponentEffect::setupProcessing(s);
    }
    tresult PLUGIN_API setActive(TBool active) override {
        if(active) {
            transportPlaying=false;jv_reset(engine);stream.reset(processSetup.sampleRate);held.fill(0);
            appliedRevision=0;syncBank();selectLine(selected.load());
            for(int i=0;i<controlCount;++i)jv_control(engine,i,controls[i].load());
        } else jv_panic(engine);
        return kResultOk;
    }
    tresult PLUGIN_API setProcessing(TBool state) override {
        if(!state){transportPlaying=false;jv_panic(engine);stream.reset(processSetup.sampleRate);held.fill(0);}
        return kResultOk;
    }
    uint32 PLUGIN_API getLatencySamples() override {return FixedRateStream::latency(processSetup.sampleRate);}
    uint32 PLUGIN_API getTailSamples() override {return uint32_t(processSetup.sampleRate*(.4+2*plainControl(Release,controls[Release].load())/1000));}
    tresult PLUGIN_API process(ProcessData& data) override {
        syncBank();
        if(data.processContext){bool playing=(data.processContext->state & ProcessContext::kPlaying)!=0;
            if(playing && !transportPlaying)selectLine(selected.load());transportPlaying=playing;}
        int request=requestedLine.exchange(-1);
        if(request>=0)selectLine(request);
        for(int i=0;i<controlCount;++i)jv_control(engine,i,controls[i].load());
        if(data.numSamples<=0 || data.numOutputs<1 || !data.outputs || data.outputs[0].numChannels<2) {
            if(data.inputParameterChanges)for(int i=0;i<data.inputParameterChanges->getParameterCount();++i){
                auto* q=data.inputParameterChanges->getParameterData(i);if(!q)continue;
                ParamValue v=0;int32 offset=0,n=q->getPointCount();
                if(n && q->getPoint(n-1,offset,v)==kResultOk)applyParameter(q->getParameterId(),v);
            }
            return kResultOk;
        }
        auto* events=data.inputEvents; int count=events ? events->getEventCount():0, evIndex=0;
        Event ev{}; bool has=events && count>0 && events->getEvent(0,ev)==kResultOk;
        struct Change {IParamValueQueue* queue;int next=0,count=0;int32 offset=0;ParamValue value=0;bool ready=false;};
        std::array<Change,64> changes{};int nChanges=0;
        if(data.inputParameterChanges) for(int i=0;i<data.inputParameterChanges->getParameterCount() && nChanges<64;++i) {
            auto* q=data.inputParameterChanges->getParameterData(i);if(!q)continue;
            auto& c=changes[nChanges++];c.queue=q;c.count=q->getPointCount();
            c.ready=c.count>0 && q->getPoint(0,c.offset,c.value)==kResultOk;
        }
        for(int sample=0;sample<data.numSamples;++sample) {
            for(int j=0;j<nChanges;++j) {
                auto& c=changes[j];
                while(c.ready && c.offset<=sample) {
                    applyParameter(c.queue->getParameterId(),c.value);
                    ++c.next;c.ready=c.next<c.count && c.queue->getPoint(c.next,c.offset,c.value)==kResultOk;
                }
            }
            bool chord=false;
            while(has && ev.sampleOffset<=sample) {
                if(ev.busIndex==0) {
                    if(ev.type==Event::kNoteOnEvent) {
                        auto& n=ev.noteOn;
                        if(n.pitch<lineCount && n.pitch>=0)selectLine(n.pitch);
                        else if(n.pitch<128 && n.pitch>=lineCount && n.channel>=0 && n.channel<16) {
                            if(rt.lines[rt.selected].sung[0]) {
                                int velocity=std::clamp(int(n.velocity*127+0.5f),1,127);
                                jv_note_on(engine,n.channel,n.pitch,velocity,chord,n.noteId);
                                chord=true;
                            }
                        }
                    } else if(ev.type==Event::kNoteOffEvent) {
                        auto& n=ev.noteOff;
                        if(n.pitch>=lineCount && n.pitch<128)jv_note_off(engine,n.channel,n.pitch,n.noteId);
                    }
                }
                ++evIndex;has=evIndex<count && events->getEvent(evIndex,ev)==kResultOk;
            }
            float input[2]{};
            if(data.numInputs>0 && data.inputs && data.inputs[0].numChannels>0){
                auto& bus=data.inputs[0];
                for(int ch=0;ch<2;++ch){int c=std::min(ch,bus.numChannels-1);
                    if(!(bus.silenceFlags & (uint64(1)<<c))){
                        if(data.symbolicSampleSize==kSample32 && bus.channelBuffers32 && bus.channelBuffers32[c])input[ch]=bus.channelBuffers32[c][sample];
                        if(data.symbolicSampleSize==kSample64 && bus.channelBuffers64 && bus.channelBuffers64[c])input[ch]=float(bus.channelBuffers64[c][sample]);
                    }
                }
            }
            auto value=stream.tick(input[0],input[1],[&](const float* l,const float* r,float* outL,float* outR){jv_render_input(engine,l,r,outL,outR);});
            float left=value[0],right=value[1];
            if(data.symbolicSampleSize==kSample32) {
                data.outputs[0].channelBuffers32[0][sample]=left;
                data.outputs[0].channelBuffers32[1][sample]=right;
            } else {
                data.outputs[0].channelBuffers64[0][sample]=left;
                data.outputs[0].channelBuffers64[1][sample]=right;
            }
        }
        data.outputs[0].silenceFlags=0;
        return kResultOk;
    }
    IPlugView* PLUGIN_API createView(FIDString name) override {
#ifdef _WIN32
        if(name && std::strcmp(name,ViewType::kEditor)==0)return new View(*this);
#endif
        return nullptr;
    }
    bool edit(int row,std::string_view text,int lang=-1) {
        if(row<0||row>=lineCount)return false;
        std::lock_guard<std::mutex> lock(mutex);
        Line line;
        if(!makeLine(text,lang<0 ? bank.language:lang,line))return false;
        bank.lines[row]=line;++revision;return true;
    }
    bool setDocument(std::string_view text) {
        std::lock_guard<std::mutex> lock(mutex);Bank copy=bank;
        for(int i=0;i<lineCount;++i)copy.lines[i]={};
        size_t start=0;int row=0;
        for(;;) {
            auto end=text.find('\n',start);auto part=text.substr(start,end==std::string_view::npos ? text.size()-start:end-start);
            if(!part.empty() && part.back()=='\r')part.remove_suffix(1);
            if(row>=lineCount || !makeLine(part,bank.language,copy.lines[row++]))return false;
            if(end==std::string_view::npos)break;start=end+1;
        }
        bank=copy;++revision;return true;
    }
    ParamValue PLUGIN_API getParamNormalized(ParamID id) override {
        if(id>=firstControlID && id<firstControlID+controlCount)return controls[id-firstControlID].load();
        if(id==1)return selected.load()/double(lineCount-1);
        return SingleComponentEffect::getParamNormalized(id);
    }
    tresult PLUGIN_API setParamNormalized(ParamID id,ParamValue v) override {
        if(id==firstControlID+ReservedControl)return kResultFalse;
        if(!std::isfinite(v))return kInvalidArgument;
        if(id>=firstControlID && id<firstControlID+controlCount)controls[id-firstControlID].store(std::clamp(v,0.0,1.0));
        return SingleComponentEffect::setParamNormalized(id,v);
    }
    double control(int i) const {return controls[i].load();}
    void gestureBegin(int i){beginEdit(firstControlID+i);}
    void gestureValue(int i,double value) {
        value=std::clamp(value,0.0,1.0);
        if(i==Voices)value=std::round(value*7)/7;
        if(i==Unison || i==ExtInput)value=value>=.5 ? 1:0;
        if(i==PitchBendRange)value=std::round(value*10)/10;
        setParamNormalized(firstControlID+i,value);performEdit(firstControlID+i,value);
    }
    void gestureEnd(int i){endEdit(firstControlID+i);}
    bool setLanguage(int lang) {
        if(lang<1||lang>9)return false;
        std::lock_guard<std::mutex> lock(mutex);Bank copy=bank;
        for(int i=0;i<storedLineCount;++i)if(!makeLine(bank.lines[i].original.data(),lang,copy.lines[i]))return false;
        copy.language=lang;bank=copy;++revision;return true;
    }
    Bank snapshot() {std::lock_guard<std::mutex> lock(mutex);Bank copy=bank;copy.selected=selected.load();return copy;}
    void pick(int line) {
        if(line<0||line>=lineCount)return;
        {std::lock_guard<std::mutex> lock(mutex);bank.selected=line;++revision;}
        selected.store(line);
        requestedLine.store(line);
        beginEdit(1);setParamNormalized(1,line/double(lineCount-1));performEdit(1,line/double(lineCount-1));endEdit(1);
    }
    uint32_t version() const {return revision.load();}
    int currentLine() const {return selected.load();}
    tresult PLUGIN_API setComponentState(IBStream* state) override {return setState(state);}
    tresult PLUGIN_API getEditorState(IBStream* state) override {return getState(state);}
    tresult PLUGIN_API setEditorState(IBStream* state) override {setState(state);return kResultOk;}
    unsigned syllablePosition(){return jv_position(engine);}
    tresult PLUGIN_API getState(IBStream* stream) override {
        if(!stream)return kInvalidArgument;
        Bank copy=snapshot();
        const uint32_t magic=0x31564A54;
        auto write=[&](const void* p,int n){int32 done=0;return stream->write(const_cast<void*>(p),n,&done)==kResultOk && done==n;};
        uint32_t values[]={magic,4,uint32_t(copy.language),uint32_t(copy.selected)};
        if(!write(values,sizeof values))return kResultFalse;
        for(auto& l:copy.lines) {
            uint32_t len=uint32_t(std::strlen(l.original.data()));
            if(!write(&len,4)||!write(l.original.data(),int(len)))return kResultFalse;
        }
        for(auto& c:controls){double v=c.load();if(!write(&v,sizeof v))return kResultFalse;}
        return kResultOk;
    }
    tresult PLUGIN_API setState(IBStream* stream) override {
        if(!stream)return kInvalidArgument;
        Bank copy;
        auto read=[&](void* p,int n){int32 done=0;return stream->read(p,n,&done)==kResultOk && done==n;};
        uint32_t values[4]{};
        if(!read(values,sizeof values)||values[0]!=0x31564A54||(values[1]<1 || values[1]>4)||values[2]<1||values[2]>9||values[3]>15)return kResultFalse;
        copy.language=int(values[2]);copy.selected=std::min(int(values[3]),lineCount-1);
        for(auto& l:copy.lines) {
            uint32_t len=0;char text[textBytes]{};
            if(!read(&len,4)||len>=textBytes||!read(text,int(len))||!makeLine(std::string_view(text,len),copy.language,l))return kResultFalse;
        }
        auto restored=controlDefaults;
        int nControls=values[1]==1 ? 0:values[1]==2 ? 10:values[1]==3 ? 14:controlCount;
        for(int i=0;i<nControls;++i){auto& v=restored[i];if(!read(&v,sizeof v)||!std::isfinite(v)||v<0||v>1)return kResultFalse;}
        for(int i=0;i<controlCount;++i)setParamNormalized(firstControlID+i,restored[i]);
        {std::lock_guard<std::mutex> lock(mutex);bank=copy;++revision;}
        selected.store(copy.selected);
        requestedLine.store(copy.selected);
        setParamNormalized(1,copy.selected/double(lineCount-1));
        return kResultOk;
    }
private:
    void applyParameter(ParamID id,double v) {
        if(!std::isfinite(v))return;v=std::clamp(v,0.0,1.0);
        if(id==1)selectLine(int(v*(lineCount-1)+.5));
        else if(id>=firstControlID && id<firstControlID+controlCount){int i=int(id-firstControlID);if(i==ReservedControl)return;controls[i].store(v);jv_control(engine,i,v);}
        else if(id>=4000 && id<4016)jv_bend(engine,int(id-4000),2*v-1);
        else if(id>=100){jv_panic(engine);held.fill(0);}
    }
    void syncBank() {
        uint32_t v=revision.load();if(v==appliedRevision)return;
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock)return;
        int old=rt.selected;
        bool changed=rt.language!=bank.language || rt.lines[old].sung!=bank.lines[old].sung;
        rt=bank;rt.selected=old;appliedRevision=revision.load();
        if(changed)selectLine(old);
    }
    void selectLine(int line) {
        rt.selected=std::clamp(line,0,lineCount-1);selected.store(rt.selected);
        jv_text(engine,rt.lines[rt.selected].sung.data(),rt.language);
    }
    bool transportPlaying=false;
    jv_engine* engine;
    Bank bank{},rt{};
    std::mutex mutex;
    std::atomic<uint32_t> revision{1};uint32_t appliedRevision=0;
    std::atomic<int> selected{0};
    std::atomic<int> requestedLine{-1};
    std::array<uint8_t,16> held{};
    std::array<std::atomic<double>,controlCount> controls;
    FixedRateStream stream;
};

#ifdef _WIN32
static int attachedViews=0;
static const wchar_t* editorClass=L"JVoiceEditorV2";
static const wchar_t* knobClass=L"JVoiceKnobV2";
static std::wstring wide(const char* text) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,nullptr,0);if(n<1)return {};
    std::wstring out(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,0,text,-1,out.data(),n);out.pop_back();return out;
}
static std::wstring lineLabel(int row) {
    static const wchar_t* notes[]={L"C",L"C#",L"D",L"D#",L"E",L"F",L"F#",L"G",L"G#",L"A",L"A#",L"B"};
    return std::to_wstring(row+1)+L"  /  "+notes[row%12]+(row==12 ? L"-1":L"-2");
}
View::View(Plugin& p):owner(p){rect={0,0,900,720};lastDrawn.fill(-1);owner.addRef();}
View::~View(){removed();owner.release();}
LRESULT CALLBACK View::proc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<View*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<View*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(self && msg==WM_COMMAND){self->command(LOWORD(w),HIWORD(w));return 0;}
    if(self && msg==WM_TIMER){self->refresh();return 0;}
    return DefWindowProcW(h,msg,w,l);
}
LRESULT CALLBACK View::knobProc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<View*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<View*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(h,msg,w,l);
    int i=GetDlgCtrlID(h)-2000;
    if(i<0||i>=controlCount)return DefWindowProcW(h,msg,w,l);
    switch(msg) {
    case WM_PAINT:self->paintKnob(h,i);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_SETFOCUS:case WM_KILLFOCUS:InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_LBUTTONDOWN:
        SetFocus(h);SetCapture(h);self->dragging=i;self->dragY=GET_Y_LPARAM(l);self->dragStart=self->owner.control(i);self->owner.gestureBegin(i);return 0;
    case WM_MOUSEMOVE:
        if(self->dragging==i && GetCapture()==h){self->owner.gestureValue(i,self->dragStart+(self->dragY-GET_Y_LPARAM(l))/160.0);InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_LBUTTONUP:
        if(self->dragging==i){self->owner.gestureEnd(i);self->dragging=-1;ReleaseCapture();}return 0;
    case WM_CAPTURECHANGED:
        if(self->dragging==i){self->owner.gestureEnd(i);self->dragging=-1;}return 0;
    case WM_LBUTTONDBLCLK:
        self->owner.gestureBegin(i);self->owner.gestureValue(i,controlDefaults[i]);self->owner.gestureEnd(i);InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN: {
        double delta=msg==WM_MOUSEWHEEL ? GET_WHEEL_DELTA_WPARAM(w)/120.0 : w==VK_UP || w==VK_RIGHT ? 1 : w==VK_DOWN || w==VK_LEFT ? -1 : 0;
        if(!delta)return DefWindowProcW(h,msg,w,l);
        self->owner.gestureBegin(i);self->owner.gestureValue(i,self->owner.control(i)+delta*(i==Voices ? 1.0/7:.01));self->owner.gestureEnd(i);InvalidateRect(h,nullptr,FALSE);return 0;
    }
    }
    return DefWindowProcW(h,msg,w,l);
}
void View::paintKnob(HWND h,int i) {
    PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);
    HDC screen=dc;HBITMAP bitmap=CreateCompatibleBitmap(screen,r.right,r.bottom);dc=CreateCompatibleDC(screen);auto oldBitmap=SelectObject(dc,bitmap);FillRect(dc,&r,GetSysColorBrush(COLOR_BTNFACE));
    auto oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(35,45,55));
    RECT label{0,0,r.right,24};auto title=wide(controlNames[i]);DrawTextW(dc,title.c_str(),-1,&label,DT_CENTER|DT_SINGLELINE);
    int cx=r.right/2,cy=52;auto pen=CreatePen(PS_SOLID,2,RGB(75,105,125));auto brush=CreateSolidBrush(RGB(245,247,250));
    auto oldPen=SelectObject(dc,pen),oldBrush=SelectObject(dc,brush);Ellipse(dc,cx-25,cy-25,cx+25,cy+25);
    double n=owner.control(i),angle=(135+n*270)*3.141592653589793/180;
    MoveToEx(dc,cx,cy,nullptr);LineTo(dc,cx+int(std::cos(angle)*20),cy+int(std::sin(angle)*20));
    SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);DeleteObject(brush);
    wchar_t value[64]{};double p=plainControl(i,n);
    if(i==Shift)swprintf(value,64,L"%+.0f st",p);
    else if(i==Voices)swprintf(value,64,L"%.0f",p);
    else if(i==Detune)swprintf(value,64,L"%.1f ct",p);
    else if(i==Release)swprintf(value,64,L"%.0f ms",p);
    else if(i==MasterVolume || i==ExtGain)swprintf(value,64,L"%+.1f dB",p);
    else swprintf(value,64,L"%.0f %%",p);
    RECT number{0,83,r.right,108};DrawTextW(dc,value,-1,&number,DT_CENTER|DT_SINGLELINE);
    if(GetFocus()==h){RECT focus{2,1,r.right-2,r.bottom-2};DrawFocusRect(dc,&focus);}
    SelectObject(dc,oldFont);BitBlt(screen,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);SelectObject(dc,oldBitmap);DeleteObject(bitmap);DeleteDC(dc);EndPaint(h,&ps);
}
tresult PLUGIN_API View::attached(void* parent,FIDString type) {
    if(!parent||window||isPlatformTypeSupported(type)!=kResultTrue)return kResultFalse;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&View::proc),&module);
    WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=module;wc.lpszClassName=editorClass;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_BTNFACE);RegisterClassW(&wc);
    wc.lpfnWndProc=knobProc;wc.lpszClassName=knobClass;wc.style=CS_DBLCLKS;RegisterClassW(&wc);
    window=CreateWindowExW(0,editorClass,L"JVoice",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,900,720,static_cast<HWND>(parent),nullptr,module,this);
    if(!window)return kResultFalse;++attachedViews;
    font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    auto control=[&](const wchar_t* cls,const wchar_t* title,DWORD style,int x,int y,int width,int height,int id){
        HWND h=CreateWindowExW(std::wcscmp(cls,L"EDIT")==0 ? WS_EX_CLIENTEDGE:0,cls,title,WS_CHILD|WS_VISIBLE|style,
            x,y,width,height,window,reinterpret_cast<HMENU>(INT_PTR(id)),module,nullptr);
        SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;
    };
    control(L"STATIC",L"JVoice",0,24,16,500,26,0);
    external=control(L"BUTTON",L"Ext. input",BS_AUTOCHECKBOX|WS_TABSTOP,540,12,115,28,28);
    control(L"STATIC",L"Pitch bend",0,666,17,90,24,0);
    bendRange=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,765,11,112,310,27);
    for(int st=2;st<=12;++st){auto label=L"+/- "+std::to_wstring(st)+L" st";SendMessageW(bendRange,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}
    control(L"STATIC",L"Language",0,24,58,80,24,0);
    language=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,105,53,85,280,20);
    for(auto* name:languageNames)SendMessageW(language,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(name).c_str()));
    control(L"STATIC",L"Line",0,220,58,64,24,0);
    linePick=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,285,53,140,420,21);
    for(int i=0;i<lineCount;++i)SendMessageW(linePick,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(lineLabel(i).c_str()));
    control(L"BUTTON",L"Previous",WS_TABSTOP,442,52,110,29,23);
    control(L"BUTTON",L"Next",WS_TABSTOP,560,52,110,29,24);
    control(L"BUTTON",L"Restart",WS_TABSTOP,678,52,90,29,22);
    unison=control(L"BUTTON",L"Unison",BS_AUTOCHECKBOX|WS_TABSTOP,790,53,90,28,25);
    control(L"STATIC",L"C-2 to C-1 (MIDI 0-12): select lines 1-13. DAW Play restarts the selected line.",0,24,94,852,24,0);
    const int firstRow[]={SpeechRate,Shift,Resonance,Breath,Buzz,Artic,Release};
    const int secondRow[]={Image,Detune,Voices,ExtGain,MasterVolume};
    for(int row=0;row<2;++row){int n=row ? 5:7;const int* indices=row ? secondRow:firstRow;
        for(int col=0;col<n;++col){int i=indices[col];knobs[i]=CreateWindowExW(0,knobClass,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,
            24+col*(row ? 170:120),row ? 242:126,row ? 158:110,110,window,reinterpret_cast<HMENU>(INT_PTR(2000+i)),module,this);}}
    control(L"STATIC",L"Lyrics - one line per keyswitch. Up to 13 lines, 128 characters per line.",0,24,367,852,24,0);
    text=control(L"EDIT",L"",ES_MULTILINE|ES_WANTRETURN|ES_AUTOHSCROLL|ES_AUTOVSCROLL|WS_HSCROLL|WS_VSCROLL|WS_TABSTOP,24,395,852,225,1000);
    SendMessageW(text,EM_SETLIMITTEXT,lineCount*256+lineCount*2,0);
    control(L"BUTTON",L"Select cursor line",WS_TABSTOP,24,632,275,32,26);
    control(L"STATIC",L"Blank lines count too. Press Enter for a new line.",0,320,638,556,24,0);
    status=control(L"STATIC",L"",0,24,685,852,26,0);
    seenRevision=0;refresh();SetTimer(window,1,80,nullptr);return CPluginView::attached(parent,type);
}
tresult PLUGIN_API View::removed() {
    if(dragging>=0){owner.gestureEnd(dragging);dragging=-1;}
    if(window){KillTimer(window,1);DestroyWindow(window);window=nullptr;if(--attachedViews==0){UnregisterClassW(editorClass,module);UnregisterClassW(knobClass,module);}}
    if(font){DeleteObject(font);font=nullptr;}return CPluginView::removed();
}
void View::refresh() {
    if(!window)return;uint32_t version=owner.version();
    if(version!=seenRevision){
        loading=true;auto bank=owner.snapshot();auto joined=document(bank);std::string crlf;
        for(char c:joined){if(c=='\n')crlf+='\r';crlf+=c;}
        auto value=wide(crlf.c_str());int len=GetWindowTextLengthW(text);std::wstring current(size_t(len)+1,L'\0');GetWindowTextW(text,current.data(),len+1);current.resize(size_t(len));
        if(value!=current){DWORD first=0,last=0;SendMessageW(text,EM_GETSEL,reinterpret_cast<WPARAM>(&first),reinterpret_cast<LPARAM>(&last));SetWindowTextW(text,value.c_str());SendMessageW(text,EM_SETSEL,first,last);}
        SendMessageW(language,CB_SETCURSEL,bank.language-1,0);seenRevision=version;loading=false;
    }
    if(SendMessageW(linePick,CB_GETCURSEL,0,0)!=owner.currentLine())SendMessageW(linePick,CB_SETCURSEL,owner.currentLine(),0);
    auto check=[&](HWND h,int i){auto state=owner.control(i)>=.5 ? BST_CHECKED:BST_UNCHECKED;if(SendMessageW(h,BM_GETCHECK,0,0)!=state)SendMessageW(h,BM_SETCHECK,state,0);};
    check(unison,Unison);check(external,ExtInput);
    int range=int(std::round(owner.control(PitchBendRange)*10));if(SendMessageW(bendRange,CB_GETCURSEL,0,0)!=range)SendMessageW(bendRange,CB_SETCURSEL,range,0);
    for(int i=0;i<controlCount;++i)if(knobs[i]){double value=owner.control(i);if(value!=lastDrawn[i]){lastDrawn[i]=value;InvalidateRect(knobs[i],nullptr,FALSE);}}

}
void View::command(int id,int notification) {
    if(loading)return;
    if(id==20 && notification==CBN_SELCHANGE){owner.setLanguage(int(SendMessageW(language,CB_GETCURSEL,0,0))+1);SetWindowTextW(status,L"Language changed. Original text preserved.");}
    else if(id==21 && notification==CBN_SELCHANGE)owner.pick(int(SendMessageW(linePick,CB_GETCURSEL,0,0)));
    else if(id==22)owner.pick(owner.currentLine());
    else if(id==23)owner.pick((owner.currentLine()+lineCount-1)%lineCount);
    else if(id==24)owner.pick((owner.currentLine()+1)%lineCount);
    else if(id==25 || id==28){int i=id==25 ? Unison:ExtInput;HWND h=id==25 ? unison:external;owner.gestureBegin(i);owner.gestureValue(i,SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED ? 1:0);owner.gestureEnd(i);}
    else if(id==27 && notification==CBN_SELCHANGE){owner.gestureBegin(PitchBendRange);owner.gestureValue(PitchBendRange,SendMessageW(bendRange,CB_GETCURSEL,0,0)/10.0);owner.gestureEnd(PitchBendRange);}
    else if(id==26){int row=int(SendMessageW(text,EM_LINEFROMCHAR,WPARAM(-1),0));if(row>=0 && row<lineCount)owner.pick(row);}
    else if(id==1000 && notification==EN_CHANGE){
        int len=GetWindowTextLengthW(text);std::wstring input(size_t(len)+1,L'\0');GetWindowTextW(text,input.data(),len+1);
        int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.c_str(),-1,nullptr,0,nullptr,nullptr);std::string utf8(size_t(n>0 ? n:1),'\0');
        if(n)WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.c_str(),-1,utf8.data(),n,nullptr,nullptr);
        if(n && owner.setDocument(std::string_view(utf8.data(),size_t(n-1)))){SetWindowTextW(status,L"Lyrics saved in plugin state.");seenRevision=owner.version();}
        else SetWindowTextW(status,L"Not saved yet: limit is 13 lines, 128 characters per line. Please shorten the text.");
    }
}
#endif
}

bool InitModule(){return true;}
bool DeinitModule(){return true;}
BEGIN_FACTORY_DEF("", "https://github.com/alexknip-producer/JVOICE", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(voice::classID),PClassInfo::kManyInstances,kVstAudioEffectClass,
    "JVoice",0,"Instrument|Synth", "0.3.1",kVstVersionString,voice::Plugin::create)
END_FACTORY
