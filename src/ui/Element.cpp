#include <easyforge/ui/Element.h>

#include "Behavior.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui
{
    using namespace internal;

    namespace
    {
        Property<std::string> NameProperty(std::shared_ptr<ElementState>& state)
        {
            return Property<std::string>(
                &state,
                [](const void* owner) -> std::string {
                    ElementState* element = StateOf(owner);
                    return element ? element->Row.Name() : std::string();
                },
                [](void* owner, const std::string& value) {
                    if (ElementState* element = StateOf(owner))
                    {
                        element->Row.Rename(value);
                    }
                });
        }
    }

    Element::Element() : Element(std::shared_ptr<ElementState>())
    {
    }

    Element::Element(std::shared_ptr<internal::ElementState> state)
        : Name(NameProperty(State)), Width(Stored<Size, "Width">(State)), Height(Stored<Size, "Height">(State)),
          MinimumWidth(Stored<float, "MinimumWidth">(State)), MinimumHeight(Stored<float, "MinimumHeight">(State)),
          MaximumWidth(Stored<float, "MaximumWidth">(State)), MaximumHeight(Stored<float, "MaximumHeight">(State)),
          Margin(Stored<Insets, "Margin">(State)), Padding(Stored<Insets, "Padding">(State)),
          CornerRadius(Stored<std::optional<float>, "CornerRadius">(State)),
          Background(Kept<std::optional<ui::Background>, "Background">(State)),
          BorderWidth(Stored<float, "BorderWidth">(State)), BorderColor(Stored<std::optional<Color>, "BorderColor">(State)),
          Opacity(StoredAnimated<float, "Opacity">(State)), Offset(StoredAnimated<Vector2, "Offset">(State)),
          Scale(StoredAnimated<float, "Scale">(State)), Effects(Kept<std::vector<Effect>, "Effects">(State)),
          Shader(Kept<easyforge::Shader, "Shader">(State)), ShaderValues(Kept<std::vector<ShaderValue>, "ShaderValues">(State)),
          Visible(Stored<bool, "Visible">(State)), Enabled(Stored<bool, "Enabled">(State)),
          Tooltip(Stored<std::string, "Tooltip">(State)), Cursor(Stored<std::optional<easyforge::Cursor>, "Cursor">(State)),
          DragText(Stored<std::string, "DragText">(State)), OnDrop(Kept<std::function<void(const std::string&)>, "OnDrop">(State)),
          OnFilesDropped(Kept<std::function<void(const std::vector<std::string>&)>, "OnFilesDropped">(State)),
          State(std::move(state))
    {
    }

    Element::Element(const Element& other) : Element(other.State)
    {
    }

    Element& Element::operator=(const Element& other)
    {
        State = other.State;
        return *this;
    }

    Element::~Element() = default;

    Element::operator bool() const
    {
        return State != nullptr;
    }

    std::string_view Element::Kind() const
    {
        return KindOf(State.get());
    }

    Element Element::Parent() const
    {
        if (!State || !State->Parent)
        {
            return Element();
        }
        return Element(State->Parent->shared_from_this());
    }

    std::vector<Element> Element::Children() const
    {
        std::vector<Element> children;
        if (State)
        {
            for (const std::shared_ptr<ElementState>& child : State->Children)
            {
                children.push_back(Element(child));
            }
        }
        return children;
    }

    void Element::Add(const Element& child) const
    {
        if (State && child.State)
        {
            AddChild(*State, child.State);
        }
    }

    void Element::Insert(std::size_t index, const Element& child) const
    {
        if (State && child.State)
        {
            AddChild(*State, child.State, index);
        }
    }

    void Element::Remove() const
    {
        if (State)
        {
            RemoveFromParent(*State);
        }
    }

    void Element::Clear() const
    {
        if (!State)
        {
            return;
        }
        std::vector<std::shared_ptr<ElementState>> children = State->Children;
        for (const std::shared_ptr<ElementState>& child : children)
        {
            RemoveFromParent(*child);
        }
    }

    Rectangle Element::Frame() const
    {
        return State ? State->Frame : Rectangle {};
    }

    bool Element::IsHovered() const
    {
        return State && State->Hovered;
    }

    bool Element::IsPressed() const
    {
        return State && State->Pressed;
    }

    bool Element::IsFocused() const
    {
        return State && State->Focused;
    }

    void Element::Focus() const
    {
        if (State)
        {
            if (RootState* root = State->Root())
            {
                root->SetFocus(State.get(), false);
            }
        }
    }

    Element::operator std::shared_ptr<View>() const
    {
        if (!State)
        {
            return nullptr;
        }
        return std::make_shared<SlotView>(State);
    }
}
