// Video monitor stage: a VideoCanvas fed with synthetic frames, one texture
// update per tick, the way a decoder would drive it.

#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "ViewWidgets.hpp"
#include "Animation.hpp"
#include <igui/widgets/String.hpp>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

using namespace ig::retained;

namespace
{
constexpr int kW = 960;
constexpr int kH = 540;

struct VideoStage
{
    unsigned char *rgba = static_cast<unsigned char *>(malloc(static_cast<size_t>(kW) * kH * 4));
    TextureHandle texture;
    VideoCanvas *canvas = nullptr;
    Label *status = nullptr;
    int frame = 0;
    int hoverX = -1, hoverY = -1;

    void put(int x, int y, int r, int g, int b, int a)
    {
        unsigned char *p = rgba + (static_cast<size_t>(y) * kW + x) * 4u;
        p[0] = static_cast<unsigned char>(r);
        p[1] = static_cast<unsigned char>(g);
        p[2] = static_cast<unsigned char>(b);
        p[3] = static_cast<unsigned char>(a);
    }

    // Colour bars, a grey ramp, a moving box and a soft translucent disc.
    void render(float t)
    {
        static const int bars[7][3] = {{235, 235, 235}, {235, 235, 16}, {16, 235, 235}, {16, 235, 16},
                                       {235, 16, 235}, {235, 16, 16},   {16, 16, 235}};
        const int boxX = static_cast<int>((sinf(t * 1.7f) * 0.5f + 0.5f) * (kW - 120));
        const int boxY = kH * 2 / 3 + static_cast<int>((cosf(t * 2.3f) * 0.5f + 0.5f) * (kH / 3 - 80));
        const float discX = kW * 0.5f + cosf(t) * 200.0f, discY = kH * 0.33f + sinf(t * 1.3f) * 60.0f;
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
            {
                int r, g, b, a = 255;
                if (y < kH * 2 / 3)
                {
                    const int *c = bars[x * 7 / kW];
                    r = c[0]; g = c[1]; b = c[2];
                }
                else
                    r = g = b = x * 255 / (kW - 1);
                if (((x ^ y) & 63) == 0)
                    r = g = b = 255 - r;
                if (x >= boxX && x < boxX + 120 && y >= boxY && y < boxY + 80)
                    r = 250, g = 250, b = 250;
                const float dx = x - discX, dy = y - discY;
                const float d = sqrtf(dx * dx + dy * dy);
                if (d < 90.0f)
                    a = 255 - static_cast<int>((1.0f - d / 90.0f) * 235.0f);
                put(x, y, r, g, b, a);
            }
    }

    void updateStatus()
    {
        String text = "frame " + String::number(frame) + "   " + String::number(kW) + "x" + String::number(kH) +
                      "   zoom " + String::number(static_cast<int>(canvas->scale() * 100.0f)) + "%";
        if (hoverX >= 0)
            text = text + "   pixel " + String::number(hoverX) + "," + String::number(hoverY);
        status->setText(text);
    }
};
} // namespace

void registerVideoStage(WidgetApp &app)
{
    VideoStage *s = new VideoStage();
    auto *root = app.addStage("video");
    auto *outer = root->createChild<BoxLayout>(LayoutDir::Vertical);
    outer->setPadding(10.0f);
    outer->setSpacing(8.0f);
    outer->setStretch(1);

    auto *bar = outer->createChild<BoxLayout>(LayoutDir::Horizontal);
    bar->setSpacing(10.0f);
    auto *back = bar->createChild<Button>("← Menu");
    back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });
    auto *fit = bar->createChild<Button>("Fit");
    auto *actual = bar->createChild<Button>("100%");
    auto *grid = bar->createChild<CheckBox>("Pixel grid");
    auto *safe = bar->createChild<CheckBox>("Safe areas");
    auto *checker = bar->createChild<CheckBox>("Alpha checker");
    checker->setChecked(true);

    s->canvas = outer->createChild<VideoCanvas>();
    s->canvas->setStretch(1);
    s->canvas->setCheckerboard(true);
    s->status = outer->createChild<Label>("");

    s->render(0.0f);
    s->texture = app.uploadTexture(s->rgba, kW, kH);
    s->canvas->setTexture(s->texture, kW, kH);
    s->updateStatus();

    fit->clicked.connect([s]() { s->canvas->resetView(); });
    actual->clicked.connect([s]() { s->canvas->actualSize(); });
    grid->toggled.connect([s](bool on) { s->canvas->setShowGrid(on); });
    safe->toggled.connect([s](bool on) { s->canvas->setShowSafeAreas(on); });
    checker->toggled.connect([s](bool on) { s->canvas->setCheckerboard(on); });
    s->canvas->viewChanged.connect([s]() { s->updateStatus(); });
    s->canvas->hoveredPixel.connect([s](int x, int y) { s->hoverX = x; s->hoverY = y; s->updateStatus(); });

    // 30 fps frame clock: a decoder would hand over a frame here instead.
    auto *clock = new Animation(0.0f, 1.0f, 1.0f / 30.0f, EaseType::Linear);
    clock->setLoopMode(Animation::LoopMode::Loop);
    clock->setLoopCount(0);
    clock->onLoopEnd.connect([&app, s](int) {
        ++s->frame;
        s->render(static_cast<float>(s->frame) / 30.0f);
        if (app.updateTexture(s->texture, s->rgba, kW, kH))
            s->canvas->refresh();
        s->updateStatus();
    });
    Animator::instance().add(clock);
    clock->start();
}
