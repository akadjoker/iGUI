#pragma once

#include <string>

// Shared types for the headless iGUI-vs-ImGui comparison.
//
// Both runners build the *same* scene (same widget sequence, same counts) and
// report the same numbers, so the two rows of the table are comparable: what
// differs between them is the library, not the workload. main() checks that
// both runners report the same widget count for a scene and warns when they
// drift apart.
namespace bench
{

struct Config
{
    std::string scene;
    int blocks;
    int windows;
    int frames;
    int warmup;
    float width;
    float height;
    // Measure text with iGUI's real font atlas (the default). Turning it off
    // swaps in a fixed-advance measurer, which isolates how much of the submit
    // cost is text measurement.
    bool useFont;
    // Print every draw command of the last frame (clip, texture, index count):
    // what breaks a window into more than one draw call is visible there.
    bool dump;

    Config()
        : scene("gallery"), blocks(10), windows(4), frames(300), warmup(30),
          width(1280.0f), height(800.0f), useFont(true), dump(false)
    {
    }
};

// Everything is measured around "submit the frame and get draw data ready":
// iGUI begins at Context::beginFrame and stops at the DrawData it hands out;
// ImGui begins at NewFrame and stops at the ImDrawData Render() finalizes.
// Walking that draw data afterwards (what a backend does) is timed separately,
// because a real backend's GPU cost is not what this compares.
struct Result
{
    std::string lib;
    bool ran;
    std::string error;

    double submitMeanUs;
    double submitMedianUs;
    double submitP95Us;
    double submitMinUs;
    double renderMeanUs;
    double totalMeanUs;

    long long vertices;
    long long indices;
    long long commands;
    long long widgets;
    // iGUI only: TextProvider calls per frame. ImGui measures and shapes text
    // inside the same widget call without a public counter, so its row leaves
    // these at zero.
    long long measureCalls;
    long long shapeCalls;
    long long textChars;
    long long peakRssKb;

    Result()
        : lib(), ran(false), error(), submitMeanUs(0.0), submitMedianUs(0.0),
          submitP95Us(0.0), submitMinUs(0.0), renderMeanUs(0.0), totalMeanUs(0.0),
          vertices(0), indices(0), commands(0), widgets(0), measureCalls(0), shapeCalls(0),
          textChars(0), peakRssKb(-1)
    {
    }
};

Result runIgui(const Config &config);
Result runImgui(const Config &config);

} // namespace bench
