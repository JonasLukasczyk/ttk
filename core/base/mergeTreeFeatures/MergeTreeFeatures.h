#pragma once

#include <Debug.h>
#include <Triangulation.h>

namespace ttk {

  class MergeTreeFeatures : virtual public Debug {

  public:

    MergeTreeFeatures(){
      this->setDebugMsgPrefix("MergeTreeFeatures");
    }

    template <typename DT, typename IT>
    int computeNumberOfNodes(
      IT& nNodes,
      const std::vector<double>& levels,
      const DT* scalars,
      const IT* edges,
      const IT nEdges
    ){
      const int nLevels = levels.size();

      for(IT e=0; e<nEdges; e++){
        const IT n0 = edges[e*2];
        const IT n1 = edges[e*2+1];
        DT s0 =scalars[n0];
        DT s1 =scalars[n1];
        if (s0 > s1) std::swap(s0, s1);

        for(int l=0; l<nLevels; l++){
          const DT level = static_cast<DT>(levels[l]);
          if(s0<level && level<=s1)
            nNodes++;
        }
      }

      return 1;
    }

    template <typename DT, typename CT, typename IT>
    int computeNodes(
      int* o_nodeIds,
      DT* nodeScalars,
      float* o_size,
      int* nodeLevels,
      CT* nodeCoords,

      const std::vector<double>& levels,
      const DT* scalars,
      const int* i_size,
      const IT* edges,
      const CT* coords,
      const IT nEdges
    ){
      const int nLevels = levels.size();

      int nodeIdx = 0;
      for(IT e=0; e<nEdges; e++){
        IT n0 = edges[e*2];
        IT n1 = edges[e*2+1];
        if(scalars[n0] > scalars[n1]) std::swap(n0, n1);

        const double s0 = static_cast<double>(scalars[n0]);
        const double s1 = static_cast<double>(scalars[n1]);

        for(int l=0; l<nLevels; l++){
          const DT level = static_cast<DT>(levels[l]);
          if(s0<level && level<=s1){

            nodeScalars[nodeIdx] = level;
            nodeLevels[nodeIdx] = l;
            o_nodeIds[nodeIdx] = static_cast<int>(n1);

            const double lambda = (level-s0)/(s1-s0);

            nodeCoords[nodeIdx*3+0] = coords[n0*3+0]*(1-lambda) + coords[n1*3+0]*lambda;
            nodeCoords[nodeIdx*3+1] = coords[n0*3+1]*(1-lambda) + coords[n1*3+1]*lambda;
            nodeCoords[nodeIdx*3+2] = coords[n0*3+2]*(1-lambda) + coords[n1*3+2]*lambda;

            o_size[nodeIdx] = static_cast<float>(i_size[n1]);
              // static_cast<double>(i_size[n0])*(1-lambda) + static_cast<double>(i_size[n1])*lambda
            // );

            nodeIdx++;
          }
        }
      }

      return 1;
    }

    template <typename DT>
    int computeParents(
      int* o_nodeIds,
      int* o_parentIds,

      const std::vector<double>& levels,
      const int* o_nodeLevels,
      const DT* scalars,
      const int* i_nodeIds,
      const int* i_nextIds,
      const int nNodes
    ){
      for(int i=0; i<nNodes; i++){
        int nodeIdx = o_nodeIds[i];
        o_nodeIds[i] = i_nodeIds[nodeIdx];

        const int l0Idx = o_nodeLevels[i];
        if(l0Idx==0){
          o_parentIds[i] = -1;
          continue;
        }

        const DT l = static_cast<DT>(levels[l0Idx-1]);

        // traverse to root until next level
        int n0 = nodeIdx;
        int n1 = i_nextIds[n0];

        while(n1>=0 && scalars[n1]>=l){
          n0=n1;
          n1=i_nextIds[n0];
        }
        o_parentIds[i] = i_nodeIds[n0];
      }

      return 1;
    }

  }; // MergeTreeFeatures class

} // namespace ttk
