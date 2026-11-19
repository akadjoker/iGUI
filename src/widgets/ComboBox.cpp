#include "ComboBox.hpp"
#include "WidgetApp.hpp"
#include "pch.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  ComboPopup_ — internal popup widget
// ─────────────────────────────────────────────────────────────────────────────

namespace ig
{
namespace retained
{

class ComboPopup_ : public Widget
{
public:
    ComboPopup_(ComboBox* owner, const ct::Vector<String>& items, int selected, int maxVis,
            float rowH)
            : owner_(owner),
              items_(items),
              selected_(selected),
              rowHeight_(rowH)
    {
        acceptsFocus_ = true;
        scrollable_ = true;
        (void) maxVis;
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
                static_cast<int>(abs.w), static_cast<int>(abs.h), t.borderRadius, 6, true);
        ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g, t.inputBorderHover.b, 255);
        ctx.line.Rectangle(static_cast<int>(abs.x), static_cast<int>(abs.y),
                static_cast<int>(abs.w), static_cast<int>(abs.h), false);

        ctx.pushClip(abs);
        int n = static_cast<int>(items_.size());
        int first = std::max(0, static_cast<int>(scrollOff_ / rowHeight_));
        int last = std::min(n, static_cast<int>((scrollOff_ + abs.h) / rowHeight_) + 1);

        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetBatch(&ctx.text);
        float asc = ctx.font.GetAscender();

        for (int i = first; i < last; ++i)
        {
            float ry = abs.y + i * rowHeight_ - scrollOff_;
            Rect rowR = { abs.x, ry, abs.w, rowHeight_ };

            bool isHov = (i == hovered_);
            bool isSel = (i == selected_);
            if (isHov || isSel)
            {
                Rect rc;
                if (ctx.clipRectIntersect(rowR, rc))
                {
                    Color c = isSel ? t.selectionColor
                                    : Color(t.buttonHover.r, t.buttonHover.g, t.buttonHover.b, 120);
                    ctx.fill.SetColor(c.r, c.g, c.b, c.a);
                    ctx.fill.Rectangle(static_cast<int>(rc.x), static_cast<int>(rc.y),
                            static_cast<int>(rc.w), static_cast<int>(rc.h), true);
                }
            }

            ctx.font.SetColor(isSel ? t.selectionText : t.textColor);
            float ty = ry + (rowHeight_ + asc) * 0.5f;
            Rect textClipR{ abs.x + t.padding, abs.y, abs.w - t.padding * 2, abs.h };
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
        // The handler of the selection may take the box out of the tree, which closes this popup and, outside a frame, deletes
        // it: nothing of the popup is read after the call (WidgetApp::hasGone).
        ComboBox* owner = owner_;
        WidgetApp& app = WidgetApp::instance();
        const uint32_t seen = app.deletions();
        e.consumed = true;
        if (idx >= 0 && idx < static_cast<int>(items_.size())) owner->setSelectedIndex(idx);
        if (app.hasGone(owner, seen)) return;
        owner->closeDropdown();
    }

    void onMouseMove(MouseEvent& e) override
    {
        Rect abs = absoluteRect();
        float localY = e.y - abs.y + scrollOff_;
        int idx = static_cast<int>(localY / rowHeight_);
        if (idx < 0 || idx >= static_cast<int>(items_.size())) idx = -1;
        if (idx != hovered_)
        {
            hovered_ = idx;
            markDirty();
        }
    }

    void onMouseScroll(MouseEvent& e) override
    {
        float totalH = items_.size() * rowHeight_;
        float viewH = rect_.h;
        if (totalH <= viewH) return;
        scrollOff_ -= e.scrollY * rowHeight_ * 0.5f;
        scrollOff_ = std::max(0.0f, std::min(scrollOff_, totalH - viewH));
        e.consumed = true;
        markDirty();
    }

    void onMouseLeave() override
    {
        hovered_ = -1;
        markDirty();
    }

    bool popupContains(float x, float y) const override
    {
        Rect abs = absoluteRect();
        return abs.contains(x, y);
    }

private:
    ComboBox* owner_;
    ct::Vector<String> items_;
    int selected_ = -1;
    int hovered_ = -1;
    float rowHeight_ = 24.0f;
    float scrollOff_ = 0.0f;
};

} // namespace retained
} // namespace ig

// ─────────────────────────────────────────────────────────────────────────────
//  ComboBox
// ─────────────────────────────────────────────────────────────────────────────

static const String kEmptyCombo;

ComboBox::ComboBox()
{
    acceptsFocus_ = true;
    setCursor(CursorType::Hand);
}

// The width sizeHint counts for an item: as the font measures it (the estimate of 0.6 em a letter where there is no font).
float ComboBox::itemWidth(const String& item) const
{
    float tw = item.empty() ? 0.0f : WidgetApp::instance().textWidth(item.c_str(), overrideFont_);
    if (!(tw > 0.0f)) tw = item.size() * Theme::instance().fontSize * 0.6f;
    return tw;
}

// The width sizeHint asks for when the widest item is `widest` wide.
float ComboBox::hintWidth(float widest) const
{
    const float w = widest + Theme::instance().padding * 3 + 16;
    return w < 100.0f ? 100.0f : w;
}

// Whether the widest item is remembered for this font and text height (sizeHint() has worked it out and nothing since
// has made it unknown). While it is not, the layout has not asked for the hint yet, or the items have changed too much to
// follow: a change asks for a layout pass, and sizeHint() measures when the layout does.
bool ComboBox::widestKnown() const
{
    const ig::retained::Font* font = overrideFont_ ? overrideFont_ : WidgetApp::instance().font();
    return widestItem_ >= 0.0f && widestFont_ == font && widestHeight_ == Theme::instance().fontSize;
}

// An item left (width `leaving`) and one arrived (`arriving`), a negative width for none, both measured only when `known`
// (widestKnown() before the change): asks for a layout pass only when the width of the hint changes, which it does not
// when the widest item stays the same (the common case of a list that is filled or renamed). When the widest item may
// have left it asks, and sizeHint() measures again. A list that nothing has laid out yet is not measured here: it is the
// layout that measures, in the order of the tree.
void ComboBox::itemsChanged(bool known, float leaving, float arriving)
{
    if (!known || leaving >= widestItem_)
    {
        widestItem_ = -1.0f;
        invalidateLayout();
        return;
    }
    const float before = hintWidth(widestItem_);
    if (arriving > widestItem_) widestItem_ = arriving;
    if (hintWidth(widestItem_) != before) invalidateLayout();
}

void ComboBox::addItem(const String& text)
{
    const bool known = widestKnown();
    items_.push_back(text);
    markDirty();
    itemsChanged(known, -1.0f, known ? itemWidth(text) : -1.0f);
}

void ComboBox::insertItem(int index, const String& text)
{
    if (index < 0) index = 0;
    if (index > static_cast<int>(items_.size())) index = static_cast<int>(items_.size());
    const bool known = widestKnown();
    items_.insert(items_.begin() + index, text);
    if (selectedIndex_ >= index) ++selectedIndex_;
    markDirty();
    itemsChanged(known, -1.0f, known ? itemWidth(text) : -1.0f);
}

void ComboBox::removeItem(int index)
{
    if (index < 0 || index >= static_cast<int>(items_.size())) return;
    const bool known = widestKnown();
    const float leaving = known ? itemWidth(items_[index]) : -1.0f;
    items_.erase(items_.begin() + index);
    const bool lostSelection = selectedIndex_ == index;
    if (lostSelection) selectedIndex_ = -1;
    else if (selectedIndex_ > index)
        --selectedIndex_;
    markDirty();
    itemsChanged(known, leaving, -1.0f);
    if (lostSelection) selectionChanged.emit(-1); // (last: a handler may take the box out of the tree, see WidgetApp::hasGone)
}

void ComboBox::clear()
{
    const bool had = !items_.empty();
    items_.clear();
    selectedIndex_ = -1;
    widestItem_ = -1.0f;
    markDirty();
    if (had) invalidateLayout();
}

const String& ComboBox::itemText(int index) const
{
    if (index >= 0 && index < static_cast<int>(items_.size())) return items_[index];
    return kEmptyCombo;
}

void ComboBox::setItemText(int index, const String& text)
{
    if (index >= 0 && index < static_cast<int>(items_.size()))
    {
        if (items_[index] == text) return;
        const bool known = widestKnown();
        const float leaving = known ? itemWidth(items_[index]) : -1.0f;
        items_[index] = text;
        markDirty();
        itemsChanged(known, leaving, known ? itemWidth(text) : -1.0f);
    }
}

const String& ComboBox::currentText() const
{
    return itemText(selectedIndex_);
}

void ComboBox::setSelectedIndex(int idx)
{
    if (idx < -1) idx = -1;
    if (idx >= static_cast<int>(items_.size())) idx = static_cast<int>(items_.size()) - 1;
    if (selectedIndex_ != idx)
    {
        selectedIndex_ = idx;
        markDirty();
        selectionChanged.emit(idx); // (last: a handler may take the box out of the tree, see WidgetApp::hasGone)
    }
}

Widget::Vec2f ComboBox::sizeHint() const
{
    const auto& t = Theme::instance();
    // the widest item as the font measures it (the estimate of 0.6 em a letter where there is no font), remembered
    const ig::retained::Font* font = overrideFont_ ? overrideFont_ : WidgetApp::instance().font();
    if (widestItem_ < 0.0f || widestFont_ != font || widestHeight_ != t.fontSize)
    {
        float widest = 0.0f;
        for (auto& it: items_)
        {
            const float tw = itemWidth(it);
            if (tw > widest) widest = tw;
        }
        widestItem_ = widest;
        widestFont_ = font;
        widestHeight_ = t.fontSize;
    }
    return { hintWidth(widestItem_), t.inputHeight };
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
                static_cast<int>(clipped.w), static_cast<int>(clipped.h), t.borderRadius, 6, true);

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
    float ty = abs.y + (abs.h + asc) * 0.5f;

    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(items_.size()))
    {
        Rect textClipR{ abs.x + t.padding, abs.y, abs.w - t.padding * 2 - 16, abs.h };
        ctx.font.SetColor(t.textColor);
        ctx.pushClip(textClipR);
        ctx.font.Print(items_[selectedIndex_].c_str(), abs.x + t.padding, ty);
        ctx.popClip();
    }
    else if (!items_.empty())
    {
        // Placeholder
        Rect textClipR{ abs.x + t.padding, abs.y, abs.w - t.padding * 2 - 16, abs.h };
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
    ctx.line.Line2D(static_cast<int>(ax), static_cast<int>(ay), static_cast<int>(ax + arrowSz),
            static_cast<int>(ay + arrowSz));
    ctx.line.Line2D(static_cast<int>(ax + arrowSz), static_cast<int>(ay + arrowSz),
            static_cast<int>(ax + arrowSz * 2), static_cast<int>(ay));
    if (overrideFont_) ctx.popFont();
}

void ComboBox::onMousePress(MouseEvent& e)
{
    if (!isEnabledInTree()) return;
    if (isOpen()) closeDropdown();
    else
        openDropdown();
    e.consumed = true;
}

void ComboBox::openDropdown()
{
    if (isOpen() || items_.empty()) return;

    Rect abs = absoluteRect();
    float rowH = Theme::instance().inputHeight;
    int vis = std::min(static_cast<int>(items_.size()), maxVisible_);
    float popH = vis * rowH + 2;

    auto* popup = new ComboPopup_(this, items_, selectedIndex_, maxVisible_, rowH);

    // Open upward if there is not enough space below. Clamp horizontally too.
    const auto& io = ig::retained::GetIO();
    float popX = abs.x;
    float popY = (abs.y + abs.h + popH > io.displayHeight && abs.y - popH >= 0.0f) ? abs.y - popH
                                                                                   : abs.y + abs.h;
    if (popY + popH > io.displayHeight) popY = io.displayHeight - popH;
    if (popY < 0.f) popY = 0.f;
    if (popX + abs.w > io.displayWidth) popX = io.displayWidth - abs.w;
    if (popX < 0.f) popX = 0.f;
    popup->setRect({ popX, popY, abs.w, popH });
    WidgetApp::instance().showPopup(popup, this);
    markDirty();
}

void ComboBox::closeDropdown()
{
    if (!isOpen()) return;
    WidgetApp::instance().closePopup();
    markDirty();
}

bool ComboBox::isOpen() const
{
    return WidgetApp::instance().popupOwner() == this;
}
