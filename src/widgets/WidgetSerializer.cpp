#include "pch.hpp"
#include "WidgetSerializer.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "InputWidgets.hpp"
#include "Widgets.hpp"

// ═════════════════════════════════════════════════════════════════════════════
//  Registry
// ═════════════════════════════════════════════════════════════════════════════

std::unordered_map<std::string, WidgetSerializer::WidgetFactory>&
WidgetSerializer::factories()
{
    static std::unordered_map<std::string, WidgetFactory> map;
    return map;
}

void WidgetSerializer::registerType(const std::string& typeName, WidgetFactory factory)
{
    factories()[typeName] = std::move(factory);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Type name from widget (dynamic_cast chain, most derived first)
// ═════════════════════════════════════════════════════════════════════════════

std::string WidgetSerializer::typeName(const Widget* w)
{
    if (dynamic_cast<const ImageView*>(w))     return "ImageView";
    if (dynamic_cast<const ScrollView*>(w))    return "ScrollView";
    if (dynamic_cast<const ScrollBar*>(w))     return "ScrollBar";
    if (dynamic_cast<const Canvas*>(w))        return "Canvas";
    if (dynamic_cast<const ProgressBar*>(w))   return "ProgressBar";
    if (dynamic_cast<const Slider*>(w))        return "Slider";
    if (dynamic_cast<const ComboBox*>(w))      return "ComboBox";
    if (dynamic_cast<const ListWidget*>(w))    return "ListWidget";
    if (dynamic_cast<const ListBox*>(w))       return "ListBox";
    if (dynamic_cast<const RadioButton*>(w))   return "RadioButton";
    if (dynamic_cast<const Switch*>(w))        return "Switch";
    if (dynamic_cast<const CheckBox*>(w))      return "CheckBox";
    if (dynamic_cast<const Button*>(w))        return "Button";
    if (dynamic_cast<const Line*>(w))          return "Line";
    if (dynamic_cast<const Spacer*>(w))        return "Spacer";
    if (dynamic_cast<const Label*>(w))         return "Label";
    if (dynamic_cast<const Panel*>(w))         return "Panel";
    if (dynamic_cast<const StatusBar*>(w))     return "StatusBar";
    if (dynamic_cast<const Collapsible*>(w))   return "Collapsible";
    if (dynamic_cast<const StackLayout*>(w))   return "StackLayout";
    if (dynamic_cast<const FormLayout*>(w))    return "FormLayout";
    if (dynamic_cast<const FlowLayout*>(w))    return "FlowLayout";
    if (dynamic_cast<const Overlay*>(w))       return "Overlay";
    if (dynamic_cast<const Splitter*>(w))      return "Splitter";
    if (dynamic_cast<const GridLayout*>(w))    return "GridLayout";
    if (dynamic_cast<const BorderLayout*>(w))  return "BorderLayout";
    if (dynamic_cast<const BoxLayout*>(w))     return "BoxLayout";
    return "Widget";
}

// ═════════════════════════════════════════════════════════════════════════════
//  JSON helpers for Color, Rect, Edges, Align, etc.
// ═════════════════════════════════════════════════════════════════════════════

static json colorToJson(const Color& c)
{
    return json::array({c.r, c.g, c.b, c.a});
}

static Color colorFromJson(const json& j)
{
    return Color(j[0].get<uint8_t>(), j[1].get<uint8_t>(),
                 j[2].get<uint8_t>(), j[3].get<uint8_t>());
}

static json edgesToJson(const Edges& e)
{
    return json::array({e.top, e.right, e.bottom, e.left});
}

static Edges edgesFromJson(const json& j)
{
    return Edges(j[0].get<float>(), j[1].get<float>(),
                 j[2].get<float>(), j[3].get<float>());
}

static const char* alignToStr(Align a)
{
    switch (a) {
        case Align::Start:  return "Start";
        case Align::Center: return "Center";
        case Align::End:    return "End";
        case Align::Fill:   return "Fill";
    }
    return "Fill";
}

static Align alignFromStr(const std::string& s)
{
    if (s == "Start")  return Align::Start;
    if (s == "Center") return Align::Center;
    if (s == "End")    return Align::End;
    return Align::Fill;
}

static const char* mainAlignToStr(MainAlign a)
{
    switch (a) {
        case MainAlign::Start:        return "Start";
        case MainAlign::Center:       return "Center";
        case MainAlign::End:          return "End";
        case MainAlign::SpaceBetween: return "SpaceBetween";
        case MainAlign::SpaceEvenly:  return "SpaceEvenly";
    }
    return "Start";
}

static MainAlign mainAlignFromStr(const std::string& s)
{
    if (s == "Center")       return MainAlign::Center;
    if (s == "End")          return MainAlign::End;
    if (s == "SpaceBetween") return MainAlign::SpaceBetween;
    if (s == "SpaceEvenly")  return MainAlign::SpaceEvenly;
    return MainAlign::Start;
}

static const char* textAlignToStr(TextAlign a)
{
    switch (a) {
        case TextAlign::LEFT:    return "Left";
        case TextAlign::CENTER:  return "Center";
        case TextAlign::RIGHT:   return "Right";
        case TextAlign::JUSTIFY: return "Justify";
    }
    return "Left";
}

static TextAlign textAlignFromStr(const std::string& s)
{
    if (s == "Center") return TextAlign::CENTER;
    if (s == "Right")  return TextAlign::RIGHT;
    return TextAlign::LEFT;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Base properties (common to all widgets)
// ═════════════════════════════════════════════════════════════════════════════

json WidgetSerializer::serializeBase(const Widget* w)
{
    json j;
    j["type"] = typeName(w);

    if (!w->id().empty())
        j["id"] = w->id();

    const auto& r = w->rect();
    if (r.x != 0 || r.y != 0 || r.w != 0 || r.h != 0)
        j["rect"] = json::array({r.x, r.y, r.w, r.h});

    if (!w->isVisible())
        j["visible"] = false;

    if (!w->isEnabled())
        j["enabled"] = false;

    const auto& m = w->margins();
    if (m.top != 0 || m.right != 0 || m.bottom != 0 || m.left != 0)
        j["margins"] = edgesToJson(m);

    if (w->layoutAlign() != Align::Fill)
        j["align"] = alignToStr(w->layoutAlign());

    if (w->stretch() != 0.0f)
        j["stretch"] = w->stretch();

    if (!w->tags().empty()) {
        json tagArr = json::array();
        for (const auto& t : w->tags())
            tagArr.push_back(t);
        j["tags"] = tagArr;
    }

    return j;
}

void WidgetSerializer::deserializeBase(const json& j, Widget* w)
{
    if (j.contains("id"))
        w->setId(j["id"].get<std::string>());

    if (j.contains("rect")) {
        const auto& r = j["rect"];
        w->setRect({r[0].get<float>(), r[1].get<float>(),
                     r[2].get<float>(), r[3].get<float>()});
    }

    if (j.contains("visible"))
        w->setVisible(j["visible"].get<bool>());

    if (j.contains("enabled"))
        w->setEnabled(j["enabled"].get<bool>());

    if (j.contains("margins"))
        w->setMargins(edgesFromJson(j["margins"]));

    if (j.contains("align"))
        w->setLayoutAlign(alignFromStr(j["align"].get<std::string>()));

    if (j.contains("stretch"))
        w->setStretch(j["stretch"].get<float>());

    if (j.contains("tags"))
        for (const auto& t : j["tags"])
            w->addTag(t.get<std::string>());
}

// ═════════════════════════════════════════════════════════════════════════════
//  Children
// ═════════════════════════════════════════════════════════════════════════════

json WidgetSerializer::serializeChildren(const Widget* w)
{
    json arr = json::array();
    for (const auto* child : w->children())
        arr.push_back(save(child));
    return arr;
}

void WidgetSerializer::deserializeChildren(const json& j, Widget* w)
{
    if (!j.contains("children")) return;
    for (const auto& cj : j["children"])
        load(cj, w);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Save (widget → JSON)
// ═════════════════════════════════════════════════════════════════════════════

json WidgetSerializer::save(const Widget* root)
{
    json j = serializeBase(root);
    const std::string type = typeName(root);

    if (type == "Label") {
        auto* w = static_cast<const Label*>(root);
        j["text"] = w->text();
        if (w->align() != TextAlign::LEFT)
            j["textAlign"] = textAlignToStr(w->align());
    }
    else if (type == "Spacer") {
        auto* w = static_cast<const Spacer*>(root);
        auto hint = w->sizeHint();
        if (hint.x > 0) j["size"] = hint.x;
    }
    else if (type == "Line") {
        auto* w = static_cast<const Line*>(root);
        if (w->thickness() != 1.0f)
            j["thickness"] = w->thickness();
        const auto& defC = Theme::instance().borderColor;
        if (w->color().r != defC.r || w->color().g != defC.g ||
            w->color().b != defC.b || w->color().a != defC.a)
            j["color"] = colorToJson(w->color());
    }
    else if (type == "Button") {
        auto* w = static_cast<const Button*>(root);
        j["text"] = w->text();
        if (w->align() != TextAlign::CENTER)
            j["textAlign"] = textAlignToStr(w->align());
    }
    else if (type == "Panel") {
        auto* w = static_cast<const Panel*>(root);
        const auto& bg = w->bgColor();
        const auto& def = Theme::instance().panelColor;
        if (bg.r != def.r || bg.g != def.g || bg.b != def.b || bg.a != def.a)
            j["bgColor"] = colorToJson(bg);
    }
    else if (type == "CheckBox") {
        auto* w = static_cast<const CheckBox*>(root);
        j["text"] = w->text();
        if (w->isChecked()) j["checked"] = true;
    }
    else if (type == "RadioButton") {
        auto* w = static_cast<const RadioButton*>(root);
        j["text"] = w->text();
        if (w->isSelected()) j["selected"] = true;
    }
    else if (type == "Switch") {
        auto* w = static_cast<const Switch*>(root);
        j["text"] = w->text();
        if (w->isOn()) j["on"] = true;
    }
    else if (type == "ListBox") {
        auto* w = static_cast<const ListBox*>(root);
        json arr = json::array();
        for (int i = 0; i < w->itemCount(); ++i)
            arr.push_back(w->itemText(i));
        j["items"] = arr;
        if (w->selectedIndex() >= 0) j["selected"] = w->selectedIndex();
    }
    else if (type == "ComboBox") {
        auto* w = static_cast<const ComboBox*>(root);
        json arr = json::array();
        for (int i = 0; i < w->itemCount(); ++i)
            arr.push_back(w->itemText(i));
        j["items"] = arr;
        if (w->selectedIndex() >= 0) j["selected"] = w->selectedIndex();
    }
    else if (type == "ListWidget") {
        auto* w = static_cast<const ListWidget*>(root);
        if (w->selectedIndex() >= 0) j["selected"] = w->selectedIndex();
    }
    else if (type == "Slider") {
        auto* w = static_cast<const Slider*>(root);
        j["min"]   = w->minValue();
        j["max"]   = w->maxValue();
        j["value"] = w->value();
        if (w->orientation() == LayoutDir::Vertical)
            j["orientation"] = "Vertical";
    }
    else if (type == "ProgressBar") {
        auto* w = static_cast<const ProgressBar*>(root);
        j["min"]   = w->minValue();
        j["max"]   = w->maxValue();
        j["value"] = w->value();
        if (w->orientation() == LayoutDir::Vertical)
            j["orientation"] = "Vertical";
        if (!w->text().empty()) j["text"] = w->text();
        if (w->textAlign() != TextAlign::CENTER)
            j["textAlign"] = textAlignToStr(w->textAlign());
        const Color defBar(140, 140, 145, 255);
        if (w->barColor().r != defBar.r || w->barColor().g != defBar.g ||
            w->barColor().b != defBar.b || w->barColor().a != defBar.a)
            j["barColor"] = colorToJson(w->barColor());
    }
    else if (type == "ScrollBar") {
        // ScrollBar: orientation stored, value serialized
        // contentSize/viewSize are typically set by ScrollView, not serialized
        auto* w = static_cast<const ScrollBar*>(root);
        j["value"] = w->value();
        if (w->maxValue() > 0) j["maxValue"] = w->maxValue();
    }
    else if (type == "ScrollView") {
        auto* w = static_cast<const ScrollView*>(root);
        if (w->scrollX() != 0) j["scrollX"] = w->scrollX();
        if (w->scrollY() != 0) j["scrollY"] = w->scrollY();
    }
    else if (type == "Canvas") {
        auto* w = static_cast<const Canvas*>(root);
        const auto& bg = w->bgColor();
        const auto& def = Theme::instance().panelColor;
        if (bg.r != def.r || bg.g != def.g || bg.b != def.b || bg.a != def.a)
            j["bgColor"] = colorToJson(bg);
    }
    else if (type == "ImageView") {
        auto* w = static_cast<const ImageView*>(root);
        if (w->offsetX() != 0 || w->offsetY() != 0)
            j["offset"] = json::array({w->offsetX(), w->offsetY()});
    }
    else if (type == "GridLayout") {
        auto* w = static_cast<const GridLayout*>(root);
        j["cols"] = w->cols();
        j["spacingH"] = w->spacingH();
        j["spacingV"] = w->spacingV();
        const auto& p = w->padding();
        if (p.top != 0 || p.right != 0 || p.bottom != 0 || p.left != 0)
            j["padding"] = edgesToJson(p);
    }
    else if (type == "StatusBar") {
        auto* w = static_cast<const StatusBar*>(root);
        j["bgColor"] = colorToJson(w->bgColor());
        j["spacing"] = w->spacing();
        if (!w->text().empty()) j["text"] = w->text();
    }
    else if (type == "Collapsible") {
        auto* w = static_cast<const Collapsible*>(root);
        j["title"] = w->title();
        j["expanded"] = w->isExpanded();
        j["headerHeight"] = w->headerHeight();
        j["contentPadding"] = w->contentPadding();
        j["contentSpacing"] = w->contentSpacing();
        if (w->headerColor().a > 0)
            j["headerColor"] = colorToJson(w->headerColor());
    }
    else if (type == "StackLayout") {
        auto* w = static_cast<const StackLayout*>(root);
        j["currentIndex"] = w->currentIndex();
    }
    else if (type == "FormLayout") {
        auto* w = static_cast<const FormLayout*>(root);
        j["labelWidth"] = w->labelWidth();
        j["spacingH"] = w->spacingH();
        j["spacingV"] = w->spacingV();
        const auto& p = w->padding();
        if (p.top != 0 || p.right != 0 || p.bottom != 0 || p.left != 0)
            j["padding"] = edgesToJson(p);
    }
    else if (type == "FlowLayout") {
        auto* w = static_cast<const FlowLayout*>(root);
        j["spacingH"] = w->spacingH();
        j["spacingV"] = w->spacingV();
        const auto& p = w->padding();
        if (p.top != 0 || p.right != 0 || p.bottom != 0 || p.left != 0)
            j["padding"] = edgesToJson(p);
    }
    else if (type == "Splitter") {
        auto* w = static_cast<const Splitter*>(root);
        j["dir"] = (w->dir() == LayoutDir::Horizontal) ? "Horizontal" : "Vertical";
        j["ratio"] = w->ratio();
        j["handleSize"] = w->handleSize();
        j["minSize"] = w->minSize();
    }
    else if (type == "BorderLayout") {
        auto* w = static_cast<const BorderLayout*>(root);
        j["spacing"] = w->spacing();
        const auto& p = w->padding();
        if (p.top != 0 || p.right != 0 || p.bottom != 0 || p.left != 0)
            j["padding"] = edgesToJson(p);
        const char* regionNames[] = {"topSize", "bottomSize", "leftSize", "rightSize", "centerSize"};
        for (int i = 0; i < 5; ++i) {
            float s = w->regionSize(static_cast<BorderRegion>(i));
            if (s != 0) j[regionNames[i]] = s;
        }
    }
    else if (type == "BoxLayout") {
        auto* w = static_cast<const BoxLayout*>(root);
        j["dir"] = (w->dir() == LayoutDir::Horizontal) ? "Horizontal" : "Vertical";
        j["spacing"] = w->spacing();
        const auto& p = w->padding();
        if (p.top != 0 || p.right != 0 || p.bottom != 0 || p.left != 0)
            j["padding"] = edgesToJson(p);
        if (w->mainAlign() != MainAlign::Start)
            j["mainAlign"] = mainAlignToStr(w->mainAlign());
    }

    if (!root->children().empty())
        j["children"] = serializeChildren(root);

    return j;
}

// ═════════════════════════════════════════════════════════════════════════════
//  File I/O — uses IO callbacks (no SDL dependency)
// ═════════════════════════════════════════════════════════════════════════════

bool WidgetSerializer::saveToFile(const Widget* root, const std::string& path)
{
    auto& io = BuGUI::GetIO();
    if (!io.writeFile) {
        fprintf(stderr, "WidgetSerializer: writeFile callback not set\n");
        return false;
    }
    json j = save(root);
    return io.writeFile(path, j.dump(2));
}

// ═════════════════════════════════════════════════════════════════════════════
//  Load (JSON → widget)
// ═════════════════════════════════════════════════════════════════════════════

Widget* WidgetSerializer::load(const json& j, Widget* parent)
{
    if (!j.contains("type")) return nullptr;

    std::string type = j["type"].get<std::string>();
    auto& fmap = factories();
    auto it = fmap.find(type);
    if (it == fmap.end()) {
        fprintf(stderr, "WidgetSerializer: unknown type '%s'\n", type.c_str());
        return nullptr;
    }

    Widget* w = it->second(j, parent);
    if (!w) return nullptr;

    deserializeBase(j, w);
    deserializeChildren(j, w);
    return w;
}

Widget* WidgetSerializer::loadFromFile(const std::string& path, Widget* parent)
{
    auto& io = BuGUI::GetIO();
    if (!io.readFile) {
        fprintf(stderr, "WidgetSerializer: readFile callback not set\n");
        return nullptr;
    }

    std::string data = io.readFile(path);
    if (data.empty()) {
        fprintf(stderr, "WidgetSerializer: cannot read '%s'\n", path.c_str());
        return nullptr;
    }

    json j;
    try {
        j = json::parse(data);
    } catch (const json::parse_error& e) {
        fprintf(stderr, "WidgetSerializer: parse error in '%s': %s\n",
                path.c_str(), e.what());
        return nullptr;
    }

    return load(j, parent);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Built-in type factories
// ═════════════════════════════════════════════════════════════════════════════

void WidgetSerializer::registerBuiltinTypes()
{
    registerType("Widget", [](const json&, Widget* parent) -> Widget* {
        return parent->createChild<Widget>();
    });

    registerType("Label", [](const json& j, Widget* parent) -> Widget* {
        std::string text = j.value("text", "");
        auto* w = parent->createChild<Label>(text);
        if (j.contains("textAlign"))
            w->setAlign(textAlignFromStr(j["textAlign"].get<std::string>()));
        return w;
    });

    registerType("Spacer", [](const json& j, Widget* parent) -> Widget* {
        float size = j.value("size", 0.0f);
        return parent->createChild<Spacer>(size);
    });

    registerType("Line", [](const json& j, Widget* parent) -> Widget* {
        float thickness = j.value("thickness", 1.0f);
        auto* w = parent->createChild<Line>(thickness);
        if (j.contains("color"))
            w->setColor(colorFromJson(j["color"]));
        return w;
    });

    registerType("Button", [](const json& j, Widget* parent) -> Widget* {
        std::string text = j.value("text", "");
        auto* w = parent->createChild<Button>(text);
        if (j.contains("textAlign"))
            w->setAlign(textAlignFromStr(j["textAlign"].get<std::string>()));
        return w;
    });

    registerType("Panel", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<Panel>();
        if (j.contains("bgColor"))
            w->setBgColor(colorFromJson(j["bgColor"]));
        return w;
    });

    registerType("CheckBox", [](const json& j, Widget* parent) -> Widget* {
        std::string text = j.value("text", "");
        auto* w = parent->createChild<CheckBox>(text);
        if (j.contains("checked"))
            w->setChecked(j["checked"].get<bool>());
        return w;
    });

    registerType("RadioButton", [](const json& j, Widget* parent) -> Widget* {
        std::string text = j.value("text", "");
        auto* w = parent->createChild<RadioButton>(text);
        if (j.contains("selected") && j["selected"].get<bool>())
            w->setSelected(true);
        return w;
    });

    registerType("Switch", [](const json& j, Widget* parent) -> Widget* {
        std::string text = j.value("text", "");
        auto* w = parent->createChild<Switch>(text);
        if (j.contains("on"))
            w->setOn(j["on"].get<bool>());
        return w;
    });

    registerType("ListBox", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<ListBox>();
        if (j.contains("items") && j["items"].is_array())
            for (auto& item : j["items"])
                w->addItem(item.get<std::string>());
        if (j.contains("selected"))
            w->setSelectedIndex(j["selected"].get<int>());
        return w;
    });

    registerType("ComboBox", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<ComboBox>();
        if (j.contains("items") && j["items"].is_array())
            for (auto& item : j["items"])
                w->addItem(item.get<std::string>());
        if (j.contains("selected"))
            w->setSelectedIndex(j["selected"].get<int>());
        return w;
    });

    registerType("ListWidget", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<ListWidget>();
        if (j.contains("selected"))
            w->setSelectedIndex(j["selected"].get<int>());
        return w;
    });

    registerType("Slider", [](const json& j, Widget* parent) -> Widget* {
        float minV = j.value("min", 0.0f);
        float maxV = j.value("max", 1.0f);
        float val  = j.value("value", 0.5f);
        auto* w = parent->createChild<Slider>(minV, maxV, val);
        if (j.contains("orientation") && j["orientation"].get<std::string>() == "Vertical")
            w->setOrientation(LayoutDir::Vertical);
        return w;
    });

    registerType("ProgressBar", [](const json& j, Widget* parent) -> Widget* {
        float minV = j.value("min", 0.0f);
        float maxV = j.value("max", 1.0f);
        float val  = j.value("value", 0.0f);
        auto* w = parent->createChild<ProgressBar>(minV, maxV, val);
        if (j.contains("orientation") && j["orientation"].get<std::string>() == "Vertical")
            w->setOrientation(LayoutDir::Vertical);
        if (j.contains("text"))
            w->setText(j["text"].get<std::string>());
        if (j.contains("textAlign"))
            w->setTextAlign(textAlignFromStr(j["textAlign"].get<std::string>()));
        if (j.contains("barColor"))
            w->setBarColor(colorFromJson(j["barColor"]));
        return w;
    });

    registerType("ScrollBar", [](const json& j, Widget* parent) -> Widget* {
        ScrollBarOrientation dir = ScrollBarOrientation::Vertical;
        if (j.contains("orientation") && j["orientation"].get<std::string>() == "Horizontal")
            dir = ScrollBarOrientation::Horizontal;
        auto* w = parent->createChild<ScrollBar>(dir);
        if (j.contains("value")) w->setValue(j["value"].get<float>());
        return w;
    });

    registerType("ScrollView", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<ScrollView>();
        if (j.contains("scrollX"))  w->setScrollX(j["scrollX"].get<float>());
        if (j.contains("scrollY"))  w->setScrollY(j["scrollY"].get<float>());
        return w;
    });

    registerType("Canvas", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<Canvas>();
        if (j.contains("bgColor"))
            w->setBgColor(colorFromJson(j["bgColor"]));
        return w;
    });

    registerType("ImageView", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<ImageView>();
        if (j.contains("offset")) {
            const auto& o = j["offset"];
            w->setOffset(o[0].get<float>(), o[1].get<float>());
        }
        return w;
    });

    registerType("StatusBar", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<StatusBar>();
        if (j.contains("bgColor")) w->setBgColor(colorFromJson(j["bgColor"]));
        if (j.contains("spacing")) w->setSpacing(j["spacing"].get<float>());
        if (j.contains("text")) w->setText(j["text"].get<std::string>());
        return w;
    });

    registerType("Collapsible", [](const json& j, Widget* parent) -> Widget* {
        std::string title = j.value("title", "");
        bool expanded = j.value("expanded", true);
        auto* w = parent->createChild<Collapsible>(title, expanded);
        if (j.contains("headerHeight"))
            w->setHeaderHeight(j["headerHeight"].get<float>());
        if (j.contains("contentPadding"))
            w->setContentPadding(j["contentPadding"].get<float>());
        if (j.contains("contentSpacing"))
            w->setContentSpacing(j["contentSpacing"].get<float>());
        if (j.contains("headerColor"))
            w->setHeaderColor(colorFromJson(j["headerColor"]));
        return w;
    });

    registerType("BorderLayout", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<BorderLayout>();
        if (j.contains("spacing"))
            w->setSpacing(j["spacing"].get<float>());
        if (j.contains("padding"))
            w->setPadding(edgesFromJson(j["padding"]));
        const char* regionNames[] = {"topSize", "bottomSize", "leftSize", "rightSize", "centerSize"};
        for (int i = 0; i < 5; ++i) {
            if (j.contains(regionNames[i]))
                w->setRegionSize(static_cast<BorderRegion>(i), j[regionNames[i]].get<float>());
        }
        return w;
    });

    registerType("GridLayout", [](const json& j, Widget* parent) -> Widget* {
        int cols = j.value("cols", 2);
        auto* w = parent->createChild<GridLayout>(cols);
        if (j.contains("spacingH") && j.contains("spacingV"))
            w->setSpacing(j["spacingH"].get<float>(), j["spacingV"].get<float>());
        else if (j.contains("spacing"))
            w->setSpacing(j["spacing"].get<float>());
        if (j.contains("padding"))
            w->setPadding(edgesFromJson(j["padding"]));
        return w;
    });

    registerType("StackLayout", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<StackLayout>();
        if (j.contains("currentIndex"))
            w->setCurrentIndex(j["currentIndex"].get<int>());
        return w;
    });

    registerType("FormLayout", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<FormLayout>();
        if (j.contains("labelWidth"))
            w->setLabelWidth(j["labelWidth"].get<float>());
        if (j.contains("spacingH") && j.contains("spacingV"))
            w->setSpacing(j["spacingH"].get<float>(), j["spacingV"].get<float>());
        if (j.contains("padding"))
            w->setPadding(edgesFromJson(j["padding"]));
        return w;
    });

    registerType("FlowLayout", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<FlowLayout>();
        if (j.contains("spacingH") && j.contains("spacingV"))
            w->setSpacing(j["spacingH"].get<float>(), j["spacingV"].get<float>());
        if (j.contains("padding"))
            w->setPadding(edgesFromJson(j["padding"]));
        return w;
    });

    registerType("Overlay", [](const json& j, Widget* parent) -> Widget* {
        (void)j;
        return parent->createChild<Overlay>();
    });

    registerType("Splitter", [](const json& j, Widget* parent) -> Widget* {
        LayoutDir dir = LayoutDir::Horizontal;
        if (j.contains("dir") && j["dir"].get<std::string>() == "Vertical")
            dir = LayoutDir::Vertical;
        auto* w = parent->createChild<Splitter>(dir);
        if (j.contains("ratio"))
            w->setRatio(j["ratio"].get<float>());
        if (j.contains("handleSize"))
            w->setHandleSize(j["handleSize"].get<float>());
        if (j.contains("minSize"))
            w->setMinSize(j["minSize"].get<float>());
        return w;
    });

    registerType("BoxLayout", [](const json& j, Widget* parent) -> Widget* {
        LayoutDir dir = LayoutDir::Vertical;
        if (j.contains("dir")) {
            std::string d = j["dir"].get<std::string>();
            if (d == "Horizontal") dir = LayoutDir::Horizontal;
        }
        auto* w = parent->createChild<BoxLayout>(dir);
        if (j.contains("spacing"))
            w->setSpacing(j["spacing"].get<float>());
        if (j.contains("padding"))
            w->setPadding(edgesFromJson(j["padding"]));
        if (j.contains("mainAlign"))
            w->setMainAlign(mainAlignFromStr(j["mainAlign"].get<std::string>()));
        return w;
    });

    registerType("TabLayout", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<TabLayout>();
        if (j.contains("tabPosition")) {
            std::string p = j["tabPosition"].get<std::string>();
            if (p == "Bottom") w->setTabPosition(TabPosition::Bottom);
            else if (p == "Left") w->setTabPosition(TabPosition::Left);
            else if (p == "Right") w->setTabPosition(TabPosition::Right);
        }
        if (j.contains("closable"))
            w->setTabsClosable(j["closable"].get<bool>());
        if (j.contains("tabBarSize"))
            w->setTabBarSize(j["tabBarSize"].get<float>());
        return w;
    });

    registerType("AnchorLayout", [](const json& j, Widget* parent) -> Widget* {
        auto* w = parent->createChild<AnchorLayout>();
        if (j.contains("padding"))
            w->setPadding(edgesFromJson(j["padding"]));
        return w;
    });
}
