// Standalone demo for the immediate-mode Context::codeEditor() widget.
//
// Shows: per-language syntax highlighting (switch with the buttons or by
// typing a file name and pressing "Load by extension"), a real (line,column)
// cursor with mouse/keyboard selection, full undo/redo (Ctrl+Z / Ctrl+Y),
// keyword+identifier autocomplete (type 2+ identifier characters, Up/Down to
// pick, Tab/Enter to accept), and literal or ct::Regex-backed find/replace.
//
// Build: cmake -DIGUI_BUILD_RAYLIB_DEMO=ON -DIGUI_BUILD_CODE_EDITOR_DEMO=ON
// Run:   ./igui_code_editor_demo

#include <raylib.h>

#include <stdio.h>

#include <igui/Gui.hpp>
#include <igui_raylib/RaylibBackend.hpp>

namespace
{

struct Sample
{
    const char* label;
    const char* fileName; // drives extension-based highlighter detection
    const char* source;
};

const Sample kSamples[] = {
    {
        "C++",
        "sample.cpp",
        "#include <cstdio>\n"
        "\n"
        "// Computes the n-th Fibonacci number recursively.\n"
        "int fibonacci(int n) {\n"
        "    if (n < 2) return n;\n"
        "    return fibonacci(n - 1) + fibonacci(n - 2);\n"
        "}\n"
        "\n"
        "int main() {\n"
        "    for (int i = 0; i < 10; ++i)\n"
        "        printf(\"fib(%d) = %d\\n\", i, fibonacci(i));\n"
        "    return 0;\n"
        "}\n"
    },
    {
        "Lua",
        "sample.lua",
        "-- Simple stack implemented with a Lua table.\n"
        "local Stack = {}\n"
        "Stack.__index = Stack\n"
        "\n"
        "function Stack.new()\n"
        "    return setmetatable({ items = {} }, Stack)\n"
        "end\n"
        "\n"
        "function Stack:push(value)\n"
        "    table.insert(self.items, value)\n"
        "end\n"
        "\n"
        "function Stack:pop()\n"
        "    return table.remove(self.items)\n"
        "end\n"
        "\n"
        "local s = Stack.new()\n"
        "s:push(1)\n"
        "s:push(2)\n"
        "print(s:pop())\n"
    },
    {
        "Java",
        "Sample.java",
        "import java.util.List;\n"
        "import java.util.ArrayList;\n"
        "\n"
        "public class Sample {\n"
        "    @Override\n"
        "    public String toString() {\n"
        "        return \"Sample\";\n"
        "    }\n"
        "\n"
        "    public static void main(String[] args) {\n"
        "        List<Integer> values = new ArrayList<>();\n"
        "        for (int i = 0; i < 5; i++) {\n"
        "            values.add(i * i);\n"
        "        }\n"
        "        System.out.println(values);\n"
        "    }\n"
        "}\n"
    },
    {
        "Python",
        "sample.py",
        "\"\"\"Small utility module.\"\"\"\n"
        "\n"
        "def is_prime(n):\n"
        "    if n < 2:\n"
        "        return False\n"
        "    for d in range(2, int(n ** 0.5) + 1):\n"
        "        if n % d == 0:\n"
        "            return False\n"
        "    return True\n"
        "\n"
        "primes = [n for n in range(2, 50) if is_prime(n)]\n"
        "print(primes)\n"
    },
    {
        "Blitz",
        "sample.blitz",
        "; Bounce a ball inside the screen bounds.\n"
        "Function Clamp#(value#, lo#, hi#)\n"
        "    If value# < lo# Then Return lo#\n"
        "    If value# > hi# Then Return hi#\n"
        "    Return value#\n"
        "End Function\n"
        "\n"
        "Global x# = 100\n"
        "Global speed# = 4\n"
        "\n"
        "While Not KeyHit(1)\n"
        "    x# = x# + speed#\n"
        "    If x# > 600 Or x# < 0 Then speed# = -speed#\n"
        "    x# = Clamp#(x#, 0, 600)\n"
        "Wend\n"
    },
};
constexpr int kSampleCount = static_cast<int>(sizeof(kSamples) / sizeof(kSamples[0]));

struct DemoState
{
    ig::CodeEditorState editor;
    int currentSample = -1;
    ig::String findText;
    ig::String replaceText;
    bool useRegex = false;
    bool caseSensitive = false;
    bool showWhitespace = false;
    bool insertSpacesForTab = false; // Tab inserts a real '\t' by default in this demo
    ig::String statusMessage;

    void loadSample(int index)
    {
        if (index < 0 || index >= kSampleCount) return;
        currentSample = index;
        editor.setText(kSamples[index].source);
        editor.setHighlighterForFile(kSamples[index].fileName);
        statusMessage = ig::String("Loaded ") + kSamples[index].label;
    }
};

void drawToolbar(ig::Context& ui, DemoState& state)
{
    ui.label("Language:");
    ui.sameLine();
    for (int i = 0; i < kSampleCount; ++i)
    {
        if (i > 0) ui.sameLine();
        // radioButton just renders the dot; treat the click as a button.
        if (ui.button(kSamples[i].label))
            state.loadSample(i);
    }

    ui.separator();

    ui.label("Find:");
    ui.sameLine();
    ui.inputText("##find", state.findText, 220.0f);
    ui.sameLine();
    ui.label("Replace:");
    ui.sameLine();
    ui.inputText("##replace", state.replaceText, 220.0f);
    ui.sameLine();
    ui.checkbox("Regex", state.useRegex);
    ui.sameLine();
    ui.checkbox("Case", state.caseSensitive);
    ui.sameLine();
    ui.checkbox("Whitespace", state.showWhitespace);
    ui.sameLine();
    ui.checkbox("Spaces for Tab", state.insertSpacesForTab);

    ui.label("");
    if (ui.button("Find next"))
    {
        if (state.editor.findNext(state.findText, state.caseSensitive, state.useRegex))
            state.statusMessage = "Match found";
        else
            state.statusMessage = "No match";
    }
    ui.sameLine();
    if (ui.button("Find prev"))
    {
        if (state.editor.findPrev(state.findText, state.caseSensitive, state.useRegex))
            state.statusMessage = "Match found";
        else
            state.statusMessage = "No match";
    }
    ui.sameLine();
    if (ui.button("Replace all"))
    {
        int n = state.editor.replaceAll(state.findText, state.replaceText,
                                        state.caseSensitive, state.useRegex);
        char buf[64];
        snprintf(buf, sizeof(buf), "%d replacement(s)", n);
        state.statusMessage = buf;
    }
    ui.sameLine();
    if (ui.button("Undo") && state.editor.canUndo())
        state.editor.undo();
    ui.sameLine();
    if (ui.button("Redo") && state.editor.canRedo())
        state.editor.redo();
    ui.sameLine();
    if (ui.button("Zoom -")) state.editor.zoomOut();
    ui.sameLine();
    if (ui.button("Zoom +")) state.editor.zoomIn();
    ui.sameLine();
    if (ui.button("Zoom 100%")) state.editor.zoomReset();

    char zoomLabel[32];
    snprintf(zoomLabel, sizeof(zoomLabel), "%d%%", static_cast<int>(state.editor.fontScale() * 100.0f + 0.5f));
    ui.sameLine();
    ui.label(zoomLabel);

    ui.sameLine();
    ui.label(state.statusMessage);
}

} // namespace

int main()
{
    InitWindow(1280, 800, "iGUI code editor demo");
    SetTargetFPS(60);

    ig::raylib::Backend backend;
    if (!backend.prepareFontAtlas())
    {
        CloseWindow();
        return 1;
    }

    ig::Context ui(backend, &backend.fontAtlas());
    DemoState state;
    state.loadSample(0);

    while (!WindowShouldClose())
    {
        ig::raylib::processInput(ui);
        ui.beginFrame(ig::raylib::frameInfo(GetFrameTime()));

        if (ui.beginMainWindow("Code editor demo"))
        {
            drawToolbar(ui, state);
            ui.separator();

            const ig::Vec2 origin = ui.cursor();
            const float width = ui.availableWidth();
            const float height = 600.0f;
            ig::CodeEditorOptions options;
            options.showWhitespace = state.showWhitespace;
            options.insertSpacesForTab = state.insertSpacesForTab;
            ui.codeEditor("editor", state.editor, ig::Rect(origin.x, origin.y, width, height), options);
            ui.setCursor(ig::Vec2(origin.x, origin.y + height + 8.0f));

            char info[128];
            snprintf(info, sizeof(info), "Line %d, Col %d  |  %d lines  |  Undo:%s Redo:%s",
                    state.editor.cursorLine + 1, state.editor.cursorColumn + 1,
                    state.editor.lineCount(),
                    state.editor.canUndo() ? "yes" : "no",
                    state.editor.canRedo() ? "yes" : "no");
            ui.label(info);

            ui.endWindow();
        }

        const ig::DrawData& drawData = ui.endFrame();

        BeginDrawing();
        ClearBackground(::Color{24u, 27u, 35u, 255u});
        backend.render(drawData);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
