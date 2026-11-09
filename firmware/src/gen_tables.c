/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT (0.6.91) the song generator's knowledge, as constant tables (gen.c reads them; included by it). Everything a
 * genre knows is written here, nothing is computed: a genre is a row, its drums a map of priorities (after Mutable
 * Instruments' Grids: a level a step a lane, a hit where the level clears the density), its bass one of ~50 archetypes
 * written against the kick (funk, post punk, jazz, reggae, house, techno, hip hop ..), its chords a progression with a
 * quality a chord, its arp and lead a way of speaking, its sounds a recipe a role, its kit a kit, its space a reverb and
 * a delay. The research behind the tables: reports/ and research_notes/ (practitioner conventions, Grids, Marbles, the
 * FiloBass corpus, Witek on syncopation, Attack's beat dissections). */

/* ---------------------------------------------------------------------------------------------------- roles --- */
enum { GP_DRUMS, GP_BASS, GP_CHORDS, GP_ARP, GP_LEAD, GP_N };   /* the parts the GEN screen weighs */
#define GA_AUTO 255u                                    /* an amount left to the genre */

/* ------------------------------------------------------------------------------------------------ drum maps --- */
/* a lane's 16 steps as levels: '.' 0, '1'..'9' 26 a step (9 = 234), 'X' 255. A step hits when its level > 255 - density;
 * '9' and 'X' are anchors (accented, never dropped by a variation); '1'..'3' ghosts. Lanes: BD SD CP CH OH TM RS CB */
/* (V10.6n) the maps in pairs (a map and its neighbour, index ^ 1: SEQ + SELECT's retries), the club's four on the floor
 * first: HOUSE .. TECHNO take the club's stabs (gen.c gclub), HOUSE .. HBASS the pools' top end (DM_TRIBAL on: their own) */
enum { DM_HOUSE, DM_DEEP, DM_TECHHOUSE, DM_TECHNO, DM_HARD, DM_HBASS, DM_CUMBIA, DM_TRIBAL, DM_REGG, DM_DEMBOW,
       DM_PHONK, DM_PHONK2, DM_TRAP, DM_DRILL, DM_BAILE, DM_BAILE2, DM_ELECTRO, DM_BREAKS, DM_AMEN, DM_DNB, DM_EBM, DM_EBM2,
       DM_EBM3, DM_EBM4, DM_METAL, DM_TROCK, DM_ROCK, DM_SYN80, DM_BOC, DM_DREAM, DM_BOOMBAP, DM_JAZZ, DM_MOTORIK, DM_SPARSE,
       DM_CUMB2, DM_CUMB3, DM_REGG2, DM_DEMB2, DM_EBM5, DM_FUNKY, DM_THINK, DM_NONE };   /* (V10.7.2: the latin throws' maps and
                                                         * V10.6z10's FUNKY, THINK (the IDM throws' other breaks: gen.c
                                                         * gen_pick) past DM_SPARSE, so SEQ + SELECT's dice and their pairs
                                                         * stay as they were) */   /* (V10.6w: HIP HOP's trap and drill by the phonk's (the hats' rolls), TECHNO ROCK's rock and synth
                                                         * 80s by its own, JAZZ beside boom bap) */
/* (0.9.7, flash) the maps and the pools below are the text tools/gen_drum_maps.py reads (not compiled): build/gen/
 * gen_drum_maps.h holds them packed, each lane once at 16 levels a nibble (GM_LANE), the tables as lane numbers
 * (DRUM_MAP_I, HAT_POOL_I ..: gen.c gm_level) */
#ifdef GEN_DRUM_MAPS_TEXT
static const char *const DRUM_MAP[][8] = {
    [DM_HOUSE]     = {"X...8...X...8...", "", "....9.......9...", "7.3.7.3.7.3.7.3.", "..8...8...8...8.", "", ".......2......3.", ""},
    [DM_DEEP]      = {"X...8...X...8..2", "", "....7.......7...", ".37..37..37..37.", "..8...8...8...8.", "", "...2......4.....", ""},
    [DM_TECHHOUSE] = {"X...X...X...X...", "", "....X.......X...", "7.57.57.7.57.57.", "..8...8...8...8.", "", "...5..5....5..5.", ""},
    [DM_TECHNO]    = {"X...9...X...9...", "", "....6.......6...", "..8...8...8...8.", "", "", "......5.....5..2", "...3.......3...."},
    [DM_HARD]      = {"X...X...X...X...", "", "....7.......7...", "8484848484848484", "..7...7...7...7.", "", "..3..3....3..3..", "......3.......3."},
    /* (V10.6n) Russian hard bass (XS Project, Hard Bass School): a hard four on the floor, the clap on 2 and 4, the open
     * hat on every offbeat (the bass bounces there too), hard eighths */
    [DM_HBASS]     = {"X...X...X...X...", "", "....X.......X...", "8.6.8.6.8.6.8.6.", "..9...9...9...9.", "", "", ""},
    /* (V10.6n) cumbia (Damas Gratis, Chancha Via Circuito; V10.6u the owner's two demos, 77 BPM): the guiro's scrape
     * on the "e" and the "and" of every beat (V10.7.2: the owner's demos measured, .XX. a beat, the "e" accented; was
     * three a beat; V10.7.5 measured again, onsets a band: above 3 kHz nothing on the beat or its "a" in either demo,
     * so the sonidera keeps the demos' phase, its throws the guiro on the beat). (V10.7.5, the owner's cumbia
     * reference, the rules after Compas (TexasRADIO, GPL-3.0), written here as JIANT's own) the sonidera's kick on 1 and
     * a lighter 3 (was every beat), the llamador (the clap's lane) on the "and"s accented on 2's and 4's, the tumba on
     * the "a"s accented on 2's and 4's (3, 7!, 11, 15!), the rim on 4, the bell on 2 */
    /* (V10.7.8, the owner: "the guiro more syncopated, a guiro, not a short hat") the guiro (DRUM-X's filter GR: an
     * accent its long stroke, the rest its "chk"s, gen.c gen_kit) long on the "e", short on the "and", and on 2's and
     * 4's "a" a short one more, into the next beat: "raaa-ca", "raaa-ca-ca" */
    [DM_CUMBIA]    = {"X.......8.......", "", "..4...9...4...9.", ".96..965.96..965", "", "...4...9...4...9", "............7...", "....7..........."},
    /* (V10.6n) the dembow riddim: the kick on every beat, the snare 3 + 3 + 2 (the "a" of 1, the "and" of 2 ..);
     * Dominican dembow harder, the clap doubling it, sixteenths of guira, a tom's answer */
    [DM_REGG]      = {"X...X...X...X...", "...X..X....X..X.", "", "7.7.7.7.7.7.7.7.", "", ".............5..", "..........4.....", ""},
    [DM_DEMBOW]    = {"X...X...X...X...", "...X..X....X..X.", "...7..7....7..7.", "8585858585858585", "", "......5.....5.5.", "", ""},
    /* (V10.6n) tribal house / techno (Danny Tenaglia, Ben Sims, Joel Mull): the four on the floor under toms and congas,
     * a shaker, a rim, a cowbell */
    [DM_TRIBAL]    = {"X...X...X...X...", "", "....6.......6...", "5.75.75.5.75.75.", "", "..8..7.8..8.7..6", "...5..5....5..5.", "......4.......4."},
    /* (V10.6p, the owner: "completely off") phonk after Memphis: half time (130 .. 156: the kick on 1 and 3's "and", a
     * ghost before, the snare and the clap together on 3), eighths of hat (rolled in triplets: gen.c); its second map
     * (V10.6u, the owner's references: Brazilian phonk, 125 .. 135, in place of the drift's) the kick on the beat, the
     * funk's claps on 3 6 12 (the "tcha"s, a hat on them, rolled now and then). No drum cowbell: the cowbell is the
     * lead's (gen.c gen_riff). Baile funk (V10.6u, the owner's demos and ten funk loops measured): the tamborzao spread
     * over the kit, the 808 kick on 1 3 6 12, the claps (a bright hat on them) on 3 6 12, the atabaque's slaps on 8 and
     * 10; the mandelao's "tum tcha": the 808 on the one alone (a pickup), the same claps, the toms, a rim */
    [DM_PHONK]     = {"X......6..X.....", "........X.......", "........9.......", "7.7.7.7.7.7.7.7.", "", "", "", ""},
    [DM_PHONK2]    = {"X...X...X...X.8.", "", "...X..X.....X...", "...X..X.....X...", "", "", "", ""},
    /* (V10.6w, the owner: "all the hip hop in one") HIP HOP's throws (gen.c gen_pick, GSUB): trap at 136 .. 150 in half
     * time (the kick on 1, 2's "a" and 3's "and", the snare and the clap on 3, sixteenth hats rolled before the snare
     * and the bar's end: the rolls below); UK drill (the snare on 3 and its pickup on 4's "and", the hats' 3 3 2 skips,
     * the kick sparse under a sliding 808) */
    [DM_TRAP]      = {"X......6..X.....", "........X.......", "........9.......", "8585858585858585", "", "", "", ""},
    [DM_DRILL]     = {"X.........8.....", "........X.....6.", "........9.......", "7..7..7.7..7..7.", "", "", "", ""},
    /* (V10.7.8, the owner: "the snare and clap hits into TOM SHOTS and COWBELLS; ultra-minimalist, weird, aggressive")
     * the "tum tcha"s on 3 6 12 two toms now (gen.c gen_kit: the tom's lane a low tom shot, the clap's a high one, driven),
     * the cowbell's answer, no hats, no claps */
    [DM_BAILE]     = {"X..7..7.....6...", "", "......X.........", "", "", "...X........X...", "", "..........7...8."},
    [DM_BAILE2]    = {"X.............5.", "", "......X.........", "", "", "...X........X...", "..............5.", "........7.7....."},
    [DM_ELECTRO]   = {"X.....8...8..7..", "....X.......X...", "....5.......5...", "7474747474747474", "", "", "", "6..6..6...6.6..."},
    [DM_BREAKS]    = {"X.6.......8.....", "....X..3.3..X..3", "", "7.6.7.6.7.6.7.6.", "......5.......5.", "", "", ""},
    /* (V10.6n) the Amen: the kick on 1 and its "and", 3's "and" twice, the snare on 2 and 4 with its ghosts (IDM's
     * break to edit, gen.c gidm_edit) */
    [DM_AMEN]      = {"X.9.......9X....", "....X..5.6..X..6", "", "7.7.7.7.7.7.7.7.", "", "", "", ""},
    [DM_DNB]       = {"X.........8.....", "....X.......X..3", "", "6.5.6.5.6.5.6.5.", "", "", ".......3........", ""},
    [DM_EBM]       = {"X...X...X...X...", "....X.......X...", "", "9373937393739373", "", "", "", "..........6....."},
    /* (V10.6m, the owner: "EBM always makes the same patterns") EBM's four on the floor two more ways, a throw's own:
     * Nitzer Ebb's (the clap on the snare, the hats off the beat, a metal hit), DAF's (the clap alone on 2 and 4, driven
     * sixteenths, a rim); (V10.6u, Front 242's place) the owner's slow EBM: half time at 148 .. 156 (gen.c gen_pick; 77
     * felt), the kick on 1 and pushed, the snare on 3 alone, no hats, a metal hit, the sixteenths the bass's */
    [DM_EBM2]      = {"X...X...X...X...", "....X.......X...", "....7.......7...", "..9...9...9...9.", "", "", "", "..............5."},
    [DM_EBM3]      = {"X.....5...7.....", "........X.......", "", "", "", "", "", "............5..."},
    [DM_EBM4]      = {"X...X...X...X...", "", "....X.......X...", "..9...9...9...9.", "", "", "..5.......5.....", "...5.......5..5."},   /* (V10.6p:
                                                         * the techno-body's: Phase Fatale, Schwefelgelb, offbeat hats, metal) */
    /* (V10.6n) industrial metal (Rammstein, Ministry): the kick on 1, 1's "and", 3, the double kick's sixteenths at the
     * bar's end, the snare on 2 and 4, eighths; techno rock (the Prodigy, the Chemical Brothers): a live kit's backbeat */
    [DM_METAL]     = {"X.X...X.X.X.5555", "....X.......X...", "", "8.8.8.8.8.8.8.8.", "", "", "", ""},
    [DM_TROCK]     = {"X.....7.X.6.....", "....X.......X..3", "", "7.5.7.5.7.5.7.5.", "..............6.", "", "", ""},
    /* (V10.6w, the owner: "rock and synth 80s inside TECHNO ROCK") its throws: rock's backbeat (the kick on 1, 3 and 3's
     * "and", eighths, the crash on the fill's one); the 80s drum machine's (a Linn's four on the floor, the snare with a
     * clap on 2 and 4, sixteenths) */
    [DM_ROCK]      = {"X.......X.6.....", "....X.......X...", "", "8.6.8.6.8.6.8.6.", "", "", "", ""},
    [DM_SYN80]     = {"X...X...X...X...", "....X.......X...", "....6.......6...", "7474747474747474", "", "", "", ""},
    /* (V10.6n) Boards of Canada's hip hop break, late and sparse */
    [DM_BOC]       = {"X......6..8.....", "....X.......X...", "", "6.4.6.4.6.4.6.4.", "", "", "......3.........", ""},
    /* (V10.6n) dream pop's slow drum machine: a soft kick, the snare on 2 and 4, eighths */
    [DM_DREAM]     = {"X.......X.6.....", "....X.......X...", "", "6.4.6.4.6.4.6.4.", "", "", "", ""},
    /* (V10.6w) HIP HOP's boom bap (swung, the kick on 1, 2's "a", 3's "and", the backbeat, eighths; its lofi throw takes
     * the Boards of Canada break above); JAZZ in swung eighths (a step an eighth: gen.c's BPM the half of the felt
     * 116 .. 176, the bar two of jazz's): the kick feathered on every beat (its lane low: gen.c gen_kit), the hi-hat's
     * foot on 2 and 4, the ride's
     * "ding, ding-da ding" (2 and 4 accented, the "da" the swung "and"), the snare's comping the ghosts' (gen.c) */
    [DM_BOOMBAP]   = {"X......7..9.....", "....X.......X...", "", "7.5.7.5.7.5.7.5.", "..............6.", "", "", ""},
    [DM_JAZZ]      = {"6.6.6.6.6.6.6.6.", "", "", "..7...7...7...7.", "7.957.957.957.95", "", "", ""},
    /* (V10.6z10, the IDM throws' other breaks, gen.c gen_pick) the Funky Drummer's (the kick on 1, its "and", 3's "and"
     * and 4's "e", the backbeat with its ghosts, sixteenths opened on 4's "a"), Think's (the kick on 1 and pushed past 3,
     * the backbeat and its pickup, eighths) */
    [DM_FUNKY]     = {"X.8.......9..7..", "....X..3.3.3X..3", "", "7575757575757.75", ".............6..", "", "", ""},
    [DM_THINK]     = {"X......7.8......", "....X.....3.X..3", "", "8.6.8.6.8.6.8.6.", "......6.........", "", "", ""},
    [DM_MOTORIK]   = {"X.6.6...8.6.6...", "....8.......8...", "", "7.5.7.5.7.5.7.5.", "..............3.", "", "", ""},
    [DM_SPARSE]    = {"7...............", "", "", "", "", "", "......3.....4...", "..........3....."},
    /* (V10.7.2, the owner: "cumbia, reggaeton and dembow very bad: each has very clear rules") the latin throws' own
     * maps (gen_tables.c GSUB_CU .. : a throw one of them). Cumbia villera (Damas Gratis, Pibes Chorros, Yerba Brava)
     * and digital cumbia (ZZK: Chancha Via Circuito, El Remolon): below (V10.7.5). Perreo (Tainy, the Bad Bunny
     * era): the dembow (the kick on every beat, the snare 3 + 3 + 2 on the "a" of 1, the "and" of 2, the "a" of 3, the
     * "and" of 4) with the clap on it, sixteenth hats (rolled: gen.c), a rim. Dominican
     * dembow rapido (El Alfa, Chimbala): the same riddim with the snare's extra hit on 3's "and", the clap, flat
     * sixteenths, the open hat off the beat, the toms' answer */
    /* (V10.7.5, after Compas (TexasRADIO, GPL-3.0)) villera: the kick on 1 and 3 (was every beat), the timbal on 4,
     * the guiro's chiqui-chiqui on the beat (X.xx..Xx: 1 and 3 accented), the conga on every "and" (2's and 4's
     * accented), the bell on 2 and 4; digital (tecnocumbia's): the kick on 1 and 3 and their pickups, the snare on 2
     * and 4, the guiro X.xx..X., the tumba on the "a"s (2's and 4's accented), the llamador a rim on the "and"s */
    /* (V10.7.8, the owner's cumbia RKT reference, 92 BPM: MC Caco's) villera as RKT: a short kick and the clap on every
     * quarter, the guiro's stroke on the beat and a "chk" on its "a" (pulled ~28 ms early: gen.c gen_drums, the
     * triplet's pull), the conga's on the "and"s, the bell on 2 and 4 */
    [DM_CUMB2]     = {"X...X...X...X...", "", "X...X...X...X...", "9..69..69..69..6", "", "..5...9...5...9.", "", "....7.......7..."},
    [DM_CUMB3]     = {"X.....5.X.....5.", "....8.......8...", "", "9.66..9.9.66..9.", "", "...4...9...4...9", "..4...7...4...7.", ""},
    [DM_REGG2]     = {"X...X...X...X...", "...X..X....X..X.", "...7..7....7..7.", "8.5.8.5.8.5.8.55", "", "", "..........4.....", ""},
    [DM_DEMB2]     = {"X...X...X...X...", "...X..X...7X..X.", "...7..7....7..7.", "6666666666666666", "..7...7...7...7.", "..6..6....6..6..", "", ""},
    /* (the owner's dark techno / industrial EBM reference, 117 BPM) EBM's fifth throw (gen_tables.c GSUB_EB): the kick
     * on every beat, the clap alone on 2 and 4, the hats off the beat with the reference's later sixteenths around
     * them (softer: .xxX..-x..-x...X), a metal hit; the sub's rumble a step after every kick (the bass), the riff the
     * lead's */
    [DM_EBM5]      = {"X...X...X...X...", "", "....X.......X...", ".397..93..93..97", "", "", "", "..............5."},
    [DM_NONE]      = {"", "", "", "", "", "", "", ""},
};
/* (0.6.97) a throw's own hats and percussion: a hat pattern, an open hat pattern and a percussion line from these pools
 * now and then instead of the map's, so a genre's throws differ in their top end */
static const char *const HAT_POOL[8] = {"7.7.7.7.7.7.7.7.", "7373737373737373", "..8...8...8...8.", "7.7.7.7.7.7.77.7",
                                        "7.37.37.37.37.3.", "8.5.8.5.8.5.8.55", "7..7..7.7..7..7.", "6666666666666666"};
static const char *const OH_POOL[6] = {"", "..7...7...7...7.", "......7.......7.", "..............7.", "..7.......7.....", "...7...7...7...7"};
static const char *const RS_POOL[8] = {"", "...5......5.....", ".4....4.4....4..", "......5.....5...", "..5...5...5...5.", "...3..3....3..3.",
                                        "..4..4..4..4..4.", "4.4..4.4.4..4.4."};
static const char *const CB_POOL[6] = {"", "5..5..5...5.5...", "......4.......4.", "2.2.2.2.2.2.2.2.", "4.....4....4....", ".4..4..4..4..4.."};
static const char *const TM_POOL[6] = {"", "..............44", "......5.......5.", "..6..6....6..6..", "5..5..5..5..5...", "..5....5....5..."};
/* (V10.6z10) the percussion pools' last two each a euclid (5 or 7 of 16, 3 of 16), turned: a throw's own cycle */
#endif
#include "gen_drum_maps.h"
/* the snare's ghost positions, the best first (the sixteenth before 2 and 4, then the "a"s, the "e"s, the "&"s) */
static const uint8_t GHOST_RANK[12] = {3, 7, 11, 15, 13, 5, 9, 1, 14, 6, 10, 2};
/* the hats' accent cells (percent of the hit's velocity on the four sixteenths of a beat) */
enum { HC_FLAT, HC_HOUSE, HC_FUNK, HC_GARAGE, HC_MACHINE };
static const uint8_t HAT_CELL[5][4] = {{100, 100, 100, 100}, {100, 55, 80, 55}, {90, 50, 70, 65}, {110, 40, 60, 90}, {100, 70, 100, 70}};
/* fills on the phrase's last bar */
enum { FL_NONE, FL_ROLL, FL_TOM, FL_DROP, FL_HAT, FL_LINEAR, FL_KICK16 };
/* (0.9.9-gen2: the 808's and the DUB kit's kick shorter, 22 -> 12, 18 -> 10) the kits: offsets on the factory kit's {PITCH PMOD DECAY NOISE COLOR} a lane (both patches), then the DRUM track's
 * TONE DECY NOIS DRV */
enum { KIT_DEF, KIT_909, KIT_808, KIT_CLICK, KIT_INDUS, KIT_SOFT, KIT_BREAKS, KIT_N };   /* (V10.6p: DUB's gone, no genre's) */
/* (V10.6w, the owner: "open the drums' brightness") the 808's snare, clap, hats and rim less darkened (COLOR -20 -> -6,
 * -10 -> -4), its TONE -10 -> -4 */
/* (V10.6z8, the owner: JAZZ's hats "a giant very low clap all the time") KIT_SOFT's hats were COLOR -30 and its TONE
 * -14 more: gen.c gen_kit's band-pass at ~2.5 kHz, a clap's. Now +28 (gen_kit halves it: +14, TONE's -14 made up): the
 * band at the top (~8 kHz) as every other kit's; its open hat (JAZZ's ride) longer and with more of its metal: a ping,
 * not a wash */
typedef struct { int8_t d[8][5]; int8_t tone, decy, nois, drv; } gkit_t;
static const gkit_t KIT[KIT_N] = {
    [KIT_DEF]    = {{{0}}, 0, 0, 0, 0},
    [KIT_909]    = {{{-2, 25, -6, 0, 25}, {0, 0, 6, 10, 10}, {0, 0, 4, 0, 0}, {4, 0, -4, 0, 6}, {4, 0, 0, 0, 6}, {0, 10, 0, 0, 10}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}}, 6, 0, 0, 6},
    [KIT_808]    = {{{-4, -20, 12, -4, -10}, {-4, -10, -8, 10, -6}, {-2, 0, 0, 0, -4}, {-6, 0, -6, 0, -6}, {-6, 0, -4, 0, -6}, {-4, -10, 10, 0, -10}, {0, 0, 0, 0, -4}, {0, 0, 4, 0, 0}}, -4, 6, 0, 0},
    [KIT_CLICK]  = {{{2, 10, -16, 0, 0}, {6, 0, -20, 20, 10}, {4, 0, -10, 0, 10}, {6, 0, -10, 0, 10}, {6, 0, -16, 0, 10}, {4, 0, -16, 0, 0}, {6, 0, -6, 0, 10}, {4, 0, -10, 0, 10}}, 10, -16, -6, 0},
    [KIT_INDUS]  = {{{-2, 30, 6, 12, 40}, {0, 20, 10, 20, 30}, {0, 0, 6, 10, 20}, {0, 0, 8, 10, 20}, {0, 0, 10, 10, 20}, {0, 20, 8, 10, 30}, {0, 10, 4, 10, 20}, {0, 0, 0, 10, 20}}, 12, 6, 10, 40},
    [KIT_SOFT]   = {{{0, -10, 6, 6, -25}, {-2, 0, 8, 14, -18}, {-2, 0, 6, 0, -20}, {-4, 0, 2, 0, 28}, {-4, 0, 16, -20, 28}, {-2, -10, 6, 6, -20}, {-2, 0, 4, 0, -20}, {-2, 0, 4, 0, -20}}, -20, 4, 4, 0},
    [KIT_BREAKS] = {{{2, 6, -4, 8, 10}, {2, 6, 2, 16, 20}, {0, 0, 2, 4, 10}, {0, 0, 4, 6, 0}, {0, 0, 8, 6, 0}, {2, 6, 0, 8, 10}, {0, 0, 0, 4, 10}, {0, 0, 0, 0, 0}}, 4, 0, 6, 10},
};

/* ----------------------------------------------------------------------------------------------- bass archetypes --- */
/* 16 steps a bar, a character a step: '.' rest, '=' the note before held on, 'R' root, 'r' root soft, 'x' a dead
 * note (the root, very soft and short), '2' '3' '5' '6' '7' '9' the chord's degrees (in the scale), 'L' the fifth
 * below, '8' the octave, 'A' an approach a semitone under the next chord's root (the fifth when the chord stays),
 * 'B' from a semitone above, 'N' the next chord's root early, 'S' a scale step towards the next root. The flags, a
 * character a step: 'a' accent, 's' slide into the next note, 'g' ghost (soft), '.' nothing. bars: 1, 2 or 4 */
enum { BA_OFFBEAT, BA_OFFOCT, BA_BOUNCE, BA_DEEPWALK, BA_ROLL16, BA_OCTBNC, BA_SYNTHW, BA_PSY, BA_PUMP8, BA_UKG,
       BA_TUMBAO, BA_BOSSA, BA_ROCK8, BA_PUNK16, BA_DUST, BA_MOTORIK, BA_ANGULAR, BA_JUNGLE, BA_DNB, BA_WOBBLE, BA_PEDAL,
       BA_FIFTHS, BA_NEOARP, BA_SEQ16, BA_ACID, BA_NITZER1, BA_NITZER2, BA_NITZER3, BA_DAF,
       BA_ROTHER, BA_VILLA, BA_GALLOP, BA_EBMOFF, BA_EBM2B, BA_CUMBIA, BA_HBASS, BA_REGG, BA_TAMBOR, BA_PH808, BA_WALK,
       BA_WALK2, BA_N,
       BA_RF1 = BA_N, BA_RF2, BA_RF3, BA_RF4, BA_FL1, BA_MANDEL, BA_CUMB2, BA_CUMB3, BA_RGTRES, BA_WALK3, BA_FUNK3, BA_MT1, BA_MT2, BA_MT3, BA_RUMB,
       BA_RF5, BA_RFE, BA_NALL };   /* (V10.7.2:
                                                         * the latin basses and V10.6z10's WALK3 (JAZZ's) after the last, the
                                                         * indices as they were) */   /* (V10.6p: the riffs past the basses: EXPLORE's
                                                         * basses never one; gen.c gen_riff) */
/* (0.9.7, flash) the archetypes below are the text tools/gen_drum_maps.py reads (not compiled): build/gen/gen_bass.h holds
 * them a byte a step (BA_STEP: the note's code | the flag's << 4; gen.c ba_note, ba_flag), each its first step and bars */
typedef struct { uint16_t at; uint8_t bars; } barch_t;
#ifdef GEN_DRUM_MAPS_TEXT
static const barch_t BASS_ARCH[BA_NALL] = {
    [BA_OFFBEAT]  = {"..R...R...R...R.", 0, 1},
    [BA_OFFOCT]   = {"..8...R...8...R.", 0, 1},
    [BA_BOUNCE]   = {"R.8.R.8.R.8.R.8A", "a...............", 1},
    [BA_DEEPWALK] = {"R..R..5..7.8..5R", "a.............s.", 1},
    [BA_ROLL16]   = {"RRRRRRRRRRRRRRRR", "..a...a...a...a.", 1},
    [BA_OCTBNC]   = {"R.8.R.8.R.8.R.85", 0, 1},
    [BA_SYNTHW]   = {"R585R585R585R585", 0, 1},
    [BA_PSY]      = {".RRR.RRR.RRR.RRR", 0, 1},   /* (V10.6n: hard techno's rumble too, the kick's tail between) */
    [BA_PUMP8]    = {"R=R=R=R=R=R=R=R=", 0, 1},
    [BA_UKG]      = {"R..R..5...R..7.N", "..............s.", 1},
    [BA_TUMBAO]   = {"R......5====N====......5====N===", 0, 2},
    [BA_BOSSA]    = {"R=====L.R=====L.", 0, 1},
    [BA_ROCK8]    = {"R.R.R.R.R.R.R.Sx", "a.......a......g", 1},
    [BA_PUNK16]   = {"RRRRRRRRRRRRRRRR", "a...a...a...a...", 1},
    [BA_DUST]     = {"R..R.R........78", 0, 1},
    [BA_MOTORIK]  = {"R.R.R.R.R.R.5.5.", 0, 1},
    [BA_ANGULAR]  = {"R..R..7..R..5..8", 0, 1},
    [BA_JUNGLE]   = {"R..R..R...R.R...", 0, 1},
    [BA_DNB]      = {"R=====..R==...7=", "..............s.", 1},
    [BA_WOBBLE]   = {"R=========R===5=", "..............s.", 1},
    [BA_PEDAL]    = {"R===============================================================", 0, 4},
    [BA_FIFTHS]   = {"R=======L=======", 0, 1},
    [BA_NEOARP]   = {"R==5==8===5===R=", 0, 1},
    [BA_SEQ16]    = {"R858R858R858R858", 0, 1},
    [BA_ACID]     = {0, 0, 1},                          /* (procedural: the 303's rules) */
    /* (0.6.95) EBM after Nitzer Ebb: relentless sixteenths on the root, the octave and the minor third thrown in, a
     * rest where the body hits, accents on the beat (Join in the Chant, Murderous, Let Your Body Learn) */
    [BA_DAF]      = {"RRRRRRR8RRRRRR8R", "a...a...a...a...", 1},   /* (gen6b) DAF, Nitzer Ebb: the 16ths never stop */
    [BA_ROTHER]   = {"R.8.7.R.R.8.7.R.", "a.......a.......", 1},   /* Anthony Rother: the electro answer */
    [BA_VILLA]    = {"..R..RR...R..R.8", "..a.......a.....", 1},   /* (gen6b) tech house's roller: off the kick */
    [BA_NITZER1]  = {"R.R8R.R8R.R3R8R8", "a...a...a...a...", 1},
    [BA_NITZER2]  = {"RR8RRR8RRR8RRR3RRR8RRR8RRR8RR2R8", "a...a...a...a...a...a...a...a...", 2},
    [BA_NITZER3]  = {"R.RR.R8.R.RR.R3.", "a.....a.a.....a.", 1},
    /* (V10.6m) EBM's sequences: Front 242's gallop (Headhunter), the body's offbeat sixteenths between the kicks, root
     * and octave, and eighths that turn up to the third and the fifth at the phrase's end (Welcome to Paradise) */
    [BA_GALLOP]   = {"R.RRR.RRR.RRR.RR", "a...a...a...a...", 1},
    [BA_EBMOFF]   = {".R.8.R.8.R.8.R.8", ".a...a...a...a..", 1},
    [BA_EBM2B]    = {"R.R.R.R.R.R.R.R.R.R.R.R.3.3.5.5.", "a.......a.......a.......a.......", 2},
    /* (V10.6n) cumbia's bass (V10.7.5, after Compas (TexasRADIO, GPL-3.0): the sonidera's tonica-quinta syncopated,
     * never a note every beat: the root on 1, the fifth under it anticipating 3 (held over the kick), the root on 3's
     * "and", the fifth on 4's "a"; the second bar closing on a fourth, the loop's turn back to the root; was the root
     * and the fifth on every beat, held); hard bass's
     * offbeat eighths bouncing under the kick, the riff's turn up the third and the fifth; reggaeton's 808 on the kick
     * and the dembow's 3 + 3 + 2 */
    [BA_CUMBIA]   = {"R==....L==R==..LR==....L==R==.4=", "a......a........a......a........", 2},
    [BA_HBASS]    = {"..R...R...R...R...R...R...3...5.", "..a...a...a...a...a...a...a...a.", 2},
    [BA_REGG]     = {"R..R..R.R..R..R.", "a.......a.....s.", 1},
    /* (V10.6p) baile funk's 808 on the tamborzao's kick, held; phonk's 808 (V10.6u: the Brazilian map's, its 3 3 2
     * accents), sliding up the octave and back; (V10.6u, past the riffs: EXPLORE's never) the mandelao's boom on the one */
    /* (V10.7.8, the owner: "more aggressive basses with lots of space") baile funk's 808 short and punchy on the
     * tamborzao's kick, its last note gliding up the octave; the mandelao's boom on the one and a slide at the bar's
     * end; a third, the boom and its answer pushed */
    [BA_TAMBOR]   = {"R..R..R.....R=8.", "a..a..a.....s...", 1},
    [BA_PH808]    = {"R==R==R=R===R=8=", "a..a..a.a...s.s.", 1},
    [BA_MANDEL]   = {"R=..........R=8.", "a...........s...", 1},
    [BA_FUNK3]    = {"R..R....R=8.....", "a..a....s.......", 1},
    /* (V10.7.2, past the riffs: EXPLORE's never; V10.7.5 after Compas) the tecnocumbia's bass (the root on 1, the
     * fifth on 2, the root on 2's "and" and on 3, the fifth on 4, the root on 4's "and"; the second bar's turn the
     * seventh) and villera's (the root on 1, the fifth on 2, the root on 2's "and" held over 3, the fifth on 4; the
     * second bar's turn a fourth);
     * reggaeton's 808 in the tresillo (3 + 3 + 2 eighths a bar, held: Despacito's, Tainy's) */
    [BA_CUMB2]    = {"R==.5=R=R==.5=R=R==.5=R=R==.5=7=", "a.....a.a.....a.a.....a.a.......", 2},
    [BA_CUMB3]    = {"R==.5=R====.5==.R==.5=R====.5=4=", "a.....a.........a.....a.........", 2},
    [BA_RGTRES]   = {"R=====R=====R===", "a.....a.....a...", 1},
    /* (V10.6w) JAZZ's walking bass in swung eighths (a step an eighth: a quarter note two steps, held): the chord's root,
     * third, fifth, seventh and octave, the fifth and third back down, a semitone into the next root from under; or the
     * scale's 2 and 3 up to the fifth, the octave, the seventh, the fifth, a step of the scale into the next root */
    [BA_WALK]     = {"R=3=5=7=8=5=3=A=", 0, 1},
    [BA_WALK2]    = {"R=2=3=5=8=7=5=S=", 0, 1},
    [BA_WALK3]    = {"R=8=7=5=3=5=6=S=", 0, 1},   /* (V10.6z10) or down from the octave, a step of the scale into the next root */
    /* (V10.6p) the leads' riffs (gen.c gen_riff), in the key's degrees from its tonic: R 2 .. 9 the mode's 1st .. 9th, A
     * the 7th under, S the 6th under, L the 5th under, N the 10th; 'a' an accent, 's' a slide. Phonk's 808 cowbell
     * (Kordhell's Murder in My Mind, DVRST's Close Eyes): the tonic as a pedal under a line in sixteenths, 3 + 3 + 2
     * accents; a 3 + 3 + 2 call and its answer; a broken minor chord falling to the b6; a sparse hook on the phrygian
     * b2 (baile funk's bells and automotivo's the same four). Baile funk's flute (Bum Bum Tam Tam: Bach's
     * A minor flute Partita, BWV 1013's Allemande): a broken chord climbing in sixteenths, its sequence falling to the
     * leading tone */
    [BA_RF1]      = {"R3R4R5R4R3R2R3RA", "a..a..a.a..a..a.", 1},
    [BA_RF2]      = {"R..R..3.R..R..5.4..4..3.2..2..A.", "a..a..a.a..a..a.a..a..a.a..a..a.", 2},
    [BA_RF3]      = {"8535853585358536", "a...a...a...a...", 1},
    [BA_RF4]      = {"R.R..R.2.R..3.2.", "a....a...a..a...", 1},
    [BA_FL1]      = {"LR35853R23423AR=535875423R2AR=..", "a...a...a...a...a...a...a...a...", 2},
    /* (the owner's minimal techno reference, 128 BPM: past the riffs, EXPLORE's never) the sub's one note in the kick's
     * gaps, never on its steps: every other sixteenth, or three after each beat (.xXX: the "and" and the "a" accented;
     * .XXX: all three); (the dark techno / industrial EBM reference) the sub's rumble retriggered a step
     * after every kick and fading under the next (-10 and -12 dB: an accent, a ghost, a dead note; the kick's duck on
     * it, gen.c G_DUCK) */
    [BA_MT1]      = {".R.R.R.R.R.R.R.R", ".a.a.a.a.a.a.a.a", 1},
    [BA_MT2]      = {".RRR.RRR.RRR.RRR", "..aa..aa..aa..aa", 1},
    [BA_MT3]      = {".RRR.RRR.RRR.RRR", ".aaa.aaa.aaa.aaa", 1},
    [BA_RUMB]     = {".RRx.RRx.RRx.RRx", ".ag..ag..ag..ag.", 1},
    /* (the owner's phonk reference: Kordhell's, DVRST's cowbell) the 808 cowbell's repeated sixteenths, X.XXXXX. a half
     * bar, its pitch moved every half bar: the tonic, the b2 (the 2nd in aeolian), the b6 under, the 7th under, low
     * (the tonic A#2 .. A3, the reference's G#2 .. G3: gen.c gen_riff); the EBM reference's mid riff on the
     * anticipations (x.xXX..--..xX.xx), the tonic with the phrygian b2 and the minor third (D2 .. F2), no chord under
     * it moving */
    [BA_RF5]      = {"R.RRRRR.2.22222.S.SSSSS.A.AAAAA.", "a.......a.......a.......a.......", 2},
    [BA_RFE]      = {"R.R2R..RR..R3.RR", "...aa..gg...a...", 1},
};
#endif
#include "gen_bass.h"
/* (flash) BA_STEP packed in the image (LZ4: gen_bass.h BA_STEP_Z, ~490 B less), unpacked into RAM at the generator's
 * first use: the same bytes the table had */
#include "boot_lz4.h"
static uint8_t gz_bass[BA_STEP_N] __attribute__((section(".pool")));
static uint8_t gz_ok;
static __attribute__((noinline)) const uint8_t *gz_unpack(void)
{
    if (!gz_ok) boot_lz4(BA_STEP_Z, gz_bass, gz_bass + sizeof gz_bass);
    gz_ok = 1;
    return gz_bass;
}
#define BA_STEP (gz_unpack())
enum { KM_DOUBLE, KM_ANSWER, KM_SUSTAIN, KM_INTERLOCK };   /* the bass against the kick */

/* ------------------------------------------------------------------------------------------------- harmony --- */
/* a chord's quality: its notes as steps of the scale from its root (a stack); R9 rootless (3 5 7 9: the bass has the
 * root, jazz and deep house) */
enum { Q_TRI, Q_7, Q_9, Q_ADD9, Q_SUS2, Q_SUS4, Q_6, Q_QUART, Q_POWER, Q_11, Q_7NO5, Q_R9, Q_N };
static const int8_t QSTACK[Q_N][4] = {{0, 2, 4, 127}, {0, 2, 4, 6}, {0, 2, 6, 8}, {0, 2, 4, 8}, {0, 1, 4, 127}, {0, 3, 4, 127},
                                      {0, 2, 4, 5}, {0, 3, 6, 9}, {0, 4, 7, 127}, {0, 2, 6, 10}, {0, 2, 6, 127}, {2, 4, 6, 8}};
/* (0.8.3) a chord's colour beyond the mode, in the quality's high bits: F_MAJ a major triad on the degree (the minor
 * key's dominant, its leading tone raised), F_DOM a dominant seventh on it (a secondary dominant: V/ii, V/V ..), F_PAR
 * the parallel mode's chord on that degree (mixture: iv in major, IV in minor), F_FLAT the flat side's major chord
 * (bIII bVI bVII: the mode's own when it has it, else borrowed). Every one of them is checked against the piece's mode
 * and the genre's permissions (gen.c gplan_ok): a borrowed chord only where the genre borrows */
#define F_MAJ 0x10u
#define F_DOM 0x20u
#define F_PAR 0x30u
#define F_FLAT 0x40u
#define QQ(q) ((q) & 0x0Fu)
#define QF(q) ((q) & 0xF0u)
typedef struct { uint8_t n; int8_t deg[4]; uint8_t q[4]; } gprog_t;
/* (0.8.3) the progressions as scale degrees of the piece's mode (0 its tonic): a loop of one to four chords, its last
 * chord the one that leads back to the first (the loop's cadence: IV-I, bVII-i, V-i, bII-i ..). A mode where a chord
 * would come out diminished (or augmented) does not take that progression (gen.c tries the genre's next one) */
enum { PR_ONE, PR_I_VII, PR_I_VI, PR_I_IV, PR_I_V, PR_I_II, PR_I_III, PR_IV_I, PR_AEOL, PR_TRANCE, PR_MINOR4, PR_ANDAL,
       PR_EPIC, PR_50S, PR_CYCLE, PR_DEEP4, PR_DEEPVI, PR_JAZZ, PR_SUS, PR_NEOCL, PR_PHRY, PR_ELEC, PR_DREAM, PR_DREAM2,
       PR_TURN, PR_MSTAB, PR_N };
static const gprog_t PROG[PR_N] = {
    [PR_ONE]     = {1, {0}, {Q_7}},                                           /* i7 (a pedal) */
    [PR_I_VII]   = {2, {0, 6}, {Q_TRI, Q_TRI | F_FLAT}},                      /* i bVII (I bVII: borrowed) */
    [PR_I_VI]    = {2, {0, 5}, {Q_7, Q_7}},                                   /* i7 VImaj7 / I vi */
    [PR_I_IV]    = {2, {0, 3}, {Q_7, Q_7}},                                   /* i7 iv7 / i7 IV7 (dorian) / Imaj7 IVmaj7 */
    [PR_I_V]     = {2, {0, 4}, {Q_TRI, Q_TRI}},                               /* I V / i v */
    [PR_I_II]    = {2, {0, 1}, {Q_9, Q_9}},                                   /* I II (lydian) / i ii (dorian) */
    [PR_I_III]   = {2, {0, 2}, {Q_7, Q_7}},
    [PR_IV_I]    = {2, {3, 0}, {Q_7, Q_7}},                                   /* iv7 i7 (Kerri Chandler's cell) */
    [PR_AEOL]    = {4, {0, 5, 2, 6}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},           /* i VI III VII */
    [PR_TRANCE]  = {4, {0, 5, 6, 3}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},           /* i VI VII iv */
    [PR_MINOR4]  = {4, {0, 3, 6, 2}, {Q_7, Q_7, Q_TRI, Q_TRI}},               /* i iv VII III */
    [PR_ANDAL]   = {4, {0, 6, 5, 4}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI | F_MAJ}},   /* i VII VI V: the Andalusian cadence */
    [PR_EPIC]    = {4, {5, 6, 0, 0}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},           /* VI VII i i: the Aeolian cadence */
    [PR_50S]     = {4, {0, 5, 3, 4}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},           /* I vi IV V / i VI iv V */
    [PR_CYCLE]   = {4, {0, 3, 4, 0}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},           /* I IV V I / i iv V i */
    [PR_DEEP4]   = {4, {0, 2, 6, 3}, {Q_7, Q_7, Q_7, Q_7}},                   /* i7 IIImaj7 VII7 iv7 */
    [PR_DEEPVI]  = {4, {0, 5, 6, 0}, {Q_9, Q_7, Q_7, Q_9}},                   /* i9 VImaj7 VII7 i9 */
    [PR_JAZZ]    = {4, {1, 4, 0, 0}, {Q_7, Q_7, Q_9, Q_6}},                   /* ii7 V7 Imaj9 I6 */
    [PR_SUS]     = {2, {0, 4}, {Q_SUS4, Q_SUS2}},
    [PR_NEOCL]   = {4, {0, 4, 5, 3}, {Q_7, Q_7, Q_7, Q_ADD9}},
    [PR_PHRY]    = {2, {0, 1}, {Q_TRI, Q_TRI}},                               /* i bII (phrygian) */
    [PR_ELEC]    = {4, {0, 6, 5, 6}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},           /* (gen6) i VII VI VII: electro's */
    [PR_DREAM]   = {4, {0, 5, 3, 3}, {Q_ADD9, Q_7, Q_7, Q_ADD9}},             /* (V10.6n) Imaj9 vi7 IVmaj7: dream pop's */
    [PR_DREAM2]  = {4, {0, 2, 3, 3}, {Q_7, Q_7, Q_ADD9, Q_7}},                /* Imaj7 iii7 IV */
    [PR_TURN]    = {4, {0, 5, 1, 4}, {Q_7, Q_7, Q_7, Q_7}},                   /* (V10.6w) Imaj7 vi7 ii7 V7: jazz's turnaround */
    [PR_MSTAB]   = {1, {0}, {Q_ADD9}},                                        /* i(add9): minimal techno's one dissonant stab
                                                         * (close: the ninth and the minor third a semitone apart) */
};
/* (0.8.3) a genre's harmony: its family (how it composes) and what it may colour with */
enum { HF_LOOP, HF_JAZZY, HF_POP, HF_COLOR, HF_CINE, HF_JAZZ };   /* modal loops; 7th loops; functional pop; colour
                                                         * washes; film; jazz (ii-V, secondary dominants) */
#define HX_RAISE 0x08u                                  /* the minor key's dominant major at the cadence (leading tone) */
#define HX_SEC 0x10u                                    /* secondary dominants */
#define HX_BOR 0x20u                                    /* borrowed chords (mixture, the flat side in major) */
#define HX_LIFT 0x40u                                   /* FORM PARTS: the last section a whole step up (a real modulation) */
#define HX_APPR 0x80u                                   /* the lead's chromatic approach notes (a semitone into a chord tone) */
#define HXF(h) ((h) & 7u)
/* how the chords are voiced: PAD led to the nearest voicing in C3..G5 as written; PLAIN the same but triads whatever
 * the progression says; RICH sevenths and ninths added; SUS suspensions; QUART fourths; POWER root fifth octave;
 * PLANE the same shape moved in parallel (techno, impressionism) */
enum { VM_PAD, VM_PLAIN, VM_RICH, VM_SUS, VM_QUART, VM_POWER, VM_PLANE };
/* how the chord track plays: held, swelling, stabbed on offbeats, pulsed, dubbed (a stab or two a bar through the
 * delay), skanked (reggae's offbeat eighths), comped (jazz, syncopated), power (a push on the bar), a drone, sparse,
 * broken sixteenths (UKG), trance (gated sixteenths), (V10.6n) chugged (a metal riff: power chords palm-muted on a
 * figure of eighths and sixteenths, low) */
enum { CS_HOLD, CS_SWELL, CS_STAB, CS_PULSE, CS_DUB, CS_SKANK, CS_COMP, CS_POWER, CS_DRONE, CS_SPARSE, CS_BROKEN,
       CS_TRANCE, CS_CHUG, CS_CHARL };   /* (V10.6w) CHARL: jazz's Charleston comping (gen.c gen_pads) */
/* (V10.6n) the chugged riffs (bit p the step p of the bar): eighths, the gallop, a syncopated push, a broken burst */
static const uint16_t CHUG_FIG[4] = {0x5555u, 0x6DB7u, 0x5749u, 0xDB5Bu};
/* the arp: an order over the chord's notes (indices, +4 an octave up, -1 the end), its rate in steps, its gate */
enum { AP_UP, AP_UPDOWN, AP_CHOP, AP_ALBERTI, AP_GLASS, AP_PULSE, AP_SEQ, AP_WALK, AP_CYCLE3, AP_CYCLE5,
       AP_BROKEN, AP_OCTAVES, AP_N };
typedef struct { int8_t ord[8]; uint8_t rate, gate; uint16_t mask; } garp_t;
static const garp_t ARP[AP_N] = {
    [AP_UP]      = {{0, 1, 2, 3, 4, 5, 6, 7}, 1, 50, 0xFFFF},
    [AP_UPDOWN]  = {{0, 1, 2, 3, 2, 1, -1}, 2, 60, 0xFFFF},
    [AP_CHOP]    = {{0, 0, 2, 0, 4, -1}, 1, 30, 0x4848},   /* (V10.6p, DOWN's place: no genre's; TRANCE gone too)
                                                         * a voice's chops off the beat (the "a" of 1, the "and" of 2 ..):
                                                         * Memphis's in phonk, the MC's chant in baile funk */
    [AP_ALBERTI] = {{0, 3, 1, 3, -1}, 1, 70, 0xFFFF},
    [AP_GLASS]   = {{0, 1, 2, 0, 1, 2, 3, 2}, 1, 75, 0xFFFF},
    [AP_PULSE]   = {{0, 0, 0, 0, 0, 0, 0, 2}, 1, 40, 0xFFFF},
    [AP_SEQ]     = {{0, 3, 1, 3, -1}, 1, 45, 0xFFFF},
    [AP_WALK]    = {{0, 2, 1, 3, 2, 0, 3, 1}, 1, 55, 0xFFFF},
    [AP_CYCLE3]  = {{0, 2, 5, -1}, 1, 55, 0xFFFF},
    [AP_CYCLE5]  = {{0, 1, 2, 3, 6, -1}, 1, 55, 0xFFFF},
    [AP_BROKEN]  = {{0, 2, 1, 3, -1}, 2, 60, 0xB6DB},
    [AP_OCTAVES] = {{0, 4, 0, 4, 1, 5, 0, 4}, 1, 45, 0xFFFF},
};
/* (gen5) the club's chord stabs, off the records (gen.c gen_pads): bit p the step p of the bar. The organ / M1 offbeats,
 * the piano's 3 + 3 + 2, deep house's syncopations, techno's stab a bar or its answer; the figures a genre draws from
 * (by its map: HOUSE DEEP TECH HOUSE TECHNO) */
static const uint16_t STAB_FIG[8] = {0x4444u, 0x1249u, 0x4848u, 0x4141u, 0x2424u, 0x0001u, 0x0408u, 0x0488u};
static const uint8_t STAB_SET[4][3] = {{0, 1, 4}, {2, 3, 4}, {0, 2, 6}, {6, 7, 2}};   /* (the owner's minimal techno
                                                         * reference: its stab two, three or four times a bar) */
/* the lead: a motif in phrases, an ostinato cell on an euclid, sparse long chord tones, Part's scale line, a machine's
 * figure, a two-bar hook (call and answer), none */
enum { LD_MOTIF, LD_OSTINATO, LD_SPARSE, LD_TINTI, LD_MECH, LD_HOOK, LD_NONE, LD_EBM, LD_ELEC, LD_RIFF, LD_RIFF2 };   /* (V10.6p)
                                                         * RIFF: a riff of BASS_ARCH's as written (gen.c gen_riff; RIFF2
                                                         * EBM's fifth throw's, BA_RFE: gen_pick's RIFF block not its) */
/* (gen6, the owner's archetypes) a fixed motif in the scale's degrees, a note every `every` steps held `hold`, the
 * chord's moves followed, its last note a step up every fourth bar (the answer): DAF's shout on the bar's one (the
 * step under it now and then); Rother's octave and fifth; (V10.6n) EBM's second shout, Nitzer Ebb's call on 1 and 3
 * (Join in the Chant: the third one higher). A genre with one is an archetype: its parts as written (no pools) */
typedef struct { int8_t d[4]; uint8_t every, hold; } gcell_t;
static const gcell_t LD_CELLS[3] = {{{0, 0, -1, 0}, 16, 3}, {{0, 7, 0, 4}, 2, 1}, {{0, 0, 2, 0}, 32, 2}};   /* (V10.6p: the
                                                         * second shout a call every other bar: sparse) */

/* ------------------------------------------------------------------------------------------ sounds and mix --- */
#define EG_WH 7u                                        /* WHEEL (the others: song_dice.c) */
/* (0.9.9-gen1) a role's SOUND ARCHETYPE, packed (18 B; was 44): up to three (engine, factory preset) options (the first
 * the likeliest; a bass takes the first two only, so its engine is the archetype's), then what the role asks of the
 * sound over the preset, in the engine's real parameters: the amp envelope (GR: a range in eighths of 0..127), the
 * filter (type LP HP BP COMB COMB- MS20, 7: the engine's own; its cutoff and resonance: ANALOG's CUT RES, the part's
 * filter on the others), the filter envelope (GR4: in fourths of 0..63), the glide, ANALOG's WAVE (two: the second one
 * time in three; MIX 0: one oscillator), the LFO on the filter (its depth, a free rate or 1/4 1/8 1/8T 1/16 synced), the
 * part's drive and chorus, the voice mode */
typedef struct {
    uint8_t o[3][2];                                    /* (engine, preset); engine 255: none */
    uint8_t atk, dec, sus, rel, cut, res;               /* GR ranges */
    uint8_t envf;                                       /* GR4 */
    uint8_t ft;                                         /* bits 0..2 P_FTYPE (7: unset), 3..7 GLIDE / 4 */
    uint8_t wv;                                         /* ANALOG: bits 0..2 WAVE, 3..5 the other, 6 MIX 0, 7 none */
    uint8_t lfo;                                        /* high nibble depth / 4, low the rate (GLFO_RATE) */
    uint8_t dc;                                         /* high nibble DIST / 4, low CHORUS / 4 */
    uint8_t voice;
} grecipe_t;
#define GR(lo, hi) (uint8_t)((((lo) / 8) << 4) | ((hi) / 8))
#define GR4(lo, hi) (uint8_t)((((lo) / 4) << 4) | ((hi) / 4))
#define GFT(t, gl) (uint8_t)((t) | (((gl) / 4) << 3))
#define GWV(a, b, m) (uint8_t)((a) | ((b) << 3) | ((m) << 6))
#define GNOWV 0x80u
#define GLFO(d, r) (uint8_t)((((d) / 4) << 4) | (r))      /* r < 12: a free rate r * 2 + 2; 12 .. 15: 1/4 1/8 1/8T 1/16 */
#define GDC(d, c) (uint8_t)((((d) / 4) << 4) | ((c) / 4))
#define NO_ {255, 0}
enum { SR_SUB, SR_ACID, SR_REESE, SR_FMBASS, SR_ROCKBASS, SR_HOOK, SR_WOW, SR_PAD, SR_STRINGS, SR_CHOIR, SR_ORGAN,
       SR_EP, SR_STAB, SR_DUBCHORD, SR_PLUCK, SR_BELL, SR_LEAD, SR_VOXLEAD, SR_CHIP, SR_KEYS, SR_DRONE, SR_NOISE,
       SR_TEXTURE, SR_HOOVER, SR_WALL, SR_GTR, SR_SINEKEY, SR_EBMBASS, SR_808, SR_UPRIGHT, SR_TECHBASS, SR_SUPERSAW,
       SR_PIANO, SR_HARPSI, SR_DIST, SR_COWBELL, SR_FLUTE, SR_CHANT, SR_MALLET, SR_N };   /* (V10.6p: SUPERSAW gone, no genre's) */
/* the factory presets by their place (eng_analog.c, eng_phase.c .. : ANALOG 2 SQR BASS, 4 ACID, 7 SUB BASS ..) */
/* (V10.6z10, the owner: a genre's throws "the same sounds") an archetype's two options never one preset twice (SUB
 * BASS's SQR BASS, the 808's, the upright's, the chant's TALK BOX; EBM's FAT BASS and phonk's 808 COWBELL twice: the
 * owner's); a third of the time another of the engine's of the same type (gen.c gen_sound) */
static const grecipe_t RECIPE[SR_N] = {
    /* ---- basses: their engine fixed */
    [SR_SUB]      = {{{EG_A, 7}, {EG_A, 2}, {EG_LO, 4}}, GR(0, 7), GR(56, 79), GR(88, 119), GR(16, 31), GR(32, 47), GR(0, 15), GR4(0, 7), GFT(0, 0), GNOWV, 0, 0, V_LEGATO},
    /* the 303 (0.9.9-gen2, after X0X's Open303: CUT ~48, RES ~100, ENV MOD ~85 of 127, DECAY ~200..600 ms, a 60 ms
     * slide, ACCENT on the filter's envelope and the level): one saw (or a square), the low-pass low (~400 Hz at rest)
     * with RES high (not ringing on its own) and the envelope high on it (~2 kHz at its peak; an ACCENT opens it ~2
     * octaves more: voice.c), its decay short (400..900 ms to the end: the filter's sweep) and a sustain under it
     * (ANALOG's one envelope is the level's too: the note's body), LEGATO, the SLIDEs gliding in TIME (gen_sound:
     * ~55 ms, no retrigger), driven after (DIST) */
    [SR_ACID]     = {{{EG_A, 4}, {EG_A, 2}, NO_}, GR(0, 7), GR(80, 88), GR(40, 48), GR(16, 31), GR(32, 40), GR(104, 112), GR4(52, 60), GFT(0, 24), GWV(0, 1, 1), 0, GDC(24, 0), V_LEGATO},
    [SR_REESE]    = {{{EG_A, 16}, {EG_A, 14}, NO_}, GR(0, 7), GR(72, 95), GR(104, 119), GR(32, 47), GR(40, 55), GR(16, 31), GR4(0, 11), GFT(0, 16), GNOWV, GLFO(12, 1), GDC(20, 28), V_LEGATO},
    /* (V10.54) the FM6 / WHEEL numbers below re-aimed after V10.42 replaced FM6 8..12 and 14 (ICE BELL DX KEYS FM PLUCK SOFT BELL FM
     * STRINGS WOOD) and WHEEL 5, 6 (DUB ORGAN SOFT KEYS): the roles got leads and basses (a SUB BASS on the bells) */
    [SR_FMBASS]   = {{{EG_FM, 2}, {EG_FM, 13}, {EG_PH, 7}}, GR(0, 7), GR(48, 71), GR(40, 87), GR(16, 31), GR(72, 103), GR(8, 23), GR4(8, 27), GFT(0, 0), GNOWV, 0, GDC(8, 0), V_LEGATO},
    [SR_ROCKBASS] = {{{EG_A, 2}, {EG_A, 14}, NO_}, GR(0, 7), GR(48, 71), GR(64, 111), GR(16, 39), GR(48, 71), GR(8, 23), GR4(8, 23), GFT(0, 0), GWV(1, 0, 0), 0, GDC(24, 16), V_LEGATO},
    /* the octave bass of italo, synth pop, electro, trance: a saw (or a square), plucked by the filter */
    [SR_HOOK]     = {{{EG_A, 2}, {EG_A, 14}, NO_}, GR(0, 7), GR(40, 63), GR(16, 55), GR(16, 31), GR(48, 71), GR(16, 39), GR4(20, 39), GFT(0, 0), GWV(0, 1, 0), 0, GDC(12, 16), V_LEGATO},
    /* dubstep's wobble: the fat saws, the resonant low-pass moved by an LFO on the beat */
    [SR_WOW]      = {{{EG_A, 14}, {EG_A, 16}, NO_}, GR(0, 7), GR(72, 95), GR(104, 127), GR(24, 39), GR(32, 47), GR(48, 71), GR4(0, 7), GFT(0, 8), GNOWV, GLFO(44, 13), GDC(40, 0), V_LEGATO},
    /* ---- chords, pads */
    [SR_PAD]      = {{{EG_A, 15}, {EG_PH, 2}, {EG_PH, 9}}, GR(56, 103), GR(80, 103), GR(104, 119), GR(80, 111), GR(40, 71), GR(8, 23), GR4(4, 19), GFT(0, 0), GNOWV, GLFO(12, 3), GDC(0, 40), V_POLY},
    [SR_STRINGS]  = {{{EG_A, 11}, {EG_A, 22}, {EG_PH, 2}}, GR(40, 79), GR(80, 103), GR(104, 127), GR(72, 103), GR(48, 79), GR(8, 23), GR4(0, 11), GFT(0, 0), GNOWV, GLFO(12, 2), GDC(0, 56), V_POLY},
    [SR_CHOIR]    = {{{EG_VO, 0}, {EG_VO, 4}, {EG_FM, 4}}, GR(56, 103), GR(88, 103), GR(104, 119), GR(80, 111), GR(56, 95), GR(0, 7), GR4(0, 7), GFT(0, 0), GNOWV, GLFO(8, 2), GDC(0, 60), V_POLY},
    [SR_ORGAN]    = {{{EG_WH, 8}, {EG_WH, 1}, {EG_PH, 1}}, GR(0, 7), GR(64, 71), GR(120, 127), GR(24, 47), GR(56, 95), GR(0, 7), GR4(0, 3), GFT(0, 0), GNOWV, 0, GDC(0, 8), V_POLY},
    [SR_EP]       = {{{EG_FM, 0}, {EG_FM, 5}, {EG_PH, 8}}, GR(0, 7), GR(72, 95), GR(16, 55), GR(48, 79), GR(56, 95), GR(0, 15), GR4(4, 19), GFT(0, 0), GNOWV, 0, GDC(0, 20), V_POLY},
    [SR_STAB]     = {{{EG_A, 18}, {EG_PH, 10}, {EG_WH, 2}}, GR(0, 7), GR(56, 71), GR(24, 71), GR(24, 47), GR(48, 79), GR(16, 39), GR4(16, 39), GFT(0, 0), GNOWV, 0, GDC(8, 28), V_POLY},
    [SR_DUBCHORD] = {{{EG_A, 18}, {EG_WH, 2}, {EG_A, 3}}, GR(0, 7), GR(48, 71), GR(16, 63), GR(32, 55), GR(48, 71), GR(24, 47), GR4(12, 31), GFT(2, 0), GNOWV, 0, GDC(0, 32), V_POLY},
    /* ---- arps, leads */
    [SR_PLUCK]    = {{{EG_A, 20}, {EG_FM, 7}, {EG_A, 8}}, GR(0, 7), GR(40, 63), GR(0, 23), GR(32, 63), GR(56, 87), GR(24, 47), GR4(28, 55), GFT(0, 0), GNOWV, 0, GDC(0, 8), V_POLY},
    [SR_BELL]     = {{{EG_FM, 1}, {EG_FM, 5}, {EG_A, 13}}, GR(0, 7), GR(80, 103), GR(0, 7), GR(72, 103), GR(72, 103), GR(0, 15), GR4(0, 11), GFT(0, 0), GNOWV, 0, GDC(0, 8), V_POLY},
    [SR_LEAD]     = {{{EG_A, 21}, {EG_A, 0}, {EG_PH, 14}}, GR(0, 23), GR(48, 87), GR(56, 95), GR(32, 71), GR(56, 87), GR(16, 39), GR4(16, 39), GFT(0, 16), GNOWV, GLFO(4, 6), GDC(12, 12), V_LEGATO},
    [SR_VOXLEAD]  = {{{EG_VO, 1}, {EG_VO, 6}, {EG_A, 21}}, GR(0, 23), GR(56, 87), GR(56, 95), GR(32, 71), GR(56, 95), GR(8, 31), GR4(8, 23), GFT(0, 20), GNOWV, GLFO(4, 6), GDC(0, 20), V_LEGATO},
    [SR_CHIP]     = {{{EG_A, 24}, {EG_LO, 6}, {EG_A, 20}}, GR(0, 7), GR(40, 79), GR(24, 79), GR(16, 47), GR(72, 111), GR(8, 31), GR4(8, 27), GFT(0, 0), GNOWV, 0, 0, V_LEGATO},
    [SR_KEYS]     = {{{EG_A, 23}, {EG_PH, 15}, {EG_WH, 1}}, GR(0, 7), GR(64, 87), GR(16, 55), GR(40, 71), GR(56, 95), GR(0, 23), GR4(4, 19), GFT(0, 0), GNOWV, 0, GDC(0, 20), V_POLY},
    /* ---- textures */
    [SR_DRONE]    = {{{EG_A, 22}, {EG_A, 19}, {EG_PH, 9}}, GR(88, 119), GR(88, 103), GR(120, 127), GR(96, 127), GR(32, 63), GR(16, 39), GR4(0, 11), GFT(0, 0), GNOWV, GLFO(16, 1), GDC(0, 44), V_POLY},
    [SR_NOISE]    = {{{EG_NO, 4}, {EG_NO, 7}, {EG_NO, 5}}, GR(80, 119), GR(88, 103), GR(104, 127), GR(80, 119), 0, 0, 0, GFT(7, 0), GNOWV, 0, 0, V_POLY},
    [SR_TEXTURE]  = {{{EG_NO, 5}, {EG_NO, 6}, {EG_LO, 11}}, GR(64, 111), GR(88, 103), GR(104, 127), GR(88, 119), GR(40, 71), GR(16, 47), GR4(0, 11), GFT(2, 0), GNOWV, GLFO(20, 2), GDC(0, 32), V_POLY},
    [SR_HOOVER]   = {{{EG_A, 17}, {EG_A, 6}, {EG_PH, 6}}, GR(0, 15), GR(56, 95), GR(56, 95), GR(32, 63), GR(56, 95), GR(8, 23), GR4(8, 31), GFT(0, 0), GNOWV, 0, GDC(24, 48), V_POLY},
    [SR_WALL]     = {{{EG_A, 22}, {EG_A, 15}, {EG_A, 11}}, GR(24, 63), GR(88, 103), GR(104, 127), GR(80, 111), GR(48, 71), GR(0, 7), GR4(0, 7), GFT(0, 0), GNOWV, GLFO(8, 2), GDC(40, 56), V_POLY},
    [SR_GTR]      = {{{EG_PH, 13}, {EG_PH, 2}, {EG_A, 8}}, GR(0, 15), GR(64, 95), GR(56, 103), GR(48, 79), GR(56, 79), GR(16, 39), GR4(4, 19), GFT(2, 0), GNOWV, 0, GDC(12, 28), V_POLY},
    [SR_SINEKEY]  = {{{EG_A, 5}, {EG_FM, 0}, {EG_A, 23}}, GR(0, 7), GR(64, 95), GR(24, 63), GR(48, 79), GR(56, 95), GR(0, 15), GR4(0, 15), GFT(0, 0), GNOWV, 0, GDC(0, 8), V_POLY},
    /* ---- (0.9.9-gen1) the archetypes the genres lacked */
    /* EBM (V10.6p, the owner: "a modular's analog basses, slightly detuned"): FAT BASS's two oscillators (two saws, two
     * squares one time in three) ~9 cents apart, the second an octave under (or a fourth, a fifth: GEN's consonant
     * INT), the MS20 filter, short, its envelope on it (the accents open it further), driven */
    [SR_EBMBASS]  = {{{EG_A, 14}, {EG_A, 14}, NO_}, GR(0, 7), GR(32, 47), GR(8, 31), GR(8, 23), GR(48, 71), GR(40, 63), GR4(28, 47), GFT(5, 0), GWV(0, 1, 0), 0, GDC(28, 0), V_LEGATO},
    /* trap's 808: a sine's long boom, gliding (the SLIDEs), driven */
    [SR_808]      = {{{EG_A, 7}, {EG_A, 2}, NO_}, GR(0, 7), GR(88, 111), GR(0, 15), GR(32, 47), GR(56, 79), GR(0, 7), GR4(0, 7), GFT(0, 48), GWV(3, 3, 1), 0, GDC(36, 0), V_LEGATO},
    /* jazz, bossa, boom bap: an upright's triangle, plucked */
    [SR_UPRIGHT]  = {{{EG_A, 7}, {EG_A, 14}, NO_}, GR(0, 7), GR(48, 63), GR(0, 23), GR(24, 39), GR(40, 63), GR(0, 15), GR4(8, 23), GFT(0, 0), GWV(2, 2, 1), 0, 0, V_LEGATO},
    /* techno's rolling bass: a saw (or a square), short, resonant, the envelope on it, driven */
    [SR_TECHBASS] = {{{EG_A, 14}, {EG_A, 2}, NO_}, GR(0, 7), GR(40, 55), GR(0, 31), GR(16, 31), GR(32, 55), GR(32, 55), GR4(20, 39), GFT(0, 0), GWV(0, 1, 1), 0, GDC(32, 0), V_LEGATO},
    /* (V10.6n) neo classic's felt piano (STRING's PIANO, PHASE's CZ PIANO, FM6's TINE EP); the fugue's harpsichord (a
     * plucked string, bright, short, no sustain); metal's and techno rock's distorted guitar (a steel string or a saw,
     * palm-muted short, the drive high) */
    [SR_PIANO]    = {{{ENGI_STRING, 5}, {EG_PH, 8}, {EG_FM, 0}}, GR(0, 7), GR(72, 103), GR(0, 31), GR(48, 79), GR(56, 87), GR(0, 15), GR4(0, 15), GFT(0, 0), GNOWV, 0, GDC(0, 8), V_POLY},
    [SR_HARPSI]   = {{{ENGI_STRING, 6}, {EG_PH, 13}, {EG_FM, 7}}, GR(0, 7), GR(48, 71), GR(0, 7), GR(24, 47), GR(80, 111), GR(8, 23), GR4(8, 23), GFT(0, 0), GNOWV, 0, GDC(0, 4), V_POLY},
    /* (V10.6p) the TR-808's cowbell pitched (phonk's melody): ANALOG's 808 COWBELL, two squares a 6th and 79 cents apart
     * (the 808's 540 / 800 Hz, x 1.48; gen.c gen_sound keeps them), band-passed, a short decay, driven */
    [SR_COWBELL]  = {{{EG_A, 25}, {EG_A, 25}, NO_}, GR(0, 7), GR(32, 47), GR(0, 7), GR(16, 31), GR(64, 79), GR(40, 63), GR4(0, 11), GFT(2, 0), GWV(1, 1, 0), 0, GDC(24, 0), V_POLY},
    /* (V10.6p) baile funk's flute (Bum Bum Tam Tam's Bach): ANIMAL's whistles, WHEEL's flute; the voice's chops and
     * shouts (VOICE's CHANT, short: phonk's Memphis chops, the MC's chant, EBM's shout) */
    [SR_FLUTE]    = {{{ENGI_ANIMAL, 25}, {EG_WH, 3}, {ENGI_ANIMAL, 5}}, GR(0, 15), GR(48, 79), GR(56, 87), GR(16, 39), GR(96, 119), GR(0, 15), GR4(4, 11), GFT(0, 0), GNOWV, 0, 0, V_LEGATO},
    [SR_CHANT]    = {{{EG_VO, 5}, {EG_VO, 6}, {EG_VO, 1}}, GR(0, 7), GR(40, 63), GR(0, 23), GR(16, 31), GR(64, 95), GR(0, 15), GR4(0, 7), GFT(0, 0), GNOWV, 0, GDC(8, 0), V_POLY},
    [SR_DIST]     = {{{ENGI_STRING, 1}, {EG_A, 9}, {EG_PH, 11}}, GR(0, 7), GR(32, 55), GR(16, 47), GR(8, 31), GR(56, 87), GR(8, 31), GR4(8, 27), GFT(0, 0), GWV(0, 0, 0), 0, GDC(60, 8), V_POLY},
    /* (V10.7.2) the latin plucks: STRING's MARIMBA (Dominican dembow's riff), NYLON (reggaeton romantico's guitar
     * picked); short, no sustain */
    [SR_MALLET]   = {{{ENGI_STRING, 7}, {ENGI_STRING, 7}, {ENGI_STRING, 0}}, GR(0, 7), GR(48, 71), GR(0, 7), GR(24, 47), GR(64, 95), GR(0, 15), GR4(0, 15), GFT(0, 0), GNOWV, 0, GDC(0, 4), V_POLY},
};
/* (the GENE mix pass) each archetype's options evened: a trim a sound (3 nibbles, the options' 0 1 2, signed, x 3 dB:
 * gen.c gen_sound), from the host render of twelve throws of every genre: what a sound gives at LEVEL 0 dB while it
 * sounds beside the others of its role (V10.6z9: a SUB BASS 9 dB over a FAT BASS or a REESE, a STRINGS or a DARK PAD
 * 10 over a stab, the 808 COWBELL and the CHANT 11 .. 15 under the other leads) */
#define RT(a, b, c) (uint16_t)(((a) & 15) | ((b) & 15) << 4 | ((c) & 15) << 8)
static const uint16_t RTRIM[SR_N] = {
    [SR_SUB] = RT(-4, -3, 0),
    [SR_ACID] = RT(-1, -1, 0),
    [SR_REESE] = RT(1, 1, 0),
    [SR_FMBASS] = RT(0, -1, 0),
    [SR_ROCKBASS] = RT(-1, 0, 0),
    [SR_HOOK] = RT(1, 2, 0),
    [SR_WOW] = RT(0, 0, 0),
    [SR_PAD] = RT(-1, -2, -1),
    [SR_STRINGS] = RT(-4, -3, -4),
    [SR_CHOIR] = RT(-1, -2, 0),
    [SR_ORGAN] = RT(0, -1, -2),
    [SR_EP] = RT(0, -1, -1),
    [SR_STAB] = RT(2, 0, 2),
    [SR_DUBCHORD] = RT(3, 3, 3),
    [SR_PLUCK] = RT(2, -3, 1),
    [SR_BELL] = RT(-1, -2, 2),
    [SR_LEAD] = RT(-1, -1, -2),
    [SR_VOXLEAD] = RT(0, 1, -4),
    [SR_CHIP] = RT(-1, 2, -1),
    [SR_KEYS] = RT(-1, -1, 2),
    [SR_DRONE] = RT(-4, -4, -4),
    [SR_NOISE] = RT(0, -2, 0),
    [SR_TEXTURE] = RT(3, 1, 0),
    [SR_HOOVER] = RT(-1, 0, -2),
    [SR_WALL] = RT(-4, -4, -3),
    [SR_GTR] = RT(-2, -1, 2),
    [SR_SINEKEY] = RT(0, 1, 0),
    [SR_EBMBASS] = RT(3, 0, 0),
    [SR_808] = RT(-2, -3, 0),
    [SR_UPRIGHT] = RT(0, 2, 0),
    [SR_TECHBASS] = RT(1, 2, 0),
    [SR_SUPERSAW] = RT(0, 0, 0),
    [SR_PIANO] = RT(1, -2, 0),
    [SR_HARPSI] = RT(2, 1, 0),
    [SR_DIST] = RT(1, 1, 0),
    [SR_COWBELL] = RT(6, 0, 0),
    [SR_FLUTE] = RT(0, 3, 0),
    [SR_CHANT] = RT(4, 5, 6),
    [SR_MALLET] = RT(4, 4, 4),   /* (V10.7.2: STRING's mallet and nylon ~15 dB under the plucks, measured) */
};
/* (the GENE mix pass) the pads' and the strings' second option PHASE's STRING and ANALOG's DARK PAD, the drones' third
 * PHASE's GLASS PAD, FM6's SOFT PAD's place: its operators' slow attacks (~1 s) under GEN's chords, stabbed or sparse,
 * left a chord's part 27 .. 34 dB under (DRUM & BASS's pad gone one throw in three); the stabs' third WHEEL's GOSPEL
 * (the house organ's stab), FM6's BRASS's place: its swell under a stab 20 dB from one throw to the next. FM6's PAD
 * is the choirs' still */
/* (gen3) the amp envelope a role keeps whatever its archetype (gen.c gen_sound clamps ATK DEC SUS REL into these GR ranges):
 * a LEAD's sustain 44..75 % (never the organ's or the choir's full one: a medium decay to it, a short release; a bell's,
 * an EP's, a pluck's lead, its archetype's sustain under 56, a pluck's), an ARP's a pluck's (a fast attack, a short-medium decay, a low sustain), the CHORDS stabbed short (STAB PULSE DUB SKANK COMP
 * BROKEN TRANCE: GSTAB) or held slow in and out (HOLD SWELL DRONE), a BASS fast in; the rest as the archetype has it */
static const uint8_t ENV_RULE[6][4] = {
    {GR(0, 23), GR(48, 95), GR(56, 95), GR(16, 47)},    /* lead */
    {GR(0, 7), GR(32, 79), GR(0, 39), GR(16, 63)},      /* arp: a pluck */
    {GR(0, 7), GR(32, 71), GR(0, 47), GR(16, 47)},      /* chords stabbed */
    {GR(16, 127), GR(48, 127), GR(56, 127), GR(64, 127)},   /* chords held */
    {GR(0, 15), GR(24, 127), GR(0, 127), GR(8, 63)},    /* bass */
    {GR(0, 127), GR(0, 127), GR(0, 127), GR(0, 127)},   /* the others */
};
#define GSTAB(p) ((0x3C7Cu >> (p)) & 1u)                /* CS_STAB PULSE DUB SKANK COMP BROKEN TRANCE CHUG CHARL: the chords short */
/* the names the song generator never picks (the cheerful, the odd), whatever the role */
static const char *const GEN_NEVER[] = {"ARCADE", "BYTEBEAT", "GOSPEL", "ROCK DRIVE", "METAL", "BYTE VAR", "FLOAT ARP",
                                        "BYTE TONE", "INIT"};   /* (V10.6p: FLOAT FM gone, no preset's name now) */

/* -------------------------------------------------------------------------------------------------- forms --- */
enum { FM_LOOP, FM_PARTS, FM_CLUB, FM_ARC };            /* one loop; A B A C; the club plan (gen_parts' rows, in PARTS
                                                         * too); a slow arc, the layers entering one by one (V10.6n: as
                                                         * rows; FUGA's its voices': gen_tables.c GSP_FUGUE) */

/* -------------------------------------------------------------------------------------------------- genres --- */
#define SC_BLUE 12u                                     /* (the other scales: song_dice.c) */
#define SC_PEN 5u
typedef struct {
    const char *name;
    uint8_t bpm_lo, bpm_hi, swing;                      /* swing: G_SWING, 0..100 */
    uint8_t scale[4];                                   /* (0.8.3: a mode of seven is the key; PEN MPEN BLUES the lead's
                                                         * pentatonic over the mode that holds it) */
    uint8_t drum, drum_pct, kit, ghosts, hats, fills;   /* the map, how often drums play, the kit, snare ghosts a bar
                                                         * at full amount, the hats' cell, the fill */
    int8_t  sd_micro;                                   /* the snare laid back (sixteenths of a step) */
    uint8_t bass[3], bass_lo, kick_mode, bass_gate;     /* three archetypes, the root's lowest note, the kick rule, GATE */
    uint8_t prog[3], cbars, pads, voicing;              /* three progressions, bars a chord, the chords' play, voicing */
    uint8_t arp[2], lead;
    uint8_t amt[GP_N];                                  /* the parts' default amounts (0: the part sleeps) */
    uint8_t snd[GP_N];                                  /* the kit (GP_DRUMS) and a recipe a role */
    uint8_t tex;                                        /* the sixth track's recipe (TRACKS 6) */
    uint8_t rtype, dtime, wet, dtype;                   /* reverb type, delay time (N_DTIME), 0..100 wet, delay type bit 0
                                                         * ANALOG / DIGITAL, bit 1 PING-PONG */
    uint8_t form;
    uint8_t human;                                      /* 0 a machine .. 100 a band: HUMAN and TIMING */
    uint8_t hx;                                         /* (0.8.3) the harmony: its family HF_ and its HX_ colours */
    uint8_t spec;                                       /* (V10.6n) its own ways, GSP_ (the struct's padding: no room) */
} ggenre_t;
/* (V10.6n) a genre's own ways: the drums programmed then edited (IDM's: 1 .. 3, gen.c gidm_edit), a 3:4 layer over a
 * cycle of 48 (IDM WEIRD), the fugue's voices (the subject's answer and its bass imitated, entering one by one), the
 * lead in thirds (a PLAYHEAD a third under it: cumbia's second voice), the bass pumped under the kick (hard bass's
 * sidechain), the pads' tape wobble and detune (Boards of Canada) */
#define GSP_EDIT(s) ((s) & 3u)
#define GSP_POLY 0x04u
#define GSP_FUGUE 0x08u
#define GSP_THIRDS 0x10u
#define GSP_PUMP 0x20u
#define GSP_WOW 0x40u
#define GSP_HEAVY 0x80u                                 /* (V10.6p) the drums heavy, distorted: the kit driven, the drum
                                                         * track's DIST, the bass's, the master's clip (EBM PHONK BAILE) */
#define AMT(d, b, c, a, l) {d, b, c, a, l}
/* (V10.6n, the owner's list, in its order: the knob's after EXPLORE and RANDOM) */
static const ggenre_t GENRE[] = {
    /* ---- the club: techno */
    /* (V10.6n) Richie Hawtin / Plastikman, Robert Hood, Ricardo Villalobos: a dry four on the floor, the hats off the
     * beat, a click or two, a rolling one- or two-note bass, one stab a bar whose filter moves slowly, little harmony */
    /* (the owner's minimal techno reference, 128 BPM) a throw straight, at 56 % or at 64 % (GSUB_MT), the sub's one low
     * note in the kick's gaps (BA_MT1 MT2 MT3: never on the kick; its rules kept whole, gen.c glatin), loud over the
     * drums (GMIX), one chord (no other: glatin), one dissonant stab two to four times a bar (PR_MSTAB, STAB_SET) */
    {"MINIMAL TECHNO", 126, 130, 0, {SC_MIN, SC_DOR, SC_MIN, SC_MIN}, DM_TECHNO, 100, KIT_CLICK, 0, HC_MACHINE, FL_NONE, 0,
     {BA_MT1, BA_MT2, BA_MT3}, 29, KM_SUSTAIN, 70, {PR_MSTAB, PR_MSTAB, PR_MSTAB}, 4, CS_STAB, VM_PLANE,
     {AP_PULSE, AP_SEQ}, LD_NONE, AMT(95, 70, 25, 0, 0), {KIT_CLICK, SR_SUB, SR_STAB, SR_PLUCK, SR_LEAD}, SR_NOISE,
     0, 2, 30, 0, FM_CLUB, 0, HF_LOOP, 0},
    /* (V10.6n: 145 .. 160, today's) I Hate Models, Klangkuenstler, VTSS: an industrial kit driven, the kick's rumble
     * (the bass's sixteenths between the kicks), sixteenth hats, rave stabs */
    {"HARD TECHNO", 145, 160, 0, {SC_PHRY, SC_MIN, SC_HARM, SC_PHRY}, DM_HARD, 100, KIT_INDUS, 0, HC_MACHINE, FL_DROP, 0,
     {BA_PSY, BA_ROLL16, BA_OCTBNC}, 33, KM_SUSTAIN, 40, {PR_ONE, PR_PHRY, PR_I_VII}, 4, CS_STAB, VM_POWER,
     {AP_PULSE, AP_OCTAVES}, LD_OSTINATO, AMT(100, 80, 30, 20, 0), {KIT_INDUS, SR_TECHBASS, SR_STAB, SR_PLUCK, SR_HOOVER}, SR_NOISE,
     1, 2, 25, 0, FM_CLUB, 0, HF_LOOP, 0},
    /* (V10.6n) Russian hard bass (XS Project, Hard Bass School): a hard four on the floor, the bass bouncing on every
     * offbeat eighth (pumped: the kick's sidechain), a simple minor riff a chord a bar, a hoover's line, chanted shouts */
    {"HARD BASS", 145, 160, 0, {SC_MIN, SC_MIN, SC_HARM, SC_PHRY}, DM_HBASS, 100, KIT_INDUS, 0, HC_MACHINE, FL_ROLL, 0,
     {BA_OFFBEAT, BA_HBASS, BA_OFFOCT}, 33, KM_SUSTAIN, 35, {PR_AEOL, PR_TRANCE, PR_I_VI}, 1, CS_STAB, VM_POWER,
     {AP_OCTAVES, AP_PULSE}, LD_OSTINATO, AMT(100, 100, 40, 0, 55), {KIT_INDUS, SR_REESE, SR_STAB, SR_PLUCK, SR_HOOVER}, SR_VOXLEAD,
     1, 2, 25, 0, FM_CLUB, 0, HF_POP | HX_RAISE, GSP_PUMP},
    /* ---- the club: house */
    /* Chicago / New York house (Frankie Knuckles, Marshall Jefferson, Kerri Chandler): a 909 four on the floor, the
     * open hat off the beat, M1 piano / organ stabs on the chords, a synth bass answering the kick, swing ~56 % */
    {"HOUSE", 122, 128, 30, {SC_MIN, SC_DOR, SC_MAJ, SC_MIN}, DM_HOUSE, 100, KIT_909, 0, HC_HOUSE, FL_ROLL, 0,
     {BA_OFFBEAT, BA_BOUNCE, BA_OFFOCT}, 36, KM_ANSWER, 45, {PR_I_IV, PR_I_VI, PR_DEEP4}, 1, CS_STAB, VM_PLANE,
     {AP_BROKEN, AP_UP}, LD_HOOK, AMT(90, 75, 70, 0, 0), {KIT_909, SR_FMBASS, SR_KEYS, SR_PLUCK, SR_ORGAN}, SR_NOISE,
     0, 1, 40, 0, FM_CLUB, 10, HF_JAZZY, 0},
    /* Larry Heard (Can You Feel It), Chez Damier, Kerri Chandler: an 808, a Rhodes on 7ths and 9ths through the
     * delay, a walking sub, swing ~57 % */
    {"DEEP HOUSE", 120, 126, 35, {SC_MIN, SC_DOR, SC_DOR, SC_MPEN}, DM_DEEP, 100, KIT_808, 0, HC_HOUSE, FL_DROP, 1,
     {BA_DEEPWALK, BA_OFFBEAT, BA_UKG}, 36, KM_ANSWER, 50, {PR_I_IV, PR_JAZZ, PR_IV_I}, 2, CS_STAB, VM_PLANE,
     {AP_BROKEN, AP_UPDOWN}, LD_SPARSE, AMT(85, 75, 75, 0, 0), {KIT_808, SR_SUB, SR_EP, SR_KEYS, SR_SINEKEY}, SR_TEXTURE,
     0, 1, 55, 0, FM_CLUB, 20, HF_JAZZY, 0},
    /* (gen6b, the owner: "nothing like the real thing") Green Velvet, Hot Since 82, Solardo, Terry Francis: a 909
     * four on the floor that never lets go, the clap on 2 and 4, shuffled 16th hats, the open hat off the beat, a
     * syncopated rim; a rolling bass off the kick, one stab with its filter moving; no lead, swing ~55 % */
    {"TECH HOUSE", 124, 130, 22, {SC_MIN, SC_DOR, SC_MIN, SC_PHRY}, DM_TECHHOUSE, 100, KIT_909, 0, HC_HOUSE, FL_ROLL, 0,
     {BA_VILLA, BA_OFFOCT, BA_UKG}, 36, KM_ANSWER, 40, {PR_ONE, PR_I_VII, PR_ONE}, 4, CS_STAB, VM_SUS,
     {AP_BROKEN, AP_SEQ}, LD_NONE, AMT(95, 90, 40, 0, 0), {KIT_909, SR_TECHBASS, SR_DUBCHORD, SR_PLUCK, SR_LEAD}, SR_NOISE,
     0, 1, 30, 0, FM_CLUB, 5, HF_LOOP, 0},
    /* (V10.6n) tribal house and techno (Danny Tenaglia, Ben Sims, Joel Mull): the four on the floor under toms, congas,
     * a shaker, a rim and a cowbell; a rolling bass, one chord, a chant over it, little melody */
    {"TRIBAL", 125, 132, 14, {SC_MIN, SC_DOR, SC_PHRY, SC_MIN}, DM_TRIBAL, 100, KIT_909, 0, HC_FUNK, FL_TOM, 0,
     {BA_VILLA, BA_OFFBEAT, BA_JUNGLE}, 33, KM_ANSWER, 40, {PR_ONE, PR_I_VII, PR_ONE}, 4, CS_STAB, VM_SUS,
     {AP_PULSE, AP_CYCLE3}, LD_NONE, AMT(100, 80, 25, 15, 0), {KIT_909, SR_TECHBASS, SR_STAB, SR_PLUCK, SR_LEAD}, SR_CHOIR,
     1, 2, 35, 0, FM_CLUB, 15, HF_LOOP, 0},
    /* ---- latin */
    /* (V10.6n) digital and villera cumbia (Damas Gratis, Pibes Chorros; Chancha Via Circuito, Dengue Dengue Dengue;
     * V10.6u the owner's demos: 77 BPM, a brighter kit): the guiro's sixteenths, the kick and the bass on every beat,
     * the llamador on the "and", the keys' offbeat chords, an organ-accordion's melody in thirds. (V10.7.2, the owner:
     * "very clear rules") the rules every throw keeps: the 2/4 (the kick on every beat or on 1 and 3), the guiro never
     * stopping, the bass's tonica-quinta on the beat (no bossa, no salsa tumbao: its downbeat silent), the keys on every
     * "and", major or minor (no dorian), I V / I IV V I / i iv V i with the V major; its throws (GSUB_CU) sonidera (the
     * row, the owner's 77), villera (88 .. 98: the timbal on 2 and 4, the chiqui-chiqui, the root anticipated, two bars
     * a chord) and digital (90 .. 100: the tambora's 1 and 3, a sub's half notes, an arpeggio over it) */
    {"CUMBIA", 76, 88, 10, {SC_MIN, SC_MAJ, SC_MIN, SC_MAJ}, DM_CUMBIA, 100, KIT_909, 0, HC_FLAT, FL_ROLL, 0,
     {BA_CUMBIA, BA_CUMB3, BA_CUMBIA}, 33, KM_DOUBLE, 60, {PR_CYCLE, PR_I_V, PR_I_IV}, 1, CS_SKANK, VM_PLAIN,
     {AP_ALBERTI, AP_UPDOWN}, LD_HOOK, AMT(90, 95, 55, 0, 60), {KIT_909, SR_FMBASS, SR_ORGAN, SR_PLUCK, SR_ORGAN}, SR_TEXTURE,
     1, 1, 35, 0, FM_PARTS, 30, HF_POP | HX_RAISE, GSP_THIRDS},
    /* (V10.6n) reggaeton (Daddy Yankee, Tainy, the Bad Bunny era): the dembow riddim, an 808 sub on the kick, i VI III VII,
     * a plucked lead. (V10.7.2) its rules: 88 .. 100, the kick on every beat, the snare on the dembow's 3 + 3 + 2 (the
     * sixteenths 3 6 11 14), eighth or sixteenth hats rolled into the snare, the 808 on the kick or in the tresillo,
     * natural minor (i VI III VII, i iv VII III, VI VII i), the chords held under it (was sparse: a bar of two silent);
     * its throws (GSUB_RG) the classic (the row), perreo (92 .. 100, the clap on the dembow, sixteenths, the 808's
     * 3 3 2 sliding, the vocal chops on the snare's steps) and romantico (88 .. 94, the 808 in the
     * tresillo, a guitar picked) */
    {"REGGAETON", 90, 96, 0, {SC_MIN, SC_MIN, SC_MIN, SC_PHRY}, DM_REGG, 100, KIT_808, 0, HC_FLAT, FL_HAT, 0,
     {BA_REGG, BA_RGTRES, BA_REGG}, 29, KM_DOUBLE, 70, {PR_AEOL, PR_I_VI, PR_EPIC}, 1, CS_HOLD, VM_PAD,
     {AP_BROKEN, AP_CYCLE3}, LD_HOOK, AMT(95, 95, 25, 70, 30), {KIT_808, SR_808, SR_PAD, SR_PLUCK, SR_PLUCK}, SR_TEXTURE,
     2, 1, 30, 0, FM_PARTS, 5, HF_POP, GSP_HEAVY},   /* (V10.7.8, the owner: "more aggression and space, arpeggios,
                                                         * street": the kit and the 808 driven (GSP_HEAVY), the 808
                                                         * shorter, the pad under (25, GMIX), the arp a plucked minor riff
                                                         * up front (70: the dembow's broken 3 + 3 + 2, a cycle), the hook
                                                         * sparse, drier) */
    /* (V10.6n) Dominican dembow (El Alfa, Bulova): faster and harsher, more percussion, one chord's stabs again and
     * again, chants. (V10.7.2) its rules: 115 .. 132, the riddim's kick on every beat and its snare 3 + 3 + 2 doubled
     * by the clap, sixteenths of guira and their rolls, the toms' answer, the 808 on the kick (no house offbeat bass),
     * one chord or two (i, i bVII, i bII), a marimba's riff of a few notes looped (no held saw), the chants chopped
     * on the snare's steps (call and response; the held talk box gone); its throws (GSUB_DB) the classic (115 .. 124)
     * and rapido (124 .. 132, the snare's extra hit, the open hat off the beat, the kit driven) */
    {"DEMBOW", 115, 124, 0, {SC_MIN, SC_PHRY, SC_MIN, SC_HARM}, DM_DEMBOW, 100, KIT_808, 0, HC_MACHINE, FL_HAT, 0,
     {BA_REGG, BA_PH808, BA_RGTRES}, 29, KM_DOUBLE, 70, {PR_ONE, PR_I_VII, PR_PHRY}, 2, CS_STAB, VM_POWER,
     {AP_CHOP, AP_CHOP}, LD_OSTINATO, AMT(100, 90, 40, 45, 55), {KIT_808, SR_808, SR_STAB, SR_CHANT, SR_MALLET}, SR_NOISE,
     1, 2, 20, 0, FM_PARTS, 0, HF_LOOP, 0},
    /* ---- electro, breaks */
    /* (gen6) Anthony Rother (Sex With The Machines, Hacker), DMX Krew, Cybotron: a pure 808 broken (the kick on 1, the
     * & of 2, 3's &), the clap on 2 and 4, 16th hats, cowbell; a syncopated bass (root octave seventh), i VII VI VII, a
     * saw lead in octaves and fifths; dry */
    {"ELECTRO", 128, 135, 0, {SC_MIN, SC_PHRY, SC_MIN, SC_PHRY}, DM_ELECTRO, 100, KIT_808, 0, HC_MACHINE, FL_TOM, 0,
     {BA_ROTHER, BA_SYNTHW, BA_ANGULAR}, 36, KM_INTERLOCK, 40, {PR_ELEC, PR_I_VII, PR_ELEC}, 1, CS_STAB, VM_SUS,
     {AP_OCTAVES, AP_SEQ}, LD_ELEC, AMT(90, 90, 30, 30, 50), {KIT_808, SR_HOOK, SR_STAB, SR_CHIP, SR_LEAD}, SR_VOXLEAD,
     1, 2, 20, 1, FM_CLUB, 0, HF_LOOP, 0},
    /* The Chemical Brothers, Plump DJs, Hybrid: a funky breakbeat, an acid bass, rave stabs, a hoover */
    {"BREAKBEAT", 125, 140, 6, {SC_MIN, SC_DOR, SC_PHRY, SC_MIN}, DM_BREAKS, 100, KIT_BREAKS, 3, HC_FUNK, FL_LINEAR, 1,
     {BA_ACID, BA_JUNGLE, BA_ACID}, 33, KM_INTERLOCK, 50, {PR_ONE, PR_I_IV, PR_I_VI}, 2, CS_STAB, VM_RICH,
     {AP_UPDOWN, AP_BROKEN}, LD_OSTINATO, AMT(95, 90, 45, 30, 40), {KIT_BREAKS, SR_ACID, SR_STAB, SR_PLUCK, SR_HOOVER}, SR_NOISE,
     1, 1, 40, 0, FM_PARTS, 20, HF_LOOP, 0},
    /* Photek, Ed Rush & Optical (Wormhole), Goldie: the breakbeat, a reese bass, a dark pad */
    {"DRUM & BASS", 172, 176, 0, {SC_MIN, SC_DOR, SC_MIN, SC_PHRY}, DM_DNB, 100, KIT_BREAKS, 2, HC_FLAT, FL_LINEAR, 0,
     {BA_DNB, BA_JUNGLE, BA_WOBBLE}, 29, KM_DOUBLE, 90, {PR_I_VI, PR_DEEP4, PR_MINOR4}, 2, CS_SPARSE, VM_RICH,
     {AP_BROKEN, AP_CYCLE3}, LD_SPARSE, AMT(95, 95, 50, 20, 30), {KIT_BREAKS, SR_REESE, SR_PAD, SR_PLUCK, SR_LEAD}, SR_NOISE,
     2, 4, 38, 1, FM_CLUB, 0, HF_JAZZY, 0},   /* (the GENE mix pass: the pedal's place a four-chord minor loop, i iv VII
                                                         * III, half its throws a single chord before; drier, 50 -> 38) */
    /* ---- (V10.6n) IDM, three ways (the drums a break programmed, then edited: gen.c gidm_edit; the harmony warm and
     * simple: the complexity in one dimension) */
    /* Autechre, Aphex Twin's edits (Windowlicker, Come to Daddy), Squarepusher: the Amen edited bar by bar (rolls,
     * stutters, a beat moved, half a bar reversed, the kick out), an acid or FM bass, a lullaby on bells */
    {"IDM GLITCH", 135, 165, 0, {SC_MIN, SC_PHRY, SC_DOR, SC_HARM}, DM_AMEN, 100, KIT_CLICK, 2, HC_FLAT, FL_ROLL, 0,
     {BA_ACID, BA_ANGULAR, BA_JUNGLE}, 33, KM_INTERLOCK, 50, {PR_MINOR4, PR_I_VI, PR_IV_I}, 2, CS_SPARSE, VM_PAD,
     {AP_WALK, AP_CYCLE5}, LD_MOTIF, AMT(90, 70, 45, 55, 50), {KIT_CLICK, SR_FMBASS, SR_EP, SR_BELL, SR_BELL}, SR_TEXTURE,
     4, 5, 45, 1, FM_ARC, 10, HF_JAZZY, 3},
    /* Boards of Canada, Floating Points, Aphex Twin's SAW: a soft hip hop break laid back, a sub on long roots, warm
     * sevenths and ninths a chord every four bars, pads with the tape's wobble, a few notes of melody */
    {"IDM AMBIENT", 85, 100, 30, {SC_DOR, SC_MAJ, SC_MIN, SC_LYD}, DM_BOC, 100, KIT_SOFT, 1, HC_FUNK, FL_NONE, 2,
     {BA_PEDAL, BA_FIFTHS, BA_DUST}, 31, KM_SUSTAIN, 110, {PR_I_IV, PR_I_VI, PR_SUS}, 4, CS_SWELL, VM_RICH,
     {AP_GLASS, AP_UPDOWN}, LD_SPARSE, AMT(60, 60, 90, 35, 40), {KIT_SOFT, SR_SUB, SR_PAD, SR_BELL, SR_SINEKEY}, SR_DRONE,
     4, 4, 75, 0, FM_ARC, 30, HF_COLOR, 1 | GSP_WOW},
    /* Aphex Twin (drukqs), Venetian Snares made musical: a break edited, a 3:4 arp and lead over a three-bar cycle,
     * detuned, bells and chips, playful */
    {"IDM WEIRD", 100, 130, 12, {SC_LYD, SC_DOR, SC_PHRY, SC_MIX}, DM_BREAKS, 100, KIT_CLICK, 1, HC_FLAT, FL_KICK16, 0,
     {BA_ANGULAR, BA_DUST, BA_ACID}, 33, KM_INTERLOCK, 55, {PR_I_II, PR_I_III, PR_DEEPVI}, 2, CS_SPARSE, VM_SUS,
     {AP_CYCLE3, AP_WALK}, LD_MOTIF, AMT(80, 70, 50, 60, 45), {KIT_CLICK, SR_FMBASS, SR_EP, SR_CHIP, SR_BELL}, SR_TEXTURE,
     4, 3, 50, 1, FM_ARC, 10, HF_COLOR, 2 | GSP_POLY | GSP_WOW},
    /* ---- kraut, EBM */
    /* NEU! (Hallogallo), Can, Harmonia: the motorik beat, one chord, a droning guitar, the layers coming in over time */
    {"KRAUT", 118, 140, 0, {SC_MAJ, SC_MIX, SC_DOR, SC_MIN}, DM_MOTORIK, 100, KIT_SOFT, 0, HC_FLAT, FL_NONE, 0,
     {BA_MOTORIK, BA_ROCK8, BA_PUMP8}, 33, KM_DOUBLE, 60, {PR_ONE, PR_I_IV, PR_I_VII}, 4, CS_PULSE, VM_PLAIN,
     {AP_UPDOWN, AP_GLASS}, LD_TINTI, AMT(90, 95, 45, 50, 35), {KIT_SOFT, SR_ROCKBASS, SR_GTR, SR_PLUCK, SR_LEAD}, SR_DRONE,
     0, 1, 45, 0, FM_ARC, 30, HF_LOOP | HX_BOR, 0},
    /* (V10.6p, the owner: "heavier and faster, pseudo-industrial, analog basses slightly detuned") DAF, Nitzer Ebb,
     * Front 242, Die Krupps (115 .. 130) and today's techno-body (Kontravoid, Phase Fatale, Schwefelgelb, Imperial Black
     * Unit: 125 .. 140): a distorted four on the floor, the snare or the clap on 2 and 4, metal; a sequenced bass in
     * sixteenths (Nitzer Ebb's, DAF's, Front 242's gallop, the offbeat octaves: gen.c gen_pick's eight) on two saws a
     * few cents apart through the MS20, accents on its filter; a second sequence an octave up, a shout every bar or
     * two; no chords, dry */
    {"EBM", 118, 138, 0, {SC_PHRY, SC_MIN, SC_PHRY, SC_MIN}, DM_EBM, 100, KIT_INDUS, 0, HC_MACHINE, FL_NONE, 1,
     {BA_NITZER1, BA_DAF, BA_GALLOP}, 33, KM_SUSTAIN, 80, {PR_ONE, PR_PHRY, PR_I_VI}, 4, CS_STAB, VM_POWER,
     {AP_SEQ, AP_OCTAVES}, LD_EBM, AMT(100, 100, 0, 20, 30), {KIT_INDUS, SR_EBMBASS, SR_STAB, SR_PLUCK, SR_CHANT}, SR_NOISE,
     1, 0, 8, 0, FM_PARTS, 0, HF_LOOP, GSP_HEAVY},
    /* ---- the room */
    /* Nils Frahm, Olafur Arnalds, Max Richter: a felt piano's patterns and its melody, strings held, no drums */
    {"NEO CLASSIC", 70, 112, 0, {SC_MIN, SC_DOR, SC_MAJ, SC_MEL}, DM_NONE, 0, KIT_SOFT, 0, HC_FLAT, FL_NONE, 0,
     {BA_NEOARP, BA_PEDAL, BA_FIFTHS}, 36, KM_SUSTAIN, 100, {PR_50S, PR_NEOCL, PR_CYCLE}, 1, CS_HOLD, VM_RICH,
     {AP_GLASS, AP_ALBERTI}, LD_TINTI, AMT(0, 60, 70, 90, 55), {KIT_SOFT, SR_PIANO, SR_STRINGS, SR_PIANO, SR_PIANO}, SR_STRINGS,
     0, 1, 70, 0, FM_ARC, 30, HF_CINE | HX_RAISE | HX_SEC | HX_BOR, 0},
    /* (V10.6n) Bach's fugues: a subject on the lead, its answer a fifth under on the second voice a bar later, the bass
     * two octaves under it two bars later (each voice moved by the chord it lands on: gen.c gfugue), the voices entering
     * one by one (the rows), harpsichords over an organ's continuo, minor keys, no drums */
    {"FUGA", 72, 96, 0, {SC_MIN, SC_HARM, SC_MIN, SC_DOR}, DM_NONE, 0, KIT_SOFT, 0, HC_FLAT, FL_NONE, 0,
     {BA_FIFTHS, BA_NEOARP, BA_PEDAL}, 36, KM_SUSTAIN, 100, {PR_CYCLE, PR_I_V, PR_ANDAL}, 1, CS_HOLD, VM_PAD,
     {AP_UP, AP_UPDOWN}, LD_MOTIF, AMT(0, 80, 35, 80, 90), {KIT_SOFT, SR_HARPSI, SR_ORGAN, SR_HARPSI, SR_HARPSI}, SR_ORGAN,
     2, 0, 45, 0, FM_ARC, 20, HF_CINE | HX_RAISE, GSP_FUGUE},
    /* ---- (V10.6n) rock against the machines */
    /* Rammstein, Nine Inch Nails, Ministry: distorted power chords palm-muted on a riff, the bass's sixteenths or its
     * gallop, the double kick's bursts, the snare on 2 and 4, phrygian */
    {"ELECTRO METAL", 100, 140, 0, {SC_PHRY, SC_MIN, SC_PHRY, SC_HARM}, DM_METAL, 100, KIT_INDUS, 0, HC_FLAT, FL_KICK16, 0,
     {BA_PUNK16, BA_GALLOP, BA_ROCK8}, 28, KM_DOUBLE, 45, {PR_ONE, PR_PHRY, PR_I_VI}, 2, CS_CHUG, VM_POWER,
     {AP_OCTAVES, AP_PULSE}, LD_OSTINATO, AMT(100, 90, 90, 25, 35), {KIT_INDUS, SR_EBMBASS, SR_DIST, SR_STAB, SR_LEAD}, SR_NOISE,
     1, 2, 20, 0, FM_PARTS, 15, HF_LOOP, 0},
    /* The Prodigy, the Chemical Brothers, Underworld, Atari Teenage Riot: a live kit's backbeat and its crash, distorted
     * riffs over a sequenced sixteenth bass, a hoover */
    {"TECHNO ROCK", 125, 140, 0, {SC_MIN, SC_DOR, SC_PHRY, SC_MIN}, DM_TROCK, 100, KIT_BREAKS, 2, HC_FUNK, FL_TOM, 1,
     {BA_SEQ16, BA_ROLL16, BA_PUNK16}, 33, KM_INTERLOCK, 45, {PR_ONE, PR_I_VII, PR_PHRY}, 2, CS_CHUG, VM_POWER,
     {AP_SEQ, AP_OCTAVES}, LD_OSTINATO, AMT(95, 85, 75, 40, 35), {KIT_BREAKS, SR_TECHBASS, SR_DIST, SR_PLUCK, SR_HOOVER}, SR_NOISE,
     1, 2, 30, 0, FM_PARTS, 35, HF_LOOP, 0},
    /* ---- (V10.6n, the owner's three more) */
    /* Beach House, Cocteau Twins, Slowdive: a slow drum machine, washed pads in chorus, a clean guitar's arpeggio in a
     * long delay, I vi IV and I iii IV in sevenths and ninths, a dreamy line, the shimmer's reverb */
    {"DREAM POP", 70, 100, 0, {SC_MAJ, SC_LYD, SC_MAJ, SC_MIX}, DM_DREAM, 100, KIT_SOFT, 0, HC_FLAT, FL_NONE, 0,
     {BA_FIFTHS, BA_ROCK8, BA_NEOARP}, 33, KM_SUSTAIN, 100, {PR_DREAM, PR_DREAM2, PR_I_IV}, 2, CS_SWELL, VM_RICH,
     {AP_UPDOWN, AP_GLASS}, LD_SPARSE, AMT(55, 70, 90, 75, 45), {KIT_SOFT, SR_SUB, SR_PAD, SR_GTR, SR_VOXLEAD}, SR_WALL,
     2, 3, 85, 2, FM_ARC, 30, HF_COLOR, GSP_WOW},
    /* (V10.6p, the owner: "a very archetypal genre": the pitched 808 cowbell's riff and heavy distorted drums) drift
     * phonk (Kordhell, DVRST, PlayaPhonk, MoonDeity, Interworld) after Memphis (DJ Screw, Three 6 Mafia): half time
     * 130 .. 156 (the snare on 3, eighths of hat rolled in triplets) or (V10.6u) Brazilian phonk's 125 .. 135 (the funk's
     * claps on 3 6 12 over the beat: gen.c gen_pick), the 808 cowbell's minor or phrygian riff of a bar or two in
     * sixteenths up front (gen_riff), the 808 distorted, long, sliding up the octave, Memphis's vocal chops off the
     * beat, one dark chord or two */
    {"PHONK", 130, 156, 0, {SC_PHRY, SC_MIN, SC_PHRY, SC_HARM}, DM_PHONK, 100, KIT_808, 0, HC_FLAT, FL_HAT, 0,
     {BA_WOBBLE, BA_PH808, BA_DNB}, 29, KM_DOUBLE, 110, {PR_ONE, PR_I_VI, PR_PHRY}, 2, CS_SPARSE, VM_PAD,
     {AP_CHOP, AP_CHOP}, LD_RIFF, AMT(100, 100, 20, 40, 100), {KIT_808, SR_808, SR_PAD, SR_CHANT, SR_COWBELL}, SR_TEXTURE,
     1, 1, 30, 0, FM_PARTS, 0, HF_LOOP, GSP_HEAVY},
    /* (V10.6p, the owner: MC Bin Laden, MC Fioti's Bum Bum Tam Tam, Brazil's funk) funk carioca's tamborzao (DJ Luciano
     * Oliveira's, after the Volt Mix's 808) and Sao Paulo's beat, mandelao and automotivo: 128 .. 136 (V10.6u: the
     * owner's demos 135, the loops 130; a quarter of the throws funk 150's 145 .. 152: gen.c gen_pick), the 808 kick on
     * 1 3 6 12 or the one alone, the claps on 3 6 12 (a 909's, bright), the atabaque or chopped toms, the 808's sub
     * held on the kick, distorted; one minor chord or two stabbed; the MC's chant chopped off the beat; a hook: Bach's
     * flute (Bum Bum Tam Tam) or a bell's riff (automotivo's) */
    {"BAILE FUNK", 128, 136, 0, {SC_MIN, SC_MIN, SC_PHRY, SC_HARM}, DM_BAILE, 100, KIT_909, 0, HC_FLAT, FL_TOM, 0,
     {BA_TAMBOR, BA_MANDEL, BA_FUNK3}, 29, KM_DOUBLE, 55, {PR_ONE, PR_I_VII, PR_I_IV}, 2, CS_STAB, VM_PLAIN,
     {AP_CHOP, AP_CHOP}, LD_RIFF, AMT(100, 100, 0, 30, 70), {KIT_909, SR_808, SR_STAB, SR_CHANT, SR_COWBELL}, SR_TEXTURE,
     1, 2, 15, 2, FM_PARTS, 5, HF_LOOP, GSP_HEAVY},   /* (V10.7.8, the owner: "ultra-minimalist, weird, aggressive": no
                                                         * chords, the chops sparse, the 808 short (GATE 55) and up front
                                                         * (GMIX), drier) */
    /* (V10.6w, the owner: "jazz has to sound like jazz") a swing quartet in swung eighths (DM_JAZZ: a step an eighth,
     * the tempo shown the half of the felt: 80 a ballad .. 300 bebop, by the throw's energy; V10.6x, the owner: "jazz has every BPM"): the ride's "ding, ding-da ding", the hi-hat's foot on 2 and 4,
     * the kick feathered, the snare's soft comping; the walking bass in quarters on the chord's tones and its approach
     * notes; ii-V-I and the I vi ii V turnaround in sevenths (secondary dominants, the turnaround's ii V in the last
     * chord: gen.c gplan), the EP's rootless voicings comped on Charleston's 1 and 2's "and" (gen_pads CS_CHARL), a
     * vibraphone's line with its chromatic approaches */
    {"JAZZ", 40, 150, 70, {SC_MAJ, SC_MAJ, SC_DOR, SC_MIN}, DM_JAZZ, 100, KIT_SOFT, 2, HC_FLAT, FL_NONE, 1,
     {BA_WALK, BA_WALK2, BA_WALK3}, 28, KM_DOUBLE, 100, {PR_JAZZ, PR_TURN, PR_JAZZ}, 1, CS_CHARL, VM_RICH,
     {AP_WALK, AP_UPDOWN}, LD_MOTIF, AMT(100, 95, 80, 0, 55), {KIT_SOFT, SR_UPRIGHT, SR_EP, SR_PIANO, SR_SINEKEY}, SR_TEXTURE,
     0, 1, 30, 0, FM_PARTS, 70, HF_JAZZ | HX_RAISE | HX_SEC | HX_APPR, 0},
    /* (V10.6w, the owner: "all the hip hop inside one HIP HOP") a throw one of five (GSUB below): trap (Metro Boomin,
     * Southside: half time, sixteenth hats rolled, the 808 long and sliding, bells over minor chords), UK drill (the
     * snare's pickup, the hats' skips, the 808 sliding, dark strings), pluggnb (trap's beat soft, an EP's seventh chords),
     * boom bap (Pete Rock, DJ Premier: 86 .. 96 swung, a break's kit, a sub, soul keys) and lofi (the break late and
     * soft, 70 .. 88 swung more, an EP's sevenths with the tape's wobble) */
    {"HIP HOP", 136, 150, 0, {SC_MIN, SC_MIN, SC_HARM, SC_PHRY}, DM_TRAP, 100, KIT_808, 0, HC_FLAT, FL_HAT, 0,
     {BA_WOBBLE, BA_PH808, BA_JUNGLE}, 29, KM_DOUBLE, 110, {PR_I_VI, PR_MINOR4, PR_AEOL}, 2, CS_SPARSE, VM_RICH,
     {AP_BROKEN, AP_UPDOWN}, LD_HOOK, AMT(95, 100, 50, 40, 45), {KIT_808, SR_808, SR_PAD, SR_PLUCK, SR_BELL}, SR_TEXTURE,
     0, 1, 35, 0, FM_PARTS, 0, HF_POP | HX_RAISE, 0},
};
#define NGENRE (sizeof GENRE / sizeof GENRE[0])
/* (0.9.9-gen1) each genre's PATTERN GRAMMAR past its maps and archetypes (GENRE's order): on the bass, how many ACCENTs
 * (off the kick), SLIDEs (a note tied into the next: its glide), octave jumps (the offbeat sixteenths), 0 .. 3 each;
 * DRUM-X GLUE (0 .. 3: x 4; 0.9.9-gen2: 2 at most, moderate); the closed hats ratcheted now and then (a condition: every other loop); MUTATE on the arp
 * or the chords (WHAT PATTERN, LOOP 4, ORBIT: a pattern orbiting the piece); (gen3) LIFT 0 .. 7: the bass, chords, arp and
 * lead's level over the drums, (LIFT - 2) x 1.5 dB (GG_LIFT: the drums at most 60 % of the parts' energy, gen_test). The
 * 303's own rules (gen_acid) are the acid bass's */
#define GG(acc, sld, oct, glue, rat, mut, lift) (uint16_t)((acc) | (sld) << 2 | (oct) << 4 | (glue) << 6 | (rat) << 8 | (mut) << 9 | (lift) << 10)
#define GG_ACC(g) ((g) & 3u)
#define GG_SLD(g) (((g) >> 2) & 3u)
#define GG_OCT(g) (((g) >> 4) & 3u)
#define GG_GLUE(g) (((g) >> 6) & 3u)
#define GG_RAT(g) (((g) >> 8) & 1u)
#define GG_MUT(g) (((g) >> 9) & 1u)
#define GG_LIFT(g) ((int32_t)(((g) >> 10) & 7u) * 3 - 6)   /* (gen3) the bass, chords, arp, lead's LEVEL over the drums: -6 ..
                                                         * +15 (half dB), by how the genre's kit weighs (gen.c gen_mix) */
static const uint16_t GGRAM[] = {   /* (gen3: + LIFT, 2 = none; V10.6n: the owner's list) */
    GG(1, 0, 0, 1, 0, 1, 5), GG(2, 0, 0, 2, 1, 0, 7), GG(1, 0, 1, 2, 0, 0, 7),   /* MINIMAL TECHNO HARD TECHNO HARD BASS */
    GG(1, 0, 1, 0, 0, 0, 6), GG(0, 1, 0, 0, 0, 0, 4), GG(1, 0, 1, 1, 0, 1, 5), GG(1, 0, 0, 1, 0, 0, 7),   /* HOUSE DEEP HOUSE TECH HOUSE TRIBAL (V10.6z10: its
                                                         * parts lifted 3 dB more: its percussion a throw's own) */
    GG(1, 1, 0, 0, 0, 0, 4), GG(1, 2, 0, 1, 1, 0, 4), GG(2, 1, 0, 2, 1, 0, 6),   /* CUMBIA REGGAETON DEMBOW */
    GG(2, 0, 2, 1, 1, 0, 4), GG(1, 0, 0, 1, 1, 0, 2), GG(1, 2, 0, 2, 1, 0, 2),   /* ELECTRO BREAKBEAT DRUM & BASS */
    GG(1, 1, 1, 1, 1, 1, 1), GG(0, 1, 0, 0, 0, 1, 2), GG(1, 1, 1, 1, 1, 1, 1),   /* IDM GLITCH IDM AMBIENT IDM WEIRD */
    GG(0, 0, 1, 0, 0, 1, 2), GG(2, 0, 1, 2, 0, 0, 5), GG(0, 0, 0, 0, 0, 0, 2), GG(0, 0, 0, 0, 0, 0, 2),   /* KRAUT EBM NEO CLASSIC FUGA */
    GG(2, 0, 0, 2, 0, 0, 6), GG(1, 0, 1, 1, 1, 0, 4),   /* ELECTRO METAL TECHNO ROCK */
    GG(0, 1, 0, 0, 0, 1, 2), GG(1, 2, 0, 2, 1, 0, 5), GG(2, 1, 0, 2, 0, 0, 4),   /* DREAM POP PHONK BAILE FUNK */
    GG(0, 0, 0, 0, 0, 0, 4), GG(1, 2, 0, 1, 1, 0, 5),   /* JAZZ HIP HOP */
};
_Static_assert(sizeof GGRAM / sizeof GGRAM[0] == NGENRE, "GGRAM: a grammar a genre");
/* (the GENE mix pass, the owner: "it sounds unprofessional, previous versions better mixed") each genre's mix, a role's
 * level past gen_mix's in 1.5 dB steps (-8 .. 7, a nibble a role: DRUM BASS, PAD LEAD, ARP TEX; the texture's 4 dB),
 * set from the stems of eight throws a genre on the host, each part's loudness while it plays (0.37 s windows, gated
 * as LUFS) beside the drums': the club's bass ~3 dB under them (V10.6z9: 4 .. 13), drum & bass's level with them (the
 * reese's weight, V10.6k's balance), the chords ~9, the hooks ~7, the arp ~11, the texture ~20 under (V10.6z9: 11 ..
 * 21, 18 .. 31, 23 .. 38, 40 .. 51: buried); the drums still the loudest. The genres the owner tuned by ear (JAZZ,
 * CUMBIA, NEO CLASSIC, FUGA, DREAM POP, ELECTRO METAL, TECHNO ROCK) at their balance as it was, RTRIM's evening made
 * up (tests/gen_test.c: the mix pass) */
#define GMX(d, b, p, l, a, t) {(uint8_t)(((d) & 15) | ((b) & 15) << 4), (uint8_t)(((p) & 15) | ((l) & 15) << 4), (uint8_t)(((a) & 15) | ((t) & 15) << 4)}
static const uint8_t GMIX[][3] = {
    GMX(0, 7, -2, 0, 5, 0),   /* MINIMAL TECHNO */
    GMX(-3, 0, 0, 0, 7, 6),   /* HARD TECHNO (V10.7.2: the bass +1.5 dB, V10.6z10's basses ~1.8 under) */
    GMX(-3, 7, 3, 7, 0, 6),   /* HARD BASS */
    GMX(-1, 2, 1, 0, 7, 0),   /* HOUSE */
    GMX(-2, 4, 2, 0, 7, 0),   /* DEEP HOUSE (V10.7.2: the bass +1.5, z10's ~1.2 under) */
    GMX(-3, 1, -1, 0, 7, 0),   /* TECH HOUSE (V10.7.2: the bass +1.5, z10's ~1.6 under) */
    GMX(-3, 4, 1, 0, 7, 5),   /* TRIBAL */
    GMX(0, 1, 2, 2, 0, -2),   /* CUMBIA */
    GMX(-1, 1, -5, 2, 5, 4),   /* REGGAETON (V10.7.8, the owner's reference: the drums up front, the 808 up, the pad, the hook
                                                         * and the arp under them, the pad under the arp) */
    GMX(-3, 2, -2, 7, 7, 4),   /* DEMBOW (V10.7.2: the marimba's riff and the chants' arp up: phonk's levels) */
    GMX(-3, 0, 1, 7, 7, 5),   /* ELECTRO */
    GMX(-2, 2, -2, 5, 6, 5),   /* BREAKBEAT (V10.7.2: the bass +1.5, z10's ~1.8 under) */
    GMX(-3, -3, 0, 6, 5, 6),   /* DRUM & BASS */
    GMX(-1, -3, 1, 4, -1, 3),   /* IDM GLITCH */
    GMX(-3, 0, 0, 3, 3, 4),   /* IDM AMBIENT */
    GMX(0, -3, 0, 2, 5, 3),   /* IDM WEIRD */
    GMX(-3, 1, 5, 7, 7, 5),   /* KRAUT */
    GMX(-3, 7, 0, 7, 7, 0),   /* EBM (the owner's dark techno / industrial reference: its sub's rumble up; the
                                                         * other throws' FAT BASS as it was, at the level's top already) */
    GMX(0, 0, 4, 3, 2, 3),   /* NEO CLASSIC */
    GMX(0, 0, 5, 1, 1, 2),   /* FUGA */
    GMX(0, -2, -2, 4, -5, 1),   /* ELECTRO METAL */
    GMX(0, -1, 0, 3, 0, 1),   /* TECHNO ROCK */
    GMX(-1, 4, -3, -1, 2, 3),   /* DREAM POP */
    GMX(-4, 1, -2, 7, 7, 4),   /* PHONK (V10.7.2: the drums -1.5, z10's DNA ~2 dB over the parts) */
    GMX(-3, 2, -3, 7, 4, 4),   /* BAILE FUNK (V10.7.8: the 808 4.5 dB up, the chops under) */
    GMX(0, 0, 0, 1, 0, -2),   /* JAZZ */
    GMX(-3, -2, -1, 7, 5, 4),   /* HIP HOP */
};
_Static_assert(sizeof GMIX / sizeof GMIX[0] == NGENRE, "GMIX: a mix a genre");
/* (V10.6z10, the owner: "every throw of a genre the same song") each genre's groove DNA (GENRE's order, EXPLORE last):
 * the two steps a throw's own extra kicks take, its idiom's (gen.c gdna: none to three of them a throw, in every bar;
 * the anchors never moved: house's and techno's four on the floor, the dembow's riddim, jazz's feathered kick (its
 * bombs on the "and"s), the motorik), the lanes the throw may take from the pools (GPL_*: the club's top end as
 * before, now every genre's but its identity's: cumbia's guiro, the dembow's and reggaeton's 3 + 3 + 2, tribal's toms
 * and congas, electro's cowbell, phonk's (the lead's) cowbell, baile funk's atabaque, jazz's ride; V10.6m's hats of
 * their maps: cumbia, reggaeton, tribal, breakbeat) and the snare and the clap swapped; a percussion lane a euclid
 * now and then (the pools' last two) */
#define GPL_HAT 1u
#define GPL_OH 2u
#define GPL_RS 4u
#define GPL_CB 8u
#define GPL_TM 16u
#define GPL_SWAP 0x2000u                                /* (gdna: the snare and the clap swapped) */
#define GPL_ALL 31u
#define GD(a, b, pl) (uint16_t)((a) | (b) << 4 | (pl) << 8)   /* (two places, the second likelier past COMPLEX 40; the lanes) */
static const uint16_t GDNA[NGENRE + 1u] = {
    GD(0, 0, GPL_ALL), GD(14, 15, GPL_ALL), GD(14, 15, GPL_HAT | GPL_RS | GPL_CB | GPL_TM),   /* MINIMAL TECHNO (the
                                                         * owner's reference: the four on the floor alone, the sub in its
                                                         * gaps) HARD TECHNO HARD BASS (its open hat) */
    GD(7, 15, GPL_ALL) | GPL_SWAP, GD(3, 10, GPL_ALL) | GPL_SWAP, GD(6, 14, GPL_ALL) | GPL_SWAP,   /* HOUSE DEEP HOUSE TECH HOUSE */
    GD(7, 10, GPL_OH | GPL_RS | GPL_CB),                                      /* TRIBAL (its toms and congas) */
    GD(0, 0, 0), GD(0, 0, GPL_OH | GPL_RS | GPL_CB | GPL_TM), GD(0, 0, GPL_OH | GPL_RS | GPL_CB),   /* CUMBIA (V10.7.2:
                                                         * its rules: the kick's 2/4, the guiro, the llamador; V10.7.5:
                                                         * the bell on 2 its own, no pool) REGGAETON
                                                         * DEMBOW (the riddim; V10.7.2: its guira's sixteenths, the clap on
                                                         * the snare, the toms' answer) */
    GD(3, 7, GPL_HAT | GPL_OH | GPL_RS | GPL_TM),                              /* ELECTRO (its cowbell) */
    GD(3, 14, GPL_OH | GPL_RS | GPL_CB | GPL_TM), GD(7, 13, GPL_ALL),          /* BREAKBEAT DRUM & BASS */
    GD(6, 13, GPL_ALL), GD(3, 13, GPL_HAT | GPL_RS | GPL_TM), GD(3, 6, GPL_ALL),   /* IDM GLITCH IDM AMBIENT IDM WEIRD */
    GD(14, 14, GPL_HAT | GPL_OH), GD(14, 15, GPL_RS | GPL_CB), 0, 0,          /* KRAUT (the motorik, its pickup) EBM NEO
                                                         * CLASSIC FUGA */
    GD(5, 9, GPL_HAT | GPL_OH | GPL_TM), GD(3, 7, GPL_HAT | GPL_OH | GPL_TM) | GPL_SWAP,   /* ELECTRO METAL TECHNO ROCK */
    GD(3, 14, GPL_HAT | GPL_OH | GPL_RS), GD(3, 13, GPL_RS | GPL_TM), GD(10, 13, GPL_RS),   /* DREAM POP PHONK BAILE FUNK */
    GD(3, 11, 0), GD(3, 13, GPL_OH | GPL_RS | GPL_CB), GD(6, 11, GPL_ALL) | GPL_SWAP,   /* JAZZ (its bombs) HIP HOP EXPLORE */
};
/* (V10.6z10) the IDM throws' breaks (GSP_EDIT 1 .. 3: AMBIENT, WEIRD, GLITCH): half the throws one of three others */
static const uint8_t IDM_ALT[3][3] = {{DM_BOOMBAP, DM_FUNKY, DM_THINK}, {DM_AMEN, DM_FUNKY, DM_THINK}, {DM_BREAKS, DM_FUNKY, DM_THINK}};
/* (V10.6w) a genre's throws, each its own style (gen.c gen_pick: a throw one of them, its fields over the genre's row,
 * a mutant's never): HIP HOP's trap (the row), drill, pluggnb, boom bap, lofi; TECHNO ROCK's techno rock (the row),
 * rock, synth 80s. The throws' count, then a throw its count of (field, value) pairs, the pairs */
#define GSO(f, v) (uint8_t)__builtin_offsetof(ggenre_t, f), (uint8_t)(v)
static const uint8_t GSUB_HH[] = {
    5,   /* HIP HOP */
    0,
    4, GSO(drum, DM_DRILL), GSO(bpm_lo, 138), GSO(bass[0], BA_DNB), GSO(snd[GP_CHORDS], SR_STRINGS),
    5, GSO(kit, KIT_SOFT), GSO(snd[GP_CHORDS], SR_EP), GSO(snd[GP_LEAD], SR_SINEKEY), GSO(prog[0], PR_DEEPVI), GSO(hx, HF_JAZZY),
    14, GSO(drum, DM_BOOMBAP), GSO(bpm_lo, 86), GSO(bpm_hi, 96), GSO(swing, 30), GSO(human, 45), GSO(kit, KIT_BREAKS),
    GSO(ghosts, 1), GSO(fills, FL_NONE), GSO(bass[0], BA_DUST), GSO(bass[1], BA_JUNGLE), GSO(bass[2], BA_DEEPWALK),
    GSO(snd[GP_BASS], SR_SUB), GSO(snd[GP_CHORDS], SR_KEYS), GSO(hx, HF_JAZZY),
    15, GSO(drum, DM_BOC), GSO(bpm_lo, 70), GSO(bpm_hi, 88), GSO(swing, 45), GSO(human, 60), GSO(kit, KIT_SOFT),
    GSO(ghosts, 1), GSO(fills, FL_NONE), GSO(bass[0], BA_FIFTHS), GSO(bass[1], BA_DUST), GSO(bass[2], BA_PEDAL),
    GSO(snd[GP_BASS], SR_SUB), GSO(snd[GP_CHORDS], SR_EP), GSO(hx, HF_JAZZY), GSO(spec, GSP_WOW),
};
/* (V10.7.2) the latin genres' throws (gen_tables.c DM_CUMB2 ..): CUMBIA's sonidera (the row), villera, digital;
 * REGGAETON's classic (the row), romantico, perreo; DEMBOW's classic (the row), rapido */
static const uint8_t GSUB_CU[] = {
    3,   /* CUMBIA */
    0,
    8, GSO(drum, DM_CUMB2), GSO(bpm_lo, 88), GSO(bpm_hi, 98), GSO(bass[0], BA_CUMB3), GSO(bass[2], BA_CUMB3), GSO(cbars, 2),
    GSO(amt[GP_BASS], 55), GSO(amt[GP_CHORDS], 30),     /* (V10.7.8: RKT's music low under the drums) */
    8, GSO(drum, DM_CUMB3), GSO(bpm_lo, 90), GSO(bpm_hi, 100), GSO(bass[0], BA_FIFTHS), GSO(bass[2], BA_CUMB2),
    GSO(snd[GP_BASS], SR_SUB), GSO(amt[GP_ARP], 35), GSO(kit, KIT_808),
};
static const uint8_t GSUB_RG[] = {
    3,   /* REGGAETON */
    0,
    6, GSO(bpm_lo, 88), GSO(bpm_hi, 94), GSO(bass[0], BA_RGTRES), GSO(arp[0], AP_UPDOWN), GSO(snd[GP_ARP], SR_MALLET),
    GSO(spec, 0),                                       /* (V10.7.8: romantico's kit and 808 clean) */
    9, GSO(drum, DM_REGG2), GSO(bpm_lo, 92), GSO(bpm_hi, 100), GSO(bass[0], BA_PH808), GSO(bass[2], BA_PH808),
    GSO(arp[0], AP_CHOP), GSO(arp[1], AP_CHOP), GSO(snd[GP_ARP], SR_CHANT), GSO(prog[1], PR_MINOR4),
};
static const uint8_t GSUB_DB[] = {
    2,   /* DEMBOW */
    0,
    5, GSO(drum, DM_DEMB2), GSO(bpm_lo, 124), GSO(bpm_hi, 132), GSO(bass[2], BA_REGG), GSO(spec, GSP_HEAVY),
};
/* (the owner's references) MINIMAL TECHNO's swing a throw's: straight (the row), 56 %, 64 % (G_SWING 30, 70: core.h
 * swing_step_len); EBM's fifth throw (one in five; the four of before the row's, gen.c gen_pick's EBM block) the dark
 * techno / industrial one at 114 .. 120 (the reference 117): its map (DM_EBM5), the sub's rumble after the kick
 * (BA_RUMB, the kick's duck: GSP_PUMP), the mid riff on the lead's track (LD_RIFF2: FAT BASS, BA_RFE), phrygian, one
 * chord (gen.c glatin), the club's plan in a song (the groove 16 bars, the break 8, the drop) */
static const uint8_t GSUB_MT[] = {3, 0, 1, GSO(swing, 30), 1, GSO(swing, 70)};
static const uint8_t GSUB_EB[] = {
    5,   /* EBM */
    0, 0, 0, 0,
    17, GSO(drum, DM_EBM5), GSO(bpm_lo, 114), GSO(bpm_hi, 120), GSO(bass[0], BA_RUMB), GSO(bass[1], BA_RUMB),
    GSO(bass[2], BA_RUMB), GSO(bass_lo, 28), GSO(snd[GP_BASS], SR_SUB), GSO(lead, LD_RIFF2), GSO(snd[GP_LEAD], SR_EBMBASS),
    GSO(scale[1], SC_PHRY), GSO(scale[3], SC_PHRY), GSO(prog[0], PR_ONE), GSO(prog[1], PR_ONE), GSO(prog[2], PR_ONE),
    GSO(spec, GSP_HEAVY | GSP_PUMP), GSO(form, FM_CLUB),
};
static const uint8_t GSUB_TR[] = {
    3,   /* TECHNO ROCK */
    0,
    8, GSO(drum, DM_ROCK), GSO(bpm_lo, 112), GSO(bass[0], BA_ROCK8), GSO(bass[1], BA_MOTORIK), GSO(snd[GP_BASS], SR_ROCKBASS),
    GSO(kick_mode, KM_DOUBLE), GSO(human, 55), GSO(prog[0], PR_CYCLE),
    18, GSO(drum, DM_SYN80), GSO(bpm_lo, 100), GSO(bpm_hi, 122), GSO(kit, KIT_909), GSO(bass[0], BA_OCTBNC),
    GSO(bass[1], BA_SYNTHW), GSO(snd[GP_BASS], SR_HOOK), GSO(snd[GP_CHORDS], SR_PAD), GSO(pads, CS_HOLD),
    GSO(voicing, VM_PLAIN), GSO(snd[GP_LEAD], SR_LEAD), GSO(hx, HF_POP | HX_RAISE), GSO(prog[0], PR_AEOL),
    GSO(prog[1], PR_EPIC), GSO(prog[2], PR_ANDAL), GSO(human, 0), GSO(kick_mode, KM_DOUBLE), GSO(wet, 50),
};

/* ------------------------------------------------------------------------------------------- (0.8.6) EXPLORE --- */
/* GENRE's first choice and the dice's default: no genre's template, every one of its rules thrown (gen.c gx_genre).
 * gp.genre keeps its values (0 RANDOM, 1 + a genre's index) and EXPLORE comes after them, so every number that names a
 * genre (the web emulator's web_gen, the tests, the tools) still names it; the knob shows it first (gen_knob) */
#define GEN_EXPLORE ((uint32_t)NGENRE + 1u)
#define GEN_NPOS ((uint32_t)NGENRE + 2u)
                                                        /* (the knob's places: EXPLORE, RANDOM, the genres; V10.6n: the
                                                         * owner's list in its order, every number before it gone: no
                                                         * project keeps one) */
/* its keys: the SCALE page's modes of seven, the exotic ones likeliest (LOC too: its tonic chord a diminished one the
 * plan never takes, the other six degrees do) */
static const uint8_t GX_SCALE[9] = {SC_MAJ, SC_MIN, SC_DOR, SC_MIX, SC_HARM, SC_PHRY, SC_LYD, 10u /* LOC */, SC_MEL};
static const uint8_t GX_SCALE_W[9] = {4, 4, 8, 8, 13, 14, 14, 8, 13};
/* the lead's degrees over the mode: all seven, the pentatonics, hexatonics, a gapped one, an odd five */
static const uint8_t GX_PAL[8] = {0x7Fu, 0x5Du, 0x37u, 0x6Fu, 0x3Bu, 0x77u, 0x2Du, 0x5Bu};
/* the recipes a role may take (beyond its usual ones: a plucked bass, a bell lead, an organ texture ..) */
static const uint8_t GX_BASS[14] = {SR_SUB, SR_ACID, SR_REESE, SR_FMBASS, SR_ROCKBASS, SR_HOOK, SR_WOW, SR_EBMBASS, SR_PLUCK, SR_CHIP, SR_GTR,
                                    SR_808, SR_UPRIGHT, SR_TECHBASS};
static const uint8_t GX_PADS[13] = {SR_PAD, SR_STRINGS, SR_PIANO, SR_ORGAN, SR_EP, SR_STAB, SR_DUBCHORD, SR_HOOVER, SR_WALL, SR_GTR, SR_KEYS, SR_BELL, SR_DRONE};   /* (V10.6n:
                                                         * no VOICE on the chords) */
static const uint8_t GX_ARPS[8] = {SR_PLUCK, SR_BELL, SR_CHIP, SR_KEYS, SR_SINEKEY, SR_EP, SR_GTR, SR_STAB};
static const uint8_t GX_LEADS[8] = {SR_LEAD, SR_VOXLEAD, SR_CHIP, SR_HOOVER, SR_BELL, SR_PLUCK, SR_WOW, SR_ORGAN};
static const uint8_t GX_TEX[8] = {SR_NOISE, SR_TEXTURE, SR_DRONE, SR_CHOIR, SR_PAD, SR_ORGAN, SR_BELL, SR_STRINGS};
/* how often a role keeps the cycle (V10.6n: else half of it or a quarter, over one chord: gen.c gen_make) */
static const uint8_t GX_KEEP[RL_N] = {60, 70, 80, 45, 45, 50};
/* (the fix of V10.6n's: every loop on the bar) the lead's, the arp's, the texture's loop off the bar half the time:
 * beats of their own over the cycle (gen.c gen_make; whole beats: no odd step) */
static const uint8_t GX_LEN[10] = {12, 20, 24, 28, 36, 40, 44, 52, 56, 60};
/* the step components it scatters (gen.c gx_comps): their weights on a melodic track */
static const uint8_t GX_COMP[10] = {CP_RAND, CP_OCT, CP_BEND, CP_ECHO, CP_PULSE, CP_WALK, CP_CLIMB, CP_ARPQ, CP_ARPN,
                                    CP_TONAL};
static const uint8_t GX_COMP_W[10] = {6, 14, 8, 14, 8, 6, 4, 10, 8, 8};   /* (V10.6l, the owner: no JUMP, no REV: steps
                                                         * thrown about are not music, V10.6v: gone; less RAND and WALK) */
/* a DRUM-X sound's own way on a step (LM_X2 .. LM_FLAM) */
static const uint8_t GX_LM_W[LM_N] = {0, 30, 14, 4, 20, 22, 10};
