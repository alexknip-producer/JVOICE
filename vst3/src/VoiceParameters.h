/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace voice {
// Slot 11 is retired; preserve subsequent parameter IDs and saved-state offsets.
enum Control { SpeechRate, Shift, Resonance, Breath, Buzz, Artic, Image, Detune, Unison, Voices, ExtInput, ReservedControl, Release, PitchBendRange, MasterVolume, ExtGain, ModDepth, ModRate, Mono, Smear, Newspeak, PhonemeGlide, Thickness, controlCount };
inline constexpr unsigned firstControlID = 10;
inline constexpr const char* controlNames[] = {"Speech Rate", "Shift", "Q", "Breath", "Buzz", "Artic", "Image", "Detune", "Unison", "Voices", "Ext. input", "", "Release", "Pitch Bend Range", "Master Volume", "Ext. Input Gain", "Mod Depth", "Mod Rate", "Mono", "Smear", "Newspeak", "Phoneme Glide", "Thickness"};
inline constexpr std::array<double,controlCount> controlDefaults = {32.0/127, .5, 72.0/127, .05, 1, 1, 0, 0, 0, 1, 0, 0, 72.0/127, 0, 60.0/72, .5, .5, 49.0/119, 0, .5, 0, .5, .5};
inline double plainControl(int i,double n) {
    n=std::clamp(n,0.0,1.0);
    if(i==SpeechRate){double rank=n*127;return rank<32 ? 50+rank*50/32 : 100+(rank-32)*500/95;}
    if(i==Detune)return n*40;
    if(i==MasterVolume)return n*72-60;
    if(i==ExtGain)return n*48-24;
    if(i==ModDepth)return n*50;
    if(i==ModRate)return .1+n*11.9;
    if(i==Smear)return n*200-100;
    if(i==PhonemeGlide)return 100*std::pow(n<=.5 ? 4.0:10.0,2*n-1);
    if(i==Thickness)return 100*std::pow(16.0,n-.5);
    if(i==Unison || i==ExtInput || i==Mono || i==Newspeak)return n>=.5 ? 1 : 0;
    if(i==Release)return std::pow(10000.0,n);
    if(i==PitchBendRange)return 2+std::round(n*10);
    if(i==Voices)return 1+std::round(n*7);
    return i==Shift ? n*24-12 : n*100;
}
inline double normalizedControl(int i,double p) {
    if(i==SpeechRate)return std::clamp((p<100 ? (p-50)*32/50 : 32+(p-100)*95/500)/127,0.0,1.0);
    if(i==Detune)return std::clamp(p/40,0.0,1.0);
    if(i==MasterVolume)return std::clamp((p+60)/72,0.0,1.0);
    if(i==ExtGain)return std::clamp((p+24)/48,0.0,1.0);
    if(i==ModDepth)return std::clamp(p/50,0.0,1.0);
    if(i==ModRate)return std::clamp((p-.1)/11.9,0.0,1.0);
    if(i==Smear)return std::clamp((p+100)/200,0.0,1.0);
    if(i==PhonemeGlide)return std::clamp(.5+.5*std::log(std::max(p,25.0)/100)/std::log(p<=100 ? 4.0:10.0),0.0,1.0);
    if(i==Thickness)return std::clamp(.5+std::log(std::max(p,25.0)/100)/std::log(16.0),0.0,1.0);
    if(i==Unison || i==ExtInput || i==Mono || i==Newspeak)return p>=.5 ? 1 : 0;
    if(i==Release)return std::clamp(std::log(std::max(p,1.0))/std::log(10000.0),0.0,1.0);
    if(i==PitchBendRange)return std::clamp((p-2)/10,0.0,1.0);
    if(i==Voices)return std::clamp((p-1)/7,0.0,1.0);
    return std::clamp(i==Shift ? (p+12)/24 : p/100,0.0,1.0);
}
}
