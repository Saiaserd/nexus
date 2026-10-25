#include "core/graph/ForceLayout.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace nexus::core {

namespace {

constexpr double kGoldenAngle = 2.399963229728653; // pi * (3 - sqrt(5))
constexpr double kTwoPi = 6.283185307179586;
constexpr double kSpiralRadius = 10.0;
constexpr double kJiggleScale = 1e-6;
constexpr double kRngRange = 4294967296.0; // 2^32

} // namespace

ForceLayout::ForceLayout(std::uint32_t seed) : m_rng(seed), m_spiralOffset(nextUnit() * kTwoPi) {}

ForceLayout::ForceLayout(std::uint32_t nodeCount,
                         std::span<const std::pair<std::uint32_t, std::uint32_t>> edges,
                         std::uint32_t seed)
    : ForceLayout(seed)
{
    m_positions.reserve(nodeCount);
    for (std::uint32_t i = 0; i < nodeCount; ++i) {
        addNode();
    }
    for (const auto& [a, b] : edges) {
        addEdge(a, b);
    }
}

double ForceLayout::nextUnit()
{
    return static_cast<double>(m_rng()) / kRngRange;
}

double ForceLayout::jiggle()
{
    return (nextUnit() - 0.5) * kJiggleScale;
}

std::uint64_t ForceLayout::edgeKey(std::uint32_t a, std::uint32_t b)
{
    const auto lo = static_cast<std::uint64_t>(std::min(a, b));
    const auto hi = static_cast<std::uint64_t>(std::max(a, b));
    return (hi << 32U) | lo;
}

std::uint32_t ForceLayout::addNode()
{
    const auto i = static_cast<double>(m_positions.size());
    const double radius = kSpiralRadius * std::sqrt(0.5 + i);
    const double angle = (i * kGoldenAngle) + m_spiralOffset;
    return addNode({radius * std::cos(angle), radius * std::sin(angle)});
}

std::uint32_t ForceLayout::addNode(Vec2 position)
{
    m_positions.push_back(position);
    m_velocities.push_back({});
    m_pinned.push_back(0);
    m_degree.push_back(0);
    reheat();
    return static_cast<std::uint32_t>(m_positions.size() - 1);
}

bool ForceLayout::removeNode(std::uint32_t i)
{
    if (i >= m_positions.size()) {
        return false;
    }
    const auto at = static_cast<std::ptrdiff_t>(i);
    m_positions.erase(m_positions.begin() + at);
    m_velocities.erase(m_velocities.begin() + at);
    m_pinned.erase(m_pinned.begin() + at);
    m_degree.pop_back(); // recomputed lazily
    std::erase_if(m_edges, [i](const Edge& e) { return e.a == i || e.b == i; });
    for (Edge& e : m_edges) {
        e.a -= e.a > i ? 1U : 0U;
        e.b -= e.b > i ? 1U : 0U;
    }
    rebuildEdgeIndex();
    m_degreesDirty = true;
    reheat();
    return true;
}

bool ForceLayout::addEdge(std::uint32_t a, std::uint32_t b)
{
    if (a == b || a >= m_positions.size() || b >= m_positions.size()) {
        return false;
    }
    if (!m_edgeSet.insert(edgeKey(a, b)).second) {
        return false;
    }
    m_edges.push_back({a, b});
    m_degreesDirty = true;
    reheat();
    return true;
}

bool ForceLayout::removeEdge(std::uint32_t a, std::uint32_t b)
{
    if (m_edgeSet.erase(edgeKey(a, b)) == 0) {
        return false;
    }
    std::erase_if(m_edges, [a, b](const Edge& e) {
        return (e.a == a && e.b == b) || (e.a == b && e.b == a);
    });
    m_degreesDirty = true;
    reheat();
    return true;
}

void ForceLayout::rebuildEdgeIndex()
{
    m_edgeSet.clear();
    m_edgeSet.reserve(m_edges.size());
    for (const Edge& e : m_edges) {
        m_edgeSet.insert(edgeKey(e.a, e.b));
    }
}

void ForceLayout::refreshDegrees()
{
    m_degree.assign(m_positions.size(), 0);
    for (const Edge& e : m_edges) {
        ++m_degree[e.a];
        ++m_degree[e.b];
    }
    m_degreesDirty = false;
}

void ForceLayout::reheat(double alpha)
{
    m_alpha = std::max(m_alpha, alpha);
}

void ForceLayout::setParams(const ForceLayoutParams& params)
{
    m_params = params;
}

bool ForceLayout::pin(std::uint32_t i, double x, double y)
{
    if (i >= m_positions.size()) {
        return false;
    }
    m_positions[i] = {x, y};
    m_velocities[i] = {};
    m_pinned[i] = 1;
    return true;
}

bool ForceLayout::unpin(std::uint32_t i)
{
    if (i >= m_positions.size()) {
        return false;
    }
    m_pinned[i] = 0;
    return true;
}

bool ForceLayout::isPinned(std::uint32_t i) const
{
    return i < m_pinned.size() && m_pinned[i] != 0;
}

void ForceLayout::step()
{
    if (isSettled() || m_positions.empty()) {
        return;
    }
    if (m_degreesDirty) {
        refreshDegrees();
    }
    const ForceLayoutParams& prm = m_params;
    m_alpha += (prm.alphaTarget - m_alpha) * prm.alphaDecay;
    const double alpha = m_alpha;

    // Springs (d3 forceLink): move both ends' velocities towards the rest length.
    for (const Edge& e : m_edges) {
        Vec2& va = m_velocities[e.a];
        Vec2& vb = m_velocities[e.b];
        double dx = (m_positions[e.b].x + vb.x) - (m_positions[e.a].x + va.x);
        double dy = (m_positions[e.b].y + vb.y) - (m_positions[e.a].y + va.y);
        if (dx == 0.0) {
            dx = jiggle();
        }
        if (dy == 0.0) {
            dy = jiggle();
        }
        const double len = std::sqrt((dx * dx) + (dy * dy));
        if (len == 0.0) {
            continue; // only if the jiggle itself came out as exactly zero
        }
        const double da = m_degree[e.a];
        const double db = m_degree[e.b];
        const double strength = prm.linkStrength > 0.0 ? prm.linkStrength : 1.0 / std::min(da, db);
        const double f = (len - prm.linkDistance) / len * alpha * strength;
        dx *= f;
        dy *= f;
        const double bias = da / (da + db);
        vb.x -= dx * bias;
        vb.y -= dy * bias;
        va.x += dx * (1.0 - bias);
        va.y += dy * (1.0 - bias);
    }

    // Repulsion (Barnes-Hut) and the weak pull towards the origin.
    m_tree.build(m_positions);
    const double repel = prm.repulsion * alpha;
    const double centre = prm.centerStrength * alpha;
    for (std::uint32_t i = 0; i < m_positions.size(); ++i) {
        const Vec2 p = m_positions[i];
        const Vec2 away = m_tree.repulsion(p, i, prm.theta, prm.minDistance);
        Vec2& v = m_velocities[i];
        v.x += (away.x * repel) - (p.x * centre);
        v.y += (away.y * repel) - (p.y * centre);
    }

    // Integrate. Pinned nodes stay put.
    const double keep = 1.0 - prm.velocityDecay;
    for (std::uint32_t i = 0; i < m_positions.size(); ++i) {
        Vec2& v = m_velocities[i];
        if (m_pinned[i] != 0) {
            v = {};
            continue;
        }
        v.x *= keep;
        v.y *= keep;
        m_positions[i].x += v.x;
        m_positions[i].y += v.y;
    }
}

} // namespace nexus::core
