#ifndef FASTSPATIALLOOKUP_H
#define FASTSPATIALLOOKUP_H

#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstdint>

#include "IcoSphere.hpp" // Pour Vec3
class FastSpatialLookup {
private:
    std::unordered_map<uint64_t, std::vector<size_t>> grid;
    std::vector<Vec3> points;
    float cellSize;
    
    uint64_t hash(const Vec3& p) const {
        int x = static_cast<int>(std::floor(p.x / cellSize));
        int y = static_cast<int>(std::floor(p.y / cellSize));
        int z = static_cast<int>(std::floor(p.z / cellSize));
        return (static_cast<uint64_t>(x & 0x1FFFFF) << 42) |
               (static_cast<uint64_t>(y & 0x1FFFFF) << 21) |
               static_cast<uint64_t>(z & 0x1FFFFF);
    }
    
public:
    FastSpatialLookup(const std::vector<Vec3>& pts, float gridSize = 0.1f) 
        : points(pts), cellSize(gridSize) {
        grid.reserve(pts.size() / 4); // Estimation
        
        for (size_t i = 0; i < pts.size(); ++i) {
            grid[hash(pts[i])].push_back(i);
        }
    }
    
    size_t nearestNeighbor(const Vec3& target) const {
        size_t bestIndex = 0;
        float bestDist = std::numeric_limits<float>::max();
        
        // Chercher dans un rayon de 2 cellules
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    Vec3 offsetPos = target;
                    offsetPos.x += dx * cellSize;
                    offsetPos.y += dy * cellSize;
                    offsetPos.z += dz * cellSize;
                    
                    auto it = grid.find(hash(offsetPos));
                    if (it != grid.end()) {
                        for (size_t idx : it->second) {
                            float dist = distance(target, points[idx]);
                            if (dist < bestDist) {
                                bestDist = dist;
                                bestIndex = idx;
                            }
                        }
                    }
                }
            }
        }
        return bestIndex;
    }
    
private:
    float distance(const Vec3& a, const Vec3& b) const {
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return dx*dx + dy*dy + dz*dz;
    }
};

#endif // FASTSPATIALLOOKUP_H