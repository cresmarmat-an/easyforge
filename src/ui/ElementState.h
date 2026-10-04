#pragma once

// How an element is kept. Its settings are properties of a node in a data table,
// which records every change; what cannot be a table value (callbacks, effects,
// textures, shaders) is kept beside it. Each tree of elements built outside a
// root has a table of its own, and moves into the root's table when it is shown.

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <easyforge/core/Rectangle.h>
#include <easyforge/data/Table.h>
#include <easyforge/ui/Element.h>

namespace easyforge::ui::internal
{
    class Behavior;
    class ElementState;
    class RootState;

    // One table of elements: a tree built outside any root, or a root's whole
    // interface.
    struct ElementTree
    {
        ElementTree();

        Table Elements;
        std::unordered_map<std::uint64_t, ElementState*> ByIdentifier;
        RootState* Root = nullptr;
    };

    struct Animation
    {
        std::string Property;
        std::function<void(void*, float)> Step;
        AnimationSettings Settings;
        float Elapsed = 0.0f;
    };

    // The settings every element has, read once each time they change.
    struct Style
    {
        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;
        Insets Margin;
        Insets Padding;
        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<Color> BorderColor;
        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;
        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;
    };

    class ElementState : public std::enable_shared_from_this<ElementState>
    {
    public:
        explicit ElementState(std::unique_ptr<Behavior> kind);
        ~ElementState();

        ElementState(const ElementState&) = delete;
        ElementState& operator=(const ElementState&) = delete;

        // A setting: the element's own value, or its kind's default.
        DataValue Get(std::string_view name) const;

        // Sets a setting; nothing clears it back to the default.
        void Set(std::string_view name, const DataValue& value);

        // Values kept beside the table.
        template <typename Value>
        Value Object(std::string_view name) const
        {
            auto found = Objects.find(name);
            if (found == Objects.end())
            {
                return Value {};
            }
            const Value* value = std::any_cast<Value>(&found->second);
            return value ? *value : Value {};
        }
        bool HasObject(std::string_view name) const { return Objects.find(name) != Objects.end(); }
        void SetObject(std::string_view name, std::any value);
        void ClearObject(std::string_view name);

        // Something about the element changed that is not in the table.
        void Changed(bool affectsLayout = true);

        const Style& CurrentStyle();

        RootState* Root() const { return Owner ? Owner->Root : nullptr; }
        bool IsEnabled() const;
        std::string Name() const { return Row.Name(); }

        std::shared_ptr<ElementTree> Owner;
        Node Row;
        std::unique_ptr<Behavior> Kind;
        ElementState* Parent = nullptr;
        std::vector<std::shared_ptr<ElementState>> Children;
        std::map<std::string, std::any, std::less<>> Objects;

        // Where the last layout put it, in points: the box inside the margin.
        Rectangle Frame;

        // The last measurement, kept for the rest of a layout.
        struct Measurement
        {
            Vector2 Available;
            bool FillWidth = false;
            bool FillHeight = false;
            Vector2 Result;
            std::uint64_t Pass = 0;
        };
        Measurement LastMeasurement;

        bool StyleStale = true;
        Style CachedStyle;

        bool Hovered = false;
        bool Pressed = false;
        bool Focused = false;

        // Hover and press, eased from 0 to 1 over the theme's transition time.
        float HoverAmount = 0.0f;
        float PressAmount = 0.0f;

        std::vector<Animation> Animations;

        // An animation is writing a property, so the write should not stop it.
        bool Animating = false;
    };

    // Makes an element with a table of its own.
    std::shared_ptr<ElementState> MakeElement(std::unique_ptr<Behavior> kind);

    // Children. A child from another table moves into the parent's.
    void AddChild(ElementState& parent, const std::shared_ptr<ElementState>& child,
        std::size_t index = static_cast<std::size_t>(-1));
    void RemoveFromParent(ElementState& child);

    // Moves the element and everything in it into another table, under `parent`,
    // or at its top when `parent` is no node.
    void MoveToTree(ElementState& element, const std::shared_ptr<ElementTree>& tree, const Node& parent);

    // The element's node was renamed, removed, or changed in the table.
    ElementState* ElementOf(const ElementTree& tree, const Node& node);

    std::shared_ptr<ElementState> FindByName(ElementState& start, std::string_view name, std::string_view kind);

    // Whether `element` is `ancestor` or inside it.
    bool IsInside(const ElementState* element, const ElementState* ancestor);

    // The state of a handle's owner pointer, as properties see it.
    inline ElementState* StateOf(const void* owner)
    {
        return static_cast<const std::shared_ptr<ElementState>*>(owner)->get();
    }

    inline std::shared_ptr<ElementState>& SharedStateOf(void* owner)
    {
        return *static_cast<std::shared_ptr<ElementState>*>(owner);
    }

    void StartElementAnimation(const void* owner, std::string_view property,
        std::function<void(void* owner, float progress)> step, const AnimationSettings& settings);
    void StopAnimation(ElementState& element, std::string_view property);
}
