#include "ElementState.h"

#include <algorithm>
#include <cmath>

#include <easyforge/core/Log.h>

#include "Behavior.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui
{
    float Ease(Easing easing, float progress)
    {
        float amount = Clamp(progress, 0.0f, 1.0f);
        switch (easing)
        {
        case Easing::Linear: return amount;
        case Easing::In: return amount * amount * amount;
        case Easing::Out:
        {
            float left = 1.0f - amount;
            return 1.0f - left * left * left;
        }
        case Easing::InOut:
        {
            if (amount < 0.5f)
            {
                return 4.0f * amount * amount * amount;
            }
            float left = -2.0f * amount + 2.0f;
            return 1.0f - left * left * left * 0.5f;
        }
        case Easing::Spring:
            if (amount >= 1.0f)
            {
                return 1.0f;
            }
            return 1.0f - std::exp(-6.0f * amount) * std::cos(12.0f * amount);
        }
        return amount;
    }
}

namespace easyforge::ui::internal
{
    ElementTree::ElementTree() : Elements(Table::New({ .ChangeLimit = 20000, .UndoLimit = 0 }))
    {
    }

    ElementState::ElementState(std::unique_ptr<Behavior> kind) : Kind(std::move(kind))
    {
    }

    ElementState::~ElementState()
    {
        // Children someone else still holds leave with their own table, so they
        // keep their settings. The rest go first, while their nodes still exist,
        // so they can do the same for their own children.
        for (std::shared_ptr<ElementState>& child : Children)
        {
            child->Parent = nullptr;
            if (child.use_count() > 1)
            {
                MoveToTree(*child, std::make_shared<ElementTree>(), Node());
            }
        }
        Children.clear();
        if (Owner)
        {
            Owner->ByIdentifier.erase(Row.Identifier());
        }
        Row.Remove();
    }

    DataValue ElementState::Get(std::string_view name) const
    {
        DataValue value = Row.Get(name);
        return value.IsNothing() ? Kind->Default(name) : value;
    }

    void ElementState::Set(std::string_view name, const DataValue& value)
    {
        Row.Set(name, value);
        StyleStale = true;
    }

    void ElementState::SetObject(std::string_view name, std::any value)
    {
        auto found = Objects.find(name);
        if (found == Objects.end())
        {
            Objects.emplace(std::string(name), std::move(value));
        }
        else
        {
            found->second = std::move(value);
        }
        StyleStale = true;
    }

    void ElementState::ClearObject(std::string_view name)
    {
        auto found = Objects.find(name);
        if (found != Objects.end())
        {
            Objects.erase(found);
        }
        StyleStale = true;
    }

    void ElementState::Changed(bool affectsLayout)
    {
        StyleStale = true;
        if (RootState* root = Root())
        {
            root->Invalidate(affectsLayout);
        }
    }

    bool ElementState::IsEnabled() const
    {
        for (const ElementState* element = this; element; element = element->Parent)
        {
            if (!element->Get("Enabled").AsBoolean())
            {
                return false;
            }
        }
        return true;
    }

    const Style& ElementState::CurrentStyle()
    {
        if (!StyleStale)
        {
            return CachedStyle;
        }
        Style& style = CachedStyle;
        style.Width = FromData<Size>(Get("Width"));
        style.Height = FromData<Size>(Get("Height"));
        style.MinimumWidth = Get("MinimumWidth").As<float>();
        style.MinimumHeight = Get("MinimumHeight").As<float>();
        style.MaximumWidth = Get("MaximumWidth").As<float>();
        style.MaximumHeight = Get("MaximumHeight").As<float>();
        style.Margin = FromData<Insets>(Get("Margin"));
        style.Padding = FromData<Insets>(Get("Padding"));
        style.CornerRadius = FromData<std::optional<float>>(Get("CornerRadius"));
        style.Background = Object<std::optional<ui::Background>>("Background");
        style.BorderWidth = Get("BorderWidth").As<float>();
        style.BorderColor = FromData<std::optional<Color>>(Get("BorderColor"));
        style.Opacity = Get("Opacity").As<float>();
        style.Offset = Get("Offset").AsVector2();
        style.Scale = Get("Scale").As<float>();
        style.Effects = Object<std::vector<Effect>>("Effects");
        style.Shader = Object<easyforge::Shader>("Shader");
        style.ShaderValues = Object<std::vector<ShaderValue>>("ShaderValues");
        style.Visible = Get("Visible").AsBoolean();
        style.Enabled = Get("Enabled").AsBoolean();
        style.Tooltip = Get("Tooltip").AsText();
        style.Cursor = FromData<std::optional<easyforge::Cursor>>(Get("Cursor"));
        StyleStale = false;
        return style;
    }

    std::shared_ptr<ElementState> MakeElement(std::unique_ptr<Behavior> kind)
    {
        auto tree = std::make_shared<ElementTree>();
        auto element = std::make_shared<ElementState>(std::move(kind));
        element->Owner = tree;
        element->Row = tree->Elements.Add("", element->Kind->Name());
        tree->ByIdentifier[element->Row.Identifier()] = element.get();
        return element;
    }

    bool IsInside(const ElementState* element, const ElementState* ancestor)
    {
        for (const ElementState* current = element; current; current = current->Parent)
        {
            if (current == ancestor)
            {
                return true;
            }
        }
        return false;
    }

    namespace
    {
        void CopyInto(ElementState& element, const std::shared_ptr<ElementTree>& tree, const Node& parent, bool removeOld)
        {
            std::string name = element.Row.Name();
            Node row = parent ? parent.Add(name, element.Kind->Name()) : tree->Elements.Add(name, element.Kind->Name());
            for (const std::string& property : element.Row.Properties())
            {
                row.Set(property, element.Row.Get(property));
            }
            Node old = element.Row;
            std::shared_ptr<ElementTree> oldTree = element.Owner;
            if (oldTree)
            {
                oldTree->ByIdentifier.erase(old.Identifier());
            }
            element.Owner = tree;
            element.Row = row;
            tree->ByIdentifier[row.Identifier()] = &element;
            element.StyleStale = true;
            // Each root counts its own layouts, so a measurement from another
            // root could look current.
            element.LastMeasurement = {};
            for (const std::shared_ptr<ElementState>& child : element.Children)
            {
                CopyInto(*child, tree, row, false);
            }
            if (removeOld)
            {
                old.Remove();
                if (oldTree && oldTree->Root)
                {
                    oldTree->Root->Invalidate(true);
                }
            }
        }
    }

    void MoveToTree(ElementState& element, const std::shared_ptr<ElementTree>& tree, const Node& parent)
    {
        RootState* leaving = element.Root();
        CopyInto(element, tree, parent, true);
        if (leaving && leaving != tree->Root)
        {
            leaving->Forget(element);
        }
        if (tree->Root)
        {
            tree->Root->Invalidate(true);
        }
    }

    void AddChild(ElementState& parent, const std::shared_ptr<ElementState>& child, std::size_t index)
    {
        if (!child)
        {
            return;
        }
        if (!parent.Kind->HoldsChildren())
        {
            Log(LogLevel::Warning, "a {} does not hold other elements; put them in a Row, Column, Stack, or Panel",
                parent.Kind->Name());
            return;
        }
        if (IsInside(&parent, child.get()))
        {
            Log(LogLevel::Warning, "an element cannot be put inside itself");
            return;
        }
        std::shared_ptr<ElementState> kept = child;
        if (ElementState* old = child->Parent)
        {
            std::erase(old->Children, child);
            old->Changed(true);
            child->Parent = nullptr;
        }
        if (child->Owner != parent.Owner)
        {
            MoveToTree(*child, parent.Owner, parent.Row);
        }
        else
        {
            child->Row.MoveTo(parent.Row);
        }
        child->Parent = &parent;
        index = std::min(index, parent.Children.size());
        parent.Children.insert(parent.Children.begin() + static_cast<std::ptrdiff_t>(index), kept);
        parent.Changed(true);
    }

    void RemoveFromParent(ElementState& child)
    {
        ElementState* parent = child.Parent;
        if (!parent)
        {
            return;
        }
        std::shared_ptr<ElementState> kept = child.shared_from_this();
        std::erase(parent->Children, kept);
        child.Parent = nullptr;
        MoveToTree(child, std::make_shared<ElementTree>(), Node());
        parent->Changed(true);
    }

    ElementState* ElementOf(const ElementTree& tree, const Node& node)
    {
        auto found = tree.ByIdentifier.find(node.Identifier());
        return found == tree.ByIdentifier.end() ? nullptr : found->second;
    }

    std::shared_ptr<ElementState> FindByName(ElementState& start, std::string_view name, std::string_view kind)
    {
        for (const std::shared_ptr<ElementState>& child : start.Children)
        {
            if (child->Row.Name() == name && (kind.empty() || child->Kind->Name() == kind))
            {
                return child;
            }
            if (std::shared_ptr<ElementState> found = FindByName(*child, name, kind))
            {
                return found;
            }
        }
        return nullptr;
    }

    std::string_view KindOf(const ElementState* state)
    {
        return state ? state->Kind->Name() : std::string_view();
    }

    std::shared_ptr<ElementState> FindInside(const ElementState* state, std::string_view name, std::string_view kind)
    {
        return state ? FindByName(const_cast<ElementState&>(*state), name, kind) : nullptr;
    }

    const std::shared_ptr<ElementState>& StateOfHandle(const Element& element)
    {
        return element.State;
    }

    void AddChildren(ElementState& parent, const std::vector<Element>& children)
    {
        for (const Element& child : children)
        {
            AddChild(parent, StateOfHandle(child));
        }
    }

    void StartElementAnimation(const void* owner, std::string_view property,
        std::function<void(void* owner, float progress)> step, const AnimationSettings& settings)
    {
        ElementState* element = StateOf(owner);
        if (!element)
        {
            return;
        }
        StopAnimation(*element, property);
        element->Animations.push_back({ std::string(property), std::move(step), settings, 0.0f });
        element->Changed(false);
    }

    void StopAnimation(ElementState& element, std::string_view property)
    {
        std::erase_if(element.Animations, [&](const Animation& animation) { return animation.Property == property; });
    }

    bool AffectsLayout(std::string_view property)
    {
        constexpr std::string_view visualOnly[] = { "Opacity", "Offset", "Scale", "BorderColor", "Color", "Tint",
            "Tooltip", "Cursor", "Enabled", "Checked", "Value", "DragText" };
        return std::find(std::begin(visualOnly), std::end(visualOnly), property) == std::end(visualOnly);
    }
}
