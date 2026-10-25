#include "core/graph/QuadTree.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace nexus::core {

namespace {

constexpr double kTwoPi = 6.283185307179586;
constexpr double kGolden = 0.6180339887498949;
constexpr double kPlastic = 0.7548776662466927;
// Enough for the deepest descent: three siblings are pushed per level.
constexpr std::size_t kStackSize = static_cast<std::size_t>(QuadTree::kMaxDepth) * 3 + 8;

int quadrantOf(double x, double y, double cx, double cy)
{
    return (x >= cx ? 1 : 0) + (y >= cy ? 2 : 0);
}

// Deterministic unit vector for two points at the very same position.
Vec2 coincidentDirection(std::uint32_t a, std::uint32_t b)
{
    double turns = (static_cast<double>(a) * kGolden) + (static_cast<double>(b) * kPlastic);
    turns -= std::floor(turns);
    const double angle = turns * kTwoPi;
    return {std::cos(angle), std::sin(angle)};
}

// d3-style softening: below minDistance the squared distance is blended towards it.
double softened(double d2, double minDist2)
{
    return d2 < minDist2 ? std::sqrt(minDist2 * d2) : d2;
}

} // namespace

void QuadTree::build(std::span<const Vec2> points)
{
    m_points = points;
    m_nodes.clear();
    m_next.assign(points.size(), -1);
    if (points.empty()) {
        return;
    }

    double minX = std::numeric_limits<double>::max();
    double minY = minX;
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = maxX;
    for (const Vec2& p : points) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    Node root;
    root.cx = (minX + maxX) * 0.5;
    root.cy = (minY + maxY) * 0.5;
    root.half = std::max({maxX - minX, maxY - minY, 1e-9}) * 0.5 * 1.0000001;
    m_nodes.reserve((points.size() * 2) + 1);
    m_nodes.push_back(root);

    for (std::uint32_t i = 0; i < points.size(); ++i) {
        insert(i);
    }
}

std::int32_t QuadTree::makeChild(std::int32_t parent, int quadrant)
{
    Node child;
    const Node& p = m_nodes[static_cast<std::size_t>(parent)];
    child.half = p.half * 0.5;
    child.cx = p.cx + ((quadrant & 1) != 0 ? child.half : -child.half);
    child.cy = p.cy + ((quadrant & 2) != 0 ? child.half : -child.half);
    m_nodes.push_back(child);
    const auto index = static_cast<std::int32_t>(m_nodes.size() - 1);
    m_nodes[static_cast<std::size_t>(parent)].child.at(static_cast<std::size_t>(quadrant)) = index;
    return index;
}

void QuadTree::insert(std::uint32_t index)
{
    const Vec2 p = m_points[index];
    std::int32_t cur = 0;
    int depth = 0;
    for (;;) {
        Node& n = m_nodes[static_cast<std::size_t>(cur)];
        const double total = n.mass + 1.0;
        n.comX = ((n.comX * n.mass) + p.x) / total;
        n.comY = ((n.comY * n.mass) + p.y) / total;
        n.mass = total;

        const bool isLeaf = n.child[0] < 0 && n.child[1] < 0 && n.child[2] < 0 && n.child[3] < 0;
        if (isLeaf) {
            if (n.head < 0) {
                n.head = static_cast<std::int32_t>(index);
                return;
            }
            if (depth >= kMaxDepth) {
                m_next[index] = n.head; // share the leaf
                n.head = static_cast<std::int32_t>(index);
                return;
            }
            // Split: push the single resident point one level down, then keep descending.
            const auto resident = static_cast<std::uint32_t>(n.head);
            n.head = -1;
            const Vec2 rp = m_points[resident];
            const int rq = quadrantOf(rp.x, rp.y, n.cx, n.cy);
            const std::int32_t rc = makeChild(cur, rq);
            Node& leaf = m_nodes[static_cast<std::size_t>(rc)];
            leaf.head = static_cast<std::int32_t>(resident);
            leaf.mass = 1.0;
            leaf.comX = rp.x;
            leaf.comY = rp.y;
        }

        // Re-fetch: makeChild may have reallocated the array.
        const Node& inner = m_nodes[static_cast<std::size_t>(cur)];
        const int q = quadrantOf(p.x, p.y, inner.cx, inner.cy);
        std::int32_t next = inner.child.at(static_cast<std::size_t>(q));
        if (next < 0) {
            next = makeChild(cur, q);
        }
        cur = next;
        ++depth;
    }
}

Vec2 QuadTree::centerOfMass() const
{
    if (m_nodes.empty()) {
        return {};
    }
    return {m_nodes[0].comX, m_nodes[0].comY};
}

Vec2 QuadTree::repulsion(Vec2 p, std::uint32_t selfIndex, double theta, double minDistance) const
{
    if (m_nodes.empty()) {
        return {};
    }
    const double minDist2 = minDistance * minDistance;
    const double theta2 = theta * theta;
    double fx = 0.0;
    double fy = 0.0;

    std::array<std::int32_t, kStackSize> stack{};
    std::size_t top = 0;
    stack.at(top++) = 0;
    while (top > 0) {
        const Node& n = m_nodes[static_cast<std::size_t>(stack.at(--top))];
        if (n.mass <= 0.0) {
            continue;
        }
        const bool isLeaf = n.child[0] < 0 && n.child[1] < 0 && n.child[2] < 0 && n.child[3] < 0;
        if (isLeaf) {
            for (std::int32_t b = n.head; b >= 0; b = m_next[static_cast<std::size_t>(b)]) {
                const auto other = static_cast<std::uint32_t>(b);
                if (other == selfIndex) {
                    continue;
                }
                const Vec2 q = m_points[other];
                double dx = p.x - q.x;
                double dy = p.y - q.y;
                double d2 = (dx * dx) + (dy * dy);
                if (d2 == 0.0) {
                    const Vec2 dir = coincidentDirection(selfIndex, other);
                    dx = dir.x;
                    dy = dir.y;
                    d2 = 1.0;
                }
                const double l2 = softened(d2, minDist2);
                fx += dx / l2;
                fy += dy / l2;
            }
            continue;
        }

        const double dx = p.x - n.comX;
        const double dy = p.y - n.comY;
        const double d2 = (dx * dx) + (dy * dy);
        const bool inside = std::abs(p.x - n.cx) <= n.half && std::abs(p.y - n.cy) <= n.half;
        const double side = n.half * 2.0;
        if (!inside && side * side < theta2 * d2) {
            const double l2 = softened(d2, minDist2);
            fx += n.mass * dx / l2;
            fy += n.mass * dy / l2;
            continue;
        }
        for (const std::int32_t c : n.child) {
            if (c >= 0) {
                stack.at(top++) = c;
            }
        }
    }
    return {fx, fy};
}

} // namespace nexus::core
