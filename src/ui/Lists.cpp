#include <easyforge/ui/Lists.h>

#include <unordered_set>

#include "Drawing.h"
#include "Properties.h"
#include "RootState.h"
#include "Rows.h"

namespace easyforge::ui::internal
{
    namespace
    {
        // The box both lists and trees have: a field's colors and border.
        Box FieldBox(ElementState& element, const Context& context)
        {
            const Theme& theme = *context.Theme;
            const Style& style = element.CurrentStyle();
            Box box;
            box.CornerRadius = style.CornerRadius.value_or(theme.CornerRadius);
            box.Background = style.Background ? style.Background : std::optional<ui::Background>(ui::Background(theme.Control));
            std::optional<float> width = FromData<std::optional<float>>(element.Get("BorderWidth"));
            box.BorderWidth = width.value_or(theme.BorderWidth);
            box.BorderColor = element.Focused ? theme.Accent : style.BorderColor.value_or(theme.Border);
            return box;
        }

        DataValue FieldDefault(std::string_view property, float width, float height)
        {
            if (property == "Width")
            {
                return DataValue(static_cast<double>(width));
            }
            if (property == "Height")
            {
                return DataValue(static_cast<double>(height));
            }
            if (property == "Padding")
            {
                return DataValue(Vector4 { 4.0f, 4.0f, 4.0f, 4.0f });
            }
            if (property == "BorderWidth")
            {
                return DataValue();
            }
            return DataValue();
        }

        // The background of a row: chosen, or under the pointer.
        void DrawRowBackground(DrawContext& context, Rectangle area, bool chosen, bool hovered, bool focused)
        {
            const Theme& theme = *context.Theme;
            if (chosen)
            {
                context.Canvas.Rectangle({ .Position = area.Position(), .Size = area.Size(),
                    .Color = focused ? theme.Selection : theme.ControlHovered, .CornerRadius = 4.0f });
            }
            else if (hovered)
            {
                context.Canvas.Rectangle({ .Position = area.Position(), .Size = area.Size(), .Color = theme.ControlHovered,
                    .CornerRadius = 4.0f });
            }
        }

        // ---- List ----------------------------------------------------------------

        class ListBehavior final : public RowsBehavior
        {
        public:
            std::string_view Name() const override { return List::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Selected")
                {
                    return DataValue(-1);
                }
                DataValue value = FieldDefault(property, 240.0f, 200.0f);
                return value.IsNothing() && property != "BorderWidth" ? Behavior::Default(property) : value;
            }

            Box LookOf(ElementState& element, const Context& context) override { return FieldBox(element, context); }
            bool TakesPointer(ElementState&) const override { return true; }
            bool TakesKeyboard(ElementState& element) const override { return element.IsEnabled(); }
            bool ShowsFocusRing() const override { return false; }

            std::vector<std::string> Items(ElementState& element) const
            {
                return element.Object<std::vector<std::string>>("Items");
            }

            std::size_t RowCount(ElementState& element) override { return Items(element).size(); }

            void DrawRow(ElementState& element, DrawContext& context, std::size_t row, Rectangle area) override
            {
                std::vector<std::string> items = Items(element);
                int selected = element.Get("Selected").As<int>();
                bool chosen = selected >= 0 && static_cast<std::size_t>(selected) == row;
                DrawRowBackground(context, area, chosen, HoveredRow == row, element.Focused);
                std::optional<Color> color = FromData<std::optional<Color>>(element.Get("Color"));
                Color shown = element.IsEnabled() ? color.value_or(context.Theme->Text) : context.Theme->DisabledText;
                DrawTextIn(context.Canvas, FontOf(element, context), items[row], FontSizeOf(element, context), shown,
                    { area.X + 8.0f, area.Y, area.Width - 16.0f, area.Height }, TextAlignment::Start, true);
            }

            std::optional<std::size_t> KeyboardRow(ElementState& element) override
            {
                int selected = element.Get("Selected").As<int>();
                if (selected < 0 || static_cast<std::size_t>(selected) >= RowCount(element))
                {
                    return std::nullopt;
                }
                return static_cast<std::size_t>(selected);
            }

            void MoveTo(ElementState& element, Context&, std::size_t row) override { Choose(element, static_cast<int>(row)); }

            void RowPressed(ElementState& element, Context& context, std::size_t row, const Pointer& pointer) override
            {
                Choose(element, static_cast<int>(row));
                if (pointer.ClickCount == 2)
                {
                    Activate(element, context, row);
                }
            }

            void Activate(ElementState& element, Context&, std::size_t row) override
            {
                std::shared_ptr<ElementState> kept = element.shared_from_this();
                std::function<void(int)> activate = element.Object<std::function<void(int)>>("OnActivate");
                if (activate)
                {
                    activate(static_cast<int>(row));
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
                std::function<void(int)> select = element.Object<std::function<void(int)>>("OnSelect");
                if (select)
                {
                    select(row);
                }
            }
        };

        // ---- Tree ----------------------------------------------------------------

        constexpr float Indent = 16.0f;

        class TreeBehavior final : public RowsBehavior
        {
        public:
            struct Line
            {
                Node Target;
                int Depth = 0;
                bool HasChildren = false;
            };

            std::string_view Name() const override { return ui::Tree::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "OpenAtStart")
                {
                    return DataValue(true);
                }
                DataValue value = FieldDefault(property, 240.0f, 300.0f);
                return value.IsNothing() && property != "BorderWidth" ? Behavior::Default(property) : value;
            }

            Box LookOf(ElementState& element, const Context& context) override { return FieldBox(element, context); }
            bool TakesPointer(ElementState&) const override { return true; }
            bool TakesKeyboard(ElementState& element) const override { return element.IsEnabled(); }
            bool ShowsFocusRing() const override { return false; }

            void Rebuild(ElementState& element)
            {
                Table table = element.Object<Table>("Source");
                if (!(table == Watched))
                {
                    Watched = table;
                    Started = false;
                    Open.clear();
                }
                if (!Started && table)
                {
                    Started = true;
                    if (element.Get("OpenAtStart").AsBoolean())
                    {
                        for (const Node& node : table.TopNodes())
                        {
                            Open.insert(node.Identifier());
                        }
                    }
                }
                Lines.clear();
                if (table)
                {
                    for (const Node& node : table.TopNodes())
                    {
                        Collect(node, 0);
                    }
                    SeenVersion = table.Version();
                }
                Built = true;
            }

            void Collect(const Node& node, int depth)
            {
                std::size_t children = node.ChildCount();
                Lines.push_back({ node, depth, children > 0 });
                if (children > 0 && Open.contains(node.Identifier()))
                {
                    for (const Node& child : node.Children())
                    {
                        Collect(child, depth + 1);
                    }
                }
            }

            void Refresh(ElementState& element)
            {
                Table table = element.Object<Table>("Source");
                if (!Built || !(table == Watched) || (table && table.Version() != SeenVersion))
                {
                    Rebuild(element);
                }
            }

            std::size_t RowCount(ElementState& element) override
            {
                Refresh(element);
                return Lines.size();
            }

            void Update(ElementState& element, Context& context) override
            {
                Refresh(element);
                RowsBehavior::Update(element, context);
            }

            void DrawRow(ElementState& element, DrawContext& context, std::size_t row, Rectangle area) override
            {
                const Line& line = Lines[row];
                Node selected = element.Object<Node>("Selected");
                DrawRowBackground(context, area, selected && selected == line.Target, HoveredRow == row, element.Focused);
                std::optional<Color> color = FromData<std::optional<Color>>(element.Get("Color"));
                Color shown = element.IsEnabled() ? color.value_or(context.Theme->Text) : context.Theme->DisabledText;
                float left = area.X + 4.0f + Indent * static_cast<float>(line.Depth);
                if (line.HasChildren)
                {
                    // An arrow: pointing right when closed, down when open.
                    Vector2 center { left + 6.0f, area.Center().Y };
                    LineStyle stroke { .Color = context.Theme->MutedText, .Width = 1.5f };
                    if (Open.contains(line.Target.Identifier()))
                    {
                        context.Canvas.Line(center + Vector2 { -4.0f, -2.0f }, center + Vector2 { 0.0f, 2.0f }, stroke);
                        context.Canvas.Line(center + Vector2 { 0.0f, 2.0f }, center + Vector2 { 4.0f, -2.0f }, stroke);
                    }
                    else
                    {
                        context.Canvas.Line(center + Vector2 { -2.0f, -4.0f }, center + Vector2 { 2.0f, 0.0f }, stroke);
                        context.Canvas.Line(center + Vector2 { 2.0f, 0.0f }, center + Vector2 { -2.0f, 4.0f }, stroke);
                    }
                }
                left += 16.0f;
                DrawTextIn(context.Canvas, FontOf(element, context), line.Target.Name(), FontSizeOf(element, context), shown,
                    { left, area.Y, area.Right() - left - 4.0f, area.Height }, TextAlignment::Start, true);
            }

            std::optional<std::size_t> KeyboardRow(ElementState& element) override
            {
                Refresh(element);
                Node selected = element.Object<Node>("Selected");
                for (std::size_t row = 0; row < Lines.size(); ++row)
                {
                    if (selected && Lines[row].Target == selected)
                    {
                        return row;
                    }
                }
                return std::nullopt;
            }

            void MoveTo(ElementState& element, Context&, std::size_t row) override { Choose(element, Lines[row].Target); }

            void Toggle(ElementState& element, std::size_t row)
            {
                std::uint64_t identifier = Lines[row].Target.Identifier();
                if (Open.contains(identifier))
                {
                    Open.erase(identifier);
                }
                else
                {
                    Open.insert(identifier);
                }
                Rebuild(element);
            }

            void RowPressed(ElementState& element, Context& context, std::size_t row, const Pointer& pointer) override
            {
                const Line& line = Lines[row];
                float arrow = RowArea(element).X + 4.0f + Indent * static_cast<float>(line.Depth);
                if (line.HasChildren && pointer.Position.X >= arrow && pointer.Position.X < arrow + 16.0f)
                {
                    Toggle(element, row);
                    return;
                }
                Choose(element, line.Target);
                if (pointer.ClickCount == 2)
                {
                    if (line.HasChildren)
                    {
                        Toggle(element, row);
                    }
                    Activate(element, context, row);
                }
            }

            void Activate(ElementState& element, Context&, std::size_t row) override
            {
                if (row >= Lines.size())
                {
                    return;
                }
                std::shared_ptr<ElementState> kept = element.shared_from_this();
                std::function<void(Node)> activate = element.Object<std::function<void(Node)>>("OnActivate");
                if (activate)
                {
                    activate(Lines[row].Target);
                }
            }

            bool KeyPressed(ElementState& element, Context& context, const Event& event) override
            {
                std::optional<std::size_t> current = KeyboardRow(element);
                if (current && (event.Key == Key::Left || event.Key == Key::Right))
                {
                    const Line& line = Lines[*current];
                    bool open = Open.contains(line.Target.Identifier());
                    if (event.Key == Key::Right && line.HasChildren && !open)
                    {
                        Toggle(element, *current);
                    }
                    else if (event.Key == Key::Right && line.HasChildren && *current + 1 < Lines.size())
                    {
                        MoveTo(element, context, *current + 1);
                    }
                    else if (event.Key == Key::Left && line.HasChildren && open)
                    {
                        Toggle(element, *current);
                    }
                    else if (event.Key == Key::Left && line.Target.Parent())
                    {
                        Choose(element, line.Target.Parent());
                    }
                    if (std::optional<std::size_t> moved = KeyboardRow(element))
                    {
                        Reveal(element, context, *moved);
                    }
                    return true;
                }
                return RowsBehavior::KeyPressed(element, context, event);
            }

            static void Choose(ElementState& element, const Node& node)
            {
                Node selected = element.Object<Node>("Selected");
                if (selected == node)
                {
                    return;
                }
                std::shared_ptr<ElementState> kept = element.shared_from_this();
                element.SetObject("Selected", node);
                std::function<void(Node)> select = element.Object<std::function<void(Node)>>("OnSelect");
                if (select)
                {
                    select(node);
                }
            }

            std::vector<Line> Lines;
            std::unordered_set<std::uint64_t> Open;
            Table Watched;
            std::uint64_t SeenVersion = 0;
            bool Started = false;
            bool Built = false;
        };

        template <typename Behavior>
        Behavior* BehaviorOf(const std::shared_ptr<ElementState>& state)
        {
            return state ? dynamic_cast<Behavior*>(state->Kind.get()) : nullptr;
        }
    }
}

namespace easyforge::ui
{
    using namespace internal;

    // ---- List ----------------------------------------------------------------------

    List::List(const ListSettings& settings) : List(MakeElement(std::make_unique<ListBehavior>()))
    {
        const ListSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "FontSize", settings.FontSize, defaults.FontSize);
        Choose(*State, "Color", settings.Color, defaults.Color);
        State->SetObject("Items", settings.Items);
        Choose(*State, "Selected", settings.Selected, defaults.Selected);
        if (settings.OnSelect)
        {
            State->SetObject("OnSelect", settings.OnSelect);
        }
        if (settings.OnActivate)
        {
            State->SetObject("OnActivate", settings.OnActivate);
        }
    }

    List::List(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Items(Kept<std::vector<std::string>, "Items">(State)),
          Selected(
              &State,
              [](const void* owner) -> int {
                  ElementState* element = StateOf(owner);
                  return element ? element->Get("Selected").As<int>() : -1;
              },
              [](void* owner, const int& row) {
                  if (ElementState* element = StateOf(owner))
                  {
                      ListBehavior::Choose(*element, row);
                  }
              }),
          FontSize(Stored<float, "FontSize">(State)), Color(Stored<std::optional<easyforge::Color>, "Color">(State)),
          OnSelect(Kept<std::function<void(int)>, "OnSelect">(State)),
          OnActivate(Kept<std::function<void(int)>, "OnActivate">(State))
    {
    }

    List::List(const List& other) : List(other.State)
    {
    }

    List& List::operator=(const List& other)
    {
        State = other.State;
        return *this;
    }

    std::string List::SelectedText() const
    {
        if (!State)
        {
            return {};
        }
        std::vector<std::string> items = State->Object<std::vector<std::string>>("Items");
        int selected = State->Get("Selected").As<int>();
        return selected >= 0 && static_cast<std::size_t>(selected) < items.size() ? items[static_cast<std::size_t>(selected)]
                                                                                   : std::string();
    }

    // ---- Tree ----------------------------------------------------------------------

    Tree::Tree(const TreeSettings& settings) : Tree(MakeElement(std::make_unique<TreeBehavior>()))
    {
        const TreeSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "OpenAtStart", settings.OpenAtStart, defaults.OpenAtStart);
        Choose(*State, "FontSize", settings.FontSize, defaults.FontSize);
        Choose(*State, "Color", settings.Color, defaults.Color);
        State->SetObject("Source", settings.Source);
        if (settings.OnSelect)
        {
            State->SetObject("OnSelect", settings.OnSelect);
        }
        if (settings.OnActivate)
        {
            State->SetObject("OnActivate", settings.OnActivate);
        }
    }

    Tree::Tree(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Source(Kept<Table, "Source">(State)),
          Selected(
              &State,
              [](const void* owner) -> Node {
                  ElementState* element = StateOf(owner);
                  return element ? element->Object<Node>("Selected") : Node();
              },
              [](void* owner, const Node& node) {
                  if (ElementState* element = StateOf(owner))
                  {
                      TreeBehavior::Choose(*element, node);
                  }
              }),
          FontSize(Stored<float, "FontSize">(State)), Color(Stored<std::optional<easyforge::Color>, "Color">(State)),
          OnSelect(Kept<std::function<void(Node)>, "OnSelect">(State)),
          OnActivate(Kept<std::function<void(Node)>, "OnActivate">(State))
    {
    }

    Tree::Tree(const Tree& other) : Tree(other.State)
    {
    }

    Tree& Tree::operator=(const Tree& other)
    {
        State = other.State;
        return *this;
    }

    void Tree::OpenNode(const Node& node) const
    {
        if (TreeBehavior* tree = BehaviorOf<TreeBehavior>(State))
        {
            // Opening a node shows it too: every node above it opens as well.
            for (Node above = node; above; above = above.Parent())
            {
                tree->Open.insert(above.Identifier());
            }
            tree->Rebuild(*State);
        }
    }

    void Tree::CloseNode(const Node& node) const
    {
        if (TreeBehavior* tree = BehaviorOf<TreeBehavior>(State))
        {
            tree->Open.erase(node.Identifier());
            tree->Rebuild(*State);
        }
    }

    bool Tree::IsOpen(const Node& node) const
    {
        TreeBehavior* tree = BehaviorOf<TreeBehavior>(State);
        return tree && tree->Open.contains(node.Identifier());
    }
}
