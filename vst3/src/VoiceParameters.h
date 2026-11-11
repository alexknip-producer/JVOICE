/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace voice {
enum Control { SpeechRate, Shift, Resonance, Breath, Buzz, Artic, Image, Detune, Unison, Voices, controlCount };
inline constexpr unsigned firstControlID = 10;
inline constexpr const char* controlNames[] = {"Speech Rate", "Shift", "Q", "Breath", "Buzz", "Artic", "Image", "Detune", "Unison", "Voices"};
inline constexpr std::array<double,controlCount> controlDefaults = {32.0/127, .5, 72.0/127, 6.0/127, 84.0/127, 1, 0, 0, 0, 1};
inline double plainControl(int i,double n) {
    n=std::clamp(n,0.0,1.0);
    if(i==SpeechRate){double rank=n*127;return rank<32 ? 50+rank*50/32 : 100+(rank-32)*300/95;}
    if(i==Detune)return n*40;
    if(i==Unison)return n>=.5 ? 1 : 0;
    if(i==Voices)return 1+std::round(n*7);
    return i==Shift ? n*24-12 : n*100;
}
inline double normalizedControl(int i,double p) {
    if(i==SpeechRate)return std::clamp((p<100 ? (p-50)*32/50 : 32+(p-100)*95/300)/127,0.0,1.0);
    if(i==Detune)return std::clamp(p/40,0.0,1.0);
    if(i==Unison)return p>=.5 ? 1 : 0;
    if(i==Voices)return std::clamp((p-1)/7,0.0,1.0);
    return std::clamp(i==Shift ? (p+12)/24 : p/100,0.0,1.0);
}
}
