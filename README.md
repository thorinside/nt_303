# NT-303

TB-303 bass synthesizer plugin for [Expert Sleepers Disting NT](https://expert-sleepers.co.uk/distingNT.html), based on [Open303](https://github.com/maddanio/open303).

## Installation

Download `nt_303.o` from the [Releases](https://github.com/thorinside/nt_303/releases) page and copy it to your Disting NT SD card's `plugins` folder.

## Features

- Classic TB-303 acid bass sound
- Saw/square waveform blend
- Resonant lowpass filter with envelope modulation
- Accent support via MIDI velocity or CV
- MIDI and CV/Gate control

## Custom UI

### Pots

| Pot | Turn | Push + Turn |
|-----|------|-------------|
| Left | Cutoff (20-20kHz) | Resonance (0-100%) |
| Center | Env Mod (0-100%) | Decay (30-3000ms) |
| Right | Waveform (0-100%) | Slide Time (1-200ms) |

### Encoders

| Encoder | Parameter | Range |
|---------|-----------|-------|
| Left | Volume | -40 to +6 dB |
| Right | Accent | 0-100% |

Pots use soft takeover to prevent parameter jumps when switching between stored values and physical positions.

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Cutoff | 20-20000 Hz | 1000 Hz | Filter cutoff frequency |
| Resonance | 0-100% | 50% | Filter resonance |
| Env Mod | 0-100% | 25% | Filter envelope depth |
| Decay | 30-3000 ms | 300 ms | Filter envelope decay |
| Accent | 0-100% | 50% | Accent intensity |
| Waveform | 0-100% | 0% | Saw (0%) to square (100%) blend |
| Volume | -40 to +6 dB | -12 dB | Output level |
| Slide Time | 1-200 ms | 60 ms | Portamento time for legato notes |
| Oversample | 1x/2x/4x | 2x | Oversampling factor (higher = better quality, more CPU) |
| MIDI Ch | 0-16 | 0 | MIDI channel filter (0 = Omni/all channels) |

## Control Inputs

### MIDI
- Note on/off with velocity (velocity >= 100 triggers accent)
- Legato playing triggers slide (play A, hold, play B → slides to B; release B → slides back to A)
- Pitch bend supported
- CC 120 (All Sound Off) and CC 123 (All Notes Off) supported
- Channel filtering via MIDI Ch parameter (0 = Omni)

### CV/Gate
- Pitch CV: 1V/oct (0V = C4), continuous frequency control (no quantization)
- Gate: >1.5V on, <1.0V off (Schmitt trigger)
- Accent CV: >2.5V triggers accent (continuously updated while gate high)

## Building

```bash
# Clone with submodules
git clone --recursive https://github.com/thorinside/nt_303.git
cd nt_303

# Full rebuild (applies patches to submodule)
make clean && make

# Incremental build
make

# Build and push to NT via USB
make push

# Verify symbols and .bss size
make check
```

Requires ARM GCC toolchain (`arm-none-eabi-g++`).

## License

MIT License - see [LICENSE](LICENSE)

Based on Open303 by Robin Schmidt (MIT License)
