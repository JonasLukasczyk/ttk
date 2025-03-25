#pragma once

#include <Debug.h>

namespace ttk {

  class SimilarityByMorseSmaleSegmentation : virtual public Debug {

  public:
    SimilarityByMorseSmaleSegmentation() {
      this->setDebugMsgPrefix("SimilarityByMorseSmaleSegmentation");
    };
    ~SimilarityByMorseSmaleSegmentation(){};

    template <typename DT, auto IndexFunc>
    int performLookup(
      int* matrix,
      const int nColumns,
      const unsigned char* types,
      const unsigned char type,
      const int* vertexIds,
      const int nPoints,
      const DT* segmentation,
      const std::unordered_map<int,int>& idIndexMap0,
      const std::unordered_map<int,int>& idIndexMap1
    ) const {

      ttk::Timer timer;

      const std::string msg = "Forward Lookup (" + std::to_string(type) + ")";
      this->printMsg(
        msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

      for(int i = 0; i < nPoints; i++) {
        if(types[i]!=type) continue;

        auto vId0 = vertexIds[i];
        auto idx0 = idIndexMap0.find(vId0);
        if(idx0==idIndexMap0.end()) continue;

        auto vId1 = segmentation[vId0];
        auto idx1 = idIndexMap1.find(vId1);
        if(idx1==idIndexMap1.end()) continue;

        matrix[IndexFunc(idx0->second,idx1->second,nColumns)] = 1;
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1;
    }

  };
} // namespace ttk
