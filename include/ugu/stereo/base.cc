/*
 * Copyright (C) 2019, unclearness
 * All rights reserved.
 */

#include "ugu/stereo/base.h"

namespace ugu {

bool Disparity2Depth(const Image1f& disparity, Image1f* depth, float baseline,
                     float fx, float lcx, float rcx, float mind, float maxd) {
  const int pix_num = disparity.rows * disparity.cols;
  const float* src = reinterpret_cast<float*>(disparity.data);
  float* dst = reinterpret_cast<float*>(depth->data);

  for (int i = 0; i < pix_num; i++) {
    dst[i] = baseline * fx / (src[i] - lcx + rcx);
    if (dst[i] < mind || maxd < dst[i]) {
      dst[i] = 0.0f;
    }
  }

  return true;
}

bool ComputeStereoBruteForce(const Image1b& left, const Image1b& right,
                             Image1f* disparity, Image1f* cost, Image1f* depth,
                             const StereoParam& param, int kernel,
                             float max_disparity) {
  if (left.rows != right.rows || left.cols != right.cols) {
    LOGE("Left and right size missmatch\n");
    return false;
  }

  const int hk = kernel / 2;
  if (max_disparity < 0) {
    max_disparity = static_cast<float>(left.cols - 1);
  }
  const int max_disparity_i = static_cast<int>(max_disparity);
  const int w = left.cols;
  const int h = left.rows;
  if (disparity->rows != h || disparity->cols != w) {
    *disparity = Image1f::zeros(h, w);
  }
  if (cost->rows != h || cost->cols != w) {
    *cost = Image1f::zeros(h, w);
    cost->setTo(std::numeric_limits<float>::max());
  }
  if (depth->rows != h || depth->cols != w) {
    *depth = Image1f::zeros(h, w);
  }

  for (int j = hk; j < h - hk; j++) {
    for (int i = hk; i < w - hk; i++) {
      float& disp = disparity->at<float>(j, i);
      float& c = cost->at<float>(j, i);

      // Find the best match in the same row
      // Integer pixel (not sub-pixel) accuracy
      for (int k = 0; k < max_disparity_i - i; k++) {
        float current_cost = 0.0f;
        for (int jj = -hk; jj <= hk; jj++) {
          for (int ii = -hk; ii <= hk; ii++) {
            // SAD
            float sad = std::abs(static_cast<float>(
                (left.at<unsigned char>(j + jj, i + ii) -
                 right.at<unsigned char>(j + jj, i + ii + k))));

            current_cost += sad;
          }
        }
        if (current_cost < c) {
          c = current_cost;
          disp = static_cast<float>(k);
          if (k == 0) {
            disp = 0.001f;
          }
        }
      }
    }
  }

  Disparity2Depth(*disparity, depth, param.baseline, param.fx, param.lcx,
                  param.rcx, param.mind, param.maxd);

  return true;
}

struct PlaneParam {};

bool ComputePatchMatchStereo(const Image3b& left, const Image3b& right,
                             Image1f* disparity, Image1f* depth, bool temporal,
                             bool refinement) {
  return true;
}

}  // namespace ugu
