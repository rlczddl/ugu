/*
 * Copyright (C) 2023, unclearness
 * All rights reserved.
 */

#include "ugu/renderer/gl/renderer.h"

#ifdef UGU_USE_GLFW
#include "glad/gl.h"
#endif

namespace ugu {

RendererGl::RendererGl() {}

RendererGl::~RendererGl() {}

bool RendererGl::Init() {
  if (m_cam == nullptr) {
    LOGE("camera has not been set\n");
    return false;
  }

  m_gbuf_shader.SetVertType(VertShaderType::GBUF);
  m_gbuf_shader.SetFragType(FragShaderType::GBUF);

  m_deferred_shader.SetVertType(VertShaderType::DEFERRED);
  m_deferred_shader.SetFragType(FragShaderType::DEFERRED);

  if (!m_gbuf_shader.Prepare() || !m_deferred_shader.Prepare()) {
    return false;
  }

  for (size_t i = 0; i < m_geoms.size(); i++) {
    auto mesh = m_geoms[i];
    auto trans = m_node_trans.at(mesh);
    int model_loc = glGetUniformLocation(m_gbuf_shader.ID, "model");
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, trans.data());
    m_node_locs[mesh] = model_loc;

    mesh->BindTextures();
    mesh->SetupMesh(static_cast<int>(i + 1));
  }

  // configure g-buffer framebuffer
  // ------------------------------
  // unsigned int gBuffer;
  glGenFramebuffers(1, &gBuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
  // unsigned int gPosition, gNormal, gAlbedoSpec;
  //  position color buffer
  glGenTextures(1, &gPosition);
  glBindTexture(GL_TEXTURE_2D, gPosition);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA,
               GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         gPosition, 0);
  // normal color buffer
  glGenTextures(1, &gNormal);
  glBindTexture(GL_TEXTURE_2D, gNormal);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA,
               GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                         gNormal, 0);
  // color + specular color buffer
  glGenTextures(1, &gAlbedoSpec);
  glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
                         gAlbedoSpec, 0);

// Face id & geometry id
#if 1
  glGenTextures(1, &gId);
  glBindTexture(GL_TEXTURE_2D, gId);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA,
               GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D,
                         gId, 0);
#endif

  // bary centric & uv
  glGenTextures(1, &gFace);
  glBindTexture(GL_TEXTURE_2D, gFace);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA,
               GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D,
                         gFace, 0);

  // tell OpenGL which color attachments we'll use (of this framebuffer) for
  // rendering
  attachments[0] = GL_COLOR_ATTACHMENT0;
  attachments[1] = GL_COLOR_ATTACHMENT1;
  attachments[2] = GL_COLOR_ATTACHMENT2;
  attachments[3] = GL_COLOR_ATTACHMENT3;
  attachments[4] = GL_COLOR_ATTACHMENT4;

  glDrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data());
  // create and attach depth buffer (renderbuffer)

  glGenRenderbuffers(1, &rboDepth);
  glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_width, m_height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, rboDepth);
  // finally check if framebuffer is complete
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cout << "Framebuffer not complete!" << std::endl;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  Eigen::Matrix4f view_mat = m_cam->c2w().inverse().matrix().cast<float>();
  m_view_loc = glGetUniformLocation(m_gbuf_shader.ID, "view");
  glUniformMatrix4fv(m_view_loc, 1, GL_FALSE, view_mat.data());

  Eigen::Matrix4f prj_mat = m_cam->ProjectionMatrixOpenGl(m_near_z, m_far_z);
  m_prj_loc = glGetUniformLocation(m_gbuf_shader.ID, "projection");
  glUniformMatrix4fv(m_prj_loc, 1, GL_FALSE, prj_mat.data());

  m_deferred_shader.Use();
  m_deferred_shader.SetInt("gPosition", 0);
  m_deferred_shader.SetInt("gNormal", 1);
  m_deferred_shader.SetInt("gAlbedoSpec", 2);
  m_deferred_shader.SetInt("gId", 3);
  m_deferred_shader.SetInt("gFace", 4);

  m_gbuf.Init(m_width, m_height);

  return true;
}

bool RendererGl::Draw(double tic) {
  (void)tic;

  // GBuf

  glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  m_gbuf_shader.Use();

  Eigen::Matrix4f view_mat = m_cam->c2w().inverse().matrix().cast<float>();
  glUniformMatrix4fv(m_view_loc, 1, GL_FALSE, view_mat.data());
  Eigen::Matrix4f prj_mat = m_cam->ProjectionMatrixOpenGl(m_near_z, m_far_z);
  glUniformMatrix4fv(m_prj_loc, 1, GL_FALSE, prj_mat.data());

  for (auto mesh : m_geoms) {
    int model_loc = m_node_locs[mesh];
    auto& trans = m_node_trans[mesh];
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, trans.data());
    mesh->Draw(m_gbuf_shader);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Deferred

#if 1
  // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  m_deferred_shader.Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gPosition);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, gNormal);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, gId);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, gFace);

#if 0
  // send light relevant uniforms
  for (unsigned int i = 0; i < lightPositions.size(); i++) {
    shaderLightingPass.setVec3("lights[" + std::to_string(i) + "].Position",
                               lightPositions[i]);
    shaderLightingPass.setVec3("lights[" + std::to_string(i) + "].Color",
                               lightColors[i]);
    // update attenuation parameters and calculate radius
    const float linear = 0.7f;
    const float quadratic = 1.8f;
    shaderLightingPass.setFloat("lights[" + std::to_string(i) + "].Linear",
                                linear);
    shaderLightingPass.setFloat("lights[" + std::to_string(i) + "].Quadratic",
                                quadratic);
  }
  shaderLightingPass.setVec3("viewPos", camera.Position);
#endif

#if 0
  for (auto [mesh, trans] : m_nodes) {
    int model_loc = m_node_locs[mesh];
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, trans.data());
    mesh->Draw(m_gbuf_shader);
  }
#endif

  if (quadVAO == 0) {
    float quadVertices[] = {
        // positions        // texture Coords
        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
    };
    // setup plane VAO
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));
  }
  glBindVertexArray(quadVAO);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
#endif

  return true;
}

bool RendererGl::ReadGbuf() {
  const bool flip_y = true;

  // To read out of [0, 1] range
  // glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
  //

  Image4f tmp4f(m_height, m_width);
  Image1f tmp1f(m_height, m_width);
  Image4i tmp4i(m_height, m_width);

  m_gbuf.Reset();

  glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

  // Depth 01
  {
    glReadBuffer(GL_NONE);
    // glRead"n"Pixels() is valid only if OpenGL4.5 (2017) or later
    // glReadPixels() has been supported from long years ago
    glReadPixels(0, 0, m_width, m_height, GL_DEPTH_COMPONENT, GL_FLOAT,
                 tmp1f.data);
    tmp1f.forEach([&](float& d, const int yx[2]) {
      int y = flip_y ? (m_height - 1 - yx[0]) : yx[0];
      int x = yx[1];

      if (d <= 0.f || 1.f <= d) {
        m_gbuf.stencil.at<uint8_t>(y, x) = 0;
      } else {
        m_gbuf.stencil.at<uint8_t>(y, x) = 255;
      }

      m_gbuf.depth_01.at<float>(y, x) = std::clamp(d, 0.f, 1.f);
    });
  }

  Eigen::Matrix3f w2c_R = m_cam->w2c().rotation().matrix().cast<float>();
  Eigen::Vector3f w2c_t = m_cam->w2c().translation().cast<float>();

  // Pos
  {
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_FLOAT, tmp4f.data);
    tmp4f.forEach([&](Vec4f& n, const int yx[2]) {
      int y = flip_y ? (m_height - 1 - yx[0]) : yx[0];
      int x = yx[1];
      if (m_gbuf.stencil.at<uint8_t>(y, x) != 255) {
        return;
      }

      auto& wld = m_gbuf.pos_wld.at<Vec3f>(y, x);
      wld[0] = n[0];
      wld[1] = n[1];
      wld[2] = n[2];
      auto& cam = m_gbuf.pos_cam.at<Vec3f>(y, x);
      cam = w2c_t + (w2c_R * wld);
    });
  }

  // Normal
  {
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_FLOAT, tmp4f.data);
    tmp4f.forEach([&](Vec4f& n, const int yx[2]) {
      int y = flip_y ? (m_height - 1 - yx[0]) : yx[0];
      int x = yx[1];
      if (m_gbuf.stencil.at<uint8_t>(y, x) != 255) {
        return;
      }
      auto& wld = m_gbuf.normal_wld.at<Vec3f>(y, x);
      wld[0] = n[0];
      wld[1] = n[1];
      wld[2] = n[2];
      auto& cam = m_gbuf.normal_cam.at<Vec3f>(y, x);
      cam = w2c_R * wld;
    });
  }

  // Color
  {
    glReadBuffer(GL_COLOR_ATTACHMENT2);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_FLOAT, tmp4f.data);
    tmp4f.forEach([&](Vec4f& n, const int yx[2]) {
      int y = flip_y ? (m_height - 1 - yx[0]) : yx[0];
      int x = yx[1];
      if (m_gbuf.stencil.at<uint8_t>(y, x) != 255) {
        return;
      }
      auto& col = m_gbuf.color.at<Vec3b>(y, x);
      col[0] = saturate_cast<uint8_t>(n[0] * 255);
      col[1] = saturate_cast<uint8_t>(n[1] * 255);
      col[2] = saturate_cast<uint8_t>(n[2] * 255);
    });
  }

  tmp4f.setTo(0.f);
  // Face id & geo id
  {
    glReadBuffer(GL_COLOR_ATTACHMENT3);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_FLOAT, tmp4f.data);
    tmp4f.forEach([&](Vec4f& val, const int yx[2]) {
      int y = flip_y ? (m_height - 1 - yx[0]) : yx[0];
      int x = yx[1];
      if (m_gbuf.stencil.at<uint8_t>(y, x) != 255) {
        return;
      }
      int geo_id = static_cast<int>(
          std::round(val[1]));  // static_cast<int>(val[1] * m_geoms.size());
      if (geo_id > 0) {
        //  std::cout << geo_id << std::endl;
      }
      m_gbuf.geo_id.at<int>(y, x) = geo_id;
      m_gbuf.face_id.at<int>(y, x) = static_cast<int>(std::round(val[0]));
    });
  }

  tmp4f.setTo(0.f);
  // Face id & barycenteric
  {
    glReadBuffer(GL_COLOR_ATTACHMENT4);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_FLOAT, tmp4f.data);
    tmp4f.forEach([&](Vec4f& val, const int yx[2]) {
      int y = flip_y ? (m_height - 1 - yx[0]) : yx[0];
      int x = yx[1];
      if (m_gbuf.stencil.at<uint8_t>(y, x) != 255) {
        return;
      }
      auto& bary = m_gbuf.bary.at<Vec3f>(y, x);
      bary[0] = val[0];
      bary[1] = val[1];
      bary[2] = 1.f - bary[0] - bary[1];
      auto& uv = m_gbuf.uv.at<Vec3f>(y, x);
      uv[0] = val[2];
      uv[1] = val[3];
      uv[2] = 1.f - uv[0] - uv[1];
    });
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return true;
}

void RendererGl::SetCamera(const CameraPtr cam) { m_cam = cam; }
void RendererGl::SetMesh(RenderableMeshPtr mesh, const Eigen::Affine3f& trans) {
  if (m_node_trans.find(mesh) == m_node_trans.end()) {
    m_geoms.push_back(mesh);
  }
  m_node_trans[mesh] = trans;
}
void RendererGl::ClearMesh() {
  m_geoms.clear();
  m_node_trans.clear();
  m_node_locs.clear();
}

void RendererGl::SetFragType(const FragShaderType& frag_type) {
  m_deferred_shader.frag_type = frag_type;
}

void RendererGl::SetNearFar(float near_z, float far_z) {
  m_near_z = near_z;
  m_far_z = far_z;
}

void RendererGl::SetSize(uint32_t width, uint32_t height) {
  m_width = width;
  m_height = height;
}

void RendererGl::GetGbuf(GBuffer& gbuf) const { gbuf = m_gbuf; }

}  // namespace ugu