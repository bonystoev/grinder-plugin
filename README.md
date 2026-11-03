# Grinder

Saturation & Distortion Plugin (VST3 / AU / CLAP / Standalone), gebaut mit JUCE und
[clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions).

## Charaktere

| Character | Beschreibung |
|---|---|
| **Tube** | Asymmetrisches Soft-Clipping (tanh mit Bias) |
| **Diode** | Zweistufige Diode-Ladder-Kurve (soft/hard) |
| **Fuzz** | Exponentiales Hard-Clipping mit scharfer Deckelung |
| **Tape** | Band-Magnetisierungsmodell + 2:1-Sättigungskompression, Wow & Flutter, Hiss |
| **Cassette** | Wie Tape, stärker: Head Bump (+3 dB @ 60 Hz), Höhen-Rolloff @ 10 kHz, mehr Hiss/W&F |
| **Dictaphone** | Bandpass 400 Hz–3 kHz, Limiter-Charakteristik, Motorbrummen, Dropouts |

Die elektrischen Charaktere (Tube/Diode/Fuzz) laufen mit 4x-Oversampling
(Fenster-Sinc-FIR) gegen Aliasing; die Bandmaschinen laufen auf Basisrate
(Wow & Flutter liegen ohnehin unterhalb der Bandbreite).

## Parameter

Drive, Character, Tilt (±12 dB), Mid Character, Low Cut (20–800 Hz),
Presence (±12 dB @ 2,5 kHz), Wow & Flutter, Noise (Off/-80…-40 dB), Mix,
Output (±). „Tilt on Wet Only“ legt den Tilt-EQ nur auf den verzerrten Pfad.

## Bauen

Voraussetzungen: CMake ≥ 3.22, C++17-Compiler.

```bash
git clone --recurse-submodules https://github.com/bonystoev/grinder-plugin.git
cd grinder-plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build            # DSP-Unit-Tests (headless)
```

Die Binaries liegen danach in `build/Grinder_artefacts/Release/`.

Systemvoraussetzungen für Linux-Builds: `libasound2-dev`, `libx11-dev`,
`libxcomposite-dev`, `libxcursor-dev`, `libxext-dev`, `libxinerama-dev`,
`libxrandr-dev`, `libxrender-dev`, `libfreetype6-dev`, `libfontconfig1-dev`,
`libglu1-mesa-dev`, `mesa-common-dev` (Debian/Ubuntu-Name).

Für macOS (AU) einfach dasselbe CMake mit Xcode-Toolchain bauen.

## Tests

`tests/test_dsp.cpp` testet den framework-freien DSP-Kern (`dsp/grinder_dsp.h`)
headless: Kurvenverläufe (Beschränktheit, Monotonie, Symmetrie, Drive-Verhalten),
Biquad-Frequenzgänge, Oversampler-Transparenz, Determinismus der Noise-Engine
(geseedet), Stabilität der kompletten Kette über alle Charaktere, Mix=0 ⇒ Dry,
Cassette-Head-Bump, Dictaphone-Bandpass und Wow-&-Flutter-Modulation.

Manueller Testlauf ohne CMake:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -I dsp -o test_dsp tests/test_dsp.cpp && ./test_dsp
```
