/*
 * Copyright (C) 2021, unclearness
 * All rights reserved.
 *
 */

#include "ugu/decimation/decimation.h"

#include <queue>
#include <unordered_set>

#include "ugu/face_adjacency.h"

namespace {

using VertexAttr = Eigen::VectorXd;
using VertexAttrs = std::vector<VertexAttr>;
using VertexAttrsPtr = std::shared_ptr<VertexAttrs>;

using QSlimEdge = std::pair<int32_t, int32_t>;

using Quadric = Eigen::MatrixXd;
using Quadrics = std::vector<Quadric>;
using QuadricPtr = std::shared_ptr<Quadric>;
using QuadricsPtr = std::shared_ptr<Quadrics>;

bool ComputeOptimalContraction(const VertexAttr& v1, const Quadric& q1,
                               const VertexAttr& v2, const Quadric& q2,
                               VertexAttr& v, double& error) {
  VertexAttr zero(v1.size());
  zero.setZero();
  zero[zero.size() - 1] = 1.0;

  bool ret = true;

  Quadric q = q1 + q2;

  if (std::abs(q.determinant()) < 0.00001) {
    // Not ivertible case
    ret = false;

    // Select best one from v1, v2 and (v1+v2)/2
    std::array<VertexAttr, 3> candidates = {v1, v2, (v1 + v2) * 0.5};
    double min_error = std::numeric_limits<double>::max();
    VertexAttr min_vert = v1;

    for (int i = 0; i < 3; i++) {
      double tmp_error = candidates[i].transpose() * q * candidates[i];
      if (tmp_error < min_error) {
        min_error = tmp_error;
        candidates[i];
        min_vert = candidates[i];
      }
    }

    error = min_error;
    v = min_vert;

  } else {
    // Invertible case
    // Eq. (1) in the paper
    Quadric q_inv = q.inverse();
    v = q_inv * zero;
    error = v.transpose() * q * v;
  }

  return ret;
}

struct QSlimEdgeInfo {
  QSlimEdge edge = {-1, -1};
  // QSlimEdge edge_uv = {-1, -1};
  int org_vid = -1;
  double error = std::numeric_limits<double>::max();
  QuadricsPtr quadrics;
  VertexAttrsPtr vert_attrs;
  VertexAttr decimated_v;
  bool keep_this_edge = false;
  QSlimEdgeInfo(QSlimEdge edge_, int org_vid_, double error_,
                VertexAttrsPtr vert_attrs_, QuadricsPtr quadrics_,
                bool keep_this_edge_) {
    edge = edge_;
    org_vid = org_vid_;
    error = error_;
    quadrics = quadrics_;
    vert_attrs = vert_attrs_;
    decimated_v.setConstant(std::numeric_limits<double>::max());
    keep_this_edge = keep_this_edge_;

    ComputeError();
  }

  void ComputeError() {
    if (keep_this_edge) {
      error = std::numeric_limits<double>::max();
    } else {
      int v0 = edge.first;
      int v1 = edge.second;
      ComputeOptimalContraction(vert_attrs->at(v0), quadrics->at(v0),
                                vert_attrs->at(v1), quadrics->at(v1),
                                decimated_v, error);
    }
  }

  QSlimEdgeInfo() {}
  ~QSlimEdgeInfo() {}
};

bool operator<(const QSlimEdgeInfo& l, const QSlimEdgeInfo& r) {
  return l.error > r.error;
};

using QSlimHeap =
    std::priority_queue<QSlimEdgeInfo, std::vector<QSlimEdgeInfo>>;

struct DecimatedMesh {
  ugu::MeshPtr mesh;
  std::vector<bool> valid_vertices;

  DecimatedMesh(ugu::MeshPtr mesh) : mesh(mesh) {
    valid_vertices.resize(mesh->vertices().size(), true);
  }

  DecimatedMesh() {}
  ~DecimatedMesh() {}
};

void UpdateDecimatedVertex() {
  // Remove original two vertex attributes
  // Mark them as invalid

  // Add the decimated vertex attribtues
  // Set decimated vertex attributes to the one of the invalidated/

  // Reconstruct new faces
  // Remove original face indices
  // Add new face indices
}

std::set<QSlimEdge> PrepareValidPairs(ugu::MeshPtr mesh,
                                      bool keep_geom_boundary,
                                      bool keep_uv_boundary) {
  std::set<QSlimEdge> valid_pairs;
  std::unordered_set<int32_t> invalid_vids;

  ugu::FaceAdjacency face_adjacency, uv_face_adjacency;
  if (keep_geom_boundary) {
    face_adjacency.Init(mesh->vertices().size(), mesh->vertex_indices());
    auto [boundary_edges, boundary_vertex_ids] =
        face_adjacency.GetBoundaryEdges();

    for (const auto& vid : boundary_vertex_ids) {
      invalid_vids.insert(vid);
    }
  }

  if (keep_uv_boundary) {
    // Treat uv vids by converting them to corresponding geom vids
    // TODO: direct solution?
    uv_face_adjacency.Init(mesh->uv().size(), mesh->uv_indices());
    auto [uv_boundary_edges, uv_boundary_vertex_ids] =
        uv_face_adjacency.GetBoundaryEdges();

    std::unordered_map<int, std::vector<int>> uv_v2f =
        ugu::GenerateVertex2FaceMap(mesh->uv_indices(), mesh->uv().size());

    for (const auto& uv_vid : uv_boundary_vertex_ids) {
      // Find the vertex id for geometry face (not uv)
      const auto uv_f_id = uv_v2f[uv_vid][0];
      const auto uv_f = mesh->vertex_indices()[uv_f_id];
      // Get index in uv face
      int index = 0;
      for (int i = 0; i < 3; i++) {
        if (uv_f[i] == uv_vid) {
          index = i;
          break;
        }
      }

      // Convert to geom face
      const auto geom_f = mesh->vertex_indices()[uv_f_id];
      // Convert to geom vid
      int vid = geom_f[index];

      invalid_vids.insert(vid);
    }
  }

  for (const auto& f : mesh->vertex_indices()) {
    size_t v0c = invalid_vids.count(f[0]);
    size_t v1c = invalid_vids.count(f[1]);
    size_t v2c = invalid_vids.count(f[2]);

    // Pair keeps always (smaller, bigger)
    // Is this okay for geometry face reconstruction?

    if (v0c == 0 && v1c == 0) {
      int32_t v0 = std::min(f[0], f[1]);
      int32_t v1 = std::max(f[0], f[1]);
      valid_pairs.insert(std::make_pair(v0, v1));
    }

    if (v1c == 0 && v2c == 0) {
      int32_t v1 = std::min(f[1], f[2]);
      int32_t v2 = std::max(f[1], f[2]);
      valid_pairs.insert(std::make_pair(v1, v2));
    }

    if (v2c == 0 && v0c == 0) {
      int32_t v2 = std::min(f[0], f[2]);
      int32_t v0 = std::max(f[0], f[2]);
      valid_pairs.insert(std::make_pair(v2, v0));
    }
  }

  return valid_pairs;
}

}  // namespace

namespace ugu {

bool QSlim(MeshPtr mesh, QSlimType type, int32_t target_face_num,
           int32_t target_vertex_num, bool keep_geom_boundary,
           bool keep_uv_boundary, bool accept_non_edge, float non_edge_dist) {
  // Set up valid pairs (edge and non-dege)
  std::set<QSlimEdge> valid_pairs =
      PrepareValidPairs(mesh, keep_geom_boundary, keep_uv_boundary);

  DecimatedMesh decimated_mesh(mesh);

  // Initialize quadrics
  QuadricsPtr quadrics;

  // Initialize heap
  QSlimHeap heap;

  // Add the valid pairs  to heap

  // Main loop
  while (!heap.empty()) {
    // Find the lowest error pair
    auto min_v = heap.top();
    heap.pop();

    // Decimate the pair

    // Update pair info
  }

  return true;
}

}  // namespace ugu
