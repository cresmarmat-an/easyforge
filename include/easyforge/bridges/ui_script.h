#pragma once

// Lets scripts drive an interface: find elements by name, read and change their
// properties, give them functions to call, and switch displays. Header-only, so
// it compiles only in programs that use both libraries.
//
//     #include <easyforge/bridges/ui_script.h>
//
//     ui::DefineInterface(scripts, ui::Root::Of(window));
//
// In a script:
//
//     constant pages = ui.Find("Pages")
//
//     function OnPlayClicked() then
//         pages.Show("Game", Fade(0.3))
//     end
//
//     ui.Find("Play").OnClick = OnPlayClicked
//
// Positions in dropdowns and lists count from 1, as in script lists, and
// nothing means none is chosen. A function a script gives an element that
// fails logs its error.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/core/Log.h>
#include <easyforge/script.h>
#include <easyforge/ui.h>

namespace easyforge
{
    namespace internal::bridges
    {
        // Calls a script's function for an element's event, logging a failure.
        inline void CallForEvent(const ScriptValue& function, const std::vector<ScriptValue>& arguments)
        {
            Result<ScriptValue> called = function.Call(arguments);
            if (!called)
            {
                Log(LogLevel::Error, "{}", called.Error());
            }
        }

        inline bool IsFunctionOrNothing(const ScriptValue& value)
        {
            return value.Kind() == ScriptValueKind::Function || value.IsNothing();
        }

        inline ScriptValue TextList(const std::vector<std::string>& texts)
        {
            std::vector<ScriptValue> items(texts.begin(), texts.end());
            return ScriptValue::List(std::move(items));
        }

        inline std::vector<std::string> TextsOf(const ScriptValue& list)
        {
            std::vector<std::string> texts;
            for (const ScriptValue& item : list.Items())
            {
                texts.push_back(item.AsText());
            }
            return texts;
        }

        // A position from 0, or -1, as a script's position from 1, or nothing.
        inline ScriptValue PositionForScript(int position)
        {
            return position < 0 ? ScriptValue() : ScriptValue(position + 1);
        }

        inline int PositionFromScript(const ScriptValue& value)
        {
            return value.IsNothing() ? -1 : value.As<int>() - 1;
        }

        // A transition a script made with Fade, Slide, Scale, or Cut.
        inline ui::Transition TransitionFrom(const ScriptValue& value)
        {
            std::string kind = value.Field("Transition").AsText();
            float seconds = value.Field("Seconds").As<float>();
            if (kind == "fade")
            {
                return ui::Transition::Fade(seconds);
            }
            if (kind == "scale")
            {
                return ui::Transition::Scale(seconds);
            }
            if (kind == "slide")
            {
                std::string from = value.Field("From").AsText();
                ui::SlideFrom side = from == "left"     ? ui::SlideFrom::Left
                                     : from == "top"    ? ui::SlideFrom::Top
                                     : from == "bottom" ? ui::SlideFrom::Bottom
                                                        : ui::SlideFrom::Right;
                return ui::Transition::Slide(seconds, side);
            }
            return ui::Transition::Cut();
        }

        class ElementObject final : public ScriptObject
        {
        public:
            ElementObject(ui::Element element, ui::Root root) : Target(std::move(element)), Owner(std::move(root)) {}

            std::string TypeName() const override
            {
                std::string_view kind = Target.Kind();
                return kind.empty() ? "Element" : std::string(kind);
            }

            ScriptValue Get(std::string_view name) override
            {
                ui::Element element = Target;
                ui::Root root = Owner;
                if (name == "Name")
                {
                    return element.Name.Get();
                }
                if (name == "Kind")
                {
                    return TypeName();
                }
                if (name == "Visible")
                {
                    return element.Visible.Get();
                }
                if (name == "Enabled")
                {
                    return element.Enabled.Get();
                }
                if (name == "Opacity")
                {
                    return element.Opacity.Get();
                }
                if (name == "Tooltip")
                {
                    return element.Tooltip.Get();
                }
                if (name == "Find")
                {
                    return ScriptValue::Function("Find", [element, root](std::string inside) -> ScriptValue {
                        ui::Element found = element.Find(inside);
                        return found ? ScriptValue(std::make_shared<ElementObject>(found, root)) : ScriptValue();
                    });
                }
                if (ui::Label label = element.As<ui::Label>())
                {
                    if (name == "Text")
                    {
                        return label.Text.Get();
                    }
                    if (name == "FontSize")
                    {
                        return label.FontSize.Get();
                    }
                }
                if (ui::Button button = element.As<ui::Button>())
                {
                    if (name == "Text")
                    {
                        return button.Text.Get();
                    }
                    if (name == "Click")
                    {
                        return ScriptValue::Function("Click", [button]() { button.Click(); });
                    }
                }
                if (ui::Checkbox check = element.As<ui::Checkbox>())
                {
                    if (name == "Text")
                    {
                        return check.Text.Get();
                    }
                    if (name == "Checked")
                    {
                        return check.Checked.Get();
                    }
                }
                if (ui::Toggle toggle = element.As<ui::Toggle>())
                {
                    if (name == "Text")
                    {
                        return toggle.Text.Get();
                    }
                    if (name == "Checked")
                    {
                        return toggle.Checked.Get();
                    }
                }
                if (ui::Slider slider = element.As<ui::Slider>())
                {
                    if (name == "Value")
                    {
                        return slider.Value.Get();
                    }
                    if (name == "Minimum")
                    {
                        return slider.Minimum.Get();
                    }
                    if (name == "Maximum")
                    {
                        return slider.Maximum.Get();
                    }
                    if (name == "Step")
                    {
                        return slider.Step.Get();
                    }
                }
                if (ui::ProgressBar bar = element.As<ui::ProgressBar>())
                {
                    if (name == "Value")
                    {
                        return bar.Value.Get();
                    }
                    if (name == "Minimum")
                    {
                        return bar.Minimum.Get();
                    }
                    if (name == "Maximum")
                    {
                        return bar.Maximum.Get();
                    }
                }
                if (ui::TextField field = element.As<ui::TextField>())
                {
                    if (name == "Text")
                    {
                        return field.Text.Get();
                    }
                    if (name == "Placeholder")
                    {
                        return field.Placeholder.Get();
                    }
                    if (name == "SelectAll")
                    {
                        return ScriptValue::Function("SelectAll", [field]() { field.SelectAll(); });
                    }
                }
                if (ui::TextArea area = element.As<ui::TextArea>())
                {
                    if (name == "Text")
                    {
                        return area.Text.Get();
                    }
                    if (name == "Placeholder")
                    {
                        return area.Placeholder.Get();
                    }
                }
                if (ui::Dropdown dropdown = element.As<ui::Dropdown>())
                {
                    if (name == "Options")
                    {
                        return TextList(dropdown.Options.Get());
                    }
                    if (name == "Selected")
                    {
                        return PositionForScript(dropdown.Selected.Get());
                    }
                    if (name == "SelectedText")
                    {
                        return dropdown.SelectedText();
                    }
                }
                if (ui::List list = element.As<ui::List>())
                {
                    if (name == "Items")
                    {
                        return TextList(list.Items.Get());
                    }
                    if (name == "Selected")
                    {
                        return PositionForScript(list.Selected.Get());
                    }
                    if (name == "SelectedText")
                    {
                        return list.SelectedText();
                    }
                }
                if (ui::Tabs tabs = element.As<ui::Tabs>())
                {
                    if (name == "Current")
                    {
                        return tabs.Current.Get();
                    }
                }
                if (ui::Displays displays = element.As<ui::Displays>())
                {
                    if (name == "Current")
                    {
                        return displays.Current.Get();
                    }
                    if (name == "Show")
                    {
                        return ScriptValue::Function("Show", [displays](const std::vector<ScriptValue>& arguments) -> Result<ScriptValue> {
                            if (arguments.empty() || arguments.size() > 2 || arguments[0].Kind() != ScriptValueKind::Text)
                            {
                                return Failure("Show takes a display's name, and optionally a transition such as Fade(0.3)");
                            }
                            bool shown = arguments.size() == 2 ? displays.Show(arguments[0].AsText(), TransitionFrom(arguments[1]))
                                                               : displays.Show(arguments[0].AsText());
                            if (!shown)
                            {
                                return Failure("there is no display named " + arguments[0].AsText());
                            }
                            return ScriptValue();
                        });
                    }
                    if (name == "Back")
                    {
                        return ScriptValue::Function("Back", [displays]() { return displays.Back(); });
                    }
                    if (name == "CanGoBack")
                    {
                        return displays.CanGoBack();
                    }
                }
                if (ui::Dialog dialog = element.As<ui::Dialog>())
                {
                    if (name == "Title")
                    {
                        return dialog.Title.Get();
                    }
                    if (name == "Open")
                    {
                        return ScriptValue::Function("Open", [dialog, root]() { dialog.Open(root); });
                    }
                    if (name == "Close")
                    {
                        return ScriptValue::Function("Close", [dialog]() { dialog.Close(); });
                    }
                    if (name == "IsOpen")
                    {
                        return dialog.IsOpen();
                    }
                }
                if (ui::Menu menu = element.As<ui::Menu>())
                {
                    if (name == "Text")
                    {
                        return menu.Text.Get();
                    }
                    if (name == "Open")
                    {
                        return ScriptValue::Function("Open", [menu]() { menu.Open(); });
                    }
                    if (name == "Close")
                    {
                        return ScriptValue::Function("Close", [menu]() { menu.Close(); });
                    }
                    if (name == "IsOpen")
                    {
                        return menu.IsOpen();
                    }
                }
                return {};
            }

            bool Set(std::string_view name, const ScriptValue& value) override
            {
                ui::Element element = Target;
                if (name == "Visible")
                {
                    element.Visible = value.IsTrue();
                    return true;
                }
                if (name == "Enabled")
                {
                    element.Enabled = value.IsTrue();
                    return true;
                }
                if (name == "Opacity" && value.Kind() == ScriptValueKind::Number)
                {
                    element.Opacity = value.As<float>();
                    return true;
                }
                if (name == "Tooltip")
                {
                    element.Tooltip = value.IsNothing() ? std::string() : value.AsText();
                    return true;
                }
                if (ui::Label label = element.As<ui::Label>())
                {
                    if (name == "Text")
                    {
                        label.Text = value.AsText();
                        return true;
                    }
                    if (name == "FontSize" && value.Kind() == ScriptValueKind::Number)
                    {
                        label.FontSize = value.As<float>();
                        return true;
                    }
                }
                if (ui::Button button = element.As<ui::Button>())
                {
                    if (name == "Text")
                    {
                        button.Text = value.AsText();
                        return true;
                    }
                    if (name == "OnClick" && IsFunctionOrNothing(value))
                    {
                        button.OnClick = value.IsNothing() ? std::function<void()>() : [value] { CallForEvent(value, {}); };
                        return true;
                    }
                }
                if (ui::Checkbox check = element.As<ui::Checkbox>())
                {
                    return SetCheck(check, name, value);
                }
                if (ui::Toggle toggle = element.As<ui::Toggle>())
                {
                    return SetCheck(toggle, name, value);
                }
                if (ui::Slider slider = element.As<ui::Slider>())
                {
                    if (value.Kind() == ScriptValueKind::Number)
                    {
                        float number = value.As<float>();
                        if (name == "Value")
                        {
                            slider.Value = number;
                            return true;
                        }
                        if (name == "Minimum")
                        {
                            slider.Minimum = number;
                            return true;
                        }
                        if (name == "Maximum")
                        {
                            slider.Maximum = number;
                            return true;
                        }
                        if (name == "Step")
                        {
                            slider.Step = number;
                            return true;
                        }
                    }
                    if (name == "OnChange" && IsFunctionOrNothing(value))
                    {
                        slider.OnChange = value.IsNothing() ? std::function<void(float)>()
                                                            : [value](float changed) { CallForEvent(value, { changed }); };
                        return true;
                    }
                }
                if (ui::ProgressBar bar = element.As<ui::ProgressBar>())
                {
                    if (value.Kind() == ScriptValueKind::Number)
                    {
                        float number = value.As<float>();
                        if (name == "Value")
                        {
                            bar.Value = number;
                            return true;
                        }
                        if (name == "Minimum")
                        {
                            bar.Minimum = number;
                            return true;
                        }
                        if (name == "Maximum")
                        {
                            bar.Maximum = number;
                            return true;
                        }
                    }
                }
                if (ui::TextField field = element.As<ui::TextField>())
                {
                    if (name == "Text")
                    {
                        field.Text = value.AsText();
                        return true;
                    }
                    if (name == "Placeholder")
                    {
                        field.Placeholder = value.AsText();
                        return true;
                    }
                    if ((name == "OnChange" || name == "OnSubmit") && IsFunctionOrNothing(value))
                    {
                        auto callback = value.IsNothing() ? std::function<void(const std::string&)>()
                                                          : [value](const std::string& text) { CallForEvent(value, { text }); };
                        (name == "OnChange" ? field.OnChange : field.OnSubmit) = callback;
                        return true;
                    }
                }
                if (ui::TextArea area = element.As<ui::TextArea>())
                {
                    if (name == "Text")
                    {
                        area.Text = value.AsText();
                        return true;
                    }
                    if (name == "Placeholder")
                    {
                        area.Placeholder = value.AsText();
                        return true;
                    }
                    if (name == "OnChange" && IsFunctionOrNothing(value))
                    {
                        area.OnChange = value.IsNothing() ? std::function<void(const std::string&)>()
                                                          : [value](const std::string& text) { CallForEvent(value, { text }); };
                        return true;
                    }
                }
                if (ui::Dropdown dropdown = element.As<ui::Dropdown>())
                {
                    if (name == "Options" && value.Kind() == ScriptValueKind::List)
                    {
                        dropdown.Options = TextsOf(value);
                        return true;
                    }
                    if (name == "Selected")
                    {
                        dropdown.Selected = PositionFromScript(value);
                        return true;
                    }
                    if (name == "OnChange" && IsFunctionOrNothing(value))
                    {
                        dropdown.OnChange = value.IsNothing() ? std::function<void(int)>()
                                                              : [value](int position) { CallForEvent(value, { PositionForScript(position) }); };
                        return true;
                    }
                }
                if (ui::List list = element.As<ui::List>())
                {
                    if (name == "Items" && value.Kind() == ScriptValueKind::List)
                    {
                        list.Items = TextsOf(value);
                        return true;
                    }
                    if (name == "Selected")
                    {
                        list.Selected = PositionFromScript(value);
                        return true;
                    }
                    if ((name == "OnSelect" || name == "OnActivate") && IsFunctionOrNothing(value))
                    {
                        auto callback = value.IsNothing() ? std::function<void(int)>()
                                                          : [value](int position) { CallForEvent(value, { PositionForScript(position) }); };
                        (name == "OnSelect" ? list.OnSelect : list.OnActivate) = callback;
                        return true;
                    }
                }
                if (ui::Tabs tabs = element.As<ui::Tabs>())
                {
                    if (name == "Current")
                    {
                        tabs.Current = value.AsText();
                        return true;
                    }
                    if (name == "OnChange" && IsFunctionOrNothing(value))
                    {
                        tabs.OnChange = value.IsNothing() ? std::function<void(const std::string&)>()
                                                          : [value](const std::string& page) { CallForEvent(value, { page }); };
                        return true;
                    }
                }
                if (ui::Displays displays = element.As<ui::Displays>())
                {
                    if (name == "Current")
                    {
                        displays.Current = value.AsText();
                        return true;
                    }
                    if (name == "OnChange" && IsFunctionOrNothing(value))
                    {
                        displays.OnChange = value.IsNothing() ? std::function<void(const std::string&)>()
                                                              : [value](const std::string& shown) { CallForEvent(value, { shown }); };
                        return true;
                    }
                }
                if (ui::Dialog dialog = element.As<ui::Dialog>())
                {
                    if (name == "Title")
                    {
                        dialog.Title = value.AsText();
                        return true;
                    }
                    if (name == "OnClosed" && IsFunctionOrNothing(value))
                    {
                        dialog.OnClosed = value.IsNothing() ? std::function<void()>() : [value] { CallForEvent(value, {}); };
                        return true;
                    }
                }
                if (ui::Menu menu = element.As<ui::Menu>())
                {
                    if (name == "Text")
                    {
                        menu.Text = value.AsText();
                        return true;
                    }
                }
                return false;
            }

            ui::Element Target;
            ui::Root Owner;

        private:
            template <typename Check>
            static bool SetCheck(Check check, std::string_view name, const ScriptValue& value)
            {
                if (name == "Text")
                {
                    check.Text = value.AsText();
                    return true;
                }
                if (name == "Checked")
                {
                    check.Checked = value.IsTrue();
                    return true;
                }
                if (name == "OnChange" && IsFunctionOrNothing(value))
                {
                    check.OnChange = value.IsNothing() ? std::function<void(bool)>()
                                                       : [value](bool checked) { CallForEvent(value, { checked }); };
                    return true;
                }
                return false;
            }
        };

        class InterfaceObject final : public ScriptObject
        {
        public:
            explicit InterfaceObject(ui::Root root) : Owner(std::move(root)) {}

            std::string TypeName() const override { return "Interface"; }

            ScriptValue Get(std::string_view name) override
            {
                ui::Root root = Owner;
                if (name == "Find")
                {
                    return ScriptValue::Function("Find", [root](std::string element) -> ScriptValue {
                        ui::Element found = root.Find<ui::Element>(element);
                        return found ? ScriptValue(std::make_shared<ElementObject>(found, root)) : ScriptValue();
                    });
                }
                return {};
            }

            ui::Root Owner;
        };
    }

    namespace ui
    {
        // An element as scripts see it, to give a script one element of its own.
        inline std::shared_ptr<ScriptObject> ScriptObjectFor(const Element& element, const Root& root)
        {
            return std::make_shared<easyforge::internal::bridges::ElementObject>(element, root);
        }

        // Gives scripts `ui`, which finds elements by name, and the transitions
        // Fade(seconds), Slide(seconds, from), Scale(seconds), and Cut() for
        // Displays' Show.
        inline void DefineInterface(ScriptEngine& scripts, const Root& root, std::string_view name = "ui")
        {
            scripts.Define(name, std::make_shared<easyforge::internal::bridges::InterfaceObject>(root));
            auto transition = [](std::string kind, double seconds, std::string from) {
                return ScriptValue::Table({ { "Transition", kind }, { "Seconds", seconds }, { "From", from } });
            };
            scripts.Define("Fade", [transition](const std::vector<ScriptValue>& arguments) {
                return transition("fade", arguments.empty() ? 0.25 : arguments[0].AsNumber(), "right");
            });
            scripts.Define("Scale", [transition](const std::vector<ScriptValue>& arguments) {
                return transition("scale", arguments.empty() ? 0.25 : arguments[0].AsNumber(), "right");
            });
            scripts.Define("Slide", [transition](const std::vector<ScriptValue>& arguments) {
                return transition("slide", arguments.empty() ? 0.25 : arguments[0].AsNumber(),
                    arguments.size() > 1 ? arguments[1].AsText() : std::string("right"));
            });
            scripts.Define("Cut", [transition]() { return transition("cut", 0.0, "right"); });
        }
    }
}
