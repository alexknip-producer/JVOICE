/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "TransliterationTable.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace voice {
inline constexpr int lineCount = 13, storedLineCount = 16, maxCharacters = 128, textBytes = 513;
struct Line { std::array<char, textBytes> original{}, sung{}; std::array<uint16_t,textBytes> sourceStart{}, sourceEnd{}; };
struct Bank {
    std::array<Line, storedLineCount> lines{}; // retain old projects' extra rows in state
    int language = 5; // German (upstream enum)
    int selected = 0;
};
inline std::string document(const Bank& bank) {
    int last=lineCount-1;
    while(last>0 && !bank.lines[last].original[0])--last;
    std::string text;
    for(int i=0;i<=last;++i){if(i)text+='\n';text+=bank.lines[i].original.data();}
    return text;
}
inline bool decode(std::string_view s, std::u32string& out) {
    out.clear();
    for (size_t i = 0; i < s.size();) {
        uint32_t c = static_cast<unsigned char>(s[i++]);
        int n = 0;
        if (c >= 0xF0 && c <= 0xF4) { n = 3; c &= 7; }
        else if (c >= 0xE0 && c <= 0xEF) { n = 2; c &= 15; }
        else if (c >= 0xC2 && c <= 0xDF) { n = 1; c &= 31; }
        else if (c >= 128) return false;
        if (i + n > s.size()) return false;
        for (int j = 0; j < n; ++j) {
            auto b = static_cast<unsigned char>(s[i++]);
            if ((b & 0xC0) != 0x80) return false;
            c = (c << 6) | (b & 63);
        }
        if ((n == 1 && c < 0x80) || (n == 2 && c < 0x800) ||
            (n == 3 && c < 0x10000) || c > 0x10FFFF ||
            (c >= 0xD800 && c <= 0xDFFF) || c == 0 || c == '\r' || c == '\n') return false;
        out.push_back(static_cast<char32_t>(c));
        if (out.size() > maxCharacters) return false;
    }
    return true;
}
inline void replaceAll(std::string& s, const std::string& from, const std::string& to) {
    size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) { s.replace(p, from.size(), to); p += to.size(); }
}
inline bool makeLine(std::string_view utf8, int language, Line& line) {
    std::u32string chars;
    if (language < 1 || language > 9 || utf8.size() >= textBytes || !decode(utf8, chars)) return false;
    std::string latin;
    auto append = [&](char32_t c) {
        { // Transliteration is independent of the selected pronunciation language.
            if (c >= U'А' && c <= U'Я') c += 32;
            if (c == U'Ё') c = U'ё';
            if (language == 4 && c == U'ь') { latin += "'"; return; }
            if (language == 4 && c == U'ъ') { latin += "''"; return; }
            // RU needs a distinct vowel token; other languages keep their old transliteration.
            if (language == 4 && c == U'э') { latin += "eh"; return; }
            if (c == U'ё') { latin += russianLatin[6]; return; }
            if (c >= U'а' && c <= U'я') {
                int i = int(c - U'а'); if (c >= U'ж') ++i;
                latin += russianLatin[i]; return;
            }
        }
        if (c >= U'A' && c <= U'Z') c += 32;
        if (c >= U'À' && c <= U'Þ' && c != U'×') c += 32;
        if (c == U'’' || c == U'‘') c = U'\'';
        if (c == U'ñ') { latin += "ny"; return; }
        if (c == U'ç') { latin += 's'; return; }
        if (language == 5) {
            if (c == U'ä') { latin += "ae"; return; }
            if (c == U'ö') { latin += "oe"; return; }
            if (c == U'ü') { latin += "ue"; return; }
            if (c == U'ß') { latin += "ss"; return; }
        }
        if (language == 6) {
            if (c == U'é') { latin += "ei"; return; }
            if (c == U'è' || c == U'ê') { latin += "eh"; return; }
            if (c == U'œ' || c == U'Œ') { latin += "oe"; return; }
        }
        if (language == 7 && (c == U'ü' || (c >= U'ǖ' && c <= U'ǜ'))) { latin += 'v'; return; }
        if ((language == 8 || language == 9) && (c == U'ã' || c == U'õ' || c == U'ñ')) {
            latin += c == U'ã' ? "a~" : c == U'õ' ? "o~" : "n~"; return;
        }
        if (c == U'á' || c == U'à' || c == U'â' || c == U'ä' || c == U'ã' || c == U'å') c = 'a';
        if (c == U'é' || c == U'è' || c == U'ê' || c == U'ë') c = 'e';
        if (c == U'í' || c == U'ì' || c == U'î' || c == U'ï') c = 'i';
        if (c == U'ó' || c == U'ò' || c == U'ô' || c == U'ö' || c == U'õ') c = 'o';
        if (c == U'ú' || c == U'ù' || c == U'û' || c == U'ü') c = 'u';
        if (c >= 0x300 && c <= 0x36F) return;
        if (c == U'¡' || c == U'¿') return;
        if (c > 126) { latin+=' ';return; } // retain original Unicode; skip unsupported phonetics
        latin += c < 32 ? ' ' : char(c);
    };
    std::vector<std::pair<uint16_t,uint16_t>> source;
    uint16_t utf16=0;
    for(char32_t c:chars){
        size_t before=latin.size();append(c);
        uint16_t next=(uint16_t)(utf16+(c>0xffff ? 2:1));
        source.insert(source.end(),latin.size()-before,{utf16,next});utf16=next;
    }
    if (language == 6) {
        auto vowel = [](char c) { return std::string_view("aeiouy").find(c) != std::string_view::npos; };
        for (size_t i = 0; i < latin.size(); ++i) {
            for (auto pair : {std::pair{"ain", "e~"}, {"an", "a~"}, {"en", "a~"}, {"on", "o~"}, {"in", "e~"}}) {
                size_t n = std::strlen(pair.first);
                if (latin.compare(i,n,pair.first) == 0 && (i+n == latin.size() || !vowel(latin[i+n]))) {
                    auto span=std::make_pair(source[i].first,source[i+n-1].second);
                    source.erase(source.begin()+i,source.begin()+i+n);
                    source.insert(source.begin()+i,std::strlen(pair.second),span);
                    latin.replace(i,n,pair.second); ++i; break;
                }
            }
        }
    }
    if (latin.size() >= textBytes) return false;
    line = {};
    std::memcpy(line.original.data(), utf8.data(), utf8.size());
    std::memcpy(line.sung.data(), latin.data(), latin.size());
    for(size_t i=0;i<source.size();++i){line.sourceStart[i]=source[i].first;line.sourceEnd[i]=source[i].second;}
    return true;
}
}
