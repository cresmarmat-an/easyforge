#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "Geometry.h"

namespace easyforge::internal::physics2d
{
    // The nodes a query still has to visit, last in first out. They stay on the
    // machine stack while they fit, which they do for any balanced tree, so a
    // query allocates nothing; a deeper tree spills into the heap.
    class NodeStack
    {
    public:
        void Push(int node)
        {
            if (Count < Fixed.size())
            {
                Fixed[Count++] = node;
            }
            else
            {
                Spilled.push_back(node);
            }
        }

        int Pop()
        {
            if (!Spilled.empty())
            {
                int node = Spilled.back();
                Spilled.pop_back();
                return node;
            }
            return Fixed[--Count];
        }

        bool IsEmpty() const { return Count == 0 && Spilled.empty(); }

    private:
        std::array<int, 128> Fixed {};
        std::size_t Count = 0;
        std::vector<int> Spilled;
    };

    // A balanced tree of boxes around bodies, for finding the bodies near a box
    // or along a ray without testing every one. Each body's box is made larger
    // than the body, so small moves leave the tree alone.
    class BoundsTree
    {
    public:
        // Adds a body with a tight box, stored grown by the margin.
        int Insert(const Extent& tight, int body);
        void Remove(int proxy);

        // Updates a body's box. Returns true when the stored box had to change.
        bool Move(int proxy, const Extent& tight);

        const Extent& StoredExtent(int proxy) const { return Nodes[static_cast<std::size_t>(proxy)].Box; }

        // Calls visit(body) for every stored box overlapping `extent`; visiting
        // stops when it returns false.
        template <typename Visit>
        void Query(const Extent& extent, Visit&& visit) const
        {
            if (Root < 0)
            {
                return;
            }
            NodeStack stack;
            stack.Push(Root);
            while (!stack.IsEmpty())
            {
                int index = stack.Pop();
                const Node& node = Nodes[static_cast<std::size_t>(index)];
                if (!node.Box.Overlaps(extent))
                {
                    continue;
                }
                if (node.IsLeaf())
                {
                    if (!visit(node.Body))
                    {
                        return;
                    }
                    continue;
                }
                stack.Push(node.First);
                stack.Push(node.Second);
            }
        }

        // Calls visit(body, fraction) for every stored box the segment from
        // `origin` along `translation` passes through, up to `fraction` of the
        // way, with every box grown by `grow`. visit returns the fraction to search
        // up to from then on.
        template <typename Visit>
        void CastRay(Vector2 origin, Vector2 translation, float fraction, float grow, Visit&& visit) const
        {
            if (Root < 0)
            {
                return;
            }
            NodeStack stack;
            stack.Push(Root);
            while (!stack.IsEmpty())
            {
                int index = stack.Pop();
                const Node& node = Nodes[static_cast<std::size_t>(index)];
                if (!SegmentTouches(node.Box.Grown(grow), origin, translation, fraction))
                {
                    continue;
                }
                if (node.IsLeaf())
                {
                    fraction = visit(node.Body, fraction);
                    if (fraction <= 0.0f)
                    {
                        return;
                    }
                    continue;
                }
                stack.Push(node.First);
                stack.Push(node.Second);
            }
        }

    private:
        struct Node
        {
            Extent Box;
            int Parent = -1;
            int First = -1;
            int Second = -1;
            int Height = 0;
            int Body = -1;

            bool IsLeaf() const { return First < 0; }
        };

        static bool SegmentTouches(const Extent& box, Vector2 origin, Vector2 translation, float fraction);

        int Allocate();
        void Free(int index);
        void InsertLeaf(int leaf);
        void RemoveLeaf(int leaf);
        int Balance(int index);
        void Refit(int index);

        std::vector<Node> Nodes;
        int Root = -1;
        int FreeList = -1;
    };
}
