#include <easyforge/ui/Root.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include <easyforge/core/Log.h>

#include "Drawing.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    namespace
    {
        constexpr std::string_view SharedName = "easyforge.ui.root";

        // Seconds the pointer rests before a tooltip shows.
        constexpr float TooltipDelay = 0.6f;

        float Approach(float value, float target, float step)
        {
            return value < target ? Min(value + step, target) : Max(value - step, target);
        }

        // A point in the root, in the space an element was laid out in: its own
        // offset and scale, and those of the elements around it, undone.
        Vector2 Unmove(const ElementState& element, Vector2 point)
        {
            std::vector<const ElementState*> chain;
            for (const ElementState* current = &element; current; current = current->Parent)
            {
                chain.push_back(current);
            }
            for (auto current = chain.rbegin(); current != chain.rend(); ++current)
            {
                const Style& style = const_cast<ElementState*>(*current)->CurrentStyle();
                if (style.Offset != Vector2 {} || style.Scale != 1.0f)
                {
                    Vector2 center = (*current)->Frame.Center();
                    Vector2 shift = style.Offset + center * (1.0f - style.Scale);
                    point = (point - shift) / Max(style.Scale, 0.0001f);
                }
            }
            return point;
        }

        bool Blocks(ElementState& element, const Context& context)
        {
            Box box = element.Kind->LookOf(element, context);
            if (!box.Background)
            {
                return false;
            }
            switch (box.Background->Type)
            {
            case Background::Kind::None: return false;
            case Background::Kind::Color: return box.Background->First.Alpha > 0.0f;
            default: return true;
            }
        }

        // How far the pointer moves with the button down before a drag starts.
        constexpr float DragDistance = 4.0f;

        bool HasDragText(ElementState& element)
        {
            return !element.Get("DragText").AsText().empty();
        }

        // The deepest element under the point that answers the pointer, blocks it
        // with a box, has a tooltip, or can be dragged.
        ElementState* HitElement(ElementState& element, Vector2 point, const Context& context)
        {
            const Style& style = element.CurrentStyle();
            if (!style.Visible)
            {
                return nullptr;
            }
            Vector2 local = point;
            if (style.Offset != Vector2 {} || style.Scale != 1.0f)
            {
                Vector2 center = element.Frame.Center();
                local = (point - (style.Offset + center * (1.0f - style.Scale))) / Max(style.Scale, 0.0001f);
            }
            bool inside = element.Frame.Contains(local);
            if (!inside && element.Kind->ClipsChildren(element))
            {
                return nullptr;
            }
            if (inside && element.Kind->ClaimsPoint(element, local))
            {
                return &element;
            }
            std::vector<ElementState*> children;
            element.Kind->InteractiveChildren(element, children);
            for (auto child = children.rbegin(); child != children.rend(); ++child)
            {
                if (ElementState* hit = HitElement(**child, local, context))
                {
                    return hit;
                }
            }
            if (!inside)
            {
                return nullptr;
            }
            if (element.Kind->TakesPointerAt(element, local) || !style.Tooltip.empty() || Blocks(element, context) ||
                HasDragText(element))
            {
                return &element;
            }
            return nullptr;
        }

        // The deepest visible, enabled element under the point that `wanted`
        // accepts, whether or not it takes the pointer.
        ElementState* FindIn(ElementState& element, Vector2 point, const std::function<bool(ElementState&)>& wanted)
        {
            const Style& style = element.CurrentStyle();
            if (!style.Visible || !style.Enabled)
            {
                return nullptr;
            }
            Vector2 local = point;
            if (style.Offset != Vector2 {} || style.Scale != 1.0f)
            {
                Vector2 center = element.Frame.Center();
                local = (point - (style.Offset + center * (1.0f - style.Scale))) / Max(style.Scale, 0.0001f);
            }
            bool inside = element.Frame.Contains(local);
            if (!inside && element.Kind->ClipsChildren(element))
            {
                return nullptr;
            }
            std::vector<ElementState*> children;
            element.Kind->InteractiveChildren(element, children);
            for (auto child = children.rbegin(); child != children.rend(); ++child)
            {
                if (ElementState* found = FindIn(**child, local, wanted))
                {
                    return found;
                }
            }
            return inside && wanted(element) ? &element : nullptr;
        }

        void CollectFocusable(ElementState& element, std::vector<ElementState*>& into)
        {
            const Style& style = element.CurrentStyle();
            if (!style.Visible || !style.Enabled)
            {
                return;
            }
            if (element.Kind->TakesKeyboard(element))
            {
                into.push_back(&element);
            }
            std::vector<ElementState*> children;
            element.Kind->InteractiveChildren(element, children);
            for (ElementState* child : children)
            {
                CollectFocusable(*child, into);
            }
        }

        void MarkAllStale(ElementState& element)
        {
            element.StyleStale = true;
            for (const std::shared_ptr<ElementState>& child : element.Children)
            {
                MarkAllStale(*child);
            }
        }
    }

    namespace
    {
        std::string EnvironmentValue(const char* name)
        {
#if defined(_MSC_VER)
            // The Microsoft library warns about getenv, and offers this instead.
            char* value = nullptr;
            std::size_t length = 0;
            std::string result;
            if (_dupenv_s(&value, &length, name) == 0 && value)
            {
                result = value;
            }
            std::free(value);
            return result;
#else
            const char* value = std::getenv(name);
            return value ? std::string(value) : std::string();
#endif
        }
    }

    const easyforge::Font& SystemFont()
    {
        static easyforge::Font font = [] {
            std::vector<std::string> candidates;
            std::string windows = EnvironmentValue("WINDIR");
            if (!windows.empty())
            {
                candidates.push_back(windows + "/Fonts/segoeui.ttf");
                candidates.push_back(windows + "/Fonts/arial.ttf");
            }
            candidates.push_back("/System/Library/Fonts/Supplemental/Arial.ttf");
            candidates.push_back("/Library/Fonts/Arial.ttf");
            candidates.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
            candidates.push_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
            candidates.push_back("/usr/share/fonts/noto/NotoSans-Regular.ttf");
            for (const std::string& path : candidates)
            {
                std::error_code error;
                if (std::filesystem::exists(path, error))
                {
                    easyforge::Font loaded = easyforge::Font::Load(path);
                    if (loaded)
                    {
                        return loaded;
                    }
                }
            }
            Log(LogLevel::Warning, "ui found no system font; give the theme a Font to show text");
            return easyforge::Font();
        }();
        return font;
    }

    // ---- The root -----------------------------------------------------------------

    RootState::RootState() : Interface(std::make_shared<ElementTree>()), CurrentTheme(ui::Theme::Light())
    {
        Interface->Root = this;
    }

    RootState::~RootState()
    {
        if (Listening && TheHost)
        {
            TheHost->RemoveListener(*this);
        }
        if (Interface)
        {
            Interface->Root = nullptr;
        }
    }

    std::shared_ptr<RootState> RootState::For(Host& host)
    {
        std::shared_ptr<void>& shared = host.Shared(SharedName);
        if (!shared)
        {
            auto root = std::make_shared<RootState>();
            root->TheHost = &host;
            root->Scale = host.Scale();
            root->CurrentTheme = ui::Theme::For(host.SystemColorScheme());
            shared = root;
        }
        return std::static_pointer_cast<RootState>(shared);
    }

    void RootState::Adopt(ElementState& element)
    {
        if (element.Parent)
        {
            std::shared_ptr<ElementState> kept = element.shared_from_this();
            std::erase(element.Parent->Children, kept);
            element.Parent->Changed(true);
            element.Parent = nullptr;
        }
        if (element.Owner != Interface)
        {
            MoveToTree(element, Interface, Node());
        }
        LayoutDirty = true;
    }

    void RootState::AddSlot(SlotView& view, const std::shared_ptr<ElementState>& element)
    {
        if (!element)
        {
            return;
        }
        Adopt(*element);
        Slots.push_back({ element, {}, &view });
        if (TheHost && !Listening)
        {
            TheHost->AddListener(*this);
            Listening = true;
        }
        if (TheHost && !TheRenderer && !RendererFailed)
        {
            TheRenderer = Renderer::New(std::shared_ptr<Host>(std::shared_ptr<Host>(), TheHost));
            if (!TheRenderer)
            {
                RendererFailed = true;
                Log(LogLevel::Error,
                    "ui cannot draw into the window: {}. A window has one renderer, and an interface needs it; draw "
                    "inside ui::DrawingArea or ui::SceneView instead of making a renderer of your own",
                    TheRenderer.Error());
            }
        }
        LayoutDirty = true;
    }

    void RootState::RemoveSlot(SlotView& view)
    {
        auto found = std::find_if(Slots.begin(), Slots.end(), [&](const Slot& slot) { return slot.View == &view; });
        if (found == Slots.end())
        {
            return;
        }
        std::shared_ptr<ElementState> element = found->Element;
        Slots.erase(found);
        bool stillShown = std::any_of(Slots.begin(), Slots.end(), [&](const Slot& slot) { return slot.Element == element; });
        if (element && !stillShown && element->Owner == Interface)
        {
            MoveToTree(*element, std::make_shared<ElementTree>(), Node());
        }
        if (Slots.empty())
        {
            CloseAllOverlays();
            if (Listening && TheHost)
            {
                TheHost->RemoveListener(*this);
                Listening = false;
            }
            // The window may be closing, so the root stops using it. A frame being
            // drawn keeps its renderer until it ends.
            TheHost = nullptr;
            if (Drawing)
            {
                ReleaseRendererAfterFrame = true;
            }
            else
            {
                TheRenderer = Renderer();
            }
            RendererFailed = false;
        }
        LayoutDirty = true;
    }

    void RootState::PlaceSlot(SlotView& view, Rectangle area)
    {
        for (Slot& slot : Slots)
        {
            if (slot.View == &view && !(slot.Area == area))
            {
                slot.Area = area;
                LayoutDirty = true;
            }
        }
    }

    void RootState::OpenOverlay(Overlay overlay)
    {
        if (!overlay.Element || IsOverlayOpen(overlay.Element.get()))
        {
            return;
        }
        Adopt(*overlay.Element);
        overlay.FocusBefore = Focus;
        bool modal = overlay.Modal;
        std::shared_ptr<ElementState> element = overlay.Element;
        Overlays.push_back(std::move(overlay));
        LayoutDirty = true;
        if (modal)
        {
            // The keyboard moves into a dialog: to its first element that takes it.
            std::vector<ElementState*> focusable;
            CollectFocusable(*element, focusable);
            SetFocus(focusable.empty() ? nullptr : focusable.front(), false);
        }
    }

    void RootState::CloseOverlay(const ElementState* element)
    {
        auto found = std::find_if(Overlays.begin(), Overlays.end(),
            [&](const Overlay& overlay) { return overlay.Element.get() == element; });
        if (found == Overlays.end())
        {
            return;
        }
        Overlay overlay = std::move(*found);
        Overlays.erase(found);

        // The keyboard goes back to where it was before the overlay opened.
        if (ElementState* focused = FocusedElement(); focused && IsInside(focused, overlay.Element.get()))
        {
            std::shared_ptr<ElementState> before = overlay.FocusBefore.lock();
            SetFocus(before && before->Root() == this ? before.get() : nullptr, false);
        }
        std::shared_ptr<ElementState> hovered = Hover.lock();
        if (hovered && IsInside(hovered.get(), overlay.Element.get()))
        {
            hovered->Hovered = false;
            Hover.reset();
        }
        std::shared_ptr<ElementState> captured = Capture.lock();
        if (captured && IsInside(captured.get(), overlay.Element.get()))
        {
            captured->Pressed = false;
            Capture.reset();
        }
        if (overlay.Element->Owner == Interface)
        {
            MoveToTree(*overlay.Element, std::make_shared<ElementTree>(), Node());
        }
        LayoutDirty = true;
        if (overlay.OnClosed)
        {
            overlay.OnClosed();
        }
    }

    void RootState::CloseAllOverlays()
    {
        while (!Overlays.empty())
        {
            CloseOverlay(Overlays.back().Element.get());
        }
    }

    bool RootState::IsOverlayOpen(const ElementState* element) const
    {
        return std::any_of(Overlays.begin(), Overlays.end(), [&](const Overlay& overlay) { return overlay.Element.get() == element; });
    }

    void RootState::SetContent(const std::shared_ptr<ElementState>& element)
    {
        auto found = std::find_if(Slots.begin(), Slots.end(), [](const Slot& slot) { return slot.View == nullptr; });
        if (found != Slots.end())
        {
            std::shared_ptr<ElementState> old = found->Element;
            Slots.erase(found);
            if (old && old != element && old->Owner == Interface)
            {
                MoveToTree(*old, std::make_shared<ElementTree>(), Node());
            }
        }
        if (element)
        {
            Adopt(*element);
            Slots.insert(Slots.begin(), { element, { 0.0f, 0.0f, LastCanvasSize.X, LastCanvasSize.Y }, nullptr });
        }
        LayoutDirty = true;
    }

    ElementState* RootState::ContentElement() const
    {
        for (const Slot& slot : Slots)
        {
            if (slot.View == nullptr || !slot.TitleBar)
            {
                return slot.Element.get();
            }
        }
        return nullptr;
    }

    void RootState::Invalidate(bool layout)
    {
        if (layout)
        {
            LayoutDirty = true;
        }
    }

    Context RootState::MakeContext(float deltaSeconds) const
    {
        Context context;
        context.Root = const_cast<RootState*>(this);
        context.Theme = &CurrentTheme;
        context.Font = CurrentTheme.Font ? CurrentTheme.Font : SystemFont();
        context.Scale = Scale;
        context.DeltaSeconds = deltaSeconds;
        context.Time = static_cast<float>(SinceStart.Seconds());
        context.Pass = LayoutPass;
        return context;
    }

    void RootState::SetTheme(const ui::Theme& theme, bool chosen)
    {
        CurrentTheme = theme;
        if (chosen)
        {
            FollowSystem = false;
        }
        LayoutDirty = true;
    }

    namespace
    {
        // Whether the element and everything around it are shown and enabled.
        bool Reachable(ElementState& element)
        {
            for (ElementState* current = &element; current; current = current->Parent)
            {
                if (!current->CurrentStyle().Visible)
                {
                    return false;
                }
            }
            return element.IsEnabled();
        }

        void StopAnimations(ElementState& element)
        {
            element.Animations.clear();
            for (const std::shared_ptr<ElementState>& child : element.Children)
            {
                StopAnimations(*child);
            }
        }
    }

    ElementState* RootState::FocusedElement() const
    {
        std::shared_ptr<ElementState> focused = Focus.lock();
        return focused && focused->Root() == this && Reachable(*focused) ? focused.get() : nullptr;
    }

    void RootState::Forget(ElementState& element)
    {
        if (std::shared_ptr<ElementState> focused = Focus.lock(); focused && IsInside(focused.get(), &element))
        {
            Focus.reset();
            focused->Focused = false;
            Context context = MakeContext();
            focused->Kind->FocusChanged(*focused, context, false);
        }
        if (std::shared_ptr<ElementState> hovered = Hover.lock(); hovered && IsInside(hovered.get(), &element))
        {
            hovered->Hovered = false;
            Hover.reset();
        }
        if (std::shared_ptr<ElementState> captured = Capture.lock(); captured && IsInside(captured.get(), &element))
        {
            captured->Pressed = false;
            Capture.reset();
        }
        if (std::shared_ptr<ElementState> source = DragSource.lock(); source && IsInside(source.get(), &element))
        {
            CancelDrag();
        }
        if (std::shared_ptr<ElementState> target = DropTarget.lock(); target && IsInside(target.get(), &element))
        {
            DropTarget.reset();
        }
        if (std::shared_ptr<ElementState> tip = TooltipElement.lock(); tip && IsInside(tip.get(), &element))
        {
            TooltipElement.reset();
            TooltipShown = false;
        }
        StopAnimations(element);
    }

    ElementState* RootState::CapturedElement() const
    {
        std::shared_ptr<ElementState> captured = Capture.lock();
        return captured && captured->Root() == this ? captured.get() : nullptr;
    }

    void RootState::SetFocus(ElementState* element, bool fromKeyboard)
    {
        if (element && (!element->Kind->TakesKeyboard(*element) || element->Root() != this || !Reachable(*element)))
        {
            return;
        }
        // The element last given the keyboard, even when it has since been hidden.
        std::shared_ptr<ElementState> old = Focus.lock();
        FocusVisible = fromKeyboard;
        if (old.get() == element)
        {
            return;
        }
        Context context = MakeContext();
        Focus.reset();
        if (old)
        {
            old->Focused = false;
            old->Kind->FocusChanged(*old, context, false);
        }
        if (element)
        {
            Focus = element->weak_from_this();
            element->Focused = true;
            element->Kind->FocusChanged(*element, context, true);
        }
    }

    std::string RootState::ClipboardText() const
    {
        return TheHost ? TheHost->ClipboardText() : OwnClipboard;
    }

    void RootState::SetClipboardText(std::string_view text)
    {
        if (TheHost)
        {
            TheHost->SetClipboardText(text);
        }
        else
        {
            OwnClipboard = std::string(text);
        }
    }

    // ---- Each frame -------------------------------------------------------------------

    void RootState::ProcessChanges()
    {
        Table& table = Interface->Elements;
        std::uint64_t version = table.Version();
        if (version == SeenVersion)
        {
            return;
        }
        if (SeenVersion + 1 < table.OldestVersion())
        {
            for (Slot& slot : Slots)
            {
                MarkAllStale(*slot.Element);
            }
            for (Overlay& overlay : Overlays)
            {
                MarkAllStale(*overlay.Element);
            }
            LayoutDirty = true;
        }
        else
        {
            for (const Change& change : table.ChangesSince(SeenVersion))
            {
                if (change.Kind == ChangeKind::PropertySet)
                {
                    if (ElementState* element = ElementOf(*Interface, change.Target))
                    {
                        element->StyleStale = true;
                    }
                    if (AffectsLayout(change.Property))
                    {
                        LayoutDirty = true;
                    }
                }
                else
                {
                    LayoutDirty = true;
                }
            }
        }
        SeenVersion = version;
    }

    void RootState::StepAnimations(ElementState& element, float deltaSeconds)
    {
        if (element.Animations.empty())
        {
            return;
        }
        std::shared_ptr<ElementState> self = element.shared_from_this();
        std::vector<std::function<void()>> finished;
        for (std::size_t index = 0; index < element.Animations.size();)
        {
            Animation& animation = element.Animations[index];
            animation.Elapsed += deltaSeconds;
            float running = animation.Elapsed - animation.Settings.Delay;
            if (running < 0.0f)
            {
                ++index;
                continue;
            }
            float progress = animation.Settings.Duration > 0.0f ? Min(running / animation.Settings.Duration, 1.0f) : 1.0f;
            // Called in place: the step keeps its starting value between frames.
            element.Animating = true;
            animation.Step(&self, Ease(animation.Settings.Easing, progress));
            element.Animating = false;
            if (progress >= 1.0f)
            {
                if (animation.Settings.OnFinished)
                {
                    finished.push_back(animation.Settings.OnFinished);
                }
                element.Animations.erase(element.Animations.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            ++index;
        }
        for (const std::function<void()>& callback : finished)
        {
            callback();
        }
    }

    void RootState::StepTheme(float deltaSeconds)
    {
        if (ThemeAnimations.empty())
        {
            return;
        }
        std::shared_ptr<RootState> self = shared_from_this();
        std::vector<std::function<void()>> finished;
        for (std::size_t index = 0; index < ThemeAnimations.size();)
        {
            Animation& animation = ThemeAnimations[index];
            animation.Elapsed += deltaSeconds;
            float running = animation.Elapsed - animation.Settings.Delay;
            if (running < 0.0f)
            {
                ++index;
                continue;
            }
            float progress = animation.Settings.Duration > 0.0f ? Min(running / animation.Settings.Duration, 1.0f) : 1.0f;
            animation.Step(&self, Ease(animation.Settings.Easing, progress));
            if (progress >= 1.0f)
            {
                if (animation.Settings.OnFinished)
                {
                    finished.push_back(animation.Settings.OnFinished);
                }
                ThemeAnimations.erase(ThemeAnimations.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            ++index;
        }
        for (const std::function<void()>& callback : finished)
        {
            callback();
        }
    }

    void RootState::UpdateElements(ElementState& element, Context& context)
    {
        StepAnimations(element, context.DeltaSeconds);
        float step = context.DeltaSeconds / Max(context.Theme->Transition, 0.001f);
        element.HoverAmount = Approach(element.HoverAmount, element.Hovered ? 1.0f : 0.0f, step);
        element.PressAmount = Approach(element.PressAmount, element.Pressed ? 1.0f : 0.0f, step * 2.0f);
        element.Kind->Update(element, context);
        std::vector<std::shared_ptr<ElementState>> children = element.Children;
        for (const std::shared_ptr<ElementState>& child : children)
        {
            UpdateElements(*child, context);
        }
    }

    void RootState::LayOut(Context& context)
    {
        ++LayoutPass;
        context.Pass = LayoutPass;
        for (Slot& slot : Slots)
        {
            // A title bar that wants another height asks the window to place it
            // again, once for each change.
            if (slot.TitleBar && slot.View)
            {
                Vector2 wanted = Measure(*slot.Element, context, { slot.Area.Width, Unlimited }, true, false);
                float height = wanted.Y + slot.Element->CurrentStyle().Margin.Vertical();
                if (height != slot.WantedHeight)
                {
                    slot.WantedHeight = height;
                    if (TheHost)
                    {
                        TheHost->RequestPlacement();
                    }
                }
            }
            if (!Shown(slot))
            {
                continue;
            }
            ElementState& element = *slot.Element;
            const Style& style = element.CurrentStyle();
            Vector2 size = Measure(element, context, slot.Area.Size(), true, true);
            if (style.Width.Type == Size::Kind::Fit)
            {
                size.X = Clamp(Max(slot.Area.Width - style.Margin.Horizontal(), 0.0f), style.MinimumWidth,
                    Max(style.MaximumWidth, style.MinimumWidth));
            }
            if (style.Height.Type == Size::Kind::Fit)
            {
                size.Y = Clamp(Max(slot.Area.Height - style.Margin.Vertical(), 0.0f), style.MinimumHeight,
                    Max(style.MaximumHeight, style.MinimumHeight));
            }
            Place(element, context, { slot.Area.X + style.Margin.Left, slot.Area.Y + style.Margin.Top, size.X, size.Y });
        }

        // Overlays go after the slots, so their anchors are already in place.
        Vector2 room = LastCanvasSize;
        for (Overlay& overlay : Overlays)
        {
            ElementState& element = *overlay.Element;
            Vector2 size = Measure(element, context, room, true, true);
            size = { Min(size.X, room.X), Min(size.Y, room.Y) };
            Rectangle area { (room.X - size.X) * 0.5f, (room.Y - size.Y) * 0.5f, size.X, size.Y };
            std::shared_ptr<ElementState> anchor = overlay.Anchor.lock();
            if (anchor && anchor->Root() == this)
            {
                // Below the anchor, or above it when there is no room below.
                Rectangle below = anchor->Frame;
                area.X = Clamp(below.X, 0.0f, Max(room.X - size.X, 0.0f));
                area.Y = below.Bottom() + 4.0f;
                if (area.Y + size.Y > room.Y && below.Y - 4.0f - size.Y >= 0.0f)
                {
                    area.Y = below.Y - 4.0f - size.Y;
                }
            }
            overlay.Area = area;
            Place(element, context, area);
        }
        LayoutDirty = false;
    }

    void RootState::Draw(const Canvas& canvas, float deltaSeconds)
    {
        if (!canvas)
        {
            return;
        }
        Vector2 size = canvas.Size();
        if (size != LastCanvasSize || canvas.Scale() != Scale)
        {
            LastCanvasSize = size;
            Scale = canvas.Scale();
            LayoutDirty = true;
        }
        for (Slot& slot : Slots)
        {
            if (slot.View == nullptr)
            {
                slot.Area = { 0.0f, 0.0f, size.X, size.Y };
            }
        }

        // Callbacks run while drawing may drop the last handle to the root.
        std::shared_ptr<RootState> self = shared_from_this();
        DropTypedText = false;
        Context context = MakeContext(deltaSeconds);
        StepTheme(deltaSeconds);
        context.Font = CurrentTheme.Font ? CurrentTheme.Font : SystemFont();
        ProcessChanges();

        // The keyboard leaves an element that was hidden or disabled.
        if (std::shared_ptr<ElementState> focused = Focus.lock(); focused && !FocusedElement())
        {
            Focus.reset();
            focused->Focused = false;
            focused->Kind->FocusChanged(*focused, context, false);
        }
        std::vector<Slot> slots = Slots;
        for (Slot& slot : slots)
        {
            UpdateElements(*slot.Element, context);
        }
        std::vector<std::shared_ptr<ElementState>> overlays;
        for (const Overlay& overlay : Overlays)
        {
            overlays.push_back(overlay.Element);
        }
        for (const std::shared_ptr<ElementState>& overlay : overlays)
        {
            UpdateElements(*overlay, context);
        }

        // Tooltips show after the pointer rests on an element that has one.
        std::shared_ptr<ElementState> hovered = Hover.lock();
        ElementState* withTip = hovered.get();
        while (withTip && withTip->CurrentStyle().Tooltip.empty())
        {
            withTip = withTip->Parent;
        }
        if (withTip && PointerInside && !CapturedElement())
        {
            RestSeconds += deltaSeconds;
            TooltipElement = withTip->weak_from_this();
            TooltipShown = RestSeconds >= TooltipDelay;
        }
        else
        {
            TooltipShown = false;
        }

        ProcessChanges();
        if (LayoutDirty)
        {
            LayOut(context);
        }

        DrawContext drawing;
        static_cast<Context&>(drawing) = context;
        drawing.Canvas = canvas;
        drawing.FocusVisible = FocusVisible;

        // Copies, since drawing code may change what the root shows.
        slots = Slots;
        std::vector<Overlay> overlaysDrawn = Overlays;
        for (Slot& slot : slots)
        {
            if (Shown(slot))
            {
                DrawElement(*slot.Element, drawing);
            }
        }
        for (Overlay& overlay : overlaysDrawn)
        {
            if (overlay.Modal)
            {
                canvas.Rectangle({ .Size = size, .Color = Color { 0.0f, 0.0f, 0.0f, 0.4f } });
            }
            DrawElement(*overlay.Element, drawing);
        }
        DrawDrag(drawing);
        DrawTooltip(drawing);
    }

    void RootState::DrawDrag(DrawContext& context)
    {
        std::shared_ptr<ElementState> source = DragSource.lock();
        if (!Dragging || !source || source->Root() != this)
        {
            return;
        }
        const Canvas& canvas = context.Canvas;
        if (std::shared_ptr<ElementState> target = DropTarget.lock(); target && target->Root() == this)
        {
            float radius = target->CurrentStyle().CornerRadius.value_or(context.Theme->CornerRadius);
            DrawRing(canvas, target->Frame, radius, 2.0f, 2.0f, context.Theme->Accent);
        }

        // A faded copy of what is dragged follows the pointer.
        Rectangle frame = source->Frame;
        canvas.PushTransform(PointerPosition - DragStart);
        canvas.BeginLayer({ frame.X - 8.0f, frame.Y - 8.0f, frame.Width + 16.0f, frame.Height + 16.0f });
        DrawElement(*source, context);
        canvas.EndLayer({ .Opacity = 0.6f });
        canvas.PopTransform();
    }

    void RootState::CancelDrag()
    {
        Dragging = false;
        DragSource.reset();
        DropTarget.reset();
    }

    ElementState* RootState::FindAt(Vector2 point, const std::function<bool(ElementState&)>& wanted)
    {
        for (auto overlay = Overlays.rbegin(); overlay != Overlays.rend(); ++overlay)
        {
            if (overlay->Area.Contains(point))
            {
                return FindIn(*overlay->Element, point, wanted);
            }
            if (overlay->Modal)
            {
                return nullptr;
            }
        }
        for (auto slot = Slots.rbegin(); slot != Slots.rend(); ++slot)
        {
            if (!slot->Area.Contains(point) && slot->View != nullptr)
            {
                continue;
            }
            return FindIn(*slot->Element, point, wanted);
        }
        return nullptr;
    }

    void RootState::DrawTooltip(DrawContext& context)
    {
        std::shared_ptr<ElementState> element = TooltipElement.lock();
        if (!TooltipShown || !element || element->Root() != this)
        {
            return;
        }
        const std::string& text = element->CurrentStyle().Tooltip;
        const ui::Theme& theme = *context.Theme;
        float size = Max(theme.FontSize - 2.0f, 9.0f);
        Vector2 textSize = context.Font ? context.Font.Measure(text, size) : Vector2 {};
        Vector2 box { textSize.X + 16.0f, textSize.Y + 10.0f };
        Vector2 corner = PointerPosition + Vector2 { 12.0f, 20.0f };
        Vector2 canvasSize = context.Canvas.Size();
        corner.X = Clamp(corner.X, 0.0f, Max(canvasSize.X - box.X, 0.0f));
        if (corner.Y + box.Y > canvasSize.Y)
        {
            corner.Y = PointerPosition.Y - box.Y - 8.0f;
        }
        context.Canvas.Rectangle({ .Position = corner + Vector2 { 0.0f, 2.0f }, .Size = box,
            .Color = Color { 0.0f, 0.0f, 0.0f, 0.25f }, .CornerRadius = 4.0f, .Blur = 6.0f });
        context.Canvas.Rectangle({ .Position = corner, .Size = box, .Color = theme.Tooltip, .CornerRadius = 4.0f });
        DrawTextIn(context.Canvas, context.Font, text, size, theme.TooltipText,
            { corner.X + 8.0f, corner.Y + 5.0f, textSize.X, textSize.Y }, TextAlignment::Start, false);
    }

    void RootState::FrameEnded()
    {
        if (!TheRenderer || !TheHost || !TheHost->IsOpen())
        {
            return;
        }
        std::shared_ptr<RootState> self = shared_from_this();
        Color clear = TheHost->IsTransparent() ? Color::Transparent : CurrentTheme.Background;
        Renderer renderer = TheRenderer;
        Canvas canvas = renderer.BeginFrame(clear);
        Drawing = true;
        Draw(canvas, PendingDelta);
        Drawing = false;
        renderer.EndFrame();
        PendingDelta = 0.0f;
        if (ReleaseRendererAfterFrame)
        {
            ReleaseRendererAfterFrame = false;
            if (Slots.empty())
            {
                TheRenderer = Renderer();
            }
        }
    }

    // ---- Events ----------------------------------------------------------------------

    ElementState* RootState::HitTest(Vector2 point, bool, const Slot** found)
    {
        Context context = MakeContext();
        for (auto overlay = Overlays.rbegin(); overlay != Overlays.rend(); ++overlay)
        {
            if (overlay->Area.Contains(point))
            {
                ElementState* hit = HitElement(*overlay->Element, point, context);
                return hit ? hit : overlay->Element.get();
            }
            if (overlay->Modal)
            {
                return nullptr;
            }
        }
        for (auto slot = Slots.rbegin(); slot != Slots.rend(); ++slot)
        {
            if (!slot->Area.Contains(point) && slot->View != nullptr)
            {
                continue;
            }
            if (found)
            {
                *found = &*slot;
            }
            return HitElement(*slot->Element, point, context);
        }
        return nullptr;
    }

    void RootState::UpdateHover(Vector2 point, Context& context)
    {
        ElementState* hit = PointerInside ? HitTest(point, false) : nullptr;
        std::shared_ptr<ElementState> old = Hover.lock();
        if (hit != old.get())
        {
            if (old)
            {
                old->Hovered = false;
            }
            if (hit)
            {
                hit->Hovered = true;
                Hover = hit->weak_from_this();
            }
            else
            {
                Hover.reset();
            }
            RestSeconds = 0.0f;
            TooltipShown = false;
        }
        if (hit && hit != CapturedElement())
        {
            Pointer pointer;
            pointer.Position = Unmove(*hit, point);
            hit->Kind->PointerMoved(*hit, context, pointer);
        }
    }

    void RootState::ApplyCursor()
    {
        if (!TheHost)
        {
            return;
        }
        std::optional<easyforge::Cursor> wanted;
        if (Dragging)
        {
            wanted = DropTarget.lock() ? easyforge::Cursor::Hand : easyforge::Cursor::Move;
        }
        ElementState* over = CapturedElement();
        std::shared_ptr<ElementState> hovered = Hover.lock();
        if (!over)
        {
            over = hovered.get();
        }
        for (ElementState* element = over; element && !wanted; element = element->Parent)
        {
            wanted = element->CurrentStyle().Cursor;
            if (!wanted && element->IsEnabled())
            {
                wanted = element->Kind->PointerCursor(*element);
            }
        }
        if (wanted != AppliedCursor)
        {
            TheHost->SetCursor(wanted.value_or(easyforge::Cursor::Arrow));
            AppliedCursor = wanted;
        }
    }

    void RootState::MoveFocus(bool backward)
    {
        std::vector<ElementState*> focusable;
        auto modal = std::find_if(Overlays.rbegin(), Overlays.rend(), [](const Overlay& overlay) { return overlay.Modal; });
        if (modal != Overlays.rend())
        {
            CollectFocusable(*modal->Element, focusable);
        }
        else
        {
            for (Slot& slot : Slots)
            {
                CollectFocusable(*slot.Element, focusable);
            }
        }
        if (focusable.empty())
        {
            return;
        }
        ElementState* current = FocusedElement();
        auto found = std::find(focusable.begin(), focusable.end(), current);
        std::size_t index = 0;
        if (found == focusable.end())
        {
            index = backward ? focusable.size() - 1 : 0;
        }
        else
        {
            std::size_t at = static_cast<std::size_t>(found - focusable.begin());
            index = backward ? (at + focusable.size() - 1) % focusable.size() : (at + 1) % focusable.size();
        }
        SetFocus(focusable[index], true);
    }

    bool RootState::Dispatch(Event& event)
    {
        // A callback may close the window or drop the last handle to the root.
        std::shared_ptr<RootState> self = shared_from_this();
        Context context = MakeContext();
        bool used = false;
        auto pointerFrom = [&](const ElementState& element, bool touch) {
            Pointer pointer;
            pointer.Position = Unmove(element, event.Position);
            pointer.ClickCount = event.ClickCount;
            pointer.Modifiers = event.Modifiers;
            pointer.Touch = touch;
            return pointer;
        };
        auto press = [&](bool touch) {
            PointerPosition = event.Position;
            PointerInside = true;
            CancelDrag();
            if (!Overlays.empty() && !Overlays.back().Area.Contains(event.Position))
            {
                const Overlay& top = Overlays.back();
                if (top.ClosesOnOutsideClick)
                {
                    CloseOverlay(top.Element.get());
                    used = true;
                    return;
                }
                if (top.Modal)
                {
                    used = true;
                    return;
                }
            }
            UpdateHover(event.Position, context);
            ElementState* target = HitTest(event.Position, true);
            if (!target)
            {
                SetFocus(nullptr, false);
                return;
            }
            used = true;
            TooltipShown = false;
            RestSeconds = -10.0f;
            if (!touch && event.Button != MouseButton::Left)
            {
                return;
            }

            // An element with drag text can be picked up from anywhere in it.
            for (ElementState* source = target; source; source = source->Parent)
            {
                if (HasDragText(*source))
                {
                    if (source->IsEnabled())
                    {
                        DragSource = source->weak_from_this();
                        DragStart = event.Position;
                    }
                    break;
                }
            }

            // The press goes to the nearest element under the pointer that takes
            // it, so a picture inside a button presses the button.
            ElementState* taker = target;
            while (taker && !taker->Kind->TakesPointerAt(*taker, Unmove(*taker, event.Position)))
            {
                taker = taker->Parent;
            }
            if (!taker || !taker->IsEnabled())
            {
                SetFocus(nullptr, false);
                return;
            }
            target = taker;
            std::shared_ptr<ElementState> keptTarget = target->shared_from_this();

            // Another element still holding the pointer, as with a second finger,
            // is let go first.
            if (ElementState* held = CapturedElement(); held && held != target)
            {
                std::shared_ptr<ElementState> keptHeld = held->shared_from_this();
                held->Pressed = false;
                Capture.reset();
                held->Kind->PointerReleased(*held, context, pointerFrom(*held, touch), false);
            }
            Capture = target->weak_from_this();
            target->Pressed = true;
            ElementState* focusable = target;
            while (focusable && !focusable->Kind->TakesKeyboard(*focusable))
            {
                focusable = focusable->Parent;
            }
            // A popup's rows leave the keyboard with what opened the popup.
            bool inOverlay = std::any_of(Overlays.begin(), Overlays.end(),
                [&](const Overlay& overlay) { return IsInside(target, overlay.Element.get()); });
            if (focusable || !inOverlay)
            {
                SetFocus(focusable, false);
            }
            target->Kind->PointerPressed(*target, context, pointerFrom(*target, touch));
        };
        auto release = [&](bool touch) {
            if (!touch && event.Button != MouseButton::Left)
            {
                used = CapturedElement() || Dragging || HitTest(event.Position, false) != nullptr;
                return;
            }
            if (Dragging)
            {
                std::shared_ptr<ElementState> source = DragSource.lock();
                std::shared_ptr<ElementState> target = DropTarget.lock();
                std::string text = source ? source->Get("DragText").AsText() : std::string();
                CancelDrag();
                if (target && target->Root() == this)
                {
                    std::function<void(const std::string&)> drop = target->Object<std::function<void(const std::string&)>>("OnDrop");
                    if (drop)
                    {
                        drop(text);
                    }
                }
                used = true;
                if (touch)
                {
                    PointerInside = false;
                    UpdateHover(event.Position, context);
                }
                return;
            }
            DragSource.reset();
            ElementState* captured = CapturedElement();
            if (!captured)
            {
                used = HitTest(event.Position, false) != nullptr;
                if (touch)
                {
                    PointerInside = false;
                    UpdateHover(event.Position, context);
                }
                return;
            }
            std::shared_ptr<ElementState> kept = captured->shared_from_this();
            ElementState* over = HitTest(event.Position, false);
            bool inside = over && IsInside(over, captured);
            captured->Pressed = false;
            Capture.reset();
            captured->Kind->PointerReleased(*captured, context, pointerFrom(*captured, touch), inside);
            used = true;
            if (touch)
            {
                PointerInside = false;
                UpdateHover(event.Position, context);
            }
        };
        auto move = [&](bool touch) {
            PointerPosition = event.Position;
            PointerInside = true;
            std::shared_ptr<ElementState> source = DragSource.lock();
            if (source && !Dragging && Distance(event.Position, DragStart) > DragDistance)
            {
                // The element pressed lets go without a click.
                Dragging = true;
                TooltipShown = false;
                if (ElementState* captured = CapturedElement())
                {
                    std::shared_ptr<ElementState> kept = captured->shared_from_this();
                    captured->Pressed = false;
                    Capture.reset();
                    captured->Kind->PointerReleased(*captured, context, pointerFrom(*captured, touch), false);
                }
            }
            if (Dragging)
            {
                used = true;
                if (!source || source->Root() != this)
                {
                    CancelDrag();
                    return;
                }
                ElementState* target = FindAt(event.Position, [&](ElementState& element) {
                    return !IsInside(&element, source.get()) &&
                           static_cast<bool>(element.Object<std::function<void(const std::string&)>>("OnDrop"));
                });
                DropTarget = target ? target->weak_from_this() : std::weak_ptr<ElementState>();
                return;
            }
            if (ElementState* captured = CapturedElement())
            {
                captured->Kind->PointerMoved(*captured, context, pointerFrom(*captured, touch));
                used = true;
            }
            UpdateHover(event.Position, context);
        };

        switch (event.Type)
        {
        case EventType::MouseMoved: move(false); break;
        case EventType::MouseButtonPressed: press(false); break;
        case EventType::MouseButtonReleased: release(false); break;
        case EventType::TouchBegan: press(true); break;
        case EventType::TouchMoved: move(true); break;
        case EventType::TouchEnded:
        case EventType::TouchCancelled: release(true); break;
        case EventType::MouseWheel:
        {
            ElementState* over = HitTest(event.Position, false);
            for (ElementState* element = over; element; element = element->Parent)
            {
                if (element->IsEnabled() && element->Kind->Wheel(*element, context, event.Wheel))
                {
                    used = true;
                    break;
                }
            }
            used = used || over != nullptr;
            break;
        }
        case EventType::MouseLeft:
            PointerInside = false;
            UpdateHover(event.Position, context);
            break;
        case EventType::KeyPressed:
        {
            if (event.Key == Key::Escape && Dragging)
            {
                CancelDrag();
                used = true;
                break;
            }

            // Escape closes the popup or dialog on top.
            if (event.Key == Key::Escape && !Overlays.empty())
            {
                CloseOverlay(Overlays.back().Element.get());
                used = true;
                break;
            }

            // The focused element first, then the elements around it, so a scroll
            // area answers Page Down while something in it has the keyboard.
            ElementState* focused = FocusedElement();
            std::shared_ptr<ElementState> focusedBefore = Focus.lock();
            std::shared_ptr<ElementState> current = focused ? focused->shared_from_this() : nullptr;
            while (current && !used)
            {
                used = current->IsEnabled() && current->Kind->KeyPressed(*current, context, event);
                current = !used && current->Parent ? current->Parent->shared_from_this() : nullptr;
            }
            focused = FocusedElement();
            if (!used && event.Key == Key::Tab)
            {
                MoveFocus(event.Modifiers.Shift);
                used = true;
            }
            else if (!used && event.Key == Key::Escape && focused)
            {
                SetFocus(nullptr, false);
                used = true;
            }
            DropTypedText = Focus.lock() != focusedBefore;
            break;
        }
        case EventType::KeyReleased:
            used = FocusedElement() != nullptr;
            break;
        case EventType::TextEntered:
        case EventType::TextComposition:
            // Space on a button that moved the keyboard to a field is not typed there.
            if (event.Type == EventType::TextEntered && DropTypedText)
            {
                DropTypedText = false;
                used = true;
                break;
            }
            if (ElementState* focused = FocusedElement(); focused && focused->IsEnabled())
            {
                used = focused->Kind->TextEntered(*focused, context, event);
            }
            break;
        case EventType::FilesDropped:
        {
            ElementState* target = FindAt(event.Position, [](ElementState& element) {
                return static_cast<bool>(element.Object<std::function<void(const std::vector<std::string>&)>>("OnFilesDropped"));
            });
            if (target)
            {
                std::shared_ptr<ElementState> kept = target->shared_from_this();
                std::function<void(const std::vector<std::string>&)> dropped =
                    target->Object<std::function<void(const std::vector<std::string>&)>>("OnFilesDropped");
                dropped(event.Files);
                used = true;
            }
            break;
        }
        case EventType::FocusLost:
            CancelDrag();
            if (ElementState* captured = CapturedElement())
            {
                captured->Pressed = false;
                Capture.reset();
                captured->Kind->PointerReleased(*captured, context, pointerFrom(*captured, false), false);
            }
            break;
        case EventType::ColorSchemeChanged:
            if (FollowSystem && TheHost)
            {
                SetTheme(ui::Theme::For(TheHost->SystemColorScheme()), false);
            }
            break;
        case EventType::Resized:
        case EventType::ScaleChanged:
            if (TheHost)
            {
                Scale = TheHost->Scale();
            }
            LayoutDirty = true;
            break;
        default: break;
        }
        ApplyCursor();
        if (used)
        {
            event.Handled = true;
        }
        return used;
    }

    HitArea RootState::TitleBarHitTest(const SlotView& view, Vector2 point)
    {
        for (Slot& slot : Slots)
        {
            if (slot.View != &view)
            {
                continue;
            }
            slot.TitleBar = true;
            if (!slot.Area.Contains(point))
            {
                return HitArea::Content;
            }
            Context context = MakeContext();
            ElementState* hit = HitElement(*slot.Element, point, context);
            for (ElementState* element = hit; element; element = element->Parent)
            {
                if (std::optional<HitArea> area = element->Kind->TitleBarArea(*element, Unmove(*element, point)))
                {
                    return *area;
                }
                if (element->Kind->TakesPointerAt(*element, Unmove(*element, point)))
                {
                    return HitArea::Content;
                }
            }
            return HitArea::Caption;
        }
        return HitArea::Content;
    }

    Vector2 RootState::PreferredSize(const SlotView& view, Vector2 available)
    {
        for (Slot& slot : Slots)
        {
            if (slot.View != &view)
            {
                continue;
            }
            slot.TitleBar = true;
            Context context = MakeContext();
            ++LayoutPass;
            context.Pass = LayoutPass;
            LayoutDirty = true;
            Vector2 size = Measure(*slot.Element, context, { available.X, Unlimited }, true, false);
            const Insets& margin = slot.Element->CurrentStyle().Margin;
            slot.WantedHeight = size.Y + margin.Vertical();
            return { available.X, slot.WantedHeight };
        }
        return available;
    }

    // ---- Window slots ------------------------------------------------------------------

    SlotView::~SlotView()
    {
        if (Root)
        {
            Root->RemoveSlot(*this);
        }
    }

    void SlotView::Attach(Host& host)
    {
        Root = RootState::For(host);
        Root->TheHost = &host;
        Root->AddSlot(*this, Element);
    }

    void SlotView::Detach()
    {
        if (Root)
        {
            Root->RemoveSlot(*this);
            Root.reset();
        }
    }

    void SlotView::Place(Rectangle area)
    {
        if (Root)
        {
            Root->PlaceSlot(*this, area);
        }
    }

    void SlotView::HandleEvent(Event& event)
    {
        // Every slot a window shows sees each event; the first handles it for all.
        std::shared_ptr<RootState> root = Root;
        if (!root)
        {
            return;
        }
        auto first = std::find_if(root->Slots.begin(), root->Slots.end(), [](const RootState::Slot& slot) { return RootState::Shown(slot); });
        if (first != root->Slots.end() && first->View == this)
        {
            root->Dispatch(event);
        }
    }

    void SlotView::Frame(float deltaSeconds)
    {
        if (Root)
        {
            Root->PendingDelta = deltaSeconds;
        }
    }

    Vector2 SlotView::PreferredSize(Vector2 available) const
    {
        return Root ? Root->PreferredSize(*this, available) : available;
    }

    HitArea SlotView::HitTest(Vector2 point) const
    {
        return Root ? Root->TitleBarHitTest(*this, point) : HitArea::Content;
    }

    namespace
    {
        void StartRootAnimation(const void* owner, std::string_view property,
            std::function<void(void* owner, float progress)> step, const AnimationSettings& settings)
        {
            RootState* root = static_cast<const std::shared_ptr<RootState>*>(owner)->get();
            if (!root)
            {
                return;
            }
            std::erase_if(root->ThemeAnimations, [&](const Animation& animation) { return animation.Property == property; });
            root->ThemeAnimations.push_back({ std::string(property), std::move(step), settings, 0.0f });
        }

        RootState* RootOf(const void* owner)
        {
            return static_cast<const std::shared_ptr<RootState>*>(owner)->get();
        }
    }

    const std::shared_ptr<RootState>& StateOfRoot(const Root& root)
    {
        return root.State;
    }

    std::shared_ptr<ElementState> FindInRoot(const RootState* root, std::string_view name, std::string_view kind)
    {
        if (!root)
        {
            return nullptr;
        }
        std::vector<std::shared_ptr<ElementState>> tops;
        for (const RootState::Slot& slot : root->Slots)
        {
            tops.push_back(slot.Element);
        }
        for (const RootState::Overlay& overlay : root->Overlays)
        {
            tops.push_back(overlay.Element);
        }
        for (const std::shared_ptr<ElementState>& top : tops)
        {
            if (top->Row.Name() == name && (kind.empty() || top->Kind->Name() == kind))
            {
                return top;
            }
            if (std::shared_ptr<ElementState> found = FindByName(*top, name, kind))
            {
                return found;
            }
        }
        return nullptr;
    }
}

namespace easyforge::ui
{
    using namespace internal;

    Root::Root() : Root(std::shared_ptr<RootState>())
    {
    }

    Root::Root(std::shared_ptr<internal::RootState> state)
        : Content(
              &State,
              [](const void* owner) -> Element {
                  RootState* root = RootOf(owner);
                  ElementState* content = root ? root->ContentElement() : nullptr;
                  return content ? Element(content->shared_from_this()) : Element();
              },
              [](void* owner, const Element& element) {
                  RootState* root = RootOf(owner);
                  if (!root)
                  {
                      return;
                  }
                  if (root->TheHost)
                  {
                      Log(LogLevel::Warning, "a window's root shows the window's Content; assign window.Content instead");
                      return;
                  }
                  root->SetContent(StateOfHandle(element));
              }),
          Theme(
              &State,
              [](const void* owner) -> ui::Theme {
                  RootState* root = RootOf(owner);
                  return root ? root->CurrentTheme : ui::Theme::Light();
              },
              [](void* owner, const ui::Theme& theme) {
                  if (RootState* root = RootOf(owner))
                  {
                      root->SetTheme(theme, true);
                  }
              },
              "Theme", &StartRootAnimation),
          State(std::move(state))
    {
    }

    Root::Root(const Root& other) : Root(other.State)
    {
    }

    Root& Root::operator=(const Root& other)
    {
        State = other.State;
        return *this;
    }

    Root::~Root() = default;

    Root Root::New(const RootSettings& settings)
    {
        auto state = std::make_shared<RootState>();
        state->FollowSystem = false;
        if (settings.Theme)
        {
            state->CurrentTheme = *settings.Theme;
        }
        if (settings.Content)
        {
            state->SetContent(StateOfHandle(settings.Content));
        }
        return Root(state);
    }

    Root Root::Of(const std::shared_ptr<Host>& host)
    {
        if (!host)
        {
            return Root();
        }
        return Root(RootState::For(*host));
    }

    Root::operator bool() const
    {
        return State != nullptr;
    }

    bool Root::HandleEvent(Event& event) const
    {
        return State ? State->Dispatch(event) : false;
    }

    void Root::Draw(const Canvas& canvas, float deltaSeconds) const
    {
        if (State)
        {
            State->Draw(canvas, deltaSeconds);
        }
    }

    Element Root::Focused() const
    {
        ElementState* focused = State ? State->FocusedElement() : nullptr;
        return focused ? Element(focused->shared_from_this()) : Element();
    }

    Table Root::Data() const
    {
        return State ? State->Interface->Elements : Table();
    }
}
