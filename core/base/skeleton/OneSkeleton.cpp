#include <OneSkeleton.h>
#include <boost/container/small_vector.hpp>

using namespace std;
using namespace ttk;

OneSkeleton::OneSkeleton() {
  setDebugMsgPrefix("OneSkeleton");
}

// 2D cells (triangles)
int OneSkeleton::buildEdgeLinks(
  const vector<std::array<SimplexId, 2>> &edgeList,
  const FlatJaggedArray &edgeStars,
  const CellArray &cellArray,
  FlatJaggedArray &edgeLinks) const {

#ifndef TTK_ENABLE_KAMIKAZE
  if(edgeList.empty())
    return -1;
  if(edgeStars.subvectorsNumber() != edgeList.size())
    return -2;
#endif

  Timer t;

  const SimplexId edgeNumber = edgeStars.subvectorsNumber();
  std::vector<SimplexId> offsets(edgeNumber + 1);
  // one vertex per star
  std::vector<SimplexId> links(edgeStars.dataSize());

  printMsg(
    "Building edge links", 0, 0, threadNumber_, ttk::debug::LineMode::REPLACE);

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(threadNumber_)
#endif // TTK_ENABLE_OPENMP
  for(SimplexId i = 0; i < edgeNumber; i++) {
    // copy the edgeStars offsets array
    offsets[i] = edgeStars.offset(i);

    for(SimplexId j = 0; j < edgeStars.size(i); j++) {
      // for each cell/triangle in edge i's star, get the opposite vertex
      for(int k = 0; k < 3; k++) {
        const auto v = cellArray.getCellVertex(edgeStars.get(i, j), k);
        if(v != edgeList[i][0] && v != edgeList[i][1]) {
          // edge i does not contain vertex i
          links[offsets[i] + j] = v;
          break;
        }
      }
    }
  }

  // don't forget the last offset
  offsets[edgeNumber] = edgeStars.offset(edgeNumber);

  edgeLinks.setData(std::move(links), std::move(offsets));

  printMsg("Built " + to_string(edgeNumber) + " edge links", 1,
           t.getElapsedTime(), threadNumber_);

  return 0;
}

// 3D cells (tetrahedron)
int OneSkeleton::buildEdgeLinks(
  const vector<std::array<SimplexId, 2>> &edgeList,
  const FlatJaggedArray &edgeStars,
  const vector<std::array<SimplexId, 6>> &cellEdges,
  FlatJaggedArray &edgeLinks) const {

#ifndef TTK_ENABLE_KAMIKAZE
  if(edgeList.empty())
    return -1;
  if((edgeStars.empty()) || (edgeStars.subvectorsNumber() != edgeList.size()))
    return -2;
  if(cellEdges.empty())
    return -3;
#endif

  Timer t;

  const SimplexId edgeNumber = edgeStars.subvectorsNumber();
  std::vector<SimplexId> offsets(edgeNumber + 1);
  // one edge per star
  std::vector<SimplexId> links(edgeStars.dataSize());

  printMsg(
    "Building edge links", 0, 0, threadNumber_, debug::LineMode::REPLACE);

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(threadNumber_)
#endif // TTK_ENABLE_OPENMP
  for(SimplexId i = 0; i < edgeNumber; i++) {
    // copy the edgeStars offsets array
    offsets[i] = edgeStars.offset(i);

    // current edge vertices
    const auto &e = edgeList[i];
    for(SimplexId j = 0; j < edgeStars.size(i); j++) {
      const auto c = edgeStars.get(i, j);
      for(size_t k = 0; k < cellEdges[c].size(); k++) {
        // cell edge id
        const auto ceid = cellEdges[c][k];
        // cell edge vertices
        const auto &ce = edgeList[ceid];

        if(ce[0] != e[0] && ce[0] != e[1] && ce[1] != e[0] && ce[1] != e[1]) {
          // ce and e have no vertex in common
          links[offsets[i] + j] = ceid;
          break;
        }
      }
    }
  }

  // don't forget the last offset
  offsets[edgeNumber] = edgeStars.offset(edgeNumber);

  edgeLinks.setData(std::move(links), std::move(offsets));

  printMsg("Built " + to_string(edgeNumber) + " edge links", 1,
           t.getElapsedTime(), threadNumber_);

  return 0;
}

template <std::size_t n>
int OneSkeleton::buildEdgeList(
  const SimplexId &vertexNumber,
  const CellArray &cellArray,
  vector<std::array<SimplexId, 2>> *edgeList,
  FlatJaggedArray *edgeStars,
  std::vector<std::array<SimplexId, n>> *cellEdgeList) const {

  Timer t;

  // check parameters consistency (we need n to be consistent with the
  // dimensionality of the mesh)
  const auto dim = cellArray.getCellVertexNumber(0) - 1;
  if(n != dim * (dim + 1) / 2 && cellEdgeList != nullptr) {
    this->printErr("Wrong template parameter (" + std::to_string(n)
                   + "edges per cell in dim " + std::to_string(dim)
                   + "), unable to compute cellEdgeList");
    return -1;
  }

  printMsg("Building edges", 0, 0, 1, ttk::debug::LineMode::REPLACE);

  const SimplexId cellNumber = cellArray.getNbCells();

  // we will nee cellEdgeList to compute edgeStars
  std::vector<std::array<SimplexId, n>> defaultCellEdgeList{};
  if(edgeStars != nullptr && cellEdgeList == nullptr) {
    cellEdgeList = &defaultCellEdgeList;
  }

  if(cellEdgeList != nullptr) {
    cellEdgeList->resize(cellNumber);
  }

  struct EdgeData {
    // the id of the edge higher vertex
    SimplexId highVert{};
    // the edge id
    SimplexId id{};
    EdgeData(SimplexId hv, SimplexId i) : highVert{hv}, id{i} {
    }
  };

  using boost::container::small_vector;
  // for each vertex, a vector of EdgeData
  std::vector<small_vector<EdgeData, 8>> edgeTable(vertexNumber);

  const int timeBuckets = std::min<ttk::SimplexId>(10, cellNumber);
  SimplexId edgeCount{};

  for(SimplexId cid = 0; cid < cellNumber; cid++) {

    const SimplexId nbVertsInCell = cellArray.getCellVertexNumber(cid);
    // id of edge in cell
    SimplexId ecid{};

    // tet case: {0-1}, {0-2}, {0-3}, {1-2}, {1-3}, {2-3}
    for(SimplexId j = 0; j <= nbVertsInCell - 2; j++) {
      for(SimplexId k = j + 1; k <= nbVertsInCell - 1; k++) {
        // edge processing
        SimplexId v0 = cellArray.getCellVertex(cid, j);
        SimplexId v1 = cellArray.getCellVertex(cid, k);
        if(v0 > v1) {
          std::swap(v0, v1);
        }
        auto &vec = edgeTable[v0];
        const auto pos
          = std::find_if(vec.begin(), vec.end(),
                         [&](const EdgeData &a) { return a.highVert == v1; });
        if(pos == vec.end()) {
          // not found in edgeTable: new edge
          vec.emplace_back(EdgeData{v1, edgeCount});
          if(cellEdgeList != nullptr) {
            (*cellEdgeList)[cid][ecid] = edgeCount;
          }
          edgeCount++;
        } else {
          // found an existing edge
          if(cellEdgeList != nullptr) {
            (*cellEdgeList)[cid][ecid] = pos->id;
          }
        }
        ecid++;
      }
    }
    if(debugLevel_ >= (int)(debug::Priority::INFO)) {
      if(!(cid % ((cellNumber) / timeBuckets)))
        printMsg("Building edges", (cid / (float)cellNumber),
                 t.getElapsedTime(), 1, debug::LineMode::REPLACE);
    }
  }

  // allocate & fill edgeList in parallel

  if(edgeList != nullptr) {
    edgeList->resize(edgeCount);
  }

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif // TTK_ENABLE_OPENMP
  for(SimplexId i = 0; i < vertexNumber; ++i) {
    const auto &etable = edgeTable[i];
    for(const auto &data : etable) {
      if(edgeList != nullptr) {
        (*edgeList)[data.id] = {i, data.highVert};
      }
    }
  }

  // return cellEdgeList to get edgeStars

  if(cellEdgeList != nullptr && edgeStars != nullptr) {
    std::vector<SimplexId> offsets(edgeCount + 1);
    // number of cells processed per edge
    std::vector<SimplexId> starIds(edgeCount);

    // store number of cells per edge
    for(const auto &ce : *cellEdgeList) {
      for(const auto eid : ce) {
        offsets[eid + 1]++;
      }
    }

    // compute partial sum of number of cells per edge
    for(size_t i = 1; i < offsets.size(); ++i) {
      offsets[i] += offsets[i - 1];
    }

    // allocate flat edge stars vector
    std::vector<SimplexId> edgeSt(offsets.back());

    // fill flat neighbors vector using offsets and neighbors count vectors
    for(size_t i = 0; i < cellEdgeList->size(); ++i) {
      const auto &ce{(*cellEdgeList)[i]};
      for(const auto eid : ce) {
        edgeSt[offsets[eid] + starIds[eid]] = i;
        starIds[eid]++;
      }
    }

    // fill FlatJaggedArray struct
    edgeStars->setData(std::move(edgeSt), std::move(offsets));
  }

  printMsg(
    "Built " + to_string(edgeCount) + " edges", 1, t.getElapsedTime(), 1);

  // ethaneDiolMedium.vtu, 70Mtets, hal9000 (12coresHT)
  // 1 thread: 10.4979 s
  // 24 threads: 12.3994 s [not efficient in parallel]

  return 0;
}

// explicit template instantiation for 2D cells (triangles)
template int OneSkeleton::buildEdgeList<3>(
  const SimplexId &vertexNumber,
  const CellArray &cellArray,
  vector<std::array<SimplexId, 2>> *edgeList,
  FlatJaggedArray *edgeStars,
  std::vector<std::array<SimplexId, 3>> *cellEdgeList) const;

// explicit template instantiation for 3D cells (tetrathedron)
template int OneSkeleton::buildEdgeList<6>(
  const SimplexId &vertexNumber,
  const CellArray &cellArray,
  vector<std::array<SimplexId, 2>> *edgeList,
  FlatJaggedArray *edgeStars,
  std::vector<std::array<SimplexId, 6>> *cellEdgeList) const;

// int OneSkeleton::buildEdgeLists(
//   const vector<vector<LongSimplexId>> &cellArrays,
//   vector<vector<pair<SimplexId, SimplexId>>> &edgeLists) const {
//   Timer t;
//   printMsg(
//     "Building edge lists", 0, 0, threadNumber_, debug::LineMode::REPLACE);
//   edgeLists.resize(cellArrays.size());
// #ifdef TTK_ENABLE_OPENMP
// #pragma omp parallel for num_threads(threadNumber_)
// #endif
//   for(SimplexId i = 0; i < (SimplexId)cellArrays.size(); i++) {
//     buildEdgeSubList(cellArrays[i].size() / (cellArrays[i][0] + 1),
//                      cellArrays[i].data(), edgeLists[i]);
//   }
//   printMsg("Built " + to_string(edgeLists.size()) + " edge lists", 1,
//            t.getElapsedTime(), threadNumber_);
//   if(debugLevel_ >= (int)(debug::Priority::DETAIL)) {
//     for(SimplexId i = 0; i < (SimplexId)edgeLists.size(); i++) {
//       {
//         stringstream stringStream;
//         stringStream << "Surface #" << i << " (" << edgeLists[i].size()
//                      << " edges):";
//         printMsg(stringStream.str(), debug::Priority::DETAIL);
//       }
//       for(SimplexId j = 0; j < (SimplexId)edgeLists[i].size(); j++) {
//         stringstream stringStream;
//         stringStream << "- [" << edgeLists[i][j].first << " - "
//                      << edgeLists[i][j].second << "]";
//         printMsg(stringStream.str(), debug::Priority::DETAIL);
//       }
//     }
//   }
//   // computing the edge list of each vertex link:
//   // 24 threads (12 cores): 1.69s.
//   // 1 thread: 7.2 (> x4)
//   return 0;
// }

int OneSkeleton::buildEdgeStars(const SimplexId &vertexNumber,
                                const CellArray &cellArray,
                                FlatJaggedArray &starList,
                                vector<std::array<SimplexId, 2>> *edgeList,
                                FlatJaggedArray *vertexStars) const {

  auto localEdgeList = edgeList;
  vector<std::array<SimplexId, 2>> defaultEdgeList{};
  if(!localEdgeList) {
    localEdgeList = &defaultEdgeList;
  }

  if(!localEdgeList->size()) {
    buildEdgeList(vertexNumber, cellArray, localEdgeList);
  }

  using boost::container::small_vector;
  // for each edge, a vector of stars/cells
  std::vector<small_vector<SimplexId, 8>> stars(localEdgeList->size());

  auto localVertexStars = vertexStars;
  FlatJaggedArray defaultVertexStars{};
  if(!localVertexStars) {
    localVertexStars = &defaultVertexStars;
  }
  if((SimplexId)localVertexStars->subvectorsNumber() != vertexNumber) {
    ZeroSkeleton zeroSkeleton;
    zeroSkeleton.setThreadNumber(threadNumber_);
    zeroSkeleton.setDebugLevel(debugLevel_);
    zeroSkeleton.buildVertexStars(vertexNumber, cellArray, *localVertexStars);
  }

  Timer t;

  printMsg(
    "Building edge stars", 0, 0, threadNumber_, debug::LineMode::REPLACE);

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(threadNumber_)
#endif // TTK_ENABLE_OPENMP
  for(size_t i = 0; i < localEdgeList->size(); i++) {
    const auto &e = (*localEdgeList)[i];
    const auto beg0 = localVertexStars->get_ptr(e[0], 0);
    const auto end0 = beg0 + localVertexStars->size(e[0]);
    const auto beg1 = localVertexStars->get_ptr(e[1], 0);
    const auto end1 = beg1 + localVertexStars->size(e[1]);
    // merge the two vertex stars
    std::set_intersection(beg0, end0, beg1, end1, std::back_inserter(stars[i]));
  }

  // convert to a FlatJaggedArray
  starList.fillFrom(stars);

  printMsg("Built " + to_string(stars.size()) + " edge stars", 1,
           t.getElapsedTime(), threadNumber_);

  // ethaneDiolMedium.vtu, 70Mtets, hal9000 (12coresHT)
  // with edge list and vertex stars
  // 1 thread: 13 s
  // 24 threads: 48 s (~ x4)

  return 0;
}

int OneSkeleton::buildEdgeSubList(
  const CellArray &cellArray,
  vector<std::array<SimplexId, 2>> &edgeList) const {

  // NOTE: here we're dealing with a subportion of the mesh.
  // hence our lookup strategy (based on the number of total vertices) is no
  // longer efficient. let's use a standard map instead
  // NOTE: when dealing with the entire mesh (case above), our vertex based
  // look up strategy is about 7 times faster than the standard map.
  // For mesh portions, the standard map is orders of magnitude faster

  map<std::array<SimplexId, 2>, bool> edgeMap;
  edgeList.clear();

  const SimplexId cellNumber = cellArray.getNbCells();
  for(SimplexId cid = 0; cid < cellNumber; cid++) {
    const SimplexId nbVertCell = cellArray.getCellVertexNumber(cid);

    array<SimplexId, 2> edgeIds;
    // tet case
    // 0 - 1
    // 0 - 2
    // 0 - 3
    // 1 - 2
    // 1 - 3
    // 2 - 3
    for(SimplexId j = 0; j <= nbVertCell - 2; j++) {
      for(SimplexId k = j + 1; k <= nbVertCell - 1; k++) {

        edgeIds[0] = cellArray.getCellVertex(cid, j);
        edgeIds[1] = cellArray.getCellVertex(cid, k);

        if(edgeIds[0] > edgeIds[1]) {
          std::swap(edgeIds[0], edgeIds[1]);
        }

        auto it = edgeMap.find(edgeIds);

        if(it == edgeMap.end()) {
          // not found, let's add this edge
          edgeList.push_back(edgeIds);
          edgeMap[edgeIds] = true;
        }
      }
    }
  }

  return 0;
}
