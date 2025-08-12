#ifndef KDTREE3D_H
#define KDTREE3D_H

#include <cmath>
#include <vector>
#include <limits>
#include <algorithm>
#include "IcoSphere.hpp" // Pour Vec3

struct KDNode {
    Vec3 point;
    size_t index;
    size_t left = (size_t)(-1);  // Indice du fils gauche dans le vecteur, ou -1 si nul
    size_t right = (size_t)(-1); // Indice du fils droit dans le vecteur, ou -1 si nul

    KDNode(const Vec3& pt, size_t idx) : point(pt), index(idx) {}
};

class KDTree3D {
public:
    KDTree3D(const std::vector<Vec3>& points) {
        std::vector<size_t> indices(points.size());
        for (size_t i = 0; i < points.size(); ++i)
            indices[i] = i;
        rootIndex = build(points, indices, 0, points.size(), 0);
    }

    size_t nearestNeighbor(const Vec3& target) const {
        size_t bestIndex = 0;
        float bestDistSq = std::numeric_limits<float>::max();
        nearest(rootIndex, target, 0, bestIndex, bestDistSq);
        return bestIndex;
    }

private:
    std::vector<KDNode> nodes;
    size_t rootIndex = (size_t)(-1);

    size_t build(const std::vector<Vec3>& points, std::vector<size_t>& indices, int start, int end, int depth) {
        if (start >= end) return (size_t)(-1);

        int axis = depth % 3;

        auto comp = [&](size_t a, size_t b) {
            if (axis == 0) return points[a].x < points[b].x;
            else if (axis == 1) return points[a].y < points[b].y;
            else return points[a].z < points[b].z;
        };

        size_t median = (start + end) / 2;
        std::nth_element(indices.begin() + start, indices.begin() + median, indices.begin() + end, comp);

        nodes.emplace_back(points[indices[median]], indices[median]);
        size_t nodeIdx = nodes.size() - 1;

        nodes[nodeIdx].left = build(points, indices, start, median, depth + 1);
        nodes[nodeIdx].right = build(points, indices, median + 1, end, depth + 1);
        return nodeIdx;
    }

    void nearest(size_t nodeIdx, const Vec3& target, int depth, size_t& bestIndex, float& bestDistSq) const {
        if (nodeIdx == (size_t)(-1)) return;

        const KDNode& node = nodes[nodeIdx];
        float distSq = (node.point - target).lengthSquared();

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIndex = node.index;
        }

        int axis = depth % 3;
        float diff = 0.0f;
        if (axis == 0) diff = target.x - node.point.x;
        else if (axis == 1) diff = target.y - node.point.y;
        else diff = target.z - node.point.z;

        size_t nearChild = diff < 0 ? node.left : node.right;
        size_t farChild = diff < 0 ? node.right : node.left;

        nearest(nearChild, target, depth + 1, bestIndex, bestDistSq);

        if ((diff * diff) < bestDistSq) {
            nearest(farChild, target, depth + 1, bestIndex, bestDistSq);
        }
    }
};

#endif // KDTREE3D_H
