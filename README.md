# My lil Melody Monster

An eight-track seeded melody sequencer for VCV Rack 2. Each track grows a 64-step pattern from a seed number, then
you shape it live with chance, variation, direction and rate. Same seed, same melody. By A_Development.

## Tracks

Eight tracks, each with its own gate and pitch output. Press a **track button** (1 to 8) to select it. The knobs and
the right-click menu edit the selected track; switching tracks loads that track's own settings into the knobs.
A knob only changes a track when you move it, so selecting a track never overwrites it.

| Control | What it does |
|---|---|
| **CHANCE** | Share of steps that play (0 to 100 %). Each step has a fixed dice roll, so lowering it drops the same steps first. |
| **NOTE** | Note variation, -100 to +100 %. Some steps move up (positive) or down (negative) by up to an octave. More % hits more steps. |
| **OCTAVE** | Octave variation, -100 to +100 %. Like NOTE, but jumps whole octaves, up to five. |
| **LENG** | Pattern length, 1 to 64 steps. |
| **RATE** | Step speed against the clock, from /64 to x64 (x1 = one step per clock pulse), including odd ratios such as x1.5 and /5.3. |
| **DIRN** | How the pattern is walked (see Directions). |
| **TRNS** | Transpose, -7 to +7 scale degrees. |
| **SHFT** | Rotate the pattern, -63 to +63 steps. |
| **OCTA** | Octave shift, -3 to +3. |
| **MUTE** | Mute the selected track (gate stays low, pitch holds). |
| **SOLO** | Solo the selected track. Only one track can be solo; pressing it on another track moves the solo. Press again to clear. |
| **FRZE** | Freeze (latch). Steps stop advancing, the clock keeps running, and every audible track holds its gate open. |
| **RUN** | Run or stop the sequencer (latch). Stopping drops all gates. |
| **RSET** | Reset: every track goes back to its first step. Works while stopped. |
| **RSED** | Reseed the selected track with a fresh random seed. The light stays on until it lands. |

A reseed lands at the end of the track's current pass, so it stays in time. Tracks that walk randomly (Drunk, Random,
Pools, Skitter) have no pass, so they reseed at once.

Scale degrees come from the scale and root note set in the right-click menu. Pitch is 1 V/oct, with 0 V at the root
in the default octave.

## Directions

Forward, Reverse, Pendulum (ends played twice), Pong (ends played once), Drunk (mostly forward, sometimes stay or step
back), Random, Tide (forward, but the start point creeps one step each pass), Shuffle (a new random order each pass),
Pools (loops a few steps for a few laps, then jumps), Stride, Gravity (forward, with a seeded chance to snap home),
Converge (from both ends toward the middle), Diverge (from the middle outward), Skitter (random, never the same
step twice) and Anchor (keeps returning to step 1 between other steps).

## Display

The screen shows all eight tracks as bar lanes, with the playing step highlighted. The selected track's number is
amber. A lane that is dim is muted, silent from Breathe, or not soloed. The letters beside a lane show `S` solo,
`M` mute, and `<n` follows track n. The right column lists the selected track's seed, chance, note, octave, evolve,
rate, length, direction, and the scale and root, plus ARM (reseed waiting) and FRZ (frozen).

## Jacks

| Jack | |
|---|---|
| GATE 1 to 8 | Gate per track. Level is 10 V unless set per track in the menu. |
| PITCH 1 to 8 | Pitch per track, 1 V/oct. Holds its last value on rests and muted steps. |
| CLK OUT | Pulse (10 V) on every clock tick. Stops when RUN is off. |
| CLK IN | External clock, one pulse per x1 step. With nothing patched, the internal clock runs at the menu BPM (one pulse per beat). |
| RST IN | Trigger: same as RSET. |
| RESEED | Trigger: reseed all eight tracks. |
| FREEZE | Gate: freezes while high. Also freezes if the FRZE latch is on. |
| SEED | 0 to 10 V picks a seed from 0 to 999 for the selected track. A change in value reseeds it. |

## Right-click menu

The top block is for the selected track:

- **Evolve**: share of steps re-rolled at the end of each pass, so the melody slowly mutates.
- **Breathe**: chance that a whole pass is silent.
- **Gate length**: how long the gate stays high, as a share of the step (5 to 100 %).
- **Tie**: share of steps whose gate stays high into the next step.
- **Slop**: delays steps by up to half a step, a different amount for each step.
- **Gate level**: gate voltage, 1 to 10 V.
- **Pitch scale**, **Pitch offset**: scale (5 to 200 %) and offset (-10 to +10 V) of the pitch output.
- **Follow**: play another track's pattern (steps and dice) with this track's own chance, rate, direction and other
  settings. A loop of follows plays its own pattern.
- **Seed**: set the seed directly, 0 to 999.

The Global block applies to every track:

- **Scale**: Major, Natural minor, Harmonic minor, Melodic minor, Dorian, Phrygian, Lydian, Mixolydian, Major
  pentatonic, Minor pentatonic (default), Blues, In-Sen, Whole tone.
- **Root note**: C to B.
- **Weight**: share of steps pulled to the nearest root, 3rd or 5th of the scale.
- **BPM**: internal clock tempo, 20 to 300.
- **Frozen clock out**: while frozen, CLK OUT keeps running or stops.
- **Poly output routing**: Off (every jack is mono), Pairs, Split 4+4 or All 8. Each jack still carries its own track
  on channel 1. The last jack of each group also carries the rest of its group on the next channels, in track
  order: with Pairs, GATE 2 and PITCH 2 carry tracks 2 and 1; with All 8, jack 8 carries tracks 8, then 1 to 7.
- **Reseed all**.

## Saving

The patch stores every track's seed, settings, pattern and walk position, so a reload resumes mid-pattern.
Random walks (Drunk, Random, Pools, Skitter) continue with new random choices after a reload.

## Building

Rack SDK 2.6.6 and MSYS2 (Windows) or a Unix shell: `make` builds, `make test` runs the core checks (from the repo
root), `make dist` packs the plugin. Source: https://github.com/Adev-io/my-lil-melody-monster

MIT licence. Author: Ari Klopfer / A_Development.
