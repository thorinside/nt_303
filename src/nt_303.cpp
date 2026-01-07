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

constexpr size_t DRAM_HEAP_SIZE = 262144;

#ifndef NT_TEST_BUILD
namespace {
    constexpr size_t MIN_BLOCK = 32;
    constexpr size_t ALIGN = 8;
    
    struct BlockHeader {
        size_t size;
        BlockHeader* next;
    };
    
    char* heapPool = nullptr;
    size_t heapSize = 0;
    size_t heapUsed = 0;
    BlockHeader* freeList = nullptr;
    
    size_t alignUp(size_t n) {
        return (n + ALIGN - 1) & ~(ALIGN - 1);
    }
    
    void initHeap(void* ptr, size_t size) {
        heapPool = static_cast<char*>(ptr);
        heapSize = size;
        heapUsed = 0;
        freeList = nullptr;
    }
    
    void* heapAlloc(size_t size) {
        if (!heapPool) return nullptr;
        
        size = alignUp(size + sizeof(BlockHeader));
        if (size < MIN_BLOCK) size = MIN_BLOCK;
        
        BlockHeader** prev = &freeList;
        BlockHeader* curr = freeList;
        while (curr) {
            if (curr->size >= size) {
                *prev = curr->next;
                return reinterpret_cast<char*>(curr) + sizeof(BlockHeader);
            }
            prev = &curr->next;
            curr = curr->next;
        }
        
        if (heapUsed + size > heapSize) {
            return nullptr;
        }
        
        BlockHeader* block = reinterpret_cast<BlockHeader*>(heapPool + heapUsed);
        block->size = size;
        block->next = nullptr;
        heapUsed += size;
        
        return reinterpret_cast<char*>(block) + sizeof(BlockHeader);
    }
    
    void heapFree(void* ptr) {
        if (!ptr || !heapPool) return;
        char* p = static_cast<char*>(ptr);
        if (p < heapPool || p >= heapPool + heapSize) return;
        
        BlockHeader* block = reinterpret_cast<BlockHeader*>(p - sizeof(BlockHeader));
        block->next = freeList;
        freeList = block;
    }
}

void* operator new(size_t size) {
    void* p = heapAlloc(size);
    if (!p) {
        while(1) {}
    }
    return p;
}

void* operator new[](size_t size) {
    return operator new(size);
}

void operator delete(void* ptr) noexcept {
    heapFree(ptr);
}

void operator delete[](void* ptr) noexcept {
    heapFree(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    heapFree(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    heapFree(ptr);
}
#endif

#include "compat.h"
#include "rosic_Open303.h"
#include "nt_soft_takeover.h"

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
    kParamOversampling,
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
    
    float smoothCutoff;
    float smoothResonance;
    float smoothDecay;
    
    float lastSampleRate;
    
    SoftTakeoverState uiState;
};

static char const * const enumStringsOversampling[] = { "1x", "2x", "4x" };

static const _NT_parameter parameters[] = {
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output", 1, 13)
    { .name = "Cutoff",     .min = 20,   .max = 20000, .def = 1000, .unit = kNT_unitHz,      .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Resonance",  .min = 0,    .max = 100,   .def = 50,   .unit = kNT_unitPercent, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Env Mod",    .min = 0,    .max = 100,   .def = 25,   .unit = kNT_unitPercent, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Decay",      .min = 30,   .max = 3000,  .def = 300,  .unit = kNT_unitMs,      .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Accent",     .min = 0,    .max = 100,   .def = 50,   .unit = kNT_unitPercent, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Waveform",   .min = 0,    .max = 100,   .def = 0,    .unit = kNT_unitPercent, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Volume",     .min = -40,  .max = 6,     .def = -12,  .unit = kNT_unitDb,      .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Slide Time", .min = 1,    .max = 200,   .def = 60,   .unit = kNT_unitMs,      .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "Oversample", .min = 0,    .max = 2,     .def = 1,    .unit = kNT_unitEnum,    .scaling = kNT_scalingNone, .enumStrings = enumStringsOversampling },
    { .name = "MIDI Ch",    .min = 0,    .max = 16,    .def = 0,    .unit = kNT_unitNone,    .scaling = kNT_scalingNone, .enumStrings = NULL },
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
    kParamWaveform,
    kParamVolume,
    kParamSlideTime,
    kParamOversampling
};

static const uint8_t pageRouting[] = {
    kParamOutput,
    kParamOutputMode,
    kParamMidiChannel,
    kParamPitchCV,
    kParamGate,
    kParamAccentCV
};

static const _NT_parameterPage pages[] = {
    { .name = "Sound",   .numParams = ARRAY_SIZE(pageSound),   .params = pageSound },
    { .name = "Routing", .numParams = ARRAY_SIZE(pageRouting), .params = pageRouting },
};

static const _NT_parameterPages parameterPages = {
    .numPages = ARRAY_SIZE(pages),
    .pages = pages,
};

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = ARRAY_SIZE(parameters);
    req.sram = sizeof(_NT303Algorithm);
    req.dram = DRAM_HEAP_SIZE;
    req.dtc = 0;
    req.itc = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs,
                         const _NT_algorithmRequirements& req,
                         const int32_t* specifications) {
#ifndef NT_TEST_BUILD
    initHeap(ptrs.dram, DRAM_HEAP_SIZE);
#endif
    
    _NT303Algorithm* alg = new (ptrs.sram) _NT303Algorithm();
    
    alg->parameters = parameters;
    alg->parameterPages = &parameterPages;
    
    alg->synth.setSampleRate(NT_globals.sampleRate);
    alg->lastSampleRate = NT_globals.sampleRate;
    
    alg->prevGate = false;
    alg->cvNoteActive = false;
    alg->currentCVNote = 60;
    alg->lastMidiChannel = 0;
    
    alg->smoothCutoff = (float)parameters[kParamCutoff].def;
    alg->smoothResonance = (float)parameters[kParamResonance].def;
    alg->smoothDecay = (float)parameters[kParamDecay].def;
    
    initSoftTakeover(&alg->uiState);
    
    alg->synth.setCutoff(parameters[kParamCutoff].def);
    alg->synth.setResonance(parameters[kParamResonance].def);
    alg->synth.setEnvMod(parameters[kParamEnvMod].def);
    alg->synth.setDecay(parameters[kParamDecay].def);
    alg->synth.setAccent(parameters[kParamAccent].def);
    alg->synth.setWaveform(parameters[kParamWaveform].def / 100.0);
    alg->synth.setVolume(parameters[kParamVolume].def);
    alg->synth.setSlideTime(parameters[kParamSlideTime].def);
    
    static const int oversamplingValues[] = {1, 2, 4};
    alg->synth.setOversampling(oversamplingValues[parameters[kParamOversampling].def]);
    
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
        case kParamOversampling: {
            static const int oversamplingValues[] = {1, 2, 4};
            pThis->synth.setOversampling(oversamplingValues[pThis->v[kParamOversampling]]);
            break;
        }
        case kParamMidiChannel:
            pThis->lastMidiChannel = pThis->v[kParamMidiChannel] - 1;
            break;
    }
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    _NT303Algorithm* pThis = (_NT303Algorithm*)self;
    int numFrames = numFramesBy4 * 4;
    
    if (NT_globals.sampleRate != pThis->lastSampleRate) {
        pThis->synth.setSampleRate(NT_globals.sampleRate);
        pThis->lastSampleRate = NT_globals.sampleRate;
    }
    
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
    
    float targetCutoff = (float)pThis->v[kParamCutoff];
    float targetRes = (float)pThis->v[kParamResonance];
    float targetDecay = (float)pThis->v[kParamDecay];
    
    constexpr float smoothCoeff = 0.001f;
    
    for (int i = 0; i < numFrames; ++i) {
        pThis->smoothCutoff += smoothCoeff * (targetCutoff - pThis->smoothCutoff);
        pThis->smoothResonance += smoothCoeff * (targetRes - pThis->smoothResonance);
        pThis->smoothDecay += smoothCoeff * (targetDecay - pThis->smoothDecay);
        
        if ((i & 7) == 0) {
            pThis->synth.setCutoff(pThis->smoothCutoff);
            pThis->synth.setResonance(pThis->smoothResonance);
            pThis->synth.setDecay(pThis->smoothDecay);
        }

        if (gateCV) {
            bool gateHigh = pThis->prevGate 
                ? (gateCV[i] >= 1.0f)
                : (gateCV[i] > 1.5f);
            
            if (gateHigh && !pThis->prevGate) {
                bool accent = accentCV && accentCV[i] > 2.5f;
                int velocity = accent ? 127 : 80;
                pThis->synth.noteOn(60, velocity);
                pThis->cvNoteActive = true;
            }
            
            if (gateHigh && pitchCV) {
                float freq = cvToFreq(pitchCV[i]);
                pThis->synth.setOscillatorFrequency(freq);
            }
            
            if (gateHigh && accentCV) {
                float accentLevel = (accentCV[i] - 2.5f) / 2.5f;
                if (accentLevel < 0.0f) accentLevel = 0.0f;
                if (accentLevel > 1.0f) accentLevel = 1.0f;
                pThis->synth.setAccentGain(accentLevel * 0.5);
            }
            
            if (!gateHigh && pThis->prevGate) {
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
    
    int midiChParam = pThis->v[kParamMidiChannel];
    if (midiChParam > 0) {
        int channel = b0 & 0x0f;
        if (channel != midiChParam - 1)
            return;
    }
    
    int status = b0 & 0xf0;
    
    switch (status) {
        case 0x90:
            pThis->synth.noteOn(b1, b2);
            break;
        case 0x80:
            pThis->synth.noteOn(b1, 0);
            break;
        case 0xB0:
            if (b1 == 120 || b1 == 123)
                pThis->synth.allNotesOff();
            break;
        case 0xE0: {
            int bend = ((b2 << 7) | b1) - 8192;
            double semitones = bend * 2.0 / 8192.0;
            pThis->synth.setPitchBend(semitones);
            break;
        }
    }
}

static const char* getParamName(int paramIdx) {
    switch (paramIdx) {
        case kParamCutoff:     return "Cutoff";
        case kParamResonance:  return "Resonance";
        case kParamEnvMod:     return "Env Mod";
        case kParamDecay:      return "Decay";
        case kParamAccent:     return "Accent";
        case kParamWaveform:   return "Waveform";
        case kParamVolume:     return "Volume";
        case kParamSlideTime:  return "Slide";
        default:               return "";
    }
}

static const char* getParamUnit(int paramIdx) {
    switch (paramIdx) {
        case kParamCutoff:     return "Hz";
        case kParamDecay:
        case kParamSlideTime:  return "ms";
        case kParamVolume:     return "dB";
        case kParamResonance:
        case kParamEnvMod:
        case kParamAccent:
        case kParamWaveform:   return "%";
        default:               return "";
    }
}

bool draw(_NT_algorithm* self) {
    _NT303Algorithm* pThis = (_NT303Algorithm*)self;
    
    char buf[32];
    
    NT_drawText(128, 12, "NT-303", 15, kNT_textCentre, kNT_textLarge);
    
    decrementDisplayTimeout(&pThis->uiState);
    
    if (isDisplayActive(&pThis->uiState)) {
        const char* name = getParamName(pThis->uiState.activeParam);
        const char* unit = getParamUnit(pThis->uiState.activeParam);
        
        NT_drawText(128, 36, name, 12, kNT_textCentre, kNT_textNormal);
        
        int len = NT_intToString(buf, pThis->uiState.activeParamValue);
        buf[len++] = ' ';
        int unitLen = 0;
        while (unit[unitLen]) {
            buf[len++] = unit[unitLen++];
        }
        buf[len] = 0;
        
        NT_drawText(128, 52, buf, 15, kNT_textCentre, kNT_textLarge);
    } else {
        NT_drawText(43, 36, "CUT", 8, kNT_textCentre, kNT_textTiny);
        NT_intToString(buf, pThis->v[kParamCutoff]);
        NT_drawText(43, 48, buf, 12, kNT_textCentre);
        
        NT_drawText(85, 36, "RES", 8, kNT_textCentre, kNT_textTiny);
        NT_intToString(buf, pThis->v[kParamResonance]);
        NT_drawText(85, 48, buf, 12, kNT_textCentre);
        
        NT_drawText(128, 36, "ENV", 8, kNT_textCentre, kNT_textTiny);
        NT_intToString(buf, pThis->v[kParamEnvMod]);
        NT_drawText(128, 48, buf, 12, kNT_textCentre);
        
        NT_drawText(171, 36, "DEC", 8, kNT_textCentre, kNT_textTiny);
        NT_intToString(buf, pThis->v[kParamDecay]);
        NT_drawText(171, 48, buf, 12, kNT_textCentre);
        
        NT_drawText(213, 36, "WAV", 8, kNT_textCentre, kNT_textTiny);
        NT_intToString(buf, pThis->v[kParamWaveform]);
        NT_drawText(213, 48, buf, 12, kNT_textCentre);
    }
    
    return true;
}

uint32_t hasCustomUi(_NT_algorithm* self) {
    (void)self;
    return kNT_potL | kNT_potC | kNT_potR |
           kNT_potButtonL | kNT_potButtonC | kNT_potButtonR |
           kNT_encoderL | kNT_encoderR;
}

static const PotConfig potConfigs[3] = {
    { kParamCutoff,   kParamResonance, { 20.0f, 0.0f, true, 1000.0f }, { 0.0f, 100.0f, false, 0.0f } },
    { kParamEnvMod,   kParamDecay,     { 0.0f, 100.0f, false, 0.0f },  { 30.0f, 3000.0f, false, 0.0f } },
    { kParamSlideTime, kParamWaveform, { 1.0f, 200.0f, false, 0.0f },  { 0.0f, 100.0f, false, 0.0f } },
};

void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    _NT303Algorithm* pThis = (_NT303Algorithm*)self;
    int algIndex = NT_algorithmIndex(self);
    uint32_t offset = NT_parameterOffset();
    
    for (int pot = 0; pot < 3; pot++) {
        PotResult result = processPot(&pThis->uiState, pot, data, potConfigs[pot]);
        if (result.changed) {
            NT_setParameterFromUi(algIndex, result.paramIdx + offset, (int16_t)result.paramValue);
            
            if (result.paramIdx == kParamCutoff) pThis->smoothCutoff = result.paramValue;
            else if (result.paramIdx == kParamResonance) pThis->smoothResonance = result.paramValue;
            else if (result.paramIdx == kParamDecay) pThis->smoothDecay = result.paramValue;
        }
    }
    
    int newVal;
    if (processEncoder(&pThis->uiState, 0, data, kParamVolume, pThis->v[kParamVolume], -40, 6, 1, &newVal)) {
        NT_setParameterFromUi(algIndex, kParamVolume + offset, newVal);
    }
    
    if (processEncoder(&pThis->uiState, 1, data, kParamAccent, pThis->v[kParamAccent], 0, 100, 5, &newVal)) {
        NT_setParameterFromUi(algIndex, kParamAccent + offset, newVal);
    }
}

void setupUi(_NT_algorithm* self, _NT_float3& pots) {
    _NT303Algorithm* pThis = (_NT303Algorithm*)self;
    setupSoftTakeover(&pThis->uiState, pots, potConfigs, pThis->v);
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('T', 'h', 'T', 'B'),
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
    .hasCustomUi = hasCustomUi,
    .customUi = customUi,
    .setupUi = setupUi,
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
