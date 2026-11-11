/* SPDX-License-Identifier: GPL-3.0-only
 * Russian mapping from preview web/editor.html, lyricText().
 * Kept separate so pronunciation can be adjusted without touching the DSP.
 * Both cases use these same replacements. Empty entries drop hard/soft signs.
 * RU overrides э -> eh in TextBank.h to distinguish it from е -> e.
 * Russian vowel targets are editable in RussianVowels.csv.
 * RU additionally preserves ь as an apostrophe and ъ as two apostrophes.
 */
#pragma once
#include <array>
namespace voice {
inline constexpr std::array<const char*, 33> russianLatin = {
    /* а */ "a",  /* б */ "b",   /* в */ "v",    /* г */ "g",
    /* д */ "d",  /* е */ "e",   /* ё */ "yo",   /* ж */ "zh",
    /* з */ "z",  /* и */ "i",   /* й */ "j",    /* к */ "k",
    /* л */ "l",  /* м */ "m",   /* н */ "n",    /* о */ "o",
    /* п */ "p",  /* р */ "r",   /* с */ "s",    /* т */ "t",
    /* у */ "u",  /* ф */ "f",   /* х */ "kh",   /* ц */ "ts",
    /* ч */ "ch", /* ш */ "sh",  /* щ */ "shch", /* ъ */ "",
    /* ы */ "y",  /* ь */ "",    /* э */ "e",    /* ю */ "yu",
    /* я */ "ya"
};
inline constexpr const char* languageNames[] = {
    "ES", "EN", "JA", "RU", "DE", "FR", "ZH", "PT", "GN"
};
}
