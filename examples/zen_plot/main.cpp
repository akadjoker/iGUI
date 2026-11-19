// A time-series chart that zooms, pans, selects a range and shows statistics, in a zen_plataform window (docs/ROUND10.md).
//
//   wheel            zoom x about the pointer           Ctrl + wheel   zoom y            Shift + wheel   pan
//   drag             pan                                Shift + drag   select a range   Escape or click clear it
//   double click     fit everything                     click a legend entry: show / hide it, the cross removes it
//   T                the next theme preset              C   cut off the data and push new points (a changed revision)
//
// The second chart below follows the x axis of the first one (PlotAxisLink).
#include <igui_zen/ZenApp.hpp>

#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/PlotChart.hpp>
#include <igui/widgets/Theme.hpp>

#include <math.h>
#include <stdio.h>

using namespace ig::retained;

namespace
{

struct Channel
{
    ct::Vector<double> x, y;
};

// the statistics of a channel over [a, b]
void stats(const Channel& c, double a, double b, double& lo, double& hi, double& mean, size_t& n)
{
    lo = 1e300;
    hi = -1e300;
    double sum = 0.0;
    n = 0;
    for (size_t i = 0; i < c.x.size(); ++i)
    {
        if (c.x[i] < a || c.x[i] > b || c.y[i] != c.y[i]) continue;
        if (c.y[i] < lo) lo = c.y[i];
        if (c.y[i] > hi) hi = c.y[i];
        sum += c.y[i];
        ++n;
    }
    mean = n ? sum / static_cast<double>(n) : 0.0;
}

} // namespace

int main()
{
    ig::zen::App zen;
    ig::zen::App::Config config;
    config.title = "iGUI chart";
    config.width = 1280;
    config.height = 800;
    if (!zen.create(config)) return 1;

    // data: pushed by the application; the chart keeps pointers (no copy) and a revision says when they changed
    const size_t n = 200000;
    ct::Vector<Channel> channels;
    channels.resize(4);
    for (size_t c = 0; c < channels.size(); ++c)
    {
        channels[c].x.resize(n);
        channels[c].y.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            const double t = static_cast<double>(i) * 0.001; // 200 s at 1 kHz
            channels[c].x[i] = t;
            double v = 0.0;
            switch (c)
            {
                case 0: v = sin(t * 0.8) + 0.15 * sin(t * 61.0); break;
                case 1: v = 0.5 * sin(t * 0.3 + 1.0) + ((i / 20000) % 2 ? 0.4 : -0.4); break;
                case 2: v = (static_cast<double>((i * 7919u) % 1000u) - 500.0) * 0.0004 + 0.3 * sin(t * 2.0); break;
                default: v = 40.0 + 25.0 * sin(t * 0.2); break; // on the right axis, in other units
            }
            if (c == 2 && i > 90000 && i < 92000) v = static_cast<double>(NAN); // a gap
            channels[c].y[i] = v;
        }
    }
    uint64_t revision = 1;

    Widget* root = zen.widgets().addStage("main");
    auto* column = root->createChild<BoxLayout>(LayoutDir::Vertical);
    column->setStretch(1);
    column->setSpacing(6);
    column->setPadding(8);
    auto* chart = column->createChild<PlotChart>();
    chart->setStretch(3);
    auto* overview = column->createChild<PlotChart>();
    overview->setStretch(1);
    overview->setLegendVisible(false);

    const char* names[] = { "speed", "torque", "noise", "temperature" };
    const char* units[] = { "m/s", "Nm", "V", "C" };
    for (int c = 0; c < 4; ++c)
    {
        PlotSeriesInfo info;
        info.id = c + 1;
        info.name = names[c];
        info.unit = units[c];
        info.yAxis = c == 3 ? 1 : 0;
        chart->setSeries(info);
        chart->setSeriesData(c + 1, channels[static_cast<size_t>(c)].x.data(), channels[static_cast<size_t>(c)].y.data(), n, revision);
    }
    chart->setLegendRemovable(true);
    chart->setMarker(1, 20.0, Color(230, 80, 80, 255), "In");
    chart->setMarker(2, 140.0, Color(80, 200, 120, 255), "Out");
    chart->setNote("1 kHz, 200 s");
    chart->fit();

    PlotSeriesInfo ov;
    ov.id = 1;
    ov.name = "speed";
    overview->setSeries(ov);
    overview->setSeriesData(1, channels[0].x.data(), channels[0].y.data(), n, revision);
    overview->fit();
    PlotAxisLink link;
    link.add(chart);
    link.add(overview);

    // the selected range: a band, and statistics for each series under the plot
    chart->rangeSelected.connect(
            [&](double a, double b)
            {
                ct::Vector<String> rows;
                rows.push_back("min");
                rows.push_back("max");
                rows.push_back("mean");
                rows.push_back("samples");
                ct::Vector<PlotStatsColumn> columns;
                for (int c = 0; c < 4; ++c)
                {
                    double lo, hi, mean;
                    size_t count;
                    stats(channels[static_cast<size_t>(c)], a, b, lo, hi, mean, count);
                    char text[4][32];
                    snprintf(text[0], 32, "%.3f", lo);
                    snprintf(text[1], 32, "%.3f", hi);
                    snprintf(text[2], 32, "%.3f", mean);
                    snprintf(text[3], 32, "%zu", count);
                    PlotStatsColumn col;
                    col.seriesId = c + 1;
                    col.header = names[c];
                    for (int r = 0; r < 4; ++r) col.cells.push_back(text[r]);
                    columns.push_back(col);
                }
                chart->setStats(rows, columns);
            });
    chart->rangeCleared.connect([&] { chart->clearStats(); });
    chart->legendEntryClicked.connect([&](int index) { chart->setSeriesVisible(chart->seriesIdAt(index), !chart->seriesInfo(chart->seriesIdAt(index))->visible); });
    chart->legendEntryRemoveClicked.connect([&](int index) { chart->removeSeries(chart->seriesIdAt(index)); });
    // the clock line moves every frame, the series layer does not rebuild
    double clock = 0.0;
    zen.setIdleSkipWithCallbacks(true);
    zen.setFrameCallback(
            [&](float dt)
            {
                clock += static_cast<double>(dt);
                if (clock > 200.0) clock = 0.0;
                chart->setClock(clock, true);
            });

    zen.widgets().setTransition(TransitionType::None, 0.0f);
    zen.widgets().setStage("main");
    int preset = 0;
    // keys: T next preset, C new data
    zen.setInputHook(
            [&](int, IO& io)
            {
                for (size_t i = 0; i < io.keyEvents.size(); ++i)
                {
                    const IO::KeyEvent& k = io.keyEvents[i];
                    if (!k.down) continue;
                    if (k.key == Key::T)
                    {
                        preset = (preset + 1) % Theme::kPresetCount;
                        zen.widgets().setThemePreset(Theme::presetName(preset));
                    }
                    if (k.key == Key::C)
                    {
                        // new samples: the arrays change in place, so the revision changes
                        for (size_t j = 0; j < n; ++j) channels[0].y[j] *= 0.98;
                        ++revision;
                        chart->setSeriesData(1, channels[0].x.data(), channels[0].y.data(), n, revision);
                        overview->setSeriesData(1, channels[0].x.data(), channels[0].y.data(), n, revision);
                    }
                }
            });
    zen.run();
    return 0;
}
