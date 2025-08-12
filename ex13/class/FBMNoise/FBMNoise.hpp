#include <vector>
#include <cmath>

class FBMNoise {
public:
    FBMNoise(int octaves, float persistence, float scale)
        : octaves_(octaves), persistence_(persistence), scale_(scale)
    {
        frequencies_.resize(octaves_);
        amplitudes_.resize(octaves_);
        
        float frequency = scale_;
        float amplitude = 1.0f;
        for (int i = 0; i < octaves_; ++i) {
            frequencies_[i] = frequency;
            amplitudes_[i] = amplitude;
            frequency *= 2.0f;
            amplitude *= persistence_;
        }
    }

    float compute(float x, float y, float z) const {
        float total = 0.0f;
        float maxValue = 0.0f;

        for (int i = 0; i < octaves_; ++i) {
            total += PerlinNoise3D(x * frequencies_[i], y * frequencies_[i], z * frequencies_[i]) * amplitudes_[i];
            maxValue += amplitudes_[i];
        }

        return total / maxValue;
    }

private:
    int octaves_;
    float persistence_;
    float scale_;
    std::vector<float> frequencies_;
    std::vector<float> amplitudes_;

    // Tu dois avoir PerlinNoise3D définie quelque part
    float PerlinNoise3D(float x, float y, float z) const;
};
