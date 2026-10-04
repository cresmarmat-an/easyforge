#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include <easyforge/core/Event.h>
#include <easyforge/core/Host.h>
#include <easyforge/data/Table.h>
#include <easyforge/graphics/Canvas.h>
#include <easyforge/ui/Element.h>
#include <easyforge/ui/Theme.h>

namespace easyforge::ui
{
    namespace internal
    {
        class RootState;
        std::shared_ptr<ElementState> FindInRoot(const RootState* root, std::string_view name, std::string_view kind);
        const std::shared_ptr<RootState>& StateOfRoot(const Root& root);
    }

    struct RootSettings
    {
        // What the root shows, filling its canvas.
        Element Content;

        // Without one, the system's light or dark theme when the root belongs to a
        // window, and the light theme otherwise.
        std::optional<ui::Theme> Theme;
    };

    // One interface: its theme, the element that has the keyboard, and what it
    // draws with. Everything assigned to one window's Content and TitleBar shares
    // the window's root:
    //
    //     ui::Root::Of(window).Theme = ui::Theme::Dark();
    //     ui::Button save = ui::Root::Of(window).Find<ui::Button>("Save");
    //
    // Without the window library, make a root and pass it events and a canvas:
    //
    //     ui::Root root = ui::Root::New({ .Content = ui::Column({ ... }) });
    //     root.HandleEvent(event);                // true when the interface used it
    //     root.Draw(canvas, deltaSeconds);
    //
    // Root is a handle: copies refer to the same root.
    class Root
    {
    public:
        static Root New(const RootSettings& settings = {});

        // The root of a window's interface, made the first time it is asked for.
        static Root Of(const std::shared_ptr<Host>& host);

        // No root. Tests as false.
        Root();
        Root(const Root& other);
        Root& operator=(const Root& other);
        ~Root();

        explicit operator bool() const;

        // Passes an event to the interface, and marks it handled when the
        // interface used it. Returns whether it did.
        bool HandleEvent(Event& event) const;

        // Lays out what changed, moves animations on by `deltaSeconds`, and draws
        // everything into the canvas, filling it.
        void Draw(const Canvas& canvas, float deltaSeconds) const;

        // The first element with the name, of the kind asked for, or no element.
        template <typename Type = Element>
        Type Find(std::string_view name) const
        {
            return Type(internal::FindInRoot(State.get(), name, Type::KindName));
        }

        // The element with the keyboard, or no element.
        Element Focused() const;

        // The table the interface is stored in: one node for each element, with
        // its settings as properties. Scripts and tools can read it; change the
        // interface through its elements.
        Table Data() const;

        // What the root shows. For a window's root, the window's Content.
        Property<Element> Content;

        // Assigning switches at once; AnimateTo changes gradually. Assigning a
        // theme stops a window's root from following the system's light or dark
        // setting.
        Animated<ui::Theme> Theme;

    private:
        explicit Root(std::shared_ptr<internal::RootState> state);

        std::shared_ptr<internal::RootState> State;

        friend const std::shared_ptr<internal::RootState>& internal::StateOfRoot(const Root& root);
    };
}
