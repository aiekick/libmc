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

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include <mc/algorithms/common/cube.h>
#include <mc/algorithms/transvoxel/edges.h>
#include <mc/algorithms/transvoxel/transform.h>
#include <mc/algorithms/transvoxel/triangles.h>

#include <mc/algorithms/transvoxel/transvoxel.h>

mcTransvoxel_Params mcTransvoxel_defaultParams = {
  .transitionFace = -1,
  .transitionScale = 0.67,
};

void mcTransvoxel_transitionMeshFromField(
    mcScalarFieldWithArgs sf, const void *args,
    unsigned int x_res, unsigned int y_res,
    const mcVec3 *min, const mcVec3 *max,
    mcMesh *mesh)
{
  mcFace triangle;
  mcFace_init(&triangle, 3);
//  assert(min->z == max->z);
  float delta_x = fabs(max->x - min->x) / (float)(x_res - 1);
  float delta_y = fabs(max->y - min->y) / (float)(y_res - 1);
  /* FIMXE: Should delta_z be determined by the transition scale parameter? */
  for (int y = 0; y < y_res - 1; ++y) {
    for (int x = 0; x < x_res - 1; ++x) {
      /* TODO: Determine the transition cell configuration from the samples */
      int cell = 0;
      for (
          unsigned int sampleIndex = 0;
          sampleIndex < MC_TRANSVOXEL_NUM_TRANSITION_CELL_SAMPLES;
          ++sampleIndex)
      {
        int pos[3];
        mcTransvoxel_transitionCellSampleRelativePosition(sampleIndex, pos);
        float sample = sf(min->x + (float)(x + pos[0]) * delta_x * 0.5f,
                          min->x + (float)(y + pos[1]) * delta_y * 0.5f,
                          min->z,
                          args);
        /* Add the bit this sample contributes to the transition cell */
        cell |= (sample >= 0.0f ? 0 : 1) << sampleIndex;
      }
      if (cell == 0 || cell == 0x1ff) {
        /* Skip trivial transition cells */
        continue;
      }
      fprintf(stderr, "cell: 0x%03x\n", cell);
      /* Look up the edges that are intersected in this transition cell */
      const int *edgeIntersections =
        mcTransvoxel_transitionCellEdgeIntersectionTable[cell].edges;
      int edgeIntersectionsIndex = 0;
      int vertexIndices[MC_TRANSVOXEL_NUM_TRANSITION_CELL_EDGES];
      for (
          int edge = 0;
          edge < MC_TRANSVOXEL_NUM_TRANSITION_CELL_EDGES;
          ++edge)
      {
        vertexIndices[edge] = -1;
        int edgeIntersection = edgeIntersections[edgeIntersectionsIndex];
        if (edge == edgeIntersection) {
          /* TODO: Look for the vertex index of this edge in our buffers */
          if (vertexIndices[edge] == -1) {
            /* The mesh vertex for this edge has not been generated yet */
            int sampleIndices[2];
            float values[2];
            mcVec3 latticePos[2];
            /* Find the sample indices on this edge */
            mcTransvoxel_transitionCellEdgeSamples(edge, sampleIndices);
            /* Compute the lattice positions and samples on this edge */
            for (int i = 0; i < 2; ++i) {
              int rel[2], abs[2];
              mcTransvoxel_transitionCellSampleRelativePosition(
                  sampleIndices[i], rel);
              abs[0] = x + rel[0];
              abs[1] = y + rel[1];
              /* NOTE: These lattice positions are in mesh space coordinates,
               * not sample space coordinates. The vertices of the mesh we
               * generate must be in mesh space coordinates in which min is
               * at the origin. */
              latticePos[i].x = (float)(abs[0]) * delta_x * 0.5f;
              latticePos[i].y = (float)(abs[1]) * delta_y * 0.5f;
              /* FIXME: I'm not sure what the z lattice position should be. It
               * might need to incorporate a delta_z value. At any rate, there
               * can be no edge intersections on the lateral edges, so the
               * lattice position is determined by the value of the edge. */
              latticePos[i].z = edge > 3 ? 0.0f : 1.0f;
              /* TODO: Retrieve this sample value from a buffer */
              values[i] = sf(
                min->x + latticePos[i].x,
                min->y + latticePos[i].y,
                min->z,
                args);
            }
            /* TODO: Interpolate between the sample values at each vertex */
            float weight = fabs(values[0] / (values[0] - values[1]));
            /* The corresponding edge vertex must lie on the edge between the
             * lattice points, so we interpolate between these points. */
            mcVertex vertex;
            vertex.pos = mcVec3_lerp(&latticePos[0], &latticePos[1], weight);
            /* TODO: Compute the surface normal */
            /* Add this vertex to the mesh */
            vertexIndices[edge] = mcMesh_addVertex(mesh, &vertex);
            fprintf(stderr, "added mesh vertex: %d\n",
                vertexIndices[edge]);
            /* XXX: Draw a small triangle to aid in debugging of the vertices */
            /*
            vertex.pos.x = vertex.pos.x + 0.1 * delta_x;
            mcMesh_addVertex(mesh, &vertex);
            vertex.pos.y = vertex.pos.y + 0.1 * delta_y;
            mcMesh_addVertex(mesh, &vertex);
            triangle.indices[0] = vertexIndices[edge];
            triangle.indices[1] = vertexIndices[edge] + 1;
            triangle.indices[2] = vertexIndices[edge] + 2;
            mcMesh_addFace(mesh, &triangle);
            */
          }
          edgeIntersectionsIndex += 1;
        }
        /* TODO: Add the vertex for this edge intersection to the appropriate
         * buffer */
      }
      fprintf(stderr, "vertexIndices: ");
      for (int i = 0; i < MC_TRANSVOXEL_NUM_TRANSITION_CELL_EDGES; ++i) {
        fprintf(stderr, "%d, ", vertexIndices[i]);
      }
      fprintf(stderr, "\n");
      /* TODO: Look in the triangulation table for the triangles
       * corresponding to this transition cell configuration. */
      for (int i = 0; i < MC_TRANSVOXEL_MAX_TRANSITION_CELL_TRIANGLES; ++i) {
        const mcTransvoxel_Triangle *t =
          &mcTransvoxel_transitionCellTriangulationTable[cell].triangles[i];
        if (t->edgeIntersections[0] == -1)
          break;  /* No more triangles */
        fprintf(stderr,
            "t->edgeIntersections: %d, %d, %d\n",
            t->edgeIntersections[0],
            t->edgeIntersections[1],
            t->edgeIntersections[2]);
        triangle.indices[0] = vertexIndices[t->edgeIntersections[0]];
        triangle.indices[1] = vertexIndices[t->edgeIntersections[1]];
        triangle.indices[2] = vertexIndices[t->edgeIntersections[2]];
        assert(triangle.indices[0] != -1);
        assert(triangle.indices[1] != -1);
        assert(triangle.indices[2] != -1);
        mcMesh_addFace(mesh, &triangle);
        fprintf(stderr,
            "transvoxel triangle %d:\n"
            "  edge intersections: %d, %d, %d\n"
            "  vertex indices: %d, %d, %d\n"
            "  vertices:\n"
            ,
            i,
            t->edgeIntersections[0],
            t->edgeIntersections[1],
            t->edgeIntersections[2],
            triangle.indices[0],
            triangle.indices[1],
            triangle.indices[2]);
        for (int j = 0; j < 3; ++j) {
          int index = triangle.indices[j];
          fprintf(stderr,
              "    vertex %d:\n"
              "      pos: %g, %g, %g\n",
              index,
              mesh->vertices[index].pos.x,
              mesh->vertices[index].pos.y,
              mesh->vertices[index].pos.z);

        }
      }
    }
  }
}

void mcTransvoxel_isosurfaceFromField(
    mcScalarFieldWithArgs sf, const void *args,
    unsigned int x_res, unsigned int y_res, unsigned int z_res,
    const mcVec3 *min, const mcVec3 *max,
    const mcTransvoxel_Params *params,
    mcMesh *mesh)
{
  if (params == NULL) {
    params = &mcTransvoxel_defaultParams;
  }
  float delta_x = fabs(max->x - min->x) / (float)(x_res - 1);
  float delta_y = fabs(max->y - min->y) / (float)(y_res - 1);
  float delta_z = fabs(max->z - min->z) / (float)(z_res - 1);
  // Iterate through the sample lattice
  if (params->transitionFace & (1 << MC_CUBE_FACE_BOTTOM))
    fprintf(stderr, "Transition on bottom face!\n");
  for (int z = 0; z < z_res - 1; ++z) {
    for (int y = 0; y < y_res - 1; ++y) {
      for (int x = 0; x < x_res - 1; ++x) {
      }
    }
  }
}
