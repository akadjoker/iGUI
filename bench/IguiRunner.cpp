// iGUI side of the headless comparison.
//
// The "gallery" block below is the canonical workload; ImguiRunner.cpp submits
// the same sequence through ImGui's immediate API. Keep the two in sync: the
// widget counter is checked against the ImGui runner in main(), so a widget
// added here and forgotten there shows up as a mismatch, not as a wrong number.
//
//   gallery block, per repetition:
//     label   button | sameLine | checkbox   sliderFloat   inputText
//     comboBox  separator  progressBar
//     collapsingHeader (expanded) -> 3 labels
//     treeNode (expanded) -> 1 label
//
// Widgets are laid out by the automatic-layout API (button(label) and friends),
// which is the closest match to ImGui::Button/ImGui::Text and the reason the two
// blocks can be written as the same sequence of calls.
//
// Where the work lands differs by design, and the two halves are kept apart
// because of it: with a TextProvider installed - the normal case, and the one
// the font module provides - iGUI asks the provider to write glyph quads while
// the widgets are submitted, so text lands in submit. Without one, the Context
// records TextCommands and the backend shapes them during render, which is what
// the SDL2 and Raylib backends do. The reported total (submit + render) is the
// comparable number; the split is what tells you which library pays where.

#include "Bench.hpp"
#include "Clock.hpp"
#include "Stats.hpp"

#include <igui/Gui.hpp>

#if IGUI_BENCH_HAS_FONT
#include <igui/FontAtlas.hpp>
#endif

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace bench
{
namespace
{

// Headless backend: no window and no GPU, but the same walk a real backend does
// - geometry is taken as it comes and any TextCommand that reaches it is shaped
// through the TextProvider, the way SdlBackend and RaylibBackend do it. The
// counters are therefore the geometry a GPU would be handed. With a provider
// installed the Context has already turned text into geometry, so the shaping
// loop below normally finds nothing to do.
class CountingBackend : public ig::Backend
{
public:
    ig::TextProvider* provider;
    long long vertices;
    long long indices;
    long long commands;

    CountingBackend()
            : provider(nullptr),
              vertices(0),
              indices(0),
              commands(0),
              textList_()
    {
    }

    ig::TextMetrics measureText(ig::FontId font, ig::StringView text, float logicalSize,
            float) override
    {
        if (provider) return provider->measureText(font, text, logicalSize);
        ig::TextMetrics metrics;
        metrics.width = static_cast<float>(text.size()) * logicalSize * 0.5f;
        metrics.height = logicalSize;
        metrics.ascent = logicalSize * 0.78f;
        metrics.descent = logicalSize * 0.22f;
        return metrics;
    }

    bool render(const ig::DrawData& data) override
    {
        vertices = static_cast<long long>(data.vertices.size());
        indices = static_cast<long long>(data.indices.size());
        // One draw call per command, which is what the SDL2 backend issues.
        commands = static_cast<long long>(data.commands.size());

        if (!provider) return true;

        textList_.clear();
        for (std::size_t i = 0; i < data.commands.size(); ++i)
        {
            const ig::DrawCommand& command = data.commands[i];
            if (command.type != ig::DrawCommandType::Text) continue;
            const ig::TextCommand& text = command.payload.text;
            if (text.textOffset > data.textBytes.size() ||
                    text.textSize > data.textBytes.size() - text.textOffset)
                continue;
            const ig::StringView view(data.textBytes.data() + text.textOffset, text.textSize);
            provider->appendText(textList_, text.font, view, text.position, text.logicalSize,
                    text.color, text.clip);
        }
        const ig::DrawData shaped = textList_.data(data.displaySize, data.dpiScale);
        vertices += static_cast<long long>(shaped.vertices.size());
        indices += static_cast<long long>(shaped.indices.size());
        return true;
    }

private:
    ig::DrawList textList_;
};

// Wraps the real text provider to count how often the Context asks for text to
// be measured and shaped. When the submit cost is dominated by text, the two
// counters say whether it is measured more often than it is drawn.
class CountingTextProvider : public ig::TextProvider
{
public:
    explicit CountingTextProvider(const ig::TextProvider& inner)
            : measureCalls(0),
              measureChars(0),
              shapeCalls(0),
              shapeChars(0),
              inner_(inner)
    {
    }

    ig::TextMetrics measureText(ig::FontId font, ig::StringView text,
            float logicalSize) const override
    {
        ++measureCalls;
        measureChars += static_cast<long long>(text.size());
        return inner_.measureText(font, text, logicalSize);
    }

    bool appendText(ig::DrawList& drawList, ig::FontId font, ig::StringView text,
            const ig::Vec2& position, float logicalSize, const ig::Color& color,
            const ig::Rect& clip) const override
    {
        ++shapeCalls;
        shapeChars += static_cast<long long>(text.size());
        return inner_.appendText(drawList, font, text, position, logicalSize, color, clip);
    }

    // Forwarded, not defaulted: the counter is an instrument, so the frame has
    // to be built exactly as it would be without it - including the white texel
    // the atlas offers for solid geometry.
    ig::TextProvider::SolidStyle solidStyle() const override
    {
        return inner_.solidStyle();
    }

    mutable long long measureCalls;
    mutable long long measureChars;
    mutable long long shapeCalls;
    mutable long long shapeChars;

private:
    const ig::TextProvider& inner_;
};

struct State
{
    bool enabled;
    bool advancedOpen;
    bool nodeOpen;
    float volume;
    int quality;
    // Indice da amostra mais antiga: a serie le-se em anel, como num osciloscopio.
    int plotOffset;
    ig::String name;

    State()
            : enabled(true),
              advancedOpen(true),
              nodeOpen(true),
              volume(0.35f),
              quality(1),
              plotOffset(0),
              name("player")
    {
    }
};

const ig::StringView kQualityItems[] = { ig::StringView("low"), ig::StringView("medium"),
    ig::StringView("high"), ig::StringView("very high"), ig::StringView("extreme") };

// One repetition of the canonical block. Every widget call bumps `widgets` so
// the two runners can be compared on workload, not only on time.
void galleryBlock(ig::Context& ui, State& state, int index, long long& widgets)
{
    char row[32];
    std::snprintf(row, sizeof(row), "row %d", index);

    ui.label(row);
    ++widgets;
    ui.button("Apply");
    ++widgets;
    ui.sameLine();
    ui.checkbox("Enabled", state.enabled);
    ++widgets;
    ui.sliderFloat("Volume", state.volume, 0.0f, 1.0f);
    ++widgets;
    ui.inputText("Name", state.name);
    ++widgets;
    ui.comboBox("Quality", state.quality, ig::Span<const ig::StringView>(kQualityItems));
    ++widgets;
    ui.separator();
    ui.progressBar(state.volume, 1.0f);
    ++widgets;
    if (ui.collapsingHeader("Advanced", state.advancedOpen))
    {
        ++widgets;
        ui.label("first option");
        ui.label("second option");
        ui.label("third option");
        widgets += 3;
    }
    if (ui.treeNode("Node", state.nodeOpen))
    {
        ++widgets;
        ui.label("leaf");
        ++widgets;
    }
}

void textBlock(ig::Context& ui, int index, long long& widgets)
{
    char row[64];
    for (int i = 0; i < 10; ++i)
    {
        std::snprintf(row, sizeof(row), "line %d.%d with some text in it", index, i);
        ui.label(row);
        ++widgets;
    }
}

// A mesma serie nos dois runners: um anel de 90 amostras preenchido uma vez, para
// que o que se mede seja a widget e nao a geracao dos dados.
const float* plotSamples()
{
    static float values[90];
    static bool filled = false;
    if (!filled)
    {
        for (int i = 0; i < 90; ++i) values[i] = std::cos(static_cast<float>(i) * 0.1f);
        filled = true;
    }
    return values;
}

// Um grafico de linha e um histograma sobre a mesma serie, com a largura e a
// altura dadas (nos dois lados) para os dois desenharem o mesmo numero de
// colunas e a legenda ser comparavel.
void plotsBlock(ig::Context& ui, int index, int offset, long long& widgets)
{
    char label[32];
    std::snprintf(label, sizeof(label), "Lines %d", index);
    ui.plotLines(label, plotSamples(), 90, 400.0f, 128.0f, offset, ig::StringView("avg 0.500000"),
            -1.0f, 1.0f);
    ++widgets;
    std::snprintf(label, sizeof(label), "Bars %d", index);
    ui.plotHistogram(label, plotSamples(), 90, 400.0f, 80.0f, 0, ig::StringView(), -1.0f, 1.0f);
    ++widgets;
}

void smallBlock(ig::Context& ui, State& state, int index, long long& widgets)
{
    char row[32];
    std::snprintf(row, sizeof(row), "panel %d", index);

    ui.label(row);
    ++widgets;
    ui.button("Apply");
    ++widgets;
    ui.checkbox("Enabled", state.enabled);
    ++widgets;
    ui.sliderFloat("Volume", state.volume, 0.0f, 1.0f);
    ++widgets;
}

void gridPosition(int index, int columns, float width, float height, float gap, ig::Rect& out)
{
    const int column = columns > 0 ? index % columns : 0;
    const int row = columns > 0 ? index / columns : 0;
    out = ig::Rect(gap + static_cast<float>(column) * (width + gap),
            gap + static_cast<float>(row) * (height + gap), width, height);
}

void buildScene(ig::Context& ui, State& state, const Config& config, int frame, long long& widgets)
{
    // Moving one value every frame keeps the submission honest: a scene that
    // never changes is the one case where a library may skip work, and real
    // applications animate.
    state.volume = 0.5f + 0.4f * std::sin(static_cast<float>(frame) * 0.1f);
    state.plotOffset = frame % 90;

    if (config.scene == "windows")
    {
        int columns = 1;
        while (columns * columns < config.windows) ++columns;
        for (int i = 0; i < config.windows; ++i)
        {
            char title[32];
            std::snprintf(title, sizeof(title), "Panel %d", i);
            ig::Rect bounds;
            gridPosition(i, columns, 280.0f, 180.0f, 8.0f, bounds);
            if (ui.beginWindow(ig::StringView(title), bounds))
            {
                smallBlock(ui, state, i, widgets);
                ui.endWindow();
            }
        }
        return;
    }

    const int blockCount = config.scene == "basic" ? 1 : config.blocks;
    const ig::Rect window(40.0f, 40.0f, 560.0f, config.height - 80.0f);
    if (!ui.beginWindow("Gallery", window)) return;

    for (int i = 0; i < blockCount; ++i)
    {
        if (config.scene == "text") textBlock(ui, i, widgets);
        else if (config.scene == "plots")
            plotsBlock(ui, i, state.plotOffset, widgets);
        else
            galleryBlock(ui, state, i, widgets);
    }
    ui.endWindow();
}

void dumpCommands(const ig::DrawData& data)
{
    std::printf("igui commands: %u\n", static_cast<unsigned>(data.commands.size()));
    for (std::size_t i = 0; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand& command = data.commands[i];
        if (command.type == ig::DrawCommandType::Geometry)
        {
            const ig::GeometryCommand& g = command.payload.geometry;
            // Bounding box of what the command draws, next to its clip: when
            // the box pokes out of the clip the scissor is doing real work.
            float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
            for (uint32_t k = 0; k < g.indexCount; ++k)
            {
                const ig::Vec2& p = data.vertices[data.indices[g.firstIndex + k]].position;
                minX = p.x < minX ? p.x : minX;
                minY = p.y < minY ? p.y : minY;
                maxX = p.x > maxX ? p.x : maxX;
                maxY = p.y > maxY ? p.y : maxY;
            }
            std::printf("  %3u geom  clip=(%7.1f,%7.1f %7.1fx%7.1f) tex=%lu idx=%-5u "
                        "bbox=(%7.1f,%7.1f)-(%7.1f,%7.1f)\n",
                    static_cast<unsigned>(i), static_cast<double>(g.clip.x),
                    static_cast<double>(g.clip.y), static_cast<double>(g.clip.width),
                    static_cast<double>(g.clip.height), static_cast<unsigned long>(g.texture.value),
                    g.indexCount, static_cast<double>(minX), static_cast<double>(minY),
                    static_cast<double>(maxX), static_cast<double>(maxY));
        }
        else
        {
            const ig::TextCommand& t = command.payload.text;
            std::printf("  %3u text  clip=(%7.1f,%7.1f %7.1fx%7.1f) bytes=%u\n",
                    static_cast<unsigned>(i), static_cast<double>(t.clip.x),
                    static_cast<double>(t.clip.y), static_cast<double>(t.clip.width),
                    static_cast<double>(t.clip.height), t.textSize);
        }
    }
}

long long readPeakRssKb()
{
#if defined(__unix__) || defined(__APPLE__)
    struct rusage usage;
    if (::getrusage(RUSAGE_SELF, &usage) == 0) return static_cast<long long>(usage.ru_maxrss);
#endif
    return -1;
}

} // namespace

Result runIgui(const Config& config)
{
    Result result;
    result.lib = "igui";

    CountingBackend backend;
    CountingTextProvider* counter = nullptr;
#if IGUI_BENCH_HAS_FONT
    ig::FontAtlas atlas;
    CountingTextProvider textProvider(atlas);
    if (config.useFont && atlas.valid())
    {
        // A backend that has uploaded the atlas: without a texture the atlas
        // refuses to shape text (that is the guard a real backend satisfies
        // when it creates the texture), and the glyph cost would silently
        // disappear from the numbers.
        atlas.setTexture(ig::TextureId(1));
        backend.provider = &textProvider;
        counter = &textProvider;
    }
    else if (config.useFont)
        result.error = "font atlas unavailable";
    else
        result.error = "fixed-advance text metrics";
#else
    result.error = "built without the font module";
#endif

    ig::Context ui(backend, backend.provider);

    // Square corners, matching the ImGui side, so the vertex counts compare the
    // widgets and not the decoration (see bench/README.md).
    ig::Theme theme = ui.theme();
    theme.borderRadius = 0.0f;
    ui.setTheme(theme);

    State state;
    std::vector<double> submit;
    std::vector<double> render;
    submit.reserve(static_cast<std::size_t>(config.frames));
    render.reserve(static_cast<std::size_t>(config.frames));

    long long widgets = 0;
    for (int frame = 0; frame < config.warmup + config.frames; ++frame)
    {
        widgets = 0;
        if (counter)
        {
            counter->measureCalls = 0;
            counter->measureChars = 0;
            counter->shapeCalls = 0;
            counter->shapeChars = 0;
        }
        backend.vertices = 0;
        backend.indices = 0;
        backend.commands = 0;

        const double start = cpuNowUs();
        ui.beginFrame(ig::FrameInfo(config.width, config.height, 1.0f, 1.0f / 60.0f));
        buildScene(ui, state, config, frame, widgets);
        const ig::DrawData& data = ui.endFrame();
        const double submitted = cpuNowUs();

        backend.render(data);
        const double rendered = cpuNowUs();

        if (frame >= config.warmup)
        {
            submit.push_back(submitted - start);
            render.push_back(rendered - submitted);
        }
        if (config.dump && frame == config.warmup + config.frames - 1) dumpCommands(data);
    }

    result.ran = true;
    result.submitMeanUs = mean(submit);
    result.submitMedianUs = percentile(submit, 0.5);
    result.submitP95Us = percentile(submit, 0.95);
    result.submitMinUs = percentile(submit, 0.0);
    result.renderMeanUs = mean(render);
    result.totalMeanUs = result.submitMeanUs + result.renderMeanUs;
    result.vertices = backend.vertices;
    result.indices = backend.indices;
    result.commands = backend.commands;
    result.widgets = widgets;
    if (counter)
    {
        result.measureCalls = counter->measureCalls;
        result.shapeCalls = counter->shapeCalls;
        result.textChars = counter->measureChars + counter->shapeChars;
    }
    result.peakRssKb = readPeakRssKb();
    return result;
}

} // namespace bench
