/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "TransliterationTable.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace voice {
inline constexpr int lineCount = 13, storedLineCount = 16, maxCharacters = 128, textBytes = 513;
struct Line { std::array<char, textBytes> original{}, sung{}; };
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
    for (char32_t c : chars) {
        { // Transliteration is independent of the selected pronunciation language.
            if (c >= U'А' && c <= U'Я') c += 32;
            if (c == U'Ё') c = U'ё';
            if (language == 4 && c == U'ь') { latin += "'"; continue; }
            if (language == 4 && c == U'ъ') { latin += "''"; continue; }
            // RU needs a distinct vowel token; other languages keep their old transliteration.
            if (language == 4 && c == U'э') { latin += "eh"; continue; }
            if (c == U'ё') { latin += russianLatin[6]; continue; }
            if (c >= U'а' && c <= U'я') {
                int i = int(c - U'а'); if (c >= U'ж') ++i;
                latin += russianLatin[i]; continue;
            }
        }
        if (c >= U'A' && c <= U'Z') c += 32;
        if (c >= U'À' && c <= U'Þ' && c != U'×') c += 32;
        if (c == U'’' || c == U'‘') c = U'\'';
        if (c == U'ñ') { latin += "ny"; continue; }
        if (c == U'ç') { latin += 's'; continue; }
        if (language == 5) {
            if (c == U'ä') { latin += "ae"; continue; }
            if (c == U'ö') { latin += "oe"; continue; }
            if (c == U'ü') { latin += "ue"; continue; }
            if (c == U'ß') { latin += "ss"; continue; }
        }
        if (language == 6) {
            if (c == U'é') { latin += "ei"; continue; }
            if (c == U'è' || c == U'ê') { latin += "eh"; continue; }
            if (c == U'œ' || c == U'Œ') { latin += "oe"; continue; }
        }
        if (language == 7 && (c == U'ü' || (c >= U'ǖ' && c <= U'ǜ'))) { latin += 'v'; continue; }
        if ((language == 8 || language == 9) && (c == U'ã' || c == U'õ' || c == U'ñ')) {
            latin += c == U'ã' ? "a~" : c == U'õ' ? "o~" : "n~"; continue;
        }
        if (c == U'á' || c == U'à' || c == U'â' || c == U'ä' || c == U'ã' || c == U'å') c = 'a';
        if (c == U'é' || c == U'è' || c == U'ê' || c == U'ë') c = 'e';
        if (c == U'í' || c == U'ì' || c == U'î' || c == U'ï') c = 'i';
        if (c == U'ó' || c == U'ò' || c == U'ô' || c == U'ö' || c == U'õ') c = 'o';
        if (c == U'ú' || c == U'ù' || c == U'û' || c == U'ü') c = 'u';
        if (c >= 0x300 && c <= 0x36F) continue;
        if (c == U'¡' || c == U'¿') continue;
        if (c > 126) { latin+=' ';continue; } // retain original Unicode; skip unsupported phonetics
        latin += c < 32 ? ' ' : char(c);
    }
    if (language == 6) {
        auto vowel = [](char c) { return std::string_view("aeiouy").find(c) != std::string_view::npos; };
        for (size_t i = 0; i < latin.size(); ++i) {
            for (auto pair : {std::pair{"ain", "e~"}, {"an", "a~"}, {"en", "a~"}, {"on", "o~"}, {"in", "e~"}}) {
                size_t n = std::strlen(pair.first);
                if (latin.compare(i,n,pair.first) == 0 && (i+n == latin.size() || !vowel(latin[i+n]))) {
                    latin.replace(i,n,pair.second); ++i; break;
                }
            }
        }
    }
    if (latin.size() >= textBytes) return false;
    line = {};
    std::memcpy(line.original.data(), utf8.data(), utf8.size());
    std::memcpy(line.sung.data(), latin.data(), latin.size());
    return true;
}
}
