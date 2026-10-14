#pragma once

#include "Modes.hpp"

#if !IGUI_IMMEDIATEMODE
#  error "iGUI: <igui/Gui.hpp> is the immediate-mode API, but IGUI_IMMEDIATEMODE is 0 (see igui/Modes.hpp)."
#endif

#include <stdint.h>

#include <ct/deque.hpp>
#include <ct/function.hpp>
#include <ct/hashmap.hpp>
#include <ct/slotmap.hpp>
#include <ct/vector.hpp>

#include "Backend.hpp"
#include "CodeEditor.hpp"
#include "Events.hpp"
#include "FileDialog.hpp"
#include "Math.hpp"
#include "Theme.hpp"
#include "WindowStateIni.hpp"

namespace ig
{

struct WindowState
{
    WidgetId id;
    String title;
    Rect bounds;
    Rect restoreBounds;
    bool maximized = false;
    bool hasRestoreBounds = false;
    bool open;
    bool minimized;
    bool focused;
    bool showWindowControls;
    // Submitted with a bool *open this frame: only then is there an X to close it.
    bool closable = false;
    bool showMinimizeButton;
    bool showMaximizeButton;
    bool showTitleBar;
    bool useClientArea;
    bool allowMove;
    bool allowResize;
    // false keeps this window's geometry out of the state file (see
    // setWindowStatePath).
    bool saveSettings;
    // Geometry came from the state file before the viewport was known: it is
    // fitted to it on the first frame the window is submitted.
    bool clampToViewport = false;
    uint64_t zOrder;
    // frameNumber_ of the last beginWindow() for this window. A window the
    // application stopped submitting is not on screen, so it must not keep
    // catching the pointer (topWindowAt) or holding the modal lock.
    uint64_t lastFrame = 0;
    // Submetida agora pela primeira vez, ou depois de ter estado fora (ver
    // isWindowAppearing).
    bool appeared = false;
    DrawList drawList;
    DrawList overlayDrawList;

    WindowState()
        : id(InvalidWidgetId), title(), bounds(), open(true), minimized(false), focused(false),
          showWindowControls(true), showMinimizeButton(true), showMaximizeButton(true),
          showTitleBar(true), useClientArea(false), allowMove(true), allowResize(true),
          saveSettings(true), zOrder(0), drawList(), overlayDrawList()
    {
    }
};

using WindowHandle = ct::Handle<WindowState>;

struct TreeItemStyle
{
    Color typeColor;
    bool selected;
    bool leaf;
    bool disabled;
    // A leaf can still accept a child; it gains an expansion arrow once the
    // application's tree model reports that child on the next frame.
    bool acceptsChildren;

    TreeItemStyle()
        : typeColor(120u, 170u, 230u, 255u), selected(false), leaf(false), disabled(false),
          acceptsChildren(false) {}
};

// Looks of an expandable row (treeNode and collapsingHeader). Every option is off by
// default, which is the plain arrow + label row.
struct TreeStyle
{
    // A dot instead of the disclosure arrow - the Bullet of the ImGui tree node
    // flags, for the bullet lists the demo draws with TreeNode.
    bool bullet;
    // The row is drawn as chosen, like a selectable (the Selected flag).
    bool selected;
    // Only the arrow opens and closes the row: a click anywhere else just reports
    // it (the OpenOnArrow flag).
    bool openOnArrow;
    // The row's hover and click cover the whole width it is given, not just the
    // label (the SpanAvailWidth flag): what a tree row in a list wants.
    bool spanAvailWidth;

    TreeStyle()
        : bullet(false), selected(false), openOnArrow(false), spanAvailWidth(false) {}
};

// Looks of an immediate table. Every option is off by default, which draws no
// background and no lines.
struct TableStyle
{
    // Every second row gets a faint overlay of the text color.
    bool alternateRows;
    // Lines between rows and columns and around the table.
    bool borders;
    // Minimum height of a row; 0 uses the theme's widget height.
    float rowHeight;
    // Gap between rows; a negative value uses the theme's item spacing.
    float rowSpacing;
    // Colunas reordenáveis: arrastar um cabeçalho para outro sítio troca a ordem,
    // que fica guardada por tabela (pelo id) entre frames. Quem desenha a coluna lê
    // a sua coluna do modelo com tableColumnIndex(), que é a posição na ordem.
    bool reorderable;
    // Rolagem própria da tabela: a tabela passa a ter uma altura fixa (a do campo
    // height) e as linhas rolam dentro dela, com a barra no lado direito. Sem isto a
    // tabela cresce com as linhas e quem rola é a janela ou o child onde ela está.
    bool scrollY;
    // Quantas linhas ficam presas no topo (incluindo a linha 0, que é o cabeçalho):
    // freezeRows = 1 prende só o cabeçalho, 2 prende o cabeçalho e a primeira linha.
    // Uma tabela sem cabeçalho usa 0, que não prende nada. Ignorado sem scrollY; com
    // scrollY e um valor negativo vale 1.
    int freezeRows;
    // Altura da tabela (com o cabeçalho e as linhas presas lá dentro) quando ela
    // rola. Sem scrollY, ou com 0, a altura é a das linhas.
    float height;
    // Rolagem horizontal: as colunas são a largura de innerWidth (0 = a da tabela) e
    // o que não couber na tabela rola dentro dela, com a barra em baixo. Sem isto as
    // colunas dividem a largura da tabela e nunca sobra nada para rolar - é a
    // diferença de fundo para o ImGui, onde a largura é de cada coluna.
    bool scrollX;
    float innerWidth;
    // Quantas colunas (as primeiras, na ordem do écra) ficam presas à esquerda
    // enquanto o resto rola. Ignorado sem scrollX.
    int freezeCols;
    // As colunas ficam com a largura do maior conteúdo delas em vez de dividirem a
    // largura da tabela pelos pesos (é o SizingFixedFit do ImGui). A largura é a que
    // as células pediram no frame anterior (o conteúdo de uma célula só se sabe
    // depois de ela ser submetida, como tudo o que se mede aqui); na primeira vez
    // vale a divisão pelos pesos, que é onde as colunas já estavam.
    bool sizingFixedFit;

    TableStyle()
        : alternateRows(false), borders(false), rowHeight(0.0f), rowSpacing(-1.0f),
          reorderable(false), scrollY(false), freezeRows(0), height(0.0f), scrollX(false),
          innerWidth(0.0f), freezeCols(0), sizingFixedFit(false) {}
};

// The relation selected when a tree item is dropped over another item.
enum class TreeDropPosition : uint8_t
{
    Before,
    Inside,
    After
};

// Returned by the draggable treeItem overload. Apply this operation to the
// application's tree model after finishing its current draw pass.
struct TreeDrop
{
    WidgetId source;
    WidgetId target;
    TreeDropPosition position;

    TreeDrop()
        : source(InvalidWidgetId), target(InvalidWidgetId), position(TreeDropPosition::Inside) {}
};

enum class MessageBoxResult : uint8_t
{
    None,
    Accepted,
    Cancelled,
    Discarded
};

enum class MessageBoxKind : uint8_t
{
    Information,
    Warning,
    Error,
    Input
};

struct MessageBoxOptions
{
    StringView acceptLabel = "OK";
    StringView cancelLabel = "Cancel";
    bool showDiscard = false;
    MessageBoxKind kind;
    bool showCancel;
    String *inputValue;

    MessageBoxOptions()
        : kind(MessageBoxKind::Information), showCancel(false), inputValue(nullptr) {}
};

enum class ToastPosition : uint8_t
{
    TopLeft, TopCenter, TopRight,
    MiddleLeft, Center, MiddleRight,
    BottomLeft, BottomCenter, BottomRight
};

struct DragDropPayload
{
    WidgetId type;
    WidgetId source;
    uint64_t data;

    DragDropPayload() : type(InvalidWidgetId), source(InvalidWidgetId), data(0u) {}
};

enum class SplitterAxis : uint8_t
{
    Vertical,
    Horizontal
};

// A dockspace divides one editor surface into persistent tab regions.
enum class DockSlot : uint8_t
{
    Left,
    Center,
    Right,
    Bottom
};

enum class Gizmo2DMode : uint8_t
{
    Translate,
    Rotate,
    Scale
};

enum class Gizmo3DMode : uint8_t
{
    Translate,
    Rotate,
    Scale
};

// Transform values are expressed in pixels relative to the gizmo canvas.
// rotation is in degrees and scale starts at (1, 1).
struct Transform2D
{
    Vec2 position;
    float rotation;
    Vec2 scale;

    Transform2D() : position(), rotation(0.0f), scale(1.0f, 1.0f) {}
};

struct Gizmo2DOptions
{
    float axisLength;
    float translateSnap;
    float rotateSnap;
    float scaleSnap;

    Gizmo2DOptions()
        : axisLength(60.0f), translateSnap(0.0f), rotateSnap(0.0f), scaleSnap(0.0f) {}
};

struct Transform3D
{
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;

    Transform3D() : position(), rotation(), scale(1.0f, 1.0f, 1.0f) {}
};

struct Gizmo3DOptions
{
    float axisLength;
    float translateSnap;
    float rotateSnap;
    float scaleSnap;
    // Draw the box the handles move, shaded and outlined. Without it a turn
    // shows only a number: the rings are pinned to the world, so they stay put
    // while the model turns.
    bool showModel;

    Gizmo3DOptions()
        : axisLength(1.0f), translateSnap(0.0f), rotateSnap(0.0f), scaleSnap(0.0f),
          showModel(true) {}
};

// Application-owned stop for gradientEditor(). Positions are normalized to
// [0, 1]; the editor keeps the vector sorted after every interaction.
struct GradientStop
{
    float position;
    Color color;

    GradientStop() : position(0.0f), color() {}
    GradientStop(float value, const Color &stopColor) : position(value), color(stopColor) {}
};

// Application-owned point for curveEditor(). Coordinates use the supplied
// value range and are kept ordered by x after an edit.
struct CurvePoint
{
    float x;
    float y;

    CurvePoint() : x(0.0f), y(0.0f) {}
    CurvePoint(float pointX, float pointY) : x(pointX), y(pointY) {}
};
struct TimelineTrack
{
    String label;
    ct::Vector<int> keys;
};
struct SequencerTrack { String label; int startFrame; int endFrame; Color color;
    SequencerTrack() : label(), startFrame(0), endFrame(0), color(90,160,230) {}
    SequencerTrack(StringView n, int s, int e, const Color& c) : label(n.data(),n.size()), startFrame(s), endFrame(e), color(c) {} };

// Which way a mixer fader travels. A vertical one reads upwards, like the ones
// on a desk: the maximum sits at the top.
enum class FaderOrientation : uint8_t
{
    Vertical,
    Horizontal
};

// The four ways an arrow can point (see arrowButton).
enum class ArrowDirection : uint8_t
{
    Up,
    Down,
    Left,
    Right
};

// A fader's colours. With none set it follows the theme - the groove and the
// cap from the slider colours, the fill and the line across the cap from the
// accent - so an application picks the accent instead of receiving a fixed one.
struct FaderStyle
{
    Color fill;
    Color groove;
    Color cap;
    Color capLine;
    bool hasFill;
    bool hasGroove;
    bool hasCap;
    bool hasCapLine;
    // Marks down the side of the groove, long ones every half.
    bool ticks;

    FaderStyle()
        : fill(0u, 0u, 0u, 0u), groove(0u, 0u, 0u, 0u), cap(0u, 0u, 0u, 0u),
          capLine(0u, 0u, 0u, 0u), hasFill(false), hasGroove(false), hasCap(false),
          hasCapLine(false), ticks(true)
    {
    }

    // Only the accent changes; the groove and the cap stay as the theme draws
    // them.
    static FaderStyle accent(const Color &color);
};

class Context
{
public:
    explicit Context(Backend &backend, TextProvider *textProvider = nullptr);

    /// @brief Destroy the context, writing the window state file first (see
    ///        setWindowStatePath).
    ~Context();

    void pushEvent(const Event &event);
    void beginFrame(const FrameInfo &frame);
    const DrawData &endFrame();

    // Chrome and interaction for beginWindow(): the switches the demo's "Window
    // options" section is about. The default is a normal window - a title bar
    // with the window controls, movable, resizable, and its geometry kept in the
    // state file.
    struct WindowOptions
    {
        bool titleBar = true;
        bool move = true;
        bool resize = true;
        bool saveSettings = true;
        // The bounds passed to beginWindow are applied every frame instead of
        // only when the window is created, so a drag or a state file cannot move
        // it. What an overlay strip or a splash window wants.
        bool fixedBounds = false;
    };

    bool beginWindow(StringView title, const Rect &initialBounds, bool *open = nullptr);
    bool beginWindow(StringView title, const Rect &initialBounds, const WindowOptions &options,
                     bool *open = nullptr);
    // A root window that follows the current viewport every frame.
    bool beginMainWindow(StringView title, bool *open = nullptr);
    void endWindow();
    // A barra de menu da aplicação: uma faixa no topo do ecrã que não pertence a
    // nenhuma janela, como o BeginMainMenuBar do ImGui. Devolve false quando não
    // há altura ou largura para ela. Lá dentro usam-se o beginMenu/endMenu (e não
    // o beginMenuBar) e fecha-se com endMainMenuBar. A altura que ela ocupa - que
    // a aplicação tem de deixar livre no topo das suas janelas - é o
    // mainMenuBarHeight().
    bool beginMainMenuBar();
    void endMainMenuBar();
    float mainMenuBarHeight() const;
    /// @brief Bounds the window with @p title has right now (an empty rect when
    ///        no such window exists yet).
    Rect windowBounds(StringView title) const;

    // ── Window state (.ini, like ImGui's imgui.ini) ───────────────────────
    // Each window keeps a section named after its title with where it was left
    // and its size, plus whether it was minimized/maximized; a dock space keeps
    // its region sizes and which panel sits in which of them (and which tab of a
    // group is on top). A window the file knows is created where the file says
    // instead of at initialBounds; a window it does not know is created as asked
    // and stored on the way out. Nothing happens until a path is set, and the
    // destructor rewrites the file (an explicit saveWindowState() does the same
    // earlier).
    //
    //   ig::Context context(backend);
    //   context.setWindowStatePath("app.ini");
    //
    // Desktop only: neither a browser tab nor an Android app can keep the file,
    // so there the state stays in memory and the disk is never touched.
    void setWindowStatePath(const String &path);
    /// @brief Get the state file path ("" when persistence is off).
    const String &windowStatePath() const { return windowState_.path(); }
    /// @brief (Re)read the state file and move the windows that exist to what it
    ///        says.
    /// @return true when the file was read.
    bool loadWindowState();
    /// @brief Store the current windows and write the file.
    /// @return true when the file was written.
    bool saveWindowState();
    // Windows live as long as the Context: one the application stops submitting
    // keeps where it was left (that is what makes a closed panel come back where
    // it belongs), so an application that opens windows with generated titles -
    // a document each, a preview each - would keep one per title forever. This
    // gets rid of one for good: after it, beginWindow() with the same title
    // starts from initialBounds again, and the .ini section that remembered it
    // goes away with it. Called between frames.
    bool removeWindow(StringView title);
    // The per-widget state kept by id - scroll positions, the text a numeric
    // field is being edited with, an open colour picker, a sequencer's zoom, a
    // dock arrangement - also lives as long as the Context, and ids are hashes:
    // a screen whose labels change with the data (a row per record) leaves one
    // entry per label. Call this when such a screen is gone. Anything being
    // edited is lost and scroll positions start over; call it between frames.
    void clearWidgetState();
    // Operate on existing floating windows identified by their title.
    void maximizeWindow(StringView title);
    void restoreWindow(StringView title);
    // Hides the minimize/maximize title-bar buttons of the window with this
    // title (the close button stays) - for popups that shouldn't be
    // minimised/maximised. Cheap enough to call every frame next to
    // beginWindow; a window that doesn't exist yet records the choice and
    // gets it when it is created (no first-frame flash).
    void setWindowButtons(StringView title, bool minimize, bool maximize);
    // Moves the open window with this title to the top of the z-order (a
    // no-op when it doesn't exist). Call it every frame next to beginWindow
    // for a window that must stay above everything else - otherwise a click
    // on any other window raises that one over it, and the popup ends up
    // invisible-but-open behind the main window. `focus` additionally makes
    // it the focused window every frame: a window's widgets only accept a
    // click while it is the focused one (itemClicked's
    // currentWindow_ == focusedWindow_ gate), so without this a popup stops
    // receiving events as soon as the user clicks outside it.
    void raiseWindow(StringView title, bool focus = false);
    // Marks (or unmarks) the window with this title as application-modal:
    // while it is open, no other window's widgets accept hover, clicks or
    // keyboard - same block messageBox gets from activeModal_, but scoped to
    // this window so its own widgets keep working. The window stays drawn
    // behind it, unlike the scrim messageBox paints. The lock also ends by
    // itself once the window is closed or no longer submitted, but callers
    // should still unmark it when the window closes (Editor::update does, in
    // the same frame the panel hides).
    void setModalWindow(StringView title, bool modal);
    void maximizeAllWindows();
    void minimizeAllWindows();
    void restoreAllWindows();
    void tileAllWindows();
    void cascadeWindows();

    void pushId(uint64_t id);
    void pushId(StringView id);
    void popId();

    void setTheme(const Theme &theme);
    // ── Style vars ───────────────────────────────────────────────────────────
    // Muda um campo do tema dentro de um bloco, sem tocar no tema global. O campo é
    // um ponteiro para membro, por isso vale para qualquer um deles (cor ou medida)
    // sem uma lista a manter:
    //
    //     ui.pushStyleColor(&Theme::buttonBackground, ig::Color(200, 60, 60));
    //     ui.pushStyleVar(&Theme::borderRadius, 0.0f);
    //     ui.button("Perigo");
    //     ui.popStyleVar();
    //     ui.popStyleColor();
    //
    // Um push sem pop vale só para o frame dele: no fim do frame o que ficou na
    // pilha é reposto (não se cola ao frame seguinte). A pilha é de 16 níveis; a
    // partir daí o push é ignorado. O setTheme() esvazia-a.
    void pushStyleColor(Color Theme::*field, const Color &color);
    void popStyleColor(int count = 1);
    void pushStyleVar(float Theme::*field, float value);
    void popStyleVar(int count = 1);
    // ── Fontes ───────────────────────────────────────────────────────────────
    // Escolhe a face dos textos seguintes, dentro de um bloco. A face traz o seu
    // próprio tamanho de fabrico, por isso o fontSize em vigor passa a ser o dela
    // (um texto desenhado a esse tamanho sai a 1:1, sem reescalonamento):
    //
    //     ui.pushFont(boldFont);
    //     ui.label("Título");
    //     ui.popFont();
    //
    // Ao contrário das style vars, a face é uma escolha e não uma medida: um
    // pushStyleVar(&Theme::fontSize, x) por dentro continua a mandar no tamanho.
    // A pilha é de 8 níveis (a partir daí o push é ignorado); um push sem pop vale
    // só para o frame dele. Um id que não é de face nenhuma não desenha texto até
    // ao pop, mas o push é feito na mesma, para o pop tirar o que lá está.
    void pushFont(FontId font);
    void popFont(int count = 1);
    const Theme &theme() const;
    // O tema em vigor, para ser editado (é o GetStyle() do ImGui). O que o editor
    // escrever vale para o que for submetido a seguir neste frame - e como o tema é
    // também o "valor de antes" dos pushStyleVar/pushStyleColor, um campo que esteja
    // debaixo de um push volta ao valor do push no pop. Quem edita o tema desenha-se
    // por isso depois do que ele mostra (a galeria submete a janela do Style Editor
    // depois da janela do demo, como o demo do ImGui: um campo mexido aparece no
    // frame seguinte, e o que se vê a ser mexido é o valor antigo).
    Theme &theme();
    // Turns the whole interface off: beginWindow() and messageBox() show
    // nothing, pushEvent() drops its events, and the widget state that was
    // under the pointer is released. Windows and their positions are kept, so
    // enabling it again brings the interface back as it was. It is on by default.
    void setEnabled(bool enabled);
    bool enabled() const;
    bool wantsPointer() const;
    bool wantsKeyboard() const;
    bool wantsTextInput() const;
    // True while a menu bar dropdown or context menu (beginMenu/
    // beginContextMenu) is open, from the frame it opens through the frame
    // it closes (inclusive - same snapshot pointerBlockedByOpenPopup and
    // shortcut()/isKeyPressed() already use). A caller with its own
    // pointer-reactive UI that sits near a menu bar - a button strip drawn
    // right under it, say - can use this to skip its own hover/click
    // handling outright instead of relying only on the guard already built
    // into itemHovered/itemClicked.
    bool isMenuOpen() const;
    // Widgets submitted between beginDisabled() and endDisabled() do not react to
    // the pointer or the keyboard, and are drawn faded: colours are blended toward
    // the window background, text and geometry alike. Blocks nest, and one left
    // open ends with the frame (see beginFrame). The fade does not deepen with
    // nesting - two levels are as faded as one, so a caption already dimmed by
    // textDisabled() does not disappear inside a block.
    void beginDisabled();
    void endDisabled();
    bool isDisabled() const { return disabledDepth_ > 0; }

    // Application-owned history entries. The callbacks must remain valid for
    // as long as the entry stays in this Context's history.
    void pushUndo(StringView label, ct::Function<void()> undo, ct::Function<void()> redo);
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;
    void clearUndoHistory();

    bool button(StringView label, const Rect &bounds);
    // Compact text action for inspectors and toolbars.
    bool smallButton(StringView label, const Rect &bounds);
    // A square button with an arrow in it (the ArrowButton of ImGui). The label is
    // an id: use "##left" when there is no text to show.
    bool arrowButton(StringView label, ArrowDirection direction, const Rect &bounds);
    bool checkbox(StringView label, bool &value, const Rect &bounds);
    // The CheckboxFlags of ImGui: a checkbox that toggles the bits in flagValue
    // instead of a bool of its own. Returns true when they changed.
    bool checkboxFlags(StringView label, unsigned &flags, unsigned flagValue,
                       const Rect &bounds);
    bool toggleSwitch(StringView label, bool &value, const Rect &bounds);
    bool radioButton(StringView label, bool selected, const Rect &bounds);
    bool selectable(StringView label, bool selected, const Rect &bounds);
    // Returns whether the section is expanded. The bool stores its persistent state.
    bool collapsingHeader(StringView label, bool &expanded, const Rect &bounds);
    // O mesmo, com um botão de fecho no canto: clicar nele põe `open` a false, e a
    // partir daí o header não é desenhado (como o p_open do ImGui).
    bool collapsingHeader(StringView label, bool &expanded, bool &open, const Rect &bounds);
    // The same with a bullet instead of the arrow, or drawn as chosen: see TreeStyle.
    bool collapsingHeader(StringView label, bool &expanded, const TreeStyle &style,
                          const Rect &bounds);
    // A compact expandable row for hierarchies. Use indent()/unindent() for its children.
    bool treeNode(StringView label, bool &expanded, const Rect &bounds);
    // The TreeStyle version: a bullet (the demo's bullet nodes), a row drawn as
    // chosen, only the arrow opening it, or a row that fills the width.
    bool treeNode(StringView label, bool &expanded, const TreeStyle &style, const Rect &bounds);
    // Scene-oriented tree row. Returns true when the row is selected; expansion
    // is changed only by pressing its disclosure arrow.
    bool treeItem(StringView label, bool &expanded, const TreeItemStyle &style,
                  const Rect &bounds);
    // Draggable scene-tree item. nodeId must be stable in the application's tree
    // model. Drop over the upper, middle or lower area for Before, Inside or After.
    // The source item cannot be its own target.
    bool treeItem(WidgetId nodeId, StringView label, bool &expanded, const TreeItemStyle &style,
                  const Rect &bounds, TreeDrop *drop = nullptr);
    // Renders a tab strip and returns true when the selected tab changes.
    bool tabBar(StringView label, int &currentItem, Span<const StringView> items,
                const Rect &bounds);
    // ── Tabs a sério (o BeginTabBar/BeginTabItem do ImGui) ───────────────────
    // A barra é um contentor: cada beginTabItem desenha a tab e, só a escolhida,
    // abre o conteúdo dela (que vai até ao endTabItem). O que está escolhido é do
    // id da barra e sobrevive ao frame; a tab de um documento que deixe de ser
    // submetida desaparece com ele.
    //
    //     if (ui.beginTabBar("bar")) {
    //         if (ui.beginTabItem("Skeleton")) {
    //             ui.label("conteudo do esqueleto");
    //             ui.endTabItem();
    //         }
    //         if (ui.beginTabItem("Aparencia", &aberta)) {
    //             ui.label("conteudo da aparencia");
    //             ui.endTabItem();
    //         }
    //         ui.endTabBar();
    //     }
    struct TabBarFlags
    {
        enum Values
        {
            None = 0u,
            /// Arrastar uma tab para cima de outra troca-lhes a ordem (o
            /// Reorderable): a ordem fica na barra, pelo id dela.
            Reorderable = 1u << 0,
            /// Uma tab submetida pela primeira vez fica escolhida (o
            /// AutoSelectNewTabs) - é o que faz um documento novo abrir à frente.
            AutoSelectNewTabs = 1u << 1,
            /// As tabs encolhem para caber todas na largura da barra (o Fitting).
            Fitting = 1u << 2,
            /// Sem o fecho com o botão do meio (o NoCloseWithMiddleMouseButton).
            NoCloseWithMiddleMouseButton = 1u << 3,
            /// Um botão em flecha encostado à esquerda da barra que abre a lista
            /// das tabs (o TabListPopupButton): a lista escolhe a tab, como um
            /// clique nela faria.
            TabListPopupButton = 1u << 4,
            /// A tab escolhida sai com um traço por cima (o DrawSelectedOverline).
            DrawSelectedOverline = 1u << 5
        };
    };
    struct TabItemFlags
    {
        enum Values
        {
            None = 0u,
            /// Fica escolhida neste frame (o SetSelected), que é o que um "abrir
            /// documento" pede.
            SetSelected = 1u << 0,
            /// Sai com um ponto a seguir ao nome (o UnsavedDocument do demo).
            UnsavedDocument = 1u << 1,
            /// A tab fica na ponta esquerda da barra, à frente das outras (o
            /// Leading): é onde o "+" e os menus das barras do demo ficam.
            Leading = 1u << 2,
            /// A tab fica na ponta direita (o Trailing).
            Trailing = 1u << 3
        };
    };
    /// @brief Starts a tab bar at the layout cursor, as wide as the available width.
    ///        Returns false when there is nothing to submit it into.
    bool beginTabBar(StringView id, unsigned flags = TabBarFlags::None);
    void endTabBar();
    /// @brief Draws the tab and, when it is the selected one, opens its content: the
    ///        caller submits the content between this and endTabItem(). With @p open
    ///        the tab carries an X that closes it (setting *open to false), and the
    ///        middle mouse button closes it too unless the bar says otherwise.
    bool beginTabItem(StringView label, bool *open = nullptr,
                      unsigned flags = TabItemFlags::None);
    void endTabItem();
    /// @brief A tab que é um botão (o TabItemButton do ImGui): ocupa o lugar de uma
    ///        tab na barra, com o aspecto de um botão, e devolve true quando é
    ///        carregada. Não escolhe nada nem abre conteúdo - é o "+" que acrescenta
    ///        uma tab, o "?" que abre um menu, ou o que se quiser, e quem trata do
    ///        clique é o chamador. Leva as bandas (Leading/Trailing) como uma tab.
    bool tabItemButton(StringView label, unsigned flags = TabItemFlags::None);
    // A scrollable list. The label is an identifier; render a separate label when needed.
    bool listBox(StringView label, int &currentItem, Span<const StringView> items,
                 const Rect &bounds);
    bool comboBox(StringView label, int &currentItem, Span<const StringView> items,
                  const Rect &bounds);
    // ── Escolha múltipla ─────────────────────────────────────────────────────
    // O BeginMultiSelect do ImGui sem o modelo de seleção dele: quem sabe o que
    // está escolhido é a aplicação (um bool por item, o span), e o bloco só lhe
    // aplica o que o rato e as teclas pediram. Dentro do bloco cada selectable é um
    // item da escolha, pela ordem em que é submetido: um clique escolhe o item e
    // desmarca os outros, o Ctrl liga/desliga o item sem mexer nos outros, o Shift
    // escolhe o alcance desde o último clique, o Ctrl+A escolhe tudo e o Escape
    // limpa (com os flags a permitirem). O realce de cada item passa a ser o da
    // seleção, por isso o clique vê-se no mesmo frame.
    struct MultiSelectFlags
    {
        enum Values
        {
            None = 0u,
            /// Um item de cada vez: o Ctrl e o Shift não acrescentam nada.
            SingleSelect = 1u << 0,
            /// O Ctrl+A não mexe na seleção.
            NoSelectAll = 1u << 1,
            /// O Shift não escolhe o alcance: o clique é sempre um item só.
            NoRangeSelect = 1u << 2,
            /// O Escape limpa a seleção.
            ClearOnEscape = 1u << 3
        };
    };
    // Devolve true enquanto o bloco está aberto (para um bloco dentro do outro
    // fechar o de fora, como no ImGui). O span tem de viver até ao endMultiSelect.
    bool beginMultiSelect(Span<bool> selected, unsigned flags = MultiSelectFlags::None);
    void endMultiSelect();
    /// @brief O último item submetido no bloco está escolhido?
    bool isItemSelected() const;
    /// @brief A escolha do último item submetido no bloco mudou neste frame?
    bool isItemToggledSelection() const;
    // Cabeçalho de uma combo cujo popup leva o conteúdo que o chamador submeter
    // entre beginCombo e endCombo: o BeginCombo/EndCombo do ImGui. `preview` é o
    // texto do cabeçalho. Devolve true enquanto o popup está aberto e o conteúdo
    // vai dentro do if: o cursor do layout fica dentro do popup e availableWidth()
    // passa a ser a largura dele, por isso os itens submetem-se como em qualquer
    // sítio (selectable, button, slider). O popup tem a largura do cabeçalho e no
    // máximo visibleItems linhas, como o HeightRegular do ImGui (8); o que não
    // couber é cortado (não há scroll).
    bool beginCombo(StringView label, StringView preview, const Rect &bounds,
                    int visibleItems = 8);
    void endCombo();
    // O SetItemDefaultFocus do ImGui: o último item submetido é o que deve ficar à
    // vista quando a lista de um combo abre. É o que um combo faz com o item
    // escolhido (o demo: `if (is_selected) ImGui::SetItemDefaultFocus();`), senão
    // abria sempre no topo e a escolha podia estar fora de vista. Só se aplica
    // quando a lista está aberta e tem o que rolar; rolar à mão desiste dele.
    void setItemDefaultFocus();
    // A horizontal menu strip. Call beginMenu()/endMenu() between these calls.
    bool beginMenuBar(const Rect &bounds);
    void endMenuBar();
    // Opens a popup menu from the current menu bar.
    bool beginMenu(StringView label);
    void endMenu();
    bool menuItem(StringView label, bool enabled = true);
    // Um item com o atalho desenhado à direita, como o MenuItem(label, shortcut)
    // do ImGui ("Undo", "Ctrl+Z"). O atalho é texto: quem o trata é a aplicação.
    // selected desenha a marca de escolhido, para os itens que fazem de caixa.
    bool menuItem(StringView label, StringView shortcut, bool enabled = true,
                  bool selected = false);
    // Checkable menu entry. Returns true when its value changed.
    bool menuCheckbox(StringView label, bool &checked, bool enabled = true);
    // One-level submenu, opened by hover or click. End it before ending its parent menu.
    bool beginSubMenu(StringView label, bool enabled = true);
    void endSubMenu();
    void menuSeparator();
    // Opens a popup at the pointer on a right click over bounds. Use menuItem()
    // and menuSeparator(), then close it with endContextMenu().
    bool beginContextMenu(StringView id, const Rect &bounds);
    void endContextMenu();
    // ── Popups ───────────────────────────────────────────────────────────────
    // Um popup de conteúdo livre, preso a um id (o OpenPopup/BeginPopup do ImGui):
    // abre-se num sítio e o corpo é submetido onde o chamador quiser. Devolve true
    // enquanto estiver aberto e o conteúdo vai dentro do if.
    //
    //     if (ui.button("Select.."))
    //         ui.openPopup("escolha");          // abre no ponteiro
    //     if (ui.beginPopup("escolha")) {
    //         ui.separatorText("Aquario");
    //         for (int i = 0; i < count; ++i)
    //             if (ui.selectable(names[i], chosen == i)) {
    //                 chosen = i;
    //                 ui.closeCurrentPopup();
    //             }
    //         ui.endPopup();
    //     }
    //
    // O id é global (não depende do sítio onde é aberto). O corpo é um contentor de
    // layout como o de um menu: um slider ou um child lá dentro ficam no popup, e o
    // popup cresce com eles no frame seguinte. Um clique fora (ou o Escape) fecha-o.
    //
    // O par tem de ser submetido todos os frames, como tudo no modo imediato: quem
    // parar de o submeter deixa de o ter (ver releaseUnsubmittedBlockers).
    void openPopup(StringView id);
    /// @brief Abre no @p position (absoluto) em vez de no ponteiro.
    void openPopup(StringView id, const Vec2 &position);
    /// @brief Abre com um tamanho pedido (@p size.x é a largura da caixa, 0 = a do
    ///        tema; @p size.y é a altura mínima, 0 = a que o conteúdo pedir).
    ///        A largura é sempre pedida e nunca medida: os itens de um popup
    ///        esticam-se à largura da caixa (é o que o ImGui faz com um Selectable),
    ///        por isso medi-la faria a caixa crescer a cada frame.
    void openPopup(StringView id, const Vec2 &position, const Vec2 &size);
    // Flags do openPopup/isPopupOpen.
    struct PopupFlags
    {
        enum Values
        {
            None = 0u,
            /// Abre no cursor do layout (encostado ao item que lá está, que é o que
            /// um menu de contexto quer) em vez de no ponteiro.
            AtCursor = 1u << 0,
            /// Não abre se já houver um popup aberto (o NoOpenOverExistingPopup do
            /// ImGui): é o que impede um menu de contexto de se abrir por cima do
            /// que já lá está.
            NoOpenOverExistingPopup = 1u << 1,
            /// Só no isPopupOpen: pergunta "há algum popup aberto?" em vez de "é
            /// este que está aberto?".
            AnyPopupId = 1u << 2
        };
    };
    void openPopup(StringView id, unsigned flags);
    bool beginPopup(StringView id);
    void endPopup();
    void closeCurrentPopup();
    bool isPopupOpen(StringView id) const;
    bool isPopupOpen(StringView id, unsigned flags) const;
    /// @brief Opens the modal with its own title bar, the BeginPopupModal of ImGui:
    ///        the caller opens it with openPopup(title) like any other popup and
    ///        then calls this where the box goes. The difference from beginPopup is
    ///        that the box is centred on the screen, is drawn above every window
    ///        (the rest of the interface is behind a dim, and neither a click
    ///        outside nor a shortcut reaches it), carries a title bar with the
    ///        title in it, and does not close on a click outside: it closes with
    ///        Escape, with closeCurrentPopup, or through @p open when the caller
    ///        wants an X in the title bar.
    ///
    ///            ui.openPopup("Settings");
    ///            if (ui.beginPopupModal("Settings", &open)) {
    ///                ui.label("Volume");
    ///                ui.sliderFloat("vol", volume, 0.0f, 1.0f);
    ///                if (ui.button("OK")) ui.closeCurrentPopup();
    ///                ui.endPopup();
    ///            }
    bool beginPopupModal(StringView title, bool *open = nullptr);
    // Flags de slider/drag. Vão depois do width (o width é 0 para "a largura que
    // sobra", como em tudo neste ficheiro): um slider logarítmico é
    // sliderFloat("Log", v, 0.001f, 100.0f, 0.0f, SliderFlags::Logarithmic).
    struct SliderFlags
    {
        enum Values
        {
            None = 0u,
            /// O valor anda na escala do logaritmo do intervalo (o "Logarithmic"
            /// do demo): a metade da pista é a média geométrica dos extremos, por
            /// isso os primeiros passos valem muito mais. Precisa de minimum > 0.
            Logarithmic = 1u << 0,
            /// Ao chegar a um fim, o valor passa para o outro. Só no arrasto.
            WrapAround = 1u << 1,
            /// Sem o editor de texto ao lado: o slider é só da pista.
            NoInput = 1u << 2,
            /// O número escrito à mão pode sair do intervalo (por omissão é
            /// limitado a ele).
            AllowOutOfRange = 1u << 3
        };
    };
    bool sliderFloat(StringView label, float &value, float minimum, float maximum,
                     const Rect &bounds, unsigned flags = SliderFlags::None,
                     const char *format = nullptr);
    bool sliderInt(StringView label, int &value, int minimum, int maximum,
                   const Rect &bounds, unsigned flags = SliderFlags::None,
                   const char *format = nullptr);
    // O SliderAngle do ImGui: o valor é em radianos, o intervalo e o texto em
    // graus ("%.0f deg" por omissão).
    bool sliderAngle(StringView label, float &radians, float degreesMinimum,
                     float degreesMaximum, const Rect &bounds,
                     unsigned flags = SliderFlags::None, const char *format = nullptr);
    // A mixer fader: a recessed groove with a cap that travels along it. The
    // whole bounds take the press and the cap follows the pointer from there,
    // so it is pushed with a finger rather than grabbed by its handle; a press
    // that lands on the cap keeps the gap between the two instead of jumping.
    // Returns true when value changed.
    bool fader(StringView id, float &value, float minimum, float maximum, const Rect &bounds,
               FaderOrientation orientation = FaderOrientation::Vertical,
               const FaderStyle &style = FaderStyle());
    // Horizontal relative drag. The value changes by speed for each moved pixel;
    // click its value field to enter a precise number.
    bool dragFloat(StringView label, float &value, float minimum, float maximum,
                   float speed, const Rect &bounds, unsigned flags = SliderFlags::None,
                   const char *format = nullptr);
    bool dragInt(StringView label, int &value, int minimum, int maximum, int speed,
                 const Rect &bounds, unsigned flags = SliderFlags::None,
                 const char *format = nullptr);
    // Slider vertical, o VSliderFloat/VSliderInt do ImGui: o valor cresce para
    // cima numa faixa estreita, ao contrário do fader, que é um controle de
    // mistura.
    bool vSliderFloat(StringView label, float &value, float minimum, float maximum,
                      const Rect &bounds);
    bool vSliderInt(StringView label, int &value, int minimum, int maximum, const Rect &bounds);
    // Draggable divider. value is the local x/y coordinate inside bounds.
    bool splitter(StringView id, float &value, float minimum, float maximum,
                  SplitterAxis axis, const Rect &bounds, float thickness = 5.0f);
    // Transparent editor overlay for a 2D transform inside bounds.
    bool gizmo2D(StringView id, Transform2D &transform, Gizmo2DMode mode, const Rect &bounds,
                 const Gizmo2DOptions &options = Gizmo2DOptions());
    // Projected 3D transform handles. view and projection are column-major 4x4
    // matrices. The transform's rotation is euler degrees in X, then Y, then Z
    // order, and gizmo3DOptions::showModel draws the box it moves.
    bool gizmo3D(StringView id, Transform3D &transform, Gizmo3DMode mode, const Rect &bounds,
                 const float *view, const float *projection,
                 const Gizmo3DOptions &options = Gizmo3DOptions());
    bool stepperInt(StringView label, int &value, int minimum, int maximum,
                    const Rect &bounds);
    // O que a caixa de texto aceita e o que ela mostra (o ImGuiInputTextFlags do
    // ImGui, por outro nome). Os Chars* filtram o que se escreve e o que se cola -
    // o texto que a aplicação já lá tinha não se mexe -, o ReadOnly tranca a
    // edição e o Password esconde o que lá está.
    struct InputTextFlags
    {
        enum Values
        {
            None = 0u,
            /// Só os dígitos, o sinal e o ponto (um "-1.5").
            CharsDecimal = 1u << 0,
            /// Só os dígitos hexadecimais, mais o "x" do "0x".
            CharsHexadecimal = 1u << 1,
            /// Tudo o que entra sai em maiúsculas (o hexadecimal de sempre).
            CharsUppercase = 1u << 2,
            /// Sem espaços nem tabs.
            CharsNoBlank = 1u << 3,
            /// Os dígitos, o sinal, o ponto e o expoente (um "1.5e-3").
            CharsScientific = 1u << 4,
            /// O Tab escreve um tab dentro do campo em vez de passar o foco ao item
            /// seguinte.
            AllowTabInput = 1u << 5,
            /// O Enter devolve true (é o EnterReturnsTrue do ImGui): quem espera
            /// pelo que se escreveu responde ao Enter, e o campo fica com o foco.
            EnterReturnsTrue = 1u << 6,
            /// O que está escrito vê-se como pontos.
            Password = 1u << 7,
            /// Só se lê: o clique e o foco continuam a valer, o texto não se mexe.
            ReadOnly = 1u << 8
        };
    };
    // O tipo do valor de um *Scalar (o ImGuiDataType do ImGui): é o que permite ter
    // uma widget numérica só para os tipos que não são float nem int - um s8, um
    // u64, um double - com o valor passado por ponteiro e o formato escrito à mão.
    // O valor por dentro é tratado em double: um s64/u64 acima de 2^53 perde os
    // bits de baixo, como no ImGui (que usa o mesmo double como FLOATTYPE nestes
    // dois tipos).
    enum class ScalarType : uint8_t
    {
        S8,
        U8,
        S16,
        U16,
        S32,
        U32,
        S64,
        U64,
        Float,
        Double
    };
    // O SliderScalar/DragScalar/InputScalar do ImGui: o slider e o drag com o
    // intervalo por ponteiros do tipo do valor (no drag podem ser nulos, que é
    // "sem limites") e um formato do valor por tipo quando não se dá nenhum.
    bool sliderScalar(StringView label, ScalarType type, void *data, const void *minimum,
                      const void *maximum, const char *format = nullptr, float width = 0.0f,
                      unsigned flags = SliderFlags::None);
    bool dragScalar(StringView label, ScalarType type, void *data, float speed,
                    const void *minimum = nullptr, const void *maximum = nullptr,
                    const char *format = nullptr, float width = 0.0f,
                    unsigned flags = SliderFlags::None);
    // O campo de texto de um tipo qualquer. O passo (+/- ao lado do campo) ainda
    // não existe: ver o GAPS.
    bool inputScalar(StringView label, ScalarType type, void *data, const char *format = nullptr,
                     float width = 0.0f, unsigned flags = InputTextFlags::None);
    bool inputText(StringView label, String &value, const Rect &bounds,
                   unsigned flags = InputTextFlags::None);
    // Igual ao inputText, mas desenha `hint` enquanto o campo está vazio.
    bool inputTextWithHint(StringView label, StringView hint, String &value, const Rect &bounds,
                           unsigned flags = InputTextFlags::None);
    // followTail keeps the view pinned to the last line as value grows (a
    // console/log), unless the user scrolled up to read back.
    bool inputTextMultiline(StringView label, String &value, const Rect &bounds,
                            bool followTail = false);
    // Syntax-highlighted source editor: line buffer, (line, column) cursor
    // with selection, full undo/redo, and keyword/identifier autocomplete.
    // state is application-owned and persists across frames (see
    // CodeEditorState); use state.setText/setHighlighterForFile to load a
    // document and pick a language. Returns true the frame the buffer changed.
    bool codeEditor(StringView id, CodeEditorState &state, const Rect &bounds,
                    const CodeEditorOptions &options = CodeEditorOptions());
    // Programmatic equivalents of the Ctrl+C/X/V shortcuts codeEditor()
    // already handles internally - for a context menu's Copy/Cut/Paste
    // items, or any other caller that wants to trigger them without
    // synthesizing key events. Safe to call with no selection (Copy/Cut
    // become no-ops) or an empty clipboard (Paste becomes a no-op).
    // Each returns true if it changed the buffer (Cut/Paste; Copy never does).
    bool codeEditorCopy(CodeEditorState &state);
    bool codeEditorCut(CodeEditorState &state);
    bool codeEditorPaste(CodeEditorState &state);
    // O `format` é o do printf e vale para o valor desenhado E para o que se
    // escreve no campo: "%.10f grams" mostra e lê o número com a legenda, "%04X"
    // mostra e lê em hexadecimal. Sem ele, o inteiro sai como sempre saiu e o
    // real com a precisão pedida.
    bool inputInt(StringView label, int &value, const Rect &bounds,
                  const char *format = nullptr);
    bool inputFloat(StringView label, float &value, const Rect &bounds, int precision = 6,
                    const char *format = nullptr);
    bool inputDouble(StringView label, double &value, const Rect &bounds, int precision = 6,
                     const char *format = nullptr);
    // Inline RGBA editor with a preview swatch and four draggable channels.
    bool colorEdit(StringView label, Color &value, const Rect &bounds);
    // A colour swatch with the label next to it, for a widget that opens the
    // picker (colorEdit) somewhere else. Returns true when the swatch is clicked.
    bool colorButton(StringView label, const Color &value, const Rect &bounds);
    // Editable multi-stop gradient. Left-click an empty part of the bar to
    // add an interpolated stop, drag a handle to move it, and right-click a
    // non-endpoint handle to remove it. selectedStop is -1 when none is set.
    bool gradientEditor(StringView id, ct::Vector<GradientStop> &stops, int &selectedStop,
                        const Rect &bounds);
    // Linear curve editor. Click empty canvas space to add a point, drag a
    // point to edit it, and right-click a non-endpoint point to remove it.
    bool curveEditor(StringView id, ct::Vector<CurvePoint> &points, int &selectedPoint,
                     const Rect &bounds, const Vec2 &minimum = Vec2(),
                     const Vec2 &maximum = Vec2(1.0f, 1.0f));
    bool sequencer(StringView id, ct::Vector<SequencerTrack>& tracks, int firstFrame, int lastFrame,
                   int& currentFrame, int& selectedTrack, const Rect& bounds);
    // Click empty track space to add a key, drag diamonds, right-click to delete.
    bool timeline(StringView id, ct::Vector<TimelineTrack>& tracks, int firstFrame, int lastFrame,
                  int& currentFrame, int& selectedTrack, int& selectedKey, const Rect& bounds);
    // Draw an already-owned backend texture. TextureId remains backend-neutral.
    void image(TextureId texture, const Rect &bounds,
               const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
               const Color &tint = Color());
    // The label is an identifier only; the texture is the button's visual.
    bool imageButton(StringView label, TextureId texture, const Rect &bounds,
                     const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
                     const Color &tint = Color());
    // Compact icon-only button. label is an identifier, not rendered text.
    bool smallImageButton(StringView label, TextureId texture, const Rect &bounds,
                          const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
                          const Color &tint = Color());
    // ── Rótulos ─────────────────────────────────────────────────────────────
    // A convenção do ImGui vale em todos os widgets que levam rótulo: o que vem
    // depois de "##" não se desenha mas faz parte do id, e "##so-id" não desenha
    // nada. Serve para dois widgets partilharem o texto visível sem partilharem o
    // estado, e para um campo não gastar largura com um rótulo:
    //
    //     ui.button("Gravar##notas");      // desenha "Gravar"
    //     ui.dragFloat("##x", x, 0.0f, 1.0f);  // só a caixa, sem rótulo
    //
    // Sem "##" o rótulo sai inteiro. O `label()`/`text()` não trata disto, como
    // o `Text()` do ImGui também não: ali a string é texto, não identificação.
    // ── Largura dos itens ───────────────────────────────────────────────────
    // Como no ImGui: uma barra fixa por item. setNextItemWidth vale para o item
    // seguinte (os widgets de layout das secções seguintes, que são os que tomam
    // uma largura), pushItemWidth/popItemWidth para um bloco deles, e
    // calcItemWidth diz qual vai ser a largura do próximo item sem o submeter.
    // Zero devolve o valor por omissão: a largura que sobra na linha.
    void setNextItemWidth(float width);
    void pushItemWidth(float width);
    void popItemWidth();
    float calcItemWidth() const;
    // Minimal building blocks for application-defined immediate widgets.
    bool invisibleButton(StringView id, const Rect &bounds);
    bool isHovered(StringView id, const Rect &bounds);
    // Equivalent to invisibleButton, named for custom widget interaction code.
    bool isClicked(StringView id, const Rect &bounds);
    // ── O que se passou com o último item (o LastItemData do ImGui) ──────────
    // Logo a seguir a submeter um widget, estas dizem o que o ponteiro e o
    // teclado fizeram com ele. Valem para o último item que passou por um teste
    // de ponteiro - todos os widgets, incluindo os de bounds -, por isso têm de
    // ser lidas no mesmo frame e antes do item seguinte:
    //
    //     ui.button("Grava");
    //     if (ui.isItemHovered()) ui.tooltip("grava o ficheiro");
    //
    // isItemEdited é posto pelos widgets que mudam um valor (sliders, drags,
    // campos, checkbox, radio, combo, listas, colorEdit, stepper); um widget que
    // não o ponha continua a dizê-lo pelo seu próprio valor de retorno.
    // Flags do isItemHovered (o IsItemHovered do ImGui). O item decide a resposta
    // quando é submetido, como no ImGui: um flag que ninguém pediu na altura do
    // widget não pode ser respondido depois.
    struct ItemHoverFlags
    {
        enum Values
        {
            None = 0u,
            /// Só passado o hoverDelay_ (0.45 s por omissão) em cima do item.
            Delay = 1u << 0,
            /// Só se o ponteiro não se mexeu desde que chegou ao item.
            Stationary = 1u << 1,
            /// Responde mesmo com outro widget ativo (um arrasto a passar por cima).
            AllowWhenBlockedByActiveItem = 1u << 2,
            /// Responde mesmo com um menu/combo aberto por cima.
            AllowWhenBlockedByPopup = 1u << 3,
            /// Basta o rect: ignora o clip (um item a sair de um child conta como
            /// "debaixo do ponteiro" mesmo na parte que o child corta).
            RectOnly = 1u << 4
        };
    };
    bool isItemHovered() const { return item_.hovered; }
    bool isItemHovered(unsigned flags) const
    {
        // Sem flags não há nada a responder além do que o item já disse.
        if (flags == 0u)
            return item_.hovered;
        // Cada flag tem o seu bit no que o item mediu (ver ItemStatus).
        const bool wantsDelay = (flags & ItemHoverFlags::Delay) != 0u;
        const bool wantsStationary = (flags & ItemHoverFlags::Stationary) != 0u;
        const bool wantsActive = (flags & ItemHoverFlags::AllowWhenBlockedByActiveItem) != 0u;
        const bool wantsPopup = (flags & ItemHoverFlags::AllowWhenBlockedByPopup) != 0u;
        const bool wantsRectOnly = (flags & ItemHoverFlags::RectOnly) != 0u;
        if (wantsDelay && !item_.hoveredDelayed)
            return false;
        if (wantsStationary && !item_.hoveredStationary)
            return false;
        if (!item_.hovered)
        {
            // Uma condição bloqueada pode ser dispensada por um flag: é para isso
            // que existem.
            const bool okActive = !item_.hoveredBlockedByActive || wantsActive;
            const bool okPopup = !item_.hoveredBlockedByPopup || wantsPopup;
            const bool okRect = item_.hoveredInRect && (wantsRectOnly || item_.hoveredInClip);
            if (!(okActive && okPopup && okRect))
                return false;
            // Sem clip e sem bloqueio, o que sobra é o rect: Delay/Stationary já
            // foram testados acima.
        }
        return true;
    }
    /// @brief The window being submitted is the focused one (the one the keyboard
    ///        and the pointer go to, see focusWindow). What a window-scoped
    ///        shortcut wants: a Ctrl+S inside the focused panel, not in the
    ///        background one.
    bool isWindowFocused() const;
    /// @brief The pointer is over the window being submitted and nothing is above
    ///        it there (another window, a modal, a popup). A panel that follows the
    ///        pointer only while the pointer is over it asks this.
    bool isWindowHovered() const;
    /// @brief The window being submitted was not on screen in the previous frame:
    ///        it was created or reopened now.
    bool isWindowAppearing() const;
    /// @brief The given button went down for the second time inside the double
    ///        click interval and distance (see setDoubleClickInterval): true for
    ///        the one frame the second press is in. A custom widget that renames on
    ///        a double click asks this, and an item uses isItemDoubleClicked below.
    bool isPointerDoubleClicked(PointerButton button = PointerButton::Left) const;
    /// @brief The last item was double clicked (the same second press, seen from
    ///        the item): the PressedOnDoubleClick of a selectable that a file list
    ///        renames with.
    bool isItemDoubleClicked() const { return item_.doubleClicked; }
    // Segundos e pixels entre duas carregadas para serem um duplo clique (por
    // omissão 0.30s e 6px, como as definições de IO do ImGui).
    void setDoubleClickInterval(float seconds);
    float doubleClickInterval() const { return doubleClickInterval_; }
    void setDoubleClickDistance(float pixels);
    float doubleClickDistance() const { return doubleClickDistance_; }
    // Segundos que o ponteiro tem de estar parado sobre o item para o flag Delay
    // (o style.HoverDelay do ImGui; o tooltip tem o seu, este é para consultas).
    void setHoverDelay(float seconds);
    float hoverDelay() const { return hoverDelay_; }
    bool isItemActive() const { return item_.active; }
    bool isItemClicked() const { return item_.clicked; }
    bool isItemEdited() const { return item_.edited; }
    bool isItemFocused() const { return item_.focused; }
    bool isItemDisabled() const { return item_.disabled; }
    // O item ficou ativo neste frame (a carregada que o começou) e deixou de o
    // estar neste frame (a largada que o acabou). O deactivatedAfterEdit diz que
    // ele foi editado enquanto esteve ativo - é o que um editor usa para gravar.
    bool isItemActivated() const { return item_.activated; }
    bool isItemDeactivated() const { return item_.deactivated; }
    bool isItemDeactivatedAfterEdit() const { return item_.deactivatedAfterEdit; }
    // Um treeNode/collapsingHeader abriu ou fechou neste frame.
    bool isItemToggledOpen() const { return item_.toggledOpen; }
    // O item ainda cai dentro do que se vê: um item rolado para fora de um child
    // não é visível.
    bool isItemVisible() const
    {
        const Rect visible = intersect(item_.bounds, contentClip());
        return visible.width > 0.0f && visible.height > 0.0f;
    }
    // Onde o último item ficou, em coordenadas absolutas (o mesmo espaço dos
    // draw* e do windowBounds), e o id com que foi submetido. Como no ImGui, um
    // label ou um dummy não identifica item nenhum: ficam com o id do último
    // widget que se identificou.
    Rect itemRect() const { return item_.bounds; }
    WidgetId itemId() const { return item_.id; }
    void drawRectFilled(const Rect &bounds, const Color &color);
    void drawRect(const Rect &bounds, const Color &color, float thickness = 1.0f);
    void drawLine(const Vec2 &from, const Vec2 &to, const Color &color, float thickness = 1.0f);
    void drawCircleFilled(const Vec2 &center, float radius, const Color &color);
    // A filled simple polygon (convex or not), points in the same content space as the other draw* calls.
    void drawPolygonFilled(Span<const Vec2> points, const Color &color);
    // Rounded rectangles, filled and outline. A large radius on a square gives
    // a circle, so one call covers pills, cards and step-sequencer cells.
    void drawRectFilledRounded(const Rect &bounds, float radius, const Color &color);
    void drawRectRounded(const Rect &bounds, float radius, const Color &color,
                         float thickness = 1.0f);
    // Arc as one continuous band. Prefer this over stroking an arc with
    // drawLine per segment: every joint there overlaps its neighbour's soft
    // edge and the alpha adds up, which makes a ring look notched.
    void drawArc(const Vec2 &center, float radius, float fromRadians,
                 float toRadians, const Color &color, float thickness = 1.0f);
    // Text in an explicit colour, and the width the theme font would need for
    // it. A custom widget that centres its own caption needs both: label()
    // always uses the theme's label colour and reports no width.
    void drawText(StringView text, const Vec2 &position, const Color &color);
    float textWidth(StringView text) const;
    bool isKeyPressed(KeyCode key) const;
    bool shortcut(KeyCode key, bool control = true, bool shift = false) const;
    // True for every frame the given button is held down, from the press
    // through the frame it's released (inclusive) - not just the one frame
    // it went down. For a caller that needs to tell "the user is actively
    // dragging something" from "a plain click landed", such as freezing a
    // side panel's cursor-follow highlight while a drag-selection is live
    // in the code editor next to it.
    bool isPointerButtonDown(PointerButton button) const;
    // Pointer position in display coordinates, and where the given button went
    // down. Together with isPointerButtonDown they let an application-defined
    // widget implement its own drag (a rotary knob, a custom scrubber) without
    // reaching into the Context - the value-drag equivalent of the drawRect/
    // drawLine building blocks above.
    // Absolute (window) coordinates.
    Vec2 pointerPosition() const;
    Vec2 pointerPressedPosition(PointerButton button) const;
    // The same points in the current content space - the one the Rects passed
    // to the draw* primitives and the bounds-based widgets use. Inside a
    // beginChild() or a scrolled window the two differ by the scroll offset, so
    // custom widgets that draw with draw* and hit-test with the pointer must
    // use these to stay aligned.
    Vec2 pointerContentPosition() const;
    Vec2 pointerPressedContentPosition(PointerButton button) const;
    // A barra preenche-se até value/maximum. Com value negativo é indeterminada:
    // uma faixa atravessa-a com o tempo do contexto. O overlay, quando dado, sai
    // centrado por cima da barra.
    void progressBar(float value, float maximum, const Rect &bounds,
                     StringView overlay = StringView());
    // Grafico de uma serie. values aponta para count amostras lidas em anel a
    // partir de offset, por isso um buffer circular entrega sempre a mais antiga
    // e o desenho sai na ordem certa. Com scaleMax <= scaleMin a escala vertical
    // vem dos dados (amostras NaN sao ignoradas). A serie e reduzida a uma
    // coluna por pixel quando tem mais amostras do que isso. Devolve o indice
    // logico da amostra sob o ponteiro (0 = a primeira desenhada), ou -1.
    int plotLines(StringView label, const float *values, int count, const Rect &bounds,
                  int offset = 0, StringView overlay = StringView(),
                  float scaleMin = 0.0f, float scaleMax = 0.0f);
    // As barras nascem na linha do zero (no fundo quando a escala nao e negativa).
    int plotHistogram(StringView label, const float *values, int count, const Rect &bounds,
                      int offset = 0, StringView overlay = StringView(),
                      float scaleMin = 0.0f, float scaleMax = 0.0f);
    // Draws an application-modal OK or OK/Cancel message box above all windows.
    MessageBoxResult messageBox(StringView title, StringView message, bool &open,
                                bool showCancel = false);
    MessageBoxResult messageBox(StringView title, StringView message, bool &open,
                                const MessageBoxOptions &options);
    // Modal dialog with arbitrary content: centred, scrim behind it, Escape
    // closes it, and every other window is blocked while it is open. Call
    // endDialog() when it returns true.
    //
    //     if (ui.beginDialog("Settings", open, ig::Vec2(380.0f, 280.0f))) {
    //         ui.label("Volume");
    //         ui.sliderFloat("vol", volume, 0.0f, 1.0f);
    //         if (ui.button("Close")) open = false;
    //         ui.endDialog();
    //     }
    bool beginDialog(StringView title, bool &open, const Vec2 &size);
    void endDialog();
    // Application-modal filesystem browser. Its provider performs OS or
    // sandbox access; this Context owns only interaction and rendering.
    FileDialogResult fileDialog(StringView id, bool &open, FileDialogState &state,
                                const FileDialogOptions &options, FileDialogProvider &provider);
    // Starts or refreshes a short notification. The id differentiates concurrent toasts.
    void showToast(StringView id, StringView text, ToastPosition position = ToastPosition::BottomRight,
                   float duration = 3.0f);
    // Call after drawing a local item. The item becomes draggable after a short mouse move.
    bool beginDragSource(WidgetId source, WidgetId type, uint64_t data, StringView preview,
                         const Rect &bounds);
    // Call over a local drop zone. On release, returns the compatible payload.
    bool acceptDragDropTarget(WidgetId acceptedType, const Rect &bounds, DragDropPayload &payload);
    void label(StringView text, const Vec2 &position);
    // Call immediately after the widget that owns this help text.
    void tooltip(StringView text);
    // A tooltip the application fills with widgets, for anything the single line
    // above cannot say: a colour swatch, a table, a key/value list. Call it right
    // after the owning widget and close it with endTooltip():
    //
    //     ui.sliderFloat("Ganho", gain, 0.0f, 2.0f);
    //     if (ui.beginTooltip()) {
    //         ui.label("Ganho em dB");
    //         ui.sliderFloat("dB", db, -60.0f, 6.0f);
    //         ui.endTooltip();
    //     }
    //
    // It appears after the same stationary hover delay as tooltip(), next to the
    // pointer and flipping away from the window edges. Nothing inside reacts to
    // the pointer - a tooltip that took the hover would take it from the widget
    // it belongs to, and the two would flicker on and off - so a tooltip is for
    // reading, not for clicking.
    bool beginTooltip();
    void endTooltip();
    // Tooltips appear after this much stationary hover time. The default is 0.45 seconds.
    void setTooltipDelay(float seconds);
    float tooltipDelay() const;

    bool button(StringView label);
    bool smallButton(StringView label);
    bool arrowButton(StringView label, ArrowDirection direction, float size = 0.0f);
    bool checkbox(StringView label, bool &value);
    bool checkboxFlags(StringView label, unsigned &flags, unsigned flagValue);
    bool toggleSwitch(StringView label, bool &value);
    bool radioButton(StringView label, bool selected);
    bool selectable(StringView label, bool selected, float width = 0.0f);
    bool collapsingHeader(StringView label, bool &expanded, float width = 0.0f);
    bool collapsingHeader(StringView label, bool &expanded, const TreeStyle &style,
                          float width = 0.0f);
    bool collapsingHeader(StringView label, bool &expanded, bool &open, float width = 0.0f);
    bool treeNode(StringView label, bool &expanded, float width = 0.0f);
    bool treeNode(StringView label, bool &expanded, const TreeStyle &style, float width = 0.0f);
    bool treeItem(StringView label, bool &expanded, const TreeItemStyle &style,
                  float width = 0.0f);
    bool treeItem(WidgetId nodeId, StringView label, bool &expanded, const TreeItemStyle &style,
                  TreeDrop *drop = nullptr, float width = 0.0f);
    bool tabBar(StringView label, int &currentItem, Span<const StringView> items,
                float width = 0.0f);
    bool listBox(StringView label, int &currentItem, Span<const StringView> items,
                 float width = 0.0f, int visibleItems = 4);
    bool comboBox(StringView label, int &currentItem, Span<const StringView> items,
                  float width = 0.0f);
    // O beginCombo do layout: o cabeçalho é o item, o popup é do popup. O cursor
    // de fora só avança quando o endCombo repõe o layout.
    bool beginCombo(StringView label, StringView preview, float width = 0.0f,
                    int visibleItems = 8);
    bool sliderFloat(StringView label, float &value, float minimum, float maximum,
                     float width = 0.0f, unsigned flags = SliderFlags::None,
                     const char *format = nullptr);
    bool sliderInt(StringView label, int &value, int minimum, int maximum,
                   float width = 0.0f, unsigned flags = SliderFlags::None,
                   const char *format = nullptr);
    bool sliderAngle(StringView label, float &radians, float degreesMinimum = -360.0f,
                     float degreesMaximum = 360.0f, float width = 0.0f,
                     unsigned flags = SliderFlags::None, const char *format = nullptr);
    bool dragFloat(StringView label, float &value, float minimum, float maximum,
                   float speed = 0.01f, float width = 0.0f,
                   unsigned flags = SliderFlags::None, const char *format = nullptr);
    bool dragInt(StringView label, int &value, int minimum, int maximum, int speed = 1,
                 float width = 0.0f, unsigned flags = SliderFlags::None,
                 const char *format = nullptr);
    bool inputFloat2(StringView label, float &x, float &y, float width = 0.0f, int precision = 3,
                     const char *format = nullptr);
    bool inputFloat3(StringView label, float &x, float &y, float &z, float width = 0.0f,
                     int precision = 3, const char *format = nullptr);
    bool inputFloat4(StringView label, float &x, float &y, float &z, float &w,
                     float width = 0.0f, int precision = 3, const char *format = nullptr);
    // As variantes de N componentes são N widgets lado a lado, com uma largura
    // igual para todos: é o mesmo que o ImGui faz, e assim cada componente
    // mantém tudo o que o de um componente já faz (arrasto, edição por teclado,
    // clamps, foco).
    bool sliderFloat2(StringView label, float &x, float &y, float minimum, float maximum,
                      float width = 0.0f);
    bool sliderFloat3(StringView label, float &x, float &y, float &z, float minimum,
                      float maximum, float width = 0.0f);
    bool sliderFloat4(StringView label, float &x, float &y, float &z, float &w, float minimum,
                      float maximum, float width = 0.0f);
    bool sliderInt2(StringView label, int &x, int &y, int minimum, int maximum,
                    float width = 0.0f);
    bool sliderInt3(StringView label, int &x, int &y, int &z, int minimum, int maximum,
                    float width = 0.0f);
    bool sliderInt4(StringView label, int &x, int &y, int &z, int &w, int minimum, int maximum,
                    float width = 0.0f);
    bool dragInt2(StringView label, int &x, int &y, int minimum, int maximum, int speed = 1,
                  float width = 0.0f);
    bool dragInt3(StringView label, int &x, int &y, int &z, int minimum, int maximum,
                  int speed = 1, float width = 0.0f);
    bool dragInt4(StringView label, int &x, int &y, int &z, int &w, int minimum, int maximum,
                  int speed = 1, float width = 0.0f);
    bool inputInt2(StringView label, int &x, int &y, float width = 0.0f,
                   const char *format = nullptr);
    bool inputInt3(StringView label, int &x, int &y, int &z, float width = 0.0f,
                   const char *format = nullptr);
    bool inputInt4(StringView label, int &x, int &y, int &z, int &w, float width = 0.0f,
                   const char *format = nullptr);
    // Dupla precisão, para um campo que não se pode dar ao luxo de arredondar a
    // um float (o do ImGui é um campo de texto que faz o parse a double).
    bool inputDouble(StringView label, double &value, float width = 0.0f, int precision = 6,
                     const char *format = nullptr);
    // O DragFloatRange2 do ImGui: o mínimo e o máximo numa só widget, duas caixas
    // lado a lado (como os drags de N componentes) com o rótulo no fim. Cada caixa
    // fica limitada pela outra - o que se arrasta não passa para o lado do outro -
    // e o `format`/`formatMaximum` escrevem cada valor (o segundo, se nulo, usa o
    // primeiro).
    bool dragFloatRange2(StringView label, float &minimumValue, float &maximumValue,
                         float minimumRange, float maximumRange, float speed = 0.01f,
                         float width = 0.0f, const char *format = nullptr,
                         const char *formatMaximum = nullptr,
                         unsigned flags = SliderFlags::None);
    bool dragIntRange2(StringView label, int &minimumValue, int &maximumValue, int minimumRange,
                       int maximumRange, int speed = 1, float width = 0.0f,
                       const char *format = nullptr, const char *formatMaximum = nullptr,
                       unsigned flags = SliderFlags::None);
    bool dragFloat2(StringView label, float &x, float &y, float minimum, float maximum,
                    float speed = 0.01f, float width = 0.0f);
    bool dragFloat3(StringView label, float &x, float &y, float &z, float minimum, float maximum,
                    float speed = 0.01f, float width = 0.0f);
    bool dragFloat4(StringView label, float &x, float &y, float &z, float &w,
                    float minimum, float maximum, float speed = 0.01f, float width = 0.0f);
    bool stepperInt(StringView label, int &value, int minimum, int maximum,
                    float width = 0.0f);
    bool inputText(StringView label, String &value, float width = 0.0f,
                   unsigned flags = InputTextFlags::None);
    bool inputTextWithHint(StringView label, StringView hint, String &value, float width = 0.0f,
                           unsigned flags = InputTextFlags::None);
    bool inputTextMultiline(StringView label, String &value, float width = 0.0f,
                            float height = 120.0f, bool followTail = false);
    bool inputInt(StringView label, int &value, float width = 0.0f,
                  const char *format = nullptr);
    bool inputFloat(StringView label, float &value, float width = 0.0f, int precision = 6,
                    const char *format = nullptr);
    bool colorEdit(StringView label, Color &value, float width = 0.0f, float height = 128.0f);
    bool colorButton(StringView label, const Color &value, float width = 0.0f,
                     float height = 0.0f);
    void image(TextureId texture, float width, float height,
               const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
               const Color &tint = Color());
    bool imageButton(StringView label, TextureId texture, float width, float height,
                     const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
                     const Color &tint = Color());
    bool smallImageButton(StringView label, TextureId texture, float size = 0.0f,
                          const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
                          const Color &tint = Color());
    // Clipped, vertically scrollable content region. height is required; width
    // defaults to the remaining window width.
    bool beginChild(StringView id, float height, bool border = true, float width = 0.0f);
    void endChild();
    // Editor-style docking. Register panels each frame; only the active tab in
    // each region returns true. Tabs can be dragged between regions; tabs and
    // splitter sizes persist in the Context.
    bool beginDockSpace(StringView id);
    // Reserves topInset logical pixels for a menu bar or toolbar in the current window.
    bool beginDockSpace(StringView id, float topInset);
    bool beginDockSpace(StringView id, const Rect &bounds);
    void endDockSpace();
    // Sets the size of the side regions of the dock space being built (left and right width, bottom height); a value
    // of 0 or less keeps the current one. Call it once, on the first frame: the user can drag the splitters after.
    void setDockSizes(float leftWidth, float rightWidth, float bottomHeight);
    // Pass open to render a close button in the tab; closing it sets *open to false.
    bool beginDockPanel(StringView title, DockSlot slot = DockSlot::Center, bool *open = nullptr);
    void endDockPanel();
    // Virtualized child content. Only render rows in [firstVisible, lastVisible)
    // and use virtualListItemRect() with the bounds-based widget overloads.
    bool beginVirtualList(StringView id, int itemCount, float itemHeight, float height,
                          int &firstVisible, int &lastVisible,
                          bool border = true, float width = 0.0f);
    Rect virtualListItemRect(int itemIndex) const;
    void endVirtualList();
    // Virtualized table rows. Render only [firstVisibleRow, lastVisibleRow) and
    // use virtualTableCellRect() for bounds-based widgets in each cell.
    bool beginVirtualTable(StringView id, int rowCount, int columns, float rowHeight, float height,
                           int &firstVisibleRow, int &lastVisibleRow,
                           bool border = true, float width = 0.0f);
    // Virtualized rows with weighted columns. Use the same weights as a
    // preceding beginTable()/tableHeader() row to create a fixed, resizable
    // header aligned with the scrolling cells.
    bool beginVirtualTable(StringView id, int rowCount, Span<const float> columnWeights,
                           float rowHeight, float height,
                           int &firstVisibleRow, int &lastVisibleRow,
                           bool border = true, float width = 0.0f);
    Rect virtualTableCellRect(int row, int column) const;
    void endVirtualTable();
    // Virtualized rows for an application-owned, flattened tree. depth controls
    // indentation; use the bounds with treeItem() to keep its existing behavior.
    bool beginVirtualTree(StringView id, int rowCount, float rowHeight, float height,
                          int &firstVisibleRow, int &lastVisibleRow,
                          bool border = true, float width = 0.0f);
    Rect virtualTreeItemRect(int row, int depth, float indentWidth = 16.0f) const;
    void endVirtualTree();
    // Lightweight immediate table. Call tableNextColumn() before each cell.
    bool beginTable(StringView id, int columns, float width = 0.0f);
    // Weighted table columns. All weights must be positive; they are copied
    // for the frame. For example, {3.0f, 1.0f} creates a 75/25 split.
    bool beginTable(StringView id, Span<const float> columnWeights, float width = 0.0f);
    // Mutable weights opt into resizing through tableHeader() grips. The
    // adjusted weights are written back to the caller and should be kept for
    // the next frame to preserve the chosen column widths.
    bool beginTable(StringView id, Span<float> columnWeights, float width = 0.0f);
    // Weighted columns where the application decides which ones are visible (the
    // demo's "Hideable" table): a hidden column takes no width, the ones after it
    // close the gap, and tableNextColumn() returns false for its cells so the
    // application skips them.
    //
    //     bool visible[3] = {true, false, true};
    //     ui.beginTable("t", ig::Span<const float>(weights, 3),
    //                   ig::Span<const bool>(visible, 3));
    //     for (int c = 0; c < 3; ++c)
    //         if (ui.tableNextColumn()) { ... }   // a coluna 1 é saltada
    //
    // The usual way to drive those flags is a context menu on the header (see
    // tableHoveredColumn()).
    bool beginTable(StringView id, Span<const float> columnWeights, Span<const bool> columnVisible,
                    float width = 0.0f);
    bool beginTable(StringView id, Span<const float> columnWeights, Span<const bool> columnVisible,
                    const TableStyle &style, float width = 0.0f);
    /// @brief How many columns of the table being built are visible.
    int tableVisibleColumnCount() const;
    /// @brief The application's column behind the cell being submitted, when the
    ///        table is reorderable (a display position never moves, the model column
    ///        behind it does). Equal to the display position when the table is not
    ///        reorderable. It is what the caller indexes its own data with:
    ///
    ///            ui.tableNextColumn();
    ///            ui.tableHeader(names[ui.tableColumnIndex()]);
    int tableColumnIndex() const;
    /// @brief The column of the header row the pointer is over (-1 while it is over
    ///        none, or before the header row has been submitted this frame). It is
    ///        the model column (see tableColumnIndex), so it is the same column the
    ///        caller's own data is indexed with, and the one a header context menu
    ///        toggles:
    ///
    ///            ui.tableNextColumn(); ui.tableHeader(name);
    ///            if (ui.tableHoveredColumn() >= 0 && rightClicked) ui.openPopup("cols");
    int tableHoveredColumn() const;
    // Styled tables. The stripes and lines of a row use the minimum row height,
    // so a row taller than that keeps the stripe of one row.
    bool beginTable(StringView id, int columns, const TableStyle &style, float width = 0.0f);
    bool beginTable(StringView id, Span<const float> columnWeights, const TableStyle &style,
                    float width = 0.0f);
    bool beginTable(StringView id, Span<float> columnWeights, const TableStyle &style,
                    float width = 0.0f);
    bool tableNextColumn();
    // Header cell without sorting: the label on the panel color with a border.
    // It resizes with the grip like the sortable one when the table owns
    // mutable weights. Call tableNextColumn() before each header.
    void tableHeader(StringView label);
    // Cell of a tree-structured table: a guide line for every ancestor level,
    // a disclosure arrow when hasChildren, and the label. depth is the nesting
    // level (0 for roots). Returns true on the frame the arrow was pressed;
    // expanded is toggled then and the caller skips the rows of a collapsed
    // node. Call tableNextColumn() before it.
    bool tableTreeCell(StringView label, int depth, bool hasChildren, bool &expanded);
    // Draw a clickable header in the current table cell. sortColumn starts at
    // -1; clicking a new column selects ascending order and clicking the
    // selected column toggles sortAscending. Returns true when the sort state
    // changes. Call tableNextColumn() before every header and cell.
    bool tableHeader(StringView label, int &sortColumn, bool &sortAscending);
    void endTable();
    // Aligned label/value row for inspectors. Render the value widgets between
    // beginPropertyRow() and endPropertyRow().
    bool beginPropertyRow(StringView label, float labelWidth = 0.0f);
    void endPropertyRow();
    void progressBar(float value, float maximum, float width = 0.0f,
                     StringView overlay = StringView());
    int plotLines(StringView label, const float *values, int count, float width = 0.0f,
                  float height = 0.0f, int offset = 0, StringView overlay = StringView(),
                  float scaleMin = 0.0f, float scaleMax = 0.0f);
    int plotHistogram(StringView label, const float *values, int count, float width = 0.0f,
                      float height = 0.0f, int offset = 0, StringView overlay = StringView(),
                      float scaleMin = 0.0f, float scaleMax = 0.0f);
    void label(StringView text);
    // Text items: each draws at the layout cursor and advances it, so the widget
    // after it lands below. textDisabled() uses the theme's disabled colour, and
    // textWrapped()/bulletText() measure and wrap their own lines.
    // Põe a linha de base do próximo texto ao nível do texto de um widget com
    // moldura que venha à frente na mesma linha (o AlignTextToFramePadding do
    // ImGui). Mexe só no texto: o cursor e os itens não se deslocam.
    void alignTextToFramePadding();
    void textDisabled(StringView text);
    void textColored(StringView text, const Color &color);
    // Wraps at wrapWidth (0 is the remaining width of the enclosing region), at
    // spaces, and inside a word when a single word is wider than the line.
    void textWrapped(StringView text, float wrapWidth = 0.0f);
    // A bullet and its text, the text indented next to it.
    void bulletText(StringView text, float wrapWidth = 0.0f);
    void sameLine(float spacing = -1.0f);
    // Fecha a linha: o item seguinte começa na margem, uma linha abaixo. Se a
    // linha já estiver vazia (nada foi submetido nela) não faz nada, como o
    // NewLine do ImGui.
    void newLine();
    // ── beginGroup/endGroup: um bloco que é UM item ──────────────────────────
    // O que estiver entre os dois fica alinhado pela margem do grupo (não a da
    // janela), e o layout de fora avança pelo bloco inteiro: um sameLine() a
    // seguir põe o item seguinte à direita do grupo todo, não da última linha.
    // É assim que se põem dois blocos de três widgets lado a lado.
    //
    // Do lado de fora o grupo não tem id - não recebe clique, foco nem edição -,
    // mas tem rect: itemRect() é a caixa do grupo e isItemHovered() responde com
    // o ponteiro em qualquer ponto dela, inclusive num espaço entre widgets.
    //
    // Um beginGroup() sem endGroup() (ou ao contrário) não rebenta: o par
    // desequilibrado não faz nada, e o layout de um container novo começa limpo.
    void beginGroup();
    void endGroup();
    void spacing(float pixels);
    void indent(float pixels = 16.0f);
    void unindent(float pixels = 16.0f);
    void separator(float thickness = 1.0f);
    void separatorText(StringView text, float width = 0.0f);
    // Reserves a block of layout space without drawing anything, so custom
    // content drawn with the draw* primitives still reports its extent to the
    // enclosing beginChild()/window (which measure from the last laid-out item).
    void dummy(float width, float height);
    void setCursor(const Vec2 &localPosition);
    Vec2 cursor() const;
    // Remaining horizontal content space at the current layout cursor.
    float availableWidth() const;
    // Remaining vertical space in the current window or panel.
    float availableHeight() const;
    // Size of the display this frame was started with (FrameInfo::displaySize),
    // in logical units: what a window or an overlay needs to place itself against
    // the screen instead of against whatever encloses it.
    Vec2 displaySize() const { return frame_.displaySize; }

private:
    struct PointerState
    {
        Vec2 position;
        Vec2 previousPosition;
        Vec2 pressedPosition[3];
        Vec2 releasedPosition[3];
        bool down[3];
        bool pressed[3];
        bool released[3];
        float wheelX;
        float wheelY;
        bool wheelControl;
        // Ctrl/Shift tal como estavam na altura em que cada botão foi premido (ver
        // Events.hpp e a escolha múltipla do selectable).
        bool pressedControl[3];
        bool pressedShift[3];

        PointerState();
    };

    struct LayoutState
    {
        Vec2 origin;
        Vec2 cursor;
        float baseOriginX;
        Rect lastItem;
        bool hasLastItem;
        // Canto inferior direito do maior item submetido desde o início deste
        // container: é o que dá a medida de uma caixa que tem de se dimensionar
        // sozinha, como o tooltip rico (ver beginTooltip).
        Vec2 contentExtent;
        // A extensão já tem um item: só o início de um contentor a limpa (um indent,
        // um setCursor ou um grupo não recomeçam a medida).
        bool hasExtent;
        // A borda direita do maior item que não ocupa só a largura que sobra (ver
        // measureTableCellWidth e endCombo).
        float fitRight;
        // Largura do próximo item (setNextItemWidth) e o valor por omissão de
        // dentro dos pushItemWidth (ver calcItemWidth). Zero quer dizer "a
        // largura que sobra", como no ImGui.
        float nextItemWidth;
        // Pilha do pushItemWidth (8 níveis): o de dentro volta ao de fora no pop, e
        // um push de 0 herda a largura que estava. Uma só guarda não bastava - dois
        // pushes aninhados e o pop de dentro deixava a largura de dentro em vigor.
        float itemWidthStack[8];
        int itemWidthDepth;
        // Nível de aninhamento de beginGroup/endGroup, só para o par
        // desequilibrado não mexer no layout (ver groups_).
        int groupDepth;
        // O y do primeiro item da linha corrente e a altura do mais alto dela. O
        // item seguinte começa por baixo do mais alto, não por baixo do último:
        // é a regra do ImGui (a altura da linha é o máximo dos itens dela), e sem
        // ela um sameLine() para um item baixo puxava a linha seguinte para cima.
        float lineTop;
        float lineHeight;

        LayoutState()
            : origin(), cursor(), baseOriginX(0.0f), lastItem(), hasLastItem(false),
              contentExtent(), hasExtent(false), fitRight(0.0f), nextItemWidth(0.0f), itemWidthStack(), itemWidthDepth(0),
              groupDepth(0), lineTop(0.0f), lineHeight(0.0f)
        {
        }
    };

    // Estado do último item submetido (ver isItemHovered e companhia). Um item
    // novo começa em itemHovered/itemClicked, que limpam o que é de um item só
    // (hover, clique, edição); advanceLayout só lhe dá os bounds.
    struct ItemStatus
    {
        WidgetId id;
        Rect bounds;
        bool hovered;
        bool active;
        bool clicked;
        bool doubleClicked;
        bool edited;
        bool focused;
        bool disabled;
        // O item passou a ser o ativo neste frame (isItemActivated) e deixou de o
        // ser neste frame (isItemDeactivated/isItemDeactivatedAfterEdit, quando foi
        // editado enquanto esteve ativo).
        bool activated;
        bool deactivated;
        bool deactivatedAfterEdit;
        // O item abriu ou fechou neste frame (isItemToggledOpen).
        bool toggledOpen;
        // As condições do hover, separadas: é com elas que o isItemHovered(flags)
        // decide se pode dispensar o que está a bloquear (ver ItemHoverFlags).
        bool hoveredInRect;
        bool hoveredInClip;
        bool hoveredDelayed;
        bool hoveredStationary;
        bool hoveredBlockedByActive;
        bool hoveredBlockedByPopup;

        ItemStatus()
            : id(InvalidWidgetId), bounds(), hovered(false), active(false), clicked(false),
              doubleClicked(false), edited(false), focused(false), disabled(false), activated(false),
              deactivated(false), deactivatedAfterEdit(false), toggledOpen(false),
              hoveredInRect(false), hoveredInClip(false), hoveredDelayed(false),
              hoveredStationary(false), hoveredBlockedByActive(false),
              hoveredBlockedByPopup(false)
        {
        }
    };

    struct ColorPickerState
    {
        float hue;
        float saturation;
        float brightness;
        Color lastColor;
        bool initialized;

        ColorPickerState()
            : hue(0.0f), saturation(0.0f), brightness(0.0f), lastColor(), initialized(false) {}
    };

    struct ChildScrollState
    {
        float offset;
        float contentHeight;

        ChildScrollState() : offset(0.0f), contentHeight(0.0f) {}
    };

    struct Gizmo2DState
    {
        uint8_t axis;
        Vec2 pointerStart;
        Transform2D transformStart;

        Gizmo2DState() : axis(0u), pointerStart(), transformStart() {}
    };

    struct Gizmo3DState
    {
        uint8_t axis;
        Vec2 pointerStart;
        Transform3D transformStart;
        // Where the pointer's ray crossed the plane of the grabbed ring when the
        // press landed, measured from the target. A rotation is the turn from
        // here to wherever the ray crosses now, on that plane rather than on the
        // screen - see gizmo3DSweptAngle.
        Vec3 ringStart;

        Gizmo3DState() : axis(0u), pointerStart(), transformStart(), ringStart() {}
    };

    struct ChildState
    {
        LayoutState parentLayout;
        Rect outer;
        Rect content;
        Rect parentClip;
        WidgetId id;
        bool border;
        // How many dock panels were open when the child began: the child is inside
        // the innermost one only when no panel was opened after it.
        uint32_t dockDepth;

        ChildState()
            : parentLayout(), outer(), content(), parentClip(), id(InvalidWidgetId), border(true),
              dockDepth(0u)
        {
        }
    };

    // A barra de tabs (ver beginTabBar): o que está escolhido, a ordem das tabs (que
    // o arrasto troca) e as que já se viram, para saber qual é nova.
    struct TabBarState
    {
        // A escolhida (o realce, que um clique muda já) e a que abre o conteúdo
        // neste frame (é a do início do frame: o conteúdo de uma tab escolhida agora
        // aparece no frame seguinte, como no ImGui).
        WidgetId selected;
        WidgetId frameSelected;
        // A ordem das tabs (é o que o arrasto troca): as posições de cada uma saem
        // dela, por isso uma troca vale para o frame seguinte (a largura de uma tab
        // só se sabe depois de ela ser submetida, como tudo o que se mede aqui).
        ct::Vector<WidgetId> order;
        ct::Vector<WidgetId> seenThisFrame;
        // A banda de cada tab (Leading/Trailing): é o que põe as das pontas nas
        // pontas, sejam tabs ou botões.
        ct::HashMap<WidgetId, unsigned> bands;
        // O nome de cada tab, para o botão da lista as poder listar (a lista é a do
        // frame anterior, como no ImGui).
        ct::HashMap<WidgetId, String> names;
        ct::HashMap<WidgetId, float> widths;
        ct::HashMap<WidgetId, float> positions;
        // Quantas tabs a barra tinha no frame anterior: é o que o Fitting usa para
        // dividir a largura (o número de tabs deste frame só se sabe no fim dele).
        int lastCount;
        // Arrasto em curso: a tab agarrada e onde o ponteiro estava.
        WidgetId dragTab;
        float dragStartX;
        uint64_t frame;

        TabBarState()
            : selected(InvalidWidgetId), frameSelected(InvalidWidgetId), order(), seenThisFrame(),
              bands(), names(), widths(), positions(),
              lastCount(0), dragTab(InvalidWidgetId), dragStartX(0.0f), frame(0u) {}
    };
    // A barra que está a ser submetida agora (uma pilha: um tab bar dentro do
    // conteúdo de outro).
    struct TabBarSubmitState
    {
        WidgetId id;
        LayoutState parentLayout;
        Rect row;
        unsigned flags;
        float cursorX;
        float contentHeight;
        // A faixa onde as tabs cabem: o row todo menos o botão da lista e as
        // bandas das pontas (é o que o Fitting divide e onde o Trailing encosta).
        float tabsX;
        float tabsWidth;
        // A largura que cada ponta ocupa neste frame, separada em tabs e botões: as
        // tabs de uma ponta encostam-se ao lado dos botões dessa ponta (é o que põe
        // o "+" à direita das tabs, e as tabs Leading depois do "?"), e o que sobra
        // é que é dividido pelo Fitting.
        float leadingTabWidth;
        float leadingButtonWidth;
        float trailingTabWidth;
        float trailingButtonWidth;

        TabBarSubmitState()
            : id(InvalidWidgetId), parentLayout(), row(), flags(0u), cursorX(0.0f),
              contentHeight(0.0f), tabsX(0.0f), tabsWidth(0.0f), leadingTabWidth(0.0f),
              leadingButtonWidth(0.0f), trailingTabWidth(0.0f), trailingButtonWidth(0.0f) {}
    };
    // O conteúdo da tab escolhida: o layout de fora e o clip que os itens de dentro
    // apanham (ver contentClip/availableWidth).
    struct TabContentState
    {
        LayoutState parentLayout;
        Rect rect;
        bool active;
        // A profundidade da barra dona do conteúdo: uma barra de dentro do
        // conteúdo tem as suas tabs, a mesma barra não abre outra antes do fim.
        int barDepth;

        TabContentState() : parentLayout(), rect(), active(false), barDepth(0) {}
    };
    struct UndoState
    {
        String label;
        ct::Function<void()> undo;
        ct::Function<void()> redo;
    };

    struct DockTabState
    {
        WidgetId id;
        String title;
        DockSlot slot;
        uint64_t lastSeenFrame;
        // O ponteiro open de um painel só vale dentro da chamada que o recebeu: a
        // tab guarda se se pode fechar e o pedido do X, e é o beginDockPanel desse
        // painel que escreve o *open.
        bool closable;
        bool closeRequested;
        bool closed;

        DockTabState()
            : id(InvalidWidgetId), title(), slot(DockSlot::Center), lastSeenFrame(0u),
              closable(false), closeRequested(false), closed(false)
        {
        }
    };

    struct DockSpaceState
    {
        String name;   // id the dock space was opened with, for the state file
        Rect bounds;
        Rect clip;
        // The sizes asked for (dragged, read from the state file or set with
        // setDockSizes) and the ones this frame uses, fitted to the dock space: a
        // frame of squeezing must not lose what was asked for.
        float leftWidth;
        float rightWidth;
        float bottomHeight;
        float usedLeft;
        float usedRight;
        float usedBottom;
        WidgetId selected[4];
        bool tabListOpen[4];
        bool tabBarHidden[4];
        uint64_t tabBarFrame[4];
        // Frame in which the region of each slot was last drawn (every tab of a slot calls beginDockPanel).
        uint64_t regionFrame[4];
        // Frame in which a panel of each slot drew its content: one per frame, even
        // when the tab on top changes halfway through it.
        uint64_t contentFrame[4];
        ct::Vector<DockTabState> tabs;

        DockSpaceState()
            : bounds(), clip(), leftWidth(180.0f), rightWidth(240.0f), bottomHeight(180.0f),
              usedLeft(180.0f), usedRight(240.0f), usedBottom(180.0f), tabs()
        {
            for (uint32_t i = 0u; i < 4u; ++i)
            {
                selected[i] = InvalidWidgetId;
                tabListOpen[i] = false;
                tabBarHidden[i] = false;
                tabBarFrame[i] = 0u;
                regionFrame[i] = 0u;
                contentFrame[i] = 0u;
            }
        }
    };

    struct DockPanelState
    {
        LayoutState parentLayout;
        Rect content;
        WidgetId id;

        DockPanelState() : parentLayout(), content(), id(InvalidWidgetId) {}
    };

    // Um bloco de beginGroup (ver a declaração pública): guarda o layout de fora
    // para o repor no fim, e a caixa do que ficou lá dentro.
    struct GroupState
    {
        // Layout de fora, tal como estava em beginGroup.
        float originX;
        float baseOriginX;
        float nextItemWidth;
        bool hasLastItem;
        // A linha de fora: dentro do grupo as linhas são as do grupo, e no fim
        // volta-se a esta (o grupo em si é um item da linha de fora).
        float lineTop;
        float lineHeight;
        // Canto superior esquerdo do grupo e canto inferior direito do maior item
        // que lhe passou por dentro (advanceLayout trata disso).
        Vec2 start;
        Vec2 extent;
        // Falso quando o nível máximo já estava atingido: o par begin/end não
        // mexe em nada, mas fica equilibrado.
        bool active;
        // Os contentores abertos no beginGroup (ver layoutNesting): o que é submetido
        // dentro de um child, de uma tabela ou de um popup não faz crescer o grupo.
        uint32_t nesting;

        GroupState()
            : originX(0.0f), baseOriginX(0.0f), nextItemWidth(0.0f), hasLastItem(false),
              lineTop(0.0f), lineHeight(0.0f), start(), extent(), active(false), nesting(0u)
        {
        }
    };

    struct VirtualListState
    {
        WidgetId childId;
        Rect content;
        int itemCount;
        float itemHeight;
        float scrollOffset;

        VirtualListState()
            : childId(InvalidWidgetId), content(), itemCount(0), itemHeight(0.0f), scrollOffset(0.0f) {}
    };

    struct VirtualTableState
    {
        WidgetId childId;
        int columns;
        ct::Vector<float> columnWeights;
        float totalColumnWeight;

        VirtualTableState()
            : childId(InvalidWidgetId), columns(0), columnWeights(), totalColumnWeight(0.0f) {}
    };

    struct VirtualTreeState
    {
        WidgetId childId;

        VirtualTreeState() : childId(InvalidWidgetId) {}
    };

    // A ordem das colunas de uma tabela reordenável, por id (ver TableStyle).
    struct TableOrderState
    {
        static const int kMaxColumns = 32;
        int order[kMaxColumns];
        int count;
        // Arrasto em curso, de uma posição para outra (sobrevive ao frame, porque o
        // arrasto ocupa vários).
        bool reordering;
        int reorderFrom;
        int reorderTo;
        // Onde o ponteiro estava quando se carregou, e o último frame em que o
        // arrasto foi visto (para um arrasto que deixe de ser submetido não ficar
        // com o rato agarrado).
        float reorderStartX;
        WidgetId reorderWidget;
        uint64_t reorderFrame;

        TableOrderState()
            : order(), count(0), reordering(false), reorderFrom(-1), reorderTo(-1),
              reorderStartX(0.0f), reorderWidget(InvalidWidgetId), reorderFrame(0) {}
    };
    // Onde está a rolagem de cada tabela (TableStyle::scrollY), em pixels do
    // conteúdo e pelo id da tabela: a altura do que rola e das linhas presas só se
    // sabe depois de as linhas serem submetidas, por isso é o frame anterior que a
    // diz (é o mesmo caminho de um child, ver beginChild).
    struct TableScrollState
    {
        float offsetY;
        float contentHeight;
        float frozenHeight;
        // Rolagem horizontal e a altura das linhas do frame anterior (é ela que diz
        // onde a barra de baixo cai quando a tabela não tem altura própria).
        float offsetX;
        float rowsHeight;

        TableScrollState()
            : offsetY(0.0f), contentHeight(0.0f), frozenHeight(0.0f), offsetX(0.0f),
              rowsHeight(0.0f) {}
    };
    struct TableState
    {
        LayoutState parentLayout;
        Rect bounds;
        WidgetId id;
        int columns;
        int column;
        float rowY;
        float rowHeight;
        ct::Vector<float> columnWeights;
        float *resizableColumnWeights;
        float totalColumnWeight;
        // Uma entrada por coluna (vazio = todas visíveis). Uma coluna escondida não
        // ocupa largura e o tableNextColumn dela devolve false.
        ct::Vector<uint8_t> columnVisible;
        // A coluna do cabeçalho sob o ponteiro (ver tableHoveredColumn).
        int headerHoveredColumn;
        // Rolagem da tabela (TableStyle::scrollY): contentY é onde a linha atual
        // estaria sem rolagem (é o que dá a altura do conteúdo), frozenHeight a
        // altura das linhas presas já submetidas e offsetY a rolagem deste frame.
        float contentY;
        float frozenHeight;
        float offsetY;
        // Largura do espaço à direita que a barra de rolagem ocupa (0 sem barra): a
        // caixa das colunas é mais estreita do que a faixa que rola.
        float barStrip;
        // SizingFixedFit: a largura que cada coluna (do modelo) pediu neste frame, para
        // o frame seguinte a usar. O cabeçalho também conta.
        ct::Vector<float> contentWidths;
        int freezeRows;
        float viewHeight;
        bool scrollY;
        // Rolagem horizontal: a largura das colunas, a rolagem deste frame, a largura
        // das colunas presas e a faixa que a barra de baixo ocupa.
        float contentWidth;
        float offsetX;
        float pinWidth;
        float barStripH;
        int freezeCols;
        bool scrollX;
        TableStyle style;
        int row;

        TableState()
            : parentLayout(), bounds(), id(InvalidWidgetId), columns(0), column(-1), rowY(0.0f),
              rowHeight(0.0f), columnWeights(), resizableColumnWeights(nullptr),
              totalColumnWeight(0.0f), columnVisible(), headerHoveredColumn(-1), contentY(0.0f),
              frozenHeight(0.0f), offsetY(0.0f), barStrip(0.0f), contentWidths(), freezeRows(0),
              viewHeight(0.0f),
              scrollY(false), contentWidth(0.0f), offsetX(0.0f), pinWidth(0.0f), barStripH(0.0f),
              freezeCols(0), scrollX(false), style(), row(0) {}
    };

    struct TableResizeState
    {
        WidgetId tableId;
        int column;
        float lastPointerX;

        TableResizeState() : tableId(InvalidWidgetId), column(-1), lastPointerX(0.0f) {}
    };

    struct PropertyRowState
    {
        LayoutState parentLayout;
        Rect bounds;

        PropertyRowState() : parentLayout(), bounds() {}
    };

    struct ToastState
    {
        WidgetId id;
        String text;
        ToastPosition position;
        float remaining;
        float duration = 0.0f;

        ToastState() : id(InvalidWidgetId), text(), position(ToastPosition::BottomRight), remaining(0.0f) {}
    };

    struct DragDropState
    {
        WidgetId widget;
        DragDropPayload payload;
        String preview;
        Vec2 pressedPosition;
        bool armed;
        bool active;
        bool accepted;

        DragDropState()
            : widget(InvalidWidgetId), payload(), preview(), pressedPosition(), armed(false), active(false), accepted(false) {}
    };

    Backend &backend_;
    TextProvider *textProvider_;
    Theme theme_;
    FrameInfo frame_;
    PointerState pointer_;
    LayoutState layout_;
    ct::Deque<Event> events_;
    ct::Vector<Event> textEvents_;
    ct::SlotMap<WindowState> windows_;
    ct::HashMap<WidgetId, WindowHandle> windowsById_;
    // Geometry the windows are remembered in ("" path = persistence off).
    WindowStateIni windowState_;
    // Per-title button visibility (bit0 minimize, bit1 maximize), kept even
    // for windows not created yet - see setWindowButtons.
    ct::HashMap<WidgetId, uint8_t> windowButtons_;
    ct::HashMap<WidgetId, int> listScrolls_;
    // A ordem das colunas de cada tabela reordenável, guardada entre frames pelo id
    // da tabela (a tabela em si não tem estado próprio, como as listas).
    ct::HashMap<WidgetId, TableOrderState> tableOrders_;
    // A rolagem de cada tabela que rola (ver TableScrollState).
    ct::HashMap<WidgetId, TableScrollState> tableScrolls_;
    // Onde está a lista de cada combo de conteúdo livre, em pixels do conteúdo. É
    // por id, como as listas: um combo fechado esquece onde estava (ver beginCombo).
    ct::HashMap<WidgetId, float> comboScrolls_;
    ct::HashMap<WidgetId, int> textScrolls_;
    // Line count drawn last frame, kept only for followTail widgets so the
    // next frame can tell growth from a scroll-away (see inputTextMultiline).
    ct::HashMap<WidgetId, int> textTailLines_;
    struct SequenceDrag
    {
        int row = -1;
        int mode = 0;
        int start = 0;
        int end = 0;
        float x = 0;
    };
    ct::HashMap<WidgetId, SequenceDrag> sequenceDrags_;
    struct TimeView
    {
        float zoom = 1;
        float offset = 0;
        float panX = 0;
        float panOffset = 0;
    };
    ct::HashMap<WidgetId, TimeView> timeViews_;
    void updateTimeView(WidgetId id, const Rect& area, const Rect& ruler, TimeView& view);
    void drawTimeView(const Rect& area, const Rect& ruler, const TimeView& view);
    void drawTimeRuler(const Rect& area, const Rect& ruler, const TimeView& view, int first, double count);
    struct NumericEditState
    {
        String text;
        bool editing = false;
        bool moved = false;
    };
    ct::HashMap<WidgetId, NumericEditState> numericEdits_;
    ct::HashMap<WidgetId, ColorPickerState> colorPickers_;
    ct::HashMap<WidgetId, ChildScrollState> childScrolls_;
    // As barras de tabs por id (a escolhida, a ordem e as vistas neste frame).
    ct::HashMap<WidgetId, TabBarState> tabBars_;
    // O botão da lista de tabs (ver TabBarFlags::TabListPopupButton): o combo sem
    // pré-visualização que abre a lista das tabs da barra.
    void tabListPopupButton(float width, TabBarState &state);
    // As larguras que cada coluna pediu, por tabela (ver TableStyle::sizingFixedFit):
    // o que o conteúdo de uma coluna mede vale para o frame seguinte.
    ct::HashMap<WidgetId, ct::Vector<float>> tableFitWidths_;
    // Mede a largura do conteúdo da célula em curso (o chamador já a submeteu) e
    // guarda-a na coluna dela do modelo.
    void measureTableCellWidth();
    // Põe a tab em x e devolve onde a próxima da mesma banda começa.
    float tabPlacePosition(TabBarState &state, WidgetId tab, float x);
    // Há uma tab da barra de cima aberta (o seu conteúdo ainda não fechou).
    bool tabItemOpenInCurrentBar() const;
    static const int kTabBarStackSize = 4;
    TabBarSubmitState tabBarStack_[kTabBarStackSize];
    int tabBarDepth_;
    TabContentState tabContent_[kTabBarStackSize];
    int tabContentDepth_;
    ct::HashMap<WidgetId, Gizmo2DState> gizmo2DStates_;
    ct::HashMap<WidgetId, Gizmo3DState> gizmo3DStates_;
    ct::HashMap<WidgetId, DockSpaceState> dockSpaces_;
    // Where a scroll area was (and how deeply nested) the last time it was
    // drawn: the wheel goes to the innermost one under the pointer, and that has
    // to be known before any of them is drawn this frame.
    struct WheelArea
    {
        Rect rect;
        WidgetId window;
        uint32_t depth;
        uint32_t order;
        uint32_t frame;
    };
    ct::HashMap<WidgetId, WheelArea> wheelAreas_;
    uint32_t wheelOrderCounter_ = 0u;
    ct::Vector<WindowHandle> windowOrder_;
    ct::Vector<WidgetId> idStack_;
    ct::Vector<WidgetId> focusOrder_;
    ct::Vector<WindowHandle> focusWindows_;
    // Same ids as focusOrder_, for an O(1) "is this one already in the Tab
    // order?" - the scan it replaces was quadratic in the number of widgets.
    ct::HashSet<WidgetId> focusSeen_;
    ct::Vector<ChildState> childStack_;
    ct::Vector<VirtualListState> virtualListStack_;
    ct::Vector<VirtualTableState> virtualTableStack_;
    ct::Vector<VirtualTreeState> virtualTreeStack_;
    ct::Vector<DockPanelState> dockPanelStack_;
    ct::Vector<ToastState> toasts_;
    ct::Vector<UndoState> undoStack_;
    ct::Vector<UndoState> redoStack_;
    DragDropState dragDrop_;
    DrawList frameDrawList_;
    DrawList dragDropDrawList_;
    DrawList toastDrawList_;
    DrawList modalDrawList_;
    DrawData drawData_;
    WindowHandle currentWindow_;
    WindowHandle focusedWindow_;
    // The window whose menu/combo list is open: a popup is allowed to hang
    // outside it (that is the point of a popup), so that window must keep
    // receiving the pointer over its own popup.
    WindowHandle popupWindow_;
    WindowHandle draggingWindow_;
    WindowHandle resizingWindow_;
    WidgetId activeWidget_;
    // O widget ativo no início do frame e se ele já foi editado: é o que distingue
    // "ficou ativo agora" de "já estava" (ver isItemActivated/isItemDeactivated).
    WidgetId activeWidgetAtFrameStart_;
    // Style vars em vigor (ver pushStyleColor/pushStyleVar): o campo e o valor que
    // ele tinha antes, para o pop repor.
    struct StyleColorPush
    {
        Color Theme::*field;
        Color previous;

        StyleColorPush() : field(nullptr), previous() {}
    };
    struct StyleVarPush
    {
        float Theme::*field;
        float previous;

        StyleVarPush() : field(nullptr), previous(0.0f) {}
    };
    static const int kStyleStackSize = 16;
    StyleColorPush styleColors_[kStyleStackSize];
    int styleColorDepth_;
    StyleVarPush styleVars_[kStyleStackSize];
    int styleVarDepth_;
    // Faces em vigor (ver pushFont): a face e o tamanho que estava em uso antes.
    struct FontPush
    {
        FontId font;
        float size;

        FontPush() : font(), size(0.0f) {}
    };
    static const int kFontStackSize = 8;
    FontPush fonts_[kFontStackSize];
    int fontDepth_;
    void clearStyleStacks();
    bool activeWidgetEdited_;
    WidgetId hotWidget_;
    // Tempo acumulado desde o primeiro frame, para o que anima sozinho (a barra
    // indeterminada). Soma os deltaSeconds de cada frame.
    double elapsedSeconds_;
    // beginDisabled()/endDisabled() depth: above zero, nothing reacts and the
    // draw lists fade what they draw (see applyDisabledFade).
    uint32_t disabledDepth_;
    // Gap between the pointer and the centre of the cap of the fader being
    // dragged: only one widget is active at a time, so one float is enough to
    // let a press on the cap carry on from where it landed.
    float faderGrabOffset_;
    WidgetId lastItemId_;
    ItemStatus item_;
    // Cache do topWindowAt(pointer_.position) para o frame corrente. mutável
    // porque as consultas de hover são const e isto é só memória.
    mutable WindowHandle pointerWindow_;
    mutable uint64_t pointerWindowFrame_;
    mutable bool pointerWindowValid_;
    // Verdadeiro enquanto os itens da lista de um combo aberto estão a ser
    // submetidos: é o que distingue esses itens do que está por baixo deles (ver
    // pointerBlockedByOpenPopup).
    bool comboItemsSubmitted_;
    // Grupos abertos neste frame (ver beginGroup/endGroup). Só os pares
    // equilibrados chegam ao fim; um beginGroup sem endGroup é descartado no
    // início do container seguinte (ver beginLayout).
    ct::Vector<GroupState> groups_;
    WidgetId focusedWidget_;
    WidgetId textInputWidget_;
    WidgetId openCombo_;
    WidgetId openMenu_;
    WidgetId openContextMenu_;
    WidgetId openSubMenu_;
    // frameNumber_ of the last frame the open submenu was submitted: one that is no
    // longer submitted (its popup or menu closed) stops counting as open.
    uint64_t openSubMenuFrame_;
    WidgetId activeMenu_;
    WidgetId subMenuParent_;
    // Snapshot of openMenu_/openContextMenu_ and the popup rects, taken once
    // in beginFrame before any beginMenu()/beginContextMenu() runs this
    // frame - see pointerBlockedByOpenPopup's comment for why this can't just
    // read openMenu_ live.
    bool menuWasOpenAtFrameStart_;
    Rect menuPopupBoundsAtFrameStart_;
    Rect subMenuPopupBoundsAtFrameStart_;
    // The same snapshot for an open comboBox popup: its header and list rects
    // as drawn last frame - see pointerBlockedByOpenPopup.
    bool comboWasOpenAtFrameStart_;
    Rect comboBoundsAtFrameStart_;
    Rect comboPopupBoundsAtFrameStart_;
    Rect comboBounds_;
    Rect comboPopupBounds_;
    // alignTextToFramePadding() pedido e ainda não consumido (ver label).
    bool alignTextNext_;
    // Escolha múltipla (ver beginMultiSelect): a seleção da aplicação, o índice do
    // próximo item do bloco e o item de onde o Shift conta o alcance.
    struct MultiSelectState
    {
        bool active;
        unsigned flags;
        Span<bool> selected;
        int nextIndex;
        // O item do último clique sem Shift, e de que lista ele é: o alcance do
        // Shift não pode atravessar duas listas.
        int anchor;
        const bool *anchorOwner;
        bool lastSelected;
        bool lastToggled;

        MultiSelectState()
            : active(false), flags(0u), selected(), nextIndex(0), anchor(-1),
              anchorOwner(nullptr), lastSelected(false), lastToggled(false)
        {
        }
    };
    MultiSelectState multiSelect_;
    // ── Os popups de conteúdo livre abertos (ver openPopup/beginPopup) ────────
    // É uma pilha: um popup aberto de dentro de outro fica por cima dele (é o
    // "Stacked Popup" do demo), e só o de cima recebe o ponteiro, o Escape e o
    // clique fora - os de baixo continuam abertos à espera de serem submetidos outra
    // vez, que é o que faz o pai voltar quando o filho fecha.
    struct PopupEntry
    {
        WidgetId id;
        Vec2 position;
        // A largura pedida no openPopup (0 = a do menu do tema).
        float width;
        // O frame em que este popup foi submetido pela última vez (quem não o
        // submeter num frame perde-o, como tudo no modo imediato).
        uint64_t frame;
        Rect bounds;
        // A caixa do início do frame: é a que decide se um clique caiu dentro (a de
        // agora ainda pode mudar, porque o corpo é medido no fim do frame).
        Rect boundsAtFrameStart;
        Vec2 measured;
        float measuredWidth;
        // A medida só vale quando o corpo foi submetido pelo menos uma vez (ver
        // endPopup); um modal mede-se, um popup comum tem a largura que foi pedida.
        bool measuredThisFrame;
        bool wasOpenAtFrameStart;
        bool modal;
        // Fechado pelo closeCurrentPopup (o corpo ainda vai até ao endPopup).
        bool open;
        WindowHandle window;

        PopupEntry()
            : id(InvalidWidgetId), position(), width(0.0f), frame(0u), bounds(),
              boundsAtFrameStart(), measured(), measuredWidth(0.0f), measuredThisFrame(false),
              wasOpenAtFrameStart(false), modal(false), open(true), window()
        {
        }
    };
    static const int kPopupStackSize = 8;
    PopupEntry popups_[kPopupStackSize];
    int popupDepth_;
    // Se o popup de cima era um modal no início do frame: é o que trava os atalhos
    // (ver shortcut/isKeyPressed) enquanto ele está aberto.
    bool modalPopupWasOpenAtFrameStart_;
    // O popup cujo corpo está a ser submetido agora (índice na pilha, -1 se nenhum):
    // é o que o ponteiro tem de respeitar (ver pointerBlockedByOpenPopup) e o que o
    // endPopup fecha.
    int popupContentIndex_;
    void openPopupSized(StringView id, const Vec2 &position, const Vec2 &size);
    // O índice do popup na pilha (-1 quando não está lá, ou já está fechado).
    int findPopup(WidgetId id) const;
    // Tira da pilha o popup e os que estão por cima dele (um filho sem pai não é
    // popup de nada).
    void closePopupAt(int index);
    // Quantos popups estão abertos (e não fechados).
    bool anyPopupOpen() const;
    // Menu aberto como contentor de layout (ver beginPopupContent). É uma pilha
    // porque um submenu abre um contentor dentro do do pai, e ao fechar tem de
    // voltar ao cursor do pai (os itens do pai continuam depois dele).
    struct PopupContentState
    {
        LayoutState layout;
        Rect clip;
        float width;
        bool isPopup;
        int outerPopupIndex;

        PopupContentState()
            : layout(), clip(), width(0.0f), isPopup(false), outerPopupIndex(-1)
        {
        }
    };
    static const int kPopupContentStackSize = 4;
    PopupContentState popupContents_[kPopupContentStackSize];
    int popupContentDepth_;
    // A barra de menu da aplicação (ver beginMainMenuBar): a janela dela, que é a
    // última a ser desenhada, e se ela foi submetida neste frame.
    WindowHandle mainMenuBarWindow_;
    // Height of the application menu bar while it is submitted (0 without one):
    // maximized, tiled and main windows start below it (see workArea).
    float menuBarStrip_ = 0.0f;
    Rect workArea() const;
    bool mainMenuBarOpen_;
    // Combo de conteúdo livre (ver beginCombo/endCombo). Enquanto o popup está
    // aberto o cursor do layout está dentro dele e o que o chamador submete vai
    // para comboDrawList_, que sai logo depois da sobreposição da janela: a caixa
    // é desenhada no endCombo (aí já se sabe a medida do conteúdo) e fica por
    // baixo do conteúdo.
    bool comboContentOpen_;
    LayoutState comboLayout_;
    Rect comboHeaderRect_;
    Rect comboContentClip_;
    float comboContentWidth_;
    WidgetId comboContentId_;
    int comboVisibleItems_;
    WindowHandle comboContentWindow_;
    DrawList comboDrawList_;
    // Medida do conteúdo do combo aberto no último frame: é ela que dá o tamanho
    // da caixa e do clip neste (ver beginCombo). A id diz de que combo é, para a
    // medida de um não aparecer como tamanho de outro.
    WidgetId comboMeasuredId_;
    Vec2 comboMeasured_;
    // A barra de scroll do popup aberto, se ela lá está: o endCombo desenha-a (o
    // begin só sabe onde ela fica).
    // O setItemDefaultFocus() só conta quando a lista ABRE (é o "IsWindowAppearing"
    // do ImGui): a aplicação pede-o a cada frame, e aceitá-lo sempre prendia a lista
    // ao item escolhido e não deixava rolar. `armed` é o pedido ainda aceite (até o
    // primeiro frame com medida), `pending` é o que foi pedido neste.
    bool comboFocusArmed_;
    bool comboFocusPending_;
    Rect comboFocusBounds_;
    bool comboJustOpened_;
    bool comboScrollbarVisible_;
    Rect comboScrollbar_;
    Rect comboScrollThumb_;
    float comboScroll_;
    float comboMaxScroll_;
    // Um beginCombo do layout avança o cabeçalho no endCombo, não em si: o layout
    // de dentro do popup é reposto lá (ver endCombo).
    bool comboAdvanceHeader_;
    // frameNumber_ of the last frame each blocker was submitted. beginFrame
    // drops a blocker the application stopped submitting (the usual
    // `if (show) messageBox(...)`): it is off screen, so nothing is left for
    // the user to dismiss, and holding on to it would freeze all input.
    uint64_t activeModalFrame_;
    uint64_t openMenuFrame_;
    uint64_t openComboFrame_;
    WidgetId activeModal_;
    WidgetId modalWindowId_;
    WidgetId dragWidget_;
    WidgetId activeDockSpace_;
    WidgetId dockDragSpace_;
    WidgetId dockDragTab_;
    String::size_type textCursor_;
    uint64_t frameNumber_;
    uint64_t nextZOrder_;
    Vec2 windowDragOffset_;
    Vec2 dockDragStart_;
    Rect menuBarBounds_;
    Rect menuPopupBounds_;
    Vec2 contextMenuAnchor_;
    // Width of the open menu's popup, keyed by menu id. A dropdown cannot
    // know how wide its widest item is before the items are submitted, so
    // menuItemInternal records the widest label it drew and endMenu latches
    // it here; beginMenu/beginContextMenu size the next frame's panel with
    // it (menuMinWidth is the floor, so a menu reaches its full width from
    // its second frame on). Without this the panel stayed at menuMinWidth
    // and a long item ("Build and run native") drew its tail outside the
    // border.
    // Text of the inputInt/inputFloat being edited. Rebuilding the text from
    // the value every frame destroyed what the user was typing ("-", "1.",
    // an empty field are not numbers yet), so while one is focused its text
    // lives here; numericEditValue_ is the value that text last produced, to
    // spot the application changing the value underneath.
    // The right edge the last availableWidth() handed out: an item that ends there
    // took the room it was given and does not ask for width (see advanceLayout).
    bool textFieldHadKeyboard_ = false;
    mutable float fillEdge_ = 0.0f;
    mutable bool fillEdgeSet_ = false;
    WidgetId numericEditId_ = InvalidWidgetId;
    String numericEditText_;
    double numericEditValue_ = 0.0;
    WidgetId menuWidthId_ = InvalidWidgetId;
    float menuPopupWidth_ = 0.0f;
    float menuMeasuredWidth_ = 0.0f;
    // Same latching, one level down: a submenu's own widest item (Recent
    // Files' longest path, say), tracked separately from the parent menu's
    // menuMeasuredWidth_ above (menuItemInternal already keeps the two
    // apart via subMenuParent_).
    WidgetId subMenuWidthId_ = InvalidWidgetId;
    float subMenuPopupWidth_ = 0.0f;
    float subMenuMeasuredWidth_ = 0.0f;
    Rect activeMenuBounds_;
    Rect subMenuPopupBounds_;
    Rect subMenuParentBounds_;
    // O menu que estava aberto quando o submenu abriu, para o endSubMenu repor. Num
    // menu é o do pai; num popup é nenhum (o submenu foi aberto por um popup, e não
    // por um menu) - sem isto o menu do popup ficava "aberto" depois do submenu
    // fechar, e os itens seguintes passavam a ser desenhados como itens de menu.
    WidgetId subMenuRestoreMenu_;
    float menuBarCursorX_;
    bool menuBarActive_;
    bool forceWindowBounds_;
    TableState table_;
    TableResizeState tableResize_;
    PropertyRowState propertyRow_;
    bool tableActive_;
    bool enabled_;
    bool propertyRowActive_;
    // O valor do início do arrasto é um double: o drag de um tipo de 64 bits (ou
    // de um double) não cabe num float, e é este o valor de que o arrasto parte.
    double dragStartValue_;
    double dragStartX_;
    // O valor no início do arrasto na escala da pista (0..1): é o que faz um drag
    // logarítmico andar depressa em cima e devagar em baixo.
    double dragStartNorm_;
    float tooltipDelay_;
    // Duplo clique: o instante e o sítio da última carregada de cada botão, e o
    // botão cujo clique deste frame foi um duplo (ver isPointerDoubleClicked).
    double lastPressSeconds_[3];
    Vec2 lastPressPosition_[3];
    bool doubleClicked_[3];
    // O intervalo e a distância dentro dos quais duas carregadas são um duplo
    // clique (é o MouseDoubleClickTime/MaxDist do ImGui).
    float doubleClickInterval_;
    float doubleClickDistance_;
    // O atraso do flag Delay do isItemHovered, e o item/tempo/posição do hover em
    // curso (é o que o Stationary compara).
    float hoverDelay_;
    WidgetId hoverDelayWidget_;
    float hoverSeconds_;
    // Quanto tempo o ponteiro está sem se mexer (o Stationary do ImGui): mexe-se
    // dentro do item e volta a zero, mas o Delay (que conta desde que entrou nele)
    // não.
    float hoverStationarySeconds_;
    Vec2 hoverStartPosition_;
    float tooltipHoverSeconds_;
    Vec2 tooltipPointerPosition_;
    WidgetId tooltipWidget_;
    // Rascunho dos plots: os pontos de uma série e os rectângulos de um
    // histograma, reutilizados de frame para frame para não alocar por gráfico.
    ct::Vector<Vec2> plotPoints_;
    ct::Vector<Rect> plotBars_;
    // Tooltip rico em curso (ver beginTooltip): enquanto está aberto, o conteúdo
    // que a aplicação submete vai para uma lista própria (por cima de tudo) e o
    // layout arranca do canto calculado.
    bool tooltipOpen_;
    LayoutState tooltipLayout_;
    Vec2 tooltipOrigin_;
    Vec2 tooltipSize_;
    DrawList tooltipDrawList_;
    // O corpo (e a caixa) de um modal de popup: sai depois das sobreposições de
    // todas as janelas e antes das message boxes (ver beginPopupModal).
    DrawList modalPopupDrawList_;
    DrawList dialogScrimDrawList_;
    bool wantsKeyboard_;
    bool wantsTextInput_;
    bool backspacePressed_;
    bool enterPressed_;
    bool homePressed_;
    bool endPressed_;
    bool upPressed_;
    bool downPressed_;
    bool leftPressed_;
    bool rightPressed_;
    bool pageUpPressed_;
    bool pageDownPressed_;
    bool copyRequested_;
    bool pasteRequested_;
    bool escapePressed_;
    bool tabPressed_;
    bool tabShiftPressed_;
    // Set by a widget (codeEditor) that used this frame's Tab itself
    // (indent, accept an autocomplete suggestion) rather than leaving it for
    // advanceFocus() to move focus with. Reset every frame in consumeEvents.
    bool tabConsumedByWidget_;
    // True while an undo callback is pushing history of its own: undo()/redo()
    // then must not also push the entry the callback just superseded.
    bool historyTouched_ = false;
    bool keyPressed_[32];
    bool keyControl_[32];
    bool keyShift_[32];

    static WidgetId hashText(StringView text);
    static WidgetId combineIds(WidgetId a, WidgetId b);
    WidgetId makeWidgetId(StringView label) const;

    void consumeEvents();
    WindowState *currentWindow();
    const WindowState *currentWindow() const;
    DrawList *currentDrawList();
    // Points a list at the text provider's white texel, so shapes and glyphs
    // share one texture (see DrawList::setSolidStyle).
    void applySolidStyle(DrawList &drawList);
    // Turns the disabled fade on or off on every list this frame can draw into,
    // per the current disabled depth (see beginDisabled).
    void applyDisabledFade();
    TextMetrics measureText(FontId font, StringView text, float logicalSize) const;
    void drawText(DrawList &drawList, FontId font, StringView text, const Vec2 &position,
                  float logicalSize, const Color &color, const Rect &clip);
    void drawDragDropPreview();
    void drawToasts();
    void beginLayout(const WindowState &window);
    void advanceLayout(const Rect &item);
    // A largura mínima de um botão de layout a partir do rótulo já medido (o
    // autoButtonWidth mede-o por dentro; quem já tem as medidas passa-as, para
    // não medir o mesmo texto duas vezes no mesmo frame - ver buttonImpl).
    float autoButtonWidth(const TextMetrics &metrics) const;
    // O corpo do botão, com as medidas do rótulo já feitas: é o que evita a
    // segunda medida no caminho do layout (que mede para se dimensionar e depois
    // mede outra vez para centrar o texto).
    bool buttonImpl(StringView label, const Rect &bounds, const TextMetrics &metrics);
    bool smallButtonImpl(StringView label, const Rect &bounds, const TextMetrics &metrics, float fontSize);
    // hitBounds é a área que responde ao ponteiro: a do desenho por omissão (as
    // versões por bounds), ou a linha toda - a caixa e o rótulo à frente, como o
    // ImGui - nas versões de layout, onde o rótulo é do widget.
    bool checkboxImpl(StringView label, bool &value, const Rect &bounds,
                      const TextMetrics &metrics, const Rect &hitBounds = Rect());
    bool toggleSwitchImpl(StringView label, bool &value, const Rect &bounds,
                          const TextMetrics &metrics, const Rect &hitBounds = Rect());
    bool radioButtonImpl(StringView label, bool selected, const Rect &bounds,
                         const TextMetrics &metrics, const Rect &hitBounds = Rect());
    bool colorButtonImpl(StringView label, const Color &value, const Rect &bounds,
                         const TextMetrics &metrics);
    WindowState *getOrCreateWindow(WidgetId id, StringView title, const Rect &bounds);
    // Move a window onto a stored geometry, and remember it the way
    // restoreWindow would put it back.
    void applyWindowGeometry(WindowState &window, const WindowGeometry &geometry);
    // Keep the title bar where it can be grabbed: the same rule a drag applies,
    // so a file written on a bigger screen cannot open a window nothing of which
    // is reachable.
    void clampWindowToViewport(WindowState &window);
    // Whether the wheel over a scroll area belongs to this area (the innermost
    // one under the pointer last frame); an area that cannot scroll leaves it to
    // the one around it.
    bool ownWheelArea(WidgetId id, const Rect &area, uint32_t depth, bool canScroll);
    bool movingTowardSubMenu(const Rect &parent) const;
    // True when @p point is inside an open popup (menu, combo list) that belongs
    // to the window being drawn: the window receives the pointer there even when
    // the popup reaches outside its bounds.
    bool pointerInsideOwnPopup(const Vec2 &point) const;
    WindowHandle popupOwnerAt(const Vec2 &point) const;
    bool outsideModalPopup() const;
    uint32_t layoutNesting() const;
    // What a popup is clipped to: the screen, not the content of the window it
    // hangs from - a combo list at the bottom of a panel used to be cut off at
    // the window's edge, and its rows were then unclickable.
    Rect popupClipRect() const;
    // Geometry a window is remembered by: a maximized one fills the viewport
    // every frame, so what is worth keeping is the bounds it was maximized from.
    static void windowGeometryOf(const WindowState &window, WindowGeometry &out);
    Rect contentRect(const Rect &local) const;
    Vec2 toContentSpace(const Vec2 &absolute) const;
    Rect contentClip() const;
    // Corpo do collapsingHeader: `open` é nulo quando não há botão de fecho.
    bool collapsingHeaderImpl(StringView label, bool &expanded, bool *open, const Rect &bounds,
                              const TreeStyle &style);
    // Texto como item de layout, na cor pedida, na posição local dada.
    void textItem(StringView text, const Color &color, const Vec2 &localPosition);
    // Parte `text` nas linhas que cabem em wrapWidth; `lines` fica com as vistas
    // para o texto original, que tem de continuar vivo.
    void wrapText(StringView text, float wrapWidth, ct::Vector<StringView> &lines) const;
    // Palavra mais larga que a linha: enche o que couber a partir de `from` (um
    // início de linha), deixa a linha em `lines` e devolve onde ela acabou.
    StringView::size_type wrapLongWord(StringView text, StringView::size_type wordEnd,
                                       StringView::size_type from, float wrapWidth,
                                       ct::Vector<StringView> &lines) const;
    bool itemHovered(const Rect &rect, const Rect &clip, WidgetId id);
    bool itemClicked(const Rect &rect, const Rect &clip, WidgetId id, bool focusable = true);
    bool subItemHovered(const Rect &rect, const Rect &clip, WidgetId id);
    // Um widget composto (o slider com o editor de valor ao lado, o drag com o
    // campo que abre no clique) submete mais do que um sub-item: sem isto, as
    // consultas do item (isItemHovered e companhia) devolviam o último deles - o
    // editor - em vez do widget que quem chama submeteu. `primary` é o estado
    // guardado logo a seguir ao sub-item principal; `bounds` é o rect do widget
    // inteiro.
    void keepItemStatus(const ItemStatus &primary, const Rect &bounds);
    void registerFocusable(WidgetId id);
    void advanceFocus();
    // O teste do ponteiro de um slider, numa vertical ou numa horizontal: é o
    // que o vSlider e o slider (e o fader, por dentro) têm em comum.
    // Um rect sem id como item (o grupo, a barra de menus): o rect e o hover são
    // dele, mas clique/edição/foco não - isso é sempre de um widget lá dentro.
    void setItemHoverRect(const Rect &bounds);
    bool sliderValue(const Rect &rect, const Rect &clip, WidgetId id,
                     double &value, double minimum, double maximum,
                     bool vertical = false, unsigned flags = SliderFlags::None,
                     bool integer = false, bool singlePrecision = false);
    // O tipo do valor de um widget numérico por dentro: real (float ou double) ou
    // inteiro, e de um inteiro a largura e o sinal - é o que faz o -1 de um int sair
    // "FFFFFFFF" num "%X" e voltar a -1 quando se lê.
    struct NumericKind
    {
        bool integer;
        bool singlePrecision;
        int bits;
        bool isSigned;
    };
    static NumericKind realKind(bool singlePrecision) { return {false, singlePrecision, 0, true}; }
    static NumericKind intKind() { return {true, false, 32, true}; }
    static NumericKind scalarKind(ScalarType type);
    // O slider e o drag com o tipo por dentro (ver o comentário em Gui.cpp).
    bool sliderCore(StringView label, double &value, double minimum, double maximum,
                    const NumericKind &kind, const Rect &bounds, unsigned flags,
                    const char *format);
    bool dragCore(StringView label, double &value, double minimum, double maximum, double speed,
                  const NumericKind &kind, const Rect &bounds, unsigned flags,
                  const char *format);
    // Põe o valor que veio do ponteiro ou do campo de texto: arredonda quando o
    // tipo é inteiro e limita ao intervalo quando lhe dizem para limitar.
    bool setScalarValue(double &value, double next, double minimum, double maximum, bool integer,
                        bool clampToRange = true, bool singlePrecision = false);
    // O trabalho dos dois vSlider: precision < 0 escreve o valor como inteiro.
    bool vSliderImpl(StringView label, float &value, float minimum, float maximum,
                     const Rect &bounds, int precision);
    WindowHandle topWindowAt(const Vec2 &position) const;
    // O topWindowAt() do ponteiro, respondido uma vez por frame: por widget era
    // uma passagem por todas as janelas (o currentWindowReceivesPointer pergunta
    // isto a cada widget), e a resposta só muda quando uma janela nasce, muda de
    // sítio ou de ordem - quem faz isso invalida (ver invalidatePointerWindow).
    WindowHandle windowAtPointer() const;
    void invalidatePointerWindow() { pointerWindowValid_ = false; }
    void focusWindow(WindowHandle handle);
    bool currentWindowReceivesPointer() const;
    bool windowBlockedByModal() const;
    bool pointerBlockedByOpenPopup(const Vec2 &point) const;
    void releaseUnsubmittedBlockers();
    bool windowSubmittedRecently(const WindowState &window) const;
    // Widgets of the current window may not take pointer or keyboard input:
    // a messageBox/fileDialog is up, or another window is the modal one.
    bool inputBlockedByModal() const;
    // The single gate for "a press on this widget starts an interaction":
    // pressed this frame inside visible, in the focused window, with no modal
    // or open popup owning that point. Use it instead of reading
    // pointer_.pressed directly.
    bool pointerPressedIn(const Rect &visible, uint32_t button) const;
    // The pointer is over visible and nothing (modal, popup, another window)
    // sits on top of it - for hover and the wheel.
    bool pointerOver(const Rect &visible) const;
    // focusedWidget_ == id, unless a modal owns the keyboard.
    bool hasKeyboardFocus(WidgetId id) const;
    // inputText for a numeric field: keeps the typed text across frames while
    // the field is focused (see numericEditId_). Returns whether the text was
    // edited this frame; text holds what to parse.
    bool editNumericText(StringView label, String &text, double value, const Rect &bounds,
                         unsigned flags = 0u);
    // O campo de texto de um número: o texto sai do formato (formatNumericValue) e
    // volta a número por ele (parseNumericValue). É o que faz um campo "%.10f
    // grams" ter a legenda à frente e um "%04X" aceitar hexadecimal à entrada.
    bool numericField(StringView label, double &value, const NumericKind &kind,
                      const Rect &bounds, const char *format, unsigned flags = 0u);
    // This frame's editing keys and typed text applied to the focused text
    // field's buffer at cursor (a byte offset kept on a UTF-8 code point
    // boundary). Every text field (inputText, inputTextMultiline, the
    // messageBox input, the fileDialog fields) edits through this, so they
    // all get the same keys. Returns whether value changed.
    // Os flags são os do inputText (o filtro do que se escreve e o ReadOnly); o
    // inputTextMultiline e o campo do combo não os usam.
    bool editTextBuffer(String &value, String::size_type &cursor, bool multiline,
                        unsigned flags = 0u);
    // Corpo comum de plotLines/plotHistogram: histogram escolhe barras em vez de
    // segmentos de linha. Recebe um rect local, como os outros widgets de bounds.
    int plotImpl(bool histogram, StringView label, const float *values, int count,
                 const Rect &bounds, int offset, StringView overlay, float scaleMin,
                 float scaleMax);
    Rect dockSlotBounds(const DockSpaceState &dockSpace, DockSlot slot) const;
    bool beginDockSpaceInternal(StringView id, const Rect &outer, const Rect &clip);
    static uint32_t dockSlotIndex(DockSlot slot);
    // Dock arrangement in the state file: the slot a panel was left in, the tab
    // on top of a slot's group, and how the arrangement is written back.
    DockSlot storedDockSlot(StringView spaceName, StringView title, DockSlot fallback) const;
    bool     storedDockTabOnTop(StringView spaceName, DockSlot slot, StringView title) const;
    void     captureDockArrangement(const DockSpaceState &space);
    float tableColumnX(int column) const;
    // Onde a coluna ficaria sem a rolagem horizontal (é o que dá as larguras e o
    // sítio das colunas presas).
    float tableNaturalColumnX(int column) const;
    // Quanto é possível rolar na horizontal (contentWidth - a largura da tabela).
    float tableMaximumOffsetX() const;
    float tableColumnWidth(int column) const;
    float tableMinimumRowHeight() const;
    float tableRowSpacing() const;
    // A coluna do modelo (a que o chamador tem nos dados) por trás de uma posição no
    // écra: a ordem quando a tabela é reordenável, a própria posição quando não é.
    int tableModelColumn(int display) const;
    // Carrega a ordem guardada das colunas da tabela que acabou de abrir (e larga o
    // arrasto que já não é submetido): ver TableStyle::reorderable.
    void configureTable(const TableStyle &style);
    void loadTableOrder(const TableStyle &style);
    // Monta a rolagem da tabela que acabou de abrir (as barras, a roda e o arrasto
    // da barra), a partir da medida do frame anterior: ver TableStyle::scrollY.
    void beginTableScroll(const TableStyle &style);
    // Onde é que a linha atual pode desenhar: com uma tabela que rola, o que rola
    // está cortado pelas linhas presas por cima e pelo fundo da tabela.
    Rect tableRowClip() const;
    Rect tableFrameClip() const;
    // A ordem das colunas da tabela aberta (colunas reordenáveis), ou nullptr quando
    // ela não é reordenável. Aponta para a entrada guardada por id, por isso o
    // arrasto que ocupa vários frames vive lá.
    TableOrderState *tableOrder_;
    // A posição no écra cuja faixa contém o @p x (-1 fora da tabela).
    int tableColumnAtX(float x) const;
    void drawTableRowStyle();
    bool tableHeaderCell(StringView label, int *sortColumn, bool *sortAscending);
    bool menuItemInternal(StringView label, bool enabled, bool *checked,
                          StringView shortcut = StringView());
    bool beginComboImpl(StringView label, StringView preview, const Rect &bounds,
                        int visibleItems, bool advanceHeader);
    static StringView mainMenuBarTitle();
    // O conteúdo de um menu aberto é um contentor de layout, como a janela de um
    // popup do ImGui: um widget de layout submetido lá dentro (slider, child,
    // input, combo) fica dentro do popup em vez de aterrar na janela. Guarda o
    // layout de fora, põe o cursor no popup e empurra o id do menu para o idStack
    // (dois menus não podem partilhar os ids dos widgets).
    void beginPopupContent(const Rect &popupBounds, float insetTop = 0.0f, float padding = 0.0f);
    void endPopupContent();
    bool beginPopupBody(int index, int outer, const Rect &bounds, float insetTop, float padding);
    void measurePopup(PopupEntry &entry);
    // O clip/o largo do contentor do menu em vigor (o do topo da pilha).
    Rect &popupContentClip();
    const Rect &popupContentClip() const;
    float &popupContentWidth();
    float popupContentWidth() const;
    void setPopupContentBounds(const Rect &clip, float width);
    // Um item de menu é um item do layout também: é o que faz o que vem a seguir
    // cair por baixo dele, dentro do popup.
    void syncMenuLayoutItem(const Rect &item);
    // O item de um bloco de escolha múltipla: aplica o que o clique pediu à seleção
    // da aplicação e devolve se o item fica escolhido (ver selectable).
    bool applyMultiSelectClick(bool clicked);
    void drawComboHeader(DrawList &drawList, const Rect &rect, StringView preview,
                         bool highlighted, const Rect &clip);
    // Um triângulo virado para onde o direction pede, centrado em `area` e com o
    // lado menor de um quarto dela (é o que o arrowButton e as setas desenhadas
    // à mão partilham).
    static void drawArrowGlyph(DrawList &drawList, const Rect &area, ArrowDirection direction,
                               const Color &color, const Rect &clip);
    // O deslocamento vertical que o alignTextToFramePadding() pediu para o próximo
    // texto (0 quando ninguém o pediu). Consome o pedido.
    float textFrameOffset(const TextMetrics &metrics);
    float comboPopupMaxHeight(int visibleItems) const;
    void drawWindow(WindowState &window);
    static uint32_t buttonIndex(PointerButton button);
    // fontSize is the editor's effective size (theme_.fontSize * the
    // CodeEditorState's zoom), never the theme's own fontSize directly - a
    // zoomed editor must scale every one of these consistently or the
    // caret/selection/click math drifts away from what is drawn.
    void drawHighlightedLine(DrawList &drawList, const CodeEditorState &state,
                             int line, const String &text, const Vec2 &position, const Rect &clip,
                             int tabSize, float fontSize);
    // Draws one run of plain text, honoring embedded '\t' bytes by advancing
    // to the next tab stop (tabSize columns, in space-widths) instead of
    // asking the font for a glyph it doesn't have. startVisCol is the
    // column the run starts at, needed to land on the correct tab stop when
    // a run doesn't start at column 0. Returns the x position past the run.
    float drawCodeRun(DrawList &drawList, const StringView &run, const Vec2 &position,
                      const Color &color, const Rect &clip, int tabSize, int startVisCol, float fontSize);
    // Pixel X offset of a column on a line, measuring the real glyph widths
    // of the prefix up to it - never assume a fixed per-character width,
    // the font is not guaranteed to be monospaced. A '\t' byte advances to
    // the next tab stop (tabSize columns, in space-widths) rather than
    // being measured as a glyph.
    float codeEditorColumnX(const CodeEditorState &state, int line, int column, int tabSize, float fontSize) const;
    // Inverse of codeEditorColumnX: the column whose prefix width is closest
    // to localX (measured from the start of the line, i.e. already relative
    // to the text area's left edge + padding).
    int codeEditorColumnAt(const CodeEditorState &state, int line, float localX, int tabSize, float fontSize) const;
    void updateCodeCompletion(CodeEditorState &state, const CodeEditorOptions &options);
    void acceptCodeCompletion(CodeEditorState &state);
};

} // namespace ig
