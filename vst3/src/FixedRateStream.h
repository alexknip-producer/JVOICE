/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "VoiceEngine.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace voice {
/* A 64-frame cushion lets the stereo input reach a whole native DSP block.
 * Linear interpolation in/out; no allocation, locks or external dependency. */
class FixedRateStream {
    float input[2][256]{},output[2][JV_BLOCK]{},previousInput[2]{},previous[2]{},next[2]{};
    int read=0,write=0,available=0,index=0;
    double ratio=1,inPhase=0,outPhase=1;
    bool first=true;
public:
    uint64_t nativeFrames=0;
    void reset(double rate) {
        *this=FixedRateStream{};ratio=JV_SAMPLE_RATE/rate;inPhase=1-ratio;
    }
    static uint32_t latency(double rate){return uint32_t(std::lround((rate==JV_SAMPLE_RATE ? JV_BLOCK:JV_BLOCK+1)*rate/JV_SAMPLE_RATE));}
    template<class Render> std::array<float,2> tick(float left,float right,Render&& render) {
        float current[2]={std::isfinite(left) ? std::clamp(left,-4.f,4.f):0,std::isfinite(right) ? std::clamp(right,-4.f,4.f):0};
        if(first){previousInput[0]=current[0];previousInput[1]=current[1];first=false;}
        inPhase+=ratio;
        while(inPhase>=1){
            double a=std::clamp(1-(inPhase-1)/ratio,0.0,1.0);
            for(int ch=0;ch<2;++ch)input[ch][write]=float(previousInput[ch]+(current[ch]-previousInput[ch])*a);
            write=(write+1)&255;++available;inPhase-=1;
        }
        previousInput[0]=current[0];previousInput[1]=current[1];
        auto take=[&](){
            if(index==JV_BLOCK){
                float l[JV_BLOCK]{},r[JV_BLOCK]{};
                for(int i=0;i<JV_BLOCK && available;++i){l[i]=input[0][read];r[i]=input[1][read];read=(read+1)&255;--available;}
                render(l,r,output[0],output[1]);nativeFrames+=JV_BLOCK;index=0;
            }
            return std::array<float,2>{output[0][index],output[1][index++]};
        };
        if(ratio==1)return take();
        while(outPhase>=1){previous[0]=next[0];previous[1]=next[1];auto v=take();next[0]=v[0];next[1]=v[1];outPhase-=1;}
        std::array<float,2> out{float(previous[0]+(next[0]-previous[0])*outPhase),float(previous[1]+(next[1]-previous[1])*outPhase)};
        outPhase+=ratio;return out;
    }
};
}
