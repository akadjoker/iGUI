#include <BasicWidgets.hpp>
#include <FileSystem.hpp>
#include <InputWidgets.hpp>
#include <LayoutWidgets.hpp>
#include <WidgetSerializer.hpp>
#include <Animation.hpp>
#include <WidgetApp.hpp>

#include <cassert>
#include <cmath>
#include <string>

int main()
{
    using namespace BuGUI;

    assert(FileSystem::exists("include/igui/widgets"));
    assert(FileSystem::isDir("include/igui/widgets"));
    assert(FileSystem::isFile("include/igui/widgets/CodeEditor.hpp"));
    assert(FileSystem::fileName("include/igui/widgets/CodeEditor.hpp") == "CodeEditor.hpp");
    assert(FileSystem::extension("include/igui/widgets/CodeEditor.hpp") == ".hpp");
    const std::vector<FileSystem::Entry> entries = FileSystem::listDir("include/igui/widgets");
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
    assert(saved.at("type").get<std::string>() == "BoxLayout");
    assert(saved.contains("children"));
    assert(saved.at("children").size() == 4u);

    Widget destination;
    Widget* loaded = WidgetSerializer::load(saved, &destination);
    assert(loaded != nullptr);
    assert(WidgetSerializer::typeName(loaded) == "BoxLayout");
    assert(loaded->children().size() == 4u);
    assert(WidgetSerializer::typeName(loaded->children()[0]) == "Label");
    assert(WidgetSerializer::typeName(loaded->children()[1]) == "Button");
    assert(WidgetSerializer::typeName(loaded->children()[2]) == "CheckBox");
    assert(WidgetSerializer::typeName(loaded->children()[3]) == "Slider");

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
