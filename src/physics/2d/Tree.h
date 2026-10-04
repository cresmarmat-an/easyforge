#pragma once

#include <vector>

#include "Geometry.h"

namespace easyforge::internal::physics2d
{
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
            std::vector<int> stack { Root };
            while (!stack.empty())
            {
                int index = stack.back();
                stack.pop_back();
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
                stack.push_back(node.First);
                stack.push_back(node.Second);
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
            std::vector<int> stack { Root };
            while (!stack.empty())
            {
                int index = stack.back();
                stack.pop_back();
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
                stack.push_back(node.First);
                stack.push_back(node.Second);
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
