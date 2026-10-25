#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/graph/Vec2.h"

namespace nexus::core {

// Barnes-Hut quadtree over a set of unit-mass points (T-301). It is rebuilt every layout step,
// so the nodes live in one flat array that keeps its capacity between builds.
//
// The tree only stores indices into the span given to build(); the caller must keep that span
// alive and unchanged while querying.
class QuadTree {
public:
    static constexpr std::uint32_t kNoSelf = 0xFFFFFFFFU;
    // Points deeper than this share one leaf (only reachable with (almost) coincident points).
    static constexpr int kMaxDepth = 32;

    // Rebuild the tree. Non-finite points must not be passed in.
    void build(std::span<const Vec2> points);

    // Sum over every other point q of (p - q) / max(|p - q|^2, minDistance^2), i.e. an
    // inverse-distance repulsion direction. `selfIndex` is skipped (use kNoSelf for a free
    // probe). A cell of size s at distance d from p is replaced by its centre of mass when
    // s / d < theta; theta = 0 gives the exact sum. Points that coincide exactly with p are
    // pushed apart in a deterministic pseudo direction derived from the two indices.
    [[nodiscard]] Vec2 repulsion(Vec2 p, std::uint32_t selfIndex, double theta,
                                 double minDistance) const;

    [[nodiscard]] std::size_t pointCount() const { return m_next.size(); }
    [[nodiscard]] std::size_t nodeCount() const { return m_nodes.size(); }
    [[nodiscard]] Vec2 centerOfMass() const;

private:
    struct Node {
        double comX = 0.0; // centre of mass
        double comY = 0.0;
        double mass = 0.0;
        double cx = 0.0; // cell centre
        double cy = 0.0;
        double half = 0.0; // half of the cell's side length
        std::array<std::int32_t, 4> child = {-1, -1, -1, -1};
        std::int32_t head = -1; // leaf: first point of the chain (linked through m_next)
    };

    [[nodiscard]] std::int32_t makeChild(std::int32_t parent, int quadrant);
    void insert(std::uint32_t index);

    std::span<const Vec2> m_points;
    std::vector<Node> m_nodes;
    std::vector<std::int32_t> m_next; // chain of points that share a leaf, -1 terminates
};

} // namespace nexus::core
