#pragma once

#include "RetainedBase.hpp"


namespace ig
{
namespace retained
{
// ═════════════════════════════════════════════════════════════════════════════
//  Theme - centralized colors/sizes for all widgets
// ═════════════════════════════════════════════════════════════════════════════


// ═════════════════════════════════════════════════════════════════════════════
//  Colours of the widgets that paint their own panels (a dock, a console, a tab bar). Each group's defaults are the
//  values these widgets always painted, which is the dark preset; light() gives the light one. Presets built on
//  those two (macOS, Windows XP) change only what their look needs.
// ═════════════════════════════════════════════════════════════════════════════

/// DockPanel: the tab bars, the tabs, the splitter handles and the leaf background.
struct DockColors
{
    Color leafBg = Color(38, 38, 42, 255);          // behind the content of a leaf
    Color border = Color(60, 60, 65, 255);          // the frame round a leaf
    Color handle = Color(50, 50, 53, 255);          // the splitter handle between two leaves
    Color handleHover = Color(90, 90, 95, 255);     // ... under the pointer or dragged
    Color barBg = Color(30, 30, 33, 255);           // the tab bar
    Color barBorder = Color(55, 55, 60, 255);       // the line under the tab bar
    Color tabBg = Color(35, 35, 38, 255);           // a tab that is not the current one
    Color tabActiveBg = Color(45, 45, 50, 255);     // the current tab
    Color accent = Color(120, 160, 220, 255);       // the line under the current tab
    Color tabText = Color(150, 150, 155, 255);      // the name on a tab that is not the current one
    Color tabTextActive = Color(220, 220, 220, 255); // the name on the current tab
    Color closeIcon = Color(110, 110, 115, 255);    // the cross of a tab that is not the current one
    Color closeIconActive = Color(170, 170, 175, 255); // the cross of the current tab
    Color closeIconHot = Color(235, 235, 235, 255); // the cross under the pointer
    Color closeHotBg = Color(90, 90, 96, 255);      // the square behind the cross under the pointer
    Color dragBg = Color(50, 55, 65, 230);          // the tab that follows the pointer while it is dragged
    Color dragBorder = Color(100, 140, 220, 180);
    Color dragText = Color(220, 230, 240, 255);

    static DockColors light()
    {
        DockColors c;
        c.leafBg = Color(250, 250, 252, 255);
        c.border = Color(200, 200, 205, 255);
        c.handle = Color(214, 214, 220, 255);
        c.handleHover = Color(176, 176, 186, 255);
        c.barBg = Color(232, 232, 236, 255);
        c.barBorder = Color(206, 206, 212, 255);
        c.tabBg = Color(222, 222, 228, 255);
        c.tabActiveBg = Color(250, 250, 252, 255);
        c.accent = Color(0, 120, 215, 255);
        c.tabText = Color(80, 80, 86, 255);
        c.tabTextActive = Color(30, 30, 30, 255);
        c.closeIcon = Color(120, 120, 126, 255);
        c.closeIconActive = Color(90, 90, 96, 255);
        c.closeIconHot = Color(20, 20, 24, 255);
        c.closeHotBg = Color(196, 196, 204, 255);
        c.dragBg = Color(238, 242, 250, 235);
        c.dragBorder = Color(0, 120, 215, 200);
        c.dragText = Color(30, 30, 36, 255);
        return c;
    }
};

/// ConsoleWidget: the tool bar, the log, the input row and the colour of each level. ConsoleWidget::setBgColor still
/// sets the background of one console; the scroll bar follows scrollbarThumb and panelColor, like every other scroll bar.
struct ConsoleColors
{
    Color bg = Color(22, 24, 28, 255);          // the log
    Color toolbarBg = Color(30, 32, 38, 255);
    Color inputBg = Color(26, 28, 34, 255);
    Color line = Color(50, 52, 60, 255);        // the lines between the three parts
    Color buttonOff = Color(40, 42, 48, 255);   // a level filter that is off
    Color buttonOffText = Color(80, 80, 90, 255);
    Color clearBg = Color(50, 52, 60, 255);
    Color clearIcon = Color(170, 170, 180, 255);
    Color searchBg = Color(20, 22, 26, 255);
    Color searchBorder = Color(70, 120, 200, 255);
    Color searchHint = Color(80, 80, 90, 255);  // "Search..." in the empty box
    Color text = Color(200, 210, 220, 255);     // an Info message, what is typed in the search box and in the input row
    Color trace = Color(140, 140, 150, 255);    // a Trace message
    Color warn = Color(240, 200, 80, 255);
    Color error = Color(240, 80, 80, 255);
    Color infoTag = Color(120, 180, 255, 255);  // the "I" filter, when on (the others take the colours of their level)
    Color warnBg = Color(60, 50, 20, 40);       // the row of a warning
    Color errorBg = Color(80, 25, 25, 50);
    Color selection = Color(50, 70, 120, 80);   // the selected row
    Color timestamp = Color(90, 90, 100, 255);
    Color badgeText = Color(100, 140, 200, 255); // the "x3" of a repeated message
    Color badgeBg = Color(40, 60, 100, 120);
    Color prompt = Color(100, 160, 240, 255);   // the ">" of the input row
    Color caret = Color(180, 200, 240, 200);

    static ConsoleColors light()
    {
        ConsoleColors c;
        c.bg = Color(250, 250, 252, 255);
        c.toolbarBg = Color(238, 238, 242, 255);
        c.inputBg = Color(244, 244, 247, 255);
        c.line = Color(210, 210, 216, 255);
        c.buttonOff = Color(228, 228, 232, 255);
        c.buttonOffText = Color(96, 96, 102, 255);
        c.clearBg = Color(222, 222, 228, 255);
        c.clearIcon = Color(80, 80, 90, 255);
        c.searchBg = Color(255, 255, 255, 255);
        c.searchBorder = Color(0, 120, 215, 255);
        c.searchHint = Color(110, 110, 118, 255);
        c.text = Color(30, 34, 40, 255);
        c.trace = Color(84, 84, 94, 255);
        c.warn = Color(120, 75, 0, 255);
        c.error = Color(170, 20, 20, 255);
        c.infoTag = Color(0, 80, 160, 255);
        c.warnBg = Color(255, 235, 170, 120);
        c.errorBg = Color(255, 200, 200, 90);
        c.selection = Color(0, 120, 215, 60);
        c.timestamp = Color(96, 96, 106, 255);
        c.badgeText = Color(30, 90, 170, 255);
        c.badgeBg = Color(180, 210, 245, 120);
        c.prompt = Color(0, 100, 190, 255);
        c.caret = Color(30, 60, 120, 220);
        return c;
    }
};

/// Tabs (TabLayout) and Splitter.
struct LayoutColors
{
    Color tabBarBg = Color(38, 40, 44, 255);
    Color tabBg = Color(42, 44, 50, 255);        // a tab that is not the current one
    Color tabActiveBg = Color(55, 58, 65, 255);
    Color tabAccent = Color(100, 160, 220, 255); // the line on the edge of the current tab
    Color tabTextInactive = Color(0, 0, 0, 0);   // alpha 0: textDisabled
    Color tabCloseIcon = Color(100, 100, 105, 150);
    Color tabCloseIconActive = Color(160, 160, 165, 200);
    Color splitHandle = Color(65, 65, 70, 255);
    Color splitHandleHover = Color(80, 80, 90, 255);
    Color splitHandleDrag = Color(100, 100, 110, 255);
    Color splitGrip = Color(130, 130, 138, 200);    // the dots in the middle of the handle
    Color splitGripHot = Color(180, 180, 190, 230);

    static LayoutColors light()
    {
        LayoutColors c;
        c.tabBarBg = Color(232, 232, 236, 255);
        c.tabBg = Color(222, 222, 228, 255);
        c.tabActiveBg = Color(250, 250, 252, 255);
        c.tabAccent = Color(0, 120, 215, 255);
        c.tabTextInactive = Color(80, 80, 86, 255);
        c.tabCloseIcon = Color(120, 120, 126, 200);
        c.tabCloseIconActive = Color(90, 90, 96, 255);
        c.splitHandle = Color(214, 214, 220, 255);
        c.splitHandleHover = Color(190, 190, 200, 255);
        c.splitHandleDrag = Color(160, 160, 172, 255);
        c.splitGrip = Color(130, 130, 138, 200);
        c.splitGripHot = Color(70, 70, 80, 230);
        return c;
    }
};

/// SearchBar, Breadcrumbs, Outliner, RichText, ProgressBar, StatusBar and the pickers: the small colours they paint with.
struct AppColors
{
    Color crumbHover = Color(120, 170, 220, 255);      // a breadcrumb link under the pointer
    Color crumbSeparator = Color(100, 100, 105, 255);
    Color searchIcon = Color(140, 140, 145, 255);      // the magnifying glass and the clear cross of a SearchBar
    Color rowHover = Color(255, 255, 255, 15);         // the row of an Outliner under the pointer
    Color outlinerArrow = Color(180, 180, 185, 255);
    Color eyeOn = Color(180, 180, 185, 255);
    Color eyeOff = Color(80, 80, 85, 255);
    Color lockOn = Color(200, 150, 50, 255);
    Color lockOff = Color(100, 100, 105, 255);
    Color richCodeBg = Color(25, 25, 28, 255);         // inline code and code blocks of a RichText
    Color richCode = Color(180, 210, 180, 255);
    Color richBold = Color(255, 255, 255, 255);
    Color richItalic = Color(200, 200, 205, 255);
    Color richLink = Color(100, 170, 230, 255);
    Color gridCellDim = Color(170, 170, 175, 255);     // the cells after the first of an unselected TreeGrid row
    Color progressText = Color(220, 220, 220, 255);    // the text a ProgressBar shows (when it was not given a colour)
    Color dateHeader = Color(140, 140, 160, 255);      // the day names of a DatePicker
    Color timeSeparator = Color(120, 120, 130, 255);   // the colons of a TimePicker
    Color statusBg = Color(35, 36, 40, 255);           // a StatusBar (when it was not given a colour)

    static AppColors light()
    {
        AppColors c;
        c.crumbHover = Color(0, 100, 190, 255);
        c.crumbSeparator = Color(130, 130, 136, 255);
        c.searchIcon = Color(110, 110, 116, 255);
        c.rowHover = Color(0, 0, 0, 15);
        c.outlinerArrow = Color(80, 80, 86, 255);
        c.eyeOn = Color(80, 80, 86, 255);
        c.eyeOff = Color(170, 170, 176, 255);
        c.lockOn = Color(170, 110, 0, 255);
        c.lockOff = Color(150, 150, 156, 255);
        c.richCodeBg = Color(235, 235, 240, 255);
        c.richCode = Color(30, 100, 40, 255);
        c.richBold = Color(0, 0, 0, 255);
        c.richItalic = Color(60, 60, 66, 255);
        c.richLink = Color(0, 90, 190, 255);
        c.gridCellDim = Color(90, 90, 96, 255);
        c.progressText = Color(30, 30, 30, 255);
        c.dateHeader = Color(90, 90, 110, 255);
        c.timeSeparator = Color(90, 90, 100, 255);
        c.statusBg = Color(232, 232, 236, 255);
        return c;
    }
};

/// FileDialog: its dividers, its rows and the colours of what is listed.
struct FileColors
{
    Color divider = Color(60, 62, 66, 255);     // the lines round the sidebar and the preview
    Color headerLine = Color(55, 58, 65, 255);  // the lines round the column headers
    Color columnSeparator = Color(105, 110, 122, 180);
    Color sortArrow = Color(150, 155, 165, 200);
    Color selectionBg = Color(40, 70, 120, 200);
    Color hoverBg = Color(50, 55, 65, 100);
    Color dirText = Color(120, 180, 255, 255);  // the name of a folder
    Color sizeText = Color(160, 160, 165, 255);
    Color dateText = Color(140, 140, 145, 255);
    Color previewHint = Color(100, 100, 110, 255); // "No preview"
    Color previewSize = Color(130, 130, 140, 255); // the size of the image under the preview

    static FileColors light()
    {
        FileColors c;
        c.divider = Color(206, 206, 212, 255);
        c.headerLine = Color(214, 214, 220, 255);
        c.columnSeparator = Color(176, 176, 186, 180);
        c.sortArrow = Color(90, 90, 100, 200);
        c.selectionBg = Color(190, 215, 245, 255);
        c.hoverBg = Color(0, 0, 0, 20);
        c.dirText = Color(0, 80, 170, 255);
        c.sizeText = Color(90, 90, 96, 255);
        c.dateText = Color(90, 90, 96, 255);
        c.previewHint = Color(110, 110, 118, 255);
        c.previewSize = Color(100, 100, 108, 255);
        return c;
    }
};

/// The property grid: the text of a value over its input box, and the description under the grid.
struct PropColors
{
    Color stringValue = Color(200, 220, 255, 255);
    Color colorHex = Color(180, 180, 180, 255);
    Color comboValue = Color(180, 220, 180, 255);
    Color sliderText = Color(230, 230, 230, 255);   // over the filled bar of a slider or a range
    Color descName = Color(220, 220, 225, 255);

    static PropColors light()
    {
        PropColors c;
        c.stringValue = Color(0, 70, 150, 255);
        c.colorHex = Color(80, 80, 86, 255);
        c.comboValue = Color(20, 100, 40, 255);
        c.sliderText = Color(20, 20, 24, 255);
        c.descName = Color(30, 30, 34, 255);
        return c;
    }
};

/// The editors that paint a canvas of their own: the node editor, the asset browser, the thumbnail grid and the timeline
/// (its clips and tracks keep the colours of the project; this is what is behind and between them).
struct EditorColors
{
    // NodeEditor
    Color nodeBg = Color(32, 34, 38, 255);
    Color nodeBorder = Color(50, 52, 58, 255);
    Color nodeGridMinor = Color(40, 42, 48, 255);
    Color nodeGridMajor = Color(50, 52, 58, 255);
    Color nodeBody = Color(40, 42, 48, 240);
    Color nodeBodySelected = Color(50, 55, 65, 240);
    Color nodeEdge = Color(60, 62, 70, 255);        // the frame of a node that is not selected
    Color nodePinLabel = Color(190, 192, 200, 220);
    // AssetBrowser
    Color assetBg = Color(22, 24, 28, 255);
    Color assetBar = Color(30, 33, 38, 255);
    Color assetLine = Color(45, 48, 55, 255);
    Color assetPath = Color(160, 165, 175, 255);
    Color assetName = Color(170, 175, 185, 255);    // the name under a thumbnail that is not selected
    Color assetNameSelected = Color(220, 225, 235, 255);
    Color assetRowName = Color(180, 185, 195, 255); // the name in the list view
    Color assetRowNameSelected = Color(230, 235, 245, 255);
    Color assetButtonOff = Color(40, 44, 52, 255);  // the view button that is not the current one
    Color assetRowSelected = Color(50, 80, 130, 255);
    Color assetRowEven = Color(26, 28, 34, 255);
    Color assetRowOdd = Color(30, 33, 38, 255);
    Color assetScrollThumb = Color(60, 65, 75, 200);
    // ThumbnailGrid
    Color thumbBg = Color(24, 26, 30, 255);
    Color thumbEdge = Color(50, 52, 58, 255);
    Color thumbLabel = Color(180, 182, 190, 255);
    Color thumbLabelSelected = Color(255, 220, 80, 255);
    // CodeEditor (the colours of the code come from the syntax highlighter; this is the rest)
    Color codeCurrentLine = Color(45, 50, 60, 255); // the line of the cursor, while the editor has the focus
    Color minimapBg = Color(25, 28, 34, 240);
    Color minimapSlider = Color(180, 195, 220, 30); // the part of the minimap that is on screen
    Color minimapSliderEdge = Color(140, 160, 190, 60);
    Color minimapLine = Color(255, 255, 255, 15);   // the line of the cursor in the minimap
    Color minimapEdge = Color(50, 55, 65, 255);     // the line between the text and the minimap
    // Timeline
    Color tlBg = Color(30, 32, 36, 255);
    Color tlBorder = Color(50, 52, 58, 255);
    Color tlRulerBg = Color(38, 40, 46, 255);
    Color tlRulerLine = Color(60, 62, 68, 255);
    Color tlTickMajor = Color(80, 82, 90, 255);
    Color tlTickMinor = Color(55, 57, 63, 255);
    Color tlRulerText = Color(150, 152, 160, 200);
    Color tlRulerTextMinor = Color(110, 112, 120, 200);
    Color tlTrackName = Color(180, 180, 180, 255);
    Color tlRowEven = Color(34, 36, 40, 255);
    Color tlRowOdd = Color(30, 32, 36, 255);
    Color tlHeader = Color(36, 38, 44, 255);
    Color tlHeaderSelected = Color(52, 60, 80, 255);
    Color tlButtonOff = Color(56, 58, 66, 255);     // the mute and lock buttons when they are off
    Color tlButtonText = Color(235, 235, 240, 255);
    Color tlTrackLine = Color(45, 47, 52, 255);
    Color tlHeaderLine = Color(50, 52, 58, 255);

    static EditorColors light()
    {
        EditorColors c;
        c.nodeBg = Color(240, 240, 243, 255);
        c.nodeBorder = Color(200, 200, 206, 255);
        c.nodeGridMinor = Color(228, 228, 233, 255);
        c.nodeGridMajor = Color(210, 210, 216, 255);
        c.nodeBody = Color(250, 250, 252, 240);
        c.nodeBodySelected = Color(232, 240, 252, 240);
        c.nodeEdge = Color(186, 186, 194, 255);
        c.nodePinLabel = Color(60, 60, 66, 230);
        c.assetBg = Color(250, 250, 252, 255);
        c.assetBar = Color(238, 238, 242, 255);
        c.assetLine = Color(210, 210, 216, 255);
        c.assetPath = Color(80, 80, 88, 255);
        c.assetName = Color(60, 60, 66, 255);
        c.assetNameSelected = Color(0, 0, 0, 255);
        c.assetRowName = Color(50, 50, 56, 255);
        c.assetRowNameSelected = Color(0, 0, 0, 255);
        c.assetButtonOff = Color(222, 222, 228, 255);
        c.assetRowSelected = Color(190, 215, 245, 255);
        c.assetRowEven = Color(250, 250, 252, 255);
        c.assetRowOdd = Color(244, 244, 247, 255);
        c.assetScrollThumb = Color(150, 150, 158, 200);
        c.thumbBg = Color(244, 244, 247, 255);
        c.thumbEdge = Color(200, 200, 206, 255);
        c.thumbLabel = Color(60, 60, 66, 255);
        c.thumbLabelSelected = Color(150, 90, 0, 255);
        c.codeCurrentLine = Color(222, 232, 248, 255);
        c.minimapBg = Color(238, 238, 242, 240);
        c.minimapSlider = Color(60, 80, 120, 40);
        c.minimapSliderEdge = Color(60, 80, 120, 90);
        c.minimapLine = Color(0, 0, 0, 18);
        c.minimapEdge = Color(206, 206, 212, 255);
        c.tlBg = Color(244, 244, 247, 255);
        c.tlBorder = Color(200, 200, 206, 255);
        c.tlRulerBg = Color(228, 228, 233, 255);
        c.tlRulerLine = Color(200, 200, 206, 255);
        c.tlTickMajor = Color(120, 120, 128, 255);
        c.tlTickMinor = Color(170, 170, 178, 255);
        c.tlRulerText = Color(70, 70, 78, 230);
        c.tlRulerTextMinor = Color(90, 90, 98, 230);
        c.tlTrackName = Color(50, 50, 56, 255);
        c.tlRowEven = Color(246, 246, 249, 255);
        c.tlRowOdd = Color(238, 238, 242, 255);
        c.tlHeader = Color(232, 232, 236, 255);
        c.tlHeaderSelected = Color(200, 218, 244, 255);
        c.tlButtonOff = Color(206, 206, 212, 255);
        c.tlButtonText = Color(40, 40, 46, 255);
        c.tlTrackLine = Color(214, 214, 220, 255);
        c.tlHeaderLine = Color(200, 200, 206, 255);
        return c;
    }
};

/// What of a preset a WidgetApp::setTheme / setThemePreset takes: everything (the default, as always), only its colours and looks
/// (the application keeps the sizes its layouts were made with), or only its sizes.
enum class ThemeParts
{
    All,
    Colors,
    Metrics
};

struct Theme
{
    // Window / Panel
    Color bgColor = Color(30, 30, 30, 255);
    Color panelColor = Color(45, 45, 48, 255);
    Color borderColor = Color(70, 70, 70, 255);

    // Text
    Color textColor = Color(220, 220, 220, 255);
    Color textDisabled = Color(120, 120, 120, 255);

    // Button
    Color buttonNormal = Color(60, 60, 65, 255);
    Color buttonHover = Color(75, 75, 80, 255);
    Color buttonPressed = Color(45, 45, 50, 255);
    Color buttonBorder = Color(90, 90, 95, 255);

    // Focus / selection
    Color focusColor = Color(180, 180, 185, 255);
    Color selectionColor = Color(180, 180, 185, 128);

    // Slider
    Color sliderTrack = Color(55, 55, 58, 255);
    Color sliderTrackHover = Color(68, 68, 72, 255);
    Color sliderTrackFocused = Color(75, 75, 80, 255);
    Color sliderFill = Color(140, 140, 145, 255);
    Color sliderFillHover = Color(165, 165, 170, 255);
    Color sliderThumb = Color(160, 160, 165, 255);
    Color sliderThumbHover = Color(195, 195, 200, 255);
    Color sliderThumbPressed = Color(220, 220, 225, 255);

    // Input (checkbox box, text input bg)
    Color inputBg = Color(35, 35, 38, 255);
    Color inputBgHover = Color(48, 48, 52, 255);
    Color inputBorder = Color(80, 80, 85, 255);
    Color inputBorderHover = Color(110, 110, 118, 255);
    Color checkMark = Color(200, 200, 205, 255);

    // Switch: the thumb, and the track when it is on and when it is off (a Switch's own setOnColor / setOffColor win). In every preset but
    // dark the thumb has 3:1 or more against both tracks (the dark one's on track, 2.4:1, is how it always was).
    Color switchThumb = Color(220, 220, 225, 255);
    Color switchOn = Color(80, 160, 80, 255);
    Color switchOff = Color(70, 70, 75, 255);

    // Tooltip
    Color tooltipBg = Color(20, 20, 22, 230);
    Color tooltipBorder = Color(90, 90, 95, 200);
    Color tooltipText = Color(220, 220, 220, 255);

    // FloatWindow
    Color floatTitleBg = Color(50, 50, 55, 255);
    Color floatTitleText = Color(210, 210, 210, 255);
    Color floatBg = Color(38, 38, 40, 255);
    Color floatBorder = Color(75, 75, 80, 255);
    Color floatBtnHover = Color(70, 70, 75, 255);
    Color floatCloseHover = Color(200, 60, 60, 255);

    // SidePanel / Drawer
    Color drawerBg = Color(40, 40, 44, 255);
    Color drawerBorder = Color(70, 70, 75, 255);

    // Dialog
    Color dialogScrim = Color(0, 0, 0, 140);
    Color dialogBg = Color(48, 48, 52, 255);
    Color dialogBorder = Color(80, 80, 85, 255);
    Color dialogTitleText = Color(220, 220, 225, 255);
    Color dialogText = Color(190, 190, 195, 255);
    Color dialogBtnBg = Color(65, 65, 70, 255);
    Color dialogBtnHover = Color(80, 80, 85, 255);
    Color dialogBtnPrimary = Color(70, 130, 180, 255);
    Color dialogBtnPrimaryHover = Color(90, 150, 200, 255);
    Color dialogBtnDanger = Color(180, 60, 60, 255);
    Color dialogBtnDangerHover = Color(200, 80, 80, 255);

    // Editor gutter (line numbers)
    Color gutterBg = Color(38, 38, 42, 255);
    Color lineNumberColor = Color(120, 120, 120, 255);
    Color lineNumberActive = Color(220, 220, 220, 255);

    // Editor
    Color currentLineHighlight = Color(255, 255, 255, 8);
    Color scrollbarThumb = Color(120, 120, 120, 80);

    // Carousel
    Color carouselArrowBg = Color(0, 0, 0, 140);
    Color carouselArrowFg = Color(230, 230, 235, 240);
    Color carouselDotActive = Color(100, 160, 220, 255);
    Color carouselDotInactive = Color(100, 100, 105, 180);

    // Collapsible
    Color collapsibleHeaderBg = Color(50, 52, 58, 255);

    // Menu
    Color menuBarBg = Color(45, 45, 48, 255);
    Color menuBarItemHover = Color(65, 65, 70, 255);
    Color menuBg = Color(45, 45, 48, 255);
    Color menuBorder = Color(70, 70, 75, 255);
    Color menuItemHover = Color(65, 65, 70, 255);
    Color menuItemText = Color(220, 220, 220, 255);
    Color menuItemTextHover = Color(255, 255, 255, 255);
    Color menuItemDisabled = Color(120, 120, 120, 255);
    Color menuSeparator = Color(70, 70, 75, 255);
    Color menuShortcutText = Color(150, 150, 155, 255);
    Color menuShortcutHover = Color(200, 200, 200, 255);
    Color menuCheckMark = Color(100, 160, 220, 255);
    Color menuSubmenuArrow = Color(180, 180, 185, 255);

    // SideTabBar (the rail of icon buttons and the page beside it)
    Color sideTabRailBg = Color(28, 29, 33, 255);
    Color sideTabPageBg = Color(38, 40, 44, 255);
    Color sideTabHoverBg = Color(50, 52, 58, 255);
    Color sideTabActiveBg = Color(32, 48, 52, 255);
    Color sideTabAccent = Color(70, 200, 190, 255);
    Color sideTabText = Color(150, 152, 158, 255);
    Color sideTabTextActive = Color(220, 240, 238, 255);
    Color sideTabBadge = Color(255, 160, 40, 255);

    // Text on a selected row (TreeView, DataGrid, TreeGrid, PropertyGrid, FileDialog, ListBox): white over the dark
    // presets' selection, dark over the light ones' pale selection.
    Color selectionText = Color(255, 255, 255, 255);
    // The tint of the icons that a widget draws without a colour of its own (the icon of a tree node, the toggle icons of a
    // row): white, which the icon sheet is made of, in the dark presets; dark in the light ones.
    Color iconColor = Color(255, 255, 255, 255);
    // Text that is a step dimmer than textColor and still has to be read (the links of a breadcrumb, a description).
    Color textSecondary = Color(150, 150, 155, 255);
    // The text typed into an input box that has the focus (a cell being edited in a grid, a value in the property grid).
    Color inputText = Color(255, 255, 255, 255);

    // The panels of the widgets that paint their own (see the structs above).
    DockColors dock;
    ConsoleColors console;
    LayoutColors layout;
    AppColors app;
    FileColors file;
    PropColors prop;
    EditorColors editor;

    // Look: how some controls are drawn beyond their colours. The dark and
    // light presets leave these at their defaults, so they paint as they
    // always did; xp and macos switch them on.
    Color progressFill = Color(140, 140, 145, 255); // the bar of a ProgressBar
    Color progressTrack = Color(0, 0, 0, 0);        // alpha 0: sliderTrack
    bool progressBlocks = false;                    // a row of small blocks instead of a smooth bar
    float controlGradient = 0.0f;                   // button faces shade from lighter (top) to darker (bottom) by this much, 0..1; 0 is flat
    Color buttonHoverRing = Color(0, 0, 0, 0);      // alpha 0: none; else a ring inside the border of a hovered button
    Color dialogTitleBg = Color(0, 0, 0, 0);        // alpha 0: panelColor; the title bar of a Dialog
    float titleGradient = 0.0f;                     // title bars (Dialog, FloatWindow) shade from lighter to darker by this much, 0..1
    Color tabAccent = Color(60, 140, 255, 255);     // the line on top of the selected tab (zenforms Tabs)
    bool sliderThumbBox = false;                    // the Slider's thumb is a small box, not a circle
    // ToolBar: what used to be fixed in its paint. The defaults are those values, so a ToolBar looks as it did.
    Color toolbarChecked = Color(60, 100, 160, 255);     // the background of a checked toggle button
    Color toolbarCheckedDot = Color(100, 200, 255, 255); // the small dot in the corner of a checked toggle (alpha 0: none)
    Color toolbarIcon = Color(220, 220, 225, 255);       // the tint of the icons of IconId buttons (not of coloured icons)
    Color toolbarIconDisabled = Color(100, 100, 110, 255); // the same, for a disabled button

    /// @brief Whether the window background is light (a luminance above the middle): what widgets that have a palette of
    ///        their own for each (the syntax colours of a CodeEditor) look at to pick one.
    bool isLight() const
    {
        return bgColor.r * 299 + bgColor.g * 587 + bgColor.b * 114 > 128 * 1000;
    }

    /// @brief @p c moved toward @p target (0 black .. 255 white) by @p k (0..1).
    static Color shade(const Color& c, int target, float k)
    {
        return Color(static_cast<unsigned char>(c.r + (target - c.r) * k),
                static_cast<unsigned char>(c.g + (target - c.g) * k),
                static_cast<unsigned char>(c.b + (target - c.b) * k), c.a);
    }

    // Sizes
    float fontSize = 16.0f;
    float padding = 6.0f;
    float borderRadius = 4.0f;
    float buttonHeight = 28.0f;
    float sliderHeight = 20.0f;
    float sliderTrackWidth = 4.0f;
    float sliderThumbRadius = 6.5f; // the round thumb of a Slider; never more than half of the slider's thickness
    float inputHeight = 26.0f;
    /// Thickness of every scroll bar the library draws: ScrollView, ListBox, ListWidget, DataGrid, TreeGrid, ThumbnailGrid,
    /// Timeline, TreeView, ConsoleWidget and TextEdit. 8 in every preset (what the bars always were). Read it through
    /// ScrollBar::thickness(), which keeps it to what a bar can be (4 to 100; NaN gives 8). The bars take a new value at
    /// the next layout (WidgetApp::requestLayout() after changing it with widgets on screen).
    float scrollbarWidth = 8.0f;
    float scrollbarMinThumb = 16.0f;
    float textEditPadding = 4.0f;
    float gutterPadding = 8.0f;
    float lineSpacing = 2.0f;
    float tooltipPadX = 8.0f;
    float tooltipPadY = 4.0f;
    float menuBarHeight = 26.0f;
    float menuItemHeight = 24.0f;
    float menuItemPadX = 16.0f;
    float menuMinWidth = 160.0f;
    float menuShadowSize = 4.0f;

    // ── Theme presets ─────────────────────────────────────────────────────

    /// @brief Get the dark theme preset (default).
    static Theme dark()
    {
        return Theme{}; // defaults are the dark theme
    }

    /// @brief Get the light theme preset.
    static Theme light()
    {
        Theme t;
        // Window / Panel
        t.bgColor = Color(240, 240, 240, 255);
        t.panelColor = Color(250, 250, 250, 255);
        t.borderColor = Color(200, 200, 200, 255);
        // Text
        t.textColor = Color(30, 30, 30, 255);
        t.textDisabled = Color(140, 140, 140, 255);
        // Button
        t.buttonNormal = Color(225, 225, 228, 255);
        t.buttonHover = Color(210, 210, 215, 255);
        t.buttonPressed = Color(195, 195, 200, 255);
        t.buttonBorder = Color(180, 180, 185, 255);
        // Focus / selection
        t.focusColor = Color(0, 120, 215, 255);
        t.selectionColor = Color(0, 120, 215, 80);
        // Slider
        t.sliderTrack = Color(210, 210, 215, 255);
        t.sliderTrackHover = Color(195, 195, 200, 255);
        t.sliderTrackFocused = Color(180, 180, 185, 255);
        t.sliderFill = Color(0, 120, 215, 255);
        t.sliderFillHover = Color(30, 140, 225, 255);
        t.sliderThumb = Color(0, 120, 215, 255);
        t.sliderThumbHover = Color(30, 140, 225, 255);
        t.sliderThumbPressed = Color(0, 90, 180, 255);
        // Input
        t.inputBg = Color(255, 255, 255, 255);
        t.inputBgHover = Color(255, 255, 255, 255);
        t.inputBorder = Color(180, 180, 185, 255);
        t.inputBorderHover = Color(0, 120, 215, 255);
        t.checkMark = Color(0, 120, 215, 255);
        // Switch
        t.switchThumb = Color(255, 255, 255, 255);
        t.switchOn = Color(0, 120, 215, 255);
        t.switchOff = Color(130, 130, 138, 255);
        // Tooltip
        t.tooltipBg = Color(255, 255, 255, 240);
        t.tooltipBorder = Color(180, 180, 185, 255);
        t.tooltipText = Color(30, 30, 30, 255);
        // FloatWindow
        t.floatTitleBg = Color(230, 230, 235, 255);
        t.floatTitleText = Color(30, 30, 30, 255);
        t.floatBg = Color(245, 245, 248, 255);
        t.floatBorder = Color(190, 190, 195, 255);
        t.floatBtnHover = Color(210, 210, 215, 255);
        t.floatCloseHover = Color(220, 60, 60, 255);
        // Drawer
        t.drawerBg = Color(245, 245, 248, 255);
        t.drawerBorder = Color(200, 200, 205, 255);
        // Dialog
        t.dialogScrim = Color(0, 0, 0, 80);
        t.dialogBg = Color(250, 250, 252, 255);
        t.dialogBorder = Color(190, 190, 195, 255);
        t.dialogTitleText = Color(30, 30, 30, 255);
        t.dialogText = Color(60, 60, 60, 255);
        t.dialogBtnBg = Color(225, 225, 230, 255);
        t.dialogBtnHover = Color(210, 210, 215, 255);
        t.dialogBtnPrimary = Color(0, 120, 215, 255);
        t.dialogBtnPrimaryHover = Color(30, 140, 225, 255);
        t.dialogBtnDanger = Color(220, 60, 60, 255);
        t.dialogBtnDangerHover = Color(240, 80, 80, 255);
        // Editor gutter
        t.gutterBg = Color(235, 235, 238, 255);
        t.lineNumberColor = Color(140, 140, 145, 255);
        t.lineNumberActive = Color(30, 30, 30, 255);
        // Editor
        t.currentLineHighlight = Color(0, 0, 0, 12);
        t.scrollbarThumb = Color(140, 140, 145, 100);
        // Carousel
        t.carouselArrowBg = Color(255, 255, 255, 180);
        t.carouselArrowFg = Color(30, 30, 30, 240);
        t.carouselDotActive = Color(0, 120, 215, 255);
        t.carouselDotInactive = Color(180, 180, 185, 200);
        // Collapsible
        t.collapsibleHeaderBg = Color(230, 230, 235, 255);
        // Menu
        t.menuBarBg = Color(240, 240, 240, 255);
        t.menuBarItemHover = Color(210, 210, 215, 255);
        t.menuBg = Color(250, 250, 250, 255);
        t.menuBorder = Color(200, 200, 205, 255);
        t.menuItemHover = Color(0, 120, 215, 255);
        t.menuItemText = Color(30, 30, 30, 255);
        t.menuItemTextHover = Color(255, 255, 255, 255);
        t.menuItemDisabled = Color(160, 160, 165, 255);
        t.menuSeparator = Color(210, 210, 215, 255);
        t.menuShortcutText = Color(100, 100, 105, 255);
        t.menuShortcutHover = Color(220, 220, 220, 255);
        t.menuCheckMark = Color(0, 120, 215, 255);
        t.menuSubmenuArrow = Color(60, 60, 65, 255);
        // SideTabBar
        t.sideTabRailBg = Color(232, 232, 236, 255);
        t.sideTabPageBg = Color(248, 248, 250, 255);
        t.sideTabHoverBg = Color(218, 218, 224, 255);
        t.sideTabActiveBg = Color(210, 236, 234, 255);
        t.sideTabAccent = Color(0, 140, 130, 255);
        t.sideTabText = Color(90, 90, 95, 255);
        t.sideTabTextActive = Color(0, 100, 95, 255);
        t.sideTabBadge = Color(235, 130, 20, 255);
        // ToolBar
        t.toolbarChecked = Color(198, 220, 244, 255);
        t.toolbarCheckedDot = Color(0, 120, 215, 255);
        t.toolbarIcon = Color(60, 60, 65, 255);
        t.toolbarIconDisabled = Color(165, 165, 170, 255);
        // Selection, icons, secondary text, panels
        t.selectionText = Color(20, 20, 24, 255);
        t.iconColor = Color(52, 52, 58, 255);
        t.textSecondary = Color(90, 90, 96, 255);
        t.dock = DockColors::light();
        t.console = ConsoleColors::light();
        t.layout = LayoutColors::light();
        t.inputText = Color(20, 20, 24, 255);
        t.app = AppColors::light();
        t.file = FileColors::light();
        t.prop = PropColors::light();
        t.editor = EditorColors::light();
        return t;
    }

    /// @brief A light macOS look (Big Sur and later): light grey windows,
    ///        white controls with soft borders, the system blue as accent.
    static Theme macOS()
    {
        Theme t = light();
        const Color accent(0, 122, 255, 255);
        const Color accentHover(30, 140, 255, 255);
        // Window / Panel
        t.bgColor = Color(236, 236, 236, 255);
        t.panelColor = Color(246, 246, 246, 255);
        t.borderColor = Color(214, 214, 214, 255);
        // Text
        t.textColor = Color(34, 34, 34, 255);
        t.textDisabled = Color(170, 170, 170, 255);
        // Button
        t.buttonNormal = Color(255, 255, 255, 255);
        t.buttonHover = Color(250, 250, 250, 255);
        t.buttonPressed = Color(228, 228, 230, 255);
        t.buttonBorder = Color(208, 208, 208, 255);
        // Focus / selection
        t.focusColor = accent;
        t.selectionColor = Color(0, 122, 255, 70);
        // Slider
        t.sliderTrack = Color(206, 206, 210, 255);
        t.sliderTrackHover = Color(198, 198, 203, 255);
        t.sliderTrackFocused = Color(198, 198, 203, 255);
        t.sliderFill = accent;
        t.sliderFillHover = accentHover;
        t.sliderThumb = Color(255, 255, 255, 255);
        t.sliderThumbHover = Color(255, 255, 255, 255);
        t.sliderThumbPressed = Color(236, 236, 238, 255);
        // Input
        t.inputBg = Color(255, 255, 255, 255);
        t.inputBgHover = Color(255, 255, 255, 255);
        t.inputBorder = Color(208, 208, 208, 255);
        t.inputBorderHover = Color(170, 170, 176, 255);
        t.checkMark = accent;
        // Switch
        t.switchThumb = Color(255, 255, 255, 255);
        t.switchOn = accent;
        t.switchOff = Color(140, 140, 148, 255);
        // Tooltip
        t.tooltipBg = Color(246, 246, 246, 245);
        t.tooltipBorder = Color(200, 200, 200, 255);
        t.tooltipText = Color(34, 34, 34, 255);
        // FloatWindow
        t.floatTitleBg = Color(228, 228, 228, 255);
        t.floatTitleText = Color(34, 34, 34, 255);
        t.floatBg = Color(246, 246, 246, 255);
        t.floatBorder = Color(190, 190, 190, 255);
        t.floatBtnHover = Color(212, 212, 214, 255);
        t.floatCloseHover = Color(255, 95, 87, 255);
        // Drawer
        t.drawerBg = Color(240, 240, 240, 255);
        t.drawerBorder = Color(210, 210, 210, 255);
        // Dialog
        t.dialogScrim = Color(0, 0, 0, 70);
        t.dialogBg = Color(246, 246, 246, 255);
        t.dialogBorder = Color(196, 196, 196, 255);
        t.dialogTitleText = Color(34, 34, 34, 255);
        t.dialogText = Color(60, 60, 60, 255);
        t.dialogBtnBg = Color(255, 255, 255, 255);
        t.dialogBtnHover = Color(240, 240, 240, 255);
        t.dialogBtnPrimary = accent;
        t.dialogBtnPrimaryHover = accentHover;
        t.dialogBtnDanger = Color(255, 59, 48, 255);
        t.dialogBtnDangerHover = Color(255, 89, 78, 255);
        // Editor gutter
        t.gutterBg = Color(240, 240, 240, 255);
        t.lineNumberColor = Color(160, 160, 165, 255);
        t.lineNumberActive = Color(34, 34, 34, 255);
        // Editor
        t.currentLineHighlight = Color(0, 0, 0, 10);
        t.scrollbarThumb = Color(0, 0, 0, 90);
        // Carousel
        t.carouselDotActive = accent;
        t.carouselDotInactive = Color(190, 190, 195, 200);
        // Collapsible
        t.collapsibleHeaderBg = Color(228, 228, 230, 255);
        // Menu
        t.menuBarBg = Color(236, 236, 236, 255);
        t.menuBarItemHover = Color(208, 208, 212, 255);
        t.menuBg = Color(246, 246, 246, 250);
        t.menuBorder = Color(206, 206, 206, 255);
        t.menuItemHover = accent;
        t.menuItemText = Color(34, 34, 34, 255);
        t.menuItemTextHover = Color(255, 255, 255, 255);
        t.menuItemDisabled = Color(170, 170, 170, 255);
        t.menuSeparator = Color(220, 220, 220, 255);
        t.menuShortcutText = Color(105, 105, 110, 255); // (120,120,125 was 4.06 on the menu)
        t.menuShortcutHover = Color(225, 235, 255, 255);
        t.menuCheckMark = accent;
        t.menuSubmenuArrow = Color(90, 90, 95, 255);
        // SideTabBar
        t.sideTabRailBg = Color(228, 228, 230, 255);
        t.sideTabPageBg = Color(246, 246, 246, 255);
        t.sideTabHoverBg = Color(214, 214, 218, 255);
        t.sideTabActiveBg = Color(214, 228, 250, 255);
        t.sideTabAccent = accent;
        t.sideTabText = Color(100, 100, 105, 255);
        t.sideTabTextActive = Color(0, 90, 200, 255);
        t.sideTabBadge = Color(255, 59, 48, 255);
        // ToolBar
        t.toolbarChecked = Color(206, 224, 250, 255);
        t.toolbarCheckedDot = accent;
        t.toolbarIcon = Color(70, 70, 75, 255);
        // Panels in the system blue
        t.dock.accent = accent;
        t.dock.dragBorder = Color(0, 122, 255, 200);
        t.console.searchBorder = accent;
        t.layout.tabAccent = accent;
        // Look
        t.progressFill = accent;
        t.progressTrack = Color(214, 214, 218, 255);
        t.tabAccent = accent;
        // Sizes
        t.fontSize = 14.0f;
        t.borderRadius = 6.0f;
        t.buttonHeight = 28.0f;
        t.inputHeight = 28.0f;
        t.scrollbarWidth = 8.0f; // (it was 10 here, read by the TextEdit alone; now every bar of the library follows it)
        t.menuBarHeight = 24.0f;
        t.menuItemHeight = 24.0f;
        t.menuShadowSize = 8.0f;
        return t;
    }

    /// @brief The macOS look in its dark appearance.
    static Theme macOSDark()
    {
        Theme t = macOS();
        const Color accent(10, 132, 255, 255);
        const Color accentHover(50, 152, 255, 255);
        // Window / Panel
        t.bgColor = Color(40, 40, 42, 255);
        t.panelColor = Color(30, 30, 32, 255);
        t.borderColor = Color(66, 66, 70, 255);
        // Text
        t.textColor = Color(232, 232, 234, 255);
        t.textDisabled = Color(120, 120, 126, 255);
        // Button
        t.buttonNormal = Color(84, 84, 88, 255);
        t.buttonHover = Color(98, 98, 102, 255);
        t.buttonPressed = Color(70, 70, 74, 255);
        t.buttonBorder = Color(54, 54, 58, 255);
        // Focus / selection
        t.focusColor = accent;
        t.selectionColor = Color(10, 132, 255, 110);
        // Slider
        t.sliderTrack = Color(78, 78, 82, 255);
        t.sliderTrackHover = Color(88, 88, 92, 255);
        t.sliderTrackFocused = Color(88, 88, 92, 255);
        t.sliderFill = accent;
        t.sliderFillHover = accentHover;
        t.sliderThumb = Color(236, 236, 238, 255);
        t.sliderThumbHover = Color(246, 246, 248, 255);
        t.sliderThumbPressed = Color(210, 210, 214, 255);
        // Input
        t.inputBg = Color(30, 30, 32, 255);
        t.inputBgHover = Color(36, 36, 38, 255);
        t.inputBorder = Color(76, 76, 80, 255);
        t.inputBorderHover = Color(110, 110, 116, 255);
        t.checkMark = accent;
        // Switch
        t.switchThumb = Color(240, 240, 242, 255);
        t.switchOn = accent;
        t.switchOff = Color(110, 110, 116, 255);
        // Tooltip
        t.tooltipBg = Color(54, 54, 56, 245);
        t.tooltipBorder = Color(80, 80, 84, 255);
        t.tooltipText = Color(232, 232, 234, 255);
        // FloatWindow
        t.floatTitleBg = Color(58, 58, 60, 255);
        t.floatTitleText = Color(232, 232, 234, 255);
        t.floatBg = Color(40, 40, 42, 255);
        t.floatBorder = Color(76, 76, 80, 255);
        t.floatBtnHover = Color(84, 84, 88, 255);
        // Drawer
        t.drawerBg = Color(36, 36, 38, 255);
        t.drawerBorder = Color(66, 66, 70, 255);
        // Dialog
        t.dialogScrim = Color(0, 0, 0, 120);
        t.dialogBg = Color(44, 44, 46, 255);
        t.dialogBorder = Color(80, 80, 84, 255);
        t.dialogTitleText = Color(240, 240, 242, 255);
        t.dialogText = Color(204, 204, 208, 255);
        t.dialogBtnBg = Color(84, 84, 88, 255);
        t.dialogBtnHover = Color(98, 98, 102, 255);
        t.dialogBtnPrimary = accent;
        t.dialogBtnPrimaryHover = accentHover;
        t.dialogBtnDanger = Color(255, 69, 58, 255);
        t.dialogBtnDangerHover = Color(255, 99, 88, 255);
        // Editor gutter
        t.gutterBg = Color(34, 34, 36, 255);
        t.lineNumberColor = Color(120, 120, 126, 255);
        t.lineNumberActive = Color(232, 232, 234, 255);
        // Editor
        t.currentLineHighlight = Color(255, 255, 255, 10);
        t.scrollbarThumb = Color(255, 255, 255, 90);
        // Carousel
        t.carouselArrowBg = Color(0, 0, 0, 140);
        t.carouselArrowFg = Color(236, 236, 238, 240);
        t.carouselDotActive = accent;
        t.carouselDotInactive = Color(100, 100, 106, 180);
        // Collapsible
        t.collapsibleHeaderBg = Color(54, 54, 56, 255);
        // Menu
        t.menuBarBg = Color(40, 40, 42, 255);
        t.menuBarItemHover = Color(70, 70, 74, 255);
        t.menuBg = Color(46, 46, 48, 250);
        t.menuBorder = Color(76, 76, 80, 255);
        t.menuItemHover = accent;
        t.menuItemText = Color(232, 232, 234, 255);
        t.menuItemDisabled = Color(120, 120, 126, 255);
        t.menuSeparator = Color(72, 72, 76, 255);
        t.menuShortcutText = Color(150, 150, 156, 255);
        t.menuShortcutHover = Color(225, 235, 255, 255);
        t.menuCheckMark = accent;
        t.menuSubmenuArrow = Color(180, 180, 186, 255);
        // SideTabBar
        t.sideTabRailBg = Color(34, 34, 36, 255);
        t.sideTabPageBg = Color(44, 44, 46, 255);
        t.sideTabHoverBg = Color(58, 58, 60, 255);
        t.sideTabActiveBg = Color(28, 52, 86, 255);
        t.sideTabAccent = accent;
        t.sideTabText = Color(160, 160, 166, 255);
        t.sideTabTextActive = Color(214, 230, 255, 255);
        t.sideTabBadge = Color(255, 69, 58, 255);
        // ToolBar
        t.toolbarChecked = Color(28, 52, 86, 255);
        t.toolbarCheckedDot = accent;
        t.toolbarIcon = Color(214, 214, 220, 255);
        t.toolbarIconDisabled = Color(110, 110, 116, 255);
        // Selection, icons, secondary text, panels: the dark ones, in the system blue, with the dim texts of the console
        // a step brighter than the dark preset's (they are read here)
        t.selectionText = Color(255, 255, 255, 255);
        t.iconColor = Color(255, 255, 255, 255);
        t.textSecondary = Color(150, 150, 155, 255);
        t.dock = DockColors();
        t.dock.accent = accent;
        t.dock.dragBorder = Color(10, 132, 255, 200);
        t.console = ConsoleColors();
        t.console.searchBorder = accent;
        t.console.error = Color(255, 120, 110, 255);
        t.console.trace = Color(165, 165, 175, 255);
        t.console.badgeText = Color(130, 170, 230, 255);
        t.console.buttonOffText = Color(132, 132, 142, 255);
        t.console.searchHint = Color(132, 132, 142, 255);
        t.console.timestamp = Color(132, 132, 142, 255);
        t.layout = LayoutColors();
        t.layout.tabAccent = accent;
        t.layout.tabTextInactive = Color(150, 150, 156, 255);
        t.inputText = Color(255, 255, 255, 255);
        t.app = AppColors();
        t.file = FileColors();
        t.prop = PropColors();
        t.editor = EditorColors();
        t.editor.tlRulerText = Color(190, 192, 200, 230);
        t.editor.tlRulerTextMinor = Color(150, 152, 160, 230);
        // Look
        t.progressFill = accent;
        t.progressTrack = Color(78, 78, 82, 255);
        t.tabAccent = accent;
        return t;
    }

    /// @brief A Windows XP (Luna, blue) look: beige windows, a blue title
    ///        bar and selection, white inputs with a blue-grey border, a
    ///        green check mark.
    static Theme windowsXP()
    {
        Theme t = light();
        const Color blue(49, 106, 197, 255);
        const Color blueLight(80, 130, 220, 255);
        const Color face(236, 233, 216, 255);
        const Color edge(172, 168, 153, 255);
        // Window / Panel
        t.bgColor = face;
        t.panelColor = face;
        t.borderColor = edge;
        // Text
        t.textColor = Color(0, 0, 0, 255);
        t.textDisabled = Color(161, 161, 146, 255);
        // Button
        t.buttonNormal = Color(236, 235, 230, 255);
        t.buttonHover = Color(244, 243, 238, 255);
        t.buttonPressed = Color(214, 210, 194, 255);
        t.buttonBorder = Color(0, 60, 116, 255);
        // Focus / selection
        t.focusColor = blue;
        t.selectionColor = Color(49, 106, 197, 110);
        // Slider
        t.sliderTrack = Color(200, 197, 182, 255);
        t.sliderTrackHover = Color(190, 187, 172, 255);
        t.sliderTrackFocused = Color(190, 187, 172, 255);
        t.sliderFill = Color(200, 197, 182, 255);
        t.sliderFillHover = Color(190, 187, 172, 255);
        t.sliderThumb = Color(236, 235, 230, 255);
        t.sliderThumbHover = Color(255, 240, 207, 255);
        t.sliderThumbPressed = Color(214, 210, 194, 255);
        // Input
        t.inputBg = Color(255, 255, 255, 255);
        t.inputBgHover = Color(255, 255, 255, 255);
        t.inputBorder = Color(127, 157, 185, 255);
        t.inputBorderHover = Color(49, 106, 197, 255);
        t.checkMark = Color(33, 161, 33, 255);
        // Switch
        t.switchThumb = Color(255, 255, 255, 255);
        t.switchOn = blue;
        t.switchOff = Color(128, 124, 110, 255);
        // Tooltip
        t.tooltipBg = Color(255, 255, 225, 255);
        t.tooltipBorder = Color(0, 0, 0, 255);
        t.tooltipText = Color(0, 0, 0, 255);
        // FloatWindow
        t.floatTitleBg = Color(0, 84, 227, 255);
        t.floatTitleText = Color(255, 255, 255, 255);
        t.floatBg = face;
        t.floatBorder = Color(0, 60, 215, 255);
        t.floatBtnHover = blueLight;
        t.floatCloseHover = Color(224, 80, 50, 255);
        // Drawer
        t.drawerBg = face;
        t.drawerBorder = edge;
        // Dialog
        t.dialogScrim = Color(0, 0, 0, 100);
        t.dialogBg = face;
        t.dialogBorder = Color(0, 60, 215, 255);
        t.dialogTitleText = Color(255, 255, 255, 255);
        t.dialogText = Color(0, 0, 0, 255);
        t.dialogBtnBg = Color(236, 235, 230, 255);
        t.dialogBtnHover = Color(244, 243, 238, 255);
        t.dialogBtnPrimary = blue;
        t.dialogBtnPrimaryHover = blueLight;
        t.dialogBtnDanger = Color(200, 60, 40, 255);
        t.dialogBtnDangerHover = Color(224, 80, 60, 255);
        // Editor gutter
        t.gutterBg = face;
        t.lineNumberColor = Color(130, 128, 118, 255);
        t.lineNumberActive = Color(0, 0, 0, 255);
        // Editor
        t.currentLineHighlight = Color(49, 106, 197, 26);
        t.scrollbarThumb = Color(130, 160, 225, 210);
        // Carousel
        t.carouselDotActive = blue;
        t.carouselDotInactive = edge;
        // Collapsible
        t.collapsibleHeaderBg = Color(198, 211, 247, 255);
        // Menu
        t.menuBarBg = face;
        t.menuBarItemHover = blue;
        t.menuBg = Color(255, 255, 255, 255);
        t.menuBorder = Color(151, 151, 151, 255);
        t.menuItemHover = blue;
        t.menuItemText = Color(0, 0, 0, 255);
        t.menuItemTextHover = Color(255, 255, 255, 255);
        t.menuItemDisabled = Color(161, 161, 146, 255);
        t.menuSeparator = edge;
        t.menuShortcutText = Color(90, 90, 80, 255);
        t.menuShortcutHover = Color(255, 255, 255, 255);
        t.menuCheckMark = Color(0, 0, 0, 255);
        t.menuSubmenuArrow = Color(0, 0, 0, 255);
        // SideTabBar
        t.sideTabRailBg = Color(214, 223, 247, 255);
        t.sideTabPageBg = face;
        t.sideTabHoverBg = Color(198, 211, 247, 255);
        t.sideTabActiveBg = Color(255, 255, 255, 255);
        t.sideTabAccent = blue;
        t.sideTabText = Color(33, 93, 198, 255);
        t.sideTabTextActive = Color(0, 40, 130, 255);
        t.sideTabBadge = Color(224, 80, 50, 255);
        // ToolBar
        t.toolbarChecked = Color(193, 210, 238, 255);
        t.toolbarCheckedDot = blue;
        t.toolbarIcon = Color(30, 30, 30, 255);
        t.toolbarIconDisabled = Color(161, 161, 146, 255);
        // Selection, icons, secondary text, panels
        t.selectionText = Color(0, 0, 0, 255);
        t.iconColor = Color(30, 30, 30, 255);
        t.textSecondary = Color(90, 90, 80, 255);
        t.inputText = Color(0, 0, 0, 255);
        t.dock.accent = blue;
        t.dock.dragBorder = Color(49, 106, 197, 200);
        t.dock.barBg = Color(214, 223, 247, 255);
        t.dock.tabBg = Color(198, 211, 247, 255);
        t.dock.tabActiveBg = Color(255, 255, 255, 255);
        t.console.searchBorder = blue;
        t.layout.tabAccent = blue;
        t.layout.tabBarBg = Color(214, 223, 247, 255);
        t.layout.tabBg = Color(198, 211, 247, 255);
        // Look
        t.progressFill = Color(60, 205, 60, 255);
        t.progressTrack = Color(255, 255, 255, 255);
        t.progressBlocks = true;
        t.controlGradient = 0.8f;
        t.buttonHoverRing = Color(250, 195, 90, 255);
        t.dialogTitleBg = Color(0, 84, 227, 255);
        t.titleGradient = 0.35f;
        t.tabAccent = Color(250, 195, 90, 255);
        t.sliderThumbBox = true;
        // Sizes
        t.fontSize = 14.0f;
        t.borderRadius = 3.0f;
        t.buttonHeight = 24.0f;
        t.inputHeight = 24.0f;
        t.scrollbarWidth = 8.0f; // (it was 16 here, read by the TextEdit alone)
        t.menuBarHeight = 22.0f;
        t.menuItemHeight = 22.0f;
        t.menuShadowSize = 2.0f;
        return t;
    }

    /// @brief The names byName() accepts, in the order a picker shows them.
    static constexpr int kPresetCount = 5;
    static const char* presetName(int index)
    {
        static const char* const names[kPresetCount] = {"dark", "light", "xp", "macos", "macos-dark"};
        return index >= 0 && index < kPresetCount ? names[index] : "";
    }

    /// @brief A preset by name (see presetName); an unknown name gives the dark one.
    static Theme byName(const char* name)
    {
        const auto same = [name](const char* other) {
            const char* a = name ? name : "";
            while (*a && *a == *other)
            {
                ++a;
                ++other;
            }
            return *a == *other;
        };
        if (same("light")) return light();
        if (same("xp")) return windowsXP();
        if (same("macos")) return macOS();
        if (same("macos-dark")) return macOSDark();
        return dark();
    }

    /// @brief Apply a theme preset to this instance.
    void apply(const Theme& preset)
    {
        *this = preset;
    }

    /// @brief The sizes of the theme: font size, paddings, heights, bar thickness, menu metrics. What the layout of the widgets depends
    ///        on, as opposed to the colours and the looks (gradients, blocks, boxes) that only change what is drawn.
    void copyMetricsFrom(const Theme& o)
    {
        fontSize = o.fontSize;
        padding = o.padding;
        borderRadius = o.borderRadius;
        buttonHeight = o.buttonHeight;
        sliderHeight = o.sliderHeight;
        sliderTrackWidth = o.sliderTrackWidth;
        sliderThumbRadius = o.sliderThumbRadius;
        inputHeight = o.inputHeight;
        scrollbarWidth = o.scrollbarWidth;
        scrollbarMinThumb = o.scrollbarMinThumb;
        textEditPadding = o.textEditPadding;
        gutterPadding = o.gutterPadding;
        lineSpacing = o.lineSpacing;
        tooltipPadX = o.tooltipPadX;
        tooltipPadY = o.tooltipPadY;
        menuBarHeight = o.menuBarHeight;
        menuItemHeight = o.menuItemHeight;
        menuItemPadX = o.menuItemPadX;
        menuMinWidth = o.menuMinWidth;
        menuShadowSize = o.menuShadowSize;
    }
    /// @brief Whether the metrics (see copyMetricsFrom) of two themes are the same.
    bool sameMetricsAs(const Theme& o) const
    {
        return fontSize == o.fontSize && padding == o.padding && borderRadius == o.borderRadius && buttonHeight == o.buttonHeight &&
               sliderHeight == o.sliderHeight && sliderTrackWidth == o.sliderTrackWidth && sliderThumbRadius == o.sliderThumbRadius &&
               inputHeight == o.inputHeight && scrollbarWidth == o.scrollbarWidth && scrollbarMinThumb == o.scrollbarMinThumb &&
               textEditPadding == o.textEditPadding && gutterPadding == o.gutterPadding && lineSpacing == o.lineSpacing &&
               tooltipPadX == o.tooltipPadX && tooltipPadY == o.tooltipPadY && menuBarHeight == o.menuBarHeight &&
               menuItemHeight == o.menuItemHeight && menuItemPadX == o.menuItemPadX && menuMinWidth == o.menuMinWidth &&
               menuShadowSize == o.menuShadowSize;
    }
    /// @brief Take the colours (and the looks: gradients, blocks, boxes) of @p preset and keep this theme's own metrics: a macOS
    ///        or XP palette on the sizes the application has laid its windows out with. Nothing that a layout reads changes.
    void applyColors(const Theme& preset)
    {
        Theme own;
        own.copyMetricsFrom(*this);
        *this = preset;
        copyMetricsFrom(own);
    }
    /// @brief Take the metrics of @p preset and keep this theme's colours.
    void applyMetrics(const Theme& preset)
    {
        copyMetricsFrom(preset);
    }

    /// @brief Get the global Theme singleton.
    static Theme& instance()
    {
        static Theme theme;
        return theme;
    }
};

} // namespace retained
} // namespace ig
