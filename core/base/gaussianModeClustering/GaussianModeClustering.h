/// \ingroup base
/// \class ttk::GaussianModeClustering
/// \author Emma Nilsson <emma.nilsson@liu.se>
/// \date The Date Here.
///
/// \b Related \b publication: \n
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

  class GaussianModeClustering : virtual public Debug {

  public:
    GaussianModeClustering() {
      this->setDebugMsgPrefix("GaussianModeClustering");
    };
    ~GaussianModeClustering(){};

    template <typename DT>
    int computeUmbrellas(std::map<int, std::vector<int>> &pointUmbrellas,
                         int &nUmbrellas,
                         const DT *coords,
                         const double *pws,
                         const double *pcs,
                         const int nPoints) const {

      ttk::Timer timer;

      const std::string msg = "Computing Point Umbrellas";
      this->printMsg(
        msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

      // umbrella indices list
      std::vector<int> inUmbrella(nPoints);
      for(int i = 0; i < nPoints; i++) {
        int maxUmbrellaIndex = -1;
        double maxUmbrellaVal = 0.0;

        for(int j = 0; j < nPoints; j++) {
          const int i3 = i * 3;
          const int j3 = j * 3;

          const DT dx = coords[i3 + 0] - coords[j3 + 0];
          const DT dy = coords[i3 + 1] - coords[j3 + 1];
          const DT dz = coords[i3 + 2] - coords[j3 + 2];

          // Evaluate js value at i
          const double umbrellaVal
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
          nUmbrellas++;
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
      std::map<int, std::vector<int>> &pointThreshUmbrellas,
      int &nUmbrellas,
      const DT *coords,
      const double *pws,
      const double *pcs,
      const double threshVal,
      const int nPoints) const {

      ttk::Timer timer;

      const std::string msg = "Computing Thresholded Umbrellas";
      this->printMsg(
        msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

      // umbrella indices list
      std::vector<int> inThreshUmbrella(nPoints);
      for(int i = 0; i < nPoints; i++) {
        int maxUmbrellaIndex = -1;
        DT maxUmbrellaVal = 0.0;

        for(int j = 0; j < nPoints; j++) {
          const int i3 = i * 3;
          const int j3 = j * 3;

          const DT dx = coords[i3 + 0] - coords[j3 + 0];
          const DT dy = coords[i3 + 1] - coords[j3 + 1];
          const DT dz = coords[i3 + 2] - coords[j3 + 2];

          const double sq = dx * dx + dy * dy + dz * dz;

          double a = ((-0.5 / pcs[i]) - (-0.5 / pcs[j])) * sq;
          double b = 2 * (-0.5 / pcs[j]) * sq;
          double c
            = (-1 * (-0.5 / pcs[j]) * sq) + std::log(pws[i]) - std::log(pws[j]);
          double t;
          int status = 0;
          status = this->rootsQuadratic<double>(a, b, c, t);
          t = 1 - t; // we have looked at i - j, but we want to do the gaussian
                     // function for j so we have to switch around.

          if(!status)
            break;

          const double pT[3]
            = {coords[i3 + 0] - (t * dx), coords[i3 + 1] - (t * dy),
               coords[i3 + 2] - (t * dz)};
          const double dxT = coords[i3 + 0] - pT[0];
          const double dyT = coords[i3 + 1] - pT[1];
          const double dzT = coords[i3 + 2] - pT[2];
          const double sqT = dxT * dxT + dyT * dyT + dzT * dzT;

          // Evaluate js value at intersection between i and j
          const double umbrellaVal = pws[j] * std::exp(-0.5 * (sqT) / pcs[j]);

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
      for(int i = 0; i < nPoints; i++) {
        if(inThreshUmbrella[i] == i) {
          pointThreshUmbrellas[i] = std::vector<int>(0);
          pointThreshUmbrellas[i].push_back(i);
          nUmbrellas++;
        } else if(inThreshUmbrella[i] == -1) {
          pointThreshUmbrellas[i] = std::vector<int>(0);
          pointThreshUmbrellas[i].push_back(-1);
        }
      }

      // loop through all points
      for(int i = 0; i < nPoints; i++) {
        // if a point is not within its own umbrella
        if(inThreshUmbrella[i] != i && inThreshUmbrella[i] != -1) {
          int j = i;

          // loop until you find the root representative
          while(j != inThreshUmbrella[j]) {
            j = inThreshUmbrella[j];
          }

          pointThreshUmbrellas[j].push_back(i);
        }
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1;
    }

    template <typename DT>
    int computeUnimodality(std::map<int, std::vector<int>> &pointClusters,
                           int &nClusters,
                           const DT *coords,
                           const double *pws,
                           const double *pcs,
                           const int nPoints) const {

      ttk::Timer timer;

      const std::string msg = "Computing Unimodality";
      this->printMsg(
        msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

      // indices list
      std::vector<int> inClusters(nPoints);
      for(int i = 0; i < nPoints; i++) {
        int maxClusterIndex = i;
        DT maxClusterVal = pws[i];

        for(int j = 0; j < nPoints; j++) {
          const int i3 = i * 3;
          const int j3 = j * 3;

          const DT dx = coords[i3 + 0] - coords[j3 + 0];
          const DT dy = coords[i3 + 1] - coords[j3 + 1];
          const DT dz = coords[i3 + 2] - coords[j3 + 2];

          // Check if points are close enough
          if(std::abs(dx) < 3 * std::sqrt(pcs[j])
             && std::abs(dy) < 3 * std::sqrt(pcs[j])
             && std::abs(dz) < 3 * std::sqrt(pcs[j])) {

            // points fulfill criteria and has the highest "mean" value
            if(std::abs(pws[j] - pws[i])
                 >= 2 * std::min(std::sqrt(pcs[i]), std::sqrt(pcs[j]))
               && pws[j] > pws[i] && pws[j] > maxClusterVal) {
              maxClusterIndex = j;
            }
          }
        }
        inClusters[i] = maxClusterIndex;
      }

      // initialize with point representatives
      for(int i = 0; i < nPoints; i++) {
        if(inClusters[i] == i) {
          pointClusters[i] = std::vector<int>(0);
          pointClusters[i].push_back(i);
          nClusters++;
        }
      }

      // loop through all points
      for(int i = 0; i < nPoints; i++) {
        // if a point is not within its own gaussian
        if(inClusters[i] != i) {
          int j = i;

          // loop until you find the root representative
          while(j != inClusters[j]) {
            j = inClusters[j];
          }

          pointClusters[j].push_back(i);
        }
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1;
    }

    template <typename DT>
    int computeMatrix(unsigned char *matrix,
                      std::map<int, std::vector<int>> &clusters0,
                      std::map<int, std::vector<int>> &clusters1,
                      const DT *ids0,
                      const DT *ids1,
                      const int nClusters0,
                      const int nClusters1) const {

      ttk::Timer timer;

      const std::string msg = "Computing Matrix (" + std::to_string(nClusters0)
                              + "x" + std::to_string(nClusters1) + ")";
      this->printMsg(
        msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

      int i = 0;
      for(const auto &c0 : clusters0) {
        if(c0.second[0] == -1) {
          continue;
        }
        int j = 0;
        for(const auto &c1 : clusters1) {
          if(c1.second[0] == -1)
            continue;
          int exist = 0;
          for(long unsigned int p0 = 0; p0 < c0.second.size(); p0++) {
            for(long unsigned int p1 = 0; p1 < c1.second.size(); p1++) {
              if(ids0[c0.second[p0]] == ids1[c1.second[p1]]) {
                exist = 1;
              }
            }
          }
          matrix[j * nClusters0 + i] = exist;
          ++j;
        }
        ++i;
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      return 1;
    }
  };
} // namespace ttk
