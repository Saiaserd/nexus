#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/graph/QuadTree.h"
#include "core/graph/Vec2.h"

namespace nexus::core {

// Tunable constants of the layout. The defaults follow d3-force.
struct ForceLayoutParams {
    double theta = 0.9;           // Barnes-Hut opening angle; 0 = exact O(n^2)
    double repulsion = 30.0;      // node-node repulsion strength (inverse-distance kernel)
    double minDistance = 1.0;     // repulsion is softened below this distance
    double linkDistance = 30.0;   // rest length of an edge
    double linkStrength = 0.0;    // <= 0: automatic, 1 / min(degree a, degree b) as in d3
    double centerStrength = 0.05; // weak pull of every node towards the origin
    double velocityDecay = 0.6;   // fraction of velocity lost per step
    double alphaMin = 0.001;      // settled when alpha drops below this
    double alphaDecay = 0.0228;   // 1 - alphaMin^(1/300): about 300 steps from 1 to alphaMin
    double alphaTarget = 0.0;     // alpha cools towards this (raise while dragging)
};

// Force-directed graph layout (T-301): Barnes-Hut repulsion, spring edges, weak centring and
// d3-force style alpha cooling. Pure standard C++, no Qt.
//
// Nodes are identified by their index 0..nodeCount()-1. Edges are undirected; self loops and
// duplicate edges are ignored. The UI calls step() once per frame and reads positions()
// directly (the span is invalidated by any structural change).
//
// Determinism: for the same seed and the same sequence of calls the result is identical on
// one platform. Random numbers come from a raw std::mt19937 (no <random> distributions) and
// only the transcendental functions may differ in the last bits between platforms.
class ForceLayout {
public:
    explicit ForceLayout(std::uint32_t seed = 1);
    // Convenience: nodeCount nodes and the given edges, ready to step().
    ForceLayout(std::uint32_t nodeCount,
                std::span<const std::pair<std::uint32_t, std::uint32_t>> edges,
                std::uint32_t seed = 1);

    // ---- structure (every change reheats the simulation, existing positions are kept) ----
    // Appends a node and returns its index. The overload without a position places it on a
    // golden-angle spiral around the origin.
    std::uint32_t addNode();
    std::uint32_t addNode(Vec2 position);
    // Removes node i; nodes with a higher index shift down by one and edges touching i go away.
    // Returns false when i is out of range.
    bool removeNode(std::uint32_t i);
    // Returns false for self loops, duplicates and out-of-range indices.
    bool addEdge(std::uint32_t a, std::uint32_t b);
    // Returns false when the edge does not exist.
    bool removeEdge(std::uint32_t a, std::uint32_t b);

    // ---- simulation ----
    // Advances one frame. Does nothing (and is cheap) once isSettled().
    void step();
    [[nodiscard]] bool isSettled() const { return m_alpha < m_params.alphaMin; }
    // Raises alpha to at least `alpha` (never lowers it); call after dragging or edits.
    void reheat(double alpha = 0.5);
    [[nodiscard]] double alpha() const { return m_alpha; }

    // Fixes node i at (x, y) until unpin(); used while dragging. The node keeps taking part in
    // the forces of the other nodes. Does not reheat by itself.
    bool pin(std::uint32_t i, double x, double y);
    bool unpin(std::uint32_t i);
    [[nodiscard]] bool isPinned(std::uint32_t i) const;

    // ---- access ----
    [[nodiscard]] std::size_t nodeCount() const { return m_positions.size(); }
    [[nodiscard]] std::size_t edgeCount() const { return m_edges.size(); }
    [[nodiscard]] std::span<const Vec2> positions() const { return m_positions; }
    [[nodiscard]] const ForceLayoutParams& params() const { return m_params; }
    // Parameters may change at any time; they take effect on the next step.
    void setParams(const ForceLayoutParams& params);

private:
    struct Edge {
        std::uint32_t a;
        std::uint32_t b;
    };

    [[nodiscard]] static std::uint64_t edgeKey(std::uint32_t a, std::uint32_t b);
    [[nodiscard]] double nextUnit(); // uniform in [0, 1) from the raw generator
    [[nodiscard]] double jiggle();   // tiny deterministic offset to separate coincident points
    void rebuildEdgeIndex();
    void refreshDegrees();

    std::mt19937 m_rng;
    double m_spiralOffset = 0.0;
    ForceLayoutParams m_params;
    double m_alpha = 1.0;

    std::vector<Vec2> m_positions;
    std::vector<Vec2> m_velocities;
    std::vector<char> m_pinned;
    std::vector<Edge> m_edges;
    std::unordered_set<std::uint64_t> m_edgeSet;
    std::vector<std::uint32_t> m_degree;
    bool m_degreesDirty = true;
    QuadTree m_tree;
};

} // namespace nexus::core
