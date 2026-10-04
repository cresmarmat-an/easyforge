#include "Tree.h"

#include <algorithm>

namespace easyforge::internal::physics2d
{
    int BoundsTree::Allocate()
    {
        if (FreeList >= 0)
        {
            int index = FreeList;
            FreeList = Nodes[static_cast<std::size_t>(index)].Parent;
            Nodes[static_cast<std::size_t>(index)] = Node {};
            return index;
        }
        Nodes.emplace_back();
        return static_cast<int>(Nodes.size()) - 1;
    }

    void BoundsTree::Free(int index)
    {
        Node& node = Nodes[static_cast<std::size_t>(index)];
        node = Node {};
        node.Height = -1;
        node.Parent = FreeList;
        FreeList = index;
    }

    int BoundsTree::Insert(const Extent& tight, int body)
    {
        int leaf = Allocate();
        Node& node = Nodes[static_cast<std::size_t>(leaf)];
        node.Box = tight.Grown(BoundsMargin);
        node.Body = body;
        node.Height = 0;
        InsertLeaf(leaf);
        return leaf;
    }

    void BoundsTree::Remove(int proxy)
    {
        RemoveLeaf(proxy);
        Free(proxy);
    }

    bool BoundsTree::Move(int proxy, const Extent& tight)
    {
        if (Nodes[static_cast<std::size_t>(proxy)].Box.Contains(tight))
        {
            return false;
        }
        RemoveLeaf(proxy);
        Nodes[static_cast<std::size_t>(proxy)].Box = tight.Grown(BoundsMargin);
        InsertLeaf(proxy);
        return true;
    }

    void BoundsTree::InsertLeaf(int leaf)
    {
        if (Root < 0)
        {
            Root = leaf;
            Nodes[static_cast<std::size_t>(leaf)].Parent = -1;
            return;
        }

        // Walk down to the place where adding the leaf grows the boxes least.
        Extent leafBox = Nodes[static_cast<std::size_t>(leaf)].Box;
        int index = Root;
        while (!Nodes[static_cast<std::size_t>(index)].IsLeaf())
        {
            const Node& node = Nodes[static_cast<std::size_t>(index)];
            float area = node.Box.Perimeter();
            float combined = Join(node.Box, leafBox).Perimeter();
            float cost = 2.0f * combined;
            float inherited = 2.0f * (combined - area);
            auto descend = [&](int child) {
                const Node& next = Nodes[static_cast<std::size_t>(child)];
                float grown = Join(leafBox, next.Box).Perimeter();
                return (next.IsLeaf() ? grown : grown - next.Box.Perimeter()) + inherited;
            };
            float firstCost = descend(node.First);
            float secondCost = descend(node.Second);
            if (cost < firstCost && cost < secondCost)
            {
                break;
            }
            index = firstCost < secondCost ? node.First : node.Second;
        }

        int sibling = index;
        int oldParent = Nodes[static_cast<std::size_t>(sibling)].Parent;
        int newParent = Allocate();
        Node& parent = Nodes[static_cast<std::size_t>(newParent)];
        parent.Parent = oldParent;
        parent.Box = Join(leafBox, Nodes[static_cast<std::size_t>(sibling)].Box);
        parent.Height = Nodes[static_cast<std::size_t>(sibling)].Height + 1;
        parent.First = sibling;
        parent.Second = leaf;
        if (oldParent >= 0)
        {
            Node& grandParent = Nodes[static_cast<std::size_t>(oldParent)];
            (grandParent.First == sibling ? grandParent.First : grandParent.Second) = newParent;
        }
        else
        {
            Root = newParent;
        }
        Nodes[static_cast<std::size_t>(sibling)].Parent = newParent;
        Nodes[static_cast<std::size_t>(leaf)].Parent = newParent;
        Refit(Nodes[static_cast<std::size_t>(leaf)].Parent);
    }

    void BoundsTree::RemoveLeaf(int leaf)
    {
        if (leaf == Root)
        {
            Root = -1;
            return;
        }
        int parent = Nodes[static_cast<std::size_t>(leaf)].Parent;
        int grandParent = Nodes[static_cast<std::size_t>(parent)].Parent;
        int sibling = Nodes[static_cast<std::size_t>(parent)].First == leaf ? Nodes[static_cast<std::size_t>(parent)].Second
                                                                            : Nodes[static_cast<std::size_t>(parent)].First;
        if (grandParent >= 0)
        {
            Node& grand = Nodes[static_cast<std::size_t>(grandParent)];
            (grand.First == parent ? grand.First : grand.Second) = sibling;
            Nodes[static_cast<std::size_t>(sibling)].Parent = grandParent;
            Free(parent);
            Refit(grandParent);
        }
        else
        {
            Root = sibling;
            Nodes[static_cast<std::size_t>(sibling)].Parent = -1;
            Free(parent);
        }
    }

    // From `index` up to the root: rebalance, then fit boxes and heights to the
    // children.
    void BoundsTree::Refit(int index)
    {
        while (index >= 0)
        {
            index = Balance(index);
            Node& node = Nodes[static_cast<std::size_t>(index)];
            const Node& first = Nodes[static_cast<std::size_t>(node.First)];
            const Node& second = Nodes[static_cast<std::size_t>(node.Second)];
            node.Height = 1 + std::max(first.Height, second.Height);
            node.Box = Join(first.Box, second.Box);
            index = node.Parent;
        }
    }

    // A rotation that lifts the taller child when one side is more than one
    // level taller than the other. Returns the node now at this place.
    int BoundsTree::Balance(int index)
    {
        Node& top = Nodes[static_cast<std::size_t>(index)];
        if (top.IsLeaf() || top.Height < 2)
        {
            return index;
        }
        int firstIndex = top.First;
        int secondIndex = top.Second;
        int balance = Nodes[static_cast<std::size_t>(secondIndex)].Height - Nodes[static_cast<std::size_t>(firstIndex)].Height;
        if (balance >= -1 && balance <= 1)
        {
            return index;
        }

        bool liftSecond = balance > 1;
        int lifted = liftSecond ? secondIndex : firstIndex;
        int stays = liftSecond ? firstIndex : secondIndex;
        Node& up = Nodes[static_cast<std::size_t>(lifted)];
        int left = up.First;
        int right = up.Second;

        // The lifted node takes the old top's place.
        up.First = index;
        up.Parent = top.Parent;
        top.Parent = lifted;
        if (up.Parent >= 0)
        {
            Node& above = Nodes[static_cast<std::size_t>(up.Parent)];
            (above.First == index ? above.First : above.Second) = lifted;
        }
        else
        {
            Root = lifted;
        }

        // The lifted node keeps its taller child; the shorter goes to the old top.
        bool leftTaller = Nodes[static_cast<std::size_t>(left)].Height > Nodes[static_cast<std::size_t>(right)].Height;
        int kept = leftTaller ? left : right;
        int given = leftTaller ? right : left;
        up.Second = kept;
        if (liftSecond)
        {
            top.Second = given;
        }
        else
        {
            top.First = given;
        }
        Nodes[static_cast<std::size_t>(given)].Parent = index;

        const Node& staying = Nodes[static_cast<std::size_t>(stays)];
        const Node& handed = Nodes[static_cast<std::size_t>(given)];
        top.Box = Join(staying.Box, handed.Box);
        top.Height = 1 + std::max(staying.Height, handed.Height);
        const Node& keeping = Nodes[static_cast<std::size_t>(kept)];
        up.Box = Join(top.Box, keeping.Box);
        up.Height = 1 + std::max(top.Height, keeping.Height);
        return lifted;
    }

    bool BoundsTree::SegmentTouches(const Extent& box, Vector2 origin, Vector2 translation, float fraction)
    {
        float enter = 0.0f;
        float leave = fraction;
        const float origins[2] = { origin.X, origin.Y };
        const float moves[2] = { translation.X, translation.Y };
        const float lowers[2] = { box.Lower.X, box.Lower.Y };
        const float uppers[2] = { box.Upper.X, box.Upper.Y };
        for (int axis = 0; axis < 2; ++axis)
        {
            if (moves[axis] == 0.0f)
            {
                if (origins[axis] < lowers[axis] || origins[axis] > uppers[axis])
                {
                    return false;
                }
                continue;
            }
            float first = (lowers[axis] - origins[axis]) / moves[axis];
            float second = (uppers[axis] - origins[axis]) / moves[axis];
            enter = std::max(enter, std::min(first, second));
            leave = std::min(leave, std::max(first, second));
            if (enter > leave)
            {
                return false;
            }
        }
        return true;
    }
}
