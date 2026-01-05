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

## Custom UI (3 Pots)

| Pot | Parameter | Range |
|-----|-----------|-------|
| Left | Cutoff | 20-20000 Hz (exponential) |
| Center | Resonance | 0-100% |
| Right | Decay | 30-3000 ms |

## Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| Cutoff | 20-20000 Hz | Filter cutoff frequency |
| Resonance | 0-100% | Filter resonance |
| Env Mod | 0-100% | Filter envelope depth |
| Decay | 30-3000 ms | Filter envelope decay |
| Accent | 0-100% | Accent intensity |
| Waveform | 0-100% | Saw to square blend |
| Volume | -40 to +6 dB | Output level |
| Slide Time | 1-200 ms | Portamento time |

## Control Inputs

### MIDI
- Note on/off with velocity (velocity >= 100 triggers accent)
- Legato playing triggers slide
- Pitch bend supported

### CV/Gate
- Pitch CV: 1V/oct (0V = C4)
- Gate: >1.5V on, <1.0V off (Schmitt trigger)
- Accent CV: >2.5V triggers accent

## Building

```bash
# Clone with submodules
git clone --recursive https://github.com/thorinside/nt_303.git
cd nt_303

# Build for Disting NT hardware
make hardware

# Build for VCV Rack testing (nt_emu)
make test
```

Requires ARM GCC toolchain (`arm-none-eabi-g++`).

## License

MIT License - see [LICENSE](LICENSE)

Based on Open303 by Robin Schmidt (MIT License)
