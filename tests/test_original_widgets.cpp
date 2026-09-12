#include <igui/Widgets.hpp>
#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/Timeline.hpp>

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

int main()
{
    using namespace ig;

    using namespace ig::retained;
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
