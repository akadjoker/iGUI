#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include "ViewWidgets.hpp"
#include <string>
#include <functional>
#include <vector>

namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  Dialog — modal overlay with scrim, title, message, action buttons
//
//  Usage:
//    auto* dlg = WidgetApp::instance().showDialog("Title", "Message");
//    dlg->addButton("Cancel", Dialog::Role::Cancel);
//    dlg->addButton("OK",     Dialog::Role::Accept, [](){ ... });
//
//  Or via convenience helpers:
//    AlertDialog::show("Error", "File not found.");
//    ConfirmDialog::show("Delete?", "This cannot be undone.",
//                        []() { /* confirmed */ });
// ═════════════════════════════════════════════════════════════════════════════

class Dialog : public Widget
{
public:
    enum class Role { Normal, Cancel, Accept, Danger };

    struct ButtonDef {
        std::string          label;
        Role                 role     = Role::Normal;
        std::function<void()> action;
    };

    Dialog(const std::string& title, const std::string& message);

    /// @brief Add an action button to the dialog.
    Dialog& addButton(const std::string& label,
                      Role role = Role::Normal,
                      std::function<void()> action = nullptr);

    /// @brief Set a callback invoked on close.
    void setOnClose(std::function<void()> cb) { onClose_ = std::move(cb); }

    /// @brief Show the dialog as a modal popup.
    void show();
    /// @brief Close and dismiss the dialog.
    void close();

    // ── Widget overrides ──────────────────────────────────────────────────
    bool popupContains(float x, float y) const override;
    void resetPopupState() override {}

    Vec2f sizeHint() const override;
    void  layout()   override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onKeyPress(KeyEvent& e)     override;

    static constexpr float kWidth    = 360.0f;
    static constexpr float kBtnH     = 32.0f;
    static constexpr float kBtnW     = 90.0f;
    static constexpr float kPad      = 20.0f;
    static constexpr float kTitleH   = 44.0f;
    static constexpr float kMsgPad   = 16.0f;

    std::string              title_;
    std::string              message_;
    std::vector<ButtonDef>   buttons_;
    std::function<void()>    onClose_;

    int hoveredBtn_ = -1;

    float dialogH_() const;
    Rect  btnRect_(int idx) const;
    Rect  dialogRect_() const;
    int   hitButton_(float x, float y) const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  AlertDialog — simple informational dialog with a single OK button
//    AlertDialog::show("Info", "Operation completed successfully.");
// ═════════════════════════════════════════════════════════════════════════════

class AlertDialog : public Dialog
{
public:
    AlertDialog(const std::string& title, const std::string& message);

    /// @brief Show a simple alert with a single OK button.
    static void show(const std::string& title, const std::string& message,
                     std::function<void()> onClose = nullptr);
};

// ═════════════════════════════════════════════════════════════════════════════
//  ConfirmDialog — dialog with Cancel + Confirm buttons
//    ConfirmDialog::show("Delete?", "This cannot be undone.",
//                        []() { doDelete(); });
// ═════════════════════════════════════════════════════════════════════════════

class ConfirmDialog : public Dialog
{
public:
    ConfirmDialog(const std::string& title, const std::string& message,
                  std::function<void()> onConfirm = nullptr,
                  std::function<void()> onCancel  = nullptr);

    /// @brief Show a confirmation dialog with Cancel and Confirm.
    static void show(const std::string& title, const std::string& message,
                     std::function<void()> onConfirm,
                     std::function<void()> onCancel = nullptr);
};

// ═════════════════════════════════════════════════════════════════════════════
//  Toast — Android-style brief notification (bottom-center, stacks)
//
//    Toast::show("File saved.");
//    Toast::show("Connection failed.", Toast::Type::Error, 5.0f);
//
//  Called automatically by WidgetApp::paint() — no Widget, no popup.
// ═════════════════════════════════════════════════════════════════════════════

class Toast
{
public:
    enum class Type { Info, Success, Warning, Error };

    static constexpr float kDefaultDuration = 3.0f;
    static constexpr float kFadeIn          = 0.25f;
    static constexpr float kFadeOut         = 0.35f;

    /// @brief Show a brief notification message.
    static void show(const std::string& message,
                     Type               type     = Type::Info,
                     float              duration = kDefaultDuration);

    /// @brief Check if any toast is visible.
    static bool hasActive();
    /// @brief Advance toast animations.
    static void tick(float dt);
    /// @brief Render all active toasts.
    static void paint(PaintContext& ctx, float screenW, float screenH);

    struct Entry {
        std::string message;
        Type        type;
        float       lifetime = kDefaultDuration;
        float       elapsed  = 0.0f;
        float       alpha    = 0.0f;
    };

private:
    static std::vector<Entry>& entries();
};

// ═════════════════════════════════════════════════════════════════════════════
//  MessageBox - modal-style alert/confirm dialog (FloatWindow)
//
//    auto* mb = app.addFloat<MessageBox>("Confirm", "Delete this file?",
//                                         MessageBox::YesNo, MessageBox::Question);
//    mb->result.connect([](MessageBox::Result r) { ... });
// ═════════════════════════════════════════════════════════════════════════════

class BoxLayout;
class Label;
class Button;
class TextInput;

class MessageBox : public FloatWindow
{
public:
    enum Type     { Info, Warning, Error, Question };
    enum Buttons  { Ok, OkCancel, YesNo, YesNoCancel };
    enum Result   { ResultOk, ResultCancel, ResultYes, ResultNo };

    MessageBox(const std::string& title, const std::string& message,
               Buttons buttons = Ok, Type type = Info);

    /// @brief Emitted with the user's chosen result.
    Signal<Result> result;

private:
    Type     type_;
    Buttons  buttons_;
    std::string message_;

    BoxLayout* mainLayout_ = nullptr;
    Label*     msgLabel_   = nullptr;

    void buildUI();
    void close(Result r);
    void onKeyPress(KeyEvent& e) override;
};

// ═════════════════════════════════════════════════════════════════════════════
//  InputBox - simple text input dialog (FloatWindow)
//
//    auto* ib = app.addFloat<InputBox>("Rename", "Enter new name:");
//    ib->setText("old_name.txt");
//    ib->accepted.connect([](const std::string& text) { ... });
// ═════════════════════════════════════════════════════════════════════════════

class InputBox : public FloatWindow
{
public:
    InputBox(const std::string& title, const std::string& prompt = "");

    /// @brief Set the input text.
    void setText(const std::string& t);
    /// @brief Get the input text.
    std::string text() const;
    /// @brief Set placeholder text.
    void setPlaceholder(const std::string& p);

    /// @brief Emitted with the text when OK is pressed.
    Signal<std::string> accepted;
    /// @brief Emitted when Cancel is pressed.
    Signal<>            cancelled;

private:
    std::string prompt_;

    BoxLayout* mainLayout_  = nullptr;
    Label*     promptLabel_ = nullptr;
    TextInput* textInput_   = nullptr;

    void buildUI();
    void onOk();
    void onCancel();
    void onKeyPress(KeyEvent& e) override;
};

} // namespace BuGUI
