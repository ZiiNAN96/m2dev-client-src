#pragma once
#include "GlTFFixtures.h"
inline GlTFFixtures::Builder SkinAnimationFixture()
{
    GlTFFixtures::Builder fixture;
    fixture.nodes = R"({"mesh":0,"skin":0,"children":[1]},{"name":"root","children":[2]},{"name":"tip","translation":[0,1,0]})";
    const auto jointsView = fixture.View(std::array<std::uint8_t,12>{0,1,0,0, 0,1,0,0, 0,1,0,0});
    const auto joints = fixture.Accessor(jointsView,5121,3,"VEC4");
    const auto weightsView = fixture.View(std::array<std::uint8_t,12>{128,127,0,0, 128,127,0,0, 128,127,0,0});
    const auto weights = fixture.Accessor(weightsView,5121,3,"VEC4",0,true);
    fixture.attributes += R"(,"JOINTS_0":)"+std::to_string(joints)+R"(,"WEIGHTS_0":)"+std::to_string(weights);
    const std::array<float,32> inverse{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,
        1,0,0,0,0,1,0,0,0,0,1,0,0,-1,0,1};
    const auto inverseView = fixture.View(inverse);
    const auto inverseAccessor = fixture.Accessor(inverseView,5126,2,"MAT4");
    const auto timeView = fixture.View(std::array<float,2>{0,2});
    const auto time = fixture.Accessor(timeView,5126,2,"SCALAR");
    const auto outputView = fixture.View(std::array<float,6>{0,1,0, 0,2,0});
    const auto output = fixture.Accessor(outputView,5126,2,"VEC3");
    fixture.extra = R"(,"skins":[{"name":"two-joint","joints":[1,2],"skeleton":1,"inverseBindMatrices":)"+
        std::to_string(inverseAccessor)+R"(}],"animations":[{"name":"move","samplers":[{"input":)"+std::to_string(time)+
        R"(,"output":)"+std::to_string(output)+R"(,"interpolation":"STEP"}],"channels":[{"sampler":0,"target":{"node":2,"path":"translation"}}]}])";
    return fixture;
}

