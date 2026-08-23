#pragma once

#include "Widget.hpp"
#include "Signal.hpp"

// ═════════════════════════════════════════════════════════════════════════════
//  ConsoleWidget — full-featured developer console / log viewer
//
//    auto* con = parent->createChild<ConsoleWidget>();
//    con->log(LogLevel::Info,  "Server started on port 8080");
//    con->log(LogLevel::Warn,  "Texture not found: grass.png");
//    con->log(LogLevel::Error, "Shader compile failed");
//    con->onCommand.connect([](const std::string& cmd) { eval(cmd); });
//
//  Features:
//    - Log levels: Trace, Info, Warn, Error with per-level color + filter
//    - Toolbar: level toggle buttons, clear, search field, entry counts
//    - Auto-scroll with sticky bottom (pauses on manual scroll-up)
//    - Command input with history (Up/Down arrows)
//    - Text search / highlight (Ctrl+F or type in search box)
//    - Max line cap (oldest entries evicted)
//    - Copy selected line(s) to clipboard (Ctrl+C)
//    - Timestamps (optional, toggleable)
//    - Scrollbar
// ═════════════════════════════════════════════════════════════════════════════

namespace BuGUI
{
enum class LogLevel { Trace, Info, Warn, Error };

struct LogEntry {
    LogLevel    level     = LogLevel::Info;
    std::string message;
    float       timestamp = 0;    // seconds since app start
    int         count     = 1;    // collapse duplicate consecutive messages
};

class ConsoleWidget : public Widget
{
public:
    ConsoleWidget();

    /// @brief Log a message at a given level.
    void log(LogLevel level, const std::string& message);
    /// @brief Log a printf-style formatted message.
    void logf(LogLevel level, const char* fmt, ...);
    /// @brief Clear all log entries.
    void clear();

    /// @brief Set maximum log entries (oldest evicted).
    void setMaxLines(int n)          { maxLines_ = n; trimEntries(); }
    /// @brief Get the max line count.
    int  maxLines() const            { return maxLines_; }

    /// @brief Show or hide timestamps.
    void setShowTimestamp(bool v)    { showTimestamp_ = v; markDirty(); }
    /// @brief Check if timestamps are shown.
    bool showTimestamp() const       { return showTimestamp_; }

    /// @brief Enable auto-scroll to bottom on new entries.
    void setAutoScroll(bool v)       { autoScroll_ = v; }
    /// @brief Check if auto-scroll is enabled.
    bool autoScroll() const          { return autoScroll_; }

    /// @brief Collapse consecutive duplicate messages.
    void setCollapseDuplicates(bool v) { collapse_ = v; }
    /// @brief Check if duplicate collapsing is enabled.
    bool collapseDuplicates() const    { return collapse_; }

    /// @brief Show or hide a log level in the output.
    void setFilter(LogLevel level, bool show);
    /// @brief Check if a log level is visible.
    bool filter(LogLevel level) const;
    /// @brief Set text search/highlight filter.
    void setSearchText(const std::string& s);
    /// @brief Get the current search filter text.
    const std::string& searchText() const { return searchText_; }

    /// @brief Set the console background color.
    void setBgColor(const Color& c) { bgColor_ = c; }

    /// @brief Emitted when a command is submitted.
    Signal<const std::string&> onCommand;

    // ── Overrides ────────────────────────────────────────────────────────
    Vec2f sizeHint() const override { return {400, 250}; }
    void paint(PaintContext& ctx) override;
    void layout() override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;
    void onTextInput(KeyEvent& e) override;

private:
    // ── Entries ──────────────────────────────────────────────────────────
    std::vector<LogEntry> entries_;
    int maxLines_ = 10000;
    bool collapse_      = true;
    bool showTimestamp_  = false;
    bool autoScroll_     = true;

    void trimEntries();

    // ── Filter state ─────────────────────────────────────────────────────
    bool showTrace_ = true;
    bool showInfo_  = true;
    bool showWarn_  = true;
    bool showError_ = true;
    std::string searchText_;

    bool passesFilter(const LogEntry& e) const;

    // ── Counts ───────────────────────────────────────────────────────────
    int countTrace_ = 0, countInfo_ = 0, countWarn_ = 0, countError_ = 0;
    void updateCount(LogLevel l, int delta);

    // ── Scroll ───────────────────────────────────────────────────────────
    float scrollY_       = 0;
    bool  scrollLocked_  = false;  // user scrolled up manually

    // ── Selection ────────────────────────────────────────────────────────
    int  selectedLine_ = -1;

    // ── Command input ────────────────────────────────────────────────────
    std::string inputBuf_;
    bool        inputFocused_ = false;
    int         cursorPos_    = 0;

    // ── Command history ──────────────────────────────────────────────────
    std::vector<std::string> history_;
    int                      historyIdx_ = -1;

    // ── Search mode ──────────────────────────────────────────────────────
    bool searchMode_   = false;
    std::string searchBuf_;

    // ── Layout rects ─────────────────────────────────────────────────────
    static constexpr float kToolbarH  = 24.f;
    static constexpr float kInputH    = 26.f;
    static constexpr float kLineH     = 18.f;

    Color bgColor_ = Color(22, 24, 28, 255);

    // ── Helpers ──────────────────────────────────────────────────────────
    Color  levelColor(LogLevel l) const;
    Color  levelBgColor(LogLevel l) const;
    const char* levelTag(LogLevel l) const;

    void paintToolbar(PaintContext& ctx, const Rect& area);
    void paintLog(PaintContext& ctx, const Rect& area);
    void paintInput(PaintContext& ctx, const Rect& area);
    void paintScrollbar(PaintContext& ctx, const Rect& logArea, float totalH);

    void submitCommand();
    void historyUp();
    void historyDown();
};

} // namespace BuGUI
