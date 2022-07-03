/*
 * Copyright (C) 2019, unclearness
 * All rights reserved.
 */

#pragma once

#include <cassert>
#include <limits>
#include <string>

#include "Eigen/Geometry"
#include "ugu/eigen_util.h"
#include "ugu/log.h"

#define UGU_FLOATING_POINT_ONLY_TEMPLATE                              \
  template <typename T,                                               \
            typename std::enable_if<std::is_floating_point<T>::value, \
                                    std::nullptr_t>::type = nullptr>

namespace ugu {

// TODO: definition in header may be invalid.
static inline const double pi = 3.14159265358979323846;

// Interpolation method in texture uv space
enum class ColorInterpolation {
  kNn = 0,       // Nearest Neigbor
  kBilinear = 1  // Bilinear interpolation
};

enum class CoordinateType { OpenCV, OpenGL };

float radians(const float& degrees);
double radians(const double& degrees);

float degrees(const float& radians);
double degrees(const double& radians);

float Fov2FocalPix(float fov, float pix, bool is_deg = false);
float FocalPix2Fov(float f, float pix, bool to_deg = false);

template <typename T, typename TT>
T saturate_cast(const TT& v);

template <typename T, typename TT>
T saturate_cast(const TT& v) {
  return static_cast<T>(
      std::clamp(v, static_cast<TT>(std::numeric_limits<T>::lowest()),
                 static_cast<TT>(std::numeric_limits<T>::max())));
}

}  // namespace ugu
