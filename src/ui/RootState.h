#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <easyforge/core/Clock.h>
#include <easyforge/core/Host.h>
#include <easyforge/graphics/Renderer.h>
#include <easyforge/ui/Theme.h>

#include "Behavior.h"
#include "ElementState.h"

namespace easyforge::ui::internal
{
    class SlotView;

    // One interface: the table its elements are stored in, the theme, the
    // keyboard focus, and, for a window, the renderer.
    class RootState : public HostListener, public std::enable_shared_from_this<RootState>
    {
    public:
        RootState();
        ~RootState() override;

        // The root of a host's interface, made the first time it is asked for.
        static std::shared_ptr<RootState> For(Host& host);

        // An element shown in an area: a window's Content or TitleBar, or the
        // content of a root without a window.
        struct Slot
        {
            std::shared_ptr<ElementState> Element;
            Rectangle Area;
            SlotView* View = nullptr;

            // The window has asked for its title bar's size or hit test.
            bool TitleBar = false;

            // The height the window was last told the title bar wants.
            float WantedHeight = -1.0f;
        };

        // An element shown over everything else: a popup below an anchor, or a
        // dialog in the middle.
        struct Overlay
        {
            std::shared_ptr<ElementState> Element;

            // A popup sits below its anchor, or above it when there is no room.
            // Without one, the overlay is centered.
            std::weak_ptr<ElementState> Anchor;

            // Dims everything below and keeps the keyboard and pointer inside.
            bool Modal = false;

            bool ClosesOnOutsideClick = true;
            std::function<void()> OnClosed;

            // Where the keyboard was when it opened, to give it back.
            std::weak_ptr<ElementState> FocusBefore;

            Rectangle Area;
        };

        void OpenOverlay(Overlay overlay);
        void CloseOverlay(const ElementState* element);
        void CloseAllOverlays();
        bool IsOverlayOpen(const ElementState* element) const;

        void AddSlot(SlotView& view, const std::shared_ptr<ElementState>& element);
        void RemoveSlot(SlotView& view);
        void PlaceSlot(SlotView& view, Rectangle area);
        void SetContent(const std::shared_ptr<ElementState>& element);
        ElementState* ContentElement() const;

        // Takes the element into the root's table, at its top.
        void Adopt(ElementState& element);

        // Something not in the table changed.
        void Invalidate(bool layout);

        // Every slot's part of an event. Returns whether the interface used it.
        bool Dispatch(Event& event);

        // Updates, lays out what changed, and draws every slot into the canvas.
        void Draw(const Canvas& canvas, float deltaSeconds);

        // For a title bar slot.
        HitArea TitleBarHitTest(const SlotView& view, Vector2 point);
        Vector2 PreferredSize(const SlotView& view, Vector2 available);

        void SetTheme(const ui::Theme& theme, bool chosen);

        void SetFocus(ElementState* element, bool fromKeyboard);
        ElementState* FocusedElement() const;
        ElementState* CapturedElement() const;

        // An element has left the root: whatever pointed into it lets go, and its
        // animations stop.
        void Forget(ElementState& element);

        // Whether a window shows the slot. A hidden title bar has no room.
        static bool Shown(const Slot& slot)
        {
            return slot.View == nullptr || (slot.Area.Width > 0.0f && slot.Area.Height > 0.0f);
        }

        // The system's clipboard for a window's root, and one of the root's own
        // otherwise.
        std::string ClipboardText() const;
        void SetClipboardText(std::string_view text);

        Context MakeContext(float deltaSeconds = 0.0f) const;

        // HostListener: a window's root draws once the window's frame is done.
        void HandleEvent(const Event&) override {}
        void FrameStarted(float) override {}
        void FrameEnded() override;

        std::shared_ptr<ElementTree> Interface;
        std::vector<Slot> Slots;
        std::vector<Overlay> Overlays;

        Host* TheHost = nullptr;
        bool Listening = false;
        Renderer TheRenderer;
        bool RendererFailed = false;

        // A frame is being drawn, so the renderer must outlive it.
        bool Drawing = false;
        bool ReleaseRendererAfterFrame = false;

        ui::Theme CurrentTheme;
        bool FollowSystem = true;

        // The shaders behind Mask and ColorAdjust, made the first time one is
        // drawn. Each root keeps its own, so they go with it.
        easyforge::Shader MaskEffect;
        easyforge::Shader ColorAdjustEffect;
        bool EffectShadersMade = false;
        std::vector<Animation> ThemeAnimations;

        Clock SinceStart;
        float PendingDelta = 0.0f;
        float Scale = 1.0f;

        std::uint64_t SeenVersion = 0;
        bool LayoutDirty = true;
        std::uint64_t LayoutPass = 0;
        Vector2 LastCanvasSize;

        std::weak_ptr<ElementState> Focus;
        std::weak_ptr<ElementState> Hover;
        std::weak_ptr<ElementState> Capture;
        bool FocusVisible = false;

        // The pointer, for hover and tooltips.
        Vector2 PointerPosition;
        bool PointerInside = false;
        float RestSeconds = 0.0f;
        std::weak_ptr<ElementState> TooltipElement;
        bool TooltipShown = false;

        // Dragging an element's DragText onto an element with OnDrop. A press on
        // an element with drag text makes it the source; moving far enough starts
        // the drag.
        std::weak_ptr<ElementState> DragSource;
        std::weak_ptr<ElementState> DropTarget;
        Vector2 DragStart;
        bool Dragging = false;
        void CancelDrag();

        std::optional<easyforge::Cursor> AppliedCursor;
        bool TextInputOn = false;

        // A key moved the keyboard, so the character it types is not for the
        // element that has the keyboard now.
        bool DropTypedText = false;
        std::string OwnClipboard;

    private:
        void ProcessChanges();
        void UpdateElements(ElementState& element, Context& context);
        void StepAnimations(ElementState& element, float deltaSeconds);
        void StepTheme(float deltaSeconds);
        void LayOut(Context& context);
        ElementState* HitTest(Vector2 point, bool interactiveOnly, const Slot** slot = nullptr);
        void UpdateHover(Vector2 point, Context& context);
        void ApplyCursor();
        void MoveFocus(bool backward);
        void DrawTooltip(DrawContext& context);
        void DrawDrag(DrawContext& context);
        ElementState* FindAt(Vector2 point, const std::function<bool(ElementState&)>& wanted);
    };

    // A root slot as a window sees it.
    class SlotView final : public View
    {
    public:
        explicit SlotView(std::shared_ptr<ElementState> element) : Element(std::move(element)) {}
        ~SlotView() override;

        void Attach(Host& host) override;
        void Detach() override;
        void Place(Rectangle area) override;
        void HandleEvent(Event& event) override;
        void Frame(float deltaSeconds) override;
        Vector2 PreferredSize(Vector2 available) const override;
        HitArea HitTest(Vector2 point) const override;

        std::shared_ptr<ElementState> Element;
        std::shared_ptr<RootState> Root;
    };

    // Draws an element, its effects, and everything in it.
    void DrawElement(ElementState& element, DrawContext& context);

    // The font used when the theme has none: the system's interface font.
    const easyforge::Font& SystemFont();

    // Whether a change to the setting can move or resize anything.
    bool AffectsLayout(std::string_view property);
}
