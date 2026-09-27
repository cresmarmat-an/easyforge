#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "FontFile.h"

namespace easyforge::internal
{
    namespace
    {
        // Pixel coverage built up edge by edge. Each edge adds, in every row it
        // crosses, the signed area it covers to the cell it is in and the change it
        // makes to every cell to its right. Summing a row from left to right then
        // gives each pixel's coverage exactly, with anti-aliasing and no sampling.
        class Accumulator
        {
        public:
            Accumulator(int width, int height)
                : Width(width), Height(height), Stride(static_cast<std::size_t>(width) + 2),
                  Cells(Stride * static_cast<std::size_t>(height), 0.0f)
            {
            }

            // Adds an edge from `from` to `to`, in pixels with Y pointing down.
            void AddEdge(Vector2 from, Vector2 to)
            {
                if (from.Y == to.Y)
                {
                    return;
                }
                float direction = 1.0f;
                if (from.Y > to.Y)
                {
                    std::swap(from, to);
                    direction = -1.0f;
                }

                float slope = (to.X - from.X) / (to.Y - from.Y);
                int firstRow = Max(0, static_cast<int>(std::floor(from.Y)));
                int endRow = Min(Height, static_cast<int>(std::ceil(to.Y)));
                for (int row = firstRow; row < endRow; ++row)
                {
                    float top = Max(from.Y, static_cast<float>(row));
                    float bottom = Min(to.Y, static_cast<float>(row + 1));
                    if (bottom <= top)
                    {
                        continue;
                    }
                    float topX = from.X + (top - from.Y) * slope;
                    float bottomX = from.X + (bottom - from.Y) * slope;
                    AddSpan(row, topX, bottomX, (bottom - top) * direction);
                }
            }

            // Sums each row into coverage values from 0 to 255.
            std::vector<std::uint8_t> Finish() const
            {
                std::vector<std::uint8_t> coverage(static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height));
                for (int row = 0; row < Height; ++row)
                {
                    const float* cells = Cells.data() + static_cast<std::size_t>(row) * Stride;
                    float sum = 0.0f;
                    for (int column = 0; column < Width; ++column)
                    {
                        sum += cells[column];
                        float value = Min(std::abs(sum), 1.0f);
                        coverage[static_cast<std::size_t>(row) * static_cast<std::size_t>(Width) + static_cast<std::size_t>(column)] =
                            static_cast<std::uint8_t>(value * 255.0f + 0.5f);
                    }
                }
                return coverage;
            }

        private:
            // One edge's part inside one row: it runs from x0 to x1 and covers `height`
            // of the row, signed by its direction.
            void AddSpan(int row, float x0, float x1, float height)
            {
                float limit = static_cast<float>(Width);
                x0 = Clamp(x0, 0.0f, limit);
                x1 = Clamp(x1, 0.0f, limit);
                if (x0 > x1)
                {
                    std::swap(x0, x1);
                }
                float* cells = Cells.data() + static_cast<std::size_t>(row) * Stride;

                float left = std::floor(x0);
                int leftCell = static_cast<int>(left);
                int rightCell = static_cast<int>(std::ceil(x1));
                if (rightCell <= leftCell + 1)
                {
                    // Inside one cell: the part of the cell right of the edge's middle is covered.
                    float middle = 0.5f * (x0 + x1) - left;
                    cells[leftCell] += height * (1.0f - middle);
                    cells[leftCell + 1] += height * middle;
                    return;
                }

                // Across several cells the covered area grows linearly, with a triangle
                // at each end.
                float perUnit = 1.0f / (x1 - x0);
                float startFraction = x0 - left;
                float firstArea = 0.5f * perUnit * (1.0f - startFraction) * (1.0f - startFraction);
                float endFraction = x1 - static_cast<float>(rightCell) + 1.0f;
                float lastArea = 0.5f * perUnit * endFraction * endFraction;

                cells[leftCell] += height * firstArea;
                if (rightCell == leftCell + 2)
                {
                    cells[leftCell + 1] += height * (1.0f - firstArea - lastArea);
                }
                else
                {
                    float secondArea = perUnit * (1.5f - startFraction);
                    cells[leftCell + 1] += height * (secondArea - firstArea);
                    for (int cell = leftCell + 2; cell < rightCell - 1; ++cell)
                    {
                        cells[cell] += height * perUnit;
                    }
                    float beforeLast = secondArea + static_cast<float>(rightCell - leftCell - 3) * perUnit;
                    cells[rightCell - 1] += height * (1.0f - beforeLast - lastArea);
                }
                cells[rightCell] += height * lastArea;
            }

            int Width;
            int Height;
            std::size_t Stride;
            std::vector<float> Cells;
        };

        // Splits a quadratic curve into straight lines short enough to be off by at
        // most about a fifth of a pixel.
        void AddCurve(Accumulator& accumulator, Vector2 start, Vector2 control, Vector2 end)
        {
            float bend = Length(start - control * 2.0f + end);
            int pieces = Clamp(static_cast<int>(std::ceil(std::sqrt(bend / 1.6f))), 1, 64);
            Vector2 previous = start;
            for (int piece = 1; piece <= pieces; ++piece)
            {
                float amount = static_cast<float>(piece) / static_cast<float>(pieces);
                float other = 1.0f - amount;
                Vector2 point = start * (other * other) + control * (2.0f * other * amount) + end * (amount * amount);
                accumulator.AddEdge(previous, point);
                previous = point;
            }
        }
    }

    GlyphBitmap RasterizeOutline(const std::vector<Contour>& contours, float scale, float largestSide)
    {
        GlyphBitmap bitmap;
        float minimumX = std::numeric_limits<float>::infinity();
        float minimumY = std::numeric_limits<float>::infinity();
        float maximumX = -std::numeric_limits<float>::infinity();
        float maximumY = -std::numeric_limits<float>::infinity();
        for (const Contour& contour : contours)
        {
            for (const OutlinePoint& point : contour)
            {
                minimumX = Min(minimumX, point.Position.X * scale);
                minimumY = Min(minimumY, point.Position.Y * scale);
                maximumX = Max(maximumX, point.Position.X * scale);
                maximumY = Max(maximumY, point.Position.Y * scale);
            }
        }
        if (!(minimumX < maximumX) || !(minimumY < maximumY))
        {
            return bitmap;
        }

        if (maximumX - minimumX > largestSide || maximumY - minimumY > largestSide)
        {
            return bitmap;
        }

        bitmap.Left = static_cast<int>(std::floor(minimumX));
        bitmap.Top = static_cast<int>(std::ceil(maximumY));
        bitmap.Width = static_cast<int>(std::ceil(maximumX)) - bitmap.Left;
        bitmap.Height = bitmap.Top - static_cast<int>(std::floor(minimumY));

        // Font units have Y up from the baseline; the bitmap has Y down from its top.
        auto toPixels = [&](Vector2 position) {
            return Vector2 { position.X * scale - static_cast<float>(bitmap.Left),
                static_cast<float>(bitmap.Top) - position.Y * scale };
        };

        Accumulator accumulator(bitmap.Width, bitmap.Height);
        for (const Contour& contour : contours)
        {
            if (contour.size() < 2)
            {
                continue;
            }
            Vector2 current = toPixels(contour[0].Position);
            for (std::size_t index = 1; index < contour.size(); ++index)
            {
                const OutlinePoint& point = contour[index];
                if (point.OnCurve)
                {
                    Vector2 next = toPixels(point.Position);
                    accumulator.AddEdge(current, next);
                    current = next;
                }
                else if (index + 1 < contour.size())
                {
                    Vector2 control = toPixels(point.Position);
                    Vector2 next = toPixels(contour[index + 1].Position);
                    AddCurve(accumulator, current, control, next);
                    current = next;
                    ++index;
                }
            }
            // Closes the contour in case it does not end where it started.
            Vector2 start = toPixels(contour[0].Position);
            if (current.X != start.X || current.Y != start.Y)
            {
                accumulator.AddEdge(current, start);
            }
        }

        bitmap.Coverage = accumulator.Finish();
        return bitmap;
    }
}
