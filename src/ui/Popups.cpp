#include <easyforge/ui/Popups.h>

#include <easyforge/ui/Root.h>

#include "Containers.h"
#include "Drawing.h"
#include "Properties.h"
#include "RootState.h"
#include "Rows.h"

namespace easyforge::ui::internal
{
    namespace
    {
        struct PopupRow
        {
            std::string Text;
            std::string Shortcut;
            bool Enabled = true;
            bool Separator = false;
        };

        // The list a dropdown or a menu opens below itself.
        class PopupListBehavior final : public RowsBehavior
        {
        public:
            std::string_view Name() const override { return "Popup"; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Padding")
                {
                    return DataValue(Vector4 { 4.0f, 4.0f, 4.0f, 4.0f });
                }
                if (property == "MaximumHeight")
                {
                    return DataValue(320.0);
                }
                return Behavior::Default(property);
            }

            std::size_t RowCount(ElementState&) override { return Rows.size(); }

            float RowHeight(ElementState& element, const Context& context, std::size_t row) override
            {
                return Rows[row].Separator ? 9.0f : RowsBehavior::RowHeight(element, context, row);
            }

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint&) override
            {
                easyforge::Font font = FontOf(element, context);
                float size = FontSizeOf(element, context);
                float width = 0.0f;
                for (const PopupRow& row : Rows)
                {
                    if (row.Separator || !font)
                    {
                        continue;
                    }
                    float text = font.Measure(row.Text, size).X;
                    if (!row.Shortcut.empty())
                    {
                        text += 32.0f + font.Measure(row.Shortcut, size).X;
                    }
                    width = Max(width, text);
                }
                return { width + 24.0f, RowsHeight(element, context) };
            }

            Box LookOf(ElementState& element, const Context& context) override
            {
                Box box = Behavior::LookOf(element, context);
                box.Background = ui::Background(context.Theme->Surface);
                box.CornerRadius = context.Theme->CornerRadius;
                box.BorderWidth = 1.0f;
                box.BorderColor = context.Theme->Border;
                return box;
            }

            // The chosen row scrolls into view once the popup has a place.
            void Arrange(ElementState& element, Context& context, Rectangle) override
            {
                if (PendingReveal)
                {
                    Reveal(element, context, *PendingReveal);
                    PendingReveal.reset();
                }
            }

            bool TakesPointer(ElementState&) const override { return true; }
            bool Skipped(ElementState&, std::size_t row) override { return Rows[row].Separator || !Rows[row].Enabled; }
            std::optional<std::size_t> KeyboardRow(ElementState&) override { return Highlight; }
            void MoveTo(ElementState&, Context&, std::size_t row) override { Highlight = row; }

            void PointerMoved(ElementState& element, Context& context, const Pointer& pointer) override
            {
                RowsBehavior::PointerMoved(element, context, pointer);
                if (HoveredRow && !Skipped(element, *HoveredRow))
                {
                    Highlight = HoveredRow;
                }
            }

            void RowPressed(ElementState& element, Context& context, std::size_t row, const Pointer&) override
            {
                if (!Skipped(element, row))
                {
                    Activate(element, context, row);
                }
            }

            void Activate(ElementState&, Context&, std::size_t row) override
            {
                if (Chosen && row < Rows.size() && !Rows[row].Separator && Rows[row].Enabled)
                {
                    std::function<void(std::size_t)> chosen = Chosen;
                    chosen(row);
                }
            }

            void DrawRow(ElementState& element, DrawContext& context, std::size_t index, Rectangle area) override
            {
                const Theme& theme = *context.Theme;
                const PopupRow& row = Rows[index];
                if (row.Separator)
                {
                    context.Canvas.Rectangle({ .Position = { area.X + 6.0f, area.Center().Y - 0.5f },
                        .Size = { area.Width - 12.0f, 1.0f }, .Color = theme.Border });
                    return;
                }
                if (Highlight == index && row.Enabled)
                {
                    context.Canvas.Rectangle({ .Position = area.Position(), .Size = area.Size(), .Color = theme.ControlHovered,
                        .CornerRadius = 4.0f });
                }
                easyforge::Font font = FontOf(element, context);
                float size = FontSizeOf(element, context);
                Rectangle text { area.X + 10.0f, area.Y, area.Width - 20.0f, area.Height };
                DrawTextIn(context.Canvas, font, row.Text, size, row.Enabled ? theme.Text : theme.DisabledText, text,
                    TextAlignment::Start, true);
                if (!row.Shortcut.empty())
                {
                    DrawTextIn(context.Canvas, font, row.Shortcut, size, theme.MutedText, text, TextAlignment::End, true);
                }
            }

            std::vector<PopupRow> Rows;
            std::optional<std::size_t> Highlight;
            std::optional<std::size_t> PendingReveal;
            std::function<void(std::size_t)> Chosen;
        };

        // Opens a popup list below `owner`. `chosen` is called with the row picked,
        // after the popup has closed.
        std::shared_ptr<ElementState> OpenPopup(ElementState& owner, std::vector<PopupRow> rows,
            std::optional<std::size_t> highlight, std::function<void(std::size_t)> chosen, std::function<void()> closed)
        {
            RootState* root = owner.Root();
            if (!root)
            {
                return nullptr;
            }
            std::shared_ptr<ElementState> popup = MakeElement(std::make_unique<PopupListBehavior>());
            auto* list = static_cast<PopupListBehavior*>(popup->Kind.get());
            list->Rows = std::move(rows);
            list->Highlight = highlight;
            std::weak_ptr<ElementState> weakPopup = popup;
            list->Chosen = [weakPopup, chosen](std::size_t row) {
                std::shared_ptr<ElementState> kept = weakPopup.lock();
                if (kept && kept->Root())
                {
                    kept->Root()->CloseOverlay(kept.get());
                }
                if (chosen)
                {
                    chosen(row);
                }
            };
            popup->Set("MinimumWidth", DataValue(owner.Frame.Width));
            popup->SetObject("Effects", std::vector<Effect> { Shadow { .Offset = { 0.0f, 6.0f }, .Blur = 18.0f,
                                            .Color = Color { 0.0f, 0.0f, 0.0f, 0.28f } } });
            list->PendingReveal = highlight;
            root->OpenOverlay({ .Element = popup, .Anchor = owner.weak_from_this(), .Modal = false, .ClosesOnOutsideClick = true,
                .OnClosed = std::move(closed) });
            return popup;
        }

        // What dropdowns and menus share: a popup of their own, and keys that
        // reach it while it is open.
        class OpensPopup : public Behavior
        {
        public:
            bool TakesPointer(ElementState&) const override { return true; }
            bool TakesKeyboard(ElementState& element) const override { return element.IsEnabled(); }

            bool IsOpen(ElementState& element) const
            {
                RootState* root = element.Root();
                return Popup && root && root->IsOverlayOpen(Popup.get());
            }

            void Close(ElementState& element)
            {
                if (RootState* root = element.Root(); root && Popup)
                {
                    root->CloseOverlay(Popup.get());
                }
                Popup.reset();
            }

            virtual void Open(ElementState& element) = 0;

            void PointerReleased(ElementState& element, Context&, const Pointer&, bool inside) override
            {
                if (inside && element.IsEnabled() && !IsOpen(element))
                {
                    Open(element);
                }
            }

            void Update(ElementState& element, Context&) override
            {
                if (Popup && !IsOpen(element))
                {
                    Popup.reset();
                }
            }

            // Keys for the open popup. Returns whether they were used.
            bool KeyForPopup(ElementState& element, Context& context, const Event& event)
            {
                if (!IsOpen(element))
                {
                    return false;
                }
                auto* list = static_cast<PopupListBehavior*>(Popup->Kind.get());
                std::shared_ptr<ElementState> popup = Popup;
                if (event.Key == Key::Space)
                {
                    if (list->Highlight)
                    {
                        list->Activate(*popup, context, *list->Highlight);
                    }
                    return true;
                }
                if (event.Key == Key::Tab)
                {
                    Close(element);
                    return false;
                }
                return list->KeyPressed(*popup, context, event);
            }

            std::shared_ptr<ElementState> Popup;
        };

        void DrawArrow(const Canvas& canvas, Vector2 center, Color color)
        {
            LineStyle stroke { .Color = color, .Width = 1.5f };
            canvas.Line(center + Vector2 { -4.0f, -2.0f }, center + Vector2 { 0.0f, 2.0f }, stroke);
            canvas.Line(center + Vector2 { 0.0f, 2.0f }, center + Vector2 { 4.0f, -2.0f }, stroke);
        }

        Box ControlBox(ElementState& element, const Context& context, ButtonStyle kind)
        {
            const Theme& theme = *context.Theme;
            const Style& style = element.CurrentStyle();
            bool enabled = element.IsEnabled();
            float hover = enabled ? element.HoverAmount : 0.0f;
            float press = enabled ? element.PressAmount : 0.0f;
            Box box;
            box.CornerRadius = style.CornerRadius.value_or(theme.CornerRadius);
            if (style.Background)
            {
                box.Background = style.Background;
            }
            else
            {
                Color base = kind == ButtonStyle::Subtle ? theme.ControlHovered.WithAlpha(0.0f) : theme.Control;
                box.Background = ui::Background(Mix(Mix(base, theme.ControlHovered, hover), theme.ControlPressed, press));
            }
            if (style.BorderWidth > 0.0f)
            {
                box.BorderWidth = style.BorderWidth;
                box.BorderColor = style.BorderColor.value_or(theme.Border);
            }
            else if (kind == ButtonStyle::Normal && !style.Background)
            {
                box.BorderWidth = theme.BorderWidth;
                box.BorderColor = element.Focused ? theme.Accent : style.BorderColor.value_or(theme.Border);
            }
            return box;
        }

        Rectangle ContentOf(ElementState& element)
        {
            const Style& style = element.CurrentStyle();
            return { element.Frame.X + style.Padding.Left, element.Frame.Y + style.Padding.Top,
                element.Frame.Width - style.Padding.Horizontal(), element.Frame.Height - style.Padding.Vertical() };
        }

        // ---- Dropdown -------------------------------------------------------------

        class DropdownBehavior final : public OpensPopup
        {
        public:
            std::string_view Name() const override { return Dropdown::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Width")
                {
                    return DataValue(200.0);
                }
                if (property == "Padding")
                {
                    return DataValue(Vector4 { 10.0f, 6.0f, 30.0f, 6.0f });
                }
                if (property == "Selected")
                {
                    return DataValue(-1);
                }
                if (property == "Placeholder")
                {
                    return DataValue("");
                }
                return Behavior::Default(property);
            }

            static std::vector<std::string> Options(ElementState& element)
            {
                return element.Object<std::vector<std::string>>("Options");
            }

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint&) override
            {
                easyforge::Font font = FontOf(element, context);
                return { 0.0f, font ? font.LineHeight(FontSizeOf(element, context)) : 0.0f };
            }

            Box LookOf(ElementState& element, const Context& context) override
            {
                return ControlBox(element, context, ButtonStyle::Normal);
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                const Theme& theme = *context.Theme;
                std::vector<std::string> options = Options(element);
                int selected = element.Get("Selected").As<int>();
                bool chosen = selected >= 0 && static_cast<std::size_t>(selected) < options.size();
                std::string text = chosen ? options[static_cast<std::size_t>(selected)] : element.Get("Placeholder").AsText();
                std::optional<Color> color = FromData<std::optional<Color>>(element.Get("Color"));
                Color shown = !element.IsEnabled() ? theme.DisabledText : chosen ? color.value_or(theme.Text) : theme.MutedText;
                DrawTextIn(context.Canvas, FontOf(element, context), text, FontSizeOf(element, context), shown, ContentOf(element),
                    TextAlignment::Start, true);
                DrawArrow(context.Canvas, { element.Frame.Right() - 16.0f, element.Frame.Center().Y },
                    element.IsEnabled() ? theme.MutedText : theme.DisabledText);
            }

            void Open(ElementState& element) override
            {
                std::vector<PopupRow> rows;
                for (const std::string& option : Options(element))
                {
                    rows.push_back({ option });
                }
                int selected = element.Get("Selected").As<int>();
                std::optional<std::size_t> highlight;
                if (selected >= 0 && static_cast<std::size_t>(selected) < rows.size())
                {
                    highlight = static_cast<std::size_t>(selected);
                }
                std::weak_ptr<ElementState> weak = element.weak_from_this();
                Popup = OpenPopup(element, std::move(rows), highlight,
                    [weak](std::size_t row) {
                        if (std::shared_ptr<ElementState> kept = weak.lock())
                        {
                            Choose(*kept, static_cast<int>(row));
                        }
                    },
                    nullptr);
            }

            bool KeyPressed(ElementState& element, Context& context, const Event& event) override
            {
                if (IsOpen(element))
                {
                    return KeyForPopup(element, context, event);
                }
                int count = static_cast<int>(Options(element).size());
                int selected = element.Get("Selected").As<int>();
                switch (event.Key)
                {
                case Key::Enter:
                case Key::NumberPadEnter:
                case Key::Space: Open(element); return true;
                case Key::Down:
                    if (event.Modifiers.Alt)
                    {
                        Open(element);
                    }
                    else if (count > 0)
                    {
                        Choose(element, Min(selected + 1, count - 1));
                    }
                    return true;
                case Key::Up:
                    if (count > 0)
                    {
                        Choose(element, Max(selected - 1, 0));
                    }
                    return true;
                default: return false;
                }
            }

            static void Choose(ElementState& element, int row)
            {
                if (element.Get("Selected").As<int>() == row)
                {
                    return;
                }
                std::shared_ptr<ElementState> kept = element.shared_from_this();
                element.Set("Selected", DataValue(row));
                std::function<void(int)> changed = element.Object<std::function<void(int)>>("OnChange");
                if (changed)
                {
                    changed(row);
                }
            }
        };

        // ---- Menu -----------------------------------------------------------------

        class MenuBehavior final : public OpensPopup
        {
        public:
            std::string_view Name() const override { return Menu::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Padding")
                {
                    return DataValue(Vector4 { 10.0f, 6.0f, 10.0f, 6.0f });
                }
                if (property == "Style")
                {
                    return DataValue("subtle");
                }
                return Behavior::Default(property);
            }

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint&) override
            {
                easyforge::Font font = FontOf(element, context);
                if (!font)
                {
                    return {};
                }
                float size = FontSizeOf(element, context);
                Vector2 measured = font.Measure(element.Get("Text").AsText(), size);
                return { measured.X, Max(measured.Y, font.LineHeight(size)) };
            }

            Box LookOf(ElementState& element, const Context& context) override
            {
                ButtonStyle kind = FromData<ButtonStyle>(element.Get("Style"));
                Box box = ControlBox(element, context, kind);
                if (IsOpen(element) && !element.CurrentStyle().Background)
                {
                    box.Background = ui::Background(context.Theme->ControlPressed);
                }
                return box;
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                std::optional<Color> color = FromData<std::optional<Color>>(element.Get("Color"));
                Color shown = element.IsEnabled() ? color.value_or(context.Theme->Text) : context.Theme->DisabledText;
                DrawTextIn(context.Canvas, FontOf(element, context), element.Get("Text").AsText(), FontSizeOf(element, context),
                    shown, ContentOf(element), TextAlignment::Center, true);
            }

            void Open(ElementState& element) override
            {
                std::vector<MenuItem> items = element.Object<std::vector<MenuItem>>("Items");
                std::vector<PopupRow> rows;
                std::optional<std::size_t> first;
                for (const MenuItem& item : items)
                {
                    rows.push_back({ item.Text, item.Shortcut, item.Enabled, item.Separator });
                }
                Popup = OpenPopup(element, std::move(rows), first,
                    [items](std::size_t row) {
                        if (row < items.size() && items[row].OnClick)
                        {
                            items[row].OnClick();
                        }
                    },
                    nullptr);
            }

            bool KeyPressed(ElementState& element, Context& context, const Event& event) override
            {
                if (IsOpen(element))
                {
                    return KeyForPopup(element, context, event);
                }
                if (event.Key == Key::Enter || event.Key == Key::NumberPadEnter || event.Key == Key::Space ||
                    event.Key == Key::Down)
                {
                    Open(element);
                    if (event.Key == Key::Down && Popup)
                    {
                        auto* list = static_cast<PopupListBehavior*>(Popup->Kind.get());
                        for (std::size_t row = 0; row < list->Rows.size(); ++row)
                        {
                            if (!list->Skipped(*Popup, row))
                            {
                                list->Highlight = row;
                                break;
                            }
                        }
                    }
                    return true;
                }
                return false;
            }
        };

        // ---- Dialog ---------------------------------------------------------------

        class DialogBehavior final : public LineBehavior
        {
        public:
            DialogBehavior() : LineBehavior(Dialog::KindName, false) {}

            DataValue Default(std::string_view property) const override
            {
                if (property == "Width")
                {
                    return DataValue(440.0);
                }
                if (property == "Padding")
                {
                    return DataValue(Vector4 { 24.0f, 24.0f, 24.0f, 24.0f });
                }
                if (property == "Gap")
                {
                    return DataValue(16.0);
                }
                if (property == "ClosesOnOutsideClick")
                {
                    return DataValue(false);
                }
                if (property == "Title")
                {
                    return DataValue("");
                }
                return LineBehavior::Default(property);
            }

            Box LookOf(ElementState& element, const Context& context) override
            {
                Box box = Behavior::LookOf(element, context);
                const Style& style = element.CurrentStyle();
                if (!style.Background)
                {
                    box.Background = ui::Background(context.Theme->Surface);
                }
                if (!style.CornerRadius)
                {
                    box.CornerRadius = context.Theme->CornerRadius * 1.5f;
                }
                return box;
            }
        };

        template <typename Behavior>
        Behavior* BehaviorOf(const std::shared_ptr<ElementState>& state)
        {
            return state ? dynamic_cast<Behavior*>(state->Kind.get()) : nullptr;
        }

        // The label that shows a dialog's title, made when the dialog first has one.
        void SetTitle(ElementState& dialog, const std::string& title)
        {
            dialog.Set("Title", DataValue(title));
            std::shared_ptr<ElementState> label = dialog.Object<std::shared_ptr<ElementState>>("TitleLabel");
            if (!label)
            {
                if (title.empty())
                {
                    return;
                }
                label = StateOfHandle(Label(title, { .FontSize = 21.0f }));
                dialog.SetObject("TitleLabel", label);
                AddChild(dialog, label, 0);
                return;
            }
            label->Set("Text", DataValue(title));
            label->Set("Visible", DataValue(!title.empty()));
        }
    }
}

namespace easyforge::ui
{
    using namespace internal;

    // ---- Dropdown --------------------------------------------------------------------

    Dropdown::Dropdown(const DropdownSettings& settings) : Dropdown(MakeElement(std::make_unique<DropdownBehavior>()))
    {
        const DropdownSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "Placeholder", settings.Placeholder, defaults.Placeholder);
        Choose(*State, "FontSize", settings.FontSize, defaults.FontSize);
        Choose(*State, "Color", settings.Color, defaults.Color);
        State->SetObject("Options", settings.Options);
        Choose(*State, "Selected", settings.Selected, defaults.Selected);
        if (settings.OnChange)
        {
            State->SetObject("OnChange", settings.OnChange);
        }
    }

    Dropdown::Dropdown(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Options(Kept<std::vector<std::string>, "Options", true>(State)),
          Selected(
              &State,
              [](const void* owner) -> int {
                  ElementState* element = StateOf(owner);
                  return element ? element->Get("Selected").As<int>() : -1;
              },
              [](void* owner, const int& row) {
                  if (ElementState* element = StateOf(owner))
                  {
                      DropdownBehavior::Choose(*element, row);
                  }
              }),
          Placeholder(Stored<std::string, "Placeholder">(State)), FontSize(Stored<float, "FontSize">(State)),
          Color(Stored<std::optional<easyforge::Color>, "Color">(State)),
          OnChange(Kept<std::function<void(int)>, "OnChange">(State))
    {
    }

    Dropdown::Dropdown(const Dropdown& other) : Dropdown(other.State)
    {
    }

    Dropdown& Dropdown::operator=(const Dropdown& other)
    {
        State = other.State;
        return *this;
    }

    std::string Dropdown::SelectedText() const
    {
        if (!State)
        {
            return {};
        }
        std::vector<std::string> options = State->Object<std::vector<std::string>>("Options");
        int selected = State->Get("Selected").As<int>();
        return selected >= 0 && static_cast<std::size_t>(selected) < options.size()
                   ? options[static_cast<std::size_t>(selected)]
                   : std::string();
    }

    // ---- Menu ------------------------------------------------------------------------

    Menu::Menu(std::string text, const MenuSettings& settings) : Menu(MakeElement(std::make_unique<MenuBehavior>()))
    {
        const MenuSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "Style", settings.Style, defaults.Style);
        Choose(*State, "FontSize", settings.FontSize, defaults.FontSize);
        Choose(*State, "Color", settings.Color, defaults.Color);
        if (!text.empty())
        {
            State->Set("Text", DataValue(std::move(text)));
        }
        State->SetObject("Items", settings.Items);
    }

    Menu::Menu(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Text(Stored<std::string, "Text">(State)),
          Items(Kept<std::vector<MenuItem>, "Items">(State)), Style(Stored<ButtonStyle, "Style">(State))
    {
    }

    Menu::Menu(const Menu& other) : Menu(other.State)
    {
    }

    Menu& Menu::operator=(const Menu& other)
    {
        State = other.State;
        return *this;
    }

    void Menu::Open() const
    {
        if (MenuBehavior* menu = BehaviorOf<MenuBehavior>(State); menu && !menu->IsOpen(*State))
        {
            menu->Open(*State);
        }
    }

    void Menu::Close() const
    {
        if (MenuBehavior* menu = BehaviorOf<MenuBehavior>(State))
        {
            menu->Close(*State);
        }
    }

    bool Menu::IsOpen() const
    {
        MenuBehavior* menu = BehaviorOf<MenuBehavior>(State);
        return menu && menu->IsOpen(*State);
    }

    // ---- Dialog ----------------------------------------------------------------------

    Dialog::Dialog(const DialogSettings& settings) : Dialog(MakeElement(std::make_unique<DialogBehavior>()))
    {
        const DialogSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "Gap", settings.Gap, defaults.Gap);
        Choose(*State, "ClosesOnOutsideClick", settings.ClosesOnOutsideClick, defaults.ClosesOnOutsideClick);
        if (settings.Effects.empty())
        {
            State->SetObject("Effects", std::vector<Effect> { Shadow { .Offset = { 0.0f, 16.0f }, .Blur = 40.0f,
                                            .Color = easyforge::Color { 0.0f, 0.0f, 0.0f, 0.45f } } });
        }
        if (settings.OnClosed)
        {
            State->SetObject("OnClosed", settings.OnClosed);
        }
        SetTitle(*State, settings.Title);
        AddChildren(*State, settings.Children);
        if (!settings.Buttons.empty())
        {
            Row buttons({ .Margin = { 0.0f, 8.0f, 0.0f, 0.0f }, .Gap = 8, .Distribution = ui::Distribution::End,
                .Children = settings.Buttons });
            AddChild(*State, StateOfHandle(buttons));
        }
    }

    Dialog::Dialog(std::shared_ptr<internal::ElementState> state)
        : Container(std::move(state)),
          Title(
              &State,
              [](const void* owner) -> std::string {
                  ElementState* element = StateOf(owner);
                  return element ? element->Get("Title").AsText() : std::string();
              },
              [](void* owner, const std::string& title) {
                  if (ElementState* element = StateOf(owner))
                  {
                      SetTitle(*element, title);
                  }
              }),
          ClosesOnOutsideClick(Stored<bool, "ClosesOnOutsideClick">(State)),
          OnClosed(Kept<std::function<void()>, "OnClosed">(State))
    {
    }

    Dialog::Dialog(const Dialog& other) : Dialog(other.State)
    {
    }

    Dialog& Dialog::operator=(const Dialog& other)
    {
        State = other.State;
        return *this;
    }

    void Dialog::Open(const ui::Root& root) const
    {
        const std::shared_ptr<RootState>& state = StateOfRoot(root);
        if (!State || !state)
        {
            return;
        }
        std::weak_ptr<ElementState> weak = State;
        state->OpenOverlay({ .Element = State, .Modal = true,
            .ClosesOnOutsideClick = State->Get("ClosesOnOutsideClick").AsBoolean(),
            .OnClosed = [weak] {
                if (std::shared_ptr<ElementState> dialog = weak.lock())
                {
                    std::function<void()> closed = dialog->Object<std::function<void()>>("OnClosed");
                    if (closed)
                    {
                        closed();
                    }
                }
            } });
    }

    void Dialog::Close() const
    {
        if (State)
        {
            if (RootState* root = State->Root())
            {
                root->CloseOverlay(State.get());
            }
        }
    }

    bool Dialog::IsOpen() const
    {
        RootState* root = State ? State->Root() : nullptr;
        return root && root->IsOverlayOpen(State.get());
    }
}
