#include "pch.hpp"
#include "DialogWidgets.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "TextInputWidgets.hpp"
#include "WidgetApp.hpp"
#include <algorithm>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
//  Dialog
// ═════════════════════════════════════════════════════════════════════════════

Dialog::Dialog(const std::string& title, const std::string& message)
    : title_(title), message_(message)
{
    acceptsFocus_ = true;
    scrollable_   = false;
}

Dialog& Dialog::addButton(const std::string& label, Role role,
                           std::function<void()> action)
{
    buttons_.push_back({label, role, std::move(action)});
    return *this;
}

void Dialog::show()
{
    Vec2f sz = sizeHint();
    float sw = static_cast<float>(WidgetApp::instance().width());
    float sh = static_cast<float>(WidgetApp::instance().height());
    setPosition((sw - sz.x) * 0.5f, (sh - sz.y) * 0.5f);
    setSize(sz.x, sz.y);
    WidgetApp::instance().showPopup(this, nullptr, true);
}

void Dialog::close()
{
    WidgetApp::instance().closePopup();
    if (onClose_) onClose_();
}

// ── Geometry helpers ──────────────────────────────────────────────────────────

float Dialog::dialogH_() const
{
    const auto& t = Theme::instance();
    // Title + top-padding + message lines + button-row + bottom-padding
    // Approximate message height: split by \n
    int lines = 1;
    for (char c : message_) if (c == '\n') ++lines;
    float msgH = lines * (t.fontSize + 4.0f) + kMsgPad * 2;
    return kTitleH + msgH + kPad + kBtnH + kPad;
}

Rect Dialog::dialogRect_() const
{
    // rect_ is set by show() to the screen-centred position
    return {rect_.x, rect_.y, kWidth, dialogH_()};
}

Rect Dialog::btnRect_(int idx) const
{
    const Rect dr = dialogRect_();
    int n = static_cast<int>(buttons_.size());
    if (n == 0) return {};

    float totalW = n * kBtnW + (n - 1) * 8.0f;
    float startX = dr.x + dr.w - kPad - totalW;
    float y      = dr.y + dr.h - kPad - kBtnH;

    return {startX + idx * (kBtnW + 8.0f), y, kBtnW, kBtnH};
}

Widget::Vec2f Dialog::sizeHint() const
{
    return {kWidth, dialogH_()};
}

int Dialog::hitButton_(float x, float y) const
{
    for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
    {
        Rect r = btnRect_(i);
        if (r.contains(x, y)) return i;
    }
    return -1;
}

// ── popupContains ─────────────────────────────────────────────────────────────

bool Dialog::popupContains(float x, float y) const
{
    // Always returns true — clicks on scrim are handled inside onMousePress
    (void)x; (void)y;
    return true;
}

// ── Layout ────────────────────────────────────────────────────────────────────

void Dialog::layout()
{
    // Position is set by show(); children (if any) fit inside dialog rect
    Rect dr = dialogRect_();
    setSize(dr.w, dr.h);
}

// ── Events ────────────────────────────────────────────────────────────────────

void Dialog::onMousePress(MouseEvent& e)
{
    int idx = hitButton_(e.x, e.y);
    if (idx >= 0 && idx < static_cast<int>(buttons_.size()))
    {
        auto action = buttons_[idx].action;
        close();
        if (action) action();
    }
    // clicks on scrim or dialog background: do nothing (dialog stays open)
    e.consumed = true;
}

void Dialog::onKeyPress(KeyEvent& e)
{
    // Escape → cancel (find Cancel button, or just close)
    if (e.key == BuGUI::Key::Escape)
    {
        for (auto& b : buttons_)
            if (b.role == Role::Cancel)
            {
                auto action = b.action;
                close();
                if (action) action();
                e.consumed = true;
                return;
            }
        close();
        e.consumed = true;
    }
    // Enter → accept
    else if (e.key == BuGUI::Key::Return)
    {
        for (auto& b : buttons_)
            if (b.role == Role::Accept)
            {
                auto action = b.action;
                close();
                if (action) action();
                e.consumed = true;
                return;
            }
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void Dialog::paint(PaintContext& ctx)
{
    // Update hover from current mouse position
    hoveredBtn_ = hitButton_(WidgetApp::instance().mouseX(),
                              WidgetApp::instance().mouseY());

    if (!visible_) return;
    const auto& t = Theme::instance();

    float sw = static_cast<float>(WidgetApp::instance().width());
    float sh = static_cast<float>(WidgetApp::instance().height());

    // Scrim
    ctx.fill.SetColor(t.dialogScrim.r, t.dialogScrim.g,
                      t.dialogScrim.b, t.dialogScrim.a);
    ctx.fill.Rectangle(0, 0, sw, sh, true);

    Rect dr = dialogRect_();

    // Dialog background
    ctx.fill.SetColor(t.dialogBg.r, t.dialogBg.g, t.dialogBg.b, 255);
    ctx.fill.RoundedRectangle(dr.x, dr.y, dr.w, dr.h, t.borderRadius * 2, 8, true);

    // Title bar
    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, 255);
    ctx.fill.RoundedRectangle(dr.x, dr.y, dr.w, kTitleH, t.borderRadius * 2, 8, true);
    ctx.fill.Rectangle(dr.x, dr.y + kTitleH - 6, dr.w, 6, true);

    // Border
    ctx.line.SetColor(t.dialogBorder.r, t.dialogBorder.g, t.dialogBorder.b, 255);
    ctx.line.RoundedRectangle(dr.x, dr.y, dr.w, dr.h, t.borderRadius * 2, 8, false);

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();

    // Title text
    ctx.font.SetColor(t.dialogTitleText);
    ctx.font.Print(title_.c_str(), dr.x + kPad, dr.y + (kTitleH + asc) * 0.5f);

    // Message
    ctx.pushClip({dr.x + kPad, dr.y + kTitleH, dr.w - kPad * 2,
                  dr.h - kTitleH - kPad - kBtnH - kPad});
    {
        ctx.font.SetColor(t.dialogText);
        float ly   = dr.y + kTitleH + kMsgPad + asc;
        float lineH = t.fontSize + 4.0f;
        size_t start = 0;
        while (start <= message_.size())
        {
            size_t end = message_.find('\n', start);
            if (end == std::string::npos) end = message_.size();
            ctx.font.Print(message_.substr(start, end - start).c_str(),
                           dr.x + kPad, ly);
            ly    += lineH;
            start  = end + 1;
        }
    }
    ctx.popClip();

    // Buttons
    for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
    {
        const auto& b  = buttons_[i];
        Rect         br = btnRect_(i);
        bool         hov = (i == hoveredBtn_);

        Color bg;
        switch (b.role) {
            case Role::Accept: bg = hov ? t.dialogBtnPrimaryHover : t.dialogBtnPrimary; break;
            case Role::Danger: bg = hov ? t.dialogBtnDangerHover  : t.dialogBtnDanger;  break;
            default:           bg = hov ? t.dialogBtnHover        : t.dialogBtnBg;      break;
        }
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.RoundedRectangle(br.x, br.y, br.w, br.h, t.borderRadius, 6, true);

        ctx.font.SetColor(t.textColor);
        float btw = ctx.font.GetTextWidth(b.label.c_str());
        float bty = br.y + (br.h + asc) * 0.5f;
        ctx.font.Print(b.label.c_str(), br.x + (br.w - btw) * 0.5f, bty);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  AlertDialog
// ═════════════════════════════════════════════════════════════════════════════

AlertDialog::AlertDialog(const std::string& title, const std::string& message)
    : Dialog(title, message)
{
    addButton("OK", Role::Accept);
}

/*static*/ void AlertDialog::show(const std::string& title,
                                    const std::string& message,
                                    std::function<void()> onClose)
{
    auto* dlg = new AlertDialog(title, message);
    if (onClose) dlg->setOnClose(std::move(onClose));
    dlg->Dialog::show();
}

// ═════════════════════════════════════════════════════════════════════════════
//  ConfirmDialog
// ═════════════════════════════════════════════════════════════════════════════

ConfirmDialog::ConfirmDialog(const std::string& title, const std::string& message,
                              std::function<void()> onConfirm,
                              std::function<void()> onCancel)
    : Dialog(title, message)
{
    addButton("Cancel", Role::Cancel, std::move(onCancel));
    addButton("Confirm", Role::Accept, std::move(onConfirm));
}

/*static*/ void ConfirmDialog::show(const std::string& title,
                                     const std::string& message,
                                     std::function<void()> onConfirm,
                                     std::function<void()> onCancel)
{
    auto* dlg = new ConfirmDialog(title, message,
                                   std::move(onConfirm), std::move(onCancel));
    dlg->Dialog::show();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Toast — static, no Widget, no popup
// ═════════════════════════════════════════════════════════════════════════════

/*static*/ std::vector<Toast::Entry>& Toast::entries()
{
    static std::vector<Entry> s_entries;
    return s_entries;
}

/*static*/ void Toast::show(const std::string& message, Type type, float duration)
{
    Entry e;
    e.message  = message;
    e.type     = type;
    e.lifetime = duration > 0.0f ? duration : kDefaultDuration;
    e.elapsed  = 0.0f;
    e.alpha    = 0.0f;
    auto& v = entries();
    // Cap at 5 visible toasts — drop oldest when over limit
    if (v.size() >= 5)
        v.erase(v.begin());
    v.push_back(std::move(e));
}

/*static*/ bool Toast::hasActive()
{
    return !entries().empty();
}

/*static*/ void Toast::tick(float dt)
{
    auto& v = entries();
    for (auto& e : v) {
        e.elapsed += dt;
        if (e.elapsed < kFadeIn)
            e.alpha = e.elapsed / kFadeIn;
        else if (e.elapsed < e.lifetime - kFadeOut)
            e.alpha = 1.0f;
        else if (e.elapsed < e.lifetime)
            e.alpha = (e.lifetime - e.elapsed) / kFadeOut;
        else
            e.alpha = 0.0f;
    }
    v.erase(std::remove_if(v.begin(), v.end(),
        [](const Entry& e) { return e.elapsed >= e.lifetime; }), v.end());
}

/*static*/ void Toast::paint(PaintContext& ctx, float screenW, float screenH)
{
    auto& v = entries();
    if (v.empty()) return;

    const auto& th = Theme::instance();
    constexpr float kPadL  = 18.0f;   // left of text (after accent bar)
    constexpr float kPadR  = 16.0f;   // right padding
    constexpr float kMinW  = 200.0f;
    constexpr float toastH = 36.0f;
    constexpr float gap    = 6.0f;
    float baseY = screenH - 40.0f;

    ctx.font.SetFontSize(th.fontSize * 0.9f);
    ctx.font.SetBatch(&ctx.text);

    for (int i = static_cast<int>(v.size()) - 1; i >= 0; --i) {
        auto& e = v[i];
        int a = static_cast<int>(e.alpha * 230.0f);
        if (a <= 0) continue;

        // Measure text and compute width to fit
        float tw    = ctx.font.GetTextWidth(e.message.c_str());
        float toastW = tw + kPadL + kPadR;
        if (toastW < kMinW)  toastW = kMinW;
        if (toastW > screenW * 0.85f) toastW = screenW * 0.85f;

        float tx = (screenW - toastW) * 0.5f;
        if (tx < 8.0f) tx = 8.0f;  // never go off-screen left
        float ty = baseY - static_cast<float>(static_cast<int>(v.size()) - 1 - i) * (toastH + gap);

        // Slide up during fade-in
        if (e.elapsed < kFadeIn) {
            float slideT = e.elapsed / kFadeIn;
            ty += (1.0f - slideT) * 20.0f;
        }

        // Background
        Color bg;
        switch (e.type) {
        case Type::Info:    bg = Color( 50,  60,  80, a); break;
        case Type::Success: bg = Color( 30,  80,  50, a); break;
        case Type::Warning: bg = Color( 90,  75,  20, a); break;
        case Type::Error:   bg = Color( 90,  30,  30, a); break;
        }
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.RoundedRectangle(
            static_cast<int>(tx), static_cast<int>(ty),
            static_cast<int>(toastW), static_cast<int>(toastH),
            6, 8, true);

        // Border
        ctx.line.SetColor(255, 255, 255, a / 4);
        ctx.line.RoundedRectangle(
            static_cast<int>(tx), static_cast<int>(ty),
            static_cast<int>(toastW), static_cast<int>(toastH),
            6, 8, false);

        // Left accent bar
        Color bar;
        switch (e.type) {
        case Type::Info:    bar = Color( 80, 140, 220, a); break;
        case Type::Success: bar = Color( 60, 200, 100, a); break;
        case Type::Warning: bar = Color(230, 180,  40, a); break;
        case Type::Error:   bar = Color(220,  60,  60, a); break;
        }
        ctx.fill.SetColor(bar.r, bar.g, bar.b, bar.a);
        ctx.fill.RoundedRectangle(
            static_cast<int>(tx), static_cast<int>(ty),
            4, static_cast<int>(toastH),
            2, 4, true);

        // Text — clipped to toast bounds so it never overflows
        ctx.font.SetColor(Color(240, 240, 245, a));
        float textY = ty + (toastH - th.fontSize * 0.9f) * 0.5f;
        ctx.pushClip({tx + 4.0f, ty, toastW - 8.0f, toastH});
        ctx.font.Print(e.message.c_str(), tx + kPadL, textY + ctx.font.GetAscender());
        ctx.popClip();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  MessageBox
// ═════════════════════════════════════════════════════════════════════════════

MessageBox::MessageBox(const std::string& title, const std::string& message,
                       Buttons buttons, Type type)
    : FloatWindow(title)
    , type_(type)
    , buttons_(buttons)
    , message_(message)
{
    setResizable(false);
    setMinimizable(false);
    buildUI();

    float fw = 400, fh = 180;
    setFloatSize(fw, fh);
    auto& io = BuGUI::GetIO();
    setFloatPos((io.displayWidth - fw) * 0.5f, (io.displayHeight - fh) * 0.5f);
}

void MessageBox::buildUI()
{
    mainLayout_ = setContent<BoxLayout>(LayoutDir::Vertical);
    mainLayout_->setSpacing(8);
    mainLayout_->setPadding(12);

    // Body: icon + message
    auto* body = mainLayout_->createChild<BoxLayout>(LayoutDir::Horizontal);
    body->setStretch(1);
    body->setSpacing(12);

    msgLabel_ = body->createChild<Label>(message_);
    msgLabel_->setStretch(1);

    // Buttons
    auto* btnRow = mainLayout_->createChild<BoxLayout>(LayoutDir::Horizontal);
    btnRow->setSize(0, 32);
    btnRow->setSpacing(8);
    btnRow->setMainAlign(MainAlign::End);

    switch (buttons_) {
    case Ok: {
        auto* ok = btnRow->createChild<Button>("OK");
        ok->setSize(80, 0);
        ok->clicked.connect([this] { close(ResultOk); });
        break;
    }
    case OkCancel: {
        auto* cancel = btnRow->createChild<Button>("Cancel");
        cancel->setSize(80, 0);
        cancel->clicked.connect([this] { close(ResultCancel); });
        auto* ok = btnRow->createChild<Button>("OK");
        ok->setSize(80, 0);
        ok->clicked.connect([this] { close(ResultOk); });
        break;
    }
    case YesNo: {
        auto* no = btnRow->createChild<Button>("No");
        no->setSize(80, 0);
        no->clicked.connect([this] { close(ResultNo); });
        auto* yes = btnRow->createChild<Button>("Yes");
        yes->setSize(80, 0);
        yes->clicked.connect([this] { close(ResultYes); });
        break;
    }
    case YesNoCancel: {
        auto* cancel = btnRow->createChild<Button>("Cancel");
        cancel->setSize(80, 0);
        cancel->clicked.connect([this] { close(ResultCancel); });
        auto* no = btnRow->createChild<Button>("No");
        no->setSize(80, 0);
        no->clicked.connect([this] { close(ResultNo); });
        auto* yes = btnRow->createChild<Button>("Yes");
        yes->setSize(80, 0);
        yes->clicked.connect([this] { close(ResultYes); });
        break;
    }
    }
}

void MessageBox::close(Result r)
{
    result.emit(r);
    WidgetApp::instance().removeFloat(this);
}

void MessageBox::onKeyPress(KeyEvent& e)
{
    if (e.key == BuGUI::Key::Escape) {
        e.consumed = true;
        if (buttons_ == Ok) close(ResultOk);
        else close(ResultCancel);
        return;
    }
    if (e.key == BuGUI::Key::Return || e.key == BuGUI::Key::KPEnter) {
        e.consumed = true;
        if (buttons_ == YesNo || buttons_ == YesNoCancel) close(ResultYes);
        else close(ResultOk);
        return;
    }
    FloatWindow::onKeyPress(e);
}

// ═════════════════════════════════════════════════════════════════════════════
//  InputBox
// ═════════════════════════════════════════════════════════════════════════════

InputBox::InputBox(const std::string& title, const std::string& prompt)
    : FloatWindow(title)
    , prompt_(prompt)
{
    setResizable(false);
    setMinimizable(false);
    buildUI();

    float fw = 400, fh = 170;
    setFloatSize(fw, fh);
    auto& io = BuGUI::GetIO();
    setFloatPos((io.displayWidth - fw) * 0.5f, (io.displayHeight - fh) * 0.5f);
}

void InputBox::buildUI()
{
    mainLayout_ = setContent<BoxLayout>(LayoutDir::Vertical);
    mainLayout_->setSpacing(8);
    mainLayout_->setPadding(12);

    if (!prompt_.empty())
        promptLabel_ = mainLayout_->createChild<Label>(prompt_);

    textInput_ = mainLayout_->createChild<TextInput>();
    textInput_->setStretch(0);
    textInput_->setSize(0, 28);

    auto* spacer = mainLayout_->createChild<Widget>();
    spacer->setStretch(1);

    auto* btnRow = mainLayout_->createChild<BoxLayout>(LayoutDir::Horizontal);
    btnRow->setSize(0, 32);
    btnRow->setSpacing(8);
    btnRow->setMainAlign(MainAlign::End);

    auto* cancel = btnRow->createChild<Button>("Cancel");
    cancel->setSize(80, 0);
    cancel->clicked.connect([this] { onCancel(); });

    auto* ok = btnRow->createChild<Button>("OK");
    ok->setSize(80, 0);
    ok->clicked.connect([this] { onOk(); });
}

void InputBox::setText(const std::string& t)
{
    if (textInput_) textInput_->setText(t);
}

std::string InputBox::text() const
{
    return textInput_ ? textInput_->text() : "";
}

void InputBox::setPlaceholder(const std::string& p)
{
    if (textInput_) textInput_->setPlaceholder(p);
}

void InputBox::onOk()
{
    accepted.emit(text());
    WidgetApp::instance().removeFloat(this);
}

void InputBox::onCancel()
{
    cancelled.emit();
    WidgetApp::instance().removeFloat(this);
}

void InputBox::onKeyPress(KeyEvent& e)
{
    if (e.key == BuGUI::Key::Escape)  { e.consumed = true; onCancel(); return; }
    if (e.key == BuGUI::Key::Return || e.key == BuGUI::Key::KPEnter) { e.consumed = true; onOk(); return; }
    FloatWindow::onKeyPress(e);
}

