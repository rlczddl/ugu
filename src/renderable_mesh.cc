/*
 * Copyright (C) 2023, unclearness
 * All rights reserved.
 */

#include "ugu/renderable_mesh.h"

#ifdef UGU_USE_GLFW
#include "glad/gl.h"

namespace ugu {
void RenderableMesh::BindTextures() {
  texture_ids.clear();
  for (size_t i = 0; i < materials().size(); i++) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    texture_ids.push_back(textureID);

#if 0
       GLenum format;
      if (nrComponents == 1)
        format = GL_RED;
      else if (nrComponents == 3)
        format = GL_RGB;
      else if (nrComponents == 4)
        format = GL_RGBA;
#endif
    GLenum format = GL_RGBA;

    auto diffuse3b = materials()[i].diffuse_tex;
    if (diffuse3b.empty()) {
      continue;
    }

    Image4b diffuse = Image4b::zeros(diffuse3b.rows, diffuse3b.cols);
    diffuse.forEach([&](Vec4b &c4, const int *xy) {
      const Vec3b &c3 = diffuse3b.at<Vec3b>(xy[1], xy[0]);
      c4[0] = c3[0];
      c4[1] = c3[1];
      c4[2] = c3[2];
    });

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, diffuse.cols, diffuse.rows, 0,
                 format, GL_UNSIGNED_BYTE, diffuse.data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
}

void RenderableMesh::SetupMesh() {
  if (!uv_.empty() && (vertices_.size() != uv_.size())) {
    SplitMultipleUvVertices();
  }

  renderable_vertices.clear();
  for (size_t i = 0; i < vertices().size(); i++) {
    Vertex v;
    v.pos = vertices()[i];

    if (normals().size() == vertices().size()) {
      v.nor = normals()[i];
    } else {
      v.nor.setOnes();
    }

    if (vertex_colors().size() == vertices().size()) {
      v.col = vertex_colors()[i];
    } else {
      v.col = {1.f, 0.5f, 0.5f};
    }

    if (uv().size() == vertices().size()) {
      v.uv = {uv()[i][0], 1.f - uv()[i][1]};
    } else {
      v.uv = {0.f, 0.f};
    }

    renderable_vertices.push_back(v);
  }

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);

  glBufferData(GL_ARRAY_BUFFER, renderable_vertices.size() * sizeof(Vertex),
               &renderable_vertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

  flatten_indices.clear();
  for (const auto &i : vertex_indices()) {
    flatten_indices.push_back(i[0]);
    flatten_indices.push_back(i[1]);
    flatten_indices.push_back(i[2]);
  }

  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               flatten_indices.size() * sizeof(uint32_t), &flatten_indices[0],
               GL_STATIC_DRAW);

  // vertex positions
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);

  // vertex normals
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, nor));

  // vertex colors
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, col));

  // vertex texture coords
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, uv));

  glBindVertexArray(0);
}

void RenderableMesh::Draw(const Shader &shader) const {
  unsigned int diffuseNr = 1;
  unsigned int specularNr = 1;
  for (unsigned int i = 0; i < materials().size(); i++) {
    glActiveTexture(GL_TEXTURE0 +
                    i);  // activate proper texture unit before binding
    // retrieve texture number (the N in diffuse_textureN)
    std::string number = std::to_string(diffuseNr++);
    std::string name = "diffuse";  // materials()[i].diffuse_texname;
    // if (name == "texture_diffuse")
    //   number = std::to_string(diffuseNr++);
    // else if (name == "texture_specular")
    //   number = std::to_string(specularNr++);

    shader.SetInt(("material." + name + number).c_str(), i);
    glBindTexture(GL_TEXTURE_2D, texture_ids[i]);
  }
  glActiveTexture(GL_TEXTURE0);

  // draw mesh
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, vertex_indices().size() * 3, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}
}  // namespace ugu
#else
void RenderableMesh::BindTextures() {
  LOGE("Not supported with this configuration\n");
}

void RenderableMesh::SetupMesh() {
  LOGE("Not supported with this configuration\n");
}

void RenderableMesh::Draw(const Shader &shader) const {
  LOGE("Not supported with this configuration\n");
}

#endif