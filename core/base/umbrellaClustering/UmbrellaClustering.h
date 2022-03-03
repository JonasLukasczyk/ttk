/// TODO 1: Provide your information
///
/// \ingroup base
/// \class ttk::UmbrellaClustering
/// \author Your Name Here <Your Email Address Here>
/// \date The Date Here.
///
/// This module defines the %UmbrellaClustering class that computes for
/// each vertex of a triangulation the average scalar value of itself and its
/// direct neighbors.
///
/// \b Related \b publication: \n
/// 'UmbrellaClustering'
/// Jonas Lukasczyk and Julien Tierny.
/// TTK Publications.
/// 2020.
///

#pragma once

#include <Debug.h>

// std includes
#include <map>
#include <math.h>
#include <set>
#include <utility>
#include <vector>

namespace ttk {

  class UmbrellaClustering : virtual public Debug {

  public:
    UmbrellaClustering() {
      this->setDebugMsgPrefix("UmbrellaClustering");
    };
    ~UmbrellaClustering(){};

    struct Umbrella {
      std::vector<int> ids;

      Umbrella() {
        ids = std::vector<int>(0);
      }
    };

    template <typename DT>
    int computeUmbrellas(std::map<int, std::vector<int>> &pointUmbrellas,
                         const DT *coords,
                         const DT *pws,
                         const DT *pcs,
                         const int nPoints) const {

      ttk::Timer timer;

      const std::string msg = "Computing Point Umbrellas";
      this->printMsg(
        msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

      // umbrella indices list
      std::vector<int> inUmbrella(nPoints);
      for(int i = 0; i < nPoints; i++) {
        int maxUmbrellaIndex = -1;
        DT maxUmbrellaVal = 0.0;

        for(int j = 0; j < nPoints; j++) {
          const int i3 = i * 3;
          const int j3 = j * 3;

          const DT dx = coords[i3 + 0] - coords[j3 + 0];
          const DT dy = coords[i3 + 1] - coords[j3 + 1];
          const DT dz = coords[i3 + 2] - coords[j3 + 2];

          // Evaluate js value at i
          const DT umbrellaVal
            = pws[j] * std::exp(-0.5 * (dx * dx + dy * dy + dz * dz) / pcs[j]);

          if(umbrellaVal > maxUmbrellaVal) {
            maxUmbrellaVal = umbrellaVal;
            maxUmbrellaIndex = j;
          }
        }
        inUmbrella[i] = maxUmbrellaIndex;
      }

      // initialize umbrellas with point representatives
      for(int i = 0; i < nPoints; i++) {
        if(inUmbrella[i] == i) {
          pointUmbrellas[i] = std::vector<int>(0);
          pointUmbrellas[i].push_back(i);
        }
      }

      // loop through all points
      for(int i = 0; i < nPoints; i++) {
        // if a point is not within its own umbrella
        if(inUmbrella[i] != i) {
          int j = i;

          // loop until you find the root representative
          while(j != inUmbrella[j]) {
            j = inUmbrella[j];
          }

          pointUmbrellas[j].push_back(i);
        }
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1;
    }

    int computeUmbrellaMatrix(unsigned char *umbrellaMatrix,
                              std::map<int, std::vector<int>> &umbrellas0,
                              std::map<int, std::vector<int>> &umbrellas1,
                              const int nUmbrellas0,
                              const int nUmbrellas1) const {

      ttk::Timer timer;

      const std::string msg = "Computing Umbrella Matrix ("
                              + std::to_string(nUmbrellas0) + "x"
                              + std::to_string(nUmbrellas1) + ")";
      this->printMsg(
        msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

      int i = 0;
      for(const auto &u0 : umbrellas0) {
        int j = 0;
        for(const auto &u1 : umbrellas1) {
          int exist = 0;
          for(long unsigned int p0 = 0; p0 < u0.second.size(); p0++) {
            for(long unsigned int p1 = 0; p1 < u1.second.size(); p1++) {
              if(u0.second[p0] == u1.second[p1]) {
                exist = 1;
              }
            }
          }
          umbrellaMatrix[j * nUmbrellas0 + i] = exist;
          ++j;
        }
        ++i;
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1;
    }
  };
} // namespace ttk
