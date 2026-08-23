#include "ConsoleWidget.hpp"
#include <cstdarg>

// ═════════════════════════════════════════════════════════════════════════════
//  Helpers
// ═════════════════════════════════════════════════════════════════════════════

namespace {

inline float setupFont(PaintContext& ctx, const Color& color, float size = 14.f)
{
    ctx.font.SetFontSize(size);
    ctx.font.SetBatch(&ctx.text);
    ctx.font.SetColor(color);
    return ctx.font.GetAscender();
}

inline bool containsCI(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower((unsigned char)a) ==
                                     std::tolower((unsigned char)b);
                          });
    return it != haystack.end();
}

} // anon

// ═════════════════════════════════════════════════════════════════════════════
//  Constructor
// ═════════════════════════════════════════════════════════════════════════════

ConsoleWidget::ConsoleWidget()
{
    acceptsFocus_ = true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Logging API
// ═════════════════════════════════════════════════════════════════════════════

void ConsoleWidget::log(LogLevel level, const std::string& message)
{
    float ts = BuGUI::GetIO().deltaTime; // approximate — callers can set real ts

    if (collapse_ && !entries_.empty()) {
        auto& last = entries_.back();
        if (last.level == level && last.message == message) {
            last.count++;
            markDirty();
            return;
        }
    }

    entries_.push_back({level, message, ts, 1});
    updateCount(level, 1);
    trimEntries();

    if (autoScroll_ && !scrollLocked_) {
        // Will snap to bottom in paint
    }
    markDirty();
}

void ConsoleWidget::logf(LogLevel level, const char* fmt, ...)
{
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    log(level, buf);
}

void ConsoleWidget::clear()
{
    entries_.clear();
    countTrace_ = countInfo_ = countWarn_ = countError_ = 0;
    scrollY_ = 0;
    scrollLocked_ = false;
    selectedLine_ = -1;
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Filter
// ═════════════════════════════════════════════════════════════════════════════

void ConsoleWidget::setFilter(LogLevel level, bool show)
{
    switch (level) {
    case LogLevel::Trace: showTrace_ = show; break;
    case LogLevel::Info:  showInfo_  = show; break;
    case LogLevel::Warn:  showWarn_  = show; break;
    case LogLevel::Error: showError_ = show; break;
    }
    markDirty();
}

bool ConsoleWidget::filter(LogLevel level) const
{
    switch (level) {
    case LogLevel::Trace: return showTrace_;
    case LogLevel::Info:  return showInfo_;
    case LogLevel::Warn:  return showWarn_;
    case LogLevel::Error: return showError_;
    }
    return true;
}

void ConsoleWidget::setSearchText(const std::string& s)
{
    searchText_ = s;
    markDirty();
}

bool ConsoleWidget::passesFilter(const LogEntry& e) const
{
    switch (e.level) {
    case LogLevel::Trace: if (!showTrace_) return false; break;
    case LogLevel::Info:  if (!showInfo_)  return false; break;
    case LogLevel::Warn:  if (!showWarn_)  return false; break;
    case LogLevel::Error: if (!showError_) return false; break;
    }
    if (!searchText_.empty() && !containsCI(e.message, searchText_))
        return false;
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Internal
// ═════════════════════════════════════════════════════════════════════════════

void ConsoleWidget::trimEntries()
{
    while ((int)entries_.size() > maxLines_) {
        updateCount(entries_.front().level, -entries_.front().count);
        entries_.erase(entries_.begin());
    }
}

void ConsoleWidget::updateCount(LogLevel l, int delta)
{
    switch (l) {
    case LogLevel::Trace: countTrace_ += delta; break;
    case LogLevel::Info:  countInfo_  += delta; break;
    case LogLevel::Warn:  countWarn_  += delta; break;
    case LogLevel::Error: countError_ += delta; break;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Colors
// ═════════════════════════════════════════════════════════════════════════════

Color ConsoleWidget::levelColor(LogLevel l) const
{
    switch (l) {
    case LogLevel::Trace: return {140, 140, 150, 255};
    case LogLevel::Info:  return {200, 210, 220, 255};
    case LogLevel::Warn:  return {240, 200,  80, 255};
    case LogLevel::Error: return {240,  80,  80, 255};
    }
    return {200, 200, 200, 255};
}

Color ConsoleWidget::levelBgColor(LogLevel l) const
{
    switch (l) {
    case LogLevel::Trace: return {0, 0, 0, 0};
    case LogLevel::Info:  return {0, 0, 0, 0};
    case LogLevel::Warn:  return {60, 50, 20, 40};
    case LogLevel::Error: return {80, 25, 25, 50};
    }
    return {0, 0, 0, 0};
}

const char* ConsoleWidget::levelTag(LogLevel l) const
{
    switch (l) {
    case LogLevel::Trace: return "[TRC]";
    case LogLevel::Info:  return "[INF]";
    case LogLevel::Warn:  return "[WRN]";
    case LogLevel::Error: return "[ERR]";
    }
    return "[???]";
}

// ═════════════════════════════════════════════════════════════════════════════
//  Layout
// ═════════════════════════════════════════════════════════════════════════════

void ConsoleWidget::layout()
{
    Widget::layout();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Paint
// ═════════════════════════════════════════════════════════════════════════════

void ConsoleWidget::paint(PaintContext& ctx)
{
    Rect r = absoluteRect();
    ctx.pushClip(r);

    // Background
    ctx.fill.SetColor(bgColor_);
    ctx.fillRect(r.x, r.y, r.w, r.h);

    // Sub-areas
    Rect toolbar = {r.x, r.y, r.w, kToolbarH};
    Rect input   = {r.x, r.bottom() - kInputH, r.w, kInputH};
    Rect logArea = {r.x, r.y + kToolbarH, r.w, r.h - kToolbarH - kInputH};

    paintToolbar(ctx, toolbar);
    paintLog(ctx, logArea);
    paintInput(ctx, input);

    ctx.popClip();

    // Don't call Widget::paint for children — console is self-contained
}

// ─── Toolbar ─────────────────────────────────────────────────────────────────

void ConsoleWidget::paintToolbar(PaintContext& ctx, const Rect& area)
{
    // Toolbar bg
    ctx.fill.SetColor(30, 32, 38, 255);
    ctx.fillRect(area.x, area.y, area.w, area.h);

    // Separator line
    ctx.line.SetColor(50, 52, 60, 255);
    ctx.drawLine(area.x, area.bottom(), area.right(), area.bottom());

    float asc = setupFont(ctx, {180, 180, 190, 255}, 12.f);
    float px = area.x + 6.f;
    float cy = area.y + (area.h - 12.f) * 0.5f;

    // Filter toggle buttons: [T] [I] [W] [E]
    struct FilterBtn {
        const char* label;
        bool*       active;
        Color       onColor;
        int         count;
    };
    FilterBtn btns[] = {
        {"T", &showTrace_, {140, 140, 150, 255}, countTrace_},
        {"I", &showInfo_,  {120, 180, 255, 255}, countInfo_},
        {"W", &showWarn_,  {240, 200,  80, 255}, countWarn_},
        {"E", &showError_, {240,  80,  80, 255}, countError_},
    };

    for (auto& b : btns) {
        float bw = 32.f;
        float bh = 18.f;
        float by = area.y + (area.h - bh) * 0.5f;

        // Button bg
        if (*b.active) {
            ctx.fill.SetColor(b.onColor.r, b.onColor.g, b.onColor.b, 40);
        } else {
            ctx.fill.SetColor(40, 42, 48, 255);
        }
        ctx.fillRoundedRect(px, by, bw, bh, 3.f);

        // Label + count
        char buf[32];
        snprintf(buf, sizeof(buf), "%s%d", b.label, b.count);
        Color tc = *b.active ? b.onColor : Color(80, 80, 90, 255);
        setupFont(ctx, tc, 11.f);
        float tw = ctx.font.GetTextWidth(buf);
        ctx.font.Print(buf, px + (bw - tw) * 0.5f, by + (bh - 11.f) * 0.5f + ctx.font.GetAscender());

        px += bw + 4.f;
    }

    // Clear button
    {
        float bw = 40.f, bh = 18.f;
        float by = area.y + (area.h - bh) * 0.5f;
        ctx.fill.SetColor(50, 52, 60, 255);
        ctx.fillRoundedRect(px, by, bw, bh, 3.f);
        setupFont(ctx, {160, 160, 170, 255}, 11.f);
        float tw = ctx.font.GetTextWidth("Clear");
        ctx.font.Print("Clear", px + (bw - tw) * 0.5f, by + (bh - 11.f) * 0.5f + ctx.font.GetAscender());
        px += bw + 8.f;
    }

    // Search box
    if (searchMode_) {
        float sw = area.right() - px - 6.f;
        if (sw > 40.f) {
            float bh = 18.f;
            float by = area.y + (area.h - bh) * 0.5f;
            ctx.fill.SetColor(20, 22, 26, 255);
            ctx.fillRoundedRect(px, by, sw, bh, 3.f);
            ctx.line.SetColor(70, 120, 200, 255);
            ctx.lineRoundedRect(px, by, sw, bh, 3.f);

            setupFont(ctx, {200, 210, 220, 255}, 12.f);
            std::string disp = searchBuf_;
            if (disp.empty()) {
                setupFont(ctx, {80, 80, 90, 255}, 12.f);
                disp = "Search...";
            }
            ctx.pushClip({px + 4.f, by, sw - 8.f, bh});
            ctx.font.Print(disp.c_str(), px + 4.f, by + (bh - 12.f) * 0.5f + ctx.font.GetAscender());
            ctx.popClip();
        }
    }
}

// ─── Log area ────────────────────────────────────────────────────────────────

void ConsoleWidget::paintLog(PaintContext& ctx, const Rect& area)
{
    ctx.pushClip(area);

    // Count visible entries
    int visCount = 0;
    for (auto& e : entries_)
        if (passesFilter(e)) visCount++;

    float totalH = visCount * kLineH;

    // Auto-scroll: snap to bottom when not locked
    if (autoScroll_ && !scrollLocked_) {
        float maxScroll = std::max(0.f, totalH - area.h);
        scrollY_ = maxScroll;
    }

    // Clamp scroll
    float maxScroll = std::max(0.f, totalH - area.h);
    if (scrollY_ > maxScroll) scrollY_ = maxScroll;
    if (scrollY_ < 0) scrollY_ = 0;

    float y = area.y - scrollY_;
    int visIdx = 0;

    for (int i = 0; i < (int)entries_.size(); i++) {
        auto& e = entries_[i];
        if (!passesFilter(e)) continue;

        float lineY = y + visIdx * kLineH;
        visIdx++;

        // Skip if above or below viewport
        if (lineY + kLineH < area.y) continue;
        if (lineY > area.bottom()) break;

        // Row bg for warn/error levels
        Color bg = levelBgColor(e.level);
        if (bg.a > 0) {
            ctx.fill.SetColor(bg);
            ctx.fillRect(area.x, lineY, area.w, kLineH);
        }

        // Selection highlight
        if (visIdx - 1 == selectedLine_) {
            ctx.fill.SetColor(50, 70, 120, 80);
            ctx.fillRect(area.x, lineY, area.w, kLineH);
        }

        float px = area.x + 6.f;
        float textY = lineY + (kLineH - 12.f) * 0.5f;

        // Timestamp
        if (showTimestamp_) {
            char ts[16];
            snprintf(ts, sizeof(ts), "%.2f", e.timestamp);
            setupFont(ctx, {90, 90, 100, 255}, 11.f);
            ctx.font.Print(ts, px, textY + ctx.font.GetAscender());
            px += 50.f;
        }

        // Level tag
        setupFont(ctx, levelColor(e.level), 12.f);
        ctx.font.Print(levelTag(e.level), px, textY + ctx.font.GetAscender());
        px += ctx.font.GetTextWidth(levelTag(e.level)) + 6.f;

        // Message
        setupFont(ctx, levelColor(e.level), 12.f);
        ctx.font.Print(e.message.c_str(), px, textY + ctx.font.GetAscender());

        // Collapse badge
        if (e.count > 1) {
            char badge[16];
            snprintf(badge, sizeof(badge), "x%d", e.count);
            float msgW = ctx.font.GetTextWidth(e.message.c_str());
            float bx = px + msgW + 8.f;
            setupFont(ctx, {100, 140, 200, 255}, 10.f);
            float bw = ctx.font.GetTextWidth(badge) + 8.f;
            float bh = 14.f;
            float by = lineY + (kLineH - bh) * 0.5f;
            ctx.fill.SetColor(40, 60, 100, 120);
            ctx.fillRoundedRect(bx, by, bw, bh, 3.f);
            ctx.font.Print(badge, bx + 4.f, by + (bh - 10.f) * 0.5f + ctx.font.GetAscender());
        }
    }

    // Scrollbar
    if (totalH > area.h) {
        paintScrollbar(ctx, area, totalH);
    }

    ctx.popClip();
}

// ─── Scrollbar ───────────────────────────────────────────────────────────────

void ConsoleWidget::paintScrollbar(PaintContext& ctx, const Rect& logArea, float totalH)
{
    float sbW = 8.f;
    float sbX = logArea.right() - sbW;
    float ratio = logArea.h / totalH;
    float thumbH = std::max(20.f, logArea.h * ratio);
    float maxScroll = totalH - logArea.h;
    float t = (maxScroll > 0) ? scrollY_ / maxScroll : 0.f;
    float thumbY = logArea.y + t * (logArea.h - thumbH);

    // Track
    ctx.fill.SetColor(25, 27, 32, 200);
    ctx.fillRect(sbX, logArea.y, sbW, logArea.h);

    // Thumb
    ctx.fill.SetColor(70, 75, 85, 180);
    ctx.fillRoundedRect(sbX + 1.f, thumbY, sbW - 2.f, thumbH, 3.f);
}

// ─── Input bar ───────────────────────────────────────────────────────────────

void ConsoleWidget::paintInput(PaintContext& ctx, const Rect& area)
{
    // Background
    ctx.fill.SetColor(26, 28, 34, 255);
    ctx.fillRect(area.x, area.y, area.w, area.h);

    // Top separator
    ctx.line.SetColor(50, 52, 60, 255);
    ctx.drawLine(area.x, area.y, area.right(), area.y);

    // Prompt
    float px = area.x + 6.f;
    float textY = area.y + (area.h - 13.f) * 0.5f;
    setupFont(ctx, {100, 160, 240, 255}, 13.f);
    ctx.font.Print(">", px, textY + ctx.font.GetAscender());
    px += 14.f;

    // Input text
    setupFont(ctx, {200, 210, 220, 255}, 13.f);
    float asc = ctx.font.GetAscender();
    ctx.pushClip({px, area.y, area.w - 20.f, area.h});
    ctx.font.Print(inputBuf_.c_str(), px, textY + asc);

    // Cursor blink
    if (inputFocused_) {
        float cw = 0;
        if (cursorPos_ > 0 && cursorPos_ <= (int)inputBuf_.size()) {
            std::string sub = inputBuf_.substr(0, cursorPos_);
            cw = ctx.font.GetTextWidth(sub.c_str());
        }
        ctx.fill.SetColor(180, 200, 240, 200);
        ctx.fillRect(px + cw, area.y + 4.f, 1.5f, area.h - 8.f);
    }
    ctx.popClip();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Mouse events
// ═════════════════════════════════════════════════════════════════════════════

void ConsoleWidget::onMousePress(MouseEvent& e)
{
    Rect r = absoluteRect();
    Rect toolbar = {r.x, r.y, r.w, kToolbarH};
    Rect input   = {r.x, r.bottom() - kInputH, r.w, kInputH};
    Rect logArea = {r.x, r.y + kToolbarH, r.w, r.h - kToolbarH - kInputH};

    // Click in toolbar — check filter buttons & clear
    if (toolbar.contains(e.x, e.y)) {
        float px = r.x + 6.f;
        float bh = 18.f;
        float by = toolbar.y + (toolbar.h - bh) * 0.5f;

        bool* filters[] = {&showTrace_, &showInfo_, &showWarn_, &showError_};
        for (int i = 0; i < 4; i++) {
            float bw = 32.f;
            Rect btn = {px, by, bw, bh};
            if (btn.contains(e.x, e.y)) {
                *filters[i] = !*filters[i];
                markDirty();
                e.consumed = true;
                return;
            }
            px += bw + 4.f;
        }

        // Clear button
        {
            float bw = 40.f;
            Rect btn = {px, by, bw, bh};
            if (btn.contains(e.x, e.y)) {
                clear();
                e.consumed = true;
                return;
            }
        }
        e.consumed = true;
        return;
    }

    // Click in log area — select line
    if (logArea.contains(e.x, e.y)) {
        float relY = e.y - logArea.y + scrollY_;
        int line = (int)(relY / kLineH);

        // Map to visible index
        int visCount = 0;
        for (auto& en : entries_)
            if (passesFilter(en)) visCount++;

        if (line >= 0 && line < visCount)
            selectedLine_ = line;
        else
            selectedLine_ = -1;

        inputFocused_ = false;
        setFocused(true);
        markDirty();
        e.consumed = true;
        return;
    }

    // Click in input area — focus input
    if (input.contains(e.x, e.y)) {
        inputFocused_ = true;
        setFocused(true);

        // Rough cursor placement
        float px = r.x + 20.f;
        float clickX = e.x - px;
        if (clickX < 0) clickX = 0;

        // Binary-ish search for cursor pos
        int best = (int)inputBuf_.size();
        for (int i = 0; i <= (int)inputBuf_.size(); i++) {
            std::string sub = inputBuf_.substr(0, i);
            // Need a temp context but we don't have one — just set to end
        }
        cursorPos_ = best;

        markDirty();
        e.consumed = true;
        return;
    }
}

void ConsoleWidget::onMouseRelease(MouseEvent& e)
{
    e.consumed = true;
}

void ConsoleWidget::onMouseScroll(MouseEvent& e)
{
    Rect r = absoluteRect();
    Rect logArea = {r.x, r.y + kToolbarH, r.w, r.h - kToolbarH - kInputH};

    if (!logArea.contains(e.x, e.y)) return;

    scrollY_ -= e.scrollY * kLineH * 3.f;

    // Count visible
    int visCount = 0;
    for (auto& en : entries_)
        if (passesFilter(en)) visCount++;

    float totalH = visCount * kLineH;
    float maxScroll = std::max(0.f, totalH - logArea.h);

    if (scrollY_ < 0) scrollY_ = 0;
    if (scrollY_ > maxScroll) scrollY_ = maxScroll;

    // Determine if user is at bottom
    scrollLocked_ = (scrollY_ < maxScroll - 1.f);

    markDirty();
    e.consumed = true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Key events
// ═════════════════════════════════════════════════════════════════════════════

void ConsoleWidget::onKeyPress(KeyEvent& e)
{
    // Ctrl+F — toggle search
    if (e.ctrl && e.key == BuGUI::Key::F) {
        searchMode_ = !searchMode_;
        if (!searchMode_) {
            searchBuf_.clear();
            searchText_.clear();
        }
        inputFocused_ = !searchMode_;
        markDirty();
        e.consumed = true;
        return;
    }

    // Ctrl+C — copy selected line
    if (e.ctrl && e.key == BuGUI::Key::C) {
        if (selectedLine_ >= 0) {
            int vis = 0;
            for (auto& en : entries_) {
                if (!passesFilter(en)) continue;
                if (vis == selectedLine_) {
                    auto& io = BuGUI::GetIO();
                    if (io.setClipboardText)
                        io.setClipboardText(en.message.c_str());
                    break;
                }
                vis++;
            }
        }
        e.consumed = true;
        return;
    }

    // Ctrl+L — clear
    if (e.ctrl && e.key == BuGUI::Key::L) {
        clear();
        e.consumed = true;
        return;
    }

    // Escape — clear search or deselect
    if (e.key == BuGUI::Key::Escape) {
        if (searchMode_) {
            searchMode_ = false;
            searchBuf_.clear();
            searchText_.clear();
        } else {
            selectedLine_ = -1;
        }
        markDirty();
        e.consumed = true;
        return;
    }

    // If search mode, route keys to search buffer
    if (searchMode_ && !inputFocused_) {
        if (e.key == BuGUI::Key::Backspace) {
            if (!searchBuf_.empty()) searchBuf_.pop_back();
            searchText_ = searchBuf_;
            markDirty();
            e.consumed = true;
            return;
        }
        if (e.key == BuGUI::Key::Return) {
            searchMode_ = false;
            inputFocused_ = true;
            markDirty();
            e.consumed = true;
            return;
        }
        // Text handled in onTextInput
        e.consumed = true;
        return;
    }

    // Input focused — command line keys
    if (inputFocused_) {
        switch (e.key) {
        case BuGUI::Key::Return: case BuGUI::Key::KPEnter:
            submitCommand();
            e.consumed = true;
            return;

        case BuGUI::Key::Backspace:
            if (cursorPos_ > 0 && !inputBuf_.empty()) {
                inputBuf_.erase(cursorPos_ - 1, 1);
                cursorPos_--;
                markDirty();
            }
            e.consumed = true;
            return;

        case BuGUI::Key::Delete:
            if (cursorPos_ < (int)inputBuf_.size()) {
                inputBuf_.erase(cursorPos_, 1);
                markDirty();
            }
            e.consumed = true;
            return;

        case BuGUI::Key::Left:
            if (cursorPos_ > 0) { cursorPos_--; markDirty(); }
            e.consumed = true;
            return;

        case BuGUI::Key::Right:
            if (cursorPos_ < (int)inputBuf_.size()) { cursorPos_++; markDirty(); }
            e.consumed = true;
            return;

        case BuGUI::Key::Home:
            cursorPos_ = 0;
            markDirty();
            e.consumed = true;
            return;

        case BuGUI::Key::End:
            cursorPos_ = (int)inputBuf_.size();
            markDirty();
            e.consumed = true;
            return;

        case BuGUI::Key::Up:
            historyUp();
            e.consumed = true;
            return;

        case BuGUI::Key::Down:
            historyDown();
            e.consumed = true;
            return;
        }
    } else {
        // Not in input — arrow keys scroll log / move selection
        if (e.key == BuGUI::Key::Up) {
            if (selectedLine_ > 0) selectedLine_--;
            markDirty();
            e.consumed = true;
            return;
        }
        if (e.key == BuGUI::Key::Down) {
            int visCount = 0;
            for (auto& en : entries_)
                if (passesFilter(en)) visCount++;
            if (selectedLine_ < visCount - 1) selectedLine_++;
            markDirty();
            e.consumed = true;
            return;
        }
        // Tab to focus input
        if (e.key == BuGUI::Key::Tab) {
            inputFocused_ = true;
            markDirty();
            e.consumed = true;
            return;
        }
    }
}

void ConsoleWidget::onTextInput(KeyEvent& e)
{
    if (e.text[0] == '\0') return;

    if (searchMode_ && !inputFocused_) {
        searchBuf_ += e.text;
        searchText_ = searchBuf_;
        markDirty();
        e.consumed = true;
        return;
    }

    if (inputFocused_) {
        std::string ins(e.text);
        inputBuf_.insert(cursorPos_, ins);
        cursorPos_ += (int)ins.size();
        markDirty();
        e.consumed = true;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Command history
// ═════════════════════════════════════════════════════════════════════════════

void ConsoleWidget::submitCommand()
{
    if (inputBuf_.empty()) return;

    // Add to history
    if (history_.empty() || history_.back() != inputBuf_)
        history_.push_back(inputBuf_);
    historyIdx_ = -1;

    // Log it as Info
    log(LogLevel::Info, "> " + inputBuf_);

    // Emit signal
    std::string cmd = inputBuf_;
    inputBuf_.clear();
    cursorPos_ = 0;
    markDirty();

    onCommand.emit(cmd);
}

void ConsoleWidget::historyUp()
{
    if (history_.empty()) return;
    if (historyIdx_ < 0)
        historyIdx_ = (int)history_.size() - 1;
    else if (historyIdx_ > 0)
        historyIdx_--;

    inputBuf_ = history_[historyIdx_];
    cursorPos_ = (int)inputBuf_.size();
    markDirty();
}

void ConsoleWidget::historyDown()
{
    if (history_.empty() || historyIdx_ < 0) return;
    historyIdx_++;
    if (historyIdx_ >= (int)history_.size()) {
        historyIdx_ = -1;
        inputBuf_.clear();
    } else {
        inputBuf_ = history_[historyIdx_];
    }
    cursorPos_ = (int)inputBuf_.size();
    markDirty();
}
