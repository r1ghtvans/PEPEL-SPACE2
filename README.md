# PEPEL SPACE
Stereo spatial processor (VST3, Windows x64), JUCE 8 / C++17 / CMake. Built for FL Studio on a single Send/Insert.
Look: post-soviet night courtyard + VHS. See `Assets/README.md` for replaceable fonts.

Local build: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release --target PepelSpace_VST3`
Tools: add `-DPEPEL_BUILD_TOOLS=ON`, targets `DspTest` (DSP checks) and `Snapshot` (UI -> PNG).
Output: `build/PepelSpace_artefacts/Release/VST3/PEPEL SPACE.vst3`
