// The clipboard of the widgets that take text, in one place (docs/ROUND10.md, section 7): which keys copy, cut and paste, what each of them does to
// the text, the context menu of right click, the paste of middle click and the PRIMARY selection. A widget does not write any of this itself: it says
// what it can do through TextClipTarget and calls onKey() and onMouse(); so the keys and the menu cannot differ from one widget to the next. An
// application's own widget that takes text does the same: derive from TextClipTarget and call these two from onKeyPress and onMousePress.
#pragma once

#include "Widget.hpp"

namespace ig
{
namespace retained
{

/// What the clipboard needs to know of a widget that takes text, and the one way it changes it. A widget that has no selection (the cell of a grid, the
/// line a search field holds) answers clipHasSelection() false and works on its whole text: see clipSelection().
class TextClipTarget
{
public:
    virtual Widget* clipWidget() = 0;
    /// Some text is selected.
    virtual bool clipHasSelection() const = 0;
    /// There is text to select (Select All).
    virtual bool clipHasText() const = 0;
    /// The text can be changed (false: read only).
    virtual bool clipEditable() const = 0;
    /// The text may leave the widget (false: a password: nothing is copied or cut).
    virtual bool clipCanCopy() const
    {
        return true;
    }
    /// One line: the line breaks of what is pasted are dropped.
    virtual bool clipOneLine() const
    {
        return true;
    }
    /// The text a copy takes: the selection.
    virtual String clipSelection() const = 0;
    /// The selection is replaced by `text` (empty: the selection is deleted), or `text` goes in at the cursor when nothing is selected. The widget does
    /// what typing would: undo, its change signal. It is the LAST thing the clipboard does with the widget (a handler of the signal may delete it).
    virtual void clipReplaceSelection(const String& text) = 0;
    virtual void clipSelectAll() = 0;
    /// The cursor goes to the point of a click (the selection is cleared).
    virtual void clipPlaceCursor(const MouseEvent& e) = 0;
    /// The point of a click is inside the selection (a right click there keeps it).
    virtual bool clipPointInSelection(const MouseEvent& e) const = 0;

protected:
    ~TextClipTarget() = default;
};

/// For a widget that edits one line with a cursor and no selection (a search field, the cell of a grid, the input line of a console): derive from this
/// and give the line, the cursor (a byte offset) and what to do when the line changed. With no selection Copy, Cut and Delete work on the whole line,
/// and Paste goes in at the cursor; there is nothing to Select.
class LineClipTarget : public TextClipTarget
{
public:
    bool clipHasSelection() const override
    {
        return !clipLine().empty();
    }
    bool clipHasText() const override
    {
        return false;
    }
    String clipSelection() const override
    {
        return clipLine();
    }
    void clipReplaceSelection(const String& text) override;
    void clipSelectAll() override
    {
    }
    bool clipPointInSelection(const MouseEvent&) const override
    {
        return false;
    }

protected:
    virtual const String& clipLine() const = 0;
    virtual String& clipLineRef() = 0;
    virtual int clipCursor() const = 0;
    virtual void clipSetCursor(int offset) = 0;
    /// The line changed: mark the widget dirty, then emit whatever it emits (the LAST thing: a handler may delete the widget).
    virtual void clipChanged() = 0;
};

/// The members of a LineClipTarget for a widget whose line and cursor are the members `buffer` and `cursor` (a byte offset) and that changes nothing but
/// its own look when the line changes (the cell of a grid, the input line of a console: the line is taken when Enter is pressed). Write it in the class,
/// where the widget is the editor of one line.
#define IG_LINE_CLIP(buffer, cursor)                      \
    Widget* clipWidget() override                         \
    {                                                     \
        return this;                                      \
    }                                                     \
    bool clipEditable() const override                    \
    {                                                     \
        return true;                                      \
    }                                                     \
    void clipPlaceCursor(const MouseEvent&) override      \
    {                                                     \
    }                                                     \
    const String& clipLine() const override               \
    {                                                     \
        return buffer;                                    \
    }                                                     \
    String& clipLineRef() override                        \
    {                                                     \
        return buffer;                                    \
    }                                                     \
    int clipCursor() const override                       \
    {                                                     \
        return cursor;                                    \
    }                                                     \
    void clipSetCursor(int offset) override               \
    {                                                     \
        cursor = offset;                                  \
    }                                                     \
    void clipChanged() override                           \
    {                                                     \
        markDirty();                                      \
    }

namespace textclip
{

enum class Action
{
    None,
    Copy,
    Cut,
    Paste,
    SelectAll
};

/// The action a key press asks for: Ctrl+C, Ctrl+Insert (copy), Ctrl+X, Shift+Delete (cut), Ctrl+V, Shift+Insert (paste), Ctrl+A.
Action keyAction(const KeyEvent& e);

/// Does `action` to the widget as far as it can (a password does not copy, a read only widget does not cut or paste). True when it was a clipboard action:
/// a key that asks for one is consumed even where the widget refuses it.
bool run(TextClipTarget& t, Action action);

/// A key press: the action it asks for, if any (e.consumed is set). Call it first in onKeyPress.
bool onKey(TextClipTarget& t, KeyEvent& e);

/// A press of the right button (the context menu: Cut, Copy, Paste, Delete, Select All) or of the middle one (the PRIMARY selection is pasted at the
/// click, or the clipboard where there is no PRIMARY). True when the press was one of those (e.consumed is set): the widget starts no selection.
bool onMouse(TextClipTarget& t, MouseEvent& e);

/// What the clipboard holds as one line: the line breaks dropped. For a one line field that is not a TextClipTarget (the bars of the code editor).
String clipboardLine();

/// A selection that a person made is the PRIMARY selection (the release of the mouse that selected, Select All): a no-op where there is none.
void publishSelection(TextClipTarget& t);

} // namespace textclip
} // namespace retained
} // namespace ig
