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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mc/algorithms/common/cube.h>
#include <mc/algorithms/simple/common.h>

#include "../common/cube_tables.h"

#define get_byte(num, byte) (((num) & (0xff << (8 * byte))) >> (8 * byte))

/*
 * This program generates the edge and triangularization tables needed for
 * implementing a performant marching cubes algorithm. While these tables are
 * available pre-generated on the internet, generating these tables ourselves
 * serves as a stepping stone towards generating larger tables used by more
 * sophisticated variants of the marching cubes algorithm.
 *
 * The first table generated is the edge table, whose purpose is to determine
 * which edges are intersected for a given voxel configuration. The marching
 * cubes algorithm uses this information to interpolate between samples on the
 * relevent edges. This table is relatively easy to generate, since any edge
 * whose samples fall on different sides of the isosurface must be intersected
 * by the isosurface.
 *
 * The second table generated is the triangularization table. This table is
 * used to quickly find a triangle representation for the voxel that can be
 * used in the resulting surface mesh. This table is much more complicated to
 * generate, but it can build upon the edge table since triangle vertices must
 * lie on intersected edges.
 */

void computeEdgeList(
    unsigned int cube,
    mcSimpleEdgeList *edgeList)
{
  unsigned int vertices[2];
  unsigned int listIndex = 0;
  /* Iterate through all edges */
  /* NOTE: The edges MUST be in sorted order. This makes it much easier for the
   * marching cubes algorithm to determine which edges are not in the edge
   * list. */
  for (unsigned int edge = 0; edge < MC_CUBE_NUM_EDGES; ++edge) {
    /* Determine the two vertex values */
    mcCube_edgeVertices(edge, vertices);
    if (mcCube_vertexValue(vertices[0], cube)
        != mcCube_vertexValue(vertices[1], cube))
    {
      /* If the vertex values disagree, we have an edge intersection */
      /* Add this edge to the edge list */
      edgeList->edges[listIndex++] = edge;
    }
  }
}

void computeTriangleList(
    unsigned int cube,
    mcSimpleTriangleList *triangleList)
{
  unsigned int numTriangles;
  unsigned int canonical, rotation;
  mcSimpleTriangle *triangle;

  memset(triangleList, -1, sizeof(mcSimpleTriangleList));
  numTriangles = 0;

  /* Determine this cube's canonical orientation and the corresponding
   * rotation sequences that brings it to that orientation */
  canonical = mcCube_canonicalOrientation(cube);
  rotation = mcCube_canonicalRotation(cube);
  /* Generate triangles for the canonical orientation */
#define make_triangle(a, b, c) \
  do { \
    triangle = &triangleList->triangles[numTriangles++]; \
    triangle->edges[0] = a; \
    triangle->edges[1] = b; \
    triangle->edges[2] = c; \
  } while(0)
  switch (canonical) {
    case MC_CUBE_CANONICAL_ORIENTATION_0:
      /* This is a cube entirely inside or outside the isosurface, with no need
       * to generate triangles */
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_1:
      /* This corresponds to a single triangle in one corner */
      make_triangle(0, 8, 3);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_2:
      /* This is the case where two samples on the same edge that are below the
       * isosurface. This makes a single quad. */
      make_triangle(1, 8, 3);
      make_triangle(1, 9, 8);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_3:
      /* This case has two samples on the front face which are below the
       * isosurface. Since these samples are diagonal from each other, this is
       * a case of an ambiguous face. See "The asymptotic Decider: Resolving
       * the Ambiguity in Marching Cubes," Nielson. */
      make_triangle(0, 8, 3);
      make_triangle(1, 2, 11);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_4:
      /* This case has three samples on the front face in an "L" shape that are
       * below the isosurface. The result resembles a fan or paper airplane. */
      make_triangle(2, 11, 3);
      make_triangle(3, 11, 8);
      make_triangle(8, 11, 9);
      /* Alternative triangulation: */
      /*
      make_triangle(3, 9, 8);
      make_triangle(2, 11, 9);
      make_triangle(2, 9, 3);
      */
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_5:
      /* In this case, four samples on one face are below the isosurface. This
       * gives a quad that divides the cube squarely in half. */
      make_triangle(8, 10, 11);
      make_triangle(8, 11, 9);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_6:
      /* This case has two samples below the isosurface on opposite corners of
       * the cube. */
      make_triangle(1, 2, 11);
      make_triangle(4, 7, 8);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_7:
      /* This case has two samples on the same edge that are below the
       * isosurface that generate a quad, and a third sample diagonal from the
       * other two that generates a lone triangle. Since this case has a face
       * with samples diagonal from each other, we again have an ambiguous
       * face. */
      make_triangle(0, 4, 3);
      make_triangle(3, 4, 7);
      make_triangle(1, 2, 11);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_8:
      /* For this case, the four samples below the isosurface are arranged in
       * what appears to be a serpentine shape along the edges of the cube.
       * This is one of two cases that look like this. Only way to
       * differentiate these two cases visually is by observing the handedness
       * of the shape. This particular case has a "Z" shape when viewed from
       * the outside of the isosurface looking in. */
      make_triangle(2, 11, 9);
      make_triangle(2, 7, 3);
      make_triangle(4, 7, 9);
      make_triangle(2, 9, 7);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_9:
      /* This case has three samples mutually diagonal from each other that
       * generate three separate triangles. This case has a number of ambiguous
       * faces. */
      make_triangle(1, 9, 0);
      make_triangle(2, 3, 10);
      make_triangle(4, 7, 8);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_10:
      /* This case places has four samples below the isosurface arranged
       * symmetrically so that the isosurface appears to intersect the cube at
       * an angle into equal parts. */
      make_triangle(2, 7, 10);
      make_triangle(1, 9, 2);
      make_triangle(4, 7, 9);
      make_triangle(2, 9, 7);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_11:
      /* For this case, the four samples below the isosurface are arranged in
       * what appears to be a serpentine shape along the edges of the cube.
       * This is one of two cases that look like this. Only way to
       * differentiate these two cases visually is by observing the handedness
       * of the shape. This particular case has a "S" shape when viewed from
       * the outside of the isosurface looking in. */
      make_triangle(7, 10, 11);
      make_triangle(0, 11, 1);
      make_triangle(0, 4, 7);
      make_triangle(0, 7, 11);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_12:
      /* This configuration has three samples under the isosurface in an "L"
       * shape, and a fourth sample apart from the other three. */
      make_triangle(4, 7, 8);
      make_triangle(0, 3, 10);
      make_triangle(0, 10, 9);
      make_triangle(9, 10, 11);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_13:
      /* This is the case with two quads facing each other. Two ambiguous edges
       * are present. */
      make_triangle(3, 10, 11);
      make_triangle(1, 3, 11);
      make_triangle(5, 7, 8);
      make_triangle(5, 8, 9);
      break;
    case MC_CUBE_CANONICAL_ORIENTATION_14:
      /* This case has four separated samples below the isosurface that
       * generate four separate triangles. */
      make_triangle(0, 1, 9);
      make_triangle(2, 3, 10);
      make_triangle(4, 7, 8);
      make_triangle(5, 11, 6);
      break;
  }
  fprintf(stderr, "triangleList before: {\n");
  for (int i = 0; i < MC_SIMPLE_MAX_TRIANGLES; ++i) {
    fprintf(stderr, "  { ");
    for (int j = 0; j < 3; ++j) {
      fprintf(stderr, "%2d, ",
          triangleList->triangles[i].edges[j]);
    }
    fprintf(stderr, "}, \n");
  }
  fprintf(stderr, "}\n");

  /* Rotate the canonical triangles back into our cube's orientation */
  for (int i = 0; i < MC_SIMPLE_MAX_TRIANGLES; ++i) {
    triangle = &triangleList->triangles[i];
    if (triangle->edges[0] == -1)
      break;  /* No more triangles to consider */
    /* Iterate over each triangle edge intersection */
    for (int j = 0; j < 3; ++j) {
      /* Rotate the triangle edge intersection about the y-axis */
      for (int k = 0; k < get_byte(rotation, 2); ++k) {
        triangle->edges[j] = mcCube_rotateEdgeReverseY(triangle->edges[j]);
      }
      /* Rotate the triangle edge intersection about the x-axis */
      for (int k = 0; k < get_byte(rotation, 1); ++k) {
        triangle->edges[j] = mcCube_rotateEdgeReverseX(triangle->edges[j]);
      }
      /* Rotate the triangle edge intersection about the z-axis */
      for (int k = 0; k < get_byte(rotation, 0); ++k) {
        triangle->edges[j] = mcCube_rotateEdgeReverseZ(triangle->edges[j]);
      }
    }
    /* TODO: Consider that cube inversion affects triangle winding order */
  }
}

void printEdgeTable(const mcSimpleEdgeList *edgeTable) {
  fprintf(stdout,
      "const mcSimpleEdgeList mcSimple_edgeTable[] = {\n");
  for (unsigned int cube = 0; cube <= 0xFF; ++cube) {
    fprintf(stdout,
        "  { .edges = { %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d } },  /* 0x%02x */\n",
        edgeTable[cube].edges[0],
        edgeTable[cube].edges[1],
        edgeTable[cube].edges[2],
        edgeTable[cube].edges[3],
        edgeTable[cube].edges[4],
        edgeTable[cube].edges[5],
        edgeTable[cube].edges[6],
        edgeTable[cube].edges[7],
        edgeTable[cube].edges[8],
        edgeTable[cube].edges[9],
        edgeTable[cube].edges[10],
        edgeTable[cube].edges[11],
        cube
        );
  }
  fprintf(stdout,
      "};\n");
}

void printTriangulationTable(
    const mcSimpleTriangleList *triangulationTable)
{
  fprintf(stdout,
      "const mcSimpleTriangleList mcSimple_triangulationTable[] = {\n");
  for (unsigned int cube = 0; cube <= 0xFF; ++cube) {
    fprintf(stdout,
        "  { .triangles = \n"
        "    {\n"
        "      { .edges = { %d, %d, %d } },\n"
        "      { .edges = { %d, %d, %d } },\n"
        "      { .edges = { %d, %d, %d } },\n"
        "      { .edges = { %d, %d, %d } },\n"
        "    },\n"
        "  },\n",
        triangulationTable[cube].triangles[0].edges[0],
        triangulationTable[cube].triangles[0].edges[1],
        triangulationTable[cube].triangles[0].edges[2],
        triangulationTable[cube].triangles[1].edges[0],
        triangulationTable[cube].triangles[1].edges[1],
        triangulationTable[cube].triangles[1].edges[2],
        triangulationTable[cube].triangles[2].edges[0],
        triangulationTable[cube].triangles[2].edges[1],
        triangulationTable[cube].triangles[2].edges[2],
        triangulationTable[cube].triangles[3].edges[0],
        triangulationTable[cube].triangles[3].edges[1],
        triangulationTable[cube].triangles[3].edges[2]
        );
  }
  fprintf(stdout,
      "};\n");
}

int main(int argc, char **argv) {
  /* TODO: Parse the arguments */

  /* Allocate memory for the edge table */
  mcSimpleEdgeList *edgeTable =
    (mcSimpleEdgeList*)malloc(sizeof(mcSimpleEdgeList) * 256);
  memset(edgeTable, -1, sizeof(mcSimpleEdgeList) * 256);
  /* Allocate memory for the triangulization table */
  mcSimpleTriangleList *triangulationTable =
    (mcSimpleTriangleList*)malloc(sizeof(mcSimpleTriangleList) * 256);
  memset(triangulationTable, -1, sizeof(mcSimpleTriangleList) * 256);

  /* Iterate through all voxel cube configurations */
  for (unsigned int cube = 0; cube <= 0xFF; ++cube) {

    /* Compute the edge list for this configuration */
    computeEdgeList(cube, &edgeTable[cube]); 

    /* Compute the triangulation list for this configuration */
    computeTriangleList(cube, &triangulationTable[cube]);

    fprintf(stderr, "cube: 0x%02x\n", cube);
    fprintf(stderr, "edgeList: { ");
    for (int i = 0; i < MC_CUBE_NUM_EDGES; ++i) {
      fprintf(stderr, "%2d", edgeTable[cube].edges[i]);
      if (i != MC_CUBE_NUM_EDGES - 1)
        fprintf(stderr, ", ");
    }
    fprintf(stderr, " } \n");
    fprintf(stderr, "triangleList: {\n");
    for (int i = 0; i < MC_SIMPLE_MAX_TRIANGLES; ++i) {
      fprintf(stderr, "  { ");
      for (int j = 0; j < 3; ++j) {
        fprintf(stderr, "%2d, ",
            triangulationTable[cube].triangles[i].edges[j]);
      }
      fprintf(stderr, "}, \n");
    }
    fprintf(stderr, "}\n");

#ifndef NDEBUG
    /* Ensure that the edge and triangulation tables agree */
    for (int i = 0; i < MC_SIMPLE_MAX_TRIANGLES; ++i) {
      mcSimpleTriangle *triangle = &triangulationTable[cube].triangles[i];
      if (triangle->edges[0] == -1)
        break;  /* No more triangles to consider */
      for (int j = 0; j < 3; ++j) {
        int found;
        unsigned int edge = triangle->edges[j];
        /* Look for this edge in the edge list */
        found = 0;
        for (int k = 0; k < MC_CUBE_NUM_EDGES; ++k) {
          if (edgeTable[cube].edges[k] == edge) {
            found = 1;
            break;
          }
        }
        assert(found);
      }
    }
#endif
  }

  /* Print the necessary headers */
  fprintf(stdout, "#include <mc/algorithms/simple/common.h>\n\n");

  /* Print the edge table */
  printEdgeTable(edgeTable);

  fprintf(stdout, "\n");

  /* Print the triangulation table */
  printTriangulationTable(triangulationTable);

  free(edgeTable);
  free(triangulationTable);
}
