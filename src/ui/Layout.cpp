#include <easyforge/ui/Layout.h>

#include <algorithm>
#include <cmath>
#include <format>

#include <easyforge/core/Log.h>

#include "Behavior.h"
#include "Containers.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    // ---- Conversions ------------------------------------------------------------

    DataValue SizeToData(Size size)
    {
        switch (size.Type)
        {
        // No value: the kind's own default applies.
        case Size::Kind::Default: return DataValue();
        case Size::Kind::Fit: return DataValue("fit");
        case Size::Kind::Points: return DataValue(size.Amount);
        case Size::Kind::Percent: return DataValue(std::format("{}%", size.Amount));
        case Size::Kind::Fill: return DataValue("fill");
        }
        return DataValue("fit");
    }

    Size SizeFromData(const DataValue& value)
    {
        if (value.Type() == DataType::Integer || value.Type() == DataType::Number)
        {
            return Size(value.As<float>());
        }
        std::string text = value.AsText();
        if (text == "fill")
        {
            return Fill;
        }
        if (!text.empty() && text.back() == '%')
        {
            bool valid = false;
            DataValue amount = DataValue::FromText(std::string_view(text).substr(0, text.size() - 1), &valid);
            if (valid)
            {
                return Percent(amount.As<float>());
            }
        }
        return Fit;
    }

    // ---- What every kind shares ----------------------------------------------------

    DataValue Behavior::Default(std::string_view property) const
    {
        if (property == "Width" || property == "Height")
        {
            return DataValue("fit");
        }
        if (property == "MaximumWidth" || property == "MaximumHeight")
        {
            return DataValue(static_cast<double>(Unlimited));
        }
        if (property == "MinimumWidth" || property == "MinimumHeight" || property == "BorderWidth" ||
            property == "Gap" || property == "FontSize")
        {
            return DataValue(0.0);
        }
        if (property == "Margin" || property == "Padding")
        {
            return DataValue(Vector4 {});
        }
        if (property == "Opacity" || property == "Scale")
        {
            return DataValue(1.0);
        }
        if (property == "Offset")
        {
            return DataValue(Vector2 {});
        }
        if (property == "Visible" || property == "Enabled")
        {
            return DataValue(true);
        }
        if (property == "Wrap")
        {
            return DataValue(false);
        }
        if (property == "Tooltip" || property == "Text" || property == "DragText")
        {
            return DataValue("");
        }
        if (property == "Alignment")
        {
            return DataValue("stretch");
        }
        if (property == "Distribution" || property == "TextAlignment")
        {
            return DataValue("start");
        }
        return DataValue();
    }

    Vector2 Behavior::MeasureContent(ElementState& element, Context& context, const Constraint& constraint)
    {
        return HoldsChildren() ? MeasureLayers(element, context, constraint) : Vector2 {};
    }

    void Behavior::Arrange(ElementState& element, Context& context, Rectangle content)
    {
        ArrangeLayers(element, context, content, Alignment::Stretch);
    }

    void Behavior::VisibleChildren(ElementState& element, std::vector<ElementState*>& into)
    {
        for (const std::shared_ptr<ElementState>& child : element.Children)
        {
            if (child->CurrentStyle().Visible)
            {
                into.push_back(child.get());
            }
        }
    }

    Box Behavior::LookOf(ElementState& element, const Context& context)
    {
        const Style& style = element.CurrentStyle();
        Box box;
        box.Background = style.Background;
        box.CornerRadius = style.CornerRadius.value_or(0.0f);
        box.BorderWidth = style.BorderWidth;
        box.BorderColor = style.BorderColor.value_or(context.Theme->Border);
        return box;
    }

    Color Mix(Color from, Color to, float amount)
    {
        return Lerp(from, to, Clamp(amount, 0.0f, 1.0f));
    }

    easyforge::Font FontOf(ElementState& element, const Context& context)
    {
        easyforge::Font font = element.Object<easyforge::Font>("Font");
        return font ? font : context.Font;
    }

    float FontSizeOf(ElementState& element, const Context& context)
    {
        float size = element.Get("FontSize").As<float>();
        return size > 0.0f ? size : context.Theme->FontSize;
    }

    // ---- Measuring and placing ------------------------------------------------------

    namespace
    {
        bool Finite(float value)
        {
            return std::isfinite(value);
        }

        std::vector<ElementState*> LaidOutChildren(ElementState& element)
        {
            std::vector<ElementState*> children;
            for (const std::shared_ptr<ElementState>& child : element.Children)
            {
                if (child->CurrentStyle().Visible)
                {
                    children.push_back(child.get());
                }
            }
            return children;
        }

        float Main(Vector2 value, bool horizontal)
        {
            return horizontal ? value.X : value.Y;
        }

        float Cross(Vector2 value, bool horizontal)
        {
            return horizontal ? value.Y : value.X;
        }

        Vector2 Join(float main, float cross, bool horizontal)
        {
            return horizontal ? Vector2 { main, cross } : Vector2 { cross, main };
        }

        bool FillsAlong(ElementState& child, bool horizontal)
        {
            const Style& style = child.CurrentStyle();
            return (horizontal ? style.Width : style.Height).Type == Size::Kind::Fill;
        }

        // Shares of the space left for the children that fill, given in rounds: a
        // child held back by its minimum or maximum keeps that size, and the
        // others share what remains again.
        std::vector<float> FillShares(const std::vector<ElementState*>& children, const std::vector<bool>& fills, float left,
            bool horizontal)
        {
            std::vector<float> shares(children.size(), 0.0f);
            std::vector<bool> settled(children.size(), false);
            float remaining = Max(left, 0.0f);
            for (std::size_t round = 0; round <= children.size(); ++round)
            {
                std::size_t open = 0;
                for (std::size_t index = 0; index < children.size(); ++index)
                {
                    open += fills[index] && !settled[index] ? 1 : 0;
                }
                if (open == 0)
                {
                    break;
                }
                float share = remaining / static_cast<float>(open);
                bool held = false;
                for (std::size_t index = 0; index < children.size(); ++index)
                {
                    if (!fills[index] || settled[index])
                    {
                        continue;
                    }
                    const Style& style = children[index]->CurrentStyle();
                    float minimum = horizontal ? style.MinimumWidth : style.MinimumHeight;
                    float maximum = horizontal ? style.MaximumWidth : style.MaximumHeight;
                    float clamped = Clamp(share, minimum, Max(maximum, minimum));
                    if (clamped != share)
                    {
                        shares[index] = clamped;
                        settled[index] = true;
                        remaining = Max(remaining - clamped, 0.0f);
                        held = true;
                    }
                }
                if (!held)
                {
                    for (std::size_t index = 0; index < children.size(); ++index)
                    {
                        if (fills[index] && !settled[index])
                        {
                            shares[index] = share;
                        }
                    }
                    break;
                }
            }
            return shares;
        }
    }

    Vector2 Measure(ElementState& element, Context& context, Vector2 available, bool fillWidth, bool fillHeight)
    {
        const Style& style = element.CurrentStyle();
        if (!style.Visible)
        {
            return {};
        }
        ElementState::Measurement& last = element.LastMeasurement;
        if (last.Pass == context.Pass && last.Available == available && last.FillWidth == fillWidth &&
            last.FillHeight == fillHeight)
        {
            return last.Result;
        }

        Vector2 space { Max(available.X - style.Margin.Horizontal(), 0.0f), Max(available.Y - style.Margin.Vertical(), 0.0f) };
        auto resolve = [](Size size, float room, bool fill, float& result) {
            switch (size.Type)
            {
            case Size::Kind::Points: result = size.Amount; return true;
            case Size::Kind::Percent:
                if (Finite(room))
                {
                    result = room * size.Amount / 100.0f;
                    return true;
                }
                return false;
            case Size::Kind::Fill:
                if (fill && Finite(room))
                {
                    result = room;
                    return true;
                }
                return false;
            case Size::Kind::Default:
            case Size::Kind::Fit: return false;
            }
            return false;
        };
        float width = 0.0f;
        float height = 0.0f;
        bool exactWidth = resolve(style.Width, space.X, fillWidth, width);
        bool exactHeight = resolve(style.Height, space.Y, fillHeight, height);
        width = Clamp(width, style.MinimumWidth, Max(style.MaximumWidth, style.MinimumWidth));
        height = Clamp(height, style.MinimumHeight, Max(style.MaximumHeight, style.MinimumHeight));

        if (!exactWidth || !exactHeight)
        {
            Constraint constraint;
            constraint.ExactWidth = exactWidth;
            constraint.ExactHeight = exactHeight;
            float roomX = exactWidth ? width : Min(space.X, style.MaximumWidth);
            float roomY = exactHeight ? height : Min(space.Y, style.MaximumHeight);
            constraint.Available = { Max(roomX - style.Padding.Horizontal(), 0.0f), Max(roomY - style.Padding.Vertical(), 0.0f) };
            Vector2 inside = element.Kind->MeasureContent(element, context, constraint);
            if (!exactWidth)
            {
                width = Clamp(inside.X + style.Padding.Horizontal(), style.MinimumWidth,
                    Max(style.MaximumWidth, style.MinimumWidth));
            }
            if (!exactHeight)
            {
                height = Clamp(inside.Y + style.Padding.Vertical(), style.MinimumHeight,
                    Max(style.MaximumHeight, style.MinimumHeight));
            }
        }

        Vector2 result { Max(width, 0.0f), Max(height, 0.0f) };
        last = { available, fillWidth, fillHeight, result, context.Pass };
        return result;
    }

    void Place(ElementState& element, Context& context, Rectangle box)
    {
        element.Frame = box;
        const Style& style = element.CurrentStyle();
        Rectangle content { box.X + style.Padding.Left, box.Y + style.Padding.Top,
            Max(box.Width - style.Padding.Horizontal(), 0.0f), Max(box.Height - style.Padding.Vertical(), 0.0f) };
        element.Kind->Arrange(element, context, content);
    }

    Vector2 MeasureLine(ElementState& element, Context& context, const Constraint& constraint, bool horizontal)
    {
        std::vector<ElementState*> children = LaidOutChildren(element);
        if (children.empty())
        {
            return {};
        }
        float gap = element.Get("Gap").As<float>();
        bool mainExact = horizontal ? constraint.ExactWidth : constraint.ExactHeight;
        bool crossExact = horizontal ? constraint.ExactHeight : constraint.ExactWidth;
        float mainRoom = Main(constraint.Available, horizontal);
        float crossRoom = Cross(constraint.Available, horizontal);

        // First each child at its own size, to find how far across the line goes.
        std::vector<Vector2> sizes(children.size());
        std::vector<bool> fills(children.size(), false);
        float naturalCross = 0.0f;
        for (std::size_t index = 0; index < children.size(); ++index)
        {
            ElementState& child = *children[index];
            fills[index] = FillsAlong(child, horizontal);
            sizes[index] = horizontal ? Measure(child, context, constraint.Available, false, crossExact)
                                      : Measure(child, context, constraint.Available, crossExact, false);
            const Insets& margin = child.CurrentStyle().Margin;
            naturalCross = Max(naturalCross, Cross({ sizes[index].X + margin.Horizontal(), sizes[index].Y + margin.Vertical() },
                                                 horizontal));
        }
        float crossSize = crossExact ? crossRoom : Min(naturalCross, crossRoom);

        // Then as ArrangeLine places them: across the line's size, and the children
        // that fill along it at their shares, so sizes that depend on the width,
        // such as wrapped text, come out the same.
        Vector2 room = Join(mainRoom, crossSize, horizontal);
        float main = gap * static_cast<float>(children.size() - 1);
        float widestFill = 0.0f;
        for (std::size_t index = 0; index < children.size(); ++index)
        {
            ElementState& child = *children[index];
            const Insets& margin = child.CurrentStyle().Margin;
            main += horizontal ? margin.Horizontal() : margin.Vertical();
            if (fills[index])
            {
                widestFill = Max(widestFill, Main(sizes[index], horizontal));
                continue;
            }
            sizes[index] = horizontal ? Measure(child, context, room, false, true) : Measure(child, context, room, true, false);
            main += Main(sizes[index], horizontal);
        }
        if (std::find(fills.begin(), fills.end(), true) != fills.end())
        {
            // A line that fits its content gives each child that fills as much as
            // the widest of them, since ArrangeLine shares the space equally.
            std::vector<float> shares = mainExact && Finite(mainRoom) ? FillShares(children, fills, mainRoom - main, horizontal)
                                                                      : std::vector<float>(children.size(), widestFill);
            for (std::size_t index = 0; index < children.size(); ++index)
            {
                if (!fills[index])
                {
                    continue;
                }
                ElementState& child = *children[index];
                const Insets& margin = child.CurrentStyle().Margin;
                Vector2 offer = horizontal ? Vector2 { shares[index] + margin.Horizontal(), room.Y }
                                           : Vector2 { room.X, shares[index] + margin.Vertical() };
                sizes[index] = Measure(child, context, offer, true, true);
                main += Main(sizes[index], horizontal);
            }
        }
        float cross = 0.0f;
        for (std::size_t index = 0; index < children.size(); ++index)
        {
            const Insets& margin = children[index]->CurrentStyle().Margin;
            cross = Max(cross, Cross({ sizes[index].X + margin.Horizontal(), sizes[index].Y + margin.Vertical() }, horizontal));
        }
        return Join(main, cross, horizontal);
    }

    void ArrangeLine(ElementState& element, Context& context, Rectangle content, bool horizontal, float offset, float sizeRoom)
    {
        std::vector<ElementState*> children = LaidOutChildren(element);
        if (children.empty())
        {
            return;
        }
        float gap = element.Get("Gap").As<float>();
        Alignment alignment = FromData<Alignment>(element.Get("Alignment"));
        Distribution distribution = FromData<Distribution>(element.Get("Distribution"));
        float mainRoom = Main(content.Size(), horizontal);
        float crossRoom = Cross(content.Size(), horizontal);
        Vector2 room = content.Size();
        if (sizeRoom >= 0.0f)
        {
            room = Join(sizeRoom, crossRoom, horizontal);
        }

        // Sizes along the line: fixed, shares, and content first; fills share the rest.
        std::vector<Vector2> sizes(children.size());
        std::vector<bool> fills(children.size(), false);
        float used = gap * static_cast<float>(children.size() - 1);
        int fillCount = 0;
        for (std::size_t index = 0; index < children.size(); ++index)
        {
            ElementState& child = *children[index];
            const Style& style = child.CurrentStyle();
            Size mainSize = horizontal ? style.Width : style.Height;
            float margins = horizontal ? style.Margin.Horizontal() : style.Margin.Vertical();
            if (mainSize.Type == Size::Kind::Fill)
            {
                fills[index] = true;
                ++fillCount;
                used += margins;
                continue;
            }
            sizes[index] = horizontal ? Measure(child, context, room, false, true) : Measure(child, context, room, true, false);
            used += Main(sizes[index], horizontal) + margins;
        }
        float left = mainRoom - used;
        if (fillCount > 0)
        {
            // Space a child cannot take because of its maximum goes to the others,
            // and what none can take is left for the distribution.
            std::vector<float> shares = FillShares(children, fills, left, horizontal);
            left = Max(left, 0.0f);
            for (std::size_t index = 0; index < children.size(); ++index)
            {
                if (!fills[index])
                {
                    continue;
                }
                ElementState& child = *children[index];
                const Insets& margin = child.CurrentStyle().Margin;
                Vector2 offer = horizontal ? Vector2 { shares[index] + margin.Horizontal(), room.Y }
                                           : Vector2 { room.X, shares[index] + margin.Vertical() };
                sizes[index] = Measure(child, context, offer, true, true);
                left -= Main(sizes[index], horizontal);
            }
            left = left > 0.5f ? left : 0.0f;
        }

        // Where the first child starts, and the extra space between children.
        float start = 0.0f;
        float between = gap;
        float count = static_cast<float>(children.size());
        if (left > 0.0f)
        {
            switch (distribution)
            {
            case Distribution::Start: break;
            case Distribution::Center: start = left * 0.5f; break;
            case Distribution::End: start = left; break;
            case Distribution::SpaceBetween:
                if (children.size() > 1)
                {
                    between += left / (count - 1.0f);
                }
                else
                {
                    start = left * 0.5f;
                }
                break;
            case Distribution::SpaceAround:
                start = left / count * 0.5f;
                between += left / count;
                break;
            case Distribution::SpaceEvenly:
                start = left / (count + 1.0f);
                between += left / (count + 1.0f);
                break;
            }
        }

        float position = Main(content.Position(), horizontal) + start + offset;
        for (std::size_t index = 0; index < children.size(); ++index)
        {
            ElementState& child = *children[index];
            const Style& style = child.CurrentStyle();
            float marginMainStart = horizontal ? style.Margin.Left : style.Margin.Top;
            float marginMainEnd = horizontal ? style.Margin.Right : style.Margin.Bottom;
            float marginCrossStart = horizontal ? style.Margin.Top : style.Margin.Left;
            float marginCrossEnd = horizontal ? style.Margin.Bottom : style.Margin.Right;

            float main = Main(sizes[index], horizontal);
            float cross = Cross(sizes[index], horizontal);
            Size crossSize = horizontal ? style.Height : style.Width;
            float crossSpace = Max(crossRoom - marginCrossStart - marginCrossEnd, 0.0f);
            if (alignment == Alignment::Stretch && crossSize.Type == Size::Kind::Fit)
            {
                float minimum = horizontal ? style.MinimumHeight : style.MinimumWidth;
                float maximum = horizontal ? style.MaximumHeight : style.MaximumWidth;
                cross = Clamp(crossSpace, minimum, Max(maximum, minimum));
            }

            float crossPosition = Cross(content.Position(), horizontal) + marginCrossStart;
            if (alignment == Alignment::Center)
            {
                crossPosition += (crossSpace - cross) * 0.5f;
            }
            else if (alignment == Alignment::End)
            {
                crossPosition += crossSpace - cross;
            }

            position += marginMainStart;
            Vector2 corner = Join(position, crossPosition, horizontal);
            Vector2 size = Join(main, cross, horizontal);
            Place(child, context, { corner.X, corner.Y, size.X, size.Y });
            position += main + marginMainEnd + between;
        }
    }

    Vector2 MeasureLayers(ElementState& element, Context& context, const Constraint& constraint)
    {
        Vector2 result;
        for (ElementState* child : LaidOutChildren(element))
        {
            Vector2 size = Measure(*child, context, constraint.Available, constraint.ExactWidth, constraint.ExactHeight);
            const Insets& margin = child->CurrentStyle().Margin;
            result.X = Max(result.X, size.X + margin.Horizontal());
            result.Y = Max(result.Y, size.Y + margin.Vertical());
        }
        return result;
    }

    void ArrangeLayers(ElementState& element, Context& context, Rectangle content, Alignment alignment)
    {
        for (ElementState* child : LaidOutChildren(element))
        {
            const Style& style = child->CurrentStyle();
            Vector2 size = Measure(*child, context, content.Size(), true, true);
            Vector2 space { Max(content.Width - style.Margin.Horizontal(), 0.0f),
                Max(content.Height - style.Margin.Vertical(), 0.0f) };
            if (alignment == Alignment::Stretch)
            {
                if (style.Width.Type == Size::Kind::Fit)
                {
                    size.X = Clamp(space.X, style.MinimumWidth, Max(style.MaximumWidth, style.MinimumWidth));
                }
                if (style.Height.Type == Size::Kind::Fit)
                {
                    size.Y = Clamp(space.Y, style.MinimumHeight, Max(style.MaximumHeight, style.MinimumHeight));
                }
            }
            Vector2 corner { content.X + style.Margin.Left, content.Y + style.Margin.Top };
            if (alignment == Alignment::Center)
            {
                corner += (space - size) * 0.5f;
            }
            else if (alignment == Alignment::End)
            {
                corner += space - size;
            }
            Place(*child, context, { corner.X, corner.Y, size.X, size.Y });
        }
    }

    // ---- Row, Column, and Panel ------------------------------------------------------

    Vector2 LineBehavior::MeasureContent(ElementState& element, Context& context, const Constraint& constraint)
    {
        return MeasureLine(element, context, constraint, Horizontal);
    }

    void LineBehavior::Arrange(ElementState& element, Context& context, Rectangle content)
    {
        ArrangeLine(element, context, content, Horizontal);
    }

    Box PanelBehavior::LookOf(ElementState& element, const Context& context)
    {
        Box box = Behavior::LookOf(element, context);
        const Style& style = element.CurrentStyle();
        if (!style.Background)
        {
            box.Background = ui::Background(context.Theme->Surface);
        }
        if (!style.CornerRadius)
        {
            box.CornerRadius = context.Theme->CornerRadius;
        }
        return box;
    }

    // ---- Stack -------------------------------------------------------------------

    namespace
    {
        class StackBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return Stack::KindName; }
            bool HoldsChildren() const override { return true; }

            void Arrange(ElementState& element, Context& context, Rectangle content) override
            {
                ArrangeLayers(element, context, content, FromData<Alignment>(element.Get("Alignment")));
            }
        };

        // ---- Grid ------------------------------------------------------------------

        class GridBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return Grid::KindName; }
            bool HoldsChildren() const override { return true; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Columns")
                {
                    return DataValue(2);
                }
                if (property == "ColumnGap" || property == "RowGap")
                {
                    return DataValue(0.0);
                }
                return Behavior::Default(property);
            }

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint& constraint) override
            {
                std::vector<ElementState*> children = LaidOutChildren(element);
                int columns = Max(element.Get("Columns").As<int>(), 1);
                float columnGap = element.Get("ColumnGap").As<float>();
                float rowGap = element.Get("RowGap").As<float>();
                float cell = Finite(constraint.Available.X)
                                 ? Max((constraint.Available.X - columnGap * static_cast<float>(columns - 1)) /
                                           static_cast<float>(columns),
                                       0.0f)
                                 : Unlimited;
                float widest = 0.0f;
                float height = 0.0f;
                float rowHeight = 0.0f;
                std::size_t rows = 0;
                for (std::size_t index = 0; index < children.size(); ++index)
                {
                    Vector2 size = Measure(*children[index], context, { cell, Unlimited }, constraint.ExactWidth, false);
                    const Insets& margin = children[index]->CurrentStyle().Margin;
                    widest = Max(widest, size.X + margin.Horizontal());
                    rowHeight = Max(rowHeight, size.Y + margin.Vertical());
                    bool rowEnds = (index + 1) % static_cast<std::size_t>(columns) == 0 || index + 1 == children.size();
                    if (rowEnds)
                    {
                        height += rowHeight + (rows > 0 ? rowGap : 0.0f);
                        rowHeight = 0.0f;
                        ++rows;
                    }
                }

                // Every column keeps its width, even when it has no child yet, as
                // Arrange divides the width among all of them.
                float width = constraint.ExactWidth ? constraint.Available.X
                                                    : widest * static_cast<float>(columns) +
                                                          columnGap * static_cast<float>(columns - 1);
                return { width, height };
            }

            void Arrange(ElementState& element, Context& context, Rectangle content) override
            {
                std::vector<ElementState*> children = LaidOutChildren(element);
                int columns = Max(element.Get("Columns").As<int>(), 1);
                float columnGap = element.Get("ColumnGap").As<float>();
                float rowGap = element.Get("RowGap").As<float>();
                Alignment alignment = FromData<Alignment>(element.Get("Alignment"));
                float cell = Max((content.Width - columnGap * static_cast<float>(columns - 1)) / static_cast<float>(columns),
                    0.0f);
                float top = content.Y;
                for (std::size_t first = 0; first < children.size(); first += static_cast<std::size_t>(columns))
                {
                    std::size_t last = Min(first + static_cast<std::size_t>(columns), children.size());
                    float rowHeight = 0.0f;
                    for (std::size_t index = first; index < last; ++index)
                    {
                        Vector2 size = Measure(*children[index], context, { cell, Unlimited }, true, false);
                        rowHeight = Max(rowHeight, size.Y + children[index]->CurrentStyle().Margin.Vertical());
                    }
                    for (std::size_t index = first; index < last; ++index)
                    {
                        float left = content.X + static_cast<float>(index - first) * (cell + columnGap);
                        ElementState& child = *children[index];
                        const Style& style = child.CurrentStyle();
                        Vector2 size = Measure(child, context, { cell, rowHeight }, true, true);
                        Vector2 space { Max(cell - style.Margin.Horizontal(), 0.0f),
                            Max(rowHeight - style.Margin.Vertical(), 0.0f) };
                        if (alignment == Alignment::Stretch)
                        {
                            if (style.Width.Type == Size::Kind::Fit)
                            {
                                size.X = Clamp(space.X, style.MinimumWidth, Max(style.MaximumWidth, style.MinimumWidth));
                            }
                            if (style.Height.Type == Size::Kind::Fit)
                            {
                                size.Y = Clamp(space.Y, style.MinimumHeight, Max(style.MaximumHeight, style.MinimumHeight));
                            }
                        }
                        Vector2 corner { left + style.Margin.Left, top + style.Margin.Top };
                        if (alignment == Alignment::Center)
                        {
                            corner += (space - size) * 0.5f;
                        }
                        else if (alignment == Alignment::End)
                        {
                            corner += space - size;
                        }
                        Place(child, context, { corner.X, corner.Y, size.X, size.Y });
                    }
                    top += rowHeight + rowGap;
                }
            }
        };

        // ---- Spacer ----------------------------------------------------------------

        class SpacerBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return Spacer::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Width" || property == "Height")
                {
                    return DataValue("fill");
                }
                return Behavior::Default(property);
            }
        };
    }

    std::unique_ptr<Behavior> MakeStackBehavior()
    {
        return std::make_unique<StackBehavior>();
    }

    // ---- Settings shared by containers ----------------------------------------------

    void ApplyContainerSettings(ElementState& element, const ContainerSettings& settings)
    {
        const ContainerSettings defaults {};
        ApplyCommonSettings(element, settings);
        Choose(element, "Gap", settings.Gap, defaults.Gap);
        Choose(element, "Alignment", settings.Alignment, defaults.Alignment);
        Choose(element, "Distribution", settings.Distribution, defaults.Distribution);
        AddChildren(element, settings.Children);
    }
}

namespace easyforge::ui
{
    using namespace internal;

    Container::Container(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Gap(Stored<float, "Gap">(State)), Alignment(Stored<ui::Alignment, "Alignment">(State)),
          Distribution(Stored<ui::Distribution, "Distribution">(State))
    {
    }

    Container::Container(const Container& other) : Container(other.State)
    {
    }

    Container& Container::operator=(const Container& other)
    {
        State = other.State;
        return *this;
    }

    Row::Row(const ContainerSettings& settings) : Container(MakeElement(std::make_unique<LineBehavior>(Row::KindName, true)))
    {
        ApplyContainerSettings(*State, settings);
    }

    Column::Column(const ContainerSettings& settings)
        : Container(MakeElement(std::make_unique<LineBehavior>(Column::KindName, false)))
    {
        ApplyContainerSettings(*State, settings);
    }

    Stack::Stack(const ContainerSettings& settings) : Container(MakeElement(MakeStackBehavior()))
    {
        ApplyContainerSettings(*State, settings);
    }

    Panel::Panel(const ContainerSettings& settings) : Container(MakeElement(std::make_unique<PanelBehavior>()))
    {
        ApplyContainerSettings(*State, settings);
    }

    Grid::Grid(const GridSettings& settings) : Grid(MakeElement(std::make_unique<GridBehavior>()))
    {
        const GridSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "Columns", Max(settings.Columns, 1), defaults.Columns);
        Choose(*State, "ColumnGap", settings.ColumnGap, defaults.ColumnGap);
        Choose(*State, "RowGap", settings.RowGap, defaults.RowGap);
        Choose(*State, "Alignment", settings.Alignment, defaults.Alignment);
        AddChildren(*State, settings.Children);
    }

    Grid::Grid(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Columns(Stored<int, "Columns">(State)), ColumnGap(Stored<float, "ColumnGap">(State)),
          RowGap(Stored<float, "RowGap">(State)), Alignment(Stored<ui::Alignment, "Alignment">(State))
    {
    }

    Grid::Grid(const Grid& other) : Grid(other.State)
    {
    }

    Grid& Grid::operator=(const Grid& other)
    {
        State = other.State;
        return *this;
    }

    Spacer::Spacer() : Element(MakeElement(std::make_unique<SpacerBehavior>()))
    {
    }

    Spacer::Spacer(float size) : Spacer()
    {
        State->Set("Width", DataValue(size));
        State->Set("Height", DataValue(size));
    }
}
