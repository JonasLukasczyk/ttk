#pragma once

// ttk common includes
#include <Debug.h>
#include <Triangulation.h>

#include <functional>

#include <algorithm>


namespace ttk {
  class FeatureCorrespondences : virtual public Debug {
  public:
    struct Explanation {
      std::vector<int> vecUNodes; //nodes in t-1
      std::vector<int> vecVNodes; //nodes in t
      float benefit;

      bool operator<(const Explanation &other) const {
        return benefit < other.benefit;
      }
    };

    struct AdditionalEdge {
      int iPossNode;
      float fPossBenefit;
      float fVol;

      bool operator<(const AdditionalEdge &other) const {
        return fPossBenefit < other.fPossBenefit;
      }
    };

    struct CliqueSortValue {
      CliqueSortValue(int cliqueSize, float diffBenefit,
        float benefit, int idx) :
        cliqueSize(cliqueSize),
        diffBenefit(diffBenefit),
        benefit(benefit),
        idx(idx) {
          confl = 1;
      }

      int cliqueSize;
	    float diffBenefit;
	    float benefit;
	    int idx;
	    int confl;

      bool operator< (const CliqueSortValue &other) const {
        return confl < other.confl ||
        (confl == other.confl && diffBenefit > other.diffBenefit) ||
        (confl == other.confl && diffBenefit == other.diffBenefit && benefit > other.benefit) ||
        (confl == other.confl && diffBenefit == other.diffBenefit && benefit == other.benefit && cliqueSize < other.cliqueSize);
      }
    };

    struct ConflictEdge {
      ConflictEdge(int left, int right) :
	      left(left),
	      right(right){}

      int left;
      int right;

      bool operator<(const ConflictEdge &other) const {
        return left < other.left || (left == other.left && right < other.right);
      }
    };

    FeatureCorrespondences() {
      this->setDebugMsgPrefix("FeatureCorrespondences");
    }

    template <typename DT, typename IDX>
    int sortAndReduceCorrespondencesPerFeature_(DT *oMatrix,
                                                const DT *iMatrix,
                                                const int iN,
                                                const int jN,
                                                const int nCandidates,
                                                const bool ascending,
                                                const IDX matrixIdx) const {

      using VT = std::pair<DT, int>;
      std::vector<VT> sortedValues;

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_) private(sortedValues)
#endif
      for(int i = 0; i < iN; i++) {
        // initialize vector
        sortedValues.resize(jN);

        // get values
        for(int j = 0; j < jN; j++) {
          const int idx = matrixIdx(i, j);
          auto &p = sortedValues[j];
          p.first = iMatrix[idx];
          p.second = idx;
        }

        // serial sort
        if(ascending)
          std::sort(sortedValues.begin(), sortedValues.end(), std::less<VT>());
        else
          std::sort(
            sortedValues.begin(), sortedValues.end(), std::greater<VT>());

        // copy maximum candidates
        const int cN = std::min(nCandidates, jN);
        for(int c = 0; c < cN; c++) {
          const auto &p = sortedValues[c];
          oMatrix[p.second] = p.first;
        }
      }

      return 1;
    };

    template <typename DT>
    int sortAndReduceCorrespondencesPerFeature(DT *oMatrix,
                                               const DT *iMatrix,
                                               const int dimX,
                                               const int dimY,
                                               const int nCandidates,
                                               const bool ascending) const {
      ttk::Timer timer;

      const std::string msg = "Computing " + std::to_string(nCandidates)
                              + (ascending ? " smallest" : " largest")
                              + " correspondences per feature.";
      this->printMsg(msg, 0, 0, this->threadNumber_, debug::LineMode::REPLACE);

      const int n = dimX * dimY;

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif
      for(int i = 0; i < n; i++) {
        oMatrix[i] = 0;
      }
      this->printMsg(msg, 0.2, timer.getElapsedTime(), this->threadNumber_,
                     debug::LineMode::REPLACE);

      // forward optimization
      this->sortAndReduceCorrespondencesPerFeature_(
        oMatrix, iMatrix, dimX, dimY, nCandidates, ascending,
        [=](const int &i, const int &j) { return j * dimX + i; });
      this->printMsg(msg, 0.6, timer.getElapsedTime(), this->threadNumber_,
                     debug::LineMode::REPLACE);

      // backward optimization
      this->sortAndReduceCorrespondencesPerFeature_(
        oMatrix, iMatrix, dimY, dimX, nCandidates, ascending,
        [=](const int &i, const int &j) { return i * dimX + j; });
      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1; // return success
    }

    template <typename DT, typename F>
    int mapEachElement(DT *oMatrix,
                       const DT *iMatrix,
                       const int dimX,
                       const int dimY,
                       const F maskFunction) const {
      ttk::Timer timer;

      const std::string msg = "Computing Mask";
      this->printMsg(msg, 0, 0, this->threadNumber_, debug::LineMode::REPLACE);

      const int n = dimX * dimY;

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif
      for(int i = 0; i < n; i++) {
        oMatrix[i] = maskFunction(iMatrix[i]);
      }
      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1; // return success
    };

    template <typename DT>
    int twoPassOptimization(
      float *oMatrix,
      const DT *iMatrix,
      const int* mIds0,
      const int* mIds1,
      const int nRows,
      const int nCols,

      const float* fSize0,
      const float* fSize1,
      const int* fIds0,
      const int* fIds1,
      const int nFeatures0,
      const int nFeatures1
    ) const {
      ttk::Timer timer;

      const std::string msg = "Two Pass Optimization";
      this->printMsg(msg, 0, 0, this->threadNumber_, debug::LineMode::REPLACE);

      //init output matrix (== 0)
      for(size_t r=0; r<nRows; r++){
        for(size_t c=0; c<nCols; c++){
          oMatrix[c*nRows+r] = 0.0;
        }
      }

      //setup maps for feature sizes in rows and cols
      std::unordered_map<int, float> fSizeMap0, fSizeMap1;
      for(size_t i=0; i<nFeatures0; i++){
        fSizeMap0[fIds0[i]] = fSize0[i];
      }
      for(size_t j=0; j<nFeatures1; j++){
        fSizeMap1[fIds1[j]] = fSize1[j];
      }

      //setup normalized Overlap Matrix for matching
      std::vector<float> normMatrix;
      normMatrix.resize(nRows*nCols);
      for(size_t r=0; r<nRows; r++){
        for(size_t c=0; c<nCols; c++){
          //oMatrix[r*nCols+c] = iMatrix[r*nCols+c]/std::max(fSizeMap0[mIds0[r]],fSizeMap1[mIds1[c]]); initial test version -> switched indices
          normMatrix[c*nRows+r] = iMatrix[c*nRows+r]/std::max(fSizeMap0[mIds0[r]],fSizeMap1[mIds1[c]]);
        }
      }

      //STEP 1: MATCHING

      //sort rows by maxValue (normOverlap) in each row
      using maxP = std::pair<float, int>;
      std::vector<maxP> sortedMaxValues;
      std::vector<int> maxColIds;
      sortedMaxValues.resize(nRows);
      maxColIds.resize(nRows);

      float maxVal;
      int maxIdx;
      for(size_t r=0; r<nRows; r++){
        maxIdx = -1;
        maxVal = 0.0;
        for(size_t c=0; c<nCols; c++){
          if(normMatrix[c*nRows+r] > maxVal){
            maxVal = normMatrix[c*nRows+r];
            maxIdx = c;
          }
        }
        auto &p = sortedMaxValues[r];
        p.first = maxVal;
        p.second = r;
        maxColIds[r] = maxIdx;
      }
      std::sort(sortedMaxValues.begin(), sortedMaxValues.end(), std::greater<maxP>());

      //matching vectors Phi and Row
      std::vector<int> vecPhi, vecRow;
      vecPhi.resize(nRows,-1);
      vecRow.resize(nCols,-1);

      for(size_t i=0; i<nRows; i++){
        const auto &p = sortedMaxValues[i];
        //assign maxValue in row if column is not assigned yet
        if(p.first>0 && vecRow[maxColIds[p.second]]==-1){
          vecPhi[p.second] = maxColIds[p.second];
          vecRow[maxColIds[p.second]] = p.second;

          oMatrix[maxColIds[p.second]*nRows+p.second] = 1.0;
        }
      }

      //STEP 2: INDEPENDENT SET

      //setup structures for ind set
      int currFeatIdx, nextFeatIdx; //row/col for matched features
      int currIdxTmp, nextIdxTmp; //row/col for add. features

      // num and vec of all explanations
      int iNumExps = 0;
      std::vector<Explanation> vecExplanations;
      std::vector<Explanation> vecExplanationsTMP;

      //add. nodes and benefit for poss. solutions
      std::vector<AdditionalEdge> vecPossEdgesU;
      std::vector<AdditionalEdge> vecPossEdgesV;
      float fPossBenefit;

      //orig. matching modes and weights
      int iUIdx, iVIdx;
      float fMatchBenefit, fMatchWeight;
      float currWeight;

      //sum vol. and benefit for feature combination
      float fSumBenefit, fSumVol;

      //cutoff for power set
      int iMaxPoss = 8;

      // idx lists for conflicting explanations
      std::vector<int> vecConflictNodes;
      std::vector<int> vecPossConfl;
      std::vector<int>::iterator itConfl;

      //nodes to setup explanations
      std::vector<int> vecUNodes, vecVNodes;

      //idx for first explanation in current Clique
      int iFirstCliqueIdx;

      //data structure for Ind Set
      std::vector<int> cliqueOffsets;
      std::vector<int> cliques;
      std::vector<int> explanationOffset;
      std::vector<int> explanationNodes;
      std::vector<int> explanationBenefit;

      std::vector<int> conflictOffset;
      std::vector<int> conflicts;

      std::vector<std::vector<int>> vecPossExps0, vecPossExps1;
      vecPossExps0.resize(nRows);
      vecPossExps1.resize(nCols);

      int currCliqueOffset = 0;
      int currExplIdx = 0;
      int currExplOffset = 0;
      int currNumExplInClique;
      int currConflOffs = 0;

      int uLen, vLen = 0;
      int iNumCliq = 0;
      std::vector<CliqueSortValue> cliqueSort;
      std::vector<ConflictEdge> conflictEdges;

      //Generate possible Event Solutions
      for(int i=0; i < nRows; i++){
        iUIdx = i;
        iVIdx = vecPhi[iUIdx];
        if(iVIdx == -1){ //node unmatched
          continue;
        }

        fMatchBenefit = iMatrix[iVIdx*nRows+iUIdx]; //overlap
        fMatchWeight = normMatrix[iVIdx*nRows+iUIdx]; //normalized overlap

        vecPossEdgesU.clear();
        vecPossEdgesV.clear();

        //find poss. connections for u in Vnodes
        for(int j=0; j<nCols; j++){
          fPossBenefit = iMatrix[j*nRows+iUIdx];
          //matched or no overlap -> not a candidate
          if(vecRow[j]!=-1 || fPossBenefit<=0){
            continue;
          }
          vecPossEdgesU.push_back(AdditionalEdge{j, fPossBenefit, fSizeMap1[j]});
        }
        //find poss. connections for v in UNodes
        for(int j=0; j<nRows; j++){
          fPossBenefit = iMatrix[iVIdx*nRows+j];
          //matched or no overlap -> not a candidate
          if(vecPhi[j]!=-1 || fPossBenefit<=0){
            continue;
          }
          vecPossEdgesV.push_back(AdditionalEdge{j, fPossBenefit, fSizeMap0[j]});
        }

        //build explanations if at least one vector with poss edges is non-empty
        if(vecPossEdgesU.size()>0 || vecPossEdgesV.size()>0){
          vecExplanationsTMP.clear();
          vecUNodes.clear();

          //insert matching node in U to vector
          vecUNodes.push_back(iUIdx);
          //save idx of first explanation in this clique
          iFirstCliqueIdx = iNumExps;

          //for all combinations in the power set (splits)
          for(int k=1; k< (1<<vecPossEdgesU.size()); k++){
            //save matching node in V
            vecVNodes.clear();
            vecVNodes.push_back(iVIdx);

            //save curr benefit and vol - here: macthing edge and vol of v
            fSumBenefit = fMatchBenefit;
            fSumVol = fSizeMap1[iVIdx];

            for(int l=0; l<vecPossEdgesU.size(); l++){
              //build combinations
              if((1 << l)&k){
                //insert corresponding node and sum up corr overlap and vol
                vecVNodes.push_back(vecPossEdgesU[l].iPossNode);
                fSumBenefit += vecPossEdgesU[l].fPossBenefit;
                fSumVol+= vecPossEdgesU[l].fVol;
              }
            }
            //calculate normOverlap of combination
            fPossBenefit = fSumBenefit/((float)std::max(fSumVol,fSizeMap0[iUIdx]));
            //discard explanations with benefit lower than matching
            if(fPossBenefit<fMatchWeight && vecVNodes.size()!=1){
              continue;
            }
            //add explanation to temp explanations
            vecExplanationsTMP.push_back(Explanation{vecUNodes, vecVNodes, fPossBenefit});
          }

          //insert matching node in V in vector
          vecVNodes.clear();
          vecVNodes.push_back(iVIdx);

          //for all combinations in power set (merges)
          for(int k=1; k< (1<<vecPossEdgesV.size()); k++){
            //add matching node in U
            vecUNodes.clear();
            vecUNodes.push_back(iUIdx);
            //save curr benefit and vol - here: macthing edge and vol of u
            fSumBenefit = fMatchBenefit;
            fSumVol = fSizeMap0[iUIdx];

            for(int l=0; l<vecPossEdgesV.size(); l++){
              //build combinations
              if((1 << l)&k){
                //insert corr. node and sum up overlap and volume
                vecUNodes.push_back(vecPossEdgesV[l].iPossNode);
                fSumBenefit += vecPossEdgesV[l].fPossBenefit;
                fSumVol += vecPossEdgesV[l].fVol;
              }
            }
            //calculate normOverlap of combination
            fPossBenefit = fSumBenefit/((float)std::max(fSumVol,fSizeMap1[iVIdx]));

            //discard explanations with benefit lower than matching
            if(fPossBenefit < fMatchWeight && vecUNodes.size()!=1){
              continue;
            }
            //add explanation to temp explanations
            vecExplanationsTMP.push_back(Explanation{vecUNodes, vecVNodes, fPossBenefit});
          }

          //sort poss. explanations by benefit and use cutoff (2^max best)
          std::sort(vecExplanationsTMP.begin(), vecExplanationsTMP.end());
          std::reverse(vecExplanationsTMP.begin(),vecExplanationsTMP.end());
          if(vecExplanationsTMP.size() > (1<<iMaxPoss)){
            vecExplanationsTMP.resize((1<<iMaxPoss));
          }

          //if explanations are found: add matching edge as trivial solution and save in ind set data struc
          if(vecExplanationsTMP.size()>0){
            //add matching edge as solution
            vecUNodes.clear();
            vecVNodes.clear();
            vecUNodes.push_back(iUIdx);
            vecVNodes.push_back(iVIdx);
            vecExplanationsTMP.push_back(Explanation{vecUNodes, vecVNodes, fMatchWeight});

            cliqueSort.push_back(CliqueSortValue(vecExplanationsTMP.size(),
                                                vecExplanationsTMP[0].benefit - vecExplanationsTMP[1].benefit,
                                                vecExplanationsTMP[0].benefit,
                                                iNumCliq));

            iNumCliq++;
            //add new value for clique in cliqueOffset
            cliqueOffsets.push_back(currCliqueOffset);
            currNumExplInClique = 0;
            //set value for number of Nodes in a clique; increase w each new node
            cliques.push_back(0);
            //insert all explanations to data struc and check conflicts
            for(int expCount=0; expCount<vecExplanationsTMP.size(); expCount++){
              explanationOffset.push_back(currExplOffset);
              //insert explanation
              vecExplanations.push_back(vecExplanationsTMP[expCount]);
              //save both node sets to find conflicts btw explanations
              vecUNodes = vecExplanationsTMP[expCount].vecUNodes;
              vecVNodes = vecExplanationsTMP[expCount].vecUNodes;
              uLen = vecUNodes.size();
              vLen = vecVNodes.size();

              explanationNodes.push_back(uLen);
              explanationNodes.push_back(vLen);
              for(int nodeU=0; nodeU<uLen; nodeU++){
                explanationNodes.push_back(vecUNodes[nodeU]);
              }
              for(int nodeV=0; nodeV<vLen; nodeV++){
                explanationNodes.push_back(vecVNodes[nodeV]);
              }

              currExplOffset += (2+uLen+vLen);
              explanationBenefit.push_back(vecExplanationsTMP[expCount].benefit);
              cliques.push_back(currExplIdx);
              cliques[currCliqueOffset]++;
              currExplIdx++;
              currNumExplInClique++;

              //clear conflict vector for current explanation
              vecConflictNodes.clear();
              //for all corresponding nodes in U, find other explanations with this node
              for(int iN=0; iN<vecUNodes.size(); iN++){
                vecPossConfl = vecPossExps0[vecUNodes[iN]];
                for(int iM=0; iM<vecPossConfl.size(); iM++){
                  //add only exps from other cliques
                  if(vecPossConfl[iM]<iFirstCliqueIdx){
                    vecConflictNodes.push_back(vecPossConfl[iM]);
                  }
                }
              }
              //for all corresponding nodes in V, find other explanations
              for(int iN=0; iN<vecVNodes.size(); iN++){
                vecPossConfl = vecPossExps1[vecVNodes[iN]];
                for(int iM=0; iM<vecPossConfl.size(); iM++){
                  //add only exps from other cliques
                  if(vecPossConfl[iM]<iFirstCliqueIdx){
                    vecConflictNodes.push_back(vecPossConfl[iM]);
                  }
                }
              }
              //remove doubled entries
              std::sort(vecConflictNodes.begin(), vecConflictNodes.end());
              itConfl = std::unique(vecConflictNodes.begin(), vecConflictNodes.end());
              vecConflictNodes.resize(itConfl - vecConflictNodes.begin());
              for(int n=0; n<vecConflictNodes.size(); n++){
               conflictEdges.push_back(ConflictEdge(vecConflictNodes[n], iNumExps));
               conflictEdges.push_back(ConflictEdge(iNumExps, vecConflictNodes[n]));
              }
              //save current explanation in list of corresponding feature to find conflicts in next iterations
              for(int m=0; m<vecUNodes.size(); m++){
                vecPossExps0[vecUNodes[m]].push_back(iNumExps);
              }
              for(int m=0; m<vecVNodes.size(); m++){
                vecPossExps1[vecVNodes[m]].push_back(iNumExps);
              }
              iNumExps++;

            }
            //increase cliqueOffset
            currCliqueOffset += (currNumExplInClique +1);
          }
        }
      }
      //build Conflict Structure
      std::sort(conflictEdges.begin(),conflictEdges.end());
      int lastLeft = -1;
      currConflOffs = 0;
      for(auto edge : conflictEdges){
        int left = edge.left;
        if(left != lastLeft){
          for(int k=0; k<left-lastLeft; k++){
            conflictOffset.push_back(currConflOffs);
          }
          lastLeft=left;
        }
        conflicts.push_back(edge.right);
        currConflOffs++;
      }
      for(int k=conflictOffset.size(); k<iNumExps; k++){
        conflictOffset.push_back(currConflOffs);
      }

      //find cliques with non conflicting first Node
      int cOff;
      int numCliq = cliqueOffsets.size();
      int numExp = explanationOffset.size();
      int numConf = conflicts.size();
      int nonConfl = 0;

      std::vector<int> bestAssign;
      bestAssign.resize(numCliq);
      int currIdx;
      int start, end;

      for(int c=0; c<numCliq; c++){
        cOff = cliqueOffsets[c];
        currIdx = cliques[cOff+1];
        start = conflictOffset[currIdx];
        end = (currIdx+1 < numExp ? conflictOffset[currIdx+1] : numConf);
        if(start==end){
          bestAssign[nonConfl] = currIdx;
          nonConfl++;
          cliqueSort[c].confl = 0;
        }
      }

      //sort cliques by difference in first and second value (non conflicting in the front)
      std::sort(cliqueSort.begin(), cliqueSort.end());
      if(numCliq - nonConfl >=1){
        //assignment Vector for Conflicting Explanations
        std::vector<int> currAssign;
        //currAssign.resize(numCliq-nonConfl);

        int currCliqueIdx;
        int currNumExpl;
        bool conflict;
        int currAssignIdx;

        for(int level=nonConfl; level<numCliq; level++){
          currCliqueIdx = cliqueSort[level].idx;
          cOff = cliqueOffsets[currCliqueIdx];
          currNumExpl = cliques[cOff];

          for(int c=1; c<=currNumExpl; c++){
            currIdx = cliques[cOff+c];
            conflict = false;

            //check for conclicts with current assignment
            start = conflictOffset[currIdx];
            end = (currIdx+1 < numExp ? conflictOffset[currIdx+1] : numConf);
            if(start == end){
              break;
            }

            currAssignIdx = 0;
            std::vector<int> tmpAssign = currAssign;
            std::sort(tmpAssign.begin(),tmpAssign.end());
            int currLevel = level-nonConfl;

            if(currLevel == 0 ||
              conflicts[end-1] < tmpAssign[currAssignIdx] ||
              conflicts[start] > tmpAssign[currLevel-1]){
                break;
            }
            while(conflict == false && start<end && currAssignIdx<currLevel){
              if(conflicts[start] < tmpAssign[currAssignIdx]){
                start++;
              }
              else if(tmpAssign[currAssignIdx] < conflicts[start]){
                currAssignIdx++;
              }
              else{
                conflict = true;
              }
            }
            if(conflict == false){
              break;
            }
          }
          currAssign.push_back(currIdx);
        }

        for(int c=nonConfl; c<numCliq; c++){
          bestAssign[c] = currAssign[c-nonConfl];
        }

      }
      //copy result to output
      for(int a=0; a<numCliq; a++){
        for(int i=0; i<vecExplanations[bestAssign[a]].vecUNodes.size(); i++){
          for(int j=0; j<vecExplanations[bestAssign[a]].vecVNodes.size(); j++){
            //vecExplanations[bestAssign[a]].vecUNodes[i]
            //vecExplanations[bestAssign[a]].vecVNodes[j]
            oMatrix[vecExplanations[bestAssign[a]].vecVNodes[j]*nRows+vecExplanations[bestAssign[a]].vecUNodes[i]] = 1.0;
          }

        }
      }
      //test output
      //for(size_t r=0; r<nRows; r++){
        //for(size_t c=0; c<nCols; c++){
          //oMatrix[c*nRows+r] = normMatrix[c*nRows+r];
        //}
      //}


      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);
      return 1;
    };
  };
} // namespace ttk
