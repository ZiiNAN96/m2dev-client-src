#pragma once
#include "Native.h"

// Existing legacy vertex declaration; layout and registration remain unchanged.
extern granny_data_type_definition GrannyPNT3322VertexType[5];

struct granny_pnt3322_vertex
{
    granny_real32 Position[3];
    granny_real32 Normal[3];
    granny_real32 UV0[2];
    granny_real32 UV1[2];
};
