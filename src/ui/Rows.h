#pragma once

// Elements drawn as rows: lists, trees, and the lists that dropdowns and menus
// open. The rows are drawn by the element itself rather than being elements, so
// a list of thousands costs no more to lay out than a list of ten.

#include <cstddef>
#include <optional>
#include <vector>

#include "Behavior.h"

namespace easyforge::ui::internal
{
    class RowsBehavior : public Behavior
    {
    public:
        bool ClipsChildren(ElementState&) const override { return true; }

        virtual std::size_t RowCount(ElementState& element) = 0;

        // Row heights, in points. A row of the default height fits one line of
        // text with room around it.
        virtual float RowHeight(ElementState& element, const Context& context, std::size_t row);

        // Draws one row into its area, which may be only partly visible.
        virtual void DrawRow(ElementState& element, DrawContext& context, std::size_t row, Rectangle area) = 0;

        // A row was pressed, with the point in the root's points.
        virtual void RowPressed(ElementState&, Context&, std::size_t /*row*/, const Pointer&) {}

        // The row the keyboard is on, and moving it, as the arrow keys do.
        virtual std::optional<std::size_t> KeyboardRow(ElementState&) { return std::nullopt; }
        virtual void MoveTo(ElementState&, Context&, std::size_t /*row*/) {}

        // Enter on the keyboard's row.
        virtual void Activate(ElementState&, Context&, std::size_t /*row*/) {}

        // Rows the keyboard skips, such as separators.
        virtual bool Skipped(ElementState&, std::size_t /*row*/) { return false; }

        // ---- Shared behavior ----------------------------------------------------

        void Draw(ElementState& element, DrawContext& context) override;
        void DrawOver(ElementState& element, DrawContext& context) override;
        bool Wheel(ElementState& element, Context& context, Vector2 amount) override;
        void PointerPressed(ElementState& element, Context& context, const Pointer& pointer) override;
        void PointerMoved(ElementState& element, Context& context, const Pointer& pointer) override;
        bool KeyPressed(ElementState& element, Context& context, const Event& event) override;
        void Update(ElementState& element, Context& context) override;

        // The total height of the rows, in points.
        float RowsHeight(ElementState& element, const Context& context);

        // The row at a point in the root's points, if any.
        std::optional<std::size_t> RowAt(ElementState& element, const Context& context, Vector2 point);

        // Scrolls until the row is in view.
        void Reveal(ElementState& element, const Context& context, std::size_t row);

        // The box the rows are drawn in: the frame inside the padding.
        Rectangle RowArea(ElementState& element) const;

        float Scrolled = 0.0f;
        std::optional<std::size_t> HoveredRow;

    protected:
        // Where each row starts, measured from the first row's top. One more entry
        // than there are rows: the last is the total height.
        const std::vector<float>& Offsets(ElementState& element, const Context& context);

        std::vector<float> RowStarts;
        float ScrollShown = 0.0f;
    };
}
