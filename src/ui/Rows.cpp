#include "Rows.h"

#include <cmath>

#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    float RowsBehavior::RowHeight(ElementState& element, const Context& context, std::size_t)
    {
        easyforge::Font font = FontOf(element, context);
        return (font ? font.LineHeight(FontSizeOf(element, context)) : 16.0f) + 10.0f;
    }

    const std::vector<float>& RowsBehavior::Offsets(ElementState& element, const Context& context)
    {
        std::size_t count = RowCount(element);
        RowStarts.resize(count + 1);
        float top = 0.0f;
        for (std::size_t row = 0; row < count; ++row)
        {
            RowStarts[row] = top;
            top += RowHeight(element, context, row);
        }
        RowStarts[count] = top;
        return RowStarts;
    }

    float RowsBehavior::RowsHeight(ElementState& element, const Context& context)
    {
        return Offsets(element, context).back();
    }

    Rectangle RowsBehavior::RowArea(ElementState& element) const
    {
        const Style& style = element.CurrentStyle();
        return { element.Frame.X + style.Padding.Left, element.Frame.Y + style.Padding.Top,
            Max(element.Frame.Width - style.Padding.Horizontal(), 0.0f),
            Max(element.Frame.Height - style.Padding.Vertical(), 0.0f) };
    }

    std::optional<std::size_t> RowsBehavior::RowAt(ElementState& element, const Context& context, Vector2 point)
    {
        Rectangle area = RowArea(element);
        if (!area.Contains(point))
        {
            return std::nullopt;
        }
        const std::vector<float>& starts = Offsets(element, context);
        float along = point.Y - area.Y + Scrolled;
        for (std::size_t row = 0; row + 1 < starts.size(); ++row)
        {
            if (along >= starts[row] && along < starts[row + 1])
            {
                return row;
            }
        }
        return std::nullopt;
    }

    void RowsBehavior::Reveal(ElementState& element, const Context& context, std::size_t row)
    {
        const std::vector<float>& starts = Offsets(element, context);
        if (row + 1 >= starts.size())
        {
            return;
        }
        float view = RowArea(element).Height;
        if (starts[row] < Scrolled)
        {
            Scrolled = starts[row];
        }
        else if (starts[row + 1] > Scrolled + view)
        {
            Scrolled = starts[row + 1] - view;
        }
        ScrollShown = 1.0f;
    }

    void RowsBehavior::Update(ElementState& element, Context& context)
    {
        float farthest = Max(RowsHeight(element, context) - RowArea(element).Height, 0.0f);
        Scrolled = Clamp(Scrolled, 0.0f, farthest);
        ScrollShown = Max(ScrollShown - context.DeltaSeconds * 1.5f, 0.0f);
        if (!element.Hovered)
        {
            HoveredRow.reset();
        }
    }

    void RowsBehavior::Draw(ElementState& element, DrawContext& context)
    {
        Rectangle area = RowArea(element);
        const std::vector<float>& starts = Offsets(element, context);
        std::vector<float> copied = starts;
        context.Canvas.PushClip(area);
        for (std::size_t row = 0; row + 1 < copied.size(); ++row)
        {
            float top = area.Y + copied[row] - Scrolled;
            float bottom = area.Y + copied[row + 1] - Scrolled;
            if (bottom < area.Y || top > area.Bottom())
            {
                continue;
            }
            DrawRow(element, context, row, { area.X, top, area.Width, bottom - top });
        }
        context.Canvas.PopClip();
    }

    void RowsBehavior::DrawOver(ElementState& element, DrawContext& context)
    {
        Rectangle area = RowArea(element);
        float total = RowsHeight(element, context);
        if (total <= area.Height + 0.5f)
        {
            return;
        }
        float visible = Max(ScrollShown, element.HoverAmount);
        if (visible <= 0.01f)
        {
            return;
        }
        float length = Max(area.Height * area.Height / total, 24.0f);
        float start = (area.Height - length) * Clamp(Scrolled / (total - area.Height), 0.0f, 1.0f);
        context.Canvas.Rectangle({ .Position = { element.Frame.Right() - 6.0f, area.Y + start }, .Size = { 4.0f, length },
            .Color = context.Theme->MutedText.WithAlpha(0.6f * visible), .CornerRadius = 2.0f });
    }

    bool RowsBehavior::Wheel(ElementState& element, Context& context, Vector2 amount)
    {
        float farthest = Max(RowsHeight(element, context) - RowArea(element).Height, 0.0f);
        float before = Scrolled;
        Scrolled = Clamp(Scrolled - amount.Y * 48.0f, 0.0f, farthest);
        ScrollShown = 1.0f;
        return Scrolled != before;
    }

    void RowsBehavior::PointerPressed(ElementState& element, Context& context, const Pointer& pointer)
    {
        if (std::optional<std::size_t> row = RowAt(element, context, pointer.Position))
        {
            RowPressed(element, context, *row, pointer);
        }
    }

    void RowsBehavior::PointerMoved(ElementState& element, Context& context, const Pointer& pointer)
    {
        HoveredRow = RowAt(element, context, pointer.Position);
    }

    bool RowsBehavior::KeyPressed(ElementState& element, Context& context, const Event& event)
    {
        std::size_t count = RowCount(element);
        if (count == 0)
        {
            return false;
        }
        std::optional<std::size_t> current = KeyboardRow(element);
        auto step = [&](std::size_t from, int direction, std::size_t times) {
            std::size_t row = from;
            for (std::size_t moved = 0; moved < times;)
            {
                if (direction < 0 && row == 0)
                {
                    break;
                }
                if (direction > 0 && row + 1 >= count)
                {
                    break;
                }
                row = direction < 0 ? row - 1 : row + 1;
                if (!Skipped(element, row))
                {
                    from = row;
                    ++moved;
                }
            }
            return from;
        };
        auto firstFrom = [&](std::size_t row, int direction) {
            while (Skipped(element, row))
            {
                if ((direction < 0 && row == 0) || (direction > 0 && row + 1 >= count))
                {
                    return row;
                }
                row = direction < 0 ? row - 1 : row + 1;
            }
            return row;
        };
        float view = RowArea(element).Height;
        std::size_t page = static_cast<std::size_t>(Max(view / Max(RowHeight(element, context, 0), 1.0f), 1.0f));
        std::optional<std::size_t> target;
        switch (event.Key)
        {
        case Key::Up: target = current ? step(*current, -1, 1) : firstFrom(count - 1, -1); break;
        case Key::Down: target = current ? step(*current, 1, 1) : firstFrom(0, 1); break;
        case Key::PageUp: target = current ? step(*current, -1, page) : firstFrom(0, 1); break;
        case Key::PageDown: target = current ? step(*current, 1, page) : firstFrom(count - 1, -1); break;
        case Key::Home: target = firstFrom(0, 1); break;
        case Key::End: target = firstFrom(count - 1, -1); break;
        case Key::Enter:
        case Key::NumberPadEnter:
            if (current)
            {
                Activate(element, context, *current);
            }
            return true;
        default: return false;
        }
        if (target && !Skipped(element, *target))
        {
            MoveTo(element, context, *target);
            Reveal(element, context, *target);
        }
        return true;
    }
}
