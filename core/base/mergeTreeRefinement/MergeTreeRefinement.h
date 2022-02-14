#pragma once

#include <Triangulation.h>

namespace ttk {

  class MergeTreeRefinement : virtual public Debug {

  public:
    MergeTreeRefinement() {
      this->setDebugMsgPrefix("MergeTreeRefinement");
    };
    ~MergeTreeRefinement(){};

    template <typename DT, typename IT>
    int computeRefinedSegmentation(IT *sNodeIds,
                                   IT *mtSizes,

                                   const bool &isSplitTree,
                                   const int *sBranchIds,
                                   const DT *sScalars,
                                   const DT *mtScalars,
                                   const IT *mtNextIds,
                                   const size_t &n_mtPoints,
                                   const size_t &n_sPoints) {
      ttk::Timer timer;
      const std::string msg = "Computing Refined Segmentation";
      this->printMsg(
        msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

      const DT flip = isSplitTree ? 1.0 : -1.0;

      std::vector<std::vector<int>> sizeDataPerThread(this->threadNumber_);
      for(int t = 0; t < this->threadNumber_; t++)
        sizeDataPerThread[t].resize(n_mtPoints, 0);

#pragma omp parallel num_threads(this->threadNumber_)
      {
        int *sizeDataThread = sizeDataPerThread[omp_get_thread_num()].data();

#pragma omp for
        for(size_t i = 0; i < n_sPoints; i++) {

          const int &branchId = sBranchIds[i];
          const DT scalar = sScalars[i] * flip;

          int curr = branchId;
          int next = mtNextIds[curr];

          // search mt edge which contains scalar value
          while(next >= 0 && flip * mtScalars[next] > scalar) {
            // if on last edge
            if(mtNextIds[next] < 0)
              break;

            // go one edge down
            curr = next;
            next = mtNextIds[curr];

            // if entered plateau
            if(mtScalars[curr] == scalar)
              break;
          }

          sNodeIds[i] = curr;

          // increase size of all edges towards root
          while(curr >= 0) {
            sizeDataThread[curr]++;
            curr = mtNextIds[curr];
          }
        }
      }

      // compute vertex id
      // for(size_t v = 0; v < n_sPoints; v++) {
      //   const auto& order = sScalarsOrder[v];
      //   const auto& nodeId = sNodeIds[v];
      //   auto& mtVertexId = mtVertexIds[nodeId];
      //   if(mtVertexId<0 || sScalarsOrder[mtVertexId]<order)
      //     mtVertexId = v;
      // }

#pragma omp parallel for num_threads(this->threadNumber_)
      for(size_t i = 0; i < n_mtPoints; i++) {
        // map point to vertex id
        // auto& vertexId = mtVertexIds[i];
        // if(vertexId>=0)
        //   sTriangulation->getVertexPoint(vertexId,
        //   mtCoords[i*3+0],mtCoords[i*3+1],mtCoords[i*3+2]);

        auto &size = mtSizes[i];
        size = 0;
        for(int t = 0; t < this->threadNumber_; t++)
          size += sizeDataPerThread[t][i];
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1;
    }
  };
} // namespace ttk