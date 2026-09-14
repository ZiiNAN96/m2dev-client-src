#pragma once

// ZiiNAN: Asset Runtime boundary
// Opaque declarations for the retained legacy adapter; never part of the public runtime API.
struct granny_animation;
struct granny_data_type_definition;
struct granny_file;
struct granny_file_info;
struct granny_material;
struct granny_mesh;
struct granny_model;
struct granny_skeleton;
struct granny_track_group;

// The SDK's opaque C++ names alias namespace granny, unlike its transparent structs.
namespace granny
{
struct control;
struct local_pose;
struct mesh_binding;
struct mesh_deformer;
struct model_instance;
struct world_pose;
}
using granny_control = granny::control;
using granny_local_pose = granny::local_pose;
using granny_mesh_binding = granny::mesh_binding;
using granny_mesh_deformer = granny::mesh_deformer;
using granny_model_instance = granny::model_instance;
using granny_world_pose = granny::world_pose;
