#ifndef KDTREE3D_H
#define KDTREE3D_H

#include <cmath>
#include <memory>
#include <algorithm>
#include <vector>
#include "IcoSphere.hpp" // Pour Vec3

struct KDNode {
    Vec3 point;
    size_t index;
    KDNode* left = nullptr;
    KDNode* right = nullptr;

    KDNode(const Vec3& pt, size_t idx) : point(pt), index(idx) {}
};

class KDTree3D {
public:
    KDTree3D(const std::vector<Vec3>& points) {
        std::vector<size_t> indices(points.size());
        for (size_t i = 0; i < points.size(); ++i)
            indices[i] = i;
        root = build(points, indices, 0);
    }

    ~KDTree3D() {
        freeNode(root);
    }

    size_t nearestNeighbor(const Vec3& target) const {
        size_t bestIndex = 0;
        float bestDist = std::numeric_limits<float>::max();
        nearest(root, target, 0, bestIndex, bestDist);
        return bestIndex;
    }

private:
    KDNode* root;

    KDNode* build(const std::vector<Vec3>& points, std::vector<size_t>& indices, int depth) {
        if (indices.empty()) return nullptr;
        int axis = depth % 3;
        auto comp = [&](size_t a, size_t b) {
            if (axis == 0) return points[a].x < points[b].x;
            else if (axis == 1) return points[a].y < points[b].y;
            else return points[a].z < points[b].z;
        };
        std::sort(indices.begin(), indices.end(), comp);
        size_t median = indices.size() / 2;

        std::vector<size_t> leftIndices(indices.begin(), indices.begin() + median);
        std::vector<size_t> rightIndices(indices.begin() + median + 1, indices.end());

        KDNode* node = new KDNode(points[indices[median]], indices[median]);
        node->left = build(points, leftIndices, depth + 1);
        node->right = build(points, rightIndices, depth + 1);
        return node;
    }

    void freeNode(KDNode* node) {
        if (!node) return;
        freeNode(node->left);
        freeNode(node->right);
        delete node;
    }

    void nearest(KDNode* node, const Vec3& target, int depth, size_t& bestIndex, float& bestDist) const {
        if (!node) return;
        float dist = (node->point - target).length();
        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = node->index;
        }
        int axis = depth % 3;
        float diff = 0.0f;
        if (axis == 0) diff = target.x - node->point.x;
        else if (axis == 1) diff = target.y - node->point.y;
        else diff = target.z - node->point.z;

        KDNode* nearChild = diff < 0 ? node->left : node->right;
        KDNode* farChild = diff < 0 ? node->right : node->left;

        nearest(nearChild, target, depth + 1, bestIndex, bestDist);
        if (fabs(diff) < bestDist) {
            nearest(farChild, target, depth + 1, bestIndex, bestDist);
        }
    }
};

#endif // KDTREE3D_H