# Pocket Tracer

A multi-mode delay plugin by **Khyle Audio**. Six delay character types (Digital,
Analog, Tape, Reverse, Lo-Fi, Multi-Tap) in one unit, with tempo sync, ping-pong,
freeze, a single-knob tilt Tone, type-appropriate modulation, and input/output gain.

- **Formats:** VST3 + Standalone
- **Architecture:** Windows x64
- **Built with:** JUCE 8 / CMake / MSVC (Visual Studio Build Tools 2022)

---

## 1. Prerequisites

These are already set up on the build machine; listed for reproducibility.

| Tool | Version used | Notes |
|---|---|---|
| CMake | 4.4.0 | on PATH |
| Compiler | VS Build Tools 2022 (MSVC x64) | `cl` found via the VS generator |
| JUCE | 8.0.14 | installed at `C:/JUCE` |

If JUCE lives somewhere else, override it at configure time:
`-DJUCE_PATH="X:/path/to/JUCE"`.

---

## 2. Build

From the project root (`C:\Users\Khalil\pocket-tracer-vst`):

```sh
# Configure (once, or after editing CMakeLists.txt)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build Release (VST3 + Standalone)
cmake --build build --config Release
```

Build a single target if preferred:

```sh
cmake --build build --config Release --target PocketTracer_VST3
cmake --build build --config Release --target PocketTracer_Standalone
```

### Output locations
```
build\PocketTracer_artefacts\Release\VST3\Pocket Tracer.vst3
build\PocketTracer_artefacts\Release\Standalone\Pocket Tracer.exe
```

---

## 3. Install for Reaper (Windows)

**Option A — copy to the system VST3 folder (needs admin):**
```sh
copy /Y "build\PocketTracer_artefacts\Release\VST3\Pocket Tracer.vst3" ^
        "C:\Program Files\Common Files\VST3\"
```
> `Pocket Tracer.vst3` is a folder bundle — copy the whole folder, not just the inner `.vst3` file.

**Option B — point Reaper at the build folder (no admin):**
1. Reaper → **Options → Preferences → Plug-ins → VST**.
2. Under *VST plug-in paths*, add:
   `C:\Users\Khalil\pocket-tracer-vst\build\PocketTracer_artefacts\Release\VST3`
3. Click **Re-scan**.

Then add it on a track via **FX → add → "Pocket Tracer"** (manufacturer: Khyle Audio).

---

## 4. Reaper testing checklist

Put it on a track with audio (a dry guitar/drum loop is ideal). Default state is
Digital, 350 ms, 35% feedback, 35% mix.

### Core
- [ ] **Mix** sweeps dry→wet smoothly (equal-power; no level dip at noon).
- [ ] **Time** 1 ms–2000 ms; readout shows ms. Changing it glides pitch naturally.
- [ ] **Tone** tilts dark↔bright on the repeats only (dry stays unaffected).
- [ ] **Input / Output** gain change level with smooth dB readouts; no clicks.

### Delay types (listen for the distinct character of each)
- [ ] **Digital** — clean, identical repeats.
- [ ] **Analog** — repeats get progressively darker; subtle grit; gentle drift with Mod up.
- [ ] **Tape** — warm saturation + audible wow/flutter wobble as Mod increases.
- [ ] **Reverse** — repeats play backwards, smooth (no clicks at grain seams).
- [ ] **Lo-Fi** — gritty/bitcrushed, degrading further each repeat.
- [ ] **Multi-Tap** — three rhythmic taps (1/4 centre, 1/8-dotted left, 1/16 right); ignores ping-pong.

### Tempo sync
- [ ] Engage **SYNC**: TIME switches to note divisions (e.g. `1/8D`).
- [ ] Repeats lock to host tempo; change Reaper's BPM and confirm they track.
- [ ] In **Standalone**, sync falls back to 120 BPM (no host).

### Ping-pong / Freeze / Feedback safety
- [ ] **Ping-Pong** (stereo track): repeats bounce L↔R.
- [ ] **Freeze**: engaging holds the current buffer looping indefinitely; smooth
      crossfade in/out; no runaway, no decay while held.
- [ ] **Feedback safety (important):** push Feedback past 100% (up to 110%) and
      let it self-oscillate. It should swell to a **stable sustained** level and
      hold — **not** build to clipping/blow up. Try slamming it to 110% with loud
      input; output must stay bounded (the tanh + ceiling limiter catches it).

### Session recall
- [ ] Save the Reaper project, reload it, confirm all parameters restore.

---

## 5. Project layout

```
CMakeLists.txt
Source/
  PluginProcessor.{cpp,h}     APVTS, signal flow, I/O gain, equal-power mix
  PluginEditor.{cpp,h}        GUI (PTKnob / PTTypeButton / PTToggle, motif)
  dsp/
    DelayEngine.{cpp,h}       circular buffer + 6 type implementations + safety
    TiltFilter.{cpp,h}        single-knob tilt Tone
    TempoSync.{cpp,h}         note-division -> ms (fallback 120 BPM)
  gui/
    PocketTracerLookAndFeel.{cpp,h}  royal-blue/black chrome knobs
```
