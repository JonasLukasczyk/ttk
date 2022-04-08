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

    // ax^2 + bx + c = 0
    template <typename DT>
    int rootsQuadratic(const DT a, const DT b, const DT c, DT &t) const {
      DT discriminant = b * b - 4 * a * c;
      DT r1;
      DT r2;

      if(discriminant > 0) {
        // this->printMsg("Diff");
        r1 = (-1 * b + std::sqrt(discriminant)) / (2 * a);
        r2 = (-1 * b - std::sqrt(discriminant)) / (2 * a);
        if(r1 > 0 && r1 < 1)
          t = r1;
        else
          t = r2;

        return 1;
      } else if(discriminant == 0) {
        // this->printMsg("Equal");
        if(a != 0)
          r1 = r2 = (-1 * b) / (2 * a);
        else
          r1 = r2 = 0;

        t = r1;
        return 1;
      } else {
        t = -1;
        return 0;
      }
    }

    template <typename DT>
    int computeThresholdedUmbrellas(
      std::map<int, std::vector<int>> &clustersThreshUmbrellas,
      const DT *coords,
      const DT *pws,
      const DT *pcs,
      const DT threshVal,
      const int nClusters) const {

      ttk::Timer timer;

      // const std::string msg = "Computing Thresholded Umbrella Clusters";
      // this->printMsg(
      //   msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

      // umbrella indices list
      std::vector<int> inThreshUmbrella(nClusters);
      for(int i = 0; i < nClusters; i++) {
        int maxUmbrellaIndex = -1;
        DT maxUmbrellaVal = 0.0;

        for(int j = 0; j < nClusters; j++) {
          const int i3 = i * 3;
          const int j3 = j * 3;

          const DT dx = coords[i3 + 0] - coords[j3 + 0];
          const DT dy = coords[i3 + 1] - coords[j3 + 1];
          const DT dz = coords[i3 + 2] - coords[j3 + 2];

          const DT sq = dx * dx + dy * dy + dz * dz;

          DT a = ((-0.5 / pcs[i]) - (-0.5 / pcs[j])) * sq;
          DT b = 2 * (-0.5 / pcs[j]) * sq;
          DT c
            = (-1 * (-0.5 / pcs[j]) * sq) + std::log(pws[i]) - std::log(pws[j]);
          DT t;
          int status = 0;
          status = this->rootsQuadratic<DT>(a, b, c, t);
          t = 1 - t; // we have looked at i - j, but we want to do the gaussian
                     // function for j so we have to switch around.

          if(!status)
            break;

          const DT pT[3]
            = {coords[i3 + 0] - (t * dx), coords[i3 + 1] - (t * dy),
               coords[i3 + 2] - (t * dz)};
          const DT dxT = coords[i3 + 0] - pT[0];
          const DT dyT = coords[i3 + 1] - pT[1];
          const DT dzT = coords[i3 + 2] - pT[2];
          const DT sqT = dxT * dxT + dyT * dyT + dzT * dzT;

          // Evaluate js value at intersection between i and j
          const DT umbrellaVal = pws[j] * std::exp(-0.5 * (sqT) / pcs[j]);
          // this->printMsg("t: " + std::to_string(t));
          // this->printMsg("Dist: " + std::to_string(sqT));
          // this->printMsg("between: " + std::to_string(i) + " " +
          // std::to_string(j)); this->printMsg("Val: " +
          // std::to_string(umbrellaVal));
          if(umbrellaVal >= threshVal) {
            if(pws[j] > maxUmbrellaVal) {
              maxUmbrellaVal = pws[j];
              maxUmbrellaIndex = j;
            }
          }
        }

        inThreshUmbrella[i] = maxUmbrellaIndex;
      }

      // initialize umbrellas with point representatives
      for(int i = 0; i < nClusters; i++) {
        if(inThreshUmbrella[i] == i) {
          clustersThreshUmbrellas[i] = std::vector<int>(0);
          clustersThreshUmbrellas[i].push_back(i);
        } else if(inThreshUmbrella[i] == -1) {
          clustersThreshUmbrellas[i] = std::vector<int>(0);
          clustersThreshUmbrellas[i].push_back(-1);
        }
      }

      // loop through all points
      for(int i = 0; i < nClusters; i++) {
        // if a point is not within its own umbrella
        if(inThreshUmbrella[i] != i && inThreshUmbrella[i] != -1) {
          int j = i;

          // loop until you find the root representative
          while(j != inThreshUmbrella[j]) {
            j = inThreshUmbrella[j];
          }

          clustersThreshUmbrellas[j].push_back(i);
        }
      }

      // this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1;
    }

    template <typename DT>
    int computeUmbrellaMatrix(unsigned char *umbrellaMatrix,
                              std::map<int, std::vector<int>> &umbrellas0,
                              std::map<int, std::vector<int>> &umbrellas1,
                              const DT *ids0,
                              const DT *ids1,
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
              if(ids0[u0.second[p0]] == ids1[u1.second[p1]]) {
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

    template <typename DT>
    int computeThresholdedUmbrellaMatrix(
      unsigned char *tUmbrellaMatrix,
      std::map<int, std::vector<int>> &tumbrellas0,
      std::map<int, std::vector<int>> &tumbrellas1,
      const DT *ids0,
      const DT *ids1,
      const int ntClusters0,
      const int ntClusters1) const {

      ttk::Timer timer;

      const std::string msg = "Computing Umbrella Matrix ("
                              + std::to_string(ntClusters0) + "x"
                              + std::to_string(ntClusters1) + ")";
      this->printMsg(
        msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

      int i = 0;
      for(const auto &tu0 : tumbrellas0) {
        int j = 0;
        for(const auto &tu1 : tumbrellas1) {
          int exist = 0;
          for(long unsigned int c0 = 0; c0 < tu0.second.size(); c0++) {
            for(long unsigned int c1 = 0; c1 < tu1.second.size(); c1++) {
              if(ids0[tu0.second[c0]] == ids1[tu1.second[c1]]) {
                exist = 1;
              }
            }
          }
          tUmbrellaMatrix[j * ntClusters0 + i] = exist;
          ++j;
        }
        ++i;
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1;
    }
  };
} // namespace ttk
