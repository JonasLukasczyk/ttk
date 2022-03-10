/// \ingroup base
/// \class ttk::ComparingSimilarityMatrices
/// \author Emma Nilsson <emma.nilsson@liu.se>
/// \date 2022-03-07.
///
/// \b Related \b publication: \n

#pragma once

// ttk common includes
#include <Debug.h>
#include <Triangulation.h>

#include <unordered_map>

namespace ttk {

  class ComparingSimilarityMatrices : virtual public Debug {

  public:
    ComparingSimilarityMatrices();

    struct Events {
      // {alg, gt, correct}
      int continuations[3]{0, 0, 0};
      int deaths[3]{0, 0, 0};
      int splits[3]{0, 0, 0};
      int births[3]{0, 0, 0};
      int merges[3]{0, 0, 0};
    };

    template <typename DT0, typename DT1>
    int compareEvents(Events &curEvents,
                      const DT0 *algMatrix,
                      const DT0 *gtMatrix,
                      const DT1 *indexIdMapAlg0,
                      const DT1 *indexIdMapAlg1,
                      const DT1 *indexIdMapGT0,
                      const DT1 *indexIdMapGT1,
                      const int dims[2]) {

      std::map<int, std::vector<int>> rowFillAlg;
      std::map<int, std::vector<int>> columnFillAlg;
      std::map<int, std::vector<int>> rowFillGT;
      std::map<int, std::vector<int>> columnFillGT;

      for(int r = 0; r < dims[1]; r++) {
        rowFillAlg[indexIdMapAlg1[r]] = std::vector<int>(0);
        rowFillGT[indexIdMapGT1[r]] = std::vector<int>(0);
      }
      for(int c = 0; c < dims[0]; c++) {
        columnFillAlg[indexIdMapAlg0[c]] = std::vector<int>(0);
        columnFillGT[indexIdMapGT0[c]] = std::vector<int>(0);
      }

      //
      for(int r = 0; r < dims[1]; r++) {
        for(int c = 0; c < dims[0]; c++) {

          int valAlg = algMatrix[r * dims[0] + c];
          int valGT = gtMatrix[r * dims[0] + c];
          if(valAlg == 1) {
            rowFillAlg[indexIdMapAlg1[r]].push_back(indexIdMapAlg0[c]);
            columnFillAlg[indexIdMapAlg0[c]].push_back(indexIdMapAlg1[r]);
          }
          if(valGT == 1) {
            rowFillGT[indexIdMapGT1[r]].push_back(indexIdMapGT0[c]);
            columnFillGT[indexIdMapGT0[c]].push_back(indexIdMapGT1[r]);
          }
        }
      }

      // Count and compare events in columns: deaths, continuations, splits
      for(int c = 0; c < dims[0]; c++) {
        int nOnesAlg = columnFillAlg[indexIdMapAlg0[c]].size();
        int nOnesGT = columnFillGT[indexIdMapGT0[c]].size();

        // Count events possible in columns
        if(nOnesAlg == 0)
          ++curEvents.deaths[0];
        else if(nOnesAlg == 1)
          ++curEvents.continuations[0];
        else if(nOnesAlg >= 2)
          ++curEvents.splits[0];

        if(nOnesGT == 0)
          ++curEvents.deaths[1];
        else if(nOnesGT == 1)
          ++curEvents.continuations[1];
        else if(nOnesGT >= 2)
          ++curEvents.splits[1];

        // Check correctness
        if(nOnesAlg == nOnesGT && nOnesAlg == 1
           && columnFillAlg[indexIdMapAlg0[c]][0]
                == columnFillGT[indexIdMapGT0[c]][0])
          ++curEvents.continuations[2];

        if(nOnesAlg == nOnesGT && nOnesAlg == 0) {
          ++curEvents.deaths[2];
        }

        // To check correctness, check that the feature ids involved in the
        // split matches
        if(nOnesAlg == nOnesGT && nOnesAlg >= 2) {
          bool match = true;
          for(int i = 0; i < nOnesAlg; i++) {
            if(columnFillAlg[indexIdMapAlg0[c]][i]
               != columnFillGT[indexIdMapGT0[c]][i])
              match = false;
          }
          if(match)
            ++curEvents.splits[2];
        }
      }

      // In rows we can check for births and merges
      // {alg, gt, correct}
      for(int r = 0; r < dims[1]; r++) {
        int nOnesAlg = rowFillAlg[indexIdMapAlg1[r]].size();
        int nOnesGT = rowFillGT[indexIdMapGT1[r]].size();

        // Count events possible in columns
        if(nOnesAlg == 0)
          ++curEvents.births[0];
        else if(nOnesAlg >= 2) {
          ++curEvents.merges[0];
          // If features merge they are no longer continuations
          curEvents.continuations[0] -= nOnesAlg;
        }

        if(nOnesGT == 0)
          ++curEvents.births[1];
        else if(nOnesGT >= 2) {
          ++curEvents.merges[1];
          // If features merge they are no longer continuations
          curEvents.continuations[1] -= nOnesGT;
        }

        // Check correctness
        if(nOnesAlg == nOnesGT && nOnesAlg == 0) {
          ++curEvents.births[2];
        }

        // To check correctness, check that the feature ids involved in the
        // merge matches
        if(nOnesAlg == nOnesGT && nOnesAlg >= 2) {
          bool match = true;
          for(int i = 0; i < nOnesAlg; i++) {
            if(rowFillAlg[indexIdMapAlg1[r]][i]
               != rowFillGT[indexIdMapGT1[r]][i])
              match = false;
          }
          if(match)
            ++curEvents.merges[2];
        }
      }

      this->printMsg("-----------------EVENTS-----------------");
      std::string cs = "Continuations: ";
      std::string bs = "Births: ";
      std::string ds = "Deaths: ";
      std::string ss = "Splits: ";
      std::string ms = "Merges: ";
      for(int i = 0; i < 3; i++) {
        cs += std::to_string(curEvents.continuations[i]) + " ";
        bs += std::to_string(curEvents.births[i]) + " ";
        ds += std::to_string(curEvents.deaths[i]) + " ";
        ss += std::to_string(curEvents.splits[i]) + " ";
        ms += std::to_string(curEvents.merges[i]) + " ";
      }
      this->printMsg(cs);
      this->printMsg(bs);
      this->printMsg(ds);
      this->printMsg(ss);
      this->printMsg(ms);

      return 1;
    }

  }; // ComparingSimilarityMatrices class

} // namespace ttk
