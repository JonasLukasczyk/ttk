/// TODO 1: Provide your information
///
/// \ingroup base
/// \class ttk::ScalarFieldFromPointsNew
/// \author Your Name Here <Your Email Address Here>
/// \date The Date Here.
///
/// This module defines the %ScalarFieldFromPointsNew class that computes for
/// each vertex of a triangulation the average scalar value of itself and its
/// direct neighbors.
///
/// \b Related \b publication: \n
/// 'ScalarFieldFromPointsNew'
/// Jonas Lukasczyk and Julien Tierny.
/// TTK Publications.
/// 2020.
///

#pragma once

// ttk common includes
#include <Debug.h>
#include <Triangulation.h>

#include <math.h>

namespace ttk {

  /**
   * The ScalarFieldFromPointsNew class provides methods to compute for each
   * vertex of a triangulation the average scalar value of itself and its direct
   * neighbors.
   */

  class ScalarFieldFromPointsNew : virtual public Debug {

  public:
    typedef double (*KERNEL)(const double &, const double &, const double &);

    static double
      Linear(const double &u, const double &bandwidth, const double &amp) {
      double su = std::sqrt(u) / bandwidth;
      return su >= 1 ? 0 : 1 - su;
    };


    static double
      Gaussian(const double &u, const double &bandwidth, const double &amp) {
      return amp * exp(-0.5 * (u / bandwidth));
    };

    static double
      Constant(const double &u, const double &bandwidth, const double &amp) {
      double su = std::sqrt(u) / bandwidth;
      return su < 1.0 ? 1.0 : 0;
    }

    ScalarFieldFromPointsNew() {
      this->setDebugMsgPrefix("ScalarFieldFromPointsNew");
    };
    ~ScalarFieldFromPointsNew(){};

    template <KERNEL k>
    int computeScalarField2D(double *outputData,
                             const double *pointCoordiantes,
                             const double *weights,
                             const double *constants,
                             const double *bounds,
                             const double *spacing,
                             const int *dims,
                             const size_t &nPoints,
                             const size_t &nPixels) const {
      ttk::Timer timer;

      this->printMsg("Computing Scalar Field 2D",
                     0, // progress form 0-1
                     0, // elapsed time so far
                     this->threadNumber_, ttk::debug::LineMode::REPLACE);

      const double dx = spacing[0];
      const double dy = spacing[1];
      const double dx2 = dx / 2.0;
      const double dy2 = dy / 2.0;
      const int width = dims[0];
      const int height = dims[1];

      const double xBound = bounds[0] - dx2;
      const double yBound = bounds[2] - dy2;

      // create locks
      omp_lock_t lock[nPixels];

      // clear data and init locks
      for(int i = 0, j = nPixels; i < j; i++) {
        outputData[i] = 0.0;
        omp_init_lock(&(lock[i]));
      }

// compute scalar field
#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif
      for(size_t i = 0; i < nPoints; i++) {
        const double &xP = pointCoordiantes[i * 3 + 0];
        const double &yP = pointCoordiantes[i * 3 + 1];

        const int xi
          = std::min(width, std::max(0, (int)floor((xP - xBound) / dx)));
        const int yi
          = std::min(height, std::max(0, (int)floor((yP - yBound) / dy)));

        const int kdx = floor(3 * sqrt(constants[i]) / dx + 0.5);
        const int kdy = floor(3 * sqrt(constants[i]) / dy + 0.5);

        const int x0 = std::max(0, std::min(width - 1, xi - kdx));
        const int x1 = std::max(0, std::min(width - 1, xi + kdx));
        const int y0 = std::max(0, std::min(height - 1, yi - kdy));
        const int y1 = std::max(0, std::min(height - 1, yi + kdy));

        // for all points in the bandwidth interval, calculate scalar value
        for(int x = x0; x <= x1; x++) {
          for(int y = y0; y <= y1; y++) {
            double xxx = (x - xi) * dx;
            double yyy = (y - yi) * dy;
            const double u = (xxx * xxx + yyy * yyy);
            // this->printMsg(std::to_string(std::sqrt(u)));
            const double ku = k(u, constants[i], weights[i]);

            int pixelIndex = y * width + x;
            omp_set_lock(&(lock[pixelIndex]));
            outputData[pixelIndex] = std::max(ku, outputData[pixelIndex]);
            omp_unset_lock(&(lock[pixelIndex]));
          }
        }
      }

      // destroy all locks
      for(int i = 0, j = nPixels; i < j; i++) {
        omp_destroy_lock(&(lock[i]));
      }

      // print the progspacings of the current subprocedure with elapsed time
      this->printMsg("Computing Scalar Field 2D",
                     1, // progress
                     timer.getElapsedTime(), this->threadNumber_);

      return 1; // return success
    }

    template <KERNEL k>
    int computeScalarField3D(double *outputData,
                             const double *pointCoordiantes,
                             const double *weights,
                             const double *constants,
                             const double *bounds,
                             const double *spacing,
                             const int *dims,
                             const size_t &nPoints,
                             const size_t &nPixels) const {
      ttk::Timer timer;

      this->printMsg("Computing Scalar Field 3D",
                     0, // progress form 0-1
                     0, // elapsed time so far
                     this->threadNumber_, ttk::debug::LineMode::REPLACE);

      const double dx = spacing[0];
      const double dy = spacing[1];
      const double dz = spacing[2];
      const double dx2 = dx / 2.0;
      const double dy2 = dy / 2.0;
      const double dz2 = dz / 2.0;
      const int width = dims[0];
      const int height = dims[1];
      const int depth = dims[2];

      const double xBound = bounds[0] - dx2;
      const double yBound = bounds[2] - dy2;
      const double zBound = bounds[4] - dz2;

      // create locks
      omp_lock_t lock[nPixels];

      // clear data and init locks
      for(int i = 0, j = nPixels; i < j; i++) {
        outputData[i] = 0.0;
        omp_init_lock(&(lock[i]));
      }

// compute scalar field
#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif
      for(size_t i = 0; i < nPoints; i++) {
        const double &xP = pointCoordiantes[i * 3 + 0];
        const double &yP = pointCoordiantes[i * 3 + 1];
        const double &zP = pointCoordiantes[i * 3 + 2];

        const int xi
          = std::min(width, std::max(0, (int)floor((xP - xBound) / dx)));
        const int yi
          = std::min(height, std::max(0, (int)floor((yP - yBound) / dy)));
        const int zi
          = std::min(depth, std::max(0, (int)floor((zP - zBound) / dz)));

        const int kdx = floor(3 * sqrt(constants[i]) / dx + 0.5);
        const int kdy = floor(3 * sqrt(constants[i]) / dy + 0.5);
        const int kdz = floor(3 * sqrt(constants[i]) / dz + 0.5);

        const int x0 = std::max(0, std::min(width - 1, xi - kdx));
        const int x1 = std::max(0, std::min(width - 1, xi + kdx));
        const int y0 = std::max(0, std::min(height - 1, yi - kdy));
        const int y1 = std::max(0, std::min(height - 1, yi + kdy));
        const int z0 = std::max(0, std::min(depth - 1, zi - kdz));
        const int z1 = std::max(0, std::min(depth - 1, zi + kdz));

        // for all points in the bandwidth interval, calculate scalar value
        for(int x = x0; x <= x1; x++) {
          for(int y = y0; y <= y1; y++) {
            for(int z = z0; z <= z1; z++) {
              double xxx = (x - xi) * dx;
              double yyy = (y - yi) * dy;
              double zzz = (z - zi) * dz;
              const double u = (xxx * xxx + yyy * yyy + zzz * zzz);
              // this->printMsg(std::to_string(std::sqrt(u)));
              const double ku = k(u, constants[i], weights[i]);

              int pixelIndex = z * width * height + y * width + x;
              omp_set_lock(&(lock[pixelIndex]));
              outputData[pixelIndex] = std::max(ku, outputData[pixelIndex]);
              omp_unset_lock(&(lock[pixelIndex]));
            }
          }
        }
      }

      // destroy all locks
      for(int i = 0, j = nPixels; i < j; i++) {
        omp_destroy_lock(&(lock[i]));
      }

      // print the progress of the current subprocedure with elapsed time
      this->printMsg("Computing Scalar Field 3D",
                     1, // progress
                     timer.getElapsedTime(), this->threadNumber_);

      return 1; // return success
    }

  }; // ScalarFieldFromPointsNew class

} // namespace ttk
