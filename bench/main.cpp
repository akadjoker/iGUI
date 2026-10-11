// Headless comparison: iGUI immediate mode vs ImGui (same widget scenes).
//
//   igui_bench [--lib=igui|imgui|both] [--scene=basic|gallery|text|windows|plots]
//              [--blocks=N] [--windows=N] [--frames=N] [--warmup=N]
//              [--size=WxH] [--csv=FILE] [--help]
//
// Both runners submit the same number of widgets per frame and are timed around
// "widgets submitted, draw data ready"; walking the draw data afterwards is
// reported as render. Run it in Release (the bench targets force -O2 when no
// build type is set) and on an idle machine: the numbers are microseconds.

#include "Bench.hpp"

#ifndef IGUI_BENCH_HAS_IMGUI
#  define IGUI_BENCH_HAS_IMGUI 0
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{

void usage()
{
    std::printf(
        "usage: igui_bench [options]\n"
        "  --lib=igui|imgui|both   which library to measure (default both)\n"
        "  --scene=basic|gallery|text|windows|plots  scene to submit (default gallery)\n"
        "  --blocks=N              widget blocks in the scene (default 10)\n"
        "  --windows=N             windows for the windows scene (default 4)\n"
        "  --frames=N              measured frames (default 300)\n"
        "  --warmup=N              frames before measuring (default 30)\n"
        "  --size=WxH              viewport (default 1280x800)\n"
        "  --no-font               iGUI only: fixed-advance text metrics instead of the\n"
        "                          font atlas (shows how much of submit is measuring text)\n"
        "  --dump                  print the draw commands of the last frame\n"
        "  --csv=FILE              also write one row per library\n"
        "  --help\n");
}

bool parseValue(const char *argument, const char *name, std::string &out)
{
    const std::size_t length = std::strlen(name);
    if (std::strncmp(argument, name, length) != 0 || argument[length] != '=')
        return false;
    out = argument + length + 1;
    return true;
}

int parseInt(const std::string &text, int fallback)
{
    if (text.empty())
        return fallback;
    return std::atoi(text.c_str());
}

void printRow(const bench::Result &result, bool singleLib)
{
    if (!result.ran)
    {
        std::printf("%-7s not measured (%s)\n", result.lib.c_str(),
                    result.error.empty() ? "runner unavailable" : result.error.c_str());
        return;
    }
    std::printf("%-7s %11.2f %9.2f %9.2f %9.2f %9.2f %9.2f", result.lib.c_str(),
                result.submitMeanUs, result.renderMeanUs, result.totalMeanUs,
                result.submitMedianUs, result.submitP95Us, result.submitMinUs);
    std::printf(" %7lld %8lld %7lld %7lld", result.vertices, result.indices, result.commands,
                result.widgets);
    if (result.measureCalls || result.shapeCalls)
        std::printf(" %5lld/%-6lld", result.measureCalls, result.shapeCalls);
    else
        std::printf(" %13s", "-");
    // Peak RSS belongs to the whole process, so in "both" mode the first row
    // would carry the other library's memory too.
    if (singleLib)
        std::printf(" %8lld\n", result.peakRssKb);
    else
        std::printf(" %8s\n", "-");
}

void writeCsv(const std::string &path, const bench::Config &config,
              const std::vector<bench::Result> &results)
{
    std::FILE *file = std::fopen(path.c_str(), "w");
    if (!file)
    {
        std::fprintf(stderr, "cannot write %s\n", path.c_str());
        return;
    }
    std::fprintf(file,
                 "lib,scene,blocks,windows,frames,viewport,submit_mean_us,render_mean_us,"
                 "total_mean_us,submit_median_us,submit_p95_us,submit_min_us,vertices,indices,"
                 "commands,widgets,peak_rss_kb,note\n");
    for (std::size_t i = 0; i < results.size(); ++i)
    {
        const bench::Result &result = results[i];
        std::fprintf(file,
                     "%s,%s,%d,%d,%d,%.0fx%.0f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%lld,%lld,%lld,%lld,"
                     "%lld,%s\n",
                     result.lib.c_str(), config.scene.c_str(), config.blocks, config.windows,
                     config.frames, static_cast<double>(config.width),
                     static_cast<double>(config.height), result.submitMeanUs,
                     result.renderMeanUs, result.totalMeanUs, result.submitMedianUs,
                     result.submitP95Us, result.submitMinUs, result.vertices, result.indices,
                     result.commands, result.widgets, result.peakRssKb, result.error.c_str());
    }
    std::fclose(file);
    std::printf("\ncsv: %s\n", path.c_str());
}

void printRatios(const bench::Result &left, const bench::Result &right)
{
    if (!left.ran || !right.ran)
        return;
    const double submitRatio =
        right.submitMeanUs > 0.0 ? left.submitMeanUs / right.submitMeanUs : 0.0;
    const double totalRatio = right.totalMeanUs > 0.0 ? left.totalMeanUs / right.totalMeanUs : 0.0;
    // O min é a métrica que sobrevive a uma máquina carregada (o resto do processo
    // rouba-lhe tempo); a média e a mediana só servem numa máquina calma.
    const double minRatio =
        right.submitMinUs > 0.0 ? left.submitMinUs / right.submitMinUs : 0.0;
    const double medianRatio =
        right.submitMedianUs > 0.0 ? left.submitMedianUs / right.submitMedianUs : 0.0;
    std::printf("\n%s relative to %s (1.00 == equal):\n", left.lib.c_str(), right.lib.c_str());
    std::printf("  submit       %.2fx        (widget calls, draw data ready, mean)\n", submitRatio);
    std::printf("  submit+render %.2fx       (the whole CPU side of a frame)\n", totalRatio);
    std::printf("  submit (min) %.2fx        (the fastest frame: the metric a noisy machine "
                "does not spoil)\n", minRatio);
    std::printf("  submit (med) %.2fx\n", medianRatio);
    std::printf("  vertices     %.2fx        (glyphs included; a real backend draws these)\n",
                right.vertices > 0 ? static_cast<double>(left.vertices) /
                                         static_cast<double>(right.vertices)
                                   : 0.0);
    std::printf("  draw calls   %.2fx\n",
                right.commands > 0 ? static_cast<double>(left.commands) /
                                         static_cast<double>(right.commands)
                                   : 0.0);
    if (left.widgets != right.widgets)
    {
        std::printf("\nwarning: the scenes are not the same workload: %s submitted %lld widgets, "
                    "%s submitted %lld\n",
                    left.lib.c_str(), left.widgets, right.lib.c_str(), right.widgets);
    }
}

} // namespace

int main(int argc, char **argv)
{
    bench::Config config;
    std::string lib = "both";
    std::string csvPath;

    for (int i = 1; i < argc; ++i)
    {
        std::string value;
        if (parseValue(argv[i], "--lib", value))
            lib = value;
        else if (parseValue(argv[i], "--scene", value))
            config.scene = value;
        else if (parseValue(argv[i], "--blocks", value))
            config.blocks = parseInt(value, config.blocks);
        else if (parseValue(argv[i], "--windows", value))
            config.windows = parseInt(value, config.windows);
        else if (parseValue(argv[i], "--frames", value))
            config.frames = parseInt(value, config.frames);
        else if (parseValue(argv[i], "--warmup", value))
            config.warmup = parseInt(value, config.warmup);
        else if (parseValue(argv[i], "--csv", value))
            csvPath = value;
        else if (std::strcmp(argv[i], "--no-font") == 0)
            config.useFont = false;
        else if (std::strcmp(argv[i], "--dump") == 0)
            config.dump = true;
        else if (parseValue(argv[i], "--size", value))
        {
            const std::size_t separator = value.find('x');
            if (separator != std::string::npos)
            {
                config.width = static_cast<float>(std::atof(value.substr(0, separator).c_str()));
                config.height = static_cast<float>(std::atof(value.substr(separator + 1).c_str()));
            }
        }
        else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
        {
            usage();
            return 0;
        }
        else
        {
            std::fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage();
            return 2;
        }
    }

    if (config.frames < 1 || config.warmup < 0 || config.blocks < 1 || config.windows < 1)
    {
        std::fprintf(stderr, "frames must be >= 1, warmup >= 0, blocks >= 1, windows >= 1\n");
        return 2;
    }

    std::printf("scene=%s blocks=%d windows=%d viewport=%.0fx%.0f frames=%d warmup=%d\n",
                config.scene.c_str(), config.blocks, config.windows,
                static_cast<double>(config.width), static_cast<double>(config.height),
                config.frames, config.warmup);

    std::vector<bench::Result> results;
    bench::Result iguiResult;
    bench::Result imguiResult;

    if (lib == "igui" || lib == "both")
    {
        iguiResult = bench::runIgui(config);
        results.push_back(iguiResult);
    }
    if (lib == "imgui" || lib == "both")
    {
#if IGUI_BENCH_HAS_IMGUI
        imguiResult = bench::runImgui(config);
#else
        imguiResult.lib = "imgui";
        imguiResult.error = "built without IGUI_BENCH_IMGUI_DIR";
#endif
        results.push_back(imguiResult);
    }
    if (lib != "igui" && lib != "imgui" && lib != "both")
    {
        std::fprintf(stderr, "unknown --lib=%s\n", lib.c_str());
        return 2;
    }

    std::printf("\n%-7s %11s %9s %9s %9s %9s %9s %7s %8s %7s %7s %13s %8s\n", "lib",
                "submit mean", "render", "total", "median", "p95", "min", "vtx", "idx", "cmds",
                "widgets", "text meas/shape", "rss kb");
    const bool singleLib = lib != "both";
    for (std::size_t i = 0; i < results.size(); ++i)
        printRow(results[i], singleLib);

    printRatios(iguiResult, imguiResult);

    for (std::size_t i = 0; i < results.size(); ++i)
    {
        if (!results[i].error.empty())
            std::printf("\nnote: %s: %s\n", results[i].lib.c_str(), results[i].error.c_str());
    }

    if (!csvPath.empty())
        writeCsv(csvPath, config, results);
    return 0;
}
