/*
 * Copyright (C) 2021, unclearness
 * All rights reserved.
 */

#pragma once

#include "ugu/mesh.h"
#include "ugu/line.h"

namespace ugu {

bool MergeMeshes(const Mesh& src1, const Mesh& src2, Mesh* merged,
                 bool use_src1_material = false);
bool MergeMeshes(const std::vector<MeshPtr>& src_meshes, Mesh* merged);

std::tuple<std::vector<std::vector<std::pair<int, int>>>,
           std::vector<std::vector<int>>>
FindBoundaryLoops(const Mesh& mesh);

// make cube with 24 vertices
MeshPtr MakeCube(const Eigen::Vector3f& length, const Eigen::Matrix3f& R,
                 const Eigen::Vector3f& t);
MeshPtr MakeCube(const Eigen::Vector3f& length);
MeshPtr MakeCube(float length, const Eigen::Matrix3f& R,
                 const Eigen::Vector3f& t);
MeshPtr MakeCube(float length);

void SetRandomVertexColor(MeshPtr mesh, int seed = 0);

int32_t CutByPlane(MeshPtr mesh, const Planef& plane, bool fill_plane = true);

}  // namespace ugu