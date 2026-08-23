#pragma once

#include "Widget.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;



namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  WidgetSerializer - save / load widget trees to/from JSON
// ═════════════════════════════════════════════════════════════════════════════

class WidgetSerializer
{
public:
    using WidgetFactory = std::function<Widget*(const json& j, Widget* parent)>;

    static void registerType(const std::string& typeName, WidgetFactory factory);
    static std::string typeName(const Widget* w);

    // Serialize
    static json save(const Widget* root);
    static bool saveToFile(const Widget* root, const std::string& path);

    // Deserialize
    static Widget* load(const json& j, Widget* parent);
    static Widget* loadFromFile(const std::string& path, Widget* parent);

    // Call once to register all standard BuGUI widget types
    static void registerBuiltinTypes();

private:
    static std::unordered_map<std::string, WidgetFactory>& factories();

    static json serializeBase(const Widget* w);
    static void deserializeBase(const json& j, Widget* w);

    static json serializeChildren(const Widget* w);
    static void deserializeChildren(const json& j, Widget* w);
};


} // namespace BuGUI
