# NT-303

TB-303 bass synthesizer for Expert Sleepers Disting NT, based on Open303.

## Building

```bash
# Clone with submodules
git clone --recursive https://github.com/your-repo/nt_303.git

# Build for hardware
make hardware

# Build for nt_emu testing
make test

# Build both
make both
```

## Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| Cutoff | 200-20000 Hz | Filter cutoff frequency |
| Resonance | 0-100% | Filter resonance |
| Env Mod | 0-100% | Filter envelope depth |
| Decay | 30-3000 ms | Filter envelope decay |
| Accent | 0-100% | Accent intensity |
| Waveform | 0-100% | Saw to square blend |
| Volume | -40 to +6 dB | Output level |
| Slide Time | 1-200 ms | MIDI legato glide time |

## Control Inputs

### MIDI
- Note on/off with velocity (velocity >= 100 triggers accent)
- Legato playing triggers slide
- Pitch bend supported

### CV/Gate
- Pitch CV: 1V/oct (0V = C4)
- Gate: >1V triggers note
- Accent CV: >2.5V triggers accent

CV pitch is tracked directly - use external slew for glide effects.

## License

MIT License

Based on Open303 by Robin Schmidt (MIT License)
https://github.com/maddanio/open303
