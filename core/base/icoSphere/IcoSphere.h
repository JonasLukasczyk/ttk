/// \ingroup base
/// \class ttk::IcoSphere
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 1.09.2019
///
/// This filter creates an IcoSphere with a specified radius, center, and number
/// of subdivisions.

#pragma once

// base code includes
#include <Debug.h>

// std includes
#include <boost/functional/hash.hpp>
#include <unordered_map>
#include <vector>

namespace ttk {

  class IcoSphere : virtual public Debug {

  public:
    IcoSphere() {
      this->setDebugMsgPrefix("IcoSphere");
    };
    ~IcoSphere(){};

    /**
     * Efficiently computes for a given subdivision level the number of
     * resulting vertices and triangles.
     */
    int computeNumberOfVerticesAndTriangles(size_t &nVertices,
                                            size_t &nTriangles,
                                            const size_t nSubdivisions) const {
      nVertices = 12;
      nTriangles = 20;

      for(size_t i = 0; i < nSubdivisions; i++) {
        nVertices += (nTriangles * 3) / 2;
        nTriangles = nTriangles * 4;
      }

      return 1;
    };

    template <class idType>
    int translateIcoSphere(
      // Output
      float *vertexCoords,
      idType *connectivityList,

      // Input
      const size_t &icoSphereIndex,
      const size_t &nVerticesPerIcoSphere,
      const size_t &nTrianglesPerIcoSphere,
      const float *centers) const;

    /**
     * Computes an icosphere for a given subdivision level, radius, and center.
     */
    template <class idType>
    int computeIcoSphere(
      // Output
      float *vertexCoords,
      idType *connectivityList,

      // Input
      const size_t &nSubdivisions,
      const float &radius) const;

    /**
     * Computes normals for multiple icospheres with the same subdivision level,
     * radius, and center.
     */
    template <class idType>
    int computeIcoSphereNormals(
      // Output
      float *normals,

      // Input
      const size_t &nSpheres,
      const size_t &nSubdivisions,
      const float &radius) const;

    /**
     * Computes an icosphere for a given subdivision level, radius, and center.
     */
    template <class idType>
    int computeIcoSpheres(
      // Output
      float *vertexCoords,
      idType *connectivityList,

      // Input
      const size_t &nSpheres,
      const size_t &nSubdivisions,
      const float &radius,
      const float *center) const;

  private:
    /**
     * Adds the coordinates of a vertex to the vertexCoords array at
     * vertexIndex*3, return the current vertexIndex, and increases the
     * vertexIndex by one. Note, the original coordinates are first
     * normalized and then multiplied by the radius.
     */
    template <class idType>
    idType addVertex(const float &x,
                     const float &y,
                     const float &z,
                     const float &radius,
                     float *vertexCoords,
                     idType &vertexIndex) const {
      idType cIndex = vertexIndex * 3;

      double length = sqrt(x * x + y * y + z * z) / radius;
      vertexCoords[cIndex] = x / length;
      vertexCoords[cIndex + 1] = y / length;
      vertexCoords[cIndex + 2] = z / length;
      return vertexIndex++;
    };

    /**
     * Adds the ijk ids of a new triangle to the connectivityList at
     * triangleIndex*3, returns the current triangleIndex, and increases
     * the triangleIndex by one.
     */
    template <class idType>
    idType addTriangle(const idType &i,
                       const idType &j,
                       const idType &k,
                       idType *connectivityList,
                       idType &triangleIndex) const {
      idType cIndex = triangleIndex * 4;

      connectivityList[cIndex] = 3;
      connectivityList[cIndex + 1] = i;
      connectivityList[cIndex + 2] = j;
      connectivityList[cIndex + 3] = k;
      return triangleIndex++;
    };

    /**
     * Adds a new vertex at the middle of an edge between the vertices i
     * and j. If this vertex was already created in a previous call (as
     * an edge of an already processed triangle), the function only
     * returns the index of the existing vertex.
     */
    template <class idType>
    idType
      addMidVertex(const idType &i,
                   const idType &j,
                   std::unordered_map<std::pair<idType, idType>,
                                      idType,
                                      boost::hash<std::pair<idType, idType>>>
                     &processedEdges,
                   const float &radius,
                   float *vertexCoords,
                   idType &vertexIndex) const {
      bool firstIsSmaller = i < j;
      idType a = firstIsSmaller ? i : j;
      idType b = firstIsSmaller ? j : i;

      // Check if edge was already processed
      {
        auto it = processedEdges.find({a, b});
        if(it != processedEdges.end())
          return it->second;
      }

      // Otherwise add mid vertex
      {
        idType aOffset = a * 3;
        idType bOffset = b * 3;
        float mx = (vertexCoords[aOffset] + vertexCoords[bOffset]) / 2.0;
        float my
          = (vertexCoords[aOffset + 1] + vertexCoords[bOffset + 1]) / 2.0;
        float mz
          = (vertexCoords[aOffset + 2] + vertexCoords[bOffset + 2]) / 2.0;

        idType mIndex
          = this->addVertex(mx, my, mz, radius, vertexCoords, vertexIndex);
        processedEdges.insert({{a, b}, mIndex});
        return mIndex;
      }
    };
  };
} // namespace ttk

template <class idType>
int ttk::IcoSphere::computeIcoSphere(
  // Output
  float *vertexCoords,
  idType *connectivityList,

  // Input
  const size_t &nSubdivisions,
  const float &radius) const {

  // initialize timer and memory
  Timer timer;

  // print status
  this->printMsg("Computing Icosphere (r:" + std::to_string(radius)
                   + ", s:" + std::to_string(nSubdivisions) + ")",
                 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

  idType vertexIndex = 0;
  idType triangleIndex = 0;

  // build icosahedron
  {
    // create 12 vertices
    float t = (1.0 + sqrt(5.0)) / 2.0;
    this->addVertex(-1, t, 0, radius, vertexCoords, vertexIndex);
    this->addVertex(1, t, 0, radius, vertexCoords, vertexIndex);
    this->addVertex(-1, -t, 0, radius, vertexCoords, vertexIndex);
    this->addVertex(1, -t, 0, radius, vertexCoords, vertexIndex);

    this->addVertex(0, -1, t, radius, vertexCoords, vertexIndex);
    this->addVertex(0, 1, t, radius, vertexCoords, vertexIndex);
    this->addVertex(0, -1, -t, radius, vertexCoords, vertexIndex);
    this->addVertex(0, 1, -t, radius, vertexCoords, vertexIndex);

    this->addVertex(t, 0, -1, radius, vertexCoords, vertexIndex);
    this->addVertex(t, 0, 1, radius, vertexCoords, vertexIndex);
    this->addVertex(-t, 0, -1, radius, vertexCoords, vertexIndex);
    this->addVertex(-t, 0, 1, radius, vertexCoords, vertexIndex);

    // create 20 triangles
    this->addTriangle<idType>(0, 11, 5, connectivityList, triangleIndex);
    this->addTriangle<idType>(0, 5, 1, connectivityList, triangleIndex);
    this->addTriangle<idType>(0, 1, 7, connectivityList, triangleIndex);
    this->addTriangle<idType>(0, 7, 10, connectivityList, triangleIndex);
    this->addTriangle<idType>(0, 10, 11, connectivityList, triangleIndex);
    this->addTriangle<idType>(1, 5, 9, connectivityList, triangleIndex);
    this->addTriangle<idType>(5, 11, 4, connectivityList, triangleIndex);
    this->addTriangle<idType>(11, 10, 2, connectivityList, triangleIndex);
    this->addTriangle<idType>(10, 7, 6, connectivityList, triangleIndex);
    this->addTriangle<idType>(7, 1, 8, connectivityList, triangleIndex);
    this->addTriangle<idType>(3, 9, 4, connectivityList, triangleIndex);
    this->addTriangle<idType>(3, 4, 2, connectivityList, triangleIndex);
    this->addTriangle<idType>(3, 2, 6, connectivityList, triangleIndex);
    this->addTriangle<idType>(3, 6, 8, connectivityList, triangleIndex);
    this->addTriangle<idType>(3, 8, 9, connectivityList, triangleIndex);
    this->addTriangle<idType>(4, 9, 5, connectivityList, triangleIndex);
    this->addTriangle<idType>(2, 4, 11, connectivityList, triangleIndex);
    this->addTriangle<idType>(6, 2, 10, connectivityList, triangleIndex);
    this->addTriangle<idType>(8, 6, 7, connectivityList, triangleIndex);
    this->addTriangle<idType>(9, 8, 1, connectivityList, triangleIndex);
  }

  // refine icosahedron
  if(nSubdivisions > 0) {
    // create temporary connectivityList
    size_t nVertices = 0;
    size_t nTriangles = 0;
    this->computeNumberOfVerticesAndTriangles(
      nVertices, nTriangles, nSubdivisions);
    std::vector<idType> connectivityListTemp(nTriangles * 4, 0);

    // cache to store processed edges
    std::unordered_map<std::pair<idType, idType>, idType,
                       boost::hash<std::pair<idType, idType>>>
      processedEdges;

    // iterate over nSubdivisions
    for(size_t s = 0; s < nSubdivisions; s++) {
      // swap lists
      idType *oldList
        = s % 2 == 0 ? connectivityList : connectivityListTemp.data();
      idType *newList
        = s % 2 != 0 ? connectivityList : connectivityListTemp.data();

      // reset indicies
      idType nOldTriangles = triangleIndex;
      triangleIndex = 0;

      for(size_t i = 0; i < nOldTriangles; i++) {
        idType offset = i * 4;

        // compute mid points
        idType a = this->addMidVertex(oldList[offset + 1], oldList[offset + 2],
                                      processedEdges, radius, vertexCoords,
                                      vertexIndex);
        idType b = this->addMidVertex(oldList[offset + 2], oldList[offset + 3],
                                      processedEdges, radius, vertexCoords,
                                      vertexIndex);
        idType c = this->addMidVertex(oldList[offset + 3], oldList[offset + 1],
                                      processedEdges, radius, vertexCoords,
                                      vertexIndex);

        // replace triangle by 4 triangles
        this->addTriangle(oldList[offset + 1], a, c, newList, triangleIndex);
        this->addTriangle(oldList[offset + 2], b, a, newList, triangleIndex);
        this->addTriangle(oldList[offset + 3], c, b, newList, triangleIndex);
        this->addTriangle(a, b, c, newList, triangleIndex);
      }

      // print progress
      this->printMsg("Computing Icosphere (r:" + std::to_string(radius)
                       + ", s:" + std::to_string(nSubdivisions) + ")",
                     ((float)s) / nSubdivisions, ttk::debug::LineMode::REPLACE);
    }

    // if uneven number of nSubdivisions then copy temp buffer to output buffer
    if(nSubdivisions > 0 && nSubdivisions % 2 != 0) {
      size_t n = nTriangles * 4;
      for(size_t i = 0; i < n; i++)
        connectivityList[i] = connectivityListTemp[i];
    }
  }

  // print progress
  this->printMsg("Computing Icosphere (r:" + std::to_string(radius)
                   + ", s:" + std::to_string(nSubdivisions) + ")",
                 1, timer.getElapsedTime(), this->threadNumber_);

  return 1;
};

template <class idType>
int ttk::IcoSphere::translateIcoSphere(float *vertexCoords,
                                       idType *connectivityList,
                                       const size_t &icoSphereIndex,
                                       const size_t &nVerticesPerIcoSphere,
                                       const size_t &nTrianglesPerIcoSphere,
                                       const float *centers) const {
  size_t vertexCoordOffset = icoSphereIndex * nVerticesPerIcoSphere * 3;
  size_t connectivityListOffset = icoSphereIndex * nTrianglesPerIcoSphere * 4;
  size_t temp = icoSphereIndex * 3;
  const float &centerX = centers[temp++];
  const float &centerY = centers[temp++];
  const float &centerZ = centers[temp];

  // vertex coords
  for(size_t i = 0, limit = nVerticesPerIcoSphere * 3; i < limit;) {
    vertexCoords[vertexCoordOffset++] = vertexCoords[i++] + centerX;
    vertexCoords[vertexCoordOffset++] = vertexCoords[i++] + centerY;
    vertexCoords[vertexCoordOffset++] = vertexCoords[i++] + centerZ;
  }

  // connectivity list
  size_t vertexIdOffset = icoSphereIndex * nVerticesPerIcoSphere;
  for(size_t i = 0, limit = nTrianglesPerIcoSphere * 4; i < limit;) {
    connectivityList[connectivityListOffset++] = connectivityList[i++];
    connectivityList[connectivityListOffset++]
      = connectivityList[i++] + vertexIdOffset;
    connectivityList[connectivityListOffset++]
      = connectivityList[i++] + vertexIdOffset;
    connectivityList[connectivityListOffset++]
      = connectivityList[i++] + vertexIdOffset;
  }

  return 1;
}

template <class idType>
int ttk::IcoSphere::computeIcoSphereNormals(
  // Output
  float *normals,

  // Input
  const size_t &nSpheres,
  const size_t &nSubdivisions,
  const float &radius) const {

  ttk::Timer timer;
  this->printMsg("Computing Normals", 0, 0, this->threadNumber_,
                 ttk::debug::LineMode::REPLACE);

  size_t nVerticesPerIcoSphere, nTrianglesPerIcoSphere;
  if(!this->computeNumberOfVerticesAndTriangles(
       nVerticesPerIcoSphere, nTrianglesPerIcoSphere, nSubdivisions))
    return 0;

  std::vector<vtkIdType> normalConnectivity(nTrianglesPerIcoSphere * 4);
  std::vector<float> normalCenter = {0, 0, 0};

  auto debugLevel = this->debugLevel_;
  this->debugLevel_ = 0;
  int status
    = this->computeIcoSpheres<idType>(normals, normalConnectivity.data(), 1,
                                      nSubdivisions, 1, normalCenter.data());
  this->debugLevel_ = debugLevel;
  if(!status)
    return 0;

// copy normals
#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(threadNumber_)
#endif
  for(size_t i = 1; i < nSpheres; i++) {
    size_t offset = nVerticesPerIcoSphere * i * 3;
    size_t offset2 = 0;
    for(size_t v = 0; v < nVerticesPerIcoSphere; v++) {
      normals[offset++] = normals[offset2++];
      normals[offset++] = normals[offset2++];
      normals[offset++] = normals[offset2++];
    }
  }

  this->printMsg(
    "Computing Normals", 1, timer.getElapsedTime(), this->threadNumber_);
  return 1;
}

template <class idType>
int ttk::IcoSphere::computeIcoSpheres(
  // Output
  float *vertexCoords,
  idType *connectivityList,

  // Input
  const size_t &nSpheres,
  const size_t &nSubdivisions,
  const float &radius,
  const float *centers) const {

  if(nSpheres < 1) {
    this->printWrn("Number of input points smaller than 1.");
    return 1;
  }

  // compute number of vertices and triangles for one ico sphere
  size_t nVerticesPerIcoSphere, nTrianglesPerIcoSphere;
  if(!this->computeNumberOfVerticesAndTriangles(
       nVerticesPerIcoSphere, nTrianglesPerIcoSphere, nSubdivisions))
    return 0;

  // compute ico sphere around origin
  if(!this->computeIcoSphere(
       vertexCoords, connectivityList, nSubdivisions, radius))
    return 0;

  // translate remaining spheres
  ttk::Timer timer;
  this->printMsg("Translating " + std::to_string(nSpheres) + " Icosphere(s)", 0,
                 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(threadNumber_)
#endif
  for(size_t i = 1; i < nSpheres; i++) {
    this->translateIcoSphere(
      vertexCoords, connectivityList,

      i, nVerticesPerIcoSphere, nTrianglesPerIcoSphere, centers

    );
  }

  // translate first ico sphere
  this->translateIcoSphere(vertexCoords, connectivityList, 0,
                           nVerticesPerIcoSphere, nTrianglesPerIcoSphere,
                           centers);
  // print status
  this->printMsg("Translating " + std::to_string(nSpheres) + " Icosphere(s)", 1,
                 timer.getElapsedTime(), this->threadNumber_);

  return 1;
}