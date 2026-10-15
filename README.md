# QSub — 808 Sub-Bass Generator

**Headroom Audio** | 808 sub-bass generator plugin (JUCE)

Insert QSub on a kick or bass track: it detects the pitch of the incoming audio
and synthesizes a clean sub sine/triangle underneath, tuned to the input, with
an envelope follower so the sub breathes with your dynamics.

## Controls

- **Sub Level** (hero) — how much sub gets added
- **Waveform** — sine ↔ triangle blend
- **Drive** — tanh saturation on the sub
- **Cutoff** — lowpass on the sub (40–500 Hz)
- **Attack / Release** — envelope follower tracking the input's dynamics
- **Pitch Detect** — on: tracks input pitch (30–200 Hz); off: fixed note via Key selector

## Presets

Factory presets plus user save/load (`~/Documents/Headroom Audio/QSub/Presets/`),
with a ♥ favorites system. Preset menu is standard across all Headroom plugins.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Outputs VST3 / AU / Standalone via the `QSub_artefacts` directory.
