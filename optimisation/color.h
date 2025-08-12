#ifndef COLOR_H
#define COLOR_H

#include <vector>

struct Color {
    float r, g, b;
};

struct ColorPoint {
    float key; // niveau de bruit (0.0 à 1.0)
    Color color;
};

Color getColorFromNoise(float noiseValue, const std::vector<ColorPoint>& palette);

Color lerp(const Color& c1, const Color& c2, float t);

#define RGB(r, g, b) \
    { (r) / 255.0f, (g) / 255.0f, (b) / 255.0f }

#define HEX(hex) \
    { ((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f }

#define COLOR_CYAN \
    { 0.0f, 1.0f, 1.0f }

#define COLOR_RED \
    { 1.0f, 0.0f, 0.0f }

#endif // COLOR_H