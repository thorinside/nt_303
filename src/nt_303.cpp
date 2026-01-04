/*
 * NT-303: Open303 TB-303 Emulator for Expert Sleepers Disting NT
 * MIT License - Copyright (c) 2025
 * 
 * Based on Open303 by Robin Schmidt (MIT License)
 * https://github.com/maddanio/open303
 */

#include <distingnt/api.h>
#include <new>
#include <cmath>
#include <cstddef>

namespace {
    constexpr size_t HEAP_SIZE = 8192;
    constexpr size_t BLOCK_SIZE = 64;
    constexpr size_t NUM_BLOCKS = HEAP_SIZE / BLOCK_SIZE;
    
    alignas(8) char heapPool[HEAP_SIZE];
    void* freeList = nullptr;
    size_t heapOffset = 0;
    
    void* allocBlock() {
        if (freeList) {
            void* ptr = freeList;
            freeList = *static_cast<void**>(freeList);
            return ptr;
        }
        if (heapOffset + BLOCK_SIZE > HEAP_SIZE) {
            for (;;) {}
        }
        void* ptr = heapPool + heapOffset;
        heapOffset += BLOCK_SIZE;
        return ptr;
    }
    
    void freeBlock(void* ptr) {
        if (!ptr) return;
        if (ptr < heapPool || ptr >= heapPool + HEAP_SIZE) return;
        *static_cast<void**>(ptr) = freeList;
        freeList = ptr;
    }
}

void* operator new(size_t size) {
    if (size > BLOCK_SIZE) {
        for (;;) {}
    }
    return allocBlock();
}

void* operator new[](size_t size) {
    return operator new(size);
}

void operator delete(void* ptr) noexcept {
    freeBlock(ptr);
}

void operator delete[](void* ptr) noexcept {
    freeBlock(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    freeBlock(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    freeBlock(ptr);
}

#include "compat.h"
#include "rosic_Open303.h"

enum {
    kParamOutput,
    kParamOutputMode,
    kParamCutoff,
    kParamResonance,
    kParamEnvMod,
    kParamDecay,
    kParamAccent,
    kParamWaveform,
    kParamVolume,
    kParamSlideTime,
    kParamMidiChannel,
    kParamPitchCV,
    kParamGate,
    kParamAccentCV,
    kNumParams
};

struct _NT303Algorithm : public _NT_algorithm {
    rosic::Open303 synth;
    
    bool prevGate;
    bool cvNoteActive;
    int currentCVNote;
    int lastMidiChannel;
};

static const _NT_parameter parameters[] = {
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output", 1, 13)
    { .name = "Cutoff",     .min = 200,  .max = 20000, .def = 1000, .unit = kNT_unitHz,      .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Resonance",  .min = 0,    .max = 100,   .def = 50,   .unit = kNT_unitPercent, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Env Mod",    .min = 0,    .max = 100,   .def = 25,   .unit = kNT_unitPercent, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Decay",      .min = 30,   .max = 3000,  .def = 300,  .unit = kNT_unitMs,      .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Accent",     .min = 0,    .max = 100,   .def = 50,   .unit = kNT_unitPercent, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Waveform",   .min = 0,    .max = 100,   .def = 0,    .unit = kNT_unitPercent, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Volume",     .min = -40,  .max = 6,     .def = -12,  .unit = kNT_unitDb,      .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Slide Time", .min = 1,    .max = 200,   .def = 60,   .unit = kNT_unitMs,      .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "MIDI Ch",    .min = 1,    .max = 16,    .def = 1,    .unit = kNT_unitNone,    .scaling = kNT_scalingNone, .enumStrings = NULL },
    NT_PARAMETER_CV_INPUT("Pitch CV", 0, 0)
    NT_PARAMETER_CV_INPUT("Gate", 0, 0)
    NT_PARAMETER_CV_INPUT("Accent CV", 0, 0)
};

static const uint8_t pageSound[] = {
    kParamCutoff,
    kParamResonance,
    kParamEnvMod,
    kParamDecay,
    kParamAccent,
    kParamWaveform
};

static const uint8_t pageAmp[] = {
    kParamVolume,
    kParamSlideTime
};

static const uint8_t pageCV[] = {
    kParamPitchCV,
    kParamGate,
    kParamAccentCV
};

static const uint8_t pageRouting[] = {
    kParamOutput,
    kParamOutputMode,
    kParamMidiChannel
};

static const _NT_parameterPage pages[] = {
    { .name = "Sound",   .numParams = ARRAY_SIZE(pageSound),   .params = pageSound },
    { .name = "Amp",     .numParams = ARRAY_SIZE(pageAmp),     .params = pageAmp },
    { .name = "CV In",   .numParams = ARRAY_SIZE(pageCV),      .params = pageCV },
    { .name = "Routing", .numParams = ARRAY_SIZE(pageRouting), .params = pageRouting },
};

static const _NT_parameterPages parameterPages = {
    .numPages = ARRAY_SIZE(pages),
    .pages = pages,
};

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = ARRAY_SIZE(parameters);
    req.sram = sizeof(_NT303Algorithm);
    req.dram = 0;
    req.dtc = 0;
    req.itc = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs,
                         const _NT_algorithmRequirements& req,
                         const int32_t* specifications) {
    _NT303Algorithm* alg = new (ptrs.sram) _NT303Algorithm();
    
    alg->parameters = parameters;
    alg->parameterPages = &parameterPages;
    
    alg->synth.setSampleRate(NT_globals.sampleRate);
    
    alg->prevGate = false;
    alg->cvNoteActive = false;
    alg->currentCVNote = 60;
    alg->lastMidiChannel = 0;
    
    alg->synth.setCutoff(parameters[kParamCutoff].def);
    alg->synth.setResonance(parameters[kParamResonance].def);
    alg->synth.setEnvMod(parameters[kParamEnvMod].def);
    alg->synth.setDecay(parameters[kParamDecay].def);
    alg->synth.setAccent(parameters[kParamAccent].def);
    alg->synth.setWaveform(parameters[kParamWaveform].def / 100.0);
    alg->synth.setVolume(parameters[kParamVolume].def);
    alg->synth.setSlideTime(parameters[kParamSlideTime].def);
    
    return alg;
}

void parameterChanged(_NT_algorithm* self, int p) {
    _NT303Algorithm* pThis = (_NT303Algorithm*)self;
    
    switch (p) {
        case kParamCutoff:
            pThis->synth.setCutoff(pThis->v[kParamCutoff]);
            break;
        case kParamResonance:
            pThis->synth.setResonance(pThis->v[kParamResonance]);
            break;
        case kParamEnvMod:
            pThis->synth.setEnvMod(pThis->v[kParamEnvMod]);
            break;
        case kParamDecay:
            pThis->synth.setDecay(pThis->v[kParamDecay]);
            break;
        case kParamAccent:
            pThis->synth.setAccent(pThis->v[kParamAccent]);
            break;
        case kParamWaveform:
            pThis->synth.setWaveform(pThis->v[kParamWaveform] / 100.0);
            break;
        case kParamVolume:
            pThis->synth.setVolume(pThis->v[kParamVolume]);
            break;
        case kParamSlideTime:
            pThis->synth.setSlideTime(pThis->v[kParamSlideTime]);
            break;
        case kParamMidiChannel:
            pThis->lastMidiChannel = pThis->v[kParamMidiChannel] - 1;
            break;
    }
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    _NT303Algorithm* pThis = (_NT303Algorithm*)self;
    int numFrames = numFramesBy4 * 4;
    
    const float* pitchCV = nullptr;
    const float* gateCV = nullptr;
    const float* accentCV = nullptr;
    
    if (pThis->v[kParamPitchCV] > 0)
        pitchCV = busFrames + (pThis->v[kParamPitchCV] - 1) * numFrames;
    if (pThis->v[kParamGate] > 0)
        gateCV = busFrames + (pThis->v[kParamGate] - 1) * numFrames;
    if (pThis->v[kParamAccentCV] > 0)
        accentCV = busFrames + (pThis->v[kParamAccentCV] - 1) * numFrames;
    
    float* out = busFrames + (pThis->v[kParamOutput] - 1) * numFrames;
    bool replace = pThis->v[kParamOutputMode];
    
    for (int i = 0; i < numFrames; ++i) {
        if (gateCV) {
            // Schmitt trigger hysteresis: rise at >1.5V, fall at <1.0V
            bool gateHigh = pThis->prevGate 
                ? (gateCV[i] >= 1.0f)   // Already high: stay high until <1.0V
                : (gateCV[i] > 1.5f);   // Currently low: need >1.5V to trigger
            
            if (gateHigh && !pThis->prevGate) {
                int midiNote = cvToMidiNote(pitchCV ? pitchCV[i] : 0.0f);
                bool accent = accentCV && accentCV[i] > 2.5f;
                int velocity = accent ? 127 : 80;
                pThis->synth.noteOn(midiNote, velocity);
                pThis->cvNoteActive = true;
                pThis->currentCVNote = midiNote;
            }
            else if (gateHigh && pThis->prevGate && pitchCV) {
                int newNote = cvToMidiNote(pitchCV[i]);
                if (newNote != pThis->currentCVNote) {
                    bool accent = accentCV && accentCV[i] > 2.5f;
                    pThis->synth.noteOn(newNote, accent ? 127 : 80);
                    pThis->currentCVNote = newNote;
                }
            }
            else if (!gateHigh && pThis->prevGate) {
                // Use allNotesOff to ensure clean release regardless of note tracking
                pThis->synth.allNotesOff();
                pThis->cvNoteActive = false;
            }
            
            pThis->prevGate = gateHigh;
        }
        
        float sample = static_cast<float>(pThis->synth.getSample());
        sample *= 5.0f;
        
        if (replace)
            out[i] = sample;
        else
            out[i] += sample;
    }
}

void midiMessage(_NT_algorithm* self, uint8_t b0, uint8_t b1, uint8_t b2) {
    _NT303Algorithm* pThis = (_NT303Algorithm*)self;
    
    int channel = b0 & 0x0f;
    if (channel != pThis->lastMidiChannel)
        return;
    
    int status = b0 & 0xf0;
    
    switch (status) {
        case 0x90:
            pThis->synth.noteOn(b1, b2);
            break;
        case 0x80:
            pThis->synth.noteOn(b1, 0);
            break;
        case 0xE0: {
            int bend = ((b2 << 7) | b1) - 8192;
            double semitones = bend * 2.0 / 8192.0;
            pThis->synth.setPitchBend(semitones);
            break;
        }
    }
}

bool draw(_NT_algorithm* self) {
    NT_drawText(10, 20, "NT-303");
    return false;
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('N', 's', '0', '3'),
    .name = "NT-303",
    .description = "TB-303 Bass Synth (Open303)",
    .numSpecifications = 0,
    .specifications = NULL,
    .calculateStaticRequirements = NULL,
    .initialise = NULL,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .draw = draw,
    .midiRealtime = NULL,
    .midiMessage = midiMessage,
    .tags = kNT_tagInstrument,
    .hasCustomUi = NULL,
    .customUi = NULL,
    .setupUi = NULL,
    .serialise = NULL,
    .deserialise = NULL,
    .midiSysEx = NULL,
};

uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
        case kNT_selector_version:
            return kNT_apiVersion9;
        case kNT_selector_numFactories:
            return 1;
        case kNT_selector_factoryInfo:
            return (uintptr_t)((data == 0) ? &factory : NULL);
    }
    return 0;
}
