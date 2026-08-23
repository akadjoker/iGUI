#include "pch.hpp"
#include "ComboBox.hpp"
#include "WidgetApp.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  ComboPopup_ — internal popup widget
// ─────────────────────────────────────────────────────────────────────────────

class ComboPopup_ : public Widget
{
public:
    ComboPopup_(ComboBox* owner, const std::vector<std::string>& items,
                int selected, int maxVis, float rowH)
        : owner_(owner), items_(items), selected_(selected), rowHeight_(rowH)
    {
        acceptsFocus_ = true;
        scrollable_   = true;
        (void)maxVis;
    }

    void paint(PaintContext& ctx) override
    {
        Rect abs = absoluteRect();
        const auto& t = Theme::instance();

        // Background + border — draw over the full popup rect, ignoring the
        // inherited clip so the solid background covers whatever is underneath
        // (important when the popup opens upward over other widgets).
        ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, 255);
        ctx.fill.RoundedRectangle(static_cast<int>(abs.x), static_cast<int>(abs.y),
                                   static_cast<int>(abs.w), static_cast<int>(abs.h),
                                   t.borderRadius, 6, true);
        ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g, t.inputBorderHover.b, 255);
        ctx.line.Rectangle(static_cast<int>(abs.x), static_cast<int>(abs.y),
                           static_cast<int>(abs.w), static_cast<int>(abs.h), false);

        ctx.pushClip(abs);
        int n     = static_cast<int>(items_.size());
        int first = std::max(0, static_cast<int>(scrollOff_ / rowHeight_));
        int last  = std::min(n, static_cast<int>((scrollOff_ + abs.h) / rowHeight_) + 1);

        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetBatch(&ctx.text);
        float asc = ctx.font.GetAscender();

        for (int i = first; i < last; ++i)
        {
            float ry = abs.y + i * rowHeight_ - scrollOff_;
            Rect rowR = {abs.x, ry, abs.w, rowHeight_};

            bool isHov = (i == hovered_);
            bool isSel = (i == selected_);
            if (isHov || isSel)
            {
                Rect rc;
                if (ctx.clipRectIntersect(rowR, rc))
                {
                    Color c = isSel ? t.selectionColor : Color(t.buttonHover.r, t.buttonHover.g, t.buttonHover.b, 120);
                    ctx.fill.SetColor(c.r, c.g, c.b, c.a);
                    ctx.fill.Rectangle(static_cast<int>(rc.x), static_cast<int>(rc.y),
                                       static_cast<int>(rc.w), static_cast<int>(rc.h), true);
                }
            }

            ctx.font.SetColor(t.textColor);
            float ty = ry + (rowHeight_ + asc) * 0.5f;
            Rect textClipR{abs.x + t.padding, abs.y, abs.w - t.padding * 2, abs.h};
            ctx.pushClip(textClipR);
            ctx.font.Print(items_[i].c_str(), abs.x + t.padding, ty);
            ctx.popClip();
        }
        ctx.popClip();
    }

    void onMousePress(MouseEvent& e) override
    {
        Rect abs = absoluteRect();
        float localY = e.y - abs.y + scrollOff_;
        int idx = static_cast<int>(localY / rowHeight_);
        if (idx >= 0 && idx < static_cast<int>(items_.size()))
            owner_->setSelectedIndex(idx);
        owner_->closeDropdown();
        e.consumed = true;
    }

    void onMouseMove(MouseEvent& e) override
    {
        Rect abs = absoluteRect();
        float localY = e.y - abs.y + scrollOff_;
        int idx = static_cast<int>(localY / rowHeight_);
        if (idx < 0 || idx >= static_cast<int>(items_.size())) idx = -1;
        if (idx != hovered_) { hovered_ = idx; markDirty(); }
    }

    void onMouseScroll(MouseEvent& e) override
    {
        float totalH = items_.size() * rowHeight_;
        float viewH  = rect_.h;
        if (totalH <= viewH) return;
        scrollOff_ -= e.scrollY * rowHeight_ * 0.5f;
        scrollOff_ = std::max(0.0f, std::min(scrollOff_, totalH - viewH));
        e.consumed = true;
        markDirty();
    }

    void onMouseLeave() override { hovered_ = -1; markDirty(); }

    bool popupContains(float x, float y) const override
    {
        Rect abs = absoluteRect();
        return abs.contains(x, y);
    }

private:
    ComboBox* owner_;
    std::vector<std::string> items_;
    int   selected_  = -1;
    int   hovered_   = -1;
    float rowHeight_ = 24.0f;
    float scrollOff_ = 0.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ComboBox
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kEmptyCombo;

ComboBox::ComboBox()
{
    acceptsFocus_ = true;
    setCursor(CursorType::Hand);
}

void ComboBox::addItem(const std::string& text)     { items_.push_back(text); markDirty(); }

void ComboBox::insertItem(int index, const std::string& text)
{
    if (index < 0) index = 0;
    if (index > static_cast<int>(items_.size())) index = static_cast<int>(items_.size());
    items_.insert(items_.begin() + index, text);
    if (selectedIndex_ >= index) ++selectedIndex_;
    markDirty();
}

void ComboBox::removeItem(int index)
{
    if (index < 0 || index >= static_cast<int>(items_.size())) return;
    items_.erase(items_.begin() + index);
    if (selectedIndex_ == index)  { selectedIndex_ = -1; selectionChanged.emit(-1); }
    else if (selectedIndex_ > index) --selectedIndex_;
    markDirty();
}

void ComboBox::clear() { items_.clear(); selectedIndex_ = -1; markDirty(); }

const std::string& ComboBox::itemText(int index) const
{
    if (index >= 0 && index < static_cast<int>(items_.size())) return items_[index];
    return kEmptyCombo;
}

void ComboBox::setItemText(int index, const std::string& text)
{
    if (index >= 0 && index < static_cast<int>(items_.size())) { items_[index] = text; markDirty(); }
}

const std::string& ComboBox::currentText() const { return itemText(selectedIndex_); }

void ComboBox::setSelectedIndex(int idx)
{
    if (idx < -1) idx = -1;
    if (idx >= static_cast<int>(items_.size())) idx = static_cast<int>(items_.size()) - 1;
    if (selectedIndex_ != idx) { selectedIndex_ = idx; selectionChanged.emit(idx); markDirty(); }
}

Widget::Vec2f ComboBox::sizeHint() const
{
    const auto& t = Theme::instance();
    float w = 100.0f;
    for (auto& it : items_)
    {
        float tw = it.size() * t.fontSize * 0.6f + t.padding * 3 + 16;
        if (tw > w) w = tw;
    }
    return {w, t.inputHeight};
}

void ComboBox::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    Rect clipped;

    // Background
    if (ctx.clipRectIntersect(abs, clipped))
    {
        Color bg = pressed_ ? t.buttonPressed : (hovered_ ? t.buttonHover : t.buttonNormal);
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, true);

        Color brd = focused_ ? t.focusColor : (hovered_ ? t.inputBorderHover : t.buttonBorder);
        ctx.line.SetColor(brd.r, brd.g, brd.b, brd.a);
        ctx.line.Rectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                           static_cast<int>(clipped.w), static_cast<int>(clipped.h), false);
    }

    // Selected text
    if (overrideFont_) ctx.pushFont(overrideFont_);
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();
    float ty  = abs.y + (abs.h + asc) * 0.5f;

    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(items_.size()))
    {
        Rect textClipR{abs.x + t.padding, abs.y, abs.w - t.padding * 2 - 16, abs.h};
        ctx.font.SetColor(t.textColor);
        ctx.pushClip(textClipR);
        ctx.font.Print(items_[selectedIndex_].c_str(), abs.x + t.padding, ty);
        ctx.popClip();
    }
    else if (!items_.empty())
    {
        // Placeholder
        Rect textClipR{abs.x + t.padding, abs.y, abs.w - t.padding * 2 - 16, abs.h};
        ctx.font.SetColor(t.textDisabled);
        ctx.pushClip(textClipR);
        ctx.font.Print("Select...", abs.x + t.padding, ty);
        ctx.popClip();
    }

    // Chevron ▼
    float arrowSz = 5.0f;
    float ax = abs.x + abs.w - t.padding - arrowSz * 2 - 2;
    float ay = abs.y + abs.h * 0.5f - arrowSz * 0.35f;
    ctx.line.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
    ctx.line.Line2D(static_cast<int>(ax),               static_cast<int>(ay),
                    static_cast<int>(ax + arrowSz),      static_cast<int>(ay + arrowSz));
    ctx.line.Line2D(static_cast<int>(ax + arrowSz),     static_cast<int>(ay + arrowSz),
                    static_cast<int>(ax + arrowSz * 2),  static_cast<int>(ay));
    if (overrideFont_) ctx.popFont();
}

void ComboBox::onMousePress(MouseEvent& e)
{
    if (!enabled_) return;
    if (open_) closeDropdown(); else openDropdown();
    e.consumed = true;
}

void ComboBox::openDropdown()
{
    if (open_ || items_.empty()) return;
    open_ = true;

    Rect abs = absoluteRect();
    float rowH = Theme::instance().inputHeight;
    int   vis  = std::min(static_cast<int>(items_.size()), maxVisible_);
    float popH = vis * rowH + 2;

    auto* popup = new ComboPopup_(this, items_, selectedIndex_, maxVisible_, rowH);

    // Open upward if there is not enough space below. Clamp horizontally too.
    const auto& io = BuGUI::GetIO();
    float popX = abs.x;
    float popY = (abs.y + abs.h + popH > io.displayHeight && abs.y - popH >= 0.0f)
                 ? abs.y - popH
                 : abs.y + abs.h;
    if (popY + popH > io.displayHeight) popY = io.displayHeight - popH;
    if (popY < 0.f) popY = 0.f;
    if (popX + abs.w > io.displayWidth)  popX = io.displayWidth  - abs.w;
    if (popX < 0.f) popX = 0.f;
    popup->setRect({popX, popY, abs.w, popH});
    WidgetApp::instance().showPopup(popup, this);
    markDirty();
}

void ComboBox::closeDropdown()
{
    if (!open_) return;
    open_ = false;
    WidgetApp::instance().closePopup();
    markDirty();
}
