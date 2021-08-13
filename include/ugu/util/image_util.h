/*
 * Copyright (C) 2021, unclearness
 * All rights reserved.
 */

#pragma once

#include "ugu/image.h"

namespace ugu {

template <typename T>
T BilinearInterpolation(float x, float y, const ugu::Image<T>& image) {
  std::array<int, 2> pos_min = {{0, 0}};
  std::array<int, 2> pos_max = {{0, 0}};
  pos_min[0] = static_cast<int>(std::floor(x));
  pos_min[1] = static_cast<int>(std::floor(y));
  pos_max[0] = pos_min[0] + 1;
  pos_max[1] = pos_min[1] + 1;

  // really need these?
  if (pos_min[0] < 0.0f) {
    pos_min[0] = 0;
  }
  if (pos_min[1] < 0.0f) {
    pos_min[1] = 0;
  }
  if (image.cols <= pos_max[0]) {
    pos_max[0] = image.cols - 1;
  }
  if (image.rows <= pos_max[1]) {
    pos_max[1] = image.rows - 1;
  }

  float local_u = x - pos_min[0];
  float local_v = y - pos_min[1];

  // bilinear interpolation
  T color;
  for (int i = 0; i < image.channels(); i++) {
    float colorf =
        (1.0f - local_u) * (1.0f - local_v) *
            image.template at<T>(pos_min[1], pos_min[0])[i] +
        local_u * (1.0f - local_v) *
            image.template at<T>(pos_min[1], pos_max[0])[i] +
        (1.0f - local_u) * local_v *
            image.template at<T>(pos_max[1], pos_min[0])[i] +
        local_u * local_v * image.template at<T>(pos_max[1], pos_max[0])[i];
    color[i] = static_cast<typename T::value_type>(colorf);
  }

  return color;
}

template <typename T>
double BilinearInterpolation(float x, float y, int channel,
                             const ugu::Image<T>& image) {
  std::array<int, 2> pos_min = {{0, 0}};
  std::array<int, 2> pos_max = {{0, 0}};
  pos_min[0] = static_cast<int>(std::floor(x));
  pos_min[1] = static_cast<int>(std::floor(y));
  pos_max[0] = pos_min[0] + 1;
  pos_max[1] = pos_min[1] + 1;

  // really need these?
  if (pos_min[0] < 0.0f) {
    pos_min[0] = 0;
  }
  if (pos_min[1] < 0.0f) {
    pos_min[1] = 0;
  }
  if (image.cols <= pos_max[0]) {
    pos_max[0] = image.cols - 1;
  }
  if (image.rows <= pos_max[1]) {
    pos_max[1] = image.rows - 1;
  }

  double local_u = x - pos_min[0];
  double local_v = y - pos_min[1];

  // bilinear interpolation
  double color =
      (1.0 - local_u) * (1.0 - local_v) *
          image.template at<T>(pos_min[1], pos_min[0])[channel] +
      local_u * (1.0f - local_v) *
          image.template at<T>(pos_min[1], pos_max[0])[channel] +
      (1.0 - local_u) * local_v *
          image.template at<T>(pos_max[1], pos_min[0])[channel] +
      local_u * local_v * image.template at<T>(pos_max[1], pos_max[0])[channel];

  return color;
}

template <typename T>
void UndistortImageOpencv(const ugu::Image<T>& src, ugu::Image<T>* dst,
                          float fx, float fy, float cx, float cy, float k1,
                          float k2, float p1, float p2, float k3 = 0.0f,
                          float k4 = 0.0f, float k5 = 0.0f, float k6 = 0.0f) {
  if (dst->rows != src.rows || dst->cols != src.cols) {
    *dst = ugu::Image<T>::zeros(src.rows, src.cols);
  }
  for (int y = 0; y < src.rows; y++) {
    for (int x = 0; x < src.cols; x++) {
      float xf = static_cast<float>(x);
      float yf = static_cast<float>(y);

      // TODO: denser and other interpolation methods.
      UndistortPixelOpencv(&xf, &yf, fx, fy, cx, cy, k1, k2, p1, p2, k3, k4, k5,
                           k6);
      int nn_x = static_cast<int>(std::round(xf));
      int nn_y = static_cast<int>(std::round(yf));

      if (nn_x < 0 || src.cols <= nn_x || nn_y < 0 || src.rows <= nn_y) {
        continue;
      }

      dst->template at<T>(nn_y, nn_x) = src.template at<T>(y, x);
    }
  }
}

void Depth2Gray(const Image1f& depth, Image1b* vis_depth, float min_d = 200.0f,
                float max_d = 1500.0f);

void Normal2Color(const Image3f& normal, Image3b* vis_normal);

void FaceId2RandomColor(const Image1i& face_id, Image3b* vis_face_id);

void Color2Gray(const Image3b& color, Image1b* gray);

void Conv(const Image1b& src, Image1f* dst, float* filter, int kernel_size);
void SobelX(const Image1b& gray, Image1f* gradx, bool scharr = false);
void SobelY(const Image1b& gray, Image1f* grady, bool scharr = false);
void Laplacian(const Image1b& gray, Image1f* laplacian);

struct InvalidSdf {
  static const float kVal;
};

void DistanceTransformL1(const Image1b& mask, Image1f* dist);
void DistanceTransformL1(const Image1b& mask, const Eigen::Vector2i& roi_min,
                         const Eigen::Vector2i& roi_max, Image1f* dist);
void MakeSignedDistanceField(const Image1b& mask, Image1f* dist,
                             bool minmax_normalize, bool use_truncation,
                             float truncation_band);
void MakeSignedDistanceField(const Image1b& mask,
                             const Eigen::Vector2i& roi_min,
                             const Eigen::Vector2i& roi_max, Image1f* dist,
                             bool minmax_normalize, bool use_truncation,
                             float truncation_band);
void SignedDistance2Color(const Image1f& sdf, Image3b* vis_sdf,
                          float min_negative_d, float max_positive_d);

#ifdef UGU_USE_TINYCOLORMAP
void Depth2Color(
    const Image1f& depth, Image3b* vis_depth, float min_d = 200.0f,
    float max_d = 1500.0f,
    tinycolormap::ColormapType type = tinycolormap::ColormapType::Viridis);
void FaceId2Color(
    const Image1i& face_id, Image3b* vis_face_id, int min_id = 0,
    int max_id = -1,
    tinycolormap::ColormapType type = tinycolormap::ColormapType::Viridis);
#endif

void BoxFilter(const Image1b& src, Image1b* dst, int kernel);
void BoxFilter(const Image1f& src, Image1f* dst, int kernel);
void BoxFilter(const Image3b& src, Image3b* dst, int kernel);
void BoxFilter(const Image3f& src, Image3f* dst, int kernel);

void Erode(const Image1b& src, Image1b* dst, int kernel);
void Dilate(const Image1b& src, Image1b* dst, int kernel);
void Diff(const Image1b& src1, const Image1b& src2, Image1b* dst);
void Not(const Image1b& src, Image1b* dst);

template <typename T>
float NormL2(const T& src) {
  return static_cast<float>(
      std::sqrt(src[0] * src[0] + src[1] * src[1] + src[2] * src[2]));
}

template <typename T>
float NormL2Squared(const T& src) {
  return static_cast<float>(src[0] * src[0] + src[1] * src[1] +
                            src[2] * src[2]);
}
}  // namespace ugu