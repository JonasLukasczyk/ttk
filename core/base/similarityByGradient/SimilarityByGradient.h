/// \ingroup base
/// \class ttk::SimilarityByGradient
/// \author Wito Engelke <wito.engelke@googlemail.com>
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date June 2020.
///
/// TODO
///
/// \b Related \b publication: \n
/// 'SimilarityByGradient'
/// Jonas Lukasczyk and Julien Tierny.
/// TTK Publications.
/// 2020.
///

#pragma once

// ttk common includes
#include <Debug.h>
#include <Triangulation.h>

namespace ttk {
  class SimilarityByGradient : virtual public Debug {

  public:
    SimilarityByGradient() {
      this->setDebugMsgPrefix("SimilarityByGradient");
    };
    ~SimilarityByGradient(){};

    int preconditionTriangulation(
      ttk::AbstractTriangulation *triangulation) const {
      return triangulation->preconditionVertexNeighbors();
    };

    template <typename IT, typename TT, typename CF>
    int computeGradientPath(IT &matchIdx,
                            const IT seedId,
                            const IT *order,
                            const TT *triangulation,
                            const CF comperatorFunction) const {
      IT nextIdx = seedId;
      IT pivotNeighborIdx = -1;
      IT pivotNeighborOrder = -1;

      // steepest descent
      while(true) {

        // assume first neighbor is the smallest neighbor
        triangulation->getVertexNeighbor(nextIdx, 0, pivotNeighborIdx);
        pivotNeighborOrder = order[pivotNeighborIdx];

        // check other neighbors
        const IT nNeighbors = triangulation->getVertexNeighborNumber(nextIdx);
        for(IT i = 1; i < nNeighbors; i++) {
          IT u;
          triangulation->getVertexNeighbor(nextIdx, i, u);
          const auto &uOrder = order[u];
          if(comperatorFunction(uOrder, pivotNeighborOrder)) {
            pivotNeighborIdx = u;
            pivotNeighborOrder = uOrder;
          }
        }

        // if we reached a minimum terminate descent
        if(comperatorFunction(order[nextIdx], pivotNeighborOrder)) {
          matchIdx = nextIdx;
          return 1;
        }

        // otherwise continue descent from smallest neighbor
        nextIdx = pivotNeighborIdx;
      }
    };

    template <typename IT, typename TT, typename CF, typename IF>
    int computeSimilarityMatrix(int *matrix,

                                const IT *order,
                                const TT *triangulation,

                                const IT *criticalPointVertexIds0,
                                const IT *criticalPointVertexIds1,
                                const IT nFeatures0,
                                const IT nFeatures1,
                                const CF comperatorFunction,
                                const IF indexFunction) const {
      ttk::Timer timer;
      this->printMsg("Computing Similarity Matrix", 0, 0, this->threadNumber_,
                     debug::LineMode::REPLACE);

      for(int i = 0, j = nFeatures0 * nFeatures1; i < j; i++)
        matrix[i] = 0;

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif // TTK_ENABLE_OPENMP
      for(IT i = 0; i < nFeatures0; i++) {
        const IT &seedIdx = criticalPointVertexIds0[i];
        IT matchIdx = -1;
        this->computeGradientPath<IT, TT, CF>(
          matchIdx, seedIdx, order, triangulation, comperatorFunction);

        // add matches to matix. Note: it is possible that a matched extrema is not in the feature list (i.e., not tracked)
        for(IT j = 0; j < nFeatures1; j++) {
          if(criticalPointVertexIds1[j] == matchIdx) {
            matrix[indexFunction(i, j, nFeatures0, nFeatures1)] = 1;
            break;
          }
        }
      }

      this->printMsg("Computing Similarity Matrix", 1, timer.getElapsedTime(),
                     this->threadNumber_);

      return 1;
    };

  }; // SimilarityByGradient class

} // namespace ttk
