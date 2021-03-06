/*
 * Copyright (c) 2016 Jonathan Glines
 * Jonathan Glines <jonathan@glines.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <glm/gtc/type_ptr.hpp>
#include <mcxx/contourBuilder.h>
#include <mcxx/vector.h>

extern "C" {
#include <mc/algorithms.h>
}

#include "../common/glError.h"
#include "../common/shaderProgram.h"
#include "../common/shaders.h"
#include "squareObject.h"

namespace mc { namespace samples { namespace squares {
  SquareObject::SquareObject(
      const glm::vec3 &position,
      const glm::quat &orientation)
    : SceneObject(position, orientation),
    m_square(0x1), m_resolution(8)
  {
    // Create buffers for the contour wireframe in the GL
    m_initWireframe();
    m_initSquareWireframe();
    m_update();
  }

  void SquareObject::setSquare(int square) {
    m_square = square;
    m_update();
  }

  void SquareObject::setResolution(int resolution) {
    m_resolution = resolution;
    m_update();
  }

  void SquareObject::m_initWireframe() {
    glGenBuffers(1, &m_wireframeVertices);
    FORCE_ASSERT_GL_ERROR();
    glGenBuffers(1, &m_wireframeIndices);
    FORCE_ASSERT_GL_ERROR();
  }

  void SquareObject::m_initSquareWireframe() {
    /* Send the vertices to the GL */
    static const WireframeVertex vertices[] = {
      {
        .pos = { 0.0f, 0.0f, 0.0f },
        .color = { 0.0f, 0.0f, 1.0f },
      },
      {
        .pos = { 1.0f, 0.0f, 0.0f },
        .color = { 0.0f, 0.0f, 1.0f },
      },
      {
        .pos = { 0.0f, 1.0f, 0.0f },
        .color = { 0.0f, 0.0f, 1.0f },
      },
      {
        .pos = { 1.0f, 1.0f, 0.0f },
        .color = { 0.0f, 0.0f, 1.0f },
      },
    };
    glGenBuffers(1, &m_squareWireframeVertices);
    FORCE_ASSERT_GL_ERROR();
    glBindBuffer(GL_ARRAY_BUFFER, m_squareWireframeVertices);
    FORCE_ASSERT_GL_ERROR();
    glBufferData(
        GL_ARRAY_BUFFER,  // target
        sizeof(vertices),  // size
        vertices,  // data
        GL_STATIC_DRAW  // usage
        );
    FORCE_ASSERT_GL_ERROR();
    static const unsigned int indices[] = {
      0, 1,
      1, 3,
      3, 2,
      2, 0,
    };
    glGenBuffers(1, &m_squareWireframeIndices);
    FORCE_ASSERT_GL_ERROR();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_squareWireframeIndices);
    FORCE_ASSERT_GL_ERROR();
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,  // target
        sizeof(indices),  // size
        indices,  // data
        GL_STATIC_DRAW  // usage
        );
    FORCE_ASSERT_GL_ERROR();
  }

  void SquareObject::m_update() {
    SquareScalarField sf(m_square);

    ContourBuilder cb;
    auto contour = cb.buildContour(
        sf,  // scalar field
        MC_MARCHING_SQUARES,  // algorithm
        m_resolution, m_resolution,  // resolution
        Vec2(0.0f, 0.0f),  // min
        Vec2(1.0f, 1.0f)  // max
        );

    // Upload the new contour wireframe data to the GL
    m_updateWireframe(*contour);
  }

  void SquareObject::m_updateWireframe(const mc::Contour &contour) {
    // Copy the contour vertices into a buffer
    auto vertices = new WireframeVertex[contour.numVertices()];
    fprintf(stderr, "numVertices: %d\n", contour.numVertices());
    for (int i = 0; i < contour.numVertices(); ++i) {
      auto vertex = contour.vertex(i);
      vertices[i].pos[0] = vertex.pos.x;
      vertices[i].pos[1] = vertex.pos.y;
      assert(vertex.pos.z == 0.0f);
      vertices[i].pos[2] = vertex.pos.z;
      vertices[i].color[0] = 1.0f;
      vertices[i].color[1] = 1.0f;
      vertices[i].color[2] = 1.0f;
    }
    // Copy the vertices buffer to the GL
    glBindBuffer(GL_ARRAY_BUFFER, m_wireframeVertices);
    ASSERT_GL_ERROR();
    glBufferData(
        GL_ARRAY_BUFFER,  // target
        sizeof(WireframeVertex) * contour.numVertices(),  // size
        vertices,  // data
        GL_STATIC_DRAW  // usage
        );
    ASSERT_GL_ERROR();
    delete[] vertices;
    // Copy the contour indices into a buffer
    auto indices = new unsigned int[contour.numLines() * 2];
    for (int i = 0; i < contour.numLines(); ++i) {
      auto line = contour.line(i);
      indices[i * 2] = line.a;
      indices[i * 2 + 1] = line.b;
    }
    // Copy the indices buffer to the GL
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_wireframeIndices);
    ASSERT_GL_ERROR();
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,  // target
        sizeof(unsigned int) * contour.numLines() * 2,  // size
        indices,  // data
        GL_STATIC_DRAW  // usage
        );
    ASSERT_GL_ERROR();
    delete[] indices;
    m_numWireframeIndices = contour.numLines() * 2;
  }

  void SquareObject::m_drawWireframe(
      const glm::mat4 &modelView,
      const glm::mat4 &projection)
  {
    // Use the wireframe shader
    auto shader = Shaders::wireframeShader();
    shader->use();

    // Prepare the uniform values
    assert(shader->modelViewLocation() != -1);
    glUniformMatrix4fv(
        shader->modelViewLocation(),  // location
        1,  // count
        0,  // transpose
        glm::value_ptr(modelView)  // value
        );
    ASSERT_GL_ERROR();
    assert(shader->projectionLocation() != -1);
    glUniformMatrix4fv(
        shader->projectionLocation(),  // location
        1,  // count
        0,  // transpose
        glm::value_ptr(projection)  // value
        );
    ASSERT_GL_ERROR();

    // Prepare the vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, m_wireframeVertices);
    ASSERT_GL_ERROR();
    assert(shader->vertPositionLocation() != -1);
    glEnableVertexAttribArray(shader->vertPositionLocation());
    ASSERT_GL_ERROR();
    glVertexAttribPointer(
        shader->vertPositionLocation(),  // index
        3,  // size
        GL_FLOAT,  // type
        0,  // normalized
        sizeof(WireframeVertex),  // stride
        &(((WireframeVertex *)0)->pos[0])  // pointer
        );
    ASSERT_GL_ERROR();
    assert(shader->vertColorLocation() != -1);
    glEnableVertexAttribArray(shader->vertColorLocation());
    ASSERT_GL_ERROR();
    glVertexAttribPointer(
        shader->vertColorLocation(),  // index
        3,  // size
        GL_FLOAT,  // type
        0,  // normalized
        sizeof(WireframeVertex),  // stride
        &(((WireframeVertex *)0)->color[0])  // pointer
        );
    ASSERT_GL_ERROR();

    // Draw the contour lines
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_wireframeIndices);
    ASSERT_GL_ERROR();
    glDrawElements(
        GL_LINES,  // mode
        m_numWireframeIndices,  // count
        GL_UNSIGNED_INT,  // type
        0  // indices
        );
    ASSERT_GL_ERROR();
  }

  void SquareObject::m_drawSquareWireframe(
      const glm::mat4 &modelView,
      const glm::mat4 &projection)
  {
    // Use the wireframe shader
    auto shader = Shaders::wireframeShader();
    shader->use();

    // Prepare the uniform values
    assert(shader->modelViewLocation() != -1);
    glUniformMatrix4fv(
        shader->modelViewLocation(),  // location
        1,  // count
        0,  // transpose
        glm::value_ptr(modelView)  // value
        );
    ASSERT_GL_ERROR();
    assert(shader->projectionLocation() != -1);
    glUniformMatrix4fv(
        shader->projectionLocation(),  // location
        1,  // count
        0,  // transpose
        glm::value_ptr(projection)  // value
        );
    ASSERT_GL_ERROR();

    // Prepare the vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, m_squareWireframeVertices);
    ASSERT_GL_ERROR();
    assert(shader->vertPositionLocation() != -1);
    glEnableVertexAttribArray(shader->vertPositionLocation());
    ASSERT_GL_ERROR();
    glVertexAttribPointer(
        shader->vertPositionLocation(),  // index
        3,  // size
        GL_FLOAT,  // type
        0,  // normalized
        sizeof(WireframeVertex),  // stride
        &(((WireframeVertex *)0)->pos[0])  // pointer
        );
    ASSERT_GL_ERROR();
    assert(shader->vertColorLocation() != -1);
    glEnableVertexAttribArray(shader->vertColorLocation());
    ASSERT_GL_ERROR();
    glVertexAttribPointer(
        shader->vertColorLocation(),  // index
        3,  // size
        GL_FLOAT,  // type
        0,  // normalized
        sizeof(WireframeVertex),  // stride
        &(((WireframeVertex *)0)->color[0])  // pointer
        );
    ASSERT_GL_ERROR();

    // Draw the square wireframe lines
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_squareWireframeIndices);
    ASSERT_GL_ERROR();
    glDrawElements(
        GL_LINES,  // mode
        4 * 2,  // count
        GL_UNSIGNED_INT,  // type
        0  // indices
        );
    ASSERT_GL_ERROR();
  }

  void SquareObject::draw(const glm::mat4 &modelWorld,
      const glm::mat4 &worldView, const glm::mat4 &projection,
      float alpha, bool debug)
  {
    auto modelView = worldView * modelWorld;
    m_drawWireframe(modelView, projection);
    m_drawSquareWireframe(modelView, projection);
  }

  SquareObject::SquareScalarField::SquareScalarField(
      int square,
      float intensity)
    : m_square(square), m_intensity(intensity)
  {
  }

  float SquareObject::SquareScalarField::operator()(float x, float y, float z) {
    /* Bi-linear interpolation along x and y */
    float result = 0.0f;
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < 2; ++i) {
        int sampleIndex = (i << 0) | (j << 1);;
        float sample = m_square & (1 << sampleIndex) ? -m_intensity : 1.0f;
        result +=
          (i ? x : 1.0f - x) *
          (j ? y : 1.0f - y) *
          sample;
      }
    }
    return result;
  }
} } }
