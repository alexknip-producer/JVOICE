/* SPDX-License-Identifier: GPL-3.0-only */
#include "TextBank.h"
#include "VoiceEngine.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace voice {
static const FUID classID(0x82B9CB47,0xC243412D,0x8F8B35EA,0x3DE56001);
class Plugin;
#ifdef _WIN32
class View : public CPluginView {
    Plugin& owner;
    HWND window = nullptr, language = nullptr, linePick = nullptr, status = nullptr;
    HWND edits[16]{};
    HFONT font = nullptr;
    bool loading = false;
    uint32_t seenRevision = 0;
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
    void refresh();
    void command(int id, int notification);
public:
    explicit View(Plugin&);
    ~View() override;
    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override { return std::strcmp(type,kPlatformTypeHWND)==0 ? kResultTrue : kResultFalse; }
    tresult PLUGIN_API attached(void*,FIDString) override;
    tresult PLUGIN_API removed() override;
};
#endif

class Plugin : public SingleComponentEffect, public IMidiMapping {
public:
    static FUnknown* create(void*) { return static_cast<IComponent*>(new Plugin); }
    Plugin() : engine(jv_create()) {
        makeLine(u8"Привет, мир",4,bank.lines[0]);
        makeLine(u8"Пой со мной",4,bank.lines[1]);
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
        addAudioOutput(STR16("Stereo output"),SpeakerArr::kStereo);
        addEventInput(STR16("MIDI input"),16);
        parameters.addParameter(STR16("Text line"),nullptr,15,0,ParameterInfo::kCanAutomate,1);
        for(int ch=0;ch<16;++ch) for(int cc : {120,123})
            parameters.addParameter(STR16("MIDI panic"),nullptr,0,0,ParameterInfo::kIsHidden,100+ch*128+cc);
        return kResultOk;
    }
    tresult PLUGIN_API getMidiControllerAssignment(int32 bus,int16 channel,CtrlNumber cc,ParamID& id) override {
        if(bus!=0 || channel<0 || channel>=16 || (cc!=120 && cc!=123))return kResultFalse;
        id=100+channel*128+cc;return kResultOk;
    }
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32 ni,SpeakerArrangement* out,int32 no) override {
        return ni==0 && no==1 && out && out[0]==SpeakerArr::kStereo ? kResultOk : kResultFalse;
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) override {
        return size==kSample32 || size==kSample64 ? kResultTrue : kResultFalse;
    }
    tresult PLUGIN_API setupProcessing(ProcessSetup& s) override {
        if(s.sampleRate<8000 || s.sampleRate>384000)return kInvalidArgument;
        return SingleComponentEffect::setupProcessing(s);
    }
    tresult PLUGIN_API setActive(TBool active) override {
        if(active) {
            jv_reset(engine);queueIndex=JV_BLOCK;phase=1;previous=next=0;held.fill(0);
            appliedRevision=0;syncBank();selectLine(selected.load());
        } else jv_panic(engine);
        return kResultOk;
    }
    tresult PLUGIN_API setProcessing(TBool state) override {
        if(!state){jv_panic(engine);queueIndex=JV_BLOCK;phase=1;previous=next=0;held.fill(0);}
        return kResultOk;
    }
    uint32 PLUGIN_API getTailSamples() override { return uint32(processSetup.sampleRate*0.4); }
    tresult PLUGIN_API process(ProcessData& data) override {
        syncBank();
        int request=requestedLine.exchange(-1);
        if(request>=0)selectLine(request);
        if(data.numOutputs<1 || !data.outputs || data.outputs[0].numChannels<2)return kResultOk;
        auto* events=data.inputEvents; int count=events ? events->getEventCount():0, evIndex=0;
        Event ev{}; bool has=events && count>0 && events->getEvent(0,ev)==kResultOk;
        // MIDI panic and host line automation are applied at the start of their processing block.
        if(data.inputParameterChanges) for(int i=0;i<data.inputParameterChanges->getParameterCount();++i) {
            auto* q=data.inputParameterChanges->getParameterData(i); if(!q)continue;
            ParamValue value=0;int32 offset=0,n=q->getPointCount();
            if(n && q->getPoint(n-1,offset,value)==kResultOk) {
                if(q->getParameterId()==1)selectLine(std::clamp(int(value*15+0.5),0,15));
                else if(q->getParameterId()>=100) {jv_panic(engine);held.fill(0);}
            }
        }
        for(int sample=0;sample<data.numSamples;++sample) {
            bool chord=false;
            while(has && ev.sampleOffset<=sample) {
                if(ev.busIndex==0) {
                    if(ev.type==Event::kNoteOnEvent) {
                        auto& n=ev.noteOn;
                        if(n.pitch<16 && n.pitch>=0)selectLine(n.pitch);
                        else if(n.pitch<128 && n.pitch>=16 && n.channel>=0 && n.channel<16) {
                            if(rt.lines[rt.selected].sung[0]) {
                                int velocity=std::clamp(int(n.velocity*127+0.5f),1,127);
                                jv_note_on(engine,n.channel,n.pitch,velocity,chord,n.noteId);
                                chord=true;
                            }
                        }
                    } else if(ev.type==Event::kNoteOffEvent) {
                        auto& n=ev.noteOff;
                        if(n.pitch>=16 && n.pitch<128)jv_note_off(engine,n.channel,n.pitch,n.noteId);
                    }
                }
                ++evIndex;has=evIndex<count && events->getEvent(evIndex,ev)==kResultOk;
            }
            while(phase>=1.0) {
                previous=next;
                if(queueIndex>=JV_BLOCK){jv_render(engine,queue);queueIndex=0;}
                next=queue[queueIndex++];phase-=1;
            }
            float value=float(previous+(next-previous)*phase);
            phase+=44100.0/processSetup.sampleRate;
            if(data.symbolicSampleSize==kSample32) {
                data.outputs[0].channelBuffers32[0][sample]=value;
                data.outputs[0].channelBuffers32[1][sample]=value;
            } else {
                data.outputs[0].channelBuffers64[0][sample]=value;
                data.outputs[0].channelBuffers64[1][sample]=value;
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
        if(row<0||row>=16)return false;
        std::lock_guard<std::mutex> lock(mutex);
        Line line;
        if(!makeLine(text,lang<0 ? bank.language:lang,line))return false;
        bank.lines[row]=line;++revision;return true;
    }
    bool setLanguage(int lang) {
        if(lang<1||lang>9)return false;
        std::lock_guard<std::mutex> lock(mutex);Bank copy=bank;
        for(int i=0;i<16;++i)if(!makeLine(bank.lines[i].original.data(),lang,copy.lines[i]))return false;
        copy.language=lang;bank=copy;++revision;return true;
    }
    Bank snapshot() {std::lock_guard<std::mutex> lock(mutex);Bank copy=bank;copy.selected=selected.load();return copy;}
    void pick(int line) {
        if(line<0||line>15)return;
        {std::lock_guard<std::mutex> lock(mutex);bank.selected=line;++revision;}
        selected.store(line);
        requestedLine.store(line);
        beginEdit(1);setParamNormalized(1,line/15.0);performEdit(1,line/15.0);endEdit(1);
    }
    uint32_t version() const {return revision.load();}
    int currentLine() const {return selected.load();}
    tresult PLUGIN_API getState(IBStream* stream) override {
        if(!stream)return kInvalidArgument;
        Bank copy=snapshot();
        const uint32_t magic=0x31564A54;
        auto write=[&](const void* p,int n){int32 done=0;return stream->write(const_cast<void*>(p),n,&done)==kResultOk && done==n;};
        uint32_t values[]={magic,1,uint32_t(copy.language),uint32_t(copy.selected)};
        if(!write(values,sizeof values))return kResultFalse;
        for(auto& l:copy.lines) {
            uint32_t len=uint32_t(std::strlen(l.original.data()));
            if(!write(&len,4)||!write(l.original.data(),int(len)))return kResultFalse;
        }
        return kResultOk;
    }
    tresult PLUGIN_API setState(IBStream* stream) override {
        if(!stream)return kInvalidArgument;
        Bank copy;
        auto read=[&](void* p,int n){int32 done=0;return stream->read(p,n,&done)==kResultOk && done==n;};
        uint32_t values[4]{};
        if(!read(values,sizeof values)||values[0]!=0x31564A54||values[1]!=1||values[2]<1||values[2]>9||values[3]>15)return kResultFalse;
        copy.language=int(values[2]);copy.selected=int(values[3]);
        for(auto& l:copy.lines) {
            uint32_t len=0;char text[textBytes]{};
            if(!read(&len,4)||len>=textBytes||!read(text,int(len))||!makeLine(std::string_view(text,len),copy.language,l))return kResultFalse;
        }
        {std::lock_guard<std::mutex> lock(mutex);bank=copy;++revision;}
        selected.store(copy.selected);
        requestedLine.store(copy.selected);
        setParamNormalized(1,copy.selected/15.0);
        return kResultOk;
    }
private:
    void syncBank() {
        uint32_t v=revision.load();if(v==appliedRevision)return;
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock)return;
        int old=rt.selected;
        bool changed=rt.language!=bank.language || rt.lines[old].sung!=bank.lines[old].sung;
        rt=bank;rt.selected=old;appliedRevision=revision.load();
        if(changed)selectLine(old);
    }
    void selectLine(int line) {
        rt.selected=std::clamp(line,0,15);selected.store(rt.selected);
        jv_text(engine,rt.lines[rt.selected].sung.data(),rt.language);
    }
    jv_engine* engine;
    Bank bank{},rt{};
    std::mutex mutex;
    std::atomic<uint32_t> revision{1};uint32_t appliedRevision=0;
    std::atomic<int> selected{0};
    std::atomic<int> requestedLine{-1};
    std::array<uint8_t,16> held{};
    float queue[JV_BLOCK]{},previous=0,next=0;int queueIndex=JV_BLOCK;double phase=1;
};

#ifdef _WIN32
static std::wstring wide(const char* text) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,nullptr,0);
    if(n<1)return {};
    std::wstring out(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,0,text,-1,out.data(),n);out.pop_back();return out;
}
View::View(Plugin& p):owner(p) {rect={0,0,760,685};owner.addRef();}
View::~View(){removed();owner.release();}
LRESULT CALLBACK View::proc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<View*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
        auto* cs=reinterpret_cast<CREATESTRUCTW*>(l);self=static_cast<View*>(cs->lpCreateParams);
        SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if(self && msg==WM_COMMAND){self->command(LOWORD(w),HIWORD(w));return 0;}
    if(self && msg==WM_TIMER){self->refresh();return 0;}
    return DefWindowProcW(h,msg,w,l);
}
tresult PLUGIN_API View::attached(void* parent,FIDString type) {
    if(!parent||isPlatformTypeSupported(type)!=kResultTrue)return kResultFalse;
    static const wchar_t* cls=L"JiantVoiceEditorV1";
    auto module=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=module;wc.lpszClassName=cls;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
    RegisterClassW(&wc);
    window=CreateWindowExW(0,cls,L"JIANT VOICE",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,760,685,
        static_cast<HWND>(parent),nullptr,module,this);
    if(!window)return kResultFalse;
    font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    auto control=[&](const wchar_t* c,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id) {
        HWND h=CreateWindowExW(c==std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE:0,c,text,WS_CHILD|WS_VISIBLE|style,
            x,y,width,height,window,reinterpret_cast<HMENU>(INT_PTR(id)),module,nullptr);
        SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;
    };
    control(L"STATIC",L"JIANT VOICE  /  preview V10.7.16  /  8 voices",0,18,12,710,24,0);
    control(L"STATIC",L"Language",0,18,47,85,24,0);
    language=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,105,43,105,260,20);
    for(auto* s:languageNames)SendMessageW(language,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(s).c_str()));
    control(L"STATIC",L"Selected line",0,245,47,120,24,0);
    linePick=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,365,43,90,420,21);
    for(int i=1;i<=16;++i)SendMessageW(linePick,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(std::to_wstring(i).c_str()));
    control(L"BUTTON",L"Restart line",WS_TABSTOP,490,43,125,28,22);
    control(L"STATIC",L"MIDI 0-15 selects a line. Notes 16-127 sing. Each new note advances one syllable.",0,18,82,720,24,0);
    for(int i=0;i<16;++i) {
        auto label=std::to_wstring(i+1)+L"  ["+std::to_wstring(i)+L"]";
        control(L"STATIC",label.c_str(),0,18,116+i*31,78,23,0);
        edits[i]=control(L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,100,112+i*31,640,26,1000+i);
        SendMessageW(edits[i],EM_SETLIMITTEXT,256,0); // Unicode scalar limit checked on every edit.
    }
    status=control(L"STATIC",L"",0,18,615,720,48,0);
    seenRevision=0;refresh();SetTimer(window,1,100,nullptr);
    return CPluginView::attached(parent,type);
}
tresult PLUGIN_API View::removed() {
    if(window){KillTimer(window,1);DestroyWindow(window);window=nullptr;}
    if(font){DeleteObject(font);font=nullptr;}
    return CPluginView::removed();
}
void View::refresh() {
    if(!window)return;
    auto v=owner.version();
    if(v!=seenRevision) {
        loading=true;auto b=owner.snapshot();
        for(int i=0;i<16;++i) {
            auto value=wide(b.lines[i].original.data());wchar_t current[514]{};
            GetWindowTextW(edits[i],current,514);
            if(value!=current)SetWindowTextW(edits[i],value.c_str());
        }
        SendMessageW(language,CB_SETCURSEL,b.language-1,0);seenRevision=v;loading=false;
    }
    SendMessageW(linePick,CB_SETCURSEL,owner.currentLine(),0);
}
void View::command(int id,int notification) {
    if(loading)return;
    if(id==20 && notification==CBN_SELCHANGE) {
        if(!owner.setLanguage(int(SendMessageW(language,CB_GETCURSEL,0,0))+1)) {
            SetWindowTextW(status,L"Some text cannot be read in this language. Keep RU for Cyrillic, or use Latin text.");
            seenRevision=0;refresh();
        } else SetWindowTextW(status,L"Language changed. Original text is stored in the DAW project.");
    } else if((id==21 && notification==CBN_SELCHANGE)||id==22) {
        owner.pick(int(SendMessageW(linePick,CB_GETCURSEL,0,0)));
    } else if(id>=1000 && id<1016 && notification==EN_CHANGE) {
        wchar_t text[514]{};GetWindowTextW(edits[id-1000],text,514);
        char utf8[2053]{};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,utf8,2053,nullptr,nullptr);
        if(n && owner.edit(id-1000,std::string_view(utf8,size_t(n-1)))) {
            SetWindowTextW(status,L"Saved in plugin state. 16 lines, up to 128 Unicode characters each.");
            seenRevision=owner.version();
        } else {
            SetWindowTextW(status,L"Limit: 128 characters; unsupported characters are rejected. Previous valid text kept.");
            seenRevision=0;refresh();
        }
    }
}
#endif
}

bool InitModule(){return true;}
bool DeinitModule(){return true;}
BEGIN_FACTORY_DEF("Voice Fork", "https://github.com/juanjiant-bit/JIANT", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(voice::classID),PClassInfo::kManyInstances,kVstAudioEffectClass,
    "JIANT Voice",0,"Instrument|Synth", "0.1.0",kVstVersionString,voice::Plugin::create)
END_FACTORY
