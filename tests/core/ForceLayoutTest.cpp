#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "core/graph/ForceLayout.h"
#include "core/graph/QuadTree.h"

using nexus::core::ForceLayout;
using nexus::core::ForceLayoutParams;
using nexus::core::QuadTree;
using nexus::core::Vec2;

namespace {

using EdgeList = std::vector<std::pair<std::uint32_t, std::uint32_t>>;

// Raw mt19937 only (distributions are not portable), fixed seed.
EdgeList randomEdges(std::uint32_t nodes, std::uint32_t edges, std::uint32_t seed)
{
    std::mt19937 rng(seed);
    EdgeList out;
    out.reserve(edges);
    while (out.size() < edges) {
        const std::uint32_t a = rng() % nodes;
        const std::uint32_t b = rng() % nodes;
        if (a != b) {
            out.emplace_back(a, b); // duplicates are ignored by ForceLayout
        }
    }
    return out;
}

// Ten dense-ish communities with a few links between neighbouring ones.
EdgeList communityEdges(std::uint32_t communities, std::uint32_t size, std::uint32_t seed)
{
    std::mt19937 rng(seed);
    EdgeList out;
    for (std::uint32_t c = 0; c < communities; ++c) {
        const std::uint32_t base = c * size;
        for (std::uint32_t i = 1; i < size; ++i) {
            out.emplace_back(base + i, base + (rng() % i)); // spanning tree
        }
        for (std::uint32_t k = 0; k < size; ++k) {
            const std::uint32_t a = base + (rng() % size);
            const std::uint32_t b = base + (rng() % size);
            if (a != b) {
                out.emplace_back(a, b);
            }
        }
        out.emplace_back(base, ((c + 1) % communities) * size);
    }
    return out;
}

bool allFinite(std::span<const Vec2> points)
{
    return std::ranges::all_of(
        points, [](const Vec2& p) { return std::isfinite(p.x) && std::isfinite(p.y); });
}

double distance(Vec2 a, Vec2 b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

void run(ForceLayout& layout, int steps)
{
    for (int i = 0; i < steps; ++i) {
        layout.step();
    }
}

Vec2 bruteRepulsion(std::span<const Vec2> pts, std::uint32_t self, double minDistance)
{
    Vec2 f;
    const double min2 = minDistance * minDistance;
    for (std::uint32_t j = 0; j < pts.size(); ++j) {
        if (j == self) {
            continue;
        }
        const double dx = pts[self].x - pts[j].x;
        const double dy = pts[self].y - pts[j].y;
        double d2 = (dx * dx) + (dy * dy);
        if (d2 < min2) {
            d2 = std::sqrt(min2 * d2);
        }
        f.x += dx / d2;
        f.y += dy / d2;
    }
    return f;
}

std::vector<Vec2> randomPoints(std::uint32_t n, std::uint32_t seed, double extent)
{
    std::mt19937 rng(seed);
    std::vector<Vec2> pts(n);
    for (Vec2& p : pts) {
        p.x = ((static_cast<double>(rng()) / 4294967296.0) - 0.5) * extent;
        p.y = ((static_cast<double>(rng()) / 4294967296.0) - 0.5) * extent;
    }
    return pts;
}

} // namespace

// ---- QuadTree ---------------------------------------------------------------------------

TEST(QuadTreeTest, ThetaZeroMatchesBruteForce)
{
    const auto pts = randomPoints(300, 7, 500.0);
    QuadTree tree;
    tree.build(pts);
    EXPECT_EQ(tree.pointCount(), pts.size());
    for (std::uint32_t i = 0; i < pts.size(); ++i) {
        const Vec2 got = tree.repulsion(pts[i], i, 0.0, 1.0);
        const Vec2 want = bruteRepulsion(pts, i, 1.0);
        EXPECT_NEAR(got.x, want.x, 1e-9);
        EXPECT_NEAR(got.y, want.y, 1e-9);
    }
}

TEST(QuadTreeTest, DefaultThetaStaysCloseToBruteForce)
{
    const auto pts = randomPoints(500, 11, 800.0);
    QuadTree tree;
    tree.build(pts);
    double errorSum = 0.0;
    double magnitudeSum = 0.0;
    for (std::uint32_t i = 0; i < pts.size(); ++i) {
        const Vec2 got = tree.repulsion(pts[i], i, 0.9, 1.0);
        const Vec2 want = bruteRepulsion(pts, i, 1.0);
        errorSum += std::hypot(got.x - want.x, got.y - want.y);
        magnitudeSum += std::hypot(want.x, want.y);
    }
    EXPECT_LT(errorSum / magnitudeSum, 0.15);
}

TEST(QuadTreeTest, CenterOfMassIsTheMean)
{
    const std::vector<Vec2> pts = {{0, 0}, {10, 0}, {0, 20}, {10, 20}};
    QuadTree tree;
    tree.build(pts);
    EXPECT_NEAR(tree.centerOfMass().x, 5.0, 1e-12);
    EXPECT_NEAR(tree.centerOfMass().y, 10.0, 1e-12);
}

TEST(QuadTreeTest, EmptyAndSinglePoint)
{
    QuadTree tree;
    tree.build({});
    EXPECT_EQ(tree.repulsion({1, 1}, QuadTree::kNoSelf, 0.9, 1.0), Vec2{});
    const std::vector<Vec2> one = {{3, 4}};
    tree.build(one);
    EXPECT_EQ(tree.repulsion(one[0], 0, 0.9, 1.0), Vec2{}); // only itself
    const Vec2 away = tree.repulsion({4, 4}, QuadTree::kNoSelf, 0.9, 1.0);
    EXPECT_GT(away.x, 0.0);
}

TEST(QuadTreeTest, ThousandCoincidentPointsAreHandled)
{
    const std::vector<Vec2> pts(1000, Vec2{5.0, 5.0});
    QuadTree tree;
    tree.build(pts);
    const Vec2 f = tree.repulsion(pts[10], 10, 0.9, 1.0);
    EXPECT_TRUE(std::isfinite(f.x) && std::isfinite(f.y));
    EXPECT_GT(std::hypot(f.x, f.y), 0.0); // pushed apart in some direction, self excluded
}

// ---- ForceLayout: structure -------------------------------------------------------------

TEST(ForceLayoutTest, EmptyLayoutStepsSafely)
{
    ForceLayout layout;
    layout.step();
    EXPECT_EQ(layout.nodeCount(), 0U);
    EXPECT_TRUE(layout.positions().empty());
}

TEST(ForceLayoutTest, ConstructorBuildsNodesAndEdges)
{
    const EdgeList edges = {{0, 1}, {1, 2}, {1, 0}, {2, 2}, {2, 9}};
    ForceLayout layout(4, edges);
    EXPECT_EQ(layout.nodeCount(), 4U);
    EXPECT_EQ(layout.edgeCount(), 2U); // duplicate, self loop and out-of-range are dropped
    EXPECT_EQ(layout.positions().size(), 4U);
    EXPECT_TRUE(allFinite(layout.positions()));
}

TEST(ForceLayoutTest, EdgeValidation)
{
    ForceLayout layout;
    layout.addNode();
    layout.addNode();
    EXPECT_FALSE(layout.addEdge(0, 0));
    EXPECT_FALSE(layout.addEdge(0, 5));
    EXPECT_TRUE(layout.addEdge(0, 1));
    EXPECT_FALSE(layout.addEdge(1, 0));
    EXPECT_EQ(layout.edgeCount(), 1U);
    EXPECT_FALSE(layout.removeEdge(0, 0));
    EXPECT_TRUE(layout.removeEdge(1, 0));
    EXPECT_FALSE(layout.removeEdge(0, 1));
    EXPECT_EQ(layout.edgeCount(), 0U);
    EXPECT_TRUE(layout.addEdge(0, 1)); // can be added again
}

TEST(ForceLayoutTest, AddingNodesKeepsExistingPositions)
{
    ForceLayout layout(1);
    for (int i = 0; i < 20; ++i) {
        layout.addNode();
    }
    run(layout, 50);
    const std::vector<Vec2> before(layout.positions().begin(), layout.positions().end());
    const auto fresh = layout.addNode({100.0, -50.0});
    EXPECT_EQ(fresh, 20U);
    ASSERT_EQ(layout.nodeCount(), 21U);
    for (std::size_t i = 0; i < before.size(); ++i) {
        EXPECT_EQ(layout.positions()[i], before[i]);
    }
    EXPECT_EQ(layout.positions()[20], (Vec2{100.0, -50.0}));
}

TEST(ForceLayoutTest, RemoveNodeShiftsIndicesAndDropsEdges)
{
    ForceLayout layout;
    for (int i = 0; i < 4; ++i) {
        layout.addNode({static_cast<double>(i) * 10.0, 0.0});
    }
    layout.addEdge(0, 1);
    layout.addEdge(1, 2);
    layout.addEdge(2, 3);
    EXPECT_FALSE(layout.removeNode(9));
    EXPECT_TRUE(layout.removeNode(1));
    EXPECT_EQ(layout.nodeCount(), 3U);
    EXPECT_EQ(layout.edgeCount(), 1U); // only 2-3 survives, now 1-2
    EXPECT_EQ(layout.positions()[0], (Vec2{0.0, 0.0}));
    EXPECT_EQ(layout.positions()[1], (Vec2{20.0, 0.0}));
    EXPECT_EQ(layout.positions()[2], (Vec2{30.0, 0.0}));
    EXPECT_FALSE(layout.addEdge(2, 1)); // exists already after the shift
    EXPECT_TRUE(layout.removeEdge(1, 2));
    run(layout, 20);
    EXPECT_TRUE(allFinite(layout.positions()));
}

// ---- ForceLayout: simulation ------------------------------------------------------------

TEST(ForceLayoutTest, SameSeedGivesIdenticalResults)
{
    const EdgeList edges = randomEdges(150, 300, 5);
    ForceLayout a(150, edges, 42);
    ForceLayout b(150, edges, 42);
    ForceLayout c(150, edges, 43);
    run(a, 60);
    run(b, 60);
    run(c, 60);
    ASSERT_EQ(a.positions().size(), b.positions().size());
    for (std::size_t i = 0; i < a.positions().size(); ++i) {
        EXPECT_EQ(a.positions()[i], b.positions()[i]);
    }
    bool differs = false;
    for (std::size_t i = 0; i < a.positions().size(); ++i) {
        differs = differs || !(a.positions()[i] == c.positions()[i]);
    }
    EXPECT_TRUE(differs);
}

TEST(ForceLayoutTest, SettlesAndThenStopsMoving)
{
    const EdgeList edges = randomEdges(100, 150, 3);
    ForceLayout layout(100, edges);
    EXPECT_FALSE(layout.isSettled());
    int steps = 0;
    while (!layout.isSettled() && steps < 1000) {
        layout.step();
        ++steps;
    }
    EXPECT_TRUE(layout.isSettled());
    EXPECT_LT(steps, 500); // alphaDecay 0.0228 -> about 300 steps
    const std::vector<Vec2> before(layout.positions().begin(), layout.positions().end());
    run(layout, 10);
    for (std::size_t i = 0; i < before.size(); ++i) {
        EXPECT_EQ(layout.positions()[i], before[i]);
    }
}

TEST(ForceLayoutTest, ReheatWakesUpASettledLayout)
{
    const EdgeList edges = randomEdges(30, 40, 9);
    ForceLayout layout(30, edges);
    run(layout, 1000);
    ASSERT_TRUE(layout.isSettled());
    layout.reheat();
    EXPECT_FALSE(layout.isSettled());
    const Vec2 before = layout.positions()[0];
    run(layout, 5);
    EXPECT_FALSE(layout.positions()[0] == before);
    // reheat never lowers alpha
    const double alpha = layout.alpha();
    layout.reheat(0.0001);
    EXPECT_EQ(layout.alpha(), alpha);
}

TEST(ForceLayoutTest, StructureChangesReheat)
{
    ForceLayout layout(2, EdgeList{{0, 1}});
    run(layout, 1000);
    ASSERT_TRUE(layout.isSettled());
    layout.addNode();
    EXPECT_FALSE(layout.isSettled());
    run(layout, 1000);
    ASSERT_TRUE(layout.isSettled());
    layout.addEdge(1, 2);
    EXPECT_FALSE(layout.isSettled());
}

TEST(ForceLayoutTest, AlphaTargetPreventsSettling)
{
    ForceLayout layout(10, EdgeList{{0, 1}, {1, 2}});
    ForceLayoutParams params = layout.params();
    params.alphaTarget = 0.3;
    layout.setParams(params);
    run(layout, 1000);
    EXPECT_FALSE(layout.isSettled());
    EXPECT_NEAR(layout.alpha(), 0.3, 1e-3);
}

TEST(ForceLayoutTest, ValuesStayFiniteOnARandomGraph)
{
    const EdgeList edges = randomEdges(400, 1200, 21);
    ForceLayout layout(400, edges);
    for (int i = 0; i < 400; ++i) {
        layout.step();
        ASSERT_TRUE(allFinite(layout.positions())) << "step " << i;
    }
}

TEST(ForceLayoutTest, OverlappingNodesSeparateWithoutExploding)
{
    ForceLayout layout;
    for (int i = 0; i < 50; ++i) {
        layout.addNode({3.0, 3.0}); // all on one spot
    }
    layout.addEdge(0, 1);
    layout.addEdge(1, 2);
    run(layout, 400);
    ASSERT_TRUE(allFinite(layout.positions()));
    double maxRadius = 0.0;
    double minGap = 1e300;
    for (std::size_t i = 0; i < layout.nodeCount(); ++i) {
        maxRadius =
            std::max(maxRadius, std::hypot(layout.positions()[i].x, layout.positions()[i].y));
        for (std::size_t j = i + 1; j < layout.nodeCount(); ++j) {
            minGap = std::min(minGap, distance(layout.positions()[i], layout.positions()[j]));
        }
    }
    EXPECT_LT(maxRadius, 1000.0);
    EXPECT_GT(minGap, 0.01);
}

TEST(ForceLayoutTest, TwoOverlappingConnectedNodesDoNotBlowUp)
{
    ForceLayout layout;
    layout.addNode({0.0, 0.0});
    layout.addNode({0.0, 0.0});
    layout.addEdge(0, 1);
    run(layout, 300);
    ASSERT_TRUE(allFinite(layout.positions()));
    const double d = distance(layout.positions()[0], layout.positions()[1]);
    EXPECT_GT(d, 1.0);
    EXPECT_LT(d, 200.0);
}

TEST(ForceLayoutTest, ConnectedNodesEndUpCloserThanRandomPairs)
{
    const EdgeList edges = communityEdges(10, 20, 17);
    ForceLayout layout(200, edges);
    run(layout, 500);
    const auto pts = layout.positions();

    double edgeSum = 0.0;
    for (const auto& [a, b] : edges) {
        edgeSum += distance(pts[a], pts[b]);
    }
    const double edgeMean = edgeSum / static_cast<double>(edges.size());

    std::mt19937 rng(99);
    double randomSum = 0.0;
    int pairs = 0;
    while (pairs < 2000) {
        const std::uint32_t a = rng() % 200;
        const std::uint32_t b = rng() % 200;
        if (a != b) {
            randomSum += distance(pts[a], pts[b]);
            ++pairs;
        }
    }
    const double randomMean = randomSum / pairs;
    EXPECT_LT(edgeMean, randomMean * 0.5) << edgeMean << " vs " << randomMean;
}

TEST(ForceLayoutTest, IsolatedNodesStayNearTheCentre)
{
    ForceLayout layout;
    for (int i = 0; i < 60; ++i) {
        layout.addNode();
    }
    run(layout, 600);
    double maxRadius = 0.0;
    for (const Vec2& p : layout.positions()) {
        maxRadius = std::max(maxRadius, std::hypot(p.x, p.y));
    }
    EXPECT_LT(maxRadius, 400.0);
}

TEST(ForceLayoutTest, PinnedNodeDoesNotMoveAndOthersReactToIt)
{
    const EdgeList edges = communityEdges(3, 10, 4);
    ForceLayout layout(30, edges);
    run(layout, 100);
    EXPECT_FALSE(layout.isPinned(5));
    ASSERT_TRUE(layout.pin(5, 250.0, -120.0));
    EXPECT_TRUE(layout.isPinned(5));
    layout.reheat(1.0);
    run(layout, 80);
    EXPECT_EQ(layout.positions()[5], (Vec2{250.0, -120.0}));

    // A neighbour of node 5 is dragged along towards it.
    std::uint32_t neighbour = 0;
    for (const auto& [a, b] : edges) {
        if (a == 5) {
            neighbour = b;
            break;
        }
        if (b == 5) {
            neighbour = a;
            break;
        }
    }
    EXPECT_LT(distance(layout.positions()[neighbour], layout.positions()[5]), 200.0);

    ASSERT_TRUE(layout.unpin(5));
    EXPECT_FALSE(layout.isPinned(5));
    layout.reheat(1.0);
    run(layout, 200);
    EXPECT_FALSE(layout.positions()[5] == (Vec2{250.0, -120.0}));
}

TEST(ForceLayoutTest, PinAndUnpinRejectBadIndices)
{
    ForceLayout layout(2, EdgeList{});
    EXPECT_FALSE(layout.pin(2, 0, 0));
    EXPECT_FALSE(layout.unpin(2));
    EXPECT_FALSE(layout.isPinned(2));
}

TEST(ForceLayoutTest, ThetaZeroAndDefaultGiveSimilarLayouts)
{
    const EdgeList edges = communityEdges(4, 15, 2);
    ForceLayout approx(60, edges, 7);
    ForceLayout exact(60, edges, 7);
    ForceLayoutParams params = exact.params();
    params.theta = 0.0;
    exact.setParams(params);
    run(approx, 300);
    run(exact, 300);
    ASSERT_TRUE(allFinite(exact.positions()));
    double edgeApprox = 0.0;
    double edgeExact = 0.0;
    for (const auto& [a, b] : edges) {
        edgeApprox += distance(approx.positions()[a], approx.positions()[b]);
        edgeExact += distance(exact.positions()[a], exact.positions()[b]);
    }
    EXPECT_NEAR(edgeApprox / edgeExact, 1.0, 0.25);
}

// ---- Performance (release only: see NEXUS_ENFORCE_PERF) ---------------------------------

namespace {

struct StepTiming {
    double averageMs = 0.0;
    double maxMs = 0.0;
};

StepTiming timeSteps(std::uint32_t nodes, std::uint32_t edges, int steps)
{
    const EdgeList list = randomEdges(nodes, edges, 1);
    ForceLayout layout(nodes, list);
    double total = 0.0;
    double worst = 0.0;
    for (int i = 0; i < steps; ++i) {
        const auto start = std::chrono::steady_clock::now();
        layout.step();
        const std::chrono::duration<double, std::milli> ms =
            std::chrono::steady_clock::now() - start;
        total += ms.count();
        worst = std::max(worst, ms.count());
    }
    EXPECT_TRUE(allFinite(layout.positions()));
    return {total / steps, worst};
}

} // namespace

TEST(ForceLayoutPerf, ThousandNodesStepTime)
{
    const StepTiming t = timeSteps(1000, 3000, 60);
    std::cout << "[perf] 1000 nodes / 3000 edges: avg " << t.averageMs << " ms, max " << t.maxMs
              << " ms per step\n";
#ifdef NEXUS_ENFORCE_PERF
    EXPECT_LT(t.averageMs, 4.0);
#endif
}

TEST(ForceLayoutPerf, TenThousandNodesStepTime)
{
    const StepTiming t = timeSteps(10000, 30000, 30);
    std::cout << "[perf] 10000 nodes / 30000 edges: avg " << t.averageMs << " ms, max " << t.maxMs
              << " ms per step\n";
#ifdef NEXUS_ENFORCE_PERF
    EXPECT_LT(t.averageMs, 60.0);
#endif
}
