#pragma once

// Behaviors for elements that hold others, shared by the kinds built on them:
// Display and TitleBar are lines too.

#include <memory>
#include <string_view>

#include <easyforge/ui/Layout.h>

#include "Behavior.h"

namespace easyforge::ui::internal
{
    // Children along one axis: Row and Column.
    class LineBehavior : public Behavior
    {
    public:
        LineBehavior(std::string_view name, bool horizontal) : KindName(name), Horizontal(horizontal) {}

        std::string_view Name() const override { return KindName; }
        bool HoldsChildren() const override { return true; }
        Vector2 MeasureContent(ElementState& element, Context& context, const Constraint& constraint) override;
        void Arrange(ElementState& element, Context& context, Rectangle content) override;

    protected:
        std::string_view KindName;
        bool Horizontal;
    };

    // A column in the theme's surface box.
    class PanelBehavior : public LineBehavior
    {
    public:
        PanelBehavior() : LineBehavior(Panel::KindName, false) {}
        Box LookOf(ElementState& element, const Context& context) override;
    };

    std::unique_ptr<Behavior> MakeStackBehavior();

    // Writes the settings of Row, Column, Stack, Panel, Display, and TitleBar.
    void ApplyContainerSettings(ElementState& element, const ContainerSettings& settings);
}
