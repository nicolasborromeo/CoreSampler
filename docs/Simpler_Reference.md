# Ableton Simpler — Feature Reference
*Source: Ableton Live 12 Manual, Chapter 30.11 (pages 736–746)*
*Used as design blueprint for the LittleSampler JUCE plugin*

---

## What Is Simpler?
An instrument that integrates a **sampler** (plays audio files) with classic **synthesizer parameters** (filter, envelopes, LFO). A voice plays a user-defined region of a sample, processed through envelope → filter → LFO → volume/pitch.

---

## Three Playback Modes

| Mode | Polyphony | Looping | Best For |
|------|-----------|---------|----------|
| **Classic** | Polyphonic | Yes, ADSR | Melodic instruments, pitched samples |
| **One-Shot** | Monophonic | No | Drum hits, short phrases |
| **Slicing** | Configurable | N/A | Drum breaks, rhythmic loops |

---

## Sample Tab Controls (Classic Mode)
- **Start / End flags** — define the playback region within the sample
- **Length** — % of the region between flags to play
- **Loop slider** — how much of the region loops (when loop is on)
- **Loop On/Off** — enable/disable looping
- **Snap** — locks loop markers to zero-crossing points (reduces pops)
- **Fade** — crossfades loop start/end points
- **Gain** — pre-filter level boost/cut
- **Voices** — max simultaneous voices (polyphony), with voice stealing
- **Retrig** — re-triggers a held note if the same note is played again

---

## Warp Controls
- **Warp On/Off** — syncs sample playback speed to current BPM (regardless of pitch)
- **Warp As…** — sets the bar/beat count for the sample
- **÷2 / ×2** — halve or double playback speed

---

## Filter
- **Types:** Low-pass, High-pass, Band-pass, Notch, Morph (sweeps LP→BP→HP→Notch)
- **Slopes:** 12 dB/oct and 24 dB/oct
- **Circuit Types:** Clean, OSR, MS2, SMP, PRD (analog hardware models)
- **Key Params:** Frequency (cutoff), Resonance (Q), Drive (non-Clean only)
- **Modulation:** via Velocity, Key (keyboard tracking), Envelope amount, LFO

---

## Envelopes — 3 ADSR Envelopes

All three share: **Attack → Decay → Sustain → Release**

1. **Amplitude Envelope** — shapes volume over time; can be looped
2. **Filter Envelope** — modulates filter cutoff; depth set by Envelope Amount
3. **Pitch Envelope** — modulates pitch; useful for percussive sounds

The **Amplitude envelope can loop**: Loop, Trigger, Beat, and Sync modes.

---

## LFO (Low Frequency Oscillator)
- **Waveforms:** Sine, Square, Triangle, Sawtooth Down, Sawtooth Up, Random
- **Rate:** 0.01–30 Hz (free) or synced to song tempo
- **Attack** — time to ramp up to full LFO intensity
- **Retrigger** — resets LFO phase on each new note
- **Offset** — sets the LFO starting phase
- **Key** — scales LFO rate with note pitch
- **Modulation Targets:** Volume, Pitch, Pan, Filter cutoff

---

## Global Parameters
- **Pan** + **Random Pan** (randomizes pan per note)
- **Spread** — stereo chorus: two voices per note, detuned, panned L/R
- **Volume** + **Velocity → Volume** (velocity sensitivity)
- **Transpose** — ±48 semitones (C3 = original pitch by default)
- **Detune** — ±50 cents (fine pitch tuning)
- **Pitch Bend** — ±5 semitones sensitivity
- **Glide** (monophonic) / **Portamento** (polyphonic) + **Time**

---

## LittleSampler Build Phases

### Phase 1 — Core Sampler (current)
- [ ] Load a sample file
- [ ] Pitch playback (MIDI note changes pitch)
- [ ] Volume knob
- [ ] ADSR amplitude envelope (Attack, Decay, Sustain, Release)

### Phase 2 — Filter
- [ ] Filter Frequency knob
- [ ] Filter Resonance knob
- [ ] Filter type selector (LP / HP / BP)

### Phase 3 — LFO
- [ ] LFO waveform selector
- [ ] LFO Rate
- [ ] LFO → Volume / Pitch / Filter routing

### Phase 4 — Playback Modes
- [ ] One-Shot mode (monophonic, no loop)
- [ ] Loop On/Off
- [ ] Start / End / Loop region controls
