#pragma once
#include <algorithm>
#include <cmath>

namespace WorldResidency {
// Two sectors cover the existing maximum 384 m view from any point in a sector.
inline constexpr int VisibleRadius=2, PreloadRadius=3, UnloadRadius=5;
inline bool WholeMap(int x,int y) {return x>0&&y>0&&x<=8&&y<=8;}
inline bool Outside(int x,int y,int centerX,int centerY,int radius) {
    return std::abs(x-centerX)>radius||std::abs(y-centerY)>radius;
}
inline unsigned TerrainLod(float distance,float nearBoundary,float farBoundary,int previous) {
    const float boundaries[]{nearBoundary,farBoundary};
    int level=previous;
    if(level<0||level>2) {level=0;while(level<2&&distance>=boundaries[level])++level;}
    else {
        while(level<2&&distance>=boundaries[level]*1.08f)++level;
        while(level>0&&distance<boundaries[level-1]*.92f)--level;
    }
    return unsigned(level);
}
}
