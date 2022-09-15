/*
 * DISTRHO Cardinal Plugin
 * Copyright (C) 2021-2022 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE file.
 */

#define STDIO_OVERRIDE Rackdep

#include <cstdio>
#include <cstring>
#include <list>
#include <string>

namespace rack {
namespace settings {
bool darkMode = true;
int rateLimit = 0;
}
}

#include "nanovg.h"

// fix bogaudio build, another missing symbol
#ifndef NDEBUG
namespace bogaudio {
struct FollowerBase {
    static float efGainMaxDecibelsDebug;
};
float FollowerBase::efGainMaxDecibelsDebug = 12.0f;
}
#endif

// Special nvgRGB for blank panels
extern "C" {
NVGcolor nvgRGBblank(const unsigned char r, const unsigned char g, const unsigned char b)
{
    return rack::settings::darkMode ? nvgRGB(0x20, 0x20, 0x20) : nvgRGB(r, g, b);
}
}

// Compile those nice implementation-in-header little libraries
#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#undef nsvgDelete
#undef nsvgParseFromFile
#include <nanosvg.h>

enum DarkMode {
    kMode21kHz,
    kModeAaronStatic,
    kModeAlefsbits,
    kModeAlgoritmarte,
    kModeArableInstruments,
    kModeAudibleInstruments,
    kModeBidoo,
    kModeCf,
    kModeDrumKit,
    kModeESeries,
    kModeHetrickCV,
    kModeJW,
    kModeLilacLoop,
    kModeLittleUtils,
    kModeKocmoc,
    kModeMyth,
    kModeNonlinearcircuits,
    kModeParableInstruments,
    kModePathSet,
    kModeVoxglitch,
    kModeWhatTheRack,
};

// Custom Cardinal filtering
static const struct {
    const DarkMode mode;
    const char* const filename;
    const char* shapeIdsToIgnore[5];
    const int shapeNumberToIgnore;
} svgFilesToInvertForDarkMode[] = {
    // MIT
    { kMode21kHz, "/21kHz/res/Panels/D_Inf.svg", {}, -1 },
    { kMode21kHz, "/21kHz/res/Panels/PalmLoop.svg", {}, -1 },
    { kMode21kHz, "/21kHz/res/Panels/TachyonEntangler.svg", {}, -1 },
    // MIT
    { kModeAaronStatic, "/AaronStatic/res/ChordCV.svg", {}, -1 },
    { kModeAaronStatic, "/AaronStatic/res/DiatonicCV.svg", {}, -1 },
    { kModeAaronStatic, "/AaronStatic/res/RandomNoteCV.svg", {}, -1 },
    { kModeAaronStatic, "/AaronStatic/res/ScaleCV.svg", {}, -1 },
    // GPL3.0-or-later
    { kModeAlefsbits, "/alefsbits/res/blank6hp.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/fibb.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/logic.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/math.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/mlt.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/noize.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/octsclr.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/polyrand.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/shift.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/simplexandhold.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/simplexvco.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/steps.svg", {}, -1 },
    { kModeAlefsbits, "/alefsbits/res/switch.svg", {}, -1 },
    // GPL3.0-or-later
    { kModeAlgoritmarte, "/Algoritmarte/res/Clockkky.svg", {}, -1 },
    { kModeAlgoritmarte, "/Algoritmarte/res/CyclicCA.svg", {}, -1 },
    { kModeAlgoritmarte, "/Algoritmarte/res/HoldMeTight.svg", {}, -1 },
    { kModeAlgoritmarte, "/Algoritmarte/res/MusiFrog.svg", {}, -1 },
    { kModeAlgoritmarte, "/Algoritmarte/res/MusiMath.svg", {}, -1 },
    { kModeAlgoritmarte, "/Algoritmarte/res/Planetz.svg", {}, -1 },
    { kModeAlgoritmarte, "/Algoritmarte/res/Zefiro.svg", {}, -1 },
    // Custom, runtime dark mode used with permission
    { kModeAudibleInstruments, "/AudibleInstruments/res/Blinds.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Braids.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Branches.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Clouds.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Elements.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Frames.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Kinks.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Links.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Marbles.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Peaks.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Plaits.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Rings.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Ripples.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Shades.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Sheep.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Shelves.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Stages.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Streams.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Tides.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Tides2.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Veils.svg", {}, -1 },
    { kModeAudibleInstruments, "/AudibleInstruments/res/Warps.svg", {}, -1 },
    // CC-BY-NC-ND-4.0, runtime dark mode used with permission
    { kModeBidoo, "/Bidoo/res/ACNE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/ANTN.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/BAFIS.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/BANCAU.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/BAR.svg", {"rect833"}, -1 },
    { kModeBidoo, "/Bidoo/res/BISTROT.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/BORDL.svg", {"rect959-3-0-7-5","rect959-3-0-7","rect959-3-0","rect959-3"}, -1 },
    { kModeBidoo, "/Bidoo/res/CANARD.svg", {"rect959-3-7"}, -1 },
    { kModeBidoo, "/Bidoo/res/CHUTE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/DFUZE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/DIKTAT.svg", {"rect843","rect843-0","rect843-0-8"}, -1 },
    { kModeBidoo, "/Bidoo/res/DILEMO.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/DTROY.svg", {"rect959-3"}, -1 },
    { kModeBidoo, "/Bidoo/res/DUKE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/EDSAROS.svg", {"rect959-3-7","rect959-3-7-8","rect959-3-7-8-1","rect959-3-7-8-1-4"}, -1 },
    { kModeBidoo, "/Bidoo/res/EMILE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/ENCORE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/ENCOREExpander.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/FLAME.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/FORK.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/FREIN.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/HCTIP.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/HUITRE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/LAMBDA.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/LATE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/LIMBO.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/LIMONADE.svg", {"rect839","rect839-6"}, -1 },
    { kModeBidoo, "/Bidoo/res/LOURDE.svg", {"rect847","rect847-7","rect847-5","rect847-3"}, -1 },
    { kModeBidoo, "/Bidoo/res/MAGMA.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/MINIBAR.svg", {"rect833"}, -1 },
    { kModeBidoo, "/Bidoo/res/MOIRE.svg", {"rect843","rect843-7"}, -1 },
    { kModeBidoo, "/Bidoo/res/MS.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/MU.svg", {"rect864"}, -1 },
    { kModeBidoo, "/Bidoo/res/OAI.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/OUAIVE.svg", {"rect959-3-7"}, -1 },
    { kModeBidoo, "/Bidoo/res/PERCO.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/PILOT.svg", {"rect843-6-4-5","rect843","rect843-4","rect843-6-4","rect843-6-7"}, -1 },
    { kModeBidoo, "/Bidoo/res/POUPRE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/RABBIT.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/REI.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/SIGMA.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/SPORE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/TIARE.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/TOCANTE.svg", {"rect843"}, -1 },
    { kModeBidoo, "/Bidoo/res/VOID.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/ZINC.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/ZOUMAI.svg", {}, -1 },
    { kModeBidoo, "/Bidoo/res/ZOUMAIExpander.svg", {}, -1 },
    // BSD-3-Clause
    { kModeCf, "/cf/res/ALGEBRA.svg", {}, -1 },
    { kModeCf, "/cf/res/BUFFER.svg", {}, -1 },
    { kModeCf, "/cf/res/CHOKE.svg", {}, -1 },
    { kModeCf, "/cf/res/CUBE.svg", {}, -1 },
    { kModeCf, "/cf/res/CUTS.svg", {}, -1 },
    { kModeCf, "/cf/res/DISTO.svg", {}, -1 },
    { kModeCf, "/cf/res/EACH.svg", {}, -1 },
    { kModeCf, "/cf/res/FOUR.svg", {}, -1 },
    { kModeCf, "/cf/res/FUNKTION.svg", {}, -1 },
    { kModeCf, "/cf/res/L3DS3Q.svg", {}, 3 },
    { kModeCf, "/cf/res/LABEL.svg", {}, -1 },
    { kModeCf, "/cf/res/LEDS.svg", {}, -1 },
    { kModeCf, "/cf/res/LEDSEQ.svg", {}, 3 },
    { kModeCf, "/cf/res/MASTER.svg", {}, -1 },
    { kModeCf, "/cf/res/METRO.svg", {}, -1 },
    { kModeCf, "/cf/res/MONO.svg", {}, -1 },
    { kModeCf, "/cf/res/PATCH.svg", {}, -1 },
    { kModeCf, "/cf/res/PEAK.svg", {}, -1 },
    { kModeCf, "/cf/res/PLAY.svg", {}, -1 },
    { kModeCf, "/cf/res/PLAYER.svg", {}, -1 },
    { kModeCf, "/cf/res/SLIDERSEQ.svg", {}, -1 },
    { kModeCf, "/cf/res/STEPS.svg", {}, -1 },
    { kModeCf, "/cf/res/STEREO.svg", {}, -1 },
    { kModeCf, "/cf/res/SUB.svg", {}, -1 },
    { kModeCf, "/cf/res/trSEQ.svg", {}, -1 },
    { kModeCf, "/cf/res/VARIABLE.svg", {}, -1 },
    // CC0-1.0
    { kModeDrumKit, "/DrumKit/res/Baronial.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/BD9.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/ClosedHH.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/CR78.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/DMX.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/Gnome.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/Marionette.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/OpenHH.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/SBD.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/Sequencer.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/Snare.svg", {}, -1 },
    { kModeDrumKit, "/DrumKit/res/Tomi.svg", {}, -1 },
    // Custom, runtime dark mode used with permission
    { kModeESeries,"/ESeries/res/E340.svg", {}, -1 },
    // CC0-1.0
    { kModeHetrickCV, "/HetrickCV/res/1OpChaos.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/2OpChaos.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/2To4.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/3OpChaos.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/ASR.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/AToD.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/BinaryGate.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/BinaryNoise.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Bitshift.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Boolean3.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/ChaoticAttractors.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/ClockedNoise.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Comparator.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Contrast.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Crackle.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/DataCompander.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Delta.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/DToA.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Dust.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Exponent.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/FBSineChaos.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/FlipFlop.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/FlipPan.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/GateJunction.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Gingerbread.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/LogicCombiner.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/LogicInverter.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/MidSide.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/MinMax.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/RandomGates.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Rotator.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Rungler.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Scanner.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/TrigShaper.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Waveshape.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/XYToPolar.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Blanks/BlankPanel1.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Blanks/BlankPanel2.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Blanks/BlankPanel3.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Blanks/BlankPanel5.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Blanks/BlankPanel6.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Blanks/BlankPanel7.svg", {}, -1},
    { kModeHetrickCV, "/HetrickCV/res/Blanks/BlankPanel8.svg", {}, -1},
    // BSD-3-Clause
    { kModeJW, "/JW-Modules/res/Add5.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/BlankPanel1hp.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/BlankPanelLarge.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/BlankPanelMedium.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/BlankPanelSmall.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/BouncyBalls.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/D1v1de.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/DivSeq.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/EightSeq.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/GridSeq.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/MinMax.svg", {"path38411"}, -1 },
    { kModeJW, "/JW-Modules/res/NoteSeq.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/NoteSeq16.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/NoteSeqFu.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/OnePattern.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/Patterns.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/Pres1t.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/PT.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/Str1ker.svg", {"rect2094","rect995","rect169"}, -1 },
    { kModeJW, "/JW-Modules/res/Trigs.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/WavHeadPanel.svg", {}, -1 },
    { kModeJW, "/JW-Modules/res/XYPad.svg", {}, -1 },
    // GPL3.0-or-later
    { kModeLilacLoop, "/LilacLoop/res/Looper.svg", {}, -1 },
    // EUPL-1.2
    { kModeLittleUtils, "/LittleUtils/res/Bias_Semitone.svg", {}, -1 },
    { kModeLittleUtils, "/LittleUtils/res/ButtonModule.svg", {}, -1 },
    { kModeLittleUtils, "/LittleUtils/res/MulDiv.svg", {}, -1 },
    { kModeLittleUtils, "/LittleUtils/res/PulseGenerator.svg", {}, -1 },
    { kModeLittleUtils, "/LittleUtils/res/TeleportIn.svg", {}, -1 },
    { kModeLittleUtils, "/LittleUtils/res/TeleportOut.svg", {}, -1 },
    // GPL-3.0-or-later
    { kModeKocmoc, "/kocmoc/res/DDLY.svg", {}, -1 },
    { kModeKocmoc, "/kocmoc/res/LADR.svg", {}, -1 },
    { kModeKocmoc, "/kocmoc/res/MUL.svg", {}, -1 },
    { kModeKocmoc, "/kocmoc/res/OP.svg", {}, -1 },
    { kModeKocmoc, "/kocmoc/res/PHASR.svg", {}, -1 },
    { kModeKocmoc, "/kocmoc/res/SKF.svg", {}, -1 },
    { kModeKocmoc, "/kocmoc/res/SVF.svg", {}, -1 },
    { kModeKocmoc, "/kocmoc/res/TRG.svg", {}, -1 },
    // GPL-3.0-or-later
    { kModeMyth, "/myth-modules/res/Mavka.svg", {}, -1 },
    { kModeMyth, "/myth-modules/res/Molphar.svg", {}, -1 },
    // CC0-1.0
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/BOOLs2.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/DoubleNeuronRef.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/LetsSplosh.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/NLC - 4seq.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/NLC - 8 BIT CIPHER.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/NLC - DIVIDE & CONQUER.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/NLC - DIVINE CMOS.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/NLC - GENiE.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/NLC - NEURON.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/NLC - NUMBERWANG.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/NLC - SEGUE.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/NLC - STATUES.svg", {}, -1 },
    { kModeNonlinearcircuits, "/nonlinearcircuits/res/squid-axon-papernoise-panel2.svg", {}, -1 },
    // GPL-3.0-or-later
    { kModePathSet, "/PathSet/res/AstroVibe.svg", {}, -1 },
    { kModePathSet, "/PathSet/res/GlassPane.svg", {}, -1 },
    { kModePathSet, "/PathSet/res/IceTray.svg", {}, -1 },
    { kModePathSet, "/PathSet/res/Nudge.svg", {}, -1 },
    { kModePathSet, "/PathSet/res/OneShot.svg", {}, -1 },
    { kModePathSet, "/PathSet/res/ShiftyExpander.svg", {}, -1 },
    { kModePathSet, "/PathSet/res/ShiftyMod.svg", {}, -1 },
    // BSD-3-Clause
    { kModeVoxglitch, "/voxglitch/res/autobreak_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/bytebeat_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/digital_programmer_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/digital_sequencer_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/digital_sequencer_xp_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/ghosts_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/glitch_sequencer_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/goblins_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/grain_engine_mk2_expander_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/grain_engine_mk2_front_panel_r3.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/grain_fx_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/hazumi_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/looper_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/repeater_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/samplerx8_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/satanonaut_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/wav_bank_front_panel.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/wav_bank_mc_front_panel_v2.svg", {}, -1 },
    { kModeVoxglitch, "/voxglitch/res/xy_front_panel.svg", {}, -1 },
    // WTFPL
    { kModeWhatTheRack, "/WhatTheRack/res/WhatTheJack.svg", {}, -1 },
    { kModeWhatTheRack, "/WhatTheRack/res/WhatTheMod.svg", {}, -1 },
    { kModeWhatTheRack, "/WhatTheRack/res/WhatTheRack.svg", {}, -1 },
};

enum LightMode {
    kMode8Mode,
    kModeAutinn,
    kModeBefaco,
    kModeCardinal,
    kModeFehlerFabrik,
    kModeForsitan,
    kModeFundamental,
    kModeGoodSheperd,
    kModeH4n4,
    kModeHamptonHarmonics,
    kModeLomas,
    kModeMockba,
    kModeMog,
    kModePrism,
    kModeRepelzen,
    kModeSonusmodular,
};

static const struct {
    const LightMode mode;
    const char* const filename;
    const char* shapeIdsToIgnore[5];
    const int shapeNumberToIgnore;
} svgFilesToInvertForLightMode[] = {
    // BSD
    { kMode8Mode, "/8Mode/res/SNsoft_Panel.svg", {}, -1 },
    // GPLv3+
    { kModeAutinn, "/Autinn/res/AmpModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/BassModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/CVConverterModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/ComponentLibrary", {}, -1 },
    { kModeAutinn, "/Autinn/res/DeadbandModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/DigiModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/DiseeModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/FilModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/FlopperModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/ImpModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/JetteModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/MelodyModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/MeraModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/Mixer6Module.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/NapModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/NonModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/OxcartModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/RebelModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/RetriModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/SawModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/SjipModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/SquareModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/VibratoModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/VxyModule.svg", {}, -1 },
    { kModeAutinn, "/Autinn/res/ZodModule.svg", {}, -1 },
    // Custom, runtime light mode used with permission
    { kModeBefaco, "/Befaco/res/components/Knurlie.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/ABC.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/ADSR.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/ChoppingKinky.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/DualAtenuverter.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/EvenVCO.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/HexmixVCA.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/Kickall.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/Mex.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/Mixer.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/Morphader.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/Muxlicer.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/NoisePlethora.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/Percall.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/Rampage.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/STMix.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/SamplingModulator.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/SlewLimiter.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/SpringReverb.svg", {}, -1 },
    { kModeBefaco, "/Befaco/res/panels/StereoStrip.svg", {}, -1 },
    // GPLv3+
    { kModeCardinal, "/Cardinal/res/AudioFile.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/AudioToCVPitch.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/Carla.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/ExpanderMIDI.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/glBars.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/HostAudio.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/HostCV.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/HostMIDI.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/HostMIDICC.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/HostMIDIGate.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/HostMIDIMap.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/HostParameters.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/HostParamsMap.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/HostTime.svg", {}, -1 },
    { kModeCardinal, "/Cardinal/res/Ildaeil.svg", {}, -1 },
    // TODO chowdsp
    // GPLv3+
    { kModeFehlerFabrik, "/FehlerFabrik/res/Arpanet.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Aspect.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Botzinger.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Chi.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Components", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Fax.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Lilt.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Luigi.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Monte.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Nova.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Planck.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/PSIOP.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Rasoir.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Sigma.svg", {}, -1 },
    { kModeFehlerFabrik, "/FehlerFabrik/res/Components/FFHexScrew.svg", {}, -1 },
    // GPLv3+
    { kModeForsitan, "/forsitan-modulare/res/alea.svg", {}, -1 },
    { kModeForsitan, "/forsitan-modulare/res/cumuli.svg", {}, -1 },
    { kModeForsitan, "/forsitan-modulare/res/deinde.svg", {}, -1 },
    { kModeForsitan, "/forsitan-modulare/res/interea.svg", {}, -1 },
    { kModeForsitan, "/forsitan-modulare/res/palette.svg", {}, -1 },
    { kModeForsitan, "/forsitan-modulare/res/pavo.svg", {}, -1 },
    // GPLv3+
    { kModeFundamental, "/Fundamental/res/8vert.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/ADSR.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Delay.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/LFO.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Merge.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/MidSide.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Mixer.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Mutes.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Noise.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Octave.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Pulses.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Quantizer.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Random.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/SEQ3.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Scope.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/SequentialSwitch1.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/SequentialSwitch2.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Split.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/Sum.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/VCA-1.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/VCA.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/VCF.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/VCMixer.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/VCO.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/WTLFO.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/WTVCO.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/components/ADSR-bg.svg", {}, -1 },
    { kModeFundamental, "/Fundamental/res/components/Scope-bg.svg", {}, -1 },
    // GPLv3+
    { kModeGoodSheperd, "/GoodSheperd/res/Holzschnabel.svg", {}, -1 },
    { kModeGoodSheperd, "/GoodSheperd/res/Hurdle.svg", {}, -1 },
    { kModeGoodSheperd, "/GoodSheperd/res/SEQ3st.svg", {}, -1 },
    { kModeGoodSheperd, "/GoodSheperd/res/Seqtrol.svg", {}, -1 },
    { kModeGoodSheperd, "/GoodSheperd/res/Stable16.svg", {}, -1 },
    { kModeGoodSheperd, "/GoodSheperd/res/Stall.svg", {}, -1 },
    { kModeGoodSheperd, "/GoodSheperd/res/Switch1.svg", {}, -1 },
    { kModeGoodSheperd, "/GoodSheperd/res/components/SquareSwitch_0.svg", {}, -1 },
    { kModeGoodSheperd, "/GoodSheperd/res/components/SquareSwitch_1.svg", {}, -1 },
    // GPLv3+
    { kModeH4n4, "/h4n4-modules/res/XenQnt.svg", {}, -1 },
    // MIT
    { kModeHamptonHarmonics, "/HamptonHarmonics/res/Arp.svg", {}, -1 },
    { kModeHamptonHarmonics, "/HamptonHarmonics/res/Progress.svg", {}, -1 },
    // GPLv3+
    { kModeLomas, "/LomasModules/res/AdvancedSampler.svg", {}, -1 },
    { kModeLomas, "/LomasModules/res/GateSequencer.svg", {}, -1 },
    // MIT
    { kModeMockba, "/MockbaModular/res/Blank.svg", {}, -1 },
    // TODO
    // CC0
    { kModeMog, "/Mog/res/Network.svg", {}, -1 },
    { kModeMog, "/Mog/res/Nexus.svg", {}, -1 },
    // CC-BY-NC-ND-4.0
    // TODO Orbits - non compat license
    // CC-BY-SA-4.0
    { kModePrism, "/Prism/res/prism_Droplet.svg", {}, -1 },
    { kModePrism, "/Prism/res/prism_Rainbow.svg", {}, -1 },
    { kModePrism, "/Prism/res/RainbowScaleExpander.svg", {}, -1 },
    // CC-BY-SA-4.0
    { kModeRepelzen, "/repelzen/res/reface/reburst_bg.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/refold_bg.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/repelzen_bg1.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/repelzen_bg2.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/repelzen_bg3.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/repelzen_bg4.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/repelzen_bg5.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/repelzen_bg6.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/repelzen_bg7.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/repelzen_bg8.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/retrig_bg.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/rewin_bg.svg", {}, -1 },
    { kModeRepelzen, "/repelzen/res/reface/rexmix_bg.svg", {}, -1 },
    // GPLv3+
    { kModeSonusmodular, "/sonusmodular/res/addiction.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/bitter.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/bymidside.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/campione.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/chainsaw.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/ctrl.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/deathcrush.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/fraction.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/harmony.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/ladrone.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/luppolo.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/luppolo3.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/micromacro.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/mrcheb.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/multimulti.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/neurosc.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/oktagon.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/osculum.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/paramath.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/piconoise.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/pith.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/pusher.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/ringo.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/scramblase.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/tropicana.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/twoff.svg", {}, -1 },
    { kModeSonusmodular, "/sonusmodular/res/yabp.svg", {}, -1 },
    // TODO xtrnt
    // TODO zzc, needs permission
};

static inline
unsigned int invertColor(const unsigned int color) noexcept
{
    return (color & 0xff000000)
         | (0xff0000 - (color & 0xff0000))
         | (0xff00 - (color & 0xff00))
         | (0xff - (color & 0xff));
}

static inline
bool invertPaintForDarkMode(const DarkMode mode, NSVGshape* const shape, NSVGpaint& paint, const char* const svgFileToInvert)
{
    if (paint.type == NSVG_PAINT_LINEAR_GRADIENT)
    {
        switch (mode)
        {
        // Special case for DrumKit background gradient
        case kModeDrumKit:
            std::free(paint.gradient);
            paint.type = NSVG_PAINT_COLOR;
            paint.color = 0xff191919;
            return true;
        // Special case for PathSet shifty gradient
        case kModePathSet:
            paint.gradient->stops[0].color = 0xff7c4919; // 50% darker than main blue
            paint.gradient->stops[1].color = 0xff5b3a1a; // 33.3% darker than main blue
            return false;
        default:
            break;
        }
    }

    if (paint.type == NSVG_PAINT_NONE)
        return true;
    if (paint.type != NSVG_PAINT_COLOR)
        return false;

    switch (mode)
    {
    // Special case for Bidoo red color
    case kModeBidoo:
        if (paint.color == 0xff001fcd)
        {
            paint.color = 0xcf8b94c4;
            return true;
        }
        if (paint.color == 0xff000000 && shape->stroke.type == NSVG_PAINT_COLOR)
        {
            switch (shape->stroke.color)
            {
            case 0xff777777:
            case 0xff7c7c7c:
            case 0xff828282:
            case 0xffb1b1b1:
            case 0xffb2b2b2:
                return false;
            }
        }
        break;
    // Special case for JW-Modules colors
    case kModeJW:
        switch (paint.color)
        {
        // do nothing
        case 0x320997ff:
        case 0x32099aff:
        case 0x3209f1ff:
        case 0x3209f3ff:
        case 0x32fc1a8f:
        case 0x32fc1a90:
        case 0x32fc9418:
        case 0x32fc9619:
        case 0xc7fc9619:
        case 0xff050505:
        case 0xffead7be:
        case 0xfff7a540:
        case 0xfffa9c2a:
        case 0xfffc9619:
        case 0xfffcb654:
        case 0xfffd9c17:
        case 0xffffffff:
            return false;
        // make more transparent
        case 0xffbad6eb:
        case 0xffbae8eb:
        case 0xffeabed5:
        case 0xffead6bd:
            paint.color = 0x32000000 | (paint.color & 0xffffff);
            return true;
        // make it more white
        case 0xffa0a0a0:
            paint.color = 0xffc0c0c0;
            return true;
        }
        break;
    // Special case for Lilac
    case kModeLilacLoop:
        switch (paint.color)
        {
        // main bg (custom)
        case 0xffd5d5da:
            paint.color = 0xff242228;
            return true;
        // main color (do nothing)
        case 0xffbfb7d7:
            return false;
        // screws (hide)
        case 0xffc8c8cf:
        case 0xffbcbcbc:
        case 0xffb1b1bb:
        case 0xffacacac:
        case 0xff898991:
        case 0xff727272:
            paint.color = 0x00000000;
            return true;
        }
        break;
    // Special case for Nonlinear Circuits
    case kModeNonlinearcircuits:
        switch (paint.color)
        {
            case 0xff9a7900:
            case 0xff96782c:
            case 0xff6a07ae:
            case 0xffcf8044:
            case 0xff2ac6ba:
            case 0xff5ba85c:
            case 0xffa97b00:
            case 0xff9f7a00:
            case 0xffff7300:
            case 0xffa47b00:
            case 0xffb09423:
                return false;
            case 0xffffffff:
                paint.color = 0x00000000;
                return true;
        }
        break;
    // Special case for PathSet colors
    case kModePathSet:
        // only invert main colors for Nudge.svg
        if (std::strcmp(svgFileToInvert, "/PathSet/res/Nudge.svg") == 0)
        {
            switch (paint.color)
            {
            case 0xffe6e6e6:
                paint.color = 0xff191919;
                return true;
            case 0xff000000:
                paint.color = 0xffffffff;
                return true;
            default:
                return false;
            }
        }
        // everything else
        else
        {
            switch (paint.color)
            {
            // main blue tone
            case 0xffdf7a1a:
            case 0xffe78a31:
            case 0xffc26a16:
                if (shape->opacity == 0.5f && std::strcmp(svgFileToInvert, "/PathSet/res/AstroVibe.svg") == 0)
                {
                    shape->opacity = 0.2f;
                    return true;
                }
                if (shape->opacity == 0.25f)
                    shape->opacity = 0.75f;
                return false;
            // bottom logo stuff, set to full white
            case 0xff000000:
                if (shape->stroke.type != NSVG_PAINT_NONE)
                {
                    paint.color = 0xffffffff;
                    return true;
                }
                break;
            /*
            // OneShot beverage
            case 0xff021755:
            case 0xff03299a:
            case 0xff032ba2:
                return false;
            */
            // pink step 2 (pink with 50% opacity on bg)
            case 0xffef73ea:
                paint.color = 0xff812d7d;
                return true;
            // pink step 3 (pink with 33.3% opacity on bg)
            case 0xfff49ff0:
                paint.color = 0xff4d234c;
                return true;
            // pink and orange
            case 0xffe941e2:
            case 0xff698efb:
            // pink and orange (translucent)
            case 0x4de941e2:
            case 0x62698efb:
                return false;
            // blue darker 1 (blue with 50% opacity on bg)
            case 0xffde944f:
            case 0xffe3b080:
            case 0xffe4cbb3:
            case 0xfff5c99f:
            case 0xfff6d1b0:
                paint.color = 0xff7c4919;
                return true;
            // blue darker 2 (blue with 33.3% opacity on bg)
            case 0xffe5d9cd:
            case 0xfff8dcc2:
            case 0xffe1a265:
                paint.color = 0xff5b3a1a;
                return true;
            // blue darker 3 (blue with 25% opacity on bg)
            case 0xffe5cbb3:
                paint.color = 0xff4b321a;
                return true;
            }
        }
        break;
    // Special case for voxglitch colors
    case kModeVoxglitch:
        switch (paint.color)
        {
        // wavbank blue
        case 0xffc5ae8a:
        // various black
        case 0xff121212:
        case 0xff2a2828:
            return false;
        // satanonaut
        case 0xff595959:
            paint.color = 0x7f3219ac;
            return true;
        }
        break;
    default:
        break;
    }

    switch (paint.color)
    {
    // scopes or other special things (do nothing)
    case 0x40ffffff:
    case 0xff0000aa:
    case 0xff004200:
    case 0xff2b281e:
    case 0xff2d2827:
    case 0xff303030:
    case 0xff362c23:
    case 0xff40352c:
    case 0xff5735d9:
    case 0xff5935d8:
    case 0xff5c26d9:
    case 0xff6140db:
    case 0xffa09400:
    case 0xffa19400:
    case 0xffa7a100:
    case 0xffa8a200:
    case 0xffaba102:
    case 0xff22a5e9:
    case 0xff6140da:
    case 0xff119ee6:
    case 0xff2432ed:
    case 0xff0095fe:
    case 0xff4d9a4d:
    case 0xff4d4d9a:
    case 0xff0187fc:
        return false;
    // pure black (convert to not quite pure white)
    case 0xff000000:
        paint.color = 0xffd0d0d0;
        return true;
    // all others (direct invert)
    default:
        paint.color = invertColor(paint.color);
        return true;
    }
}

static inline
bool invertPaintForLightMode(const LightMode mode, NSVGshape* const shape, NSVGpaint& paint)
{
    if (paint.type == NSVG_PAINT_LINEAR_GRADIENT)
    {
        switch (mode)
        {
        case kModeFundamental:
            paint.gradient->stops[0].color = 0xffffffff;
            paint.gradient->stops[1].color = 0xffe6d6d6;
            return true;
        default:
            for (int i=0; i<paint.gradient->nstops; ++i)
                paint.gradient->stops[i].color = invertColor(paint.gradient->stops[i].color);
            return true;
        }
    }

    if (paint.type == NSVG_PAINT_RADIAL_GRADIENT && mode == kModeMog)
    {
        std::free(paint.gradient);
        paint.type = NSVG_PAINT_COLOR;
        paint.color = 0xffe5e5e5;
        return true;
    }

    if (paint.type == NSVG_PAINT_NONE)
        return true;
    if (paint.type != NSVG_PAINT_COLOR)
        return false;

    switch (mode)
    {
    case kMode8Mode:
        switch (paint.color)
        {
        case 0xff000000:
            if (std::strcmp(shape->id, "rect1211") == 0)
                break;
            return false;
        case 0xff1a1a1a:
            if (std::strcmp(shape->id, "rect1523") != 0)
                break;
            return false;
        default:
            return false;
        }
        break;
    case kModeAutinn:
        switch (paint.color)
        {
        // red stripe
        case 0xff0a115e:
            paint.color = 0xffa1a8f5;
            return true;
        // logo
        case 0xff00d7ff:
            paint.color = 0xff005868;
            return true;
        }
        break;
    case kModeFehlerFabrik:
        switch (paint.color)
        {
        // make a few colors reverse in luminance/lightness
        case 0xff3edcfc: paint.color = 0xff039fbf; return true;
        case 0xff4a6fff: paint.color = 0xff0024b2; return true;
        case 0xff5c49fd: paint.color = 0xff1502b6; return true;
        case 0xff61a6ff: paint.color = 0xff00459e; return true;
        case 0xff6e97ad: paint.color = 0xff537c93; return true;
        case 0xff78ffb1: paint.color = 0xff008739; return true;
        case 0xffb5cf00: paint.color = 0xff627000; return true;
        case 0xffbfa463: paint.color = 0xff9c8140; return true;
        case 0xffcba5e4: paint.color = 0xff411b5a; return true;
        case 0xffce86ef: paint.color = 0xff58107a; return true;
        case 0xffcf7685: paint.color = 0xff8a303e; return true;
        case 0xffd1e471: paint.color = 0xff798c1b; return true;
        // screw core
        case 0xff1a1a1a: paint.color = 0xffcccccc; return true;
        // keep already darkish colors
        case 0xff6a8800:
        case 0xff7cce00:
            return false;
        }
        break;
    case kModeFundamental:
        switch (paint.color)
        {
        case 0xc0000000:
            paint.color = 0xe6000000;
            return true;
        case 0xff909092:
            paint.color = 0xff000000;
            return true;
        case 0xff000000:
            if (shape->opacity == 0.5f)
            {
                shape->opacity = 0.9f;
                return false;
            }
            break;
        }
        break;
    case kModeGoodSheperd:
        switch (paint.color)
        {
        // background
        case 0xff332e21: paint.color = 0xffdfdacd; return true;
        case 0xff462f17: paint.color = 0xffe8d2ba; return true;
        // jack box overlays
        case 0xff56534a: paint.color = 0xffb6b3aa; return true;
        case 0xffbc9d8e: paint.color = 0xff705142; return true;
        case 0xfeede9e2: paint.color = 0xff1c1812; return true;
        // colors to keep the same
        case 0xff2400fe:
        case 0xffcab39b:
            return false;
        }
        break;
    case kModeH4n4:
        switch (paint.color)
        {
        case 0xffffb380:
            return false;
        case 0xffffccaa:
            paint.color = 0xff572300;
            return true;
        }
        break;
    case kModeMog:
        switch (paint.color)
        {
        case 0xff442499:
        case 0xff587ee2:
        case 0xff1ecae8:
        case 0xff2dd6ac:
        case 0xffcf924c:
        case 0xffd8b3bb:
        case 0xff29165d:
        case 0xff354d89:
        case 0xff127b8d:
        case 0xff1b8269:
        case 0xff7e592e:
        case 0xff836d72:
            return false;
        }
        break;
    case kModePrism:
        switch (paint.color)
        {
        case 0xff000000:
        case 0xff505770:
        case 0xff657c9b:
        case 0xff7ba357:
        case 0xff7f64f2:
        case 0xff99e4ff:
        case 0xffa7ff6c:
        case 0xffc279e2:
        case 0xffe079c4:
        case 0xffe5ff66:
        case 0xffff88d0:
        case 0xffffa369:
            return false;
        case 0xff0f0f0f:
            if (std::strcmp(shape->id, "path10") == 0 || std::strcmp(shape->id, "circle506") == 0)
            {
                paint.color = 0xffffffff;
                return true;
            }
            return false;
        case 0xffbababa:
            paint.color = 0xff000000;
            return true;
        }
        break;
    case kModeRepelzen:
        switch (paint.color)
        {
        case 0xff4c4ccc:
        case 0xff87a610:
        case 0xffb78e09:
            return false;
        case 0xff44bbd8:
            paint.color = 0xff228ba5;
            return true;
        }
        break;
    case kModeSonusmodular:
        switch (paint.color)
        {
        case 0xff2a2aff:
        case 0xff4e4ed3:
        case 0xff55ddff:
        case 0xff87cdde:
        case 0xffdbdbe3:
        case 0xffe9afaf:
            return false;
        case 0xff0a1284:
            paint.color = 0xff7a82f5;
            return true;
        }
        break;
    default:
        break;
    }

    paint.color = invertColor(paint.color);
    return true;
}

extern "C" {
NSVGimage* nsvgParseFromFileCardinal(const char* filename, const char* units, float dpi);
void nsvgDeleteCardinal(NSVGimage*);
}

struct ExtendedNSVGimage {
    NSVGimage* const handle;
    NSVGimage* handleOrig;
    NSVGimage* handleMOD;
    NSVGshape* shapesOrig;
    NSVGshape* shapesMOD;
};

static std::list<ExtendedNSVGimage> loadedDarkSVGs;
static std::list<ExtendedNSVGimage> loadedLightSVGs;

static inline
void nsvg__duplicatePaint(NSVGpaint& dst, NSVGpaint& src)
{
	if (dst.type == NSVG_PAINT_LINEAR_GRADIENT || dst.type == NSVG_PAINT_RADIAL_GRADIENT)
    {
        const size_t size = sizeof(NSVGgradient) + sizeof(NSVGgradientStop)*(src.gradient->nstops-1);
		dst.gradient = static_cast<NSVGgradient*>(malloc(size));
		std::memcpy(dst.gradient, src.gradient, size);
    }
}

static inline
NSVGshape* nsvg__duplicateShapes(NSVGshape* const orig)
{
    NSVGshape* const dup = static_cast<NSVGshape*>(malloc(sizeof(NSVGshape)));
    std::memcpy(dup, orig, sizeof(NSVGshape));
    nsvg__duplicatePaint(dup->fill, orig->fill);
    nsvg__duplicatePaint(dup->stroke, orig->stroke);

    for (NSVGshape* shape2 = dup;;)
    {
        if (shape2->next == nullptr)
            break;

        NSVGshape* const shapedup = static_cast<NSVGshape*>(malloc(sizeof(NSVGshape)));
        std::memcpy(shapedup, shape2->next, sizeof(NSVGshape));
        nsvg__duplicatePaint(shapedup->fill, shape2->next->fill);
        nsvg__duplicatePaint(shapedup->stroke, shape2->next->stroke);
        shape2->next = shapedup;
        shape2 = shapedup;
    }

    return dup;
}

static inline
void deleteExtendedNSVGimage(ExtendedNSVGimage& ext)
{
    if (ext.shapesMOD != nullptr)
    {
        // delete duplicated resources
        for (NSVGshape *next, *shape = ext.shapesMOD;;)
        {
            next = shape->next;

            nsvg__deletePaint(&shape->fill);
            nsvg__deletePaint(&shape->stroke);
            std::free(shape);

            if (next == nullptr)
                break;

            shape = next;
        }

        // revert shapes back to original
        ext.handle->shapes = ext.shapesOrig;
        ext.shapesMOD = nullptr;
    }

    if (ext.handleMOD != nullptr)
    {
        nsvgDelete(ext.handleMOD);
        ext.handleMOD = nullptr;
    }

    if (ext.handleOrig != nullptr)
    {
        std::memcpy(ext.handle, ext.handleOrig, sizeof(NSVGimage));
        std::free(ext.handleOrig);
        ext.handleOrig = nullptr;
    }
}

NSVGimage* nsvgParseFromFileCardinal(const char* const filename, const char* const units, const float dpi)
{
    if (NSVGimage* const handle = nsvgParseFromFile(filename, units, dpi))
    {
        const size_t filenamelen = std::strlen(filename);

        bool hasDarkMode = false;
        bool hasLightMode = false;
        NSVGimage* handleOrig;
        NSVGimage* handleMOD = nullptr;
        NSVGshape* shapesOrig;
        NSVGshape* shapesMOD;

        // Special case for light/dark screws
        if (std::strncmp(filename + (filenamelen-16), "/ScrewSilver.svg", 16) == 0)
        {
            const std::string blackfilename = std::string(filename).substr(0, filenamelen-10) + "Black.svg";
            hasDarkMode = true;
            shapesOrig = shapesMOD = nullptr;
            handleMOD = nsvgParseFromFile(blackfilename.c_str(), units, dpi);
            goto postparse;
        }

        if (std::strncmp(filename + (filenamelen-15), "/ScrewBlack.svg", 15) == 0)
        {
            const std::string silverfilename = std::string(filename).substr(0, filenamelen-9) + "Silver.svg";
            hasLightMode = true;
            shapesOrig = shapesMOD = nullptr;
            handleMOD = nsvgParseFromFile(silverfilename.c_str(), units, dpi);
            goto postparse;
        }

#if 0
        // Special case for GlueTheGiant
        if (std::strstr(filename, "/GlueTheGiant/res/") != nullptr)
        {
            printf("special hack for glue\n");
            if (std::strncmp(filename + (filenamelen - 13), "/BusDepot.svg",     13) == 0 ||
                std::strncmp(filename + (filenamelen - 13), "/BusRoute.svg",     13) == 0 ||
                std::strncmp(filename + (filenamelen - 13), "/EnterBus.svg",     13) == 0 ||
                std::strncmp(filename + (filenamelen - 12), "/ExitBus.svg",      12) == 0 ||
                std::strncmp(filename + (filenamelen - 11), "/GigBus.svg",       11) == 0 ||
                std::strncmp(filename + (filenamelen - 17), "/MetroCityBus.svg", 17) == 0 ||
                std::strncmp(filename + (filenamelen - 12), "/MiniBus.svg",      12) == 0 ||
                std::strncmp(filename + (filenamelen -  9), "/Road.svg",          9) == 0 ||
                std::strncmp(filename + (filenamelen - 14), "/SchoolBus.svg",    14) == 0)
            {
                const std::string nightfilename = std::string(filename).substr(0, filenamelen-4) + "_Night.svg";
                hasDarkMode = true;
                shapesOrig = shapesMOD = nullptr;
                handleMOD = nsvgParseFromFile(nightfilename.c_str(), units, dpi);
                printf("special hack for glue: %s -> %s\n", filename, nightfilename.c_str());
                goto postparse;
            }
        }
#endif

        for (size_t i = 0; i < sizeof(svgFilesToInvertForDarkMode)/sizeof(svgFilesToInvertForDarkMode[0]); ++i)
        {
            const char* const svgFileToInvert = svgFilesToInvertForDarkMode[i].filename;
            const size_t filterlen = std::strlen(svgFileToInvert);

            if (filenamelen < filterlen)
                continue;
            if (std::strncmp(filename + (filenamelen-filterlen), svgFileToInvert, filterlen) != 0)
                continue;

            const DarkMode mode = svgFilesToInvertForDarkMode[i].mode;
            const char* const* const shapeIdsToIgnore = svgFilesToInvertForDarkMode[i].shapeIdsToIgnore;
            const int shapeNumberToIgnore = svgFilesToInvertForDarkMode[i].shapeNumberToIgnore;
            int shapeCounter = 0;

            hasDarkMode = true;
            handleMOD = nullptr;
            shapesOrig = handle->shapes;
            shapesMOD = nsvg__duplicateShapes(shapesOrig);

            // shape paint inversion
            for (NSVGshape* shape = shapesMOD; shape != nullptr; shape = shape->next, ++shapeCounter)
            {
                if (shapeNumberToIgnore == shapeCounter)
                    continue;

                bool ignore = false;
                for (size_t j = 0; j < 5 && shapeIdsToIgnore[j] != nullptr; ++j)
                {
                    if (std::strcmp(shape->id, shapeIdsToIgnore[j]) == 0)
                    {
                        ignore = true;
                        break;
                    }
                }
                if (ignore)
                    continue;

                if (invertPaintForDarkMode(mode, shape, shape->fill, svgFileToInvert))
                    invertPaintForDarkMode(mode, shape, shape->stroke, svgFileToInvert);
            }

            goto postparse;
        }

        for (size_t i = 0; i < sizeof(svgFilesToInvertForLightMode)/sizeof(svgFilesToInvertForLightMode[0]); ++i)
        {
            const char* const svgFileToInvert = svgFilesToInvertForLightMode[i].filename;
            const size_t filenamelen = std::strlen(filename);
            const size_t filterlen = std::strlen(svgFileToInvert);

            if (filenamelen < filterlen)
                continue;
            if (std::strncmp(filename + (filenamelen-filterlen), svgFileToInvert, filterlen) != 0)
                continue;

            const LightMode mode = svgFilesToInvertForLightMode[i].mode;

            hasLightMode = true;
            handleMOD = nullptr;
            shapesOrig = handle->shapes;
            shapesMOD = nsvg__duplicateShapes(shapesOrig);

            // shape paint inversion
            for (NSVGshape* shape = shapesMOD; shape != nullptr; shape = shape->next)
            {
                if (invertPaintForLightMode(mode, shape, shape->fill))
                    invertPaintForLightMode(mode, shape, shape->stroke);
            }

            goto postparse;
        }

        // Special case for AmalgamatedHarmonics background color
        if (handle->shapes != nullptr && handle->shapes->fill.color == 0xff000000)
            if (std::strstr(filename, "/AmalgamatedHarmonics/") != nullptr)
                handle->shapes->fill.color = 0xff191919;

postparse:
        if (handleMOD != nullptr)
        {
            handleOrig = static_cast<NSVGimage*>(malloc(sizeof(NSVGimage)));
            std::memcpy(handleOrig, handle, sizeof(NSVGimage));
        }
        else
        {
            handleOrig = nullptr;
        }

        if (hasDarkMode)
        {
            const ExtendedNSVGimage ext = { handle, handleOrig, handleMOD, shapesOrig, shapesMOD };
            loadedDarkSVGs.push_back(ext);

            if (rack::settings::darkMode)
            {
                if (shapesMOD != nullptr)
                    handle->shapes = shapesMOD;
                else if (handleMOD != nullptr)
                    std::memcpy(handle, handleMOD, sizeof(NSVGimage));
            }
        }

        if (hasLightMode)
        {
            const ExtendedNSVGimage ext = { handle, handleOrig, handleMOD, shapesOrig, shapesMOD };
            loadedLightSVGs.push_back(ext);

            if (!rack::settings::darkMode)
            {
                if (shapesMOD != nullptr)
                    handle->shapes = shapesMOD;
                else if (handleMOD != nullptr)
                    std::memcpy(handle, handleMOD, sizeof(NSVGimage));
            }
        }

        return handle;
    }

    return nullptr;
}

void nsvgDeleteCardinal(NSVGimage* const handle)
{
    for (auto it = loadedDarkSVGs.begin(), end = loadedDarkSVGs.end(); it != end; ++it)
    {
        ExtendedNSVGimage& ext(*it);

        if (ext.handle != handle)
            continue;

        deleteExtendedNSVGimage(ext);
        loadedDarkSVGs.erase(it);
        break;
    }

    for (auto it = loadedLightSVGs.begin(), end = loadedLightSVGs.end(); it != end; ++it)
    {
        ExtendedNSVGimage& ext(*it);

        if (ext.handle != handle)
            continue;

        deleteExtendedNSVGimage(ext);
        loadedLightSVGs.erase(it);
        break;
    }

    nsvgDelete(handle);
}

void switchDarkMode(const bool darkMode)
{
    if (rack::settings::darkMode == darkMode)
        return;

    rack::settings::darkMode = darkMode;

    for (ExtendedNSVGimage& ext : loadedDarkSVGs)
    {
        if (ext.shapesMOD != nullptr)
            ext.handle->shapes = darkMode ? ext.shapesMOD : ext.shapesOrig;
        else if (ext.handleMOD != nullptr)
            std::memcpy(ext.handle, darkMode ? ext.handleMOD : ext.handleOrig, sizeof(NSVGimage));
    }

    for (ExtendedNSVGimage& ext : loadedLightSVGs)
    {
        if (ext.shapesMOD != nullptr)
            ext.handle->shapes = !darkMode ? ext.shapesMOD : ext.shapesOrig;
        else if (ext.handleMOD != nullptr)
            std::memcpy(ext.handle, !darkMode ? ext.handleMOD : ext.handleOrig, sizeof(NSVGimage));
    }
}

namespace rack {
namespace asset {

void destroy() {
    for (auto it = loadedDarkSVGs.begin(), end = loadedDarkSVGs.end(); it != end; ++it)
    {
        ExtendedNSVGimage& ext(*it);
        deleteExtendedNSVGimage(ext);
    }

    for (auto it = loadedLightSVGs.begin(), end = loadedLightSVGs.end(); it != end; ++it)
    {
        ExtendedNSVGimage& ext(*it);
        deleteExtendedNSVGimage(ext);
    }

    loadedDarkSVGs.clear();
    loadedLightSVGs.clear();
}

}
}
