#include <vector>
#include <algorithm>
#include "color.h"

// Interpolation linéaire entre deux couleurs
Color lerp(const Color& c1, const Color& c2, float t) {
    return {
        c1.r + (c2.r - c1.r) * t,
        c1.g + (c2.g - c1.g) * t,
        c1.b + (c2.b - c1.b) * t
    };
}

/**
 * 
 * Fonction qui, donné un niveau de bruit, renvoie la couleur interpolée
 * @param noiseValue Valeur de bruit normalisée entre 0 et 1.
 * @param palette Palette de couleurs définie par des points clés.
 * 
 * @return Couleur interpolée correspondant à la valeur de bruit.
 * 
 */
Color getColorFromNoise(float noiseValue, const std::vector<ColorPoint>& palette) {
    if (palette.empty()) return {0,0,0};
    // Clamp bruit aux bornes
    if (noiseValue <= palette.front().key) return palette.front().color;
    if (noiseValue >= palette.back().key) return palette.back().color;

    // Recherche entre deux paliers
    for (size_t i = 0; i + 1 < palette.size(); ++i) {
        if (noiseValue >= palette[i].key && noiseValue <= palette[i+1].key) {
            float t = (noiseValue - palette[i].key) / (palette[i+1].key - palette[i].key);
            return lerp(palette[i].color, palette[i+1].color, t);
        }
    }

    // Par défaut (devrait pas arriver)
    return palette.back().color;
}