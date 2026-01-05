# NT-303 Development Guide for AI Agents

This document captures project-specific knowledge for AI coding assistants working on NT-303.

## Project Overview

NT-303 is a TB-303 bass synthesizer plugin for Expert Sleepers Disting NT Eurorack module, ported from [Open303](https://github.com/maddanio/open303). The plugin runs on a resource-constrained ARM Cortex-M7 microcontroller.

## Architecture

### Key Files

| File | Purpose |
|------|---------|
| `src/nt_303.cpp` | Main plugin: parameters, MIDI handling, CV processing, audio loop |
| `src/compat.h` | Helper functions (`cvToMidiNote`, `cvToFreq`) |
| `open303/Source/DSPCode/rosic_Open303.h` | Synth header (`getSample()` is inlined here) |
| `open303/Source/DSPCode/rosic_Open303.cpp` | Synth implementation |
| `patches/001-embedded-memory-optimization.patch` | All modifications to Open303 |

### Submodule Patch System

The `open303/` directory is a git submodule pointing to upstream commit `52f8614`. All modifications are maintained as a patch file applied at build time.

**To modify Open303:**
1. Make edits directly in `open303/` files
2. Test your changes
3. Regenerate patch: `cd open303 && git diff > ../patches/001-embedded-memory-optimization.patch`

**Important:** The Makefile runs `git apply --ignore-whitespace` to apply patches. If you see CRLF issues, the `--ignore-whitespace` flag handles them.

### Hardware Constraints (Disting NT)

| Resource | Limit | Notes |
|----------|-------|-------|
| SRAM | 256KB per instance | `sizeof(Open303)` must fit |
| DRAM heap | 262KB | For dynamic allocations (FFT buffers) |
| .bss section | 8KB max | Static/global variables |
| CPU | Cortex-M7 | Single-precision FPU; double is slower but used for filter precision |

### Memory Optimizations Applied

- Wavetables use `float` instead of `double`
- FFT buffers allocated from DRAM heap via `heapAlloc()`
- Replaced `std::list<MidiNoteEvent>` with simple 2-note tracking (`currentNote`, `heldNote`)
- `oversampling` changed from `static const` to runtime-configurable member

## Build System

```bash
make clean && make    # Full rebuild (reapplies patches)
make                  # Incremental build
make push             # Build and push to NT via USB
make check            # Verify symbols and .bss size
```

## Release Process

```bash
git add patches/001-embedded-memory-optimization.patch src/nt_303.cpp
git commit -m "Description"
git tag v0.x.x
git push origin main --tags
# GitHub Actions builds and creates release automatically
```

## MIDI Implementation

### Note Handling

The synth uses a simple 2-note system instead of the original complex note buffer:

- `currentNote`: The note currently sounding
- `heldNote`: A previously held note to return to (for TB-303 style slide behavior)

**TB-303 slide behavior:**
- Play A, hold it
- Play B while holding A → slides to B
- Release B (while still holding A) → slides back to A

### MIDI CCs Supported

- CC 120: All Sound Off (immediate silence)
- CC 123: All Notes Off (note off, allows release)

### Channel Filtering

- Parameter "MIDI Ch" ranges 0-16
- 0 = Omni (responds to all channels)
- 1-16 = Specific channel only

## CV Implementation

### Pitch CV

- 1V/octave standard
- 0V = C4 (MIDI note 60)
- Frequency is set directly via `setOscillatorFrequency()` - no MIDI note quantization
- This allows smooth pitch slides from CV sequencers

### Gate CV

- Schmitt trigger: >1.5V = on, <1.0V = off
- Only processed if Gate CV input is connected (`gateCV != nullptr`)

### Accent CV

- Threshold: >2.5V triggers accent
- Continuously updated while gate is high via `setAccentGain()`

## Common Gotchas

### Sample Rate Changes

`NT_globals.sampleRate` can change at runtime. The `step()` function checks for changes and calls `setSampleRate()` when detected. Make sure any new code that depends on sample rate handles this.

### Patch Application Failures

If builds fail with patch errors:
1. Check that `open303` submodule is at the correct upstream commit
2. Run `git -C open303 checkout 52f8614`
3. Try `make clean && make`

### Floating CV Inputs

If a CV input is configured but the jack is unplugged, it may read noise. Always check `pointer != nullptr` before processing CV inputs.

### Double vs Float

Open303 uses `double` for filter calculations (precision matters for resonance). Don't blindly convert to float - the filter can become unstable.

## API Methods Added to Open303

These were added in the patch for CV control:

```cpp
void setOscillatorFrequency(double freq);  // Direct frequency control (Hz)
void setAccentGain(double gain);           // Continuous accent (0.0-1.0)
void setOversampling(int factor);          // 1, 2, or 4
int getOversampling() const;
```

## Testing Tips

1. **Basic functionality**: Note on/off with various velocities
2. **Slide behavior**: Hold note A, play note B, release B - should slide back to A
3. **CV control**: Patch a sequencer to Pitch CV and Gate
4. **Oversampling**: Compare 1x vs 4x - should hear cleaner filter at high resonance
5. **MIDI channel filtering**: Set to channel 2, send notes on channel 1 - should be silent

## Version History Context

- **v0.5.1**: Added configurable oversampling (1x/2x/4x)
- **v0.5.2**: Fixed sample rate handling (was causing pitch issues)
- **v0.5.3**: Fixed slide/glide - removed erroneous 0.005 multiplier, added continuous CV control
- **v0.5.4**: Fixed MIDI note handling - simplified to 2-note system, added CC 120/123
