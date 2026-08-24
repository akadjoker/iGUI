#pragma once

#include "Widget.hpp"
#include <ct/json.hpp>

using json = ct::Json;

namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  WidgetSerializer - save / load widget trees to/from JSON
// ═════════════════════════════════════════════════════════════════════════════

class WidgetSerializer
{
public:
    using WidgetFactory = ct::Function<Widget*(const json& j, Widget* parent)>;

    static void registerType(const String& typeName, WidgetFactory factory);
    static String typeName(const Widget* w);

    // Serialize
    static json save(const Widget* root);
    static bool saveToFile(const Widget* root, const String& path);

    // Deserialize
    static Widget* load(const json& j, Widget* parent);
    static Widget* loadFromFile(const String& path, Widget* parent);

    // Call once to register all standard BuGUI widget types
    static void registerBuiltinTypes();

private:
    static ct::HashMap<String, WidgetFactory>& factories();

    static json serializeBase(const Widget* w);
    static void deserializeBase(const json& j, Widget* w);

    static json serializeChildren(const Widget* w);
    static void deserializeChildren(const json& j, Widget* w);
};


} // namespace BuGUI
