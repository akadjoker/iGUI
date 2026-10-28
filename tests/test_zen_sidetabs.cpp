// O SideTabBar numa janela do zen_plataform, sem SDL: o ig::zen::App corre os
// widgets retidos, o gancho de input faz de rato (move, carrega, larga) e o
// callback depois do render lê a janela tal como foi pintada. Com o backend
// fake do zen_plataform (PLATFORM_USE_FAKE=ON) não há janela nem ecrã; com o
// X11/Wayland abre-se uma janela de verdade e o teste sai com 77 (saltado) se
// não houver ecrã.

#include <igui_zen/ZenApp.hpp>

#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "SideTabBar.hpp"
#include "Theme.hpp"

#include <platform.h>

#include <assert.h>
#include <stdio.h>

using namespace ig::retained;

namespace
{

const int kWidth = 900;
const int kHeight = 600;
const Color kViewportBg(22, 23, 26, 255);

struct Scene
{
    SideTabBar* left = nullptr;
    SideTabBar* right = nullptr;
    Panel* viewport = nullptr;
    Label* character = nullptr;
    Label* camera = nullptr;
    ct::Vector<int> leftChanged;
    ct::Vector<int> rightChanged;
};

void build(WidgetApp& app, Scene& s)
{
    Widget* root = app.addStage("sidetabs");
    auto* body = root->createChild<BoxLayout>(LayoutDir::Horizontal);
    body->setStretch(1);
    body->setSpacing(0);
    body->setPadding(0);

    s.left = body->createChild<SideTabBar>(SideTabBar::Side::Left);
    s.left->setPageWidth(240.0f);
    s.character = s.left->addPage<Label>(IconId::User, "Character", "character");
    s.camera = s.left->addPage<Label>(IconId::Eye, "Camera", "camera");
    s.left->addPage<Label>(IconId::Edit, "Pose", "pose");
    s.left->addPage<Label>(IconId::None, "Import", "import");
    s.left->setPageBadge(2, true);
    s.left->setPageEnabled(3, false);

    s.viewport = body->createChild<Panel>();
    s.viewport->setStretch(1);
    s.viewport->setBgColor(kViewportBg);

    s.right = body->createChild<SideTabBar>(SideTabBar::Side::Right);
    s.right->setShowLabels(false);
    s.right->setButtonHeight(44.0f);
    s.right->setPageWidth(200.0f);
    s.right->addPage<Label>(IconId::Gear, "Settings", "settings");
    s.right->addPage<Label>(IconId::Info, "Log", "log");
    s.right->collapse();

    s.left->currentChanged.connect([&s](int index) { s.leftChanged.push_back(index); });
    s.right->currentChanged.connect([&s](int index) { s.rightChanged.push_back(index); });

    app.setTransition(TransitionType::None, 0.0f);
    app.setStage("sidetabs");
}

uint32_t argb(const Color& c)
{
    return (static_cast<uint32_t>(c.a) << 24) | (static_cast<uint32_t>(c.r) << 16) |
           (static_cast<uint32_t>(c.g) << 8) | static_cast<uint32_t>(c.b);
}

// Os passos do guião: cada um espera alguns frames para o clique chegar ao
// widget e o layout assentar, e depois verifica o que a janela mostra.
enum class Step
{
    Start,
    OpenCamera,
    FoldLeft,
    ClickDisabled,
    OpenRight,
    Done
};

struct Runner
{
    ig::zen::App& zen;
    Scene& s;
    Step step = Step::Start;
    int stepFrame = 0;    // o frame em que o passo começou
    float clickX = -1.0f; // o clique em curso, -1 sem nenhum
    float clickY = -1.0f;
    ct::Vector<uint32_t> pixels;
    int pw = 0, ph = 0;
    int checks = 0;

    uint32_t pixel(float x, float y) const
    {
        const int ix = static_cast<int>(x), iy = static_cast<int>(y);
        assert(ix >= 0 && iy >= 0 && ix < pw && iy < ph);
        return pixels[static_cast<size_t>(iy) * pw + ix] | 0xff000000u;
    }

    void clickButton(SideTabBar* bar, int index)
    {
        Rect r;
        assert(bar->buttonRect(index, r));
        clickX = r.x + r.w * 0.5f;
        clickY = r.y + r.h * 0.5f;
    }

    // Um ponto dentro da zona de páginas, longe do texto do Label
    void checkPageArea(SideTabBar* bar)
    {
        const Rect page = bar->pageAreaRect();
        assert(page.w > 0.0f);
        assert(pixel(page.x + page.w * 0.5f, page.y + page.h - 8.0f) ==
                argb(Theme::instance().sideTabPageBg));
        ++checks;
    }

    void next(Step to, int frame)
    {
        step = to;
        stepFrame = frame;
    }

    // Frame f+0: o rato em cima do botão; f+1: carrega; f+2: larga; f+5: verifica
    void input(int frame, IO& io)
    {
        const int t = frame - stepFrame;
        if (clickX >= 0.0f)
        {
            io.mouseX = clickX;
            io.mouseY = clickY;
            io.mouseDown[0] = t == 1;
        }
        else
        {
            io.mouseX = 5.0f;
            io.mouseY = kHeight - 5.0f;
            io.mouseDown[0] = false;
        }
        if (t < 5) return;
        verify(frame);
    }

    void verify(int frame)
    {
        switch (step)
        {
            case Step::Start:
                // A esquerda abre na primeira página, a direita começa dobrada
                assert(s.left->currentIndex() == 0 && s.character->isVisible() &&
                        !s.camera->isVisible());
                assert(s.right->isCollapsed() && s.right->pageAreaRect().w == 0.0f);
                assert(s.right->absoluteRect().x + s.right->absoluteRect().w ==
                        static_cast<float>(kWidth));
                checkPageArea(s.left);
                {
                    // A calha da direita e o ponto da página com o badge
                    Rect pose;
                    assert(s.left->buttonRect(2, pose));
                    assert(pixel(pose.x + pose.w - 9.0f, pose.y + 9.0f) ==
                            argb(Theme::instance().sideTabBadge));
                    const Rect rail = s.right->absoluteRect();
                    assert(pixel(rail.x + rail.w * 0.5f, kHeight - 8.0f) ==
                            argb(Theme::instance().sideTabRailBg));
                    checks += 2;
                }
                clickButton(s.left, 1);
                next(Step::OpenCamera, frame);
                break;

            case Step::OpenCamera:
                assert(s.left->currentIndex() == 1 && s.camera->isVisible() &&
                        !s.character->isVisible());
                assert(s.leftChanged.size() == 1u && s.leftChanged[0] == 1);
                checkPageArea(s.left);
                clickButton(s.left, 1); // o botão ativo dobra a página
                next(Step::FoldLeft, frame);
                break;

            case Step::FoldLeft:
            {
                assert(s.left->isCollapsed() && !s.camera->isVisible());
                assert(s.leftChanged.size() == 2u && s.leftChanged[1] == -1);
                // O viewport ocupa o lugar da página: começa logo a seguir à calha
                const float rail = s.left->effectiveRailWidth();
                const Rect vp = s.viewport->absoluteRect();
                assert(vp.x > rail - 1.0f && vp.x < rail + 1.0f);
                assert(pixel(rail + 120.0f, kHeight * 0.5f) == argb(kViewportBg));
                ++checks;
                clickButton(s.left, 3); // desativado: não faz nada
                next(Step::ClickDisabled, frame);
                break;
            }

            case Step::ClickDisabled:
                assert(s.left->isCollapsed() && s.leftChanged.size() == 2u);
                clickButton(s.right, 1);
                next(Step::OpenRight, frame);
                break;

            case Step::OpenRight:
            {
                assert(s.right->currentIndex() == 1 && s.rightChanged.size() == 1u &&
                        s.rightChanged[0] == 1);
                // A página da direita abre para dentro, a calha fica encostada à borda
                const Rect page = s.right->pageAreaRect();
                Rect button;
                assert(s.right->buttonRect(1, button));
                assert(page.x + page.w <= button.x + 0.5f);
                assert(button.x + button.w <= static_cast<float>(kWidth) + 0.5f);
                checkPageArea(s.right);
                clickX = clickY = -1.0f;
                next(Step::Done, frame);
                window_set_should_close(zen.window(), true);
                break;
            }

            case Step::Done:
                break;
        }
    }
};

} // namespace

int main()
{
    ig::zen::App::Config config;
    config.title = "iGUI SideTabBar test";
    config.width = kWidth;
    config.height = kHeight;
    config.preferOpenGL = false; // o rasterizador de software: o mesmo resultado com ou sem GPU
    config.vsync = false;
    config.showStats = false;
    config.samples = 1;

    ig::zen::App zen;
    if (!zen.create(config))
    {
        fprintf(stderr, "test_zen_sidetabs: no window (no display?), skipped\n");
        return 77;
    }

    Scene scene;
    build(zen.widgets(), scene);

    Runner runner{ zen, scene };
    zen.setInputHook([&runner](int frame, IO& io) { runner.input(frame, io); });
    zen.setAfterRenderCallback(
            [&runner, &zen] { assert(zen.readWindowPixels(runner.pixels, runner.pw, runner.ph)); });

    // Uma rede de segurança: um passo que não avança não pode prender o teste
    zen.setFrameCallback(
            [&zen](float)
            {
                static int frames = 0;
                if (++frames > 600)
                {
                    fprintf(stderr, "test_zen_sidetabs: the script did not finish\n");
                    assert(false);
                    window_set_should_close(zen.window(), true);
                }
            });

    zen.run();
    assert(runner.step == Step::Done);
    assert(runner.checks == 6);
    printf("test_zen_sidetabs: ok (%d pixel checks)\n", runner.checks);
    return 0;
}
