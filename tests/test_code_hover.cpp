// The pointer resting on a word of the retained CodeEditor asks the host for its
// documentation and opens a box; it closes when the pointer moves on. Driven
// through the real frame loop with a real font, so the words have widths.

#include "RetainedHarness.hpp"

#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/LayoutWidgets.hpp>

using namespace rh;

namespace
{

const char* kScript =
    "class Door(ScriptComponent):\n"
    "    def on_start(self):\n"
    "        self.sprite = self.node.get_sprite()\n"
    "        self.sprite.set_color(255, 0, 0, 255)\n";

void testPointerOnAWord()
{
    printf("the pointer resting on a word opens its documentation\n");
    Harness h(900.0f, 500.0f);
    check(h.ok(), "the font atlas builds");
    h.io().deltaTime = 1.0f / 60.0f;
    CodeEditor* editor = h.root()->createChild<CodeEditor>();
    editor->setHighlighter<PythonHighlighter>();
    editor->setRect({ 20.0f, 20.0f, 700.0f, 300.0f });
    editor->setText(kScript);
    h.relayout();
    h.frames(3);

    int asked = 0;
    CodeEditor::HoverContext last;
    editor->setHoverProvider([&](const CodeEditor::HoverContext& ctx, String& title, String& doc) {
        ++asked;
        last = ctx;
        if (ctx.word == "set_color" && ctx.receiver == "self.sprite")
        {
            title = "Sprite.set_color(r, g, b, a)";
            doc = "Tints the sprite. Each channel is 0 to 255, and the colour multiplies the texture.";
            return true;
        }
        return false;
    });

    const Rect r = editor->absoluteRect();
    const float lh = Theme::instance().fontSize + Theme::instance().lineSpacing;
    const float y = r.y + 4.0f + 3.0f * lh + lh * 0.5f;
    bool found = false;
    float foundX = 0.0f;
    for (float x = r.x + 40.0f; x < r.x + 400.0f && !found; x += 4.0f)
    {
        h.moveTo(x, y);
        for (int i = 0; i < 2; ++i) h.frame();
        check(!editor->hoverVisible(), "nothing opens before the delay");
        for (int i = 0; i < 40 && !editor->hoverVisible(); ++i) h.frame();
        if (editor->hoverVisible())
        {
            found = true;
            foundX = x;
        }
    }
    check(found, "the pointer resting on set_color opens the box");
    check(last.word == "set_color" && last.receiver == "self.sprite" && last.line == 3, "the host was asked with the word, its receiver and its line");
    check(editor->hoverTitle() == "Sprite.set_color(r, g, b, a)" && editor->hoverDoc().size() > 20, "the box has the title and the documentation");
    h.frames(10);
    check(editor->hoverVisible(), "it stays while the pointer rests");

    h.moveTo(foundX, y + 3.0f * lh);
    h.frame();
    check(!editor->hoverVisible(), "the pointer on another line closes it");
    const int before = asked;
    h.moveTo(r.x + 650.0f, y);
    h.frames(60);
    check(!editor->hoverVisible() && asked == before, "the blank past the end of a line asks nothing");

    h.moveTo(foundX, y);
    h.frames(40);
    check(editor->hoverVisible(), "back on the word it opens again");
    h.io().mouseDown[0] = true;
    h.frame();
    h.io().mouseDown[0] = false;
    h.frame();
    check(!editor->hoverVisible(), "a click closes it");

    h.moveTo(foundX + 1.0f, y);
    h.frames(40);
    check(editor->hoverVisible(), "and the pointer moving a little on the word opens it again");
    editor->setHoverDelay(5.0f);
    h.moveTo(foundX, y - 3.0f * lh);
    h.frame();
    h.moveTo(foundX, y);
    h.frames(60);
    check(!editor->hoverVisible(), "a longer delay is waited for");
}

} // namespace

int main()
{
    testPointerOnAWord();
    shutdown();
    if (failures())
        printf("%d failure(s)\n", failures());
    else
        printf("code hover: all checks passed\n");
    return failures() == 0 ? 0 : 1;
}
