#include <igui/Widgets.hpp>
#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/Theme.hpp>

#include <cassert>
#include <cmath>
#include <type_traits>

static_assert(std::is_same<ig::Color, ig::retained::Color>::value,
              "retained widgets must use ig::Color");
static_assert(std::is_same<ig::Vec2, ig::retained::Vec2>::value,
              "retained widgets must use ig::Vec2");
static_assert(std::is_same<ig::Rect, ig::retained::Rect>::value,
              "retained widgets must use ig::Rect");
static_assert(std::is_same<ig::Widget, ig::retained::Widget>::value,
              "retained widgets must be exposed as ig::Widget");

// The rounded rectangle used to be built from a fixed 30 degree table: four
// points per corner, which leaves a radius above a few pixels visibly faceted
// next to the immediate-mode rectangles. The corner must now be a curve.
static void test_round_rect_corner_is_a_curve()
{
    ig::retained::DrawList list;
    const float radius = 12.0f;
    const ig::Rect rect = {0.0f, 0.0f, 100.0f, 40.0f};
    list.addRoundRectFilled(rect, radius, ig::Color(255u, 255u, 255u, 255u), 8);

    // Outline plus the fringe ring the anti-aliasing adds, 4 * (8 + 1) each.
    const ct::Vector<ig::retained::DrawVertex> &vertices = list.vertices();
    assert(vertices.size() == 2u * 4u * 9u);

    const ig::Vec2 centres[4] = {
        {rect.x + rect.w - radius, rect.y + radius},
        {rect.x + rect.w - radius, rect.y + rect.h - radius},
        {rect.x + radius, rect.y + rect.h - radius},
        {rect.x + radius, rect.y + radius}
    };
    for (int corner = 0; corner < 4; ++corner)
    {
        float minDistance = 1e9f;
        float maxDistance = 0.0f;
        float maxGap = 0.0f;
        ig::Vec2 previous;
        for (int i = 0; i <= 8; ++i)
        {
            // Odd vertices are the transparent fringe; the solid corner points
            // are the even ones.
            const ig::retained::DrawVertex &vertex =
                vertices[static_cast<size_t>(2 * (corner * 9 + i))];
            const float dx = vertex.x - centres[corner].x;
            const float dy = vertex.y - centres[corner].y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            minDistance = distance < minDistance ? distance : minDistance;
            maxDistance = distance > maxDistance ? distance : maxDistance;
            if (i > 0)
            {
                const float gapX = vertex.x - previous.x;
                const float gapY = vertex.y - previous.y;
                const float gap = std::sqrt(gapX * gapX + gapY * gapY);
                maxGap = gap > maxGap ? gap : maxGap;
            }
            previous = {vertex.x, vertex.y};
        }
        // The nine points lie on one circle centred on the corner.
        assert(maxDistance - minDistance < 0.01f);
        assert(minDistance > radius - 1.0f && maxDistance < radius + 1.0f);
        // Eight steps of 11.25 degrees: a chord of 2 * 12 * sin(5.625) = 2.35 px.
        // The old 30 degree table stepped 6.2 px, and that is what read as a bevel.
        assert(maxGap < 3.0f);
    }
}

// The knob's arc is open at the bottom, like a hardware pot and like the
// immediate-mode knob. The old table started at 225 degrees, which left the
// opening on the left instead - the two knobs did not match.
static void test_knob_arc_is_open_at_the_bottom()
{
    ig::retained::DrawList list;
    ig::retained::PaintContext ctx(list);
    ig::retained::Knob knob;
    knob.setRect({0.0f, 0.0f, 64.0f, 64.0f});
    knob.setShowValue(false);
    knob.setValue(1.0f);
    knob.paint(ctx);

    const float cx = 32.0f;
    const float cy = 32.0f;
    const ig::Color track(52u, 58u, 82u, 255u);
    int trackPoints = 0;
    bool inBottomGap = false;
    bool nearLeft = false;
    bool nearTop = false;
    bool nearRight = false;
    const ct::Vector<ig::retained::DrawVertex> &vertices = list.vertices();
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        const ig::retained::DrawVertex &vertex = vertices[i];
        if (vertex.color.r != track.r || vertex.color.g != track.g || vertex.color.b != track.b)
            continue;
        const float dx = vertex.x - cx;
        const float dy = vertex.y - cy;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance < 20.0f || distance > 34.0f)   // the value ring only
            continue;
        ++trackPoints;
        float angle = std::atan2(dy, dx) * 180.0f / 3.14159265f;
        if (angle < 0.0f) angle += 360.0f;
        // Y grows down, so the bottom of the dial is 90 degrees.
        if (angle > 50.0f && angle < 130.0f) inBottomGap = true;
        if (angle > 165.0f && angle < 195.0f) nearLeft = true;
        if (angle > 255.0f && angle < 285.0f) nearTop = true;
        if (angle < 15.0f || angle > 345.0f) nearRight = true;
    }
    assert(trackPoints > 40);
    assert(!inBottomGap);
    assert(nearLeft && nearTop && nearRight);
}

// The knob takes its accent from the caller: with none set it follows the theme
// accent instead of a colour fixed inside the widget, so a voice picks its own.
static void test_knob_accent_comes_from_the_caller()
{
    const ig::Color accent = ig::retained::Theme::instance().focusColor;

    ig::retained::DrawList themed;
    ig::retained::PaintContext themedCtx(themed);
    ig::retained::Knob plain;
    plain.setRect({0.0f, 0.0f, 64.0f, 64.0f});
    plain.setShowValue(false);
    plain.setValue(1.0f);
    plain.paint(themedCtx);

    bool foundThemeAccent = false;
    for (size_t i = 0; i < themed.vertices().size(); ++i)
    {
        if (themed.vertices()[i].color == accent)
            foundThemeAccent = true;
    }
    assert(foundThemeAccent);

    ig::retained::DrawList coloured;
    ig::retained::PaintContext colouredCtx(coloured);
    ig::retained::Knob picked;
    picked.setRect({0.0f, 0.0f, 64.0f, 64.0f});
    picked.setShowValue(false);
    picked.setArcColor(ig::Color(10u, 20u, 30u, 255u));
    picked.setValue(1.0f);
    picked.paint(colouredCtx);

    bool foundPicked = false;
    bool foundTheme = false;
    for (size_t i = 0; i < coloured.vertices().size(); ++i)
    {
        const ig::Color &colour = coloured.vertices()[i].color;
        if (colour.r == 10u && colour.g == 20u && colour.b == 30u)
            foundPicked = true;
        if (colour == accent)
            foundTheme = true;
    }
    assert(foundPicked);
    assert(!foundTheme);
}

int main()
{
    using namespace ig;

    using namespace ig::retained;
    test_round_rect_corner_is_a_curve();
    test_knob_arc_is_open_at_the_bottom();
    test_knob_accent_comes_from_the_caller();
    PropertyGrid transformGrid;
    transformGrid.setRect({0, 0, 400, 180});
    float editedX = 1, editedY = 2, editedZ = 3;
    int changes = 0;
    transformGrid.addVec3("Position", 1, 2, 3, -100, 100,
        [&](float x, float y, float z) { editedX=x; editedY=y; editedZ=z; ++changes; });
    MouseEvent numericMouse;
    numericMouse.x=180; numericMouse.y=12;
    transformGrid.onMousePress(numericMouse);
    assert(changes == 0); // pressing must not jump to the pointer's absolute value
    transformGrid.onMouseRelease(numericMouse);
    KeyEvent editKey;
    editKey.key=ig::retained::Key::Home;
    transformGrid.onKeyPress(editKey);
    editKey.key=ig::retained::Key::Delete;
    for (int i=0; i<32; ++i) transformGrid.onKeyPress(editKey);
    KeyEvent numericText;
    numericText.text[0]='4'; numericText.text[1]='.'; numericText.text[2]='5';
    transformGrid.onTextInput(numericText);
    editKey.key=ig::retained::Key::Return;
    transformGrid.onKeyPress(editKey);
    assert(editedX == 4.5f && editedY == 2 && editedZ == 3 && changes == 1);
    transformGrid.onMousePress(numericMouse);
    numericMouse.x += 20;
    transformGrid.onMouseMove(numericMouse);
    transformGrid.onMouseRelease(numericMouse);
    assert(editedX > 4.5f && editedY == 2 && editedZ == 3);
    Timeline timeline;
    timeline.setRect({0, 0, 600, 240});
    timeline.setTimeRange(0, 5);
    const int rootJoint = timeline.addJoint("Root");
    const int handJoint = timeline.addJoint("Hand", rootJoint);
    const int fingerJoint = timeline.addJoint("Finger", handJoint);
    assert(fingerJoint == 2 && timeline.visibleTracks().size() == 3);
    timeline.setExpanded(handJoint, false);
    assert(timeline.visibleTracks().size() == 2);
    timeline.setExpanded(handJoint, true);
    timeline.addKeyframe(rootJoint, 0);
    timeline.addKeyframe(rootJoint, 5);
    MouseEvent mouse;
    mouse.x = 176; mouse.y = 38;
    timeline.onMousePress(mouse);
    assert(timeline.track(rootJoint).keyframes[0].selected);
    mouse.x = 190;
    timeline.onMouseMove(mouse);
    timeline.onMouseRelease(mouse);
    assert(timeline.track(rootJoint).keyframes[0].time > 0);
    mouse.x = 584;
    timeline.onMousePress(mouse);
    assert(timeline.track(rootJoint).keyframes[1].selected);
    mouse.x = 599;
    timeline.onMouseMove(mouse);
    timeline.onMouseRelease(mouse);
    assert(timeline.track(rootJoint).keyframes[1].time == 5);
    mouse.x = 380; mouse.y = 66; mouse.clickCount = 2;
    timeline.onMousePress(mouse);
    assert(timeline.track(handJoint).keyframes.size() == 1);
    timeline.setTimeRange(2, 2);
    assert(timeline.viewStart() == 0 && timeline.viewEnd() == 5);
    SyntaxLanguage language;
    language.name = "Example";
    language.extensions = {"example"};
    language.keywords = {"emit"};
    language.constants = {"yes"};
    DefinedHighlighter defined(language);
    auto tokens = defined.highlightLine(0, "emit yes // comment", 0);
    assert(tokens.spans[0].type == SyntaxHighlighter::TokenType::Keyword);
    assert(tokens.spans[2].type == SyntaxHighlighter::TokenType::Constant);
    assert(tokens.spans.back().type == SyntaxHighlighter::TokenType::Comment);
    auto block = defined.highlightLine(0, "/* start", 0);
    assert(block.state == 1);
    auto endBlock = defined.highlightLine(1, "end */ emit", block.state);
    assert(endBlock.state == 0 && endBlock.spans.back().type == SyntaxHighlighter::TokenType::Keyword);
    auto quoted = defined.highlightLine(0, "\"// text\"", 0);
    assert(quoted.spans.size() == 1 && quoted.spans[0].type == SyntaxHighlighter::TokenType::String);
    assert(defined.foldDelta(0, "{ \"}\" // }") == 1);
    CodeEditor editor;
    CodeEditor::registerLanguage(language);
    assert(editor.setHighlighterForFile("test.EXAMPLE") != nullptr);
    assert(String(editor.highlighter()->languageName()) == "Example");
    assert(editor.setHighlighterForFile("query.sql") != nullptr);
    auto sql = editor.highlighter()->highlightLine(0, "SELECT null", 0);
    assert(sql.spans[0].type == SyntaxHighlighter::TokenType::Keyword);
    assert(sql.spans.back().type == SyntaxHighlighter::TokenType::Constant);
    assert(editor.setHighlighterForFile("data.json") != nullptr);
    assert(editor.setHighlighterForFile("data.jsonc") != nullptr);
    assert(editor.setHighlighterForFile("main.go") != nullptr);
    assert(editor.setHighlighterForFile("notes.txt") == nullptr && editor.highlighter() == nullptr);

    // ct::Regex-backed find/replace (see third_party/containers submodule)
    editor.setText("foo bar foo baz");
    TextEdit::TextPos expectCol3{0, 3};
    TextEdit::TextPos expectCol11{0, 11};
    assert(editor.findNext("foo", true, false, false));
    assert(editor.cursorPos() == expectCol3);
    assert(editor.findNext("foo", true, false, false));
    assert(editor.cursorPos() == expectCol11);
    assert(editor.findNext("foo", true, false, false)); // wraps
    assert(editor.cursorPos() == expectCol3);

    editor.setText("value1 = 10\nvalue2 = 20");
    assert(editor.replaceAll("(\\w+) = (\\d+)", "\\2 -> \\1", false, true) == 2);
    assert(editor.lineAt(0) == "10 -> value1");
    assert(editor.lineAt(1) == "20 -> value2");

    editor.setText("abc");
    assert(!editor.findNext("(unterminated", true, false, true)); // invalid pattern, no crash
    assert(editor.replaceAll("(unterminated", "x", true, true) == 0);
    assert(editor.lineAt(0) == "abc");

    editor.setText("Hello WORLD hello");
    assert(editor.replaceAll("hello", "hi", false, true) == 2);
    assert(editor.lineAt(0) == "hi WORLD hi");

    String edited("abc");
    edited.append(2, '!');
    assert(edited == "abc!!");
    edited.insert(3, 2, '-');
    assert(edited == "abc--!!");
    edited.erase(edited.begin() + 3, edited.begin() + 5);
    assert(edited == "abc!!");
    assert(edited.compare(0, 3, "abc") == 0);
    assert(String("  x").find_first_not_of(" ") == 2u);
    assert(String("   ").find_first_not_of(" ") == String::npos);
    assert(String("file.json").find_last_of(".") == 4u);
    assert(String("42").to_int() == 42);
    assert(std::fabs(String("3.5").to_float() - 3.5f) < 0.001f);

    assert(FileSystem::exists("include/igui/widgets"));
    assert(FileSystem::isDir("include/igui/widgets"));
    assert(FileSystem::isFile("include/igui/widgets/CodeEditor.hpp"));
    assert(FileSystem::fileName("include/igui/widgets/CodeEditor.hpp") == "CodeEditor.hpp");
    assert(FileSystem::extension("include/igui/widgets/CodeEditor.hpp") == ".hpp");
    const ct::Vector<FileSystem::Entry> entries = FileSystem::listDir("include/igui/widgets");
    assert(!entries.empty());

    WidgetSerializer::registerBuiltinTypes();
    auto* source = new BoxLayout(LayoutDir::Vertical);
    source->setPadding(12.0f);
    source->setSpacing(6.0f);
    source->createChild<Label>("Serializer round-trip");
    source->createChild<Button>("Run");
    source->createChild<CheckBox>("Enabled")->setChecked(true);
    source->createChild<Slider>(0.0f, 100.0f, 42.0f);

    const json saved = WidgetSerializer::save(source);
    assert(String(saved["type"].str()) == "BoxLayout");
    assert(saved.contains("children"));
    assert(saved["children"].size() == 4u);

    json::Error parseError;
    const json parsed = json::parse(saved.dump(2), &parseError);
    assert(!parseError);
    assert(parsed == saved);

    Widget destination;
    Widget* loaded = WidgetSerializer::load(saved, &destination);
    assert(loaded != nullptr);
    assert(WidgetSerializer::typeName(loaded) == "BoxLayout");
    assert(loaded->children().size() == 4u);
    assert(WidgetSerializer::typeName(loaded->children()[0]) == "Label");
    assert(WidgetSerializer::typeName(loaded->children()[1]) == "Button");
    assert(WidgetSerializer::typeName(loaded->children()[2]) == "CheckBox");
    assert(WidgetSerializer::typeName(loaded->children()[3]) == "Slider");

    json malformed = json::object();
    malformed["type"] = "Label";
    malformed["rect"] = "not an array";
    malformed["tags"] = "also not an array";
    Widget* malformedLoaded = WidgetSerializer::load(malformed, &destination);
    assert(malformedLoaded != nullptr);
    assert(malformedLoaded->rect().w == 0.0f && malformedLoaded->rect().h == 0.0f);
    assert(malformedLoaded->tags().empty());

    malformed["rect"] = json::array();
    malformed["rect"].push_back(1.0f);
    malformed["tags"] = json::array();
    malformed["tags"].push_back("safe");
    Widget* shortRectLoaded = WidgetSerializer::load(malformed, &destination);
    assert(shortRectLoaded != nullptr);
    assert(shortRectLoaded->rect().w == 0.0f && shortRectLoaded->rect().h == 0.0f);
    assert(shortRectLoaded->tags().size() == 1u);

    Animation animation(0.0f, 10.0f, 1.0f, EaseType::Linear);
    animation.setAutoDelete(false);
    animation.start();
    animation.tick(0.5f);
    assert(animation.state() == Animation::State::Running);
    assert(std::fabs(animation.value() - 5.0f) < 0.001f);
    animation.tick(0.5f);
    assert(animation.state() == Animation::State::Finished);
    assert(std::fabs(animation.value() - 10.0f) < 0.001f);

    delete source;
    return 0;
}
