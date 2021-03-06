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

#ifndef MC_SAMPLES_TERRAIN_TERRAIN_MESH_H_
#define MC_SAMPLES_TERRAIN_TERRAIN_MESH_H_

#include <mcxx/scalarField.h>

#include "../common/meshObject.h"
#include "lodTree.h"

namespace mc { namespace samples { namespace terrain {
  /**
   * A mesh within a voxel terrain. This mesh is only a piece of the voxel
   * terrain, representing a cube of voxels.
   *
   * The size of the mesh is determined by the level of detail specified, but
   * the resolution of the mesh is the same for any level of detail. This is
   * inspired by the method described by Lengyel in \cite Lengyel:2010.
   */
  class TerrainMesh : public MeshObject {
    private:
      bool m_empty;
    public:
      /**
       * The number of samples along each axis in the sample lattice for each
       * terrain mesh object. This determines how complicated each mesh is,
       * since the isosurface extraction algorithm will use a sample lattice
       * with dimensions BLOCK_SIZE by BLOCK_SIZE by BLOCK_SIZE.
       */
      static const int BLOCK_SIZE = 16;
      /**
       * The distance (in scene units) between voxels. This determines how fine
       * the meshes are in the highest level of detail.
       */
      static const int VOXEL_DELTA = 1.0f;

      /**
       * Constructs a terrain mesh object representing the given scalar field
       * function at the given level of detail. The integer coordinates
       * determine the terrain mesh's position in the terrain octree.
       *
       * The terrain mesh is constructed without making any calls to the GL.
       * It is necessary to avoid making GL calls because our terrain meshes
       * are generated in worker threads outside of the main thread.
       *
       * \param sf The scalar field function that defines the terrain surface.
       * \param block The coordinates of this terrain mesh in the voxel block
       * octree.
       * \param lod The level of detail of the terrain mesh. The zero
       * value is the highest level of detail, and subsequent ascending values
       * reduce the level of detail by half in each dimension.
       */
      TerrainMesh(
          ScalarField &sf,
          const LodTree::Coordinates &block,
          int lod);

      ~TerrainMesh();

      /**
       * \return True if the mesh generated by the isosurface extraction
       * algorithm contained no vertices, false otherwise.
       */
      bool isEmpty() const { return m_empty; }
  };
} } }

#endif
