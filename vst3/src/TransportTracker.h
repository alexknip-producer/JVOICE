/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include <cmath>
namespace voice {
class TransportTracker {
    bool known=false,playing=false;
    Steinberg::int64 expected=0;
public:
    void reset(){known=false;playing=false;expected=0;}
    bool observe(const Steinberg::Vst::ProcessContext* c,int frames){
        if(!c){known=false;return false;}
        bool now=(c->state & Steinberg::Vst::ProcessContext::kPlaying)!=0;
        bool jumped=known && std::abs(double(c->projectTimeSamples)-double(expected))>1;
        bool started=now && !playing;
        expected=c->projectTimeSamples+(now && frames>0 ? frames:0);known=true;playing=now;
        return jumped || started;
    }
};
}
