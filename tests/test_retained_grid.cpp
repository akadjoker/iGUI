// DataGrid and TreeGrid: the text of a header, a cell or a cell being edited stays inside its
// column (checked on the pixels the software rasteriser draws), text that does not fit ends in
// "...", a text that fits costs no clip and no allocation. Driven through the real frame loop
// (tests/RetainedHarness.hpp); the allocations are counted with tests/AllocCounter.hpp. No
// standard library: ct and C only.

#include "AllocCounter.hpp"
#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/DataWidgets.hpp>
#include <igui/widgets/IconAtlas.hpp>
#include <igui/widgets/ItemModel.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui_zen/SoftRaster.hpp>

using namespace rh;

namespace
{

const float kWindowW = 640.0f;
const float kWindowH = 320.0f;
const float kHeaderH = 28.0f; // DataGrid's and TreeGrid's header
const float kRowH = 24.0f;

int ceilInt(float v)
{
    const int i = static_cast<int>(v);
    return static_cast<float>(i) < v ? i + 1 : i;
}

// The elided text the grid should show in a room of `room` pixels, as the font measures it.
String elided(const String& text, float room)
{
    Harness h(kWindowW, kWindowH, false);
    DrawList list;
    PaintContext ctx(list, h.font());
    ctx.font.SetFontSize(Theme::instance().fontSize);
    return ctx.fitText(text, room);
}

// A text far wider than any of the columns used here.
String longText()
{
    return "Entrevista com Ação e muitas outras coisas bem mais compridas do que a coluna";
}

// ---- the DataGrid under test ----

struct DataSpec
{
    String header0 = "Nome";
    String header1 = "B";
    String cell0;
    String cell1;
    float col0 = 120.0f;
    float col1 = 120.0f;
    int sortColumn = -1; // click this header once: an ascending arrow
    int rows = 6;
    int editColumn = -1; // double click this column of the first row: its cell is being edited
    String typed;        // typed into the edited cell (after its text)
    int homeKeys = 0;    // Home pressed this many times (once is enough) after the typing
    int leftKeys = 0;    // Left pressed this many times after that
};

// A window with the grid of `spec`, drawn once: what it drew, where the grid is and how many clips
// the frame pushed.
Shot dataShot(const DataSpec& spec, Rect* gridRect = nullptr, int* clipPushes = nullptr)
{
    Screen screen(kWindowW, kWindowH);
    BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    DataGrid* grid = box->createChild<DataGrid>();
    grid->setStretch(1.0f);
    grid->addColumn(spec.header0, spec.col0);
    grid->addColumn(spec.header1, spec.col1);
    grid->addColumn("C", 160.0f);
    for (int r = 0; r < spec.rows; ++r) grid->addRow({ spec.cell0, spec.cell1, "" });
    screen.h.relayout();
    const Rect rect = grid->absoluteRect();
    if (spec.sortColumn >= 0)
    {
        // the middle of the header (a point less than 4 pixels from a border would start a resize)
        const float start = spec.sortColumn == 0 ? 0.0f : (spec.sortColumn == 1 ? spec.col0 : spec.col0 + spec.col1);
        const float width = spec.sortColumn == 0 ? spec.col0 : (spec.sortColumn == 1 ? spec.col1 : 160.0f);
        screen.h.click(rect.x + start + width * 0.5f, rect.y + kHeaderH * 0.5f);
    }
    if (spec.editColumn >= 0)
    {
        const float start = spec.editColumn == 0 ? 0.0f : (spec.editColumn == 1 ? spec.col0 : spec.col0 + spec.col1);
        const float width = spec.editColumn == 0 ? spec.col0 : (spec.editColumn == 1 ? spec.col1 : 160.0f);
        const float x = rect.x + start + (width < 40.0f ? width * 0.5f : 20.0f), y = rect.y + kHeaderH + kRowH * 0.5f;
        screen.h.click(x, y);
        screen.h.click(x, y); // a second click on the same row within 400 ms starts the edit
        if (spec.typed.size() > 0) screen.h.type(spec.typed.c_str());
        for (int i = 0; i < spec.homeKeys; ++i) screen.h.key(Key::Home);
        for (int i = 0; i < spec.leftKeys; ++i) screen.h.key(Key::Left);
    }
    screen.h.frames(2);
    if (gridRect) *gridRect = rect;
    if (clipPushes) *clipPushes = GetDrawList().pushClipCount();
    return screen.take();
}

// The pixels right of the first column (header and rows), left of its right border. `header` limits
// the rectangle to the header, `body` to the rows.
int pixelsRightOfFirstColumn(const Shot& a, const Shot& b, const Rect& grid, float col0, bool header, bool body)
{
    const int x0 = ceilInt(grid.x + col0) + 1;
    const int y0 = static_cast<int>(grid.y) + (header ? 0 : static_cast<int>(kHeaderH));
    const int y1 = static_cast<int>(grid.y) + (body ? static_cast<int>(grid.h) : static_cast<int>(kHeaderH));
    return differing(a, b, x0, y0, static_cast<int>(grid.x + grid.w) - 1, y1);
}

// ---- the TreeGrid under test ----

struct TreeSpec
{
    String header0 = "Nome";
    String header1 = "B";
    String text0;
    String text1;
    float col0 = 120.0f;
    float col1 = 120.0f;
    int sortColumn = -1;
    int roots = 6;
    int chain = 0;         // every root has a chain of this many nested nodes under it, all expanded
    float indent = 16.0f;  // TreeGrid::setIndentWidth
    bool icons = false;    // every node has an icon
    int editColumn = -1;   // double click this column of the first row
    String typed;
    int homeKeys = 0;
    int leftKeys = 0;
};

Shot treeShot(const TreeSpec& spec, Rect* gridRect = nullptr, int* clipPushes = nullptr)
{
    Screen screen(kWindowW, kWindowH);
    BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    TreeGrid* grid = box->createChild<TreeGrid>();
    grid->setStretch(1.0f);
    grid->setIndentWidth(spec.indent);
    grid->addColumn(spec.header0, spec.col0);
    grid->addColumn(spec.header1, spec.col1);
    grid->addColumn("C", 160.0f);
    for (int r = 0; r < spec.roots; ++r)
    {
        TreeGrid::Node* node = grid->addRoot({ spec.text0, spec.text1, "" });
        if (spec.icons) node->iconId = IconId::File;
        for (int d = 0; d < spec.chain; ++d)
        {
            node = node->addChild({ spec.text0, spec.text1, "" });
            if (spec.icons) node->iconId = IconId::File;
        }
    }
    grid->rebuild();
    screen.h.relayout();
    const Rect rect = grid->absoluteRect();
    if (spec.sortColumn >= 0)
    {
        const float start = spec.sortColumn == 0 ? 0.0f : (spec.sortColumn == 1 ? spec.col0 : spec.col0 + spec.col1);
        const float width = spec.sortColumn == 0 ? spec.col0 : (spec.sortColumn == 1 ? spec.col1 : 160.0f);
        screen.h.click(rect.x + start + width * 0.5f, rect.y + kHeaderH * 0.5f);
    }
    if (spec.editColumn >= 0)
    {
        const float start = spec.editColumn == 0 ? 0.0f : (spec.editColumn == 1 ? spec.col0 : spec.col0 + spec.col1);
        const float width = spec.editColumn == 0 ? spec.col0 : (spec.editColumn == 1 ? spec.col1 : 160.0f);
        const float x = rect.x + start + (width < 40.0f ? width * 0.5f : 40.0f), y = rect.y + kHeaderH + kRowH * 0.5f;
        screen.h.click(x, y);
        screen.h.click(x, y);
        if (spec.typed.size() > 0) screen.h.type(spec.typed.c_str());
        for (int i = 0; i < spec.homeKeys; ++i) screen.h.key(Key::Home);
        for (int i = 0; i < spec.leftKeys; ++i) screen.h.key(Key::Left);
    }
    screen.h.frames(2);
    if (gridRect) *gridRect = rect;
    if (clipPushes) *clipPushes = GetDrawList().pushClipCount();
    return screen.take(spec.icons);
}

// The x of a cursor, a vertical line of bright pixels at least `run` rows tall, in [x0, x1) x [y0, y1)
// (text is never that tall in a 24 pixel row), or -1.
int findCursor(const Shot& s, int x0, int x1, int y0, int y1, int run)
{
    for (int x = x0; x < x1 && x < s.w; ++x)
    {
        int best = 0, now = 0;
        for (int y = y0; y < y1 && y < s.h; ++y)
        {
            const uint32_t p = s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)];
            const int sum = static_cast<int>((p >> 16) & 0xff) + static_cast<int>((p >> 8) & 0xff) + static_cast<int>(p & 0xff);
            now = sum >= 3 * 170 ? now + 1 : 0;
            if (now > best) best = now;
        }
        if (best >= run) return x;
    }
    return -1;
}

// ---- scenarios ----

void testDataGridHeaderStaysInItsColumn()
{
    printf("a DataGrid header longer than its column does not reach the next one\n");
    DataSpec plain;
    DataSpec longHeader;
    longHeader.header0 = longText();
    Rect r;
    const Shot a = dataShot(plain, &r);
    const Shot b = dataShot(longHeader);
    check(differing(a, b, static_cast<int>(r.x), static_cast<int>(r.y), ceilInt(r.x + 120.0f), static_cast<int>(r.y + kHeaderH)) > 50,
            "the long header is drawn inside its own column");
    check(pixelsRightOfFirstColumn(a, b, r, 120.0f, true, false) == 0, "and nothing of it is drawn in the next column");

    // the same with the sort arrow: the text stops before it, and the arrow is where it was
    DataSpec sorted;
    sorted.sortColumn = 0;
    DataSpec sortedLong = longHeader;
    sortedLong.sortColumn = 0;
    const Shot c = dataShot(sorted);
    const Shot d = dataShot(sortedLong);
    check(pixelsRightOfFirstColumn(c, d, r, 120.0f, true, false) == 0, "a sorted long header stays in its column too");
    // the arrow sits 14 pixels from the column's right edge and is 6 wide: its box, a pixel around it
    const int arrowX = static_cast<int>(r.x + 120.0f - 14.0f);
    check(differing(c, d, arrowX - 1, static_cast<int>(r.y), arrowX + 7, static_cast<int>(r.y + kHeaderH)) == 0,
            "the header text stops before the sort arrow");
}

void testDataGridHeaderInAColumnTooNarrow()
{
    printf("a DataGrid header in a column too narrow even for the dots stays in it\n");
    const float widths[] = { 60.0f, 30.0f, 14.0f, 8.0f, 2.0f };
    for (size_t i = 0; i < sizeof widths / sizeof widths[0]; ++i)
    {
        DataSpec plain;
        plain.col0 = widths[i];
        DataSpec longHeader = plain;
        longHeader.header0 = longText();
        Rect r;
        const Shot a = dataShot(plain, &r);
        const Shot b = dataShot(longHeader);
        check(pixelsRightOfFirstColumn(a, b, r, widths[i], true, false) == 0, "nothing is drawn right of the column");
    }
}

void testDataGridCellsStayInTheirColumn()
{
    printf("a DataGrid cell longer than its column does not reach the next one\n");
    DataSpec plain;
    DataSpec longCell;
    longCell.cell0 = longText();
    Rect r;
    const Shot a = dataShot(plain, &r);
    const Shot b = dataShot(longCell);
    check(differing(a, b, static_cast<int>(r.x), static_cast<int>(r.y + kHeaderH), ceilInt(r.x + 120.0f), static_cast<int>(r.y + r.h)) > 200,
            "the long cells are drawn inside their column");
    check(pixelsRightOfFirstColumn(a, b, r, 120.0f, false, true) == 0, "and nothing of them is drawn in the next column");

    // a middle column: neither its neighbour on the right nor the one on the left
    DataSpec middle;
    middle.cell1 = longText();
    const Shot c = dataShot(middle);
    check(differing(a, c, ceilInt(r.x + 240.0f) + 1, static_cast<int>(r.y), static_cast<int>(r.x + r.w), static_cast<int>(r.y + r.h)) == 0,
            "a long cell of the second column stays out of the third");
    check(differing(a, c, static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.x + 120.0f), static_cast<int>(r.y + r.h)) == 0,
            "and out of the first");

    const float widths[] = { 60.0f, 30.0f, 14.0f, 8.0f, 2.0f };
    for (size_t i = 0; i < sizeof widths / sizeof widths[0]; ++i)
    {
        DataSpec narrow;
        narrow.col0 = widths[i];
        DataSpec narrowLong = narrow;
        narrowLong.cell0 = longText();
        const Shot e = dataShot(narrow);
        const Shot t = dataShot(narrowLong);
        check(pixelsRightOfFirstColumn(e, t, r, widths[i], false, true) == 0, "in a column too narrow for the dots too");
    }
}

// What a cell shows when its text does not fit is the text cut with "...", measured with the font:
// the picture of the long cell is that of a grid holding the elided text, whatever the margin is.
void testDataGridElidesWithDots()
{
    printf("a DataGrid cell and header that do not fit end in \"...\"\n");
    DataSpec longCell;
    longCell.cell0 = longText();
    Rect r;
    const Shot shownCell = dataShot(longCell, &r);
    DataSpec longHeader;
    longHeader.header0 = longText();
    const Shot shownHeader = dataShot(longHeader);
    bool cellMatched = false, headerMatched = false;
    String cellText, headerText;
    for (int room = 120; room >= 100 && !(cellMatched && headerMatched); --room)
    {
        const String shown = elided(longText(), static_cast<float>(room));
        if (!cellMatched)
        {
            DataSpec spec;
            spec.cell0 = shown;
            if (differing(shownCell, dataShot(spec), static_cast<int>(r.x), static_cast<int>(r.y + kHeaderH), ceilInt(r.x + 120.0f),
                        static_cast<int>(r.y + r.h)) == 0)
            {
                cellMatched = true;
                cellText = shown;
            }
        }
        if (!headerMatched)
        {
            DataSpec spec;
            spec.header0 = shown;
            if (differing(shownHeader, dataShot(spec), static_cast<int>(r.x), static_cast<int>(r.y), ceilInt(r.x + 120.0f),
                        static_cast<int>(r.y + kHeaderH)) == 0)
            {
                headerMatched = true;
                headerText = shown;
            }
        }
    }
    check(cellMatched, "the cell shows the text elided to a room a little narrower than the column");
    check(headerMatched, "the header shows the text elided to a room a little narrower than the column");
    check(cellText.size() > 3 && cellText.substr(cellText.size() - 3) == String("..."), "the cell's text ends in the dots");
    check(headerText.size() > 3 && headerText.substr(headerText.size() - 3) == String("..."), "the header's text ends in the dots");

    // a text that fits is shown whole: the grid draws what a grid with that text always drew
    check(elided("Cabe", 112.0f) == String("Cabe"), "a text that fits is not elided");
}

void testDataGridSortArrowStaysInItsColumn()
{
    printf("the sort arrow of a narrow DataGrid column does not leave it\n");
    // 10 is the narrowest column a click can sort (less, and every point of its header is within
    // 4 pixels of a border, which starts a resize); the arrow is 14 pixels from the right edge
    const float widths[] = { 40.0f, 20.0f, 14.0f, 10.0f };
    for (size_t i = 0; i < sizeof widths / sizeof widths[0]; ++i)
    {
        DataSpec unsorted;
        unsorted.col1 = widths[i];
        unsorted.header0 = "";
        unsorted.header1 = "";
        DataSpec sorted = unsorted;
        sorted.sortColumn = 1;
        Rect r;
        const Shot a = dataShot(unsorted, &r);
        const Shot b = dataShot(sorted);
        check(differing(a, b, ceilInt(r.x + 120.0f) + 1, static_cast<int>(r.y), ceilInt(r.x + 120.0f + widths[i]), static_cast<int>(r.y + kHeaderH)) > 0,
                "the arrow is drawn in the sorted column");
        check(differing(a, b, static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.x + 120.0f), static_cast<int>(r.y + kHeaderH)) == 0,
                "the arrow of the second column is not drawn in the first");
        check(differing(a, b, ceilInt(r.x + 120.0f + widths[i]) + 1, static_cast<int>(r.y), static_cast<int>(r.x + r.w) - 1,
                      static_cast<int>(r.y + kHeaderH)) == 0,
                "nor in the third");
    }
}

// A cell being edited shows its text cut at the cell, not elided, and scrolled so that the cursor
// stays in sight: with the cursor at the end the last part of the text is shown, with Home the first.
void testDataGridEditedCellStaysInItsColumn()
{
    printf("a DataGrid cell being edited does not leave its column and keeps the cursor in sight\n");
    DataSpec plain;
    plain.editColumn = 0;
    DataSpec longEdit = plain;
    longEdit.cell0 = longText();
    Rect r;
    const Shot a = dataShot(plain, &r);
    const Shot b = dataShot(longEdit);
    check(differing(a, b, static_cast<int>(r.x), static_cast<int>(r.y + kHeaderH), ceilInt(r.x + 120.0f), static_cast<int>(r.y + kHeaderH + kRowH)) > 100,
            "the edited text is drawn in the cell");
    check(pixelsRightOfFirstColumn(a, b, r, 120.0f, false, true) == 0, "and none of it in the next column");

    const int y0 = static_cast<int>(r.y + kHeaderH) + 4, y1 = static_cast<int>(r.y + kHeaderH + kRowH) - 4;
    const int x0 = static_cast<int>(r.x) + 3, x1 = static_cast<int>(r.x + 120.0f) - 3;
    check(findCursor(b, x0, x1, y0, y1, 14) >= 0, "the cursor, at the end of a text longer than the cell, is inside the cell");

    // the end of the text is what is shown: the same as for a shorter text with the same ending
    DataSpec shorter = plain;
    shorter.cell0 = longText().substr(20);
    const Shot c = dataShot(shorter);
    check(differing(b, c, x0, y0, x1, y1) == 0, "with the cursor at the end the end of the text is shown, not the dots");

    // typing more keeps the cursor in sight; Home brings the start back
    DataSpec typed = longEdit;
    typed.typed = "xyz";
    const Shot d = dataShot(typed);
    check(findCursor(d, x0, x1, y0, y1, 14) >= 0, "after typing the cursor is still inside the cell");
    DataSpec home = longEdit;
    home.homeKeys = 1;
    const Shot e = dataShot(home);
    const int cursorX = findCursor(e, x0, x1, y0, y1, 14);
    check(cursorX >= 0 && cursorX <= static_cast<int>(r.x) + 8, "with Home the cursor is at the start of the cell");
    DataSpec sameStart = home;
    sameStart.cell0 = longText().substr(0, 50) + "ZZZ";
    const Shot f = dataShot(sameStart);
    check(differing(e, f, x0, y0, x1, y1) == 0, "and the start of the text is shown");

    // moving the cursor back through the text keeps it in sight
    DataSpec walk = longEdit;
    for (int keys = 5; keys <= 60; keys += 11)
    {
        walk.leftKeys = keys;
        const int x = findCursor(dataShot(walk), x0, x1, y0, y1, 14);
        check(x >= 0, "after Left the cursor is still inside the cell");
    }

    // another column, and one too narrow to hold the cursor
    DataSpec second;
    second.editColumn = 1;
    DataSpec secondLong = second;
    secondLong.cell1 = longText();
    const Shot g = dataShot(second);
    const Shot h = dataShot(secondLong);
    check(differing(g, h, ceilInt(r.x + 240.0f) + 1, static_cast<int>(r.y), static_cast<int>(r.x + r.w), static_cast<int>(r.y + r.h)) == 0,
            "the edited cell of the second column stays out of the third");
    check(differing(g, h, static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.x + 120.0f), static_cast<int>(r.y + r.h)) == 0,
            "and out of the first");
    const float widths[] = { 30.0f, 14.0f, 8.0f, 4.0f };
    for (size_t i = 0; i < sizeof widths / sizeof widths[0]; ++i)
    {
        DataSpec narrow = plain;
        narrow.col0 = widths[i];
        DataSpec narrowLong = narrow;
        narrowLong.cell0 = longText();
        const Shot n = dataShot(narrow);
        const Shot m = dataShot(narrowLong);
        check(pixelsRightOfFirstColumn(n, m, r, widths[i], false, true) == 0, "an edited cell in a column too narrow stays in it");
    }
}

void testTreeGridHeaderStaysInItsColumn()
{
    printf("a TreeGrid header longer than its column does not reach the next one\n");
    TreeSpec plain;
    TreeSpec longHeader;
    longHeader.header0 = longText();
    Rect r;
    const Shot a = treeShot(plain, &r);
    const Shot b = treeShot(longHeader);
    check(differing(a, b, static_cast<int>(r.x), static_cast<int>(r.y), ceilInt(r.x + 120.0f), static_cast<int>(r.y + kHeaderH)) > 50,
            "the long header is drawn inside its own column");
    check(pixelsRightOfFirstColumn(a, b, r, 120.0f, true, false) == 0, "and nothing of it is drawn in the next column");

    TreeSpec sorted;
    sorted.sortColumn = 0;
    TreeSpec sortedLong = longHeader;
    sortedLong.sortColumn = 0;
    const Shot c = treeShot(sorted);
    const Shot d = treeShot(sortedLong);
    check(pixelsRightOfFirstColumn(c, d, r, 120.0f, true, false) == 0, "a sorted long header stays in its column too");
    const int arrowX = static_cast<int>(r.x + 120.0f - 14.0f);
    check(differing(c, d, arrowX - 1, static_cast<int>(r.y), arrowX + 7, static_cast<int>(r.y + kHeaderH)) == 0,
            "the header text stops before the sort arrow");

    const float widths[] = { 60.0f, 30.0f, 14.0f, 8.0f, 2.0f };
    for (size_t i = 0; i < sizeof widths / sizeof widths[0]; ++i)
    {
        TreeSpec narrow;
        narrow.col0 = widths[i];
        TreeSpec narrowLong = narrow;
        narrowLong.header0 = longText();
        check(pixelsRightOfFirstColumn(treeShot(narrow), treeShot(narrowLong), r, widths[i], true, false) == 0,
                "in a column too narrow for the dots too");
    }

    // the arrow of a narrow second column
    const float arrowWidths[] = { 40.0f, 20.0f, 14.0f, 10.0f };
    for (size_t i = 0; i < sizeof arrowWidths / sizeof arrowWidths[0]; ++i)
    {
        TreeSpec unsorted;
        unsorted.col1 = arrowWidths[i];
        unsorted.header0 = "";
        unsorted.header1 = "";
        TreeSpec arrow = unsorted;
        arrow.sortColumn = 1;
        const Shot e = treeShot(unsorted);
        const Shot f = treeShot(arrow);
        check(differing(e, f, ceilInt(r.x + 120.0f) + 1, static_cast<int>(r.y), ceilInt(r.x + 120.0f + arrowWidths[i]), static_cast<int>(r.y + kHeaderH)) > 0,
                "the arrow is drawn in the sorted column");
        check(differing(e, f, static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.x + 120.0f), static_cast<int>(r.y + kHeaderH)) == 0,
                "the arrow of the second column is not drawn in the first");
        check(differing(e, f, ceilInt(r.x + 120.0f + arrowWidths[i]) + 1, static_cast<int>(r.y), static_cast<int>(r.x + r.w) - 1,
                      static_cast<int>(r.y + kHeaderH)) == 0,
                "nor in the third");
    }
}

void testTreeGridCellsStayInTheirColumn()
{
    printf("a TreeGrid cell longer than its column does not reach the next one\n");
    TreeSpec plain;
    TreeSpec longCell;
    longCell.text0 = longText();
    Rect r;
    const Shot a = treeShot(plain, &r);
    const Shot b = treeShot(longCell);
    check(differing(a, b, static_cast<int>(r.x), static_cast<int>(r.y + kHeaderH), ceilInt(r.x + 120.0f), static_cast<int>(r.y + r.h)) > 200,
            "the long cells are drawn inside their column");
    check(pixelsRightOfFirstColumn(a, b, r, 120.0f, false, true) == 0, "and nothing of them is drawn in the next column");

    TreeSpec middle;
    middle.text1 = longText();
    const Shot c = treeShot(middle);
    check(differing(a, c, ceilInt(r.x + 240.0f) + 1, static_cast<int>(r.y), static_cast<int>(r.x + r.w), static_cast<int>(r.y + r.h)) == 0,
            "a long cell of the second column stays out of the third");
    check(differing(a, c, static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.x + 120.0f), static_cast<int>(r.y + r.h)) == 0,
            "and out of the first");

    const float widths[] = { 60.0f, 30.0f, 14.0f, 8.0f, 2.0f };
    for (size_t i = 0; i < sizeof widths / sizeof widths[0]; ++i)
    {
        TreeSpec narrow;
        narrow.col0 = widths[i];
        TreeSpec narrowLong = narrow;
        narrowLong.text0 = longText();
        check(pixelsRightOfFirstColumn(treeShot(narrow), treeShot(narrowLong), r, widths[i], false, true) == 0,
                "in a column too narrow for the dots too");
    }
}

// The first column of a tree starts its text after the indentation, the expander and the icon: a deep node
// in a narrow column has nothing left of the column for its text, and the expander and the icon themselves
// can lie past its right edge. The picture right of the column must be that of a tree that is not indented.
void testTreeGridIndentationStaysInTheFirstColumn()
{
    printf("the indentation, expander and icon of a deep TreeGrid node stay in the first column\n");
    const float widths[] = { 120.0f, 60.0f, 30.0f, 16.0f };
    for (int icons = 0; icons < 2; ++icons)
        for (size_t i = 0; i < sizeof widths / sizeof widths[0]; ++i)
        {
            TreeSpec flat;
            flat.col0 = widths[i];
            flat.roots = 2;
            flat.chain = 8;
            flat.indent = 0.0f;
            flat.icons = icons != 0;
            flat.text0 = longText();
            TreeSpec deep = flat;
            deep.indent = 16.0f;
            Rect r;
            const Shot a = treeShot(flat, &r);
            const Shot b = treeShot(deep);
            check(differing(a, b, static_cast<int>(r.x), static_cast<int>(r.y + kHeaderH), ceilInt(r.x + widths[i]),
                          static_cast<int>(r.y + r.h)) > 100,
                    "the indentation changes what the first column shows");
            check(pixelsRightOfFirstColumn(a, b, r, widths[i], false, true) == 0,
                    icons ? "with icons: nothing of the deep nodes is drawn in the next column" : "nothing of the deep nodes is drawn in the next column");
        }
}

void testTreeGridElidesWithDots()
{
    printf("a TreeGrid cell and header that do not fit end in \"...\"\n");
    TreeSpec longCell;
    longCell.text0 = longText();
    longCell.text1 = longText();
    Rect r;
    const Shot shownCell = treeShot(longCell, &r);
    bool firstMatched = false, secondMatched = false;
    String firstText, secondText;
    // The first column keeps 16 pixels for the expander and a margin each side, the second a margin each
    // side: the candidate rooms are those a little narrower than the column, in the range where the
    // dots would still be inside it.
    for (int room = 108; room >= 80 && !firstMatched; --room)
    {
        const String shown = elided(longText(), static_cast<float>(room));
        TreeSpec spec;
        spec.text0 = shown;
        spec.text1 = longText();
        if (differing(shownCell, treeShot(spec), static_cast<int>(r.x), static_cast<int>(r.y + kHeaderH), ceilInt(r.x + 120.0f),
                    static_cast<int>(r.y + r.h)) == 0)
        {
            firstMatched = true;
            firstText = shown;
        }
    }
    for (int room = 124; room >= 100 && !secondMatched; --room)
    {
        const String shown = elided(longText(), static_cast<float>(room));
        TreeSpec spec;
        spec.text0 = longText();
        spec.text1 = shown;
        if (differing(shownCell, treeShot(spec), ceilInt(r.x + 120.0f), static_cast<int>(r.y + kHeaderH), ceilInt(r.x + 240.0f),
                    static_cast<int>(r.y + r.h)) == 0)
        {
            secondMatched = true;
            secondText = shown;
        }
    }
    check(firstMatched && secondMatched, "both columns show the text elided to a room a little narrower than the column");
    check(firstText.size() > 3 && firstText.substr(firstText.size() - 3) == String("..."), "the first column's text ends in the dots");
    check(secondText.size() > 3 && secondText.substr(secondText.size() - 3) == String("..."), "the second column's text ends in the dots");
}

void testTreeGridEditedCellStaysInItsColumn()
{
    printf("a TreeGrid cell being edited does not leave its column and keeps the cursor in sight\n");
    for (int column = 0; column < 2; ++column)
    {
        TreeSpec plain;
        plain.editColumn = column;
        TreeSpec longEdit = plain;
        (column == 0 ? longEdit.text0 : longEdit.text1) = longText();
        Rect r;
        const Shot a = treeShot(plain, &r);
        const Shot b = treeShot(longEdit);
        const float left = column == 0 ? 0.0f : 120.0f;
        check(differing(a, b, ceilInt(r.x + left + 120.0f) + 1, static_cast<int>(r.y), static_cast<int>(r.x + r.w) - 1, static_cast<int>(r.y + r.h)) == 0,
                "the edited text does not reach the next column");
        if (column == 1)
            check(differing(a, b, static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.x + 120.0f), static_cast<int>(r.y + r.h)) == 0,
                    "or the one before");
        const int y0 = static_cast<int>(r.y + kHeaderH) + 4, y1 = static_cast<int>(r.y + kHeaderH + kRowH) - 4;
        const int x0 = static_cast<int>(r.x + left) + 3, x1 = static_cast<int>(r.x + left + 120.0f) - 3;
        check(findCursor(b, x0, x1, y0, y1, 14) >= 0, "the cursor at the end of a long text is inside the cell");
        TreeSpec typed = longEdit;
        typed.typed = "xyz";
        check(findCursor(treeShot(typed), x0, x1, y0, y1, 14) >= 0, "after typing it is still inside");
        TreeSpec walk = longEdit;
        for (int keys = 5; keys <= 60; keys += 11)
        {
            walk.leftKeys = keys;
            check(findCursor(treeShot(walk), x0, x1, y0, y1, 14) >= 0, "after Left it is still inside");
        }
        TreeSpec home = longEdit;
        home.homeKeys = 1;
        const int cursorX = findCursor(treeShot(home), x0, x1, y0, y1, 14);
        check(cursorX >= 0 && cursorX <= x0 + 20, "with Home it is at the start of the cell");
    }
    // a column too narrow for the field
    const float widths[] = { 30.0f, 14.0f, 8.0f, 4.0f };
    for (size_t i = 0; i < sizeof widths / sizeof widths[0]; ++i)
    {
        TreeSpec narrow;
        narrow.col0 = widths[i];
        narrow.editColumn = 0;
        TreeSpec narrowLong = narrow;
        narrowLong.text0 = longText();
        Rect r;
        const Shot n = treeShot(narrow, &r);
        check(pixelsRightOfFirstColumn(n, treeShot(narrowLong), r, widths[i], false, true) == 0, "an edited cell in a column too narrow stays in it");
    }
}

// ---- the widths of the columns ----

void addRows(DataGrid* grid)
{
    for (int r = 0; r < 3; ++r) grid->addRow({ "a", "b", "c", "d" });
}
void addRows(TreeGrid* grid)
{
    for (int r = 0; r < 3; ++r) grid->addRoot({ "a", "b", "c", "d" });
    grid->rebuild();
}

void applyStretch(DataGrid* grid, int count, int mask, bool last)
{
    for (int i = 0; i < count; ++i)
        if (mask & (1 << i)) grid->setColumnStretch(i, true);
    grid->setStretchLastColumn(last);
}
void applyStretch(TreeGrid* grid, int count, int mask, bool last)
{
    for (int i = 0; i < count; ++i)
        if (mask & (1 << i)) grid->setColumnStretch(i, true);
    grid->setStretchLastColumn(last);
}

// A window with a grid filling it (no padding) and the columns of the given widths; the clicks of the
// tests are in the grid's own coordinates.
template<class Grid>
struct Rig
{
    Harness h;
    Grid* grid = nullptr;
    int sorted = -1; // the column the last header click asked to sort

    // stretchMask: bit i set makes column i stretch; stretchLast the last one
    Rig(float windowWidth, const float* widths, int count, int stretchMask = 0, bool stretchLast = false)
            : h(windowWidth, kWindowH)
    {
        BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(0.0f);
        grid = box->createChild<Grid>();
        grid->setStretch(1.0f);
        for (int i = 0; i < count; ++i) grid->addColumn(String("C") + String::number(i), widths[i]);
        applyStretch(grid, count, stretchMask, stretchLast);
        addRows(grid);
        int* where = &sorted;
        grid->columnSorted.connect([where](int column) { *where = column; });
        h.relayout();
    }
    Rect rect() const
    {
        return grid->absoluteRect();
    }
    // The column whose header sort a click at x asks for (-1: none).
    int headerAt(float x)
    {
        sorted = -1;
        h.click(rect().x + x, rect().y + kHeaderH * 0.5f);
        return sorted;
    }
    // A drag that starts on the header at x and moves dx pixels to the right.
    void drag(float x, float dx)
    {
        h.moveTo(rect().x + x, rect().y + kHeaderH * 0.5f);
        h.down();
        h.moveTo(rect().x + x + dx, rect().y + kHeaderH * 0.5f);
        h.up();
    }
    // The window made this wide (and the layout made again, as an application does).
    void resizeTo(float width)
    {
        h.resize(width, kWindowH);
        h.relayout();
    }
    // The shift and the wheel together, as a horizontal scroll.
    void scrollRight(float notches)
    {
        h.moveTo(rect().x + 10.0f, rect().y + kHeaderH + 10.0f);
        h.io().keyShift = true;
        h.wheel(-notches);
        h.io().keyShift = false;
    }
};

// Columns that do not stretch (the default) are as wide as they were given, and everything reads that
// width: the header click, the resize edge, and how far the grid scrolls.
template<class Grid>
void fixedColumns()
{
    const float widths[] = { 100.0f, 150.0f, 80.0f };
    {
        Rig<Grid> r(640.0f, widths, 3);
        check(r.grid->columnWidth(0) == 100.0f && r.grid->columnWidth(1) == 150.0f && r.grid->columnWidth(2) == 80.0f, "each column has the width it was given");
        check(r.grid->columnWidth(3) == 0.0f && r.grid->columnWidth(-1) == 0.0f, "and a column that does not exist none");
        float x = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            check(r.headerAt(x + 10.0f) == i && r.headerAt(x + widths[i] * 0.5f) == i && r.headerAt(x + widths[i] - 10.0f) == i, "a header click lands on the column under it");
            x += widths[i];
        }
        check(r.headerAt(x + 10.0f) == -1, "and one right of the last column on none");

        // the edge between the first and the second column: dragging it makes the first wider
        r.drag(100.0f, 30.0f);
        check(r.grid->columnWidth(0) == 130.0f && r.grid->columnWidth(1) == 150.0f, "dragging the edge of a column widens it");
        check(r.headerAt(120.0f) == 0 && r.headerAt(140.0f) == 1, "and the next clicks follow the new border");
        r.drag(130.0f, -500.0f);
        check(r.grid->columnWidth(0) == 40.0f, "a column does not get narrower than its minimum");
        r.drag(40.0f + 150.0f + 80.0f, 25.0f);
        check(r.grid->columnWidth(2) == 105.0f, "the last column's right edge can be dragged too");
    }
    {
        // narrower than the columns: the grid scrolls sideways as far as the columns need
        Rig<Grid> r(250.0f, widths, 3);
        check(r.headerAt(240.0f) == 1, "the right end of the columns is out of sight at first");
        r.scrollRight(10.0f);
        check(r.headerAt(240.0f) == 2 && r.headerAt(180.0f) == 2 && r.headerAt(150.0f) == 1, "and in sight when scrolled as far as it goes (330 - 250)");
    }
}

// TreeGrid::setColumnWidth (as DataGrid's): the width a column has, the minimum it keeps, the pixels the
// grid draws (the same as a grid made with that width), the width of the columns after it and the scroll.
void testTreeGridSetColumnWidth()
{
    Harness h(kWindowW, kWindowH);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    TreeGrid* grid = box->createChild<TreeGrid>();
    grid->setStretch(1.0f);
    grid->addColumn("A", 100.0f);
    grid->addColumn("B", 100.0f);
    grid->addColumn("C", 100.0f);
    grid->addRoot({ "um", "dois", "tres" });
    grid->rebuild();
    h.relayout();
    check(grid->columnWidth(1) == 100.0f, "columns have the width they were added with");
    grid->setColumnWidth(1, 250.0f);
    h.frame();
    check(grid->columnWidth(1) == 250.0f, "setColumnWidth sets the width of a column");
    check(grid->columnWidth(0) == 100.0f && grid->columnWidth(2) == 100.0f, "the other columns keep theirs");
    check(grid->sizeHint().x == 450.0f, "the size hint follows the column widths");
    grid->setColumnWidth(1, 1.0f);
    DataGrid reference;
    reference.addColumn("A", 100.0f);
    reference.setColumnWidth(0, 1.0f);
    check(grid->columnWidth(1) == reference.columnWidth(0) && grid->columnWidth(1) > 1.0f,
            "a width below the minimum of the column is the minimum, as in the DataGrid");
    grid->setColumnWidth(1, 250.0f);
    grid->setColumnWidth(-1, 10.0f);
    grid->setColumnWidth(3, 10.0f);
    grid->setColumnWidth(1, 0.0f / 0.0f);
    check(grid->columnWidth(1) == 250.0f && grid->columnWidth(0) == 100.0f, "a column that does not exist or a width that is not a number changes nothing");
    // the columns after it move: a click in the middle of the third column (at 100 + 250 + 50) selects the row
    h.frame();
    const Rect rect = grid->absoluteRect();
    bool selected = false;
    grid->selectionChanged.connect([&selected](TreeGrid::Node*) { selected = true; });
    h.click(rect.x + 100.0f + 250.0f + 50.0f, rect.y + kHeaderH + kRowH * 0.5f);
    check(selected, "the third column starts after the wider second one");
    // the grid draws as one made with the widths
    TreeSpec wide;
    wide.col0 = 100.0f;
    wide.col1 = 250.0f;
    wide.text0 = "um";
    wide.text1 = "dois";
    wide.roots = 1;
    Rect grid1;
    const Shot expected = treeShot(wide, &grid1);
    Screen screen(kWindowW, kWindowH);
    BoxLayout* box2 = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box2->setPadding(0.0f);
    TreeGrid* g2 = box2->createChild<TreeGrid>();
    g2->setStretch(1.0f);
    g2->addColumn("Nome", 10.0f);
    g2->addColumn("B", 10.0f);
    g2->addColumn("C", 160.0f);
    g2->setColumnWidth(0, 100.0f);
    g2->setColumnWidth(1, 250.0f);
    g2->addRoot({ "um", "dois", "" });
    g2->rebuild();
    screen.h.relayout();
    screen.h.frames(2);
    const Shot got = screen.take();
    check(differing(expected, got, 0, 0, got.w, got.h) == 0, "a grid whose columns were widened draws as one made with those widths");
}

// A column width that is not a number (NaN) is ignored by both grids: it made the total width of the
// columns, the scroll range and every cell a NaN.
void testGridIgnoresANaNColumnWidth()
{
    Harness h(kWindowW, kWindowH);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    DataGrid* grid = box->createChild<DataGrid>();
    grid->setStretch(1.0f);
    grid->addColumn("A", 100.0f);
    grid->addColumn("B", 100.0f);
    grid->addRow({ "um", "dois" });
    h.relayout();
    grid->setColumnWidth(0, 0.0f / 0.0f);
    check(grid->columnWidth(0) == 100.0f, "a DataGrid column keeps its width when given a NaN");
    check(grid->sizeHint().x == 200.0f, "and the size hint is a number");
    h.frames(2);
    const Rect rect = grid->absoluteRect();
    bool selected = false;
    grid->selectionChanged.connect([&selected](int) { selected = true; });
    h.click(rect.x + 150.0f, rect.y + kHeaderH + kRowH * 0.5f);
    check(selected, "the second column can still be clicked");
}

// The icon of a node is drawn when the application paints with no icon atlas (the zen adapter paints with
// none: ZenApp passes nullptr): the vector icons need no atlas. The frame is the one the harness draws
// (no atlas), and the text of the node is where the icon leaves it.
Shot iconShot(bool icon, Rect* gridRect)
{
    Screen screen(kWindowW, kWindowH);
    BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    TreeGrid* grid = box->createChild<TreeGrid>();
    grid->setStretch(1.0f);
    grid->addColumn("Nome", 200.0f);
    TreeGrid::Node* node = grid->addRoot({ "No" });
    if (icon) node->iconId = IconId::File;
    grid->rebuild();
    screen.h.relayout();
    screen.h.frames(2);
    *gridRect = grid->absoluteRect();
    return screen.take(false);
}

void testTreeGridDrawsNodeIconsWithoutAnAtlas()
{
    Rect grid;
    const Shot with = iconShot(true, &grid);
    const Shot without = iconShot(false, &grid);
    // the icon sits after the expander: from 14 pixels in, a row (24) less 6 pixels square
    const int x0 = static_cast<int>(grid.x) + 14, y0 = static_cast<int>(grid.y + kHeaderH) + 3;
    const int n = differing(with, without, x0, y0, x0 + 18, y0 + 18);
    if (n < 10) printf("    only %d pixels differ where the icon goes\n", n);
    check(n >= 10, "the icon of a node is drawn with no icon atlas");
}

// The time and the node of the last click of a TreeGrid belong to the grid (they were static, one pair for
// every TreeGrid in the program) and are forgotten with the nodes: a node made after the others were
// cleared, at the address of one that was clicked a moment before, was taken for it (a double click).
void testTreeGridDoubleClickIsPerGridAndForgottenWithTheNode()
{
    Harness h(kWindowW, kWindowH);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    TreeGrid* grid = box->createChild<TreeGrid>();
    grid->setStretch(1.0f);
    grid->addColumn("A", 200.0f);
    TreeGrid::Node* first = grid->addRoot({ "um" });
    grid->rebuild();
    h.relayout();
    const Rect rect = grid->absoluteRect();
    const float x = rect.x + 100.0f, y = rect.y + kHeaderH + kRowH * 0.5f;
    int edited = 0;
    grid->cellEdited.connect([&edited](TreeGrid::Node*, int) { ++edited; });
    h.click(x, y);
    grid->clearAll();
    TreeGrid::Node* second = grid->addRoot({ "dois" });
    grid->rebuild();
    h.relayout();
    if (second != first) printf("    (the new node is not at the address of the old one: the check cannot fail here)\n");
    h.click(x, y);
    h.type("z");
    h.key(Key::Return);
    check(edited == 0, "a click on a new node is not a double click with the node that was there");

    // and two grids: a click in one is not the first click of a double click in the other
    TreeGrid* other = box->createChild<TreeGrid>();
    other->setStretch(1.0f);
    other->addColumn("A", 200.0f);
    other->addRoot({ "tres" });
    other->rebuild();
    h.relayout();
    const Rect r2 = other->absoluteRect();
    int edited2 = 0;
    other->cellEdited.connect([&edited2](TreeGrid::Node*, int) { ++edited2; });
    h.click(x, y);
    h.click(r2.x + 100.0f, r2.y + kHeaderH + kRowH * 0.5f);
    h.type("z");
    h.key(Key::Return);
    check(edited2 == 0, "a click in another grid is not a double click");
}

// ---- the scroll bars of the grids (the same for a DataGrid and a TreeGrid) ----

bool within(float a, float b)
{
    return a - b < 0.01f && b - a < 0.01f;
}

// One kind of grid, behind the same calls: a column of numbers (the text of row i is i), `rows` rows, and
// what the click on a row selected.
class BarGrid
{
public:
    BarGrid(bool tree, int rows, int columns, float columnWidth, float width = kWindowW, float height = kWindowH)
            : h(width, height),
              tree_(tree)
    {
        BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(0.0f);
        if (tree)
        {
            tg = box->createChild<TreeGrid>();
            tg->setStretch(1.0f);
            for (int c = 0; c < columns; ++c) tg->addColumn(String("C") + String::number(c), columnWidth);
            tg->selectionChanged.connect(
                    [this](TreeGrid::Node* n) { selected = n && n->cells.size() > 0 ? atoi(n->cells[0].c_str()) : -1; });
            tg->columnSorted.connect([this](int col) { sortedColumn = col; });
        }
        else
        {
            dg = box->createChild<DataGrid>();
            dg->setStretch(1.0f);
            for (int c = 0; c < columns; ++c) dg->addColumn(String("C") + String::number(c), columnWidth);
            dg->selectionChanged.connect([this](int row) { selected = row; });
            dg->columnSorted.connect([this](int col) { sortedColumn = col; });
        }
        for (int r = 0; r < rows; ++r) addRow(r, columns);
        columns_ = columns;
        relayout();
    }
    void addRow(int r, int columns)
    {
        ct::Vector<String> cells;
        for (int c = 0; c < columns; ++c) cells.push_back(String::number(r));
        if (tree_) tg->addRoot(cells);
        else
            dg->addRow(cells);
    }
    void relayout()
    {
        if (tree_) tg->rebuild();
        h.relayout();
    }
    Rect rect() const
    {
        return tree_ ? tg->absoluteRect() : dg->absoluteRect();
    }
    float columnWidth(int c) const
    {
        return tree_ ? tg->columnWidth(c) : dg->columnWidth(c);
    }
    void stretchLast()
    {
        if (tree_) tg->setStretchLastColumn(true);
        else
            dg->setStretchLastColumn(true);
        h.frames(2);
    }
    // The first row in sight: what a click on the top of the rows (x = 20) selects.
    int topRow()
    {
        selected = -1;
        const Rect r = rect();
        h.click(r.x + 20.0f, r.y + kHeaderH + 2.0f);
        return selected;
    }
    // The first column in sight: what a click at the left of the header sorts.
    int leftColumn()
    {
        sortedColumn = -1;
        const Rect r = rect();
        h.click(r.x + 5.0f, r.y + kHeaderH * 0.5f);
        return sortedColumn;
    }
    // A press on (x, y) in the grid, a move to (x2, y2) and the release
    void drag(float x, float y, float x2, float y2)
    {
        h.moveTo(x, y);
        h.down();
        h.moveTo((x + x2) * 0.5f, (y + y2) * 0.5f);
        h.moveTo(x2, y2);
        h.up();
    }
    Harness h;
    DataGrid* dg = nullptr;
    TreeGrid* tg = nullptr;
    int selected = -1;
    int sortedColumn = -1;

private:
    bool tree_;
    int columns_ = 0;
};

void testGridVerticalBarThumbAndTrack()
{
    for (int tree = 0; tree < 2; ++tree)
    {
        const char* name = tree ? "TreeGrid" : "DataGrid";
        BarGrid g(tree != 0, 100, 1, 100.0f);
        const Rect r = g.rect();
        check(g.topRow() == 0, "the first row is in sight");
        // the track is the 292 pixels under the header (28), 8 pixels wide at the right; the thumb is at the top
        const float barX = r.x + r.w - 4.0f, trackTop = r.y + kHeaderH;
        g.selected = -1;
        g.drag(barX, trackTop + 10.0f, barX, trackTop + 110.0f);
        check(g.selected == -1, (String(name) + ": a press on the bar does not select a row").c_str());
        const int dragged = g.topRow();
        // 100 pixels of 292 - 36 of track move the content 100 / 256 of 2108 pixels: row 34
        check(dragged >= 30 && dragged <= 38, (String(name) + ": the thumb of the vertical bar can be dragged").c_str());
        if (!(dragged >= 30 && dragged <= 38)) printf("    top row %d\n", dragged);
        // a press on the track below the thumb scrolls a page: the 292 pixels of the rows, 12 rows
        g.selected = -1;
        g.h.click(barX, trackTop + 280.0f);
        const int paged = g.topRow();
        check(g.selected == paged && paged - dragged >= 11 && paged - dragged <= 13, (String(name) + ": a press on the track pages").c_str());
        if (!(paged - dragged >= 11 && paged - dragged <= 13)) printf("    from row %d to %d\n", dragged, paged);
        g.h.click(barX, trackTop + 5.0f); // above the thumb
        const int back = g.topRow();
        check(paged - back >= 11 && paged - back <= 13, (String(name) + ": and a press above the thumb pages back").c_str());
        // the horizontal wheel does not move it and the vertical one does
        g.h.wheel(-1.0f);
        check(g.topRow() == back + 2, (String(name) + ": a notch of the wheel is two rows").c_str());
    }
}

void testGridHorizontalBarThumbAndTrack()
{
    for (int tree = 0; tree < 2; ++tree)
    {
        const char* name = tree ? "TreeGrid" : "DataGrid";
        // 20 columns of 100: 2000 pixels in 640; the thumb is 205 long
        BarGrid g(tree != 0, 3, 20, 100.0f);
        const Rect r = g.rect();
        check(g.leftColumn() == 0, "the first column is in sight");
        const float barY = r.y + r.h - 4.0f;
        // a press on the track right of the thumb scrolls a page (640 pixels): the column at the left edge is 6
        g.h.click(r.x + 330.0f, barY);
        const int paged = g.leftColumn();
        check(paged == 6, (String(name) + ": a press on the track of the horizontal bar pages").c_str());
        if (paged != 6) printf("    left column %d\n", paged);
        // the thumb is dragged back to the start
        // (scrolled 640 of 1360: the thumb starts 640 / 1360 * 435 = 205 pixels in, and is 205 long)
        g.drag(r.x + 300.0f, barY, r.x + 50.0f, barY);
        const int back = g.leftColumn();
        check(back == 0, (String(name) + ": the thumb of the horizontal bar can be dragged").c_str());
        if (back != 0) printf("    left column %d\n", back);
    }
}

void testGridScrollStaysInsideTheContent()
{
    for (int tree = 0; tree < 2; ++tree)
    {
        const char* name = tree ? "TreeGrid" : "DataGrid";
        // rows taken away: scrolled to the end of 100 rows, then 70 of them go
        {
            BarGrid g(tree != 0, 100, 1, 100.0f);
            const Rect r = g.rect();
            g.h.moveTo(r.x + 20.0f, r.y + 100.0f);
            g.h.wheel(-200.0f);
            check(g.topRow() >= 85, (String(name) + ": the wheel scrolls to the end").c_str());
            if (tree)
            {
                // the first root has the other rows under it, closed
                g.tg->clearAll();
                for (int i = 0; i < 30; ++i) g.addRow(i, 1);
                g.relayout();
            }
            else
            {
                for (int i = 0; i < 70; ++i) g.dg->removeRow(99 - i);
                g.h.frames(2);
            }
            // 30 rows of 24 in 292 pixels: the last row in sight is the 12th, and the grid is not scrolled past its end
            const int top = g.topRow();
            check(top >= 0 && top <= 18, (String(name) + ": rows taken away do not leave the grid scrolled past its end").c_str());
            if (!(top >= 0 && top <= 18)) printf("    top row %d\n", top);
        }
        // a window that grows: 100 rows scrolled to the end, then the window is three times as tall
        {
            BarGrid g(tree != 0, 20, 1, 100.0f);
            const Rect r = g.rect();
            g.h.moveTo(r.x + 20.0f, r.y + 100.0f);
            g.h.wheel(-200.0f);
            check(g.topRow() >= 7, (String(name) + ": 20 rows scroll to row 8 in 292 pixels").c_str());
            g.h.resize(kWindowW, 1000.0f);
            const int top = g.topRow();
            check(top == 0, (String(name) + ": a window that grows takes the grid back to the top").c_str());
            if (top != 0) printf("    top row %d\n", top);
        }
    }
}

void testGridColumnsEndWhereTheBarBegins()
{
    for (int tree = 0; tree < 2; ++tree)
    {
        const char* name = tree ? "TreeGrid" : "DataGrid";
        {
            BarGrid few(tree != 0, 3, 3, 100.0f);
            few.stretchLast();
            const Rect r = few.rect();
            const float sum = few.columnWidth(0) + few.columnWidth(1) + few.columnWidth(2);
            check(within(sum, r.w), (String(name) + ": without a vertical bar the columns fill the width").c_str());
        }
        {
            BarGrid many(tree != 0, 100, 3, 100.0f);
            many.stretchLast();
            const Rect r = many.rect();
            const float sum = many.columnWidth(0) + many.columnWidth(1) + many.columnWidth(2);
            check(within(sum, r.w - 8.0f), (String(name) + ": with a vertical bar the columns end where it begins").c_str());
            if (!within(sum, r.w - 8.0f)) printf("    %g of %g\n", sum, r.w);
            // and the bar is where the last column was: its pixels are the track, not the cells
            many.selected = -1;
            many.h.click(r.x + r.w - 4.0f, r.y + kHeaderH + 100.0f);
            check(many.selected == -1, (String(name) + ": a press on the bar is not a press on a cell").c_str());
        }
    }
}

void testFixedColumnsKeepTheirWidth()
{
    printf("columns that do not stretch keep the width they were given\n");
    fixedColumns<DataGrid>();
    fixedColumns<TreeGrid>();

    // DataGrid only: setColumnWidth, and the checkbox column in front
    const float widths[] = { 100.0f, 150.0f };
    Rig<DataGrid> r(640.0f, widths, 2);
    r.grid->setColumnWidth(1, 200.0f);
    check(r.grid->columnWidth(1) == 200.0f, "setColumnWidth sets the width");
    r.grid->setColumnWidth(1, 10.0f);
    check(r.grid->columnWidth(1) == 40.0f, "and not below the minimum");
    r.grid->setShowCheckboxes(true);
    check(r.grid->columnWidth(0) == 100.0f, "the checkbox column is not one of the columns");
    check(r.headerAt(24.0f + 10.0f) == 0 && r.headerAt(24.0f + 100.0f + 10.0f) == 1, "but the header clicks start after it");
}

bool near(float a, float b)
{
    return fabsf(a - b) < 0.01f;
}

// Four columns: 100 fixed, 80 stretching, 60 fixed, 120 stretching. The fixed ones take 160, the
// stretching ones share what is left, none under its own width (so the second and fourth have 200 of
// minimums and the columns fill the grid from 360 up).
struct FourColumns
{
    float widths[4];
    float expected(float window, int i) const
    {
        const float leftover = window - 160.0f;
        float second, fourth;
        if (leftover * 0.5f >= 120.0f) second = fourth = leftover * 0.5f;
        else if (leftover - 120.0f >= 80.0f)
        {
            fourth = 120.0f; // its minimum is more than the equal share: it keeps it, the other gets the rest
            second = leftover - 120.0f;
        }
        else
        {
            second = 80.0f;
            fourth = 120.0f;
        }
        const float all[4] = { 100.0f, second, 60.0f, fourth };
        return all[i];
    }
};

const FourColumns kFour = { { 100.0f, 80.0f, 60.0f, 120.0f } };
const int kFourMask = 0x2 | 0x8;

template<class Grid>
void stretchingColumns()
{
    const float windows[] = { 800.0f, 640.0f, 500.0f, 400.0f, 380.0f, 361.0f, 360.0f, 359.0f, 300.0f, 200.0f, 100.0f, 40.0f };
    for (size_t k = 0; k < sizeof windows / sizeof windows[0]; ++k)
    {
        const float window = windows[k];
        Rig<Grid> r(window, kFour.widths, 4, kFourMask);
        float total = 0.0f;
        bool widthsOk = true;
        for (int i = 0; i < 4; ++i)
        {
            widthsOk = widthsOk && near(r.grid->columnWidth(i), kFour.expected(window, i));
            total += r.grid->columnWidth(i);
        }
        check(widthsOk, "each column has its width: the fixed ones their own, the others their share but never less than their own");
        check(near(total, window > 360.0f ? window : 360.0f), "the columns fill the grid, or are as narrow as they can be");
        if (!widthsOk) printf("    window %.0f: %.2f %.2f %.2f %.2f\n", window, r.grid->columnWidth(0), r.grid->columnWidth(1), r.grid->columnWidth(2), r.grid->columnWidth(3));

        // a click on a header lands on the column that is drawn there (at the left of the window)
        float x = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float w = r.grid->columnWidth(i);
            if (x + w * 0.5f + 5.0f < window) check(r.headerAt(x + w * 0.5f) == i, "a click on the middle of a header lands on its column");
            x += w;
        }
        if (window >= 360.0f) check(r.headerAt(window - 3.0f) == 3, "the last column reaches the right edge of the grid");
    }
}

// The widths follow the window as it is resized, up and down, through the narrower-than-the-minimums.
template<class Grid>
void stretchFollowsTheWindow()
{
    Rig<Grid> r(640.0f, kFour.widths, 4, kFourMask);
    const float sizes[] = { 640.0f, 400.0f, 300.0f, 200.0f, 380.0f, 700.0f, 1000.0f, 360.0f, 640.0f };
    for (size_t k = 0; k < sizeof sizes / sizeof sizes[0]; ++k)
    {
        r.resizeTo(sizes[k]);
        bool ok = near(r.rect().w, sizes[k]);
        for (int i = 0; i < 4; ++i) ok = ok && near(r.grid->columnWidth(i), kFour.expected(sizes[k], i));
        check(ok, "after the window is resized the columns have the widths of that size");
        if (!ok) printf("    window %.0f: grid %.1f wide, columns %.2f %.2f %.2f %.2f\n", sizes[k], r.rect().w, r.grid->columnWidth(0), r.grid->columnWidth(1), r.grid->columnWidth(2), r.grid->columnWidth(3));
    }
    // a grid scrolled sideways in a narrow window is not left scrolled when the window is wide enough for it
    r.resizeTo(200.0f);
    r.scrollRight(20.0f);
    check(r.headerAt(190.0f) == 3, "scrolled as far as it goes, the end of the columns is at the right edge");
    r.resizeTo(640.0f);
    check(r.headerAt(10.0f) == 0, "made wide enough, the first column is back at the left edge");
    check(r.headerAt(630.0f) == 3, "and the last at the right");
}

// The last column stretches by itself, alone or with the ones asked for.
template<class Grid>
void stretchLastColumn()
{
    const float widths[] = { 100.0f, 100.0f, 100.0f };
    {
        Rig<Grid> r(640.0f, widths, 3, 0, true);
        check(r.grid->stretchLastColumn(), "the flag is on");
        check(near(r.grid->columnWidth(0), 100.0f) && near(r.grid->columnWidth(1), 100.0f) && near(r.grid->columnWidth(2), 440.0f),
                "the last column has what the others leave");
        r.resizeTo(250.0f);
        check(near(r.grid->columnWidth(2), 100.0f), "and its own width when there is no more");
        r.resizeTo(301.0f);
        check(near(r.grid->columnWidth(2), 101.0f), "and as much as the window gives from there on");
        check(r.headerAt(299.0f) == 2, "its right edge is the window's");
        r.resizeTo(400.0f);
        r.grid->addColumn("late", 50.0f);
        check(near(r.grid->columnWidth(2), 100.0f) && near(r.grid->columnWidth(3), 100.0f) && !r.grid->columnStretch(3),
                "a column added later is the last one now, and the one that stretches");
        r.grid->setStretchLastColumn(false);
        check(near(r.grid->columnWidth(3), 50.0f), "switched off, it is as wide as it was given");
    }
    {
        // the last column and another one asked for share equally
        Rig<Grid> r(640.0f, widths, 3, 0x2, true);
        check(near(r.grid->columnWidth(0), 100.0f) && near(r.grid->columnWidth(1), 270.0f) && near(r.grid->columnWidth(2), 270.0f),
                "the last column and a stretching one share what is left");
        check(r.grid->columnStretch(1) && !r.grid->columnStretch(2) && r.grid->stretchLastColumn(), "the last is not marked as stretching by itself");
    }
    {
        // off by default: nothing stretches
        Rig<Grid> r(640.0f, widths, 3);
        check(!r.grid->stretchLastColumn() && !r.grid->columnStretch(0) && !r.grid->columnStretch(2) && !r.grid->columnStretch(5), "nothing stretches by default");
        check(near(r.grid->columnWidth(0), 100.0f) && near(r.grid->columnWidth(2), 100.0f), "and the columns have their widths");
        r.grid->setColumnStretch(5, true); // no such column: ignored
        r.grid->setColumnStretch(-1, true);
        check(near(r.grid->columnWidth(2), 100.0f), "setColumnStretch on a column that does not exist changes nothing");
        r.grid->setColumnStretch(0, true);
        check(near(r.grid->columnWidth(0), 440.0f) && r.grid->columnStretch(0), "the first column can stretch as well");
        r.grid->setColumnStretch(0, false);
        check(near(r.grid->columnWidth(0), 100.0f), "and be switched off");
    }
}

// What dragging an edge does when columns stretch: a column that does not stretch is resized (and the
// stretching ones take up what it gives or takes); the edge of a stretching column is not an edge.
template<class Grid>
void stretchAndResize()
{
    // 100 fixed | 80 stretching | 60 fixed | 120 stretching, in 640: 100 | 240 | 60 | 240
    {
        Rig<Grid> r(640.0f, kFour.widths, 4, kFourMask);
        r.drag(100.0f, 20.0f);
        check(near(r.grid->columnWidth(0), 120.0f), "the first column is resized by its edge");
        check(near(r.grid->columnWidth(1), 230.0f) && near(r.grid->columnWidth(3), 230.0f), "and the stretching ones share what is left");
        r.drag(120.0f + 230.0f + 60.0f, -10.0f); // the edge between the third column and the fourth
        check(near(r.grid->columnWidth(2), 50.0f), "the third, between two that stretch, too");
        check(near(r.grid->columnWidth(1), 235.0f) && near(r.grid->columnWidth(3), 235.0f), "(40 less to the others from the first and the third)");
    }
    {
        // the edge of a stretching column is not dragged: nothing changes, and the press is a press on a header
        Rig<Grid> r(640.0f, kFour.widths, 4, kFourMask);
        r.sorted = -1;
        r.drag(340.0f, 30.0f); // the right edge of the second column, 100 + 240
        check(near(r.grid->columnWidth(0), 100.0f) && near(r.grid->columnWidth(1), 240.0f) && near(r.grid->columnWidth(2), 60.0f) && near(r.grid->columnWidth(3), 240.0f),
                "dragging the edge of a stretching column changes no width");
        check(r.sorted == 2, "the press on it was a press on the header under it (the third column)");
        r.drag(639.0f, 15.0f); // the last column's edge is the window's
        check(near(r.grid->columnWidth(3), 240.0f) && near(r.grid->columnWidth(0), 100.0f), "nor does the edge of the last column");
        r.drag(-1.0f, 15.0f); // the left edge of the first
        check(near(r.grid->columnWidth(0), 100.0f), "or the left of the grid");
    }
    {
        // widen a fixed column until the stretching ones are at their own width: the columns are wider than the window
        Rig<Grid> r(640.0f, kFour.widths, 4, kFourMask);
        r.drag(100.0f, 400.0f);
        check(near(r.grid->columnWidth(0), 500.0f) && near(r.grid->columnWidth(1), 80.0f) && near(r.grid->columnWidth(2), 60.0f) && near(r.grid->columnWidth(3), 120.0f),
                "the stretching columns do not go under their own width");
        check(r.headerAt(630.0f) == 2, "the end of the columns is out of sight");
        r.scrollRight(10.0f);
        check(r.headerAt(630.0f) == 3 && r.headerAt(500.0f) == 2, "and scrolling brings it in by exactly what is missing (760 - 640)");
        r.drag(500.0f - 120.0f, -300.0f); // made narrower again: the first column's edge, 500 - 120 scrolled
        check(near(r.grid->columnWidth(0), 200.0f) && near(r.grid->columnWidth(1), 190.0f) && near(r.grid->columnWidth(3), 190.0f),
                "narrowed again, the stretching ones take the room back");
        check(r.headerAt(10.0f) == 0, "and the scroll is back at the left (the columns fit)");
    }
}

// The least width the columns need is what the grid asks its parent for.
template<class Grid>
void stretchSizeHint()
{
    {
        Rig<Grid> r(640.0f, kFour.widths, 4, kFourMask);
        check(near(r.grid->sizeHint().x, 360.0f), "a grid with stretching columns asks for the least they need");
        r.resizeTo(1000.0f);
        check(near(r.grid->sizeHint().x, 360.0f), "whatever its width");
    }
    {
        Rig<Grid> r(640.0f, kFour.widths, 4);
        check(near(r.grid->sizeHint().x, 360.0f), "and one without them the sum of the widths");
    }
}

void addLongRows(DataGrid* grid)
{
    for (int r = 0; r < 5; ++r) grid->addRow({ longText(), longText(), longText(), longText() });
}
void addLongRows(TreeGrid* grid)
{
    for (int r = 0; r < 5; ++r) grid->addRoot({ longText(), longText(), longText(), longText() });
    grid->rebuild();
}

// The grid with stretching columns is painted as one whose columns have, without stretching, the widths
// that it works out: every pixel the same.
template<class Grid>
void stretchPaintsLikeFixedColumns()
{
    const float windows[] = { 640.0f, 380.0f, 300.0f };
    for (size_t k = 0; k < sizeof windows / sizeof windows[0]; ++k)
    {
        float effective[4];
        Shot stretched;
        {
            Screen screen(windows[k], kWindowH);
            BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
            box->setPadding(0.0f);
            Grid* grid = box->createChild<Grid>();
            grid->setStretch(1.0f);
            for (int i = 0; i < 4; ++i) grid->addColumn(longText(), kFour.widths[i]);
            applyStretch(grid, 4, kFourMask, false);
            addLongRows(grid);
            screen.h.relayout();
            screen.h.click(5.0f, 10.0f); // a sort arrow on the first column
            screen.h.frames(2);
            for (int i = 0; i < 4; ++i) effective[i] = grid->columnWidth(i);
            stretched = screen.take();
        }
        Shot fixed;
        {
            Screen screen(windows[k], kWindowH);
            BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
            box->setPadding(0.0f);
            Grid* grid = box->createChild<Grid>();
            grid->setStretch(1.0f);
            for (int i = 0; i < 4; ++i) grid->addColumn(longText(), effective[i]);
            addLongRows(grid);
            screen.h.relayout();
            screen.h.click(5.0f, 10.0f);
            screen.h.frames(2);
            fixed = screen.take();
        }
        check(stretched.w == fixed.w && stretched.h == fixed.h, "of the same size");
        check(differing(stretched, fixed, 0, 0, static_cast<int>(windows[k]), static_cast<int>(kWindowH)) == 0, "the same picture");
    }
}

// A click lands on the cell that is drawn there, and the cell it starts to edit is that one: the last column
// of a grid that stretches is clicked far to the right of where it ends without stretching.
void stretchedCellsAreClickedAndEdited(DataGrid*)
{
    const float widths[] = { 100.0f, 100.0f, 100.0f };
    Rig<DataGrid> r(640.0f, widths, 3, 0, true);
    int editedRow = -1, editedColumn = -1;
    r.grid->cellEdited.connect([&editedRow, &editedColumn](int row, int column) { editedRow = row; editedColumn = column; });
    const float y = r.rect().y + kHeaderH + kRowH * 1.5f; // the second row
    r.h.click(r.rect().x + 500.0f, y);
    r.h.click(r.rect().x + 500.0f, y);
    check(r.grid->selectedRow() == 1, "the click selected the row under it");
    r.h.type("z");
    r.h.key(Key::Return);
    check(editedRow == 1 && editedColumn == 2, "and the double click edited the last column, 500 pixels in");
    check(r.grid->cell(1, 2) == String("cz"), "the typed text is in that cell");
}
void stretchedCellsAreClickedAndEdited(TreeGrid*)
{
    const float widths[] = { 100.0f, 100.0f, 100.0f };
    Rig<TreeGrid> r(640.0f, widths, 3, 0, true);
    TreeGrid::Node* edited = nullptr;
    int editedColumn = -1;
    r.grid->cellEdited.connect([&edited, &editedColumn](TreeGrid::Node* node, int column) { edited = node; editedColumn = column; });
    const float y = r.rect().y + kHeaderH + kRowH * 1.5f;
    r.h.click(r.rect().x + 500.0f, y);
    r.h.click(r.rect().x + 500.0f, y);
    check(r.grid->selectedNode() == r.grid->roots()[1], "the click selected the node under it");
    r.h.type("z");
    r.h.key(Key::Return);
    check(edited == r.grid->roots()[1] && editedColumn == 2, "and the double click edited the last column, 500 pixels in");
    check(edited && edited->cells[2] == String("cz"), "the typed text is in that cell");
}

// The widths are worked out when the grid or a column changes, not every frame: an idle frame of a grid
// with stretching columns allocates nothing.
template<class Grid>
void stretchingGridIdlesWithoutAllocating()
{
    Rig<Grid> r(640.0f, kFour.widths, 4, kFourMask, true);
    r.h.frames(5);
    const unsigned long long before = gAllocs;
    for (int i = 0; i < 10; ++i) r.h.frame();
    check(gAllocs - before == 0, "ten idle frames of a grid with stretching columns allocate nothing");
}

void testStretchingColumns()
{
    printf("stretching columns share the width the fixed ones leave over\n");
    stretchingColumns<DataGrid>();
    stretchingColumns<TreeGrid>();
    printf("the columns follow the window\n");
    stretchFollowsTheWindow<DataGrid>();
    stretchFollowsTheWindow<TreeGrid>();
    printf("the last column stretches\n");
    stretchLastColumn<DataGrid>();
    stretchLastColumn<TreeGrid>();
    printf("dragging edges with stretching columns\n");
    stretchAndResize<DataGrid>();
    stretchAndResize<TreeGrid>();
    printf("the size hint of a grid with stretching columns\n");
    stretchSizeHint<DataGrid>();
    stretchSizeHint<TreeGrid>();
    printf("a grid with stretching columns is painted like one with those widths\n");
    stretchPaintsLikeFixedColumns<DataGrid>();
    stretchPaintsLikeFixedColumns<TreeGrid>();
    printf("a click on a stretched column selects and edits its cell\n");
    stretchedCellsAreClickedAndEdited(static_cast<DataGrid*>(nullptr));
    stretchedCellsAreClickedAndEdited(static_cast<TreeGrid*>(nullptr));
    printf("the idle frame of a grid with stretching columns\n");
    stretchingGridIdlesWithoutAllocating<DataGrid>();
    stretchingGridIdlesWithoutAllocating<TreeGrid>();
}

// ---- what the grids remember of their cells (the memos of PaintContext::printElided) ----
//
// A grid keeps, for each cell in sight, the width of its text and the text elided to its column. Whatever
// changes in the grid after it has drawn that - a cell, the width of a column, the font, the data of a
// model, the scroll, the order of the rows - must show in the next frame as in a grid made after the
// change: each test draws a grid, changes it, draws it again at several scroll positions, and compares
// that with a grid that never saw the old state, pixel by pixel (the font size, which a baked font does not
// follow, by where the glyphs are).

// The text of a cell, from its row and column: the ones next to each other differ, so that a memo made
// for one that moves to the slot of another is not taken for it: some are cut by their column, some fit,
// some are short, some empty.
typedef String (*Scheme)(int row, int column);

// longText() with its first letter another: as long, and as wide, and not the same text.
String otherLongText()
{
    String t = longText();
    t[0] = 'X';
    return t;
}

String schemeA(int row, int column)
{
    switch ((row + column) % 4)
    {
        case 0: return longText();
        case 1: return String("r") + String::number(row) + "c" + String::number(column);
        case 2: return String("Entrevista com o cliente ") + String::number(row);
        default: return "";
    }
}
String schemeB(int row, int column)
{
    switch ((row * 2 + column * 3) % 5)
    {
        case 0: return String("Cabe ") + String::number(row);
        case 1: return longText().substr(static_cast<size_t>(column * 5), 40);
        case 2: return otherLongText();
        case 3: return "";
        default: return String("c") + String::number(column);
    }
}
// A of the cells whose row and column add up to a multiple of three, B of the others.
String schemeMixed(int row, int column)
{
    return (row + column) % 3 == 0 ? schemeA(row, column) : schemeB(row, column);
}

const int kMemoRows = 60;
const float kMemoWidths[4] = { 100.0f, 140.0f, 90.0f, 160.0f };

// A text model of the tests: it holds what it is given, and says so only when asked to.
class TextModel : public AbstractItemModel
{
public:
    TextModel(Scheme scheme, int rows) : rows_(rows)
    {
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < 4; ++c) cells_.push_back(scheme(r, c));
    }
    int rowCount(const ModelIndex& = {}) const override
    {
        return rows_;
    }
    int columnCount(const ModelIndex& = {}) const override
    {
        return 4;
    }
    String data(const ModelIndex& index, ItemRole = ItemRole::Display) const override
    {
        return cells_[static_cast<size_t>(index.row() * 4 + index.column())];
    }
    String headerData(int section, bool = true) const override
    {
        return String("Coluna comprida ") + String::number(section);
    }
    int flags(const ModelIndex&) const override
    {
        return Selectable | Editable | Enabled;
    }
    bool setData(const ModelIndex& index, const String& value, ItemRole = ItemRole::Edit) override
    {
        set(index.row(), index.column(), value);
        return true;
    }
    void set(int row, int column, const String& text)
    {
        cells_[static_cast<size_t>(row * 4 + column)] = text;
    }
    void announce(int row, int column)
    {
        dataChanged.emit(ModelIndex(row, column));
    }

private:
    ct::Vector<String> cells_;
    int rows_;
};

// A window with a DataGrid of kMemoRows rows of four columns, filled by a scheme (or a model's).
struct DataStage
{
    Screen screen;
    DataGrid* grid = nullptr;
    TextModel* model = nullptr;

    DataStage(Scheme scheme, float firstWidth = 100.0f, float width = kWindowW, bool dynamicFonts = false, TextModel* withModel = nullptr)
            : screen(width, kWindowH, dynamicFonts)
    {
        BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(0.0f);
        grid = box->createChild<DataGrid>();
        grid->setStretch(1.0f);
        for (int c = 0; c < 4; ++c) grid->addColumn(String("Coluna comprida ") + String::number(c), c == 0 ? firstWidth : kMemoWidths[c]);
        if (withModel)
        {
            model = withModel;
            grid->setModel(model);
        }
        else
        {
            for (int r = 0; r < kMemoRows; ++r) grid->addRow({ scheme(r, 0), scheme(r, 1), scheme(r, 2), scheme(r, 3) });
        }
        screen.h.relayout();
        screen.h.frames(3);
    }
    Harness& h()
    {
        return screen.h;
    }
};

// The same, with a TreeGrid: kMemoRows / 4 roots of three children each.
struct TreeStage
{
    Screen screen;
    TreeGrid* grid = nullptr;
    ct::Vector<TreeGrid::Node*> roots;

    TreeStage(Scheme scheme, float width = kWindowW, bool dynamicFonts = false)
            : screen(width, kWindowH, dynamicFonts)
    {
        BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(0.0f);
        grid = box->createChild<TreeGrid>();
        grid->setStretch(1.0f);
        for (int c = 0; c < 4; ++c) grid->addColumn(String("Coluna comprida ") + String::number(c), kMemoWidths[c]);
        int n = 0;
        for (int r = 0; r < kMemoRows / 4; ++r)
        {
            TreeGrid::Node* root = grid->addRoot({ scheme(n, 0), scheme(n, 1), scheme(n, 2), scheme(n, 3) });
            ++n;
            roots.push_back(root);
            for (int k = 0; k < 3; ++k)
            {
                root->addChild({ scheme(n, 0), scheme(n, 1), scheme(n, 2), scheme(n, 3) });
                ++n;
            }
        }
        grid->rebuild();
        screen.h.relayout();
        screen.h.frames(3);
    }
    Harness& h()
    {
        return screen.h;
    }
};

// Where the glyphs and shapes of the last frame are (x and y of every vertex): compared when the pixels
// cannot tell (a baked font ignores the size).
ct::Vector<float> positions()
{
    ct::Vector<float> out;
    const ct::Vector<DrawVertex>& v = GetDrawList().vertices();
    for (size_t i = 0; i < v.size(); ++i)
    {
        out.push_back(v[i].x);
        out.push_back(v[i].y);
    }
    return out;
}

bool samePositions(const ct::Vector<float>& a, const ct::Vector<float>& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

// The grid drawn at the top, scrolled a few notches down, a long way down, and back up some: the first
// frames of each are the ones that meet the memos of the rows that were in sight before.
template<class Stage>
ct::Vector<Shot> viewsOf(Stage& s)
{
    ct::Vector<Shot> shots;
    const Rect r = s.grid->absoluteRect();
    s.h().moveTo(r.x + 20.0f, r.y + kHeaderH + 40.0f);
    const float steps[] = { 0.0f, -3.0f, -20.0f, 7.0f, -1.0f, 30.0f };
    for (size_t i = 0; i < sizeof steps / sizeof steps[0]; ++i)
    {
        if (steps[i] != 0.0f) s.h().wheel(steps[i]);
        s.h().frames(2);
        shots.push_back(s.screen.take());
    }
    return shots;
}

// Is `history` (drawn, changed, drawn again) what `fresh` (made after the change) is, at every view?
bool sameViews(const ct::Vector<Shot>& history, const ct::Vector<Shot>& fresh, int* firstDifference)
{
    if (history.size() != fresh.size()) return false;
    for (size_t i = 0; i < history.size(); ++i)
        if (differing(history[i], fresh[i], 0, 0, history[i].w, history[i].h) != 0)
        {
            if (firstDifference) *firstDifference = static_cast<int>(i);
            return false;
        }
    return true;
}

// A DataGrid made with `before`, drawn, changed by `change`, and a DataGrid made with `after` and put in
// the same state by `settle` (what the change did that a different scheme does not say), compared at every view.
template<class Change, class Settle>
void staleDataGrid(const char* what, Scheme before, Scheme after, float firstWidthBefore, float firstWidthAfter, Change change, Settle settle)
{
    ct::Vector<Shot> history, fresh;
    {
        DataStage s(before, firstWidthBefore);
        change(s);
        s.h().frames(2);
        history = viewsOf(s);
    }
    {
        DataStage s(after, firstWidthAfter);
        settle(s);
        s.h().frames(2);
        fresh = viewsOf(s);
    }
    int at = -1;
    if (!sameViews(history, fresh, &at)) printf("    (%s: view %d)\n", what, at);
    check(sameViews(history, fresh, nullptr), what);
}

template<class Change, class Settle>
void staleTreeGrid(const char* what, Scheme before, Scheme after, Change change, Settle settle)
{
    ct::Vector<Shot> history, fresh;
    {
        TreeStage s(before);
        change(s);
        s.h().frames(2);
        history = viewsOf(s);
    }
    {
        TreeStage s(after);
        settle(s);
        s.h().frames(2);
        fresh = viewsOf(s);
    }
    int at = -1;
    if (!sameViews(history, fresh, &at)) printf("    (%s: view %d)\n", what, at);
    check(sameViews(history, fresh, nullptr), what);
}

void testDataGridMemosDoNotGoStale()
{
    printf("a DataGrid draws what it would draw new, after a cell, a column, the model, the order or the rows change\n");
    // a cell changes (some of them: the ones that stay keep their memos)
    staleDataGrid("a changed cell shows its new text", schemeA, schemeMixed, 100.0f, 100.0f,
            [](DataStage& s)
            {
                for (int r = 0; r < kMemoRows; ++r)
                    for (int c = 0; c < 4; ++c)
                        if ((r + c) % 3 != 0) s.grid->setCell(r, c, schemeB(r, c));
            },
            [](DataStage&) {});
    // a column gets wider and narrower: the texts that were cut fit, and the ones that fit are cut
    staleDataGrid("a column made wider shows what fits in it", schemeA, schemeA, 100.0f, 300.0f,
            [](DataStage& s) { s.grid->setColumnWidth(0, 300.0f); }, [](DataStage&) {});
    staleDataGrid("a column made narrower cuts what does not fit", schemeB, schemeB, 300.0f, 60.0f,
            [](DataStage& s) { s.grid->setColumnWidth(0, 60.0f); }, [](DataStage&) {});
    // there and back: the memo made first is not the one that is right now
    staleDataGrid("a column made wider and narrower again", schemeA, schemeA, 100.0f, 100.0f,
            [](DataStage& s)
            {
                s.grid->setColumnWidth(0, 300.0f);
                s.h().frames(2);
                s.grid->setColumnWidth(0, 100.0f);
            },
            [](DataStage&) {});
    // the order of the rows (the header clicked: ascending, then descending), which moves each text to
    // the position of another
    staleDataGrid("sorted rows show their own text", schemeA, schemeA, 100.0f, 100.0f,
            [](DataStage& s)
            {
                const Rect r = s.grid->absoluteRect();
                s.h().click(r.x + 50.0f, r.y + kHeaderH * 0.5f);
                s.h().frames(2);
                s.h().click(r.x + 50.0f, r.y + kHeaderH * 0.5f);
            },
            [](DataStage& s)
            {
                const Rect r = s.grid->absoluteRect();
                s.h().click(r.x + 50.0f, r.y + kHeaderH * 0.5f);
                s.h().click(r.x + 50.0f, r.y + kHeaderH * 0.5f);
            });
    // rows go and come: every row below takes the position (and the slot) of the one above
    staleDataGrid("removed rows leave the ones below with their own text", schemeA, schemeA, 100.0f, 100.0f,
            [](DataStage& s)
            {
                s.grid->removeRow(3);
                s.grid->removeRow(3);
                s.grid->removeRow(10);
            },
            [](DataStage& s)
            {
                s.grid->removeRow(3);
                s.grid->removeRow(3);
                s.grid->removeRow(10);
            });
    // a cell edited and committed (the text of the grid's own row changes under its slot)
    staleDataGrid("an edited cell shows what was typed", schemeA, schemeA, 100.0f, 100.0f,
            [](DataStage& s)
            {
                const Rect r = s.grid->absoluteRect();
                const float y = r.y + kHeaderH + kRowH * 1.5f;
                s.h().click(r.x + 20.0f, y);
                s.h().click(r.x + 20.0f, y);
                s.h().type("zz");
                s.h().key(Key::Return);
            },
            [](DataStage& s)
            {
                const Rect r = s.grid->absoluteRect();
                const float y = r.y + kHeaderH + kRowH * 1.5f;
                s.h().click(r.x + 20.0f, y);
                s.h().click(r.x + 20.0f, y);
                s.h().type("zz");
                s.h().key(Key::Return);
            });
    // columns that stretch: the window gets narrower and wider, and so does what the column can show
    staleDataGrid("stretching columns follow the window", schemeA, schemeA, 100.0f, 100.0f,
            [](DataStage& s)
            {
                s.grid->setStretchLastColumn(true);
                s.grid->setColumnStretch(1, true);
                s.h().frames(2);
                s.h().resize(400.0f, kWindowH);
                s.h().relayout();
                s.h().resize(kWindowW, kWindowH);
                s.h().relayout();
            },
            [](DataStage& s)
            {
                s.grid->setStretchLastColumn(true);
                s.grid->setColumnStretch(1, true);
            });
}

// A model: it changes without telling, and tells; the grid shows it either way.
void testModelDataIsNotStaleInTheGrid()
{
    printf("a DataGrid with a model shows the model's data as it is, told or not\n");
    for (int told = 0; told < 2; ++told)
    {
        ct::Vector<Shot> history, fresh;
        {
            TextModel model(schemeA, kMemoRows);
            DataStage s(schemeA, 100.0f, kWindowW, false, &model);
            for (int r = 0; r < kMemoRows; ++r)
                for (int c = 0; c < 4; ++c)
                    if ((r + c) % 3 != 0)
                    {
                        model.set(r, c, schemeB(r, c));
                        if (told) model.announce(r, c);
                    }
            s.h().frames(2);
            history = viewsOf(s);
        }
        {
            TextModel model(schemeMixed, kMemoRows);
            DataStage s(schemeMixed, 100.0f, kWindowW, false, &model);
            fresh = viewsOf(s);
        }
        check(sameViews(history, fresh, nullptr), told ? "data changed and announced shows" : "data changed without a word shows too");
    }
    // one cell, back and forth, each frame (one window at a time: the grid is drawn through all the
    // changes first, and the grids made after each of them one by one after)
    {
        ct::Vector<Shot> history;
        {
            TextModel model(schemeA, kMemoRows);
            DataStage s(schemeA, 100.0f, kWindowW, false, &model);
            for (int i = 0; i < 6; ++i)
            {
                model.set(2, 1, i % 2 ? longText() : String("curto"));
                s.h().frames(2);
                history.push_back(s.screen.take());
            }
        }
        bool same = true;
        for (int i = 0; i < 6; ++i)
        {
            TextModel again(schemeA, kMemoRows);
            again.set(2, 1, i % 2 ? longText() : String("curto"));
            DataStage f(schemeA, 100.0f, kWindowW, false, &again);
            const Shot expected = f.screen.take();
            const int nd = differing(history[static_cast<size_t>(i)], expected, 0, 0, expected.w, expected.h);
            if (nd) printf("    change %d: %d pixels differ\n", i, nd);
            same = same && nd == 0;
        }
        check(same, "one cell flipped between a long and a short text, each frame");
    }
}

void testTreeGridMemosDoNotGoStale()
{
    printf("a TreeGrid draws what it would draw new, after a cell, a node folding or a window resize\n");
    staleTreeGrid("a changed cell shows its new text", schemeA, schemeMixed,
            [](TreeStage& s)
            {
                int n = 0;
                for (size_t r = 0; r < s.roots.size(); ++r)
                {
                    TreeGrid::Node* root = s.roots[r];
                    for (int c = 0; c < 4; ++c)
                        if ((n + c) % 3 != 0) root->cells[static_cast<size_t>(c)] = schemeB(n, c);
                    ++n;
                    for (size_t k = 0; k < root->children.size(); ++k, ++n)
                        for (int c = 0; c < 4; ++c)
                            if ((n + c) % 3 != 0) root->children[k]->cells[static_cast<size_t>(c)] = schemeB(n, c);
                }
            },
            [](TreeStage&) {});
    // nodes folded and opened: the rows below move to other positions (other slots)
    staleTreeGrid("folded nodes leave the rows below with their own text", schemeA, schemeA,
            [](TreeStage& s)
            {
                s.roots[1]->setExpanded(false);
                s.roots[4]->setExpanded(false);
                s.h().frames(2);
                s.roots[1]->setExpanded(true);
                s.roots[2]->setExpanded(false);
            },
            [](TreeStage& s)
            {
                s.roots[4]->setExpanded(false);
                s.roots[2]->setExpanded(false);
                s.roots[4]->setExpanded(true);
                s.roots[4]->setExpanded(false);
            });
    staleTreeGrid("stretching columns follow the window", schemeA, schemeA,
            [](TreeStage& s)
            {
                s.grid->setStretchLastColumn(true);
                s.grid->setColumnStretch(1, true);
                s.h().frames(2);
                s.h().resize(420.0f, kWindowH);
                s.h().relayout();
                s.h().resize(kWindowW, kWindowH);
                s.h().relayout();
            },
            [](TreeStage& s)
            {
                s.grid->setStretchLastColumn(true);
                s.grid->setColumnStretch(1, true);
            });
    // the sort by a column (which reorders the children, and the rows)
    staleTreeGrid("sorted nodes show their own text", schemeA, schemeA,
            [](TreeStage& s)
            {
                const Rect r = s.grid->absoluteRect();
                s.h().click(r.x + 50.0f, r.y + kHeaderH * 0.5f);
                s.h().frames(2);
                s.h().click(r.x + 50.0f, r.y + kHeaderH * 0.5f);
            },
            [](TreeStage& s)
            {
                const Rect r = s.grid->absoluteRect();
                s.h().click(r.x + 50.0f, r.y + kHeaderH * 0.5f);
                s.h().click(r.x + 50.0f, r.y + kHeaderH * 0.5f);
            });
}

// The font size changes (the theme's): a baked font does not follow it, so the glyphs are compared by
// where they are, in a window with the fonts made at each size, as an application has.
struct ThemeSizeGuard
{
    float saved;
    ThemeSizeGuard() : saved(Theme::instance().fontSize) {}
    ~ThemeSizeGuard()
    {
        Theme::instance().fontSize = saved;
    }
};

void testMemosFollowTheFontSize()
{
    printf("the grids draw their text at the new size after the theme's font size changes\n");
    ThemeSizeGuard guard;
    const float sizes[] = { 14.0f, 22.0f, 11.0f, 16.0f };
    const size_t count = sizeof sizes / sizeof sizes[0];
    for (int tree = 0; tree < 2; ++tree)
    {
        // one window at a time: the grid is drawn at each size, then a grid made at each size
        ct::Vector<ct::Vector<float> > history;
        Theme::instance().fontSize = sizes[0];
        if (tree)
        {
            TreeStage s(schemeA, kWindowW, true);
            for (size_t i = 1; i < count; ++i)
            {
                Theme::instance().fontSize = sizes[i];
                s.h().frames(2);
                history.push_back(positions());
            }
        }
        else
        {
            DataStage s(schemeA, 100.0f, kWindowW, true);
            for (size_t i = 1; i < count; ++i)
            {
                Theme::instance().fontSize = sizes[i];
                s.h().frames(2);
                history.push_back(positions());
            }
        }
        for (size_t i = 1; i < count; ++i)
        {
            Theme::instance().fontSize = sizes[i];
            if (tree)
            {
                TreeStage fresh(schemeA, kWindowW, true);
                fresh.h().frames(2);
                check(samePositions(history[i - 1], positions()), "a TreeGrid at the size the theme has now");
            }
            else
            {
                DataStage fresh(schemeA, 100.0f, kWindowW, true);
                fresh.h().frames(2);
                check(samePositions(history[i - 1], positions()), "a DataGrid at the size the theme has now");
            }
        }
    }
    // the window made taller and shorter, and narrower: more and fewer cells in sight than the memos were made for
    const float windows[][2] = { { 640.0f, 600.0f }, { 400.0f, 200.0f }, { 900.0f, 700.0f }, { 640.0f, 320.0f } };
    const size_t windowCount = sizeof windows / sizeof windows[0];
    Theme::instance().fontSize = sizes[0];
    ct::Vector<ct::Vector<float> > history;
    {
        DataStage s(schemeA, 100.0f, kWindowW, true);
        for (size_t i = 0; i < windowCount; ++i)
        {
            s.h().resize(windows[i][0], windows[i][1]);
            s.h().relayout();
            history.push_back(positions());
        }
    }
    for (size_t i = 0; i < windowCount; ++i)
    {
        Screen screen(windows[i][0], windows[i][1], true);
        BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(0.0f);
        DataGrid* grid = box->createChild<DataGrid>();
        grid->setStretch(1.0f);
        for (int c = 0; c < 4; ++c) grid->addColumn(String("Coluna comprida ") + String::number(c), c == 0 ? 100.0f : kMemoWidths[c]);
        for (int r = 0; r < kMemoRows; ++r) grid->addRow({ schemeA(r, 0), schemeA(r, 1), schemeA(r, 2), schemeA(r, 3) });
        screen.h.relayout();
        screen.h.frames(3);
        check(samePositions(history[i], positions()), "a window of another size shows the cells it has room for");
    }
}

// What the memos take is a table of the cells in sight, not of the rows (ElideMemos::reserve): the table
// is as big as the window asks and gives back what it does not need; and a grid of fifty thousand rows, the
// slots of which have been taken over by thousands of rows, still shows each one's own text.
void testMemosAreBoundedByTheCellsInSight()
{
    printf("the memos of a grid are as many as the cells in sight, however many rows it has\n");
    ElideMemos table;
    table.reserve(15, 6);
    check(table.capacity() == 90 && table.cols() == 6, "a table has a memo for each cell asked for");
    table.reserve(10, 4);
    check(table.capacity() == 90, "asked for less (not four times less) it keeps what it has");
    table.reserve(3, 3);
    check(table.capacity() == 9, "asked for much less it gives back what it does not need");
    table.reserve(20, 8);
    check(table.capacity() == 160, "and for more it grows");
    table.reserve(0, 0);
    check(table.capacity() >= 1, "never to nothing: a grid of no size has a slot");

    ct::Vector<Shot> history, fresh;
    for (int pass = 0; pass < 2; ++pass)
    {
        Screen screen(kWindowW, kWindowH);
        BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(0.0f);
        DataGrid* grid = box->createChild<DataGrid>();
        grid->setStretch(1.0f);
        for (int c = 0; c < 4; ++c) grid->addColumn(String("C") + String::number(c), kMemoWidths[c]);
        for (int r = 0; r < 50000; ++r) grid->addRow({ schemeA(r, 0), schemeA(r, 1), schemeA(r, 2), schemeA(r, 3) });
        screen.h.relayout();
        screen.h.frames(3);
        const Rect r = grid->absoluteRect();
        screen.h.moveTo(r.x + 20.0f, r.y + kHeaderH + 40.0f);
        // pass 0 goes there by thousands of small steps (every slot is taken over many times), pass 1 in one
        if (pass == 0)
        {
            for (int i = 0; i < 3000; ++i) screen.h.wheel(-1.0f);
            for (int i = 0; i < 1000; ++i) screen.h.wheel(1.0f);
        }
        else
        {
            screen.h.wheel(-2000.0f);
        }
        screen.h.frames(2);
        (pass == 0 ? history : fresh).push_back(screen.take());
    }
    check(history.size() == 1 && fresh.size() == 1 && differing(history[0], fresh[0], 0, 0, history[0].w, history[0].h) == 0,
            "after thousands of rows scrolled through, the rows in sight are the ones that are there");
}

// ---- stretching columns of a DataGrid with a model ----
//
// The columns of a grid with a model come from the grid (addColumn) or, when it has none yet, from the
// model's headers (120 wide each). Either way they stretch as the others do, and the model's rows are
// found where the stretched columns are.
//
// (TreeGrid takes no model: it has no setModel.)

// A grid of the model in a window, with what `prepare` does to it (addColumn, setColumnStretch) before
// the model is set.
struct ModelRig
{
    Harness h;
    DataGrid* grid = nullptr;
    int sorted = -1;

    template<class Prepare>
    ModelRig(float windowWidth, TextModel* model, Prepare prepare)
            : h(windowWidth, kWindowH)
    {
        BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(0.0f);
        grid = box->createChild<DataGrid>();
        grid->setStretch(1.0f);
        prepare(grid);
        grid->setModel(model);
        int* where = &sorted;
        grid->columnSorted.connect([where](int column) { *where = column; });
        h.relayout();
    }
    Rect rect() const
    {
        return grid->absoluteRect();
    }
    int headerAt(float x)
    {
        sorted = -1;
        h.click(rect().x + x, rect().y + kHeaderH * 0.5f);
        return sorted;
    }
    void resizeTo(float width)
    {
        h.resize(width, kWindowH);
        h.relayout();
    }
    // A drag that starts on the header at x and moves dx pixels to the right.
    void drag(float x, float dx)
    {
        h.moveTo(rect().x + x, rect().y + kHeaderH * 0.5f);
        h.down();
        h.moveTo(rect().x + x + dx, rect().y + kHeaderH * 0.5f);
        h.up();
    }
};

void testStretchWithAModel()
{
    printf("stretching columns of a DataGrid with a model\n");
    // columns given to the grid before the model, two of them asked to stretch (the least they get: 90 and 160)
    {
        TextModel model(schemeA, 8); // few rows: no vertical bar, the columns have the whole width
        ModelRig r(640.0f, &model,
                [](DataGrid* g)
                {
                    for (int c = 0; c < 4; ++c) g->addColumn(String("C") + String::number(c), kMemoWidths[c]);
                    g->setColumnStretch(2, true);
                    g->setColumnStretch(3, true);
                });
        // 100 + 140 fixed, 90 and 160 stretching: (640 - 240) / 2 = 200 each
        check(near(r.grid->columnWidth(0), 100.0f) && near(r.grid->columnWidth(1), 140.0f) && near(r.grid->columnWidth(2), 200.0f) && near(r.grid->columnWidth(3), 200.0f),
                "the columns share what the fixed ones leave over");
        check(r.grid->columnStretch(2) && r.grid->columnStretch(3) && !r.grid->columnStretch(0), "the flags set before the model are there after it");
        check(r.headerAt(100.0f + 140.0f + 100.0f) == 2 && r.headerAt(100.0f + 140.0f + 200.0f + 100.0f) == 3 && r.headerAt(636.0f) == 3, "a header click lands on the stretched column under it");
        r.resizeTo(500.0f);
        // 260 left for two columns: the equal share, 130, is under the 160 the fourth was given, which keeps it
        check(near(r.grid->columnWidth(2), 100.0f) && near(r.grid->columnWidth(3), 160.0f), "they follow the window narrower (the one with the larger own width keeps it)");
        r.resizeTo(300.0f);
        check(near(r.grid->columnWidth(2), 90.0f) && near(r.grid->columnWidth(3), 160.0f), "and stay at their own width when it does not hold them");
        r.resizeTo(900.0f);
        check(near(r.grid->columnWidth(2), 330.0f) && near(r.grid->columnWidth(3), 330.0f), "and wider");
        // the model's rows are added and taken away: nothing of the widths changes
        model.set(3, 1, "x");
        model.modelReset.emit();
        model.rowsInserted.emit();
        model.rowsRemoved.emit();
        r.h.frames(2);
        check(near(r.grid->columnWidth(2), 330.0f) && near(r.grid->columnWidth(0), 100.0f), "the model's signals leave the widths alone");
        // the edge of a fixed column is dragged (the stretching ones take what it gives), the edge of a
        // stretching column is not
        r.drag(100.0f, 30.0f);
        check(near(r.grid->columnWidth(0), 130.0f) && near(r.grid->columnWidth(2), 315.0f) && near(r.grid->columnWidth(3), 315.0f), "dragging the edge of a fixed column widens it and the stretching ones give way");
        r.drag(130.0f + 140.0f + 315.0f, 40.0f);
        check(near(r.grid->columnWidth(0), 130.0f) && near(r.grid->columnWidth(1), 140.0f) && near(r.grid->columnWidth(2), 315.0f) && near(r.grid->columnWidth(3), 315.0f),
                "dragging the edge of a stretching column changes nothing");
    }
    // the columns of the model: 4 of 120, the last one stretching
    {
        TextModel model(schemeA, 8); // few rows: no vertical bar, the columns have the whole width
        ModelRig r(640.0f, &model, [](DataGrid* g) { g->setStretchLastColumn(true); });
        check(near(r.grid->columnWidth(0), 120.0f) && near(r.grid->columnWidth(2), 120.0f) && near(r.grid->columnWidth(3), 640.0f - 360.0f),
                "columns made from the model's headers stretch too (the last one: 280)");
        r.resizeTo(400.0f);
        check(near(r.grid->columnWidth(3), 120.0f), "and stay at 120 when there is no more room");
        r.resizeTo(1000.0f);
        check(near(r.grid->columnWidth(3), 640.0f), "and grow with the window");
        check(r.headerAt(996.0f) == 3, "to its edge");
    }
    // a cell of the stretched column is edited, and the model is the one that gets it
    {
        TextModel model(schemeA, 8); // few rows: no vertical bar, the columns have the whole width
        ModelRig r(640.0f, &model,
                [](DataGrid* g)
                {
                    for (int c = 0; c < 4; ++c) g->addColumn(String("C") + String::number(c), 100.0f);
                    g->setStretchLastColumn(true);
                });
        int editedRow = -1, editedColumn = -1;
        r.grid->cellEdited.connect([&editedRow, &editedColumn](int row, int column) { editedRow = row; editedColumn = column; });
        const float y = r.rect().y + kHeaderH + kRowH * 1.5f;
        r.h.click(r.rect().x + 500.0f, y); // 400 + 240: well past the 100 the last column was given
        r.h.click(r.rect().x + 500.0f, y);
        r.h.type("!");
        r.h.key(Key::Return);
        check(editedRow == 1 && editedColumn == 3, "a double click far in the last column edits its cell");
        check(model.data(ModelIndex(1, 3)) == schemeA(1, 3) + "!", "and the model has what was typed");
    }
    // painted as a grid with those widths, every pixel
    {
        float effective[4];
        Shot stretched, fixed;
        {
            TextModel model(schemeA, 8); // few rows: no vertical bar, the columns have the whole width
            Screen screen(500.0f, kWindowH);
            BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
            box->setPadding(0.0f);
            DataGrid* grid = box->createChild<DataGrid>();
            grid->setStretch(1.0f);
            for (int c = 0; c < 4; ++c) grid->addColumn(String("Coluna comprida ") + String::number(c), kMemoWidths[c]);
            grid->setColumnStretch(0, true);
            grid->setColumnStretch(2, true);
            grid->setModel(&model);
            screen.h.relayout();
            screen.h.frames(3);
            for (int c = 0; c < 4; ++c) effective[c] = grid->columnWidth(c);
            stretched = screen.take();
        }
        {
            TextModel model(schemeA, 8); // few rows: no vertical bar, the columns have the whole width
            Screen screen(500.0f, kWindowH);
            BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
            box->setPadding(0.0f);
            DataGrid* grid = box->createChild<DataGrid>();
            grid->setStretch(1.0f);
            for (int c = 0; c < 4; ++c) grid->addColumn(String("Coluna comprida ") + String::number(c), effective[c]);
            grid->setModel(&model);
            screen.h.relayout();
            screen.h.frames(3);
            fixed = screen.take();
        }
        check(differing(stretched, fixed, 0, 0, stretched.w, stretched.h) == 0, "a grid with a model and stretching columns is painted like one with those widths");
    }
}

// A text that fits asks for no clip and no allocation: only the clips of the header and the rows
// are pushed, and a frame of a grid whose texts all fit allocates nothing.
void testFittingTextCostsNothingExtra()
{
    printf("a DataGrid whose text fits pushes no clip per cell and allocates nothing\n");
    DataSpec empty;
    DataSpec fits;
    fits.cell0 = "Cabe";
    fits.cell1 = "Cabe tambem";
    int clipsEmpty = 0, clipsFits = 0;
    dataShot(empty, nullptr, &clipsEmpty);
    dataShot(fits, nullptr, &clipsFits);
    check(clipsEmpty > 0 && clipsEmpty == clipsFits, "fitting texts push no clip of their own");

    Harness h(kWindowW, kWindowH);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    DataGrid* grid = box->createChild<DataGrid>();
    grid->setStretch(1.0f);
    for (int c = 0; c < 6; ++c) grid->addColumn(String("Coluna ") + String::number(c), 280.0f);
    for (int r = 0; r < 60; ++r)
    {
        ct::Vector<String> cells;
        for (int c = 0; c < 6; ++c) cells.push_back(String("Entrevista ") + String::number(r) + " com o cliente " + String::number(c));
        grid->addRow(cells);
    }
    h.relayout();
    h.frames(5);
    const unsigned long long before = gAllocs;
    for (int i = 0; i < 10; ++i) h.frame();
    check(gAllocs - before == 0, "ten frames of a grid with 30 character cells allocate nothing");
    if (gAllocs - before != 0) printf("    %llu allocations\n", gAllocs - before);
}

} // namespace

// A scenario name on the command line runs only that one.
int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testDataGridHeaderStaysInItsColumn);
    RUN(testDataGridHeaderInAColumnTooNarrow);
    RUN(testDataGridCellsStayInTheirColumn);
    RUN(testDataGridElidesWithDots);
    RUN(testDataGridSortArrowStaysInItsColumn);
    RUN(testDataGridEditedCellStaysInItsColumn);
    RUN(testTreeGridHeaderStaysInItsColumn);
    RUN(testTreeGridCellsStayInTheirColumn);
    RUN(testTreeGridIndentationStaysInTheFirstColumn);
    RUN(testTreeGridElidesWithDots);
    RUN(testTreeGridEditedCellStaysInItsColumn);
    RUN(testFixedColumnsKeepTheirWidth);
    RUN(testTreeGridSetColumnWidth);
    RUN(testGridVerticalBarThumbAndTrack);
    RUN(testGridHorizontalBarThumbAndTrack);
    RUN(testGridScrollStaysInsideTheContent);
    RUN(testGridColumnsEndWhereTheBarBegins);
    RUN(testTreeGridDoubleClickIsPerGridAndForgottenWithTheNode);
    RUN(testTreeGridDrawsNodeIconsWithoutAnAtlas);
    RUN(testGridIgnoresANaNColumnWidth);
    RUN(testDataGridMemosDoNotGoStale);
    RUN(testModelDataIsNotStaleInTheGrid);
    RUN(testTreeGridMemosDoNotGoStale);
    RUN(testMemosFollowTheFontSize);
    RUN(testMemosAreBoundedByTheCellsInSight);
    RUN(testStretchWithAModel);
    RUN(testStretchingColumns);
    RUN(testFittingTextCostsNothingExtra);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
