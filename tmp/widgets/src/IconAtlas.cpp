#include "pch.hpp"

using BuImage = BuGUI::BuImage;

// ─────────────────────────────────────────────────────────────────────────────
//  IconAtlas
// ─────────────────────────────────────────────────────────────────────────────

IconAtlas::~IconAtlas() {}

BuGUI::BuImage* IconAtlas::buildImage(int cellSize)
{
    cellSize_ = cellSize;
    cols_ = 8;
    int iconCount = static_cast<int>(IconId::COUNT) - 1;
    int rows = (iconCount + cols_ - 1) / cols_;
    auto* img = new BuImage(cols_ * cellSize, rows * cellSize, 4);
    img->Fill(0, 0, 0, 0);
    drawAllIcons(*img);
    return img;
}

FloatRect IconAtlas::srcRect(IconId id) const
{
    int idx = static_cast<int>(id) - 1;
    if (idx < 0 || !tex_) return {};
    int col = idx % cols_;
    int row = idx / cols_;
    return FloatRect(
        static_cast<float>(col * cellSize_),
        static_cast<float>(row * cellSize_),
        static_cast<float>(cellSize_),
        static_cast<float>(cellSize_));
}

// ─────────────────────────────────────────────────────────────────────────────
//  drawAllIcons — iterate every IconId and dispatch
// ─────────────────────────────────────────────────────────────────────────────

void IconAtlas::drawAllIcons(BuImage& pm)
{
    int count = static_cast<int>(IconId::COUNT) - 1;
    for (int i = 0; i < count; ++i) {
        int col = i % cols_;
        int row = i / cols_;
        int ox = col * cellSize_;
        int oy = row * cellSize_;
        auto id = static_cast<IconId>(i + 1);
        drawIcon(pm, id, ox, oy, cellSize_);
    }
}

void IconAtlas::drawIcon(BuImage& pm, IconId id, int ox, int oy, int sz)
{
    switch (id) {
    case IconId::Folder:      drawFolder(pm, ox, oy, sz);      break;
    case IconId::FolderOpen:  drawFolderOpen(pm, ox, oy, sz);  break;
    case IconId::File:        drawFile(pm, ox, oy, sz);        break;
    case IconId::FileCode:    drawFileCode(pm, ox, oy, sz);    break;
    case IconId::Book:        drawBook(pm, ox, oy, sz);        break;
    case IconId::Gear:        drawGear(pm, ox, oy, sz);        break;
    case IconId::Star:        drawStar(pm, ox, oy, sz);        break;
    case IconId::Heart:       drawHeart(pm, ox, oy, sz);       break;
    case IconId::Search:      drawSearch(pm, ox, oy, sz);      break;
    case IconId::Plus:        drawPlus(pm, ox, oy, sz);        break;
    case IconId::Minus:       drawMinus(pm, ox, oy, sz);       break;
    case IconId::Check:       drawCheck(pm, ox, oy, sz);       break;
    case IconId::Cross:       drawCross(pm, ox, oy, sz);       break;
    case IconId::ArrowRight:  drawArrowRight(pm, ox, oy, sz);  break;
    case IconId::ArrowDown:   drawArrowDown(pm, ox, oy, sz);   break;
    case IconId::ArrowUp:     drawArrowUp(pm, ox, oy, sz);     break;
    case IconId::ArrowLeft:   drawArrowLeft(pm, ox, oy, sz);   break;
    case IconId::Eye:         drawEye(pm, ox, oy, sz);         break;
    case IconId::EyeOff:      drawEyeOff(pm, ox, oy, sz);     break;
    case IconId::Lock:        drawLock(pm, ox, oy, sz);        break;
    case IconId::Unlock:      drawUnlock(pm, ox, oy, sz);      break;
    case IconId::Refresh:     drawRefresh(pm, ox, oy, sz);     break;
    case IconId::Trash:       drawTrash(pm, ox, oy, sz);       break;
    case IconId::Edit:        drawEdit(pm, ox, oy, sz);        break;
    case IconId::Home:        drawHome(pm, ox, oy, sz);        break;
    case IconId::User:        drawUser(pm, ox, oy, sz);        break;
    case IconId::Warning:     drawWarning(pm, ox, oy, sz);     break;
    case IconId::Info:        drawInfo(pm, ox, oy, sz);        break;
    case IconId::Error:       drawError(pm, ox, oy, sz);       break;
    case IconId::FileImage:   drawFileImage(pm, ox, oy, sz);   break;
    case IconId::FileArchive: drawFileArchive(pm, ox, oy, sz); break;
    case IconId::ViewDetail:  drawViewDetail(pm, ox, oy, sz);  break;
    case IconId::ViewList:    drawViewList(pm, ox, oy, sz);    break;
    case IconId::ViewGrid:    drawViewGrid(pm, ox, oy, sz);    break;
    case IconId::Play:        drawPlay(pm, ox, oy, sz);        break;
    case IconId::Pause:       drawPause(pm, ox, oy, sz);       break;
    case IconId::Stop:        drawStop(pm, ox, oy, sz);        break;
    case IconId::StepForward: drawStepForward(pm, ox, oy, sz); break;
    case IconId::StepBack:    drawStepBack(pm, ox, oy, sz);    break;
    case IconId::Record:      drawRecord(pm, ox, oy, sz);      break;
    default: break;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Individual icon draw routines
//  All coords relative to cell origin (ox, oy) with cell size sz.
//  Using BuImage::DrawLine, DrawRect(fill), DrawCircle(fill).
//  Icon color: light grey (200,200,210,255) for visibility on dark backgrounds.
// ═════════════════════════════════════════════════════════════════════════════

static const Color IC(200, 200, 210, 255);   // icon color
static const Color IC2(160, 180, 220, 255);  // accent (folders, links)
static const Color IC3(180, 210, 140, 255);  // secondary accent (code)

// ── Folder ───────────────────────────────────────────────────────────────────

void IconAtlas::drawFolder(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 6;  // margin
    int x = ox + m, y = oy + m + sz/6;
    int w = sz - 2*m, h = sz - 2*m - sz/6;
    // Tab
    pm.DrawRect(x, y - h/5, w/3, h/5 + 1, IC2, true);
    // Body
    pm.DrawRect(x, y, w, h, IC2, true);
}

void IconAtlas::drawFolderOpen(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 6;
    int x = ox + m, y = oy + m + sz/6;
    int w = sz - 2*m, h = sz - 2*m - sz/6;
    // Tab
    pm.DrawRect(x, y - h/5, w/3, h/5 + 1, IC2, true);
    // Body (slightly shifted)
    pm.DrawRect(x, y, w, h, IC2, false);
    // Open front
    pm.DrawRect(x + 2, y + h/3, w - 2, h - h/3, IC2, true);
}

// ── File ─────────────────────────────────────────────────────────────────────

void IconAtlas::drawFile(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 5;
    int x = ox + m, y = oy + m/2;
    int w = sz - 2*m, h = sz - m;
    // Dark fill so the icon is visible on dark backgrounds
    static const Color ICBg(50, 55, 65, 255);
    pm.DrawRect(x, y, w, h, ICBg, true);
    pm.DrawRect(x, y, w, h, IC, false);
    // Dog-ear
    int ear = w / 3;
    // Clear dog-ear triangle area, then draw fold lines
    pm.DrawLine(x + w - ear, y, x + w, y + ear, IC);
    pm.DrawLine(x + w - ear, y, x + w - ear, y + ear, IC);
    pm.DrawLine(x + w - ear, y + ear, x + w, y + ear, IC);
}

void IconAtlas::drawFileCode(BuImage& pm, int ox, int oy, int sz)
{
    drawFile(pm, ox, oy, sz);
    // < > brackets
    int cx = ox + sz/2, cy = oy + sz/2 + sz/8;
    int d = sz/7;
    pm.DrawLine(cx - d, cy, cx - d*2, cy - d, IC3);
    pm.DrawLine(cx - d, cy, cx - d*2, cy + d, IC3);
    pm.DrawLine(cx + d, cy, cx + d*2, cy - d, IC3);
    pm.DrawLine(cx + d, cy, cx + d*2, cy + d, IC3);
}

void IconAtlas::drawFileImage(BuImage& pm, int ox, int oy, int sz)
{
    drawFile(pm, ox, oy, sz);
    // Small mountain/sun
    int cx = ox + sz/2, cy = oy + sz*2/3;
    int r = sz/8;
    pm.DrawCircle(cx - r, cy - r, r/2, IC2, true);
    pm.DrawLine(cx - r*2, cy + r, cx, cy - r/2, IC2);
    pm.DrawLine(cx, cy - r/2, cx + r*2, cy + r, IC2);
}

void IconAtlas::drawFileArchive(BuImage& pm, int ox, int oy, int sz)
{
    drawFile(pm, ox, oy, sz);
    // Zip pattern (horizontal stripes)
    int m = sz / 5;
    int x = ox + m + 1;
    for (int i = 0; i < 3; ++i) {
        int y = oy + sz/3 + i * (sz/7);
        pm.DrawRect(x, y, sz/5, 1, IC2, true);
    }
}

// ── Book ─────────────────────────────────────────────────────────────────────

void IconAtlas::drawBook(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 5;
    int x = ox + m, y = oy + m;
    int w = sz - 2*m, h = sz - 2*m;
    pm.DrawRect(x, y, w, h, IC, false);
    // Spine
    pm.DrawLine(x + w/4, y, x + w/4, y + h, IC);
    // Lines on cover
    pm.DrawLine(x + w/4 + 3, y + h/4, x + w - 2, y + h/4, Color(150,150,160,200));
    pm.DrawLine(x + w/4 + 3, y + h/2, x + w - 2, y + h/2, Color(150,150,160,200));
}

// ── Gear ─────────────────────────────────────────────────────────────────────

void IconAtlas::drawGear(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int r1 = sz/4, r2 = sz/3;
    pm.DrawCircle(cx, cy, r1, IC, false);
    // Teeth (8 lines radiating out)
    for (int i = 0; i < 8; ++i) {
        float a = i * 3.14159f / 4.0f;
        int x1 = cx + static_cast<int>(r1 * std::cos(a));
        int y1 = cy + static_cast<int>(r1 * std::sin(a));
        int x2 = cx + static_cast<int>(r2 * std::cos(a));
        int y2 = cy + static_cast<int>(r2 * std::sin(a));
        pm.DrawLine(x1, y1, x2, y2, IC);
    }
}

// ── Star ─────────────────────────────────────────────────────────────────────

void IconAtlas::drawStar(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int r = sz * 2 / 5;
    // 5-point star using lines
    for (int i = 0; i < 5; ++i) {
        float a1 = (i * 2) * 3.14159f * 2.0f / 5.0f - 3.14159f/2.0f;
        float a2 = ((i * 2 + 2) % 5) * 3.14159f * 2.0f / 5.0f - 3.14159f/2.0f;
        pm.DrawLine(cx + static_cast<int>(r * std::cos(a1)),
                    cy + static_cast<int>(r * std::sin(a1)),
                    cx + static_cast<int>(r * std::cos(a2)),
                    cy + static_cast<int>(r * std::sin(a2)), Color(240, 200, 80, 255));
    }
}

// ── Heart ────────────────────────────────────────────────────────────────────

void IconAtlas::drawHeart(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int r = sz / 6;
    pm.DrawCircle(cx - r, cy - r/2, r, Color(220, 80, 80, 255), true);
    pm.DrawCircle(cx + r, cy - r/2, r, Color(220, 80, 80, 255), true);
    // Bottom triangle approx
    for (int dy = 0; dy < r*2; ++dy) {
        int hw = r*2 - dy;
        pm.DrawLine(cx - hw, cy + dy, cx + hw, cy + dy, Color(220, 80, 80, 255));
    }
}

// ── Search ───────────────────────────────────────────────────────────────────

void IconAtlas::drawSearch(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz*2/5, cy = oy + sz*2/5;
    int r = sz / 4;
    pm.DrawCircle(cx, cy, r, IC, false);
    // Handle
    int hx = cx + static_cast<int>(r * 0.7f);
    int hy = cy + static_cast<int>(r * 0.7f);
    pm.DrawLine(hx, hy, hx + sz/5, hy + sz/5, IC);
}

// ── Plus ─────────────────────────────────────────────────────────────────────

void IconAtlas::drawPlus(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int d = sz / 4;
    pm.DrawLine(cx, cy - d, cx, cy + d, IC);
    pm.DrawLine(cx - d, cy, cx + d, cy, IC);
}

// ── Minus ────────────────────────────────────────────────────────────────────

void IconAtlas::drawMinus(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int d = sz / 4;
    pm.DrawLine(cx - d, cy, cx + d, cy, IC);
}

// ── Check ────────────────────────────────────────────────────────────────────

void IconAtlas::drawCheck(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 4;
    pm.DrawLine(ox + m, oy + sz/2, ox + sz*2/5, oy + sz - m, Color(100, 220, 100, 255));
    pm.DrawLine(ox + sz*2/5, oy + sz - m, ox + sz - m, oy + m, Color(100, 220, 100, 255));
}

// ── Cross ────────────────────────────────────────────────────────────────────

void IconAtlas::drawCross(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 4;
    pm.DrawLine(ox + m, oy + m, ox + sz - m, oy + sz - m, Color(220, 80, 80, 255));
    pm.DrawLine(ox + sz - m, oy + m, ox + m, oy + sz - m, Color(220, 80, 80, 255));
}

// ── Arrows ───────────────────────────────────────────────────────────────────

void IconAtlas::drawArrowRight(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int d = sz / 4;
    pm.DrawLine(cx - d, cy, cx + d, cy, IC);
    pm.DrawLine(cx + d, cy, cx, cy - d, IC);
    pm.DrawLine(cx + d, cy, cx, cy + d, IC);
}

void IconAtlas::drawArrowLeft(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int d = sz / 4;
    pm.DrawLine(cx + d, cy, cx - d, cy, IC);
    pm.DrawLine(cx - d, cy, cx, cy - d, IC);
    pm.DrawLine(cx - d, cy, cx, cy + d, IC);
}

void IconAtlas::drawArrowUp(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int d = sz / 4;
    pm.DrawLine(cx, cy + d, cx, cy - d, IC);
    pm.DrawLine(cx, cy - d, cx - d, cy, IC);
    pm.DrawLine(cx, cy - d, cx + d, cy, IC);
}

void IconAtlas::drawArrowDown(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int d = sz / 4;
    pm.DrawLine(cx, cy - d, cx, cy + d, IC);
    pm.DrawLine(cx, cy + d, cx - d, cy, IC);
    pm.DrawLine(cx, cy + d, cx + d, cy, IC);
}

// ── Eye / EyeOff ────────────────────────────────────────────────────────────

void IconAtlas::drawEye(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int rx = sz * 2 / 5, ry = sz / 5;
    // Eye shape (two arcs approximated by lines)
    for (int i = -rx; i <= rx; ++i) {
        float t = static_cast<float>(i) / rx;
        int dy = static_cast<int>(ry * (1.0f - t * t));
        pm.SetPixel(cx + i, cy - dy, IC.r, IC.g, IC.b, IC.a);
        pm.SetPixel(cx + i, cy + dy, IC.r, IC.g, IC.b, IC.a);
    }
    // Pupil
    pm.DrawCircle(cx, cy, sz/8, IC, true);
}

void IconAtlas::drawEyeOff(BuImage& pm, int ox, int oy, int sz)
{
    drawEye(pm, ox, oy, sz);
    // Diagonal strike
    int m = sz / 5;
    pm.DrawLine(ox + m, oy + sz - m, ox + sz - m, oy + m, Color(220, 80, 80, 255));
}

// ── Lock / Unlock ────────────────────────────────────────────────────────────

void IconAtlas::drawLock(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 5;
    int bx = ox + m, by = oy + sz/2;
    int bw = sz - 2*m, bh = sz/2 - m;
    pm.DrawRect(bx, by, bw, bh, IC, true);
    // Shackle
    int sx = ox + sz/2, sr = bw/3;
    pm.DrawCircle(sx, by, sr, IC, false);
    pm.DrawRect(sx - sr, by - sr/2, sr*2, sr/2 + 1, Color(0,0,0,0), true); // clear bottom of circle
}

void IconAtlas::drawUnlock(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 5;
    int bx = ox + m, by = oy + sz/2;
    int bw = sz - 2*m, bh = sz/2 - m;
    pm.DrawRect(bx, by, bw, bh, IC, true);
    // Shackle shifted right
    int sx = ox + sz/2 + sz/8, sr = bw/3;
    pm.DrawCircle(sx, by, sr, IC, false);
}

// ── Refresh ──────────────────────────────────────────────────────────────────

void IconAtlas::drawRefresh(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int r = sz / 3;
    pm.DrawCircle(cx, cy, r, IC, false);
    // Arrow head at top-right
    int ax = cx + r, ay = cy;
    pm.DrawLine(ax, ay, ax - sz/8, ay - sz/8, IC);
    pm.DrawLine(ax, ay, ax + sz/8, ay - sz/8, IC);
}

// ── Trash ────────────────────────────────────────────────────────────────────

void IconAtlas::drawTrash(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 5;
    int x = ox + m, y = oy + m + sz/8;
    int w = sz - 2*m, h = sz - 2*m - sz/8;
    pm.DrawRect(x, y, w, h, Color(200, 100, 100, 255), false);
    // Lid
    pm.DrawLine(ox + m - 1, y, ox + sz - m + 1, y, Color(200, 100, 100, 255));
    // Handle
    pm.DrawLine(ox + sz/2 - sz/8, y, ox + sz/2 - sz/8, y - sz/8, Color(200, 100, 100, 255));
    pm.DrawLine(ox + sz/2 + sz/8, y, ox + sz/2 + sz/8, y - sz/8, Color(200, 100, 100, 255));
    pm.DrawLine(ox + sz/2 - sz/8, y - sz/8, ox + sz/2 + sz/8, y - sz/8, Color(200, 100, 100, 255));
}

// ── Edit (pencil) ────────────────────────────────────────────────────────────

void IconAtlas::drawEdit(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 5;
    pm.DrawLine(ox + sz - m, oy + m, ox + m, oy + sz - m, IC);
    pm.DrawLine(ox + m, oy + sz - m, ox + m + sz/8, oy + sz - m, IC);
    pm.DrawLine(ox + sz - m, oy + m, ox + sz - m - sz/8, oy + m, IC);
}

// ── Home ─────────────────────────────────────────────────────────────────────

void IconAtlas::drawHome(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2;
    int m = sz / 5;
    // Roof
    pm.DrawLine(cx, oy + m, ox + m, oy + sz/2, IC);
    pm.DrawLine(cx, oy + m, ox + sz - m, oy + sz/2, IC);
    // Walls
    int wx = ox + m + 2, wy = oy + sz/2;
    int ww = sz - 2*m - 4, wh = sz/2 - m - 1;
    pm.DrawRect(wx, wy, ww, wh, IC, false);
    // Door
    pm.DrawRect(cx - sz/10, wy + wh/3, sz/5, wh - wh/3, IC, true);
}

// ── User ─────────────────────────────────────────────────────────────────────

void IconAtlas::drawUser(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/3;
    int r = sz / 6;
    pm.DrawCircle(cx, cy, r, IC, true);
    // Body
    int bx = ox + sz/4, by = oy + sz/2 + sz/8;
    int bw = sz/2, bh = sz/4;
    pm.DrawRect(bx, by, bw, bh, IC, true);
}

// ── Warning ──────────────────────────────────────────────────────────────────

void IconAtlas::drawWarning(BuImage& pm, int ox, int oy, int sz)
{
    Color yel(240, 200, 60, 255);
    int cx = ox + sz/2;
    int m = sz / 6;
    // Triangle outline
    pm.DrawLine(cx, oy + m, ox + m, oy + sz - m, yel);
    pm.DrawLine(ox + m, oy + sz - m, ox + sz - m, oy + sz - m, yel);
    pm.DrawLine(ox + sz - m, oy + sz - m, cx, oy + m, yel);
    // Exclamation mark
    pm.DrawLine(cx, oy + sz/3, cx, oy + sz*2/3 - 1, yel);
    pm.SetPixel(cx, oy + sz*2/3 + 1, yel.r, yel.g, yel.b, yel.a);
}

// ── Info ─────────────────────────────────────────────────────────────────────

void IconAtlas::drawInfo(BuImage& pm, int ox, int oy, int sz)
{
    int cx = ox + sz/2, cy = oy + sz/2;
    int r = sz / 3;
    pm.DrawCircle(cx, cy, r, IC2, false);
    // "i"
    pm.SetPixel(cx, cy - r/2, IC2.r, IC2.g, IC2.b, IC2.a);
    pm.DrawLine(cx, cy - r/4, cx, cy + r/2, IC2);
}

// ── Error ────────────────────────────────────────────────────────────────────

void IconAtlas::drawError(BuImage& pm, int ox, int oy, int sz)
{
    Color red(220, 80, 80, 255);
    int cx = ox + sz/2, cy = oy + sz/2;
    int r = sz / 3;
    pm.DrawCircle(cx, cy, r, red, false);
    int d = r * 2 / 3;
    pm.DrawLine(cx - d, cy - d, cx + d, cy + d, red);
    pm.DrawLine(cx + d, cy - d, cx - d, cy + d, red);
}

// ── ViewDetail ───────────────────────────────────────────────────────────────

void IconAtlas::drawViewDetail(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 5;
    int y = oy + m;
    int lineH = (sz - 2*m) / 4;
    for (int i = 0; i < 4; ++i) {
        int ly = y + i * lineH + lineH / 2;
        // Small square (icon placeholder)
        pm.DrawRect(ox + m, ly - 1, 3, 3, IC2, true);
        // Text line
        pm.DrawLine(ox + m + 5, ly, ox + sz - m, ly, IC);
    }
}

// ── ViewList ─────────────────────────────────────────────────────────────────

void IconAtlas::drawViewList(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 5;
    int y = oy + m;
    int lineH = (sz - 2*m) / 4;
    for (int i = 0; i < 4; ++i) {
        int ly = y + i * lineH + lineH / 2;
        pm.DrawLine(ox + m, ly, ox + sz - m, ly, IC);
    }
}

// ── ViewGrid ─────────────────────────────────────────────────────────────────

void IconAtlas::drawViewGrid(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 5;
    int gap = 2;
    int cellW = (sz - 2*m - gap) / 2;
    int cellH = (sz - 2*m - gap) / 2;
    pm.DrawRect(ox + m,              oy + m,               cellW, cellH, IC, false);
    pm.DrawRect(ox + m + cellW + gap, oy + m,               cellW, cellH, IC, false);
    pm.DrawRect(ox + m,              oy + m + cellH + gap, cellW, cellH, IC, false);
    pm.DrawRect(ox + m + cellW + gap, oy + m + cellH + gap, cellW, cellH, IC, false);
}

// ── Play (right-pointing triangle) ──────────────────────────────────────────

void IconAtlas::drawPlay(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 4;
    int x0 = ox + m + 1;
    int y0 = oy + m;
    int x1 = ox + sz - m;
    int cy = oy + sz / 2;
    // Fill triangle with horizontal scan lines
    int h = sz - 2 * m;
    for (int row = 0; row < h; ++row) {
        float t = static_cast<float>(row) / static_cast<float>(h - 1);
        // top-to-bottom: x goes from x0 to x1 (widest at cy) then back
        float frac = (t <= 0.5f) ? (t * 2.f) : (2.f - t * 2.f);
        int xe = x0 + static_cast<int>(frac * (x1 - x0));
        pm.DrawLine(x0, y0 + row, xe, y0 + row, IC);
    }
}

// ── Pause (two vertical bars) ───────────────────────────────────────────────

void IconAtlas::drawPause(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 4;
    int barW = std::max(2, sz / 6);
    int gap = std::max(2, sz / 6);
    int totalW = barW * 2 + gap;
    int x0 = ox + (sz - totalW) / 2;
    pm.DrawRect(x0,              oy + m, barW, sz - 2 * m, IC, true);
    pm.DrawRect(x0 + barW + gap, oy + m, barW, sz - 2 * m, IC, true);
}

// ── Stop (filled square) ────────────────────────────────────────────────────

void IconAtlas::drawStop(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 4;
    pm.DrawRect(ox + m, oy + m, sz - 2 * m, sz - 2 * m, IC, true);
}

// ── StepForward (triangle + vertical bar) ───────────────────────────────────

void IconAtlas::drawStepForward(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 4;
    int barW = std::max(2, sz / 8);
    // Triangle (left part)
    int triRight = ox + sz - m - barW - 1;
    int triLeft  = ox + m;
    int h = sz - 2 * m;
    for (int row = 0; row < h; ++row) {
        float t = static_cast<float>(row) / static_cast<float>(h - 1);
        float frac = (t <= 0.5f) ? (t * 2.f) : (2.f - t * 2.f);
        int xe = triLeft + static_cast<int>(frac * (triRight - triLeft));
        pm.DrawLine(triLeft, oy + m + row, xe, oy + m + row, IC);
    }
    // Vertical bar (right)
    pm.DrawRect(ox + sz - m - barW, oy + m, barW, h, IC, true);
}

// ── StepBack (vertical bar + triangle pointing left) ────────────────────────

void IconAtlas::drawStepBack(BuImage& pm, int ox, int oy, int sz)
{
    int m = sz / 4;
    int barW = std::max(2, sz / 8);
    int h = sz - 2 * m;
    // Vertical bar (left)
    pm.DrawRect(ox + m, oy + m, barW, h, IC, true);
    // Triangle (right part, pointing left)
    int triLeft  = ox + m + barW + 1;
    int triRight = ox + sz - m;
    for (int row = 0; row < h; ++row) {
        float t = static_cast<float>(row) / static_cast<float>(h - 1);
        float frac = (t <= 0.5f) ? (t * 2.f) : (2.f - t * 2.f);
        int xs = triRight - static_cast<int>(frac * (triRight - triLeft));
        pm.DrawLine(xs, oy + m + row, triRight, oy + m + row, IC);
    }
}

// ── Record (filled red circle) ──────────────────────────────────────────────

void IconAtlas::drawRecord(BuImage& pm, int ox, int oy, int sz)
{
    static const Color red(220, 60, 60, 255);
    int cx = ox + sz / 2;
    int cy = oy + sz / 2;
    int r  = sz / 3;
    pm.DrawCircle(cx, cy, r, red, true);
}
