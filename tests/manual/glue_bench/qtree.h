/* Point-region quadtree in C++: capacity 8, depth <= 8, nodes from a pool
** that is reused every frame (clear() is O(1)). Same rules as the script
** versions, but count() only depends on the points, not on the tree shape. */
#pragma once
#include <vector>

struct QuadTree
{
    struct Node
    {
        double x, y, w, h;
        int depth;
        int count;
        int first_child; /* -1 = leaf; else 4 consecutive nodes */
        double px[8], py[8];
    };

    double width, height;
    std::vector<Node> nodes;

    QuadTree(double w, double h) : width(w), height(h) { clear(); }

    void clear()
    {
        nodes.clear();
        nodes.push_back(Node{0, 0, width, height, 0, 0, -1, {}, {}});
    }

    void insert(double x, double y)
    {
        int idx = 0;
        while (true)
        {
            Node &n = nodes[idx];
            if (n.first_child < 0)
            {
                if (n.count < 8 || n.depth >= 8)
                {
                    if (n.count < 8)
                    {
                        n.px[n.count] = x;
                        n.py[n.count] = y;
                        n.count++;
                    }
                    else
                        overflow_push(idx, x, y);
                    return;
                }
                subdivide(idx);
            }
            idx = child_for(idx, x, y);
        }
    }

    int count(double x0, double y0, double x1, double y1) const { return count_in(0, x0, y0, x1, y1); }

private:
    /* Leaves at max depth can hold more than 8 points: spill into a side list. */
    std::vector<std::vector<std::pair<double, double>>> spill_;

    void overflow_push(int idx, double x, double y)
    {
        Node &n = nodes[idx];
        if (n.count < 8) return;
        /* encode: count >= 8 means "has spill list at index count - 8" */
        if (n.count == 8)
        {
            spill_.emplace_back();
            n.count = 8 + (int)spill_.size() - 1 + 1; /* 9 + list index */
        }
        spill_[n.count - 9].emplace_back(x, y);
    }

    int child_for(int idx, double x, double y) const
    {
        const Node &n = nodes[idx];
        int c = n.first_child;
        if (y >= n.y + n.h * 0.5) c += 2;
        if (x >= n.x + n.w * 0.5) c += 1;
        return c;
    }

    void subdivide(int idx)
    {
        double x = nodes[idx].x, y = nodes[idx].y, hw = nodes[idx].w * 0.5, hh = nodes[idx].h * 0.5;
        int d = nodes[idx].depth + 1;
        int first = (int)nodes.size();
        nodes.push_back(Node{x, y, hw, hh, d, 0, -1, {}, {}});
        nodes.push_back(Node{x + hw, y, hw, hh, d, 0, -1, {}, {}});
        nodes.push_back(Node{x, y + hh, hw, hh, d, 0, -1, {}, {}});
        nodes.push_back(Node{x + hw, y + hh, hw, hh, d, 0, -1, {}, {}});
        Node &n = nodes[idx]; /* re-fetch: push_back may have reallocated */
        n.first_child = first;
        for (int i = 0; i < n.count; i++)
        {
            int c = child_for(idx, n.px[i], n.py[i]);
            Node &ch = nodes[c];
            ch.px[ch.count] = n.px[i];
            ch.py[ch.count] = n.py[i];
            ch.count++;
        }
        nodes[idx].count = 0;
    }

    int count_in(int idx, double x0, double y0, double x1, double y1) const
    {
        const Node &n = nodes[idx];
        if (x1 <= n.x || y1 <= n.y || x0 >= n.x + n.w || y0 >= n.y + n.h)
            return 0;
        if (n.first_child >= 0)
        {
            int c = n.first_child;
            return count_in(c, x0, y0, x1, y1) + count_in(c + 1, x0, y0, x1, y1) +
                   count_in(c + 2, x0, y0, x1, y1) + count_in(c + 3, x0, y0, x1, y1);
        }
        int r = 0;
        int cnt = n.count > 8 ? 8 : n.count;
        for (int i = 0; i < cnt; i++)
            if (n.px[i] >= x0 && n.px[i] < x1 && n.py[i] >= y0 && n.py[i] < y1)
                r++;
        if (n.count > 8)
            for (const auto &p : spill_[n.count - 9])
                if (p.first >= x0 && p.first < x1 && p.second >= y0 && p.second < y1)
                    r++;
        return r;
    }
};
