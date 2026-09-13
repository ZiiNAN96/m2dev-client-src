#pragma once

namespace Renderer
{
// ZiiNAN: CPU matrix slots, independent of D3D9 transform-state numbering.
enum MatrixSlot : unsigned
{
    MatrixWorld, MatrixView, MatrixProjection,
    MatrixTexture0, MatrixTexture1, MatrixTexture2, MatrixTexture3,
    MatrixTexture4, MatrixTexture5, MatrixTexture6, MatrixTexture7,
    MatrixSlotCount
};
}
