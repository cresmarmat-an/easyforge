#include <easyforge/ui/Elements.h>

#include "Behavior.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    namespace
    {
        class DrawingAreaBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return DrawingArea::KindName; }

            void Draw(ElementState& element, DrawContext& context) override
            {
                DrawFunction drawing = element.Object<DrawFunction>("OnDraw");
                if (!drawing)
                {
                    return;
                }
                const Style& style = element.CurrentStyle();
                Rectangle content { element.Frame.X + style.Padding.Left, element.Frame.Y + style.Padding.Top,
                    Max(element.Frame.Width - style.Padding.Horizontal(), 0.0f),
                    Max(element.Frame.Height - style.Padding.Vertical(), 0.0f) };
                Canvas canvas = context.Canvas;
                canvas.PushClip(content);
                canvas.PushTransform(content.Position());
                std::shared_ptr<ElementState> kept = element.shared_from_this();
                drawing(canvas, content.Size());
                canvas.PopTransform();
                canvas.PopClip();
            }
        };

        class SceneViewBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return SceneView::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Width" || property == "Height")
                {
                    return DataValue("fill");
                }
                return Behavior::Default(property);
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                easyforge::Scene scene = element.Object<easyforge::Scene>("Scene");
                if (scene && !element.Frame.IsEmpty())
                {
                    context.Canvas.Draw(scene, element.Frame);
                }
            }
        };
    }
}

namespace easyforge::ui
{
    using namespace internal;

    DrawingArea::DrawingArea(const DrawingAreaSettings& settings)
        : DrawingArea(MakeElement(std::make_unique<DrawingAreaBehavior>()))
    {
        ApplyCommonSettings(*State, settings);
        if (settings.OnDraw)
        {
            State->SetObject("OnDraw", settings.OnDraw);
        }
    }

    DrawingArea::DrawingArea(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), OnDraw(Kept<DrawFunction, "OnDraw">(State))
    {
    }

    DrawingArea::DrawingArea(const DrawingArea& other) : DrawingArea(other.State)
    {
    }

    DrawingArea& DrawingArea::operator=(const DrawingArea& other)
    {
        State = other.State;
        return *this;
    }

    SceneView::SceneView(const easyforge::Scene& scene, const SceneViewSettings& settings)
        : SceneView(MakeElement(std::make_unique<SceneViewBehavior>()))
    {
        ApplyCommonSettings(*State, settings);
        State->SetObject("Scene", scene);
    }

    SceneView::SceneView(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Scene(Kept<easyforge::Scene, "Scene">(State))
    {
    }

    SceneView::SceneView(const SceneView& other) : SceneView(other.State)
    {
    }

    SceneView& SceneView::operator=(const SceneView& other)
    {
        State = other.State;
        return *this;
    }
}
