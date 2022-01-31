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

    static double Epanechnikov(const double &u,
                               const double &bandwidth,
                               const double &amp) {
      double su = std::sqrt(u) / bandwidth;
      return su >= 1 ? 0 : 0.75 - 0.75 * su * su;
    };

    static double
      Gaussian(const double &u, const double &bandwidth, const double &amp) {
      // double c = (1.0/std::sqrt(std::pow(2.0*3.14159265359, dim) *
      // std::pow(bandwidth, dim))); return c * exp(-0.5*(u/bandwidth));
      return amp * exp(-0.5 * (u / bandwidth));
    };

    static double
      Constant(const double &u, const double &bandwidth, const double &amp) {
      double su = std::sqrt(u) / bandwidth;
      // if (u < 0.0) {
      //   ttk::ScalarFieldFromPointsNew hm;
      //   hm.printMsg("ha");
      // }
      return su < 1.0 ? 1.0 : 0;
    }

    ScalarFieldFromPointsNew() {
      this->setDebugMsgPrefix("ScalarFieldFromPointsNew");
    };
    ~ScalarFieldFromPointsNew(){};

    template <typename TT, KERNEL k>
    int computeScalarField(double *outputData,
                           int *voronoiData,
                           int *weightedVoronoiData,
                           int *powerDiagramArrayData,
                           const double *pointCoordiantes,
                           const double *amplitudes,
                           const double *spreads,
                           const double *rates,
                           const int *births,
                           const int *deaths,
                           const size_t nPoints,
                           const int t,
                           const float bandwidth,
                           const TT *triangulation) const {
      ttk::Timer timer;

      this->printMsg("Computing scalar field",
                     0, // progress form 0-1
                     0, // elapsed time so far
                     this->threadNumber_, ttk::debug::LineMode::REPLACE);

      // compute the average of each vertex in parallel
      size_t nVertices = triangulation->getNumberOfVertices();

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif
      for(size_t i = 0; i < nVertices; i++) {
        outputData[i] = 0.0;
      }

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif
      for(size_t i = 0; i < nVertices; i++) {
        float x, y, z;
        triangulation->getVertexPoint(i, x, y, z);

        double &f = outputData[i];
        int &vCell = voronoiData[i];
        int &wvCell = weightedVoronoiData[i];
        int &pdCell = powerDiagramArrayData[i];
        // f = 0;
        vCell = -1;
        wvCell = -1;
        pdCell = -1;

        double minDistance = std::numeric_limits<double>::max();
        double wMinDistance = std::numeric_limits<double>::max();
        double minPower = std::numeric_limits<double>::max();

        for(size_t j = 0; j < nPoints; j++) {
          const size_t &j3 = j * 3;

          double dx = x - pointCoordiantes[j3 + 0];
          double dy = y - pointCoordiantes[j3 + 1];
          double dz = z - pointCoordiantes[j3 + 2];

          // S_j(t) decides how the feature emerges and decays based on rate,
          // birth time, death time and the current timestep
          double Sj_t = (1 / (1 + std::exp(-1 * rates[j] * (t - births[j]))))
                        + (1 / (1 + std::exp(-1 * rates[j] * (deaths[j] - t))))
                        - 1;
          // Sj_t = 1.0;
          double amp_j = Sj_t * amplitudes[j];

          // Calculate norm of vector to be used in kernel
          const double u = (dx * dx + dy * dy + dz * dz);
          const double ku = k(u, spreads[j], amp_j);
          // f += ku;
          f = std::max(f, ku);

          // Check if distance is the smallest to save that point
          // for the voronoi segmentation
          if(std::sqrt(u) < minDistance) {
            minDistance = std::sqrt(u);
            vCell = j;
          }

          if((std::sqrt(u) / amp_j) < wMinDistance) { // weighted voronoi
            wMinDistance = std::sqrt(u) / amp_j;
            wvCell = j;
          }

          if((u - amp_j * amp_j) < minPower) { // power diagram
            minPower = u - amp_j * amp_j;
            pdCell = j;
          }
        }
      }

      // print the progress of the current subprocedure with elapsed time
      this->printMsg("Computing scalar field",
                     1, // progress
                     timer.getElapsedTime(), this->threadNumber_);

      return 1; // return success
    }

    template <typename TT, KERNEL k>
    int computeScalarField3Dv2(double *outputData,
                               const double *pointCoordiantes,
                               const double *amplitudes,
                               const double *spreads,
                               const double *rates,
                               const int *births,
                               const int *deaths,
                               const size_t nPoints,
                               const int t,
                               const double *bounds,
                               const double *res,
                               const TT *triangulation) const {
      ttk::Timer timer;

      this->printMsg("Computing scalar field 3D v2",
                     0, // progress form 0-1
                     0, // elapsed time so far
                     this->threadNumber_, ttk::debug::LineMode::REPLACE);

      // Initialize all vertices to 0 for the image data
      size_t nVertices = triangulation->getNumberOfVertices();

#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif
      for(size_t i = 0; i < nVertices; i++) {
        outputData[i] = 0.0;
      }

      // Set variables
      const double width = bounds[1] - bounds[0];
      const double height = bounds[3] - bounds[2];
      const double depth = bounds[5] - bounds[4];
      const double resXm1 = res[0] - 1;
      const double resYm1 = res[1] - 1;
      const double resZm1 = res[2] - 1;
      const int iResX = res[0];
      const int iResY = res[1];
      const int iResZ = res[2];
      const int iResXm1 = iResX - 1;
      const int iResYm1 = iResY - 1;
      const int iResZm1 = iResZ - 1;

      // Calculate width, height and depth for the output kernel, and the number
      // of points in each direction that should be given scalar values
      const double dx = width / resXm1;
      const double dy = height / resYm1;
      const double dz = depth / resZm1;

// compute scalar field
#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif
      for(size_t i = 0; i < nPoints; i++) {
        const double &xP = pointCoordiantes[i * 3 + 0];
        const double &yP = pointCoordiantes[i * 3 + 1];
        const double &zP = pointCoordiantes[i * 3 + 2];

        const int xi = std::min(
          resXm1, std::max(0.0, floor((xP - (bounds[0] - dx / 2.0)) / dx)));
        const int yi = std::min(
          resYm1, std::max(0.0, floor((yP - (bounds[2] - dy / 2.0)) / dy)));
        const int zi = std::min(
          resZm1, std::max(0.0, floor((zP - (bounds[4] - dz / 2.0)) / dz)));

        const int kdx = floor(3 * sqrt(spreads[i]) / dx + 0.5);
        const int kdy = floor(3 * sqrt(spreads[i]) / dy + 0.5);
        const int kdz = floor(3 * sqrt(spreads[i]) / dz + 0.5);

        const int x0 = std::max(0, std::min(iResXm1, xi - kdx));
        const int x1 = std::max(0, std::min(iResXm1, xi + kdx));
        const int y0 = std::max(0, std::min(iResYm1, yi - kdy));
        const int y1 = std::max(0, std::min(iResYm1, yi + kdy));
        const int z0 = std::max(0, std::min(iResZm1, zi - kdz));
        const int z1 = std::max(0, std::min(iResZm1, zi + kdz));

        // S_i(t) decides how the feature emerges and decays based on rate,
        // birth time, death time and the current timestep
        double Si_t = (1 / (1 + std::exp(-1 * rates[i] * (t - births[i]))))
                      + (1 / (1 + std::exp(-1 * rates[i] * (deaths[i] - t))))
                      - 1;
        Si_t = 1.0;
        double amp_i = Si_t * amplitudes[i];

        // for all points in the bandwidth interval, calculate scalar value
        for(int x = x0; x <= x1; x++) {
          for(int y = y0; y <= y1; y++) {
            for(int z = z0; z <= z1; z++) {

              double xxx = (x - xi) * dx;
              double yyy = (y - yi) * dy;
              double zzz = (z - zi) * dz;
              const double u = (xxx * xxx + yyy * yyy + zzz * zzz);
              const double ku = k(u, spreads[i], amp_i);

              int imgIndex = z * iResY * iResX + y * iResX + x;

#ifdef TTK_ENABLE_OPENMP
#pragma omp atomic update
#endif
              outputData[imgIndex] += ku;
            }
          }
        }
      }

      // print the progress of the current subprocedure with elapsed time
      this->printMsg("Computing scalar field 3D v2",
                     1, // progress
                     timer.getElapsedTime(), this->threadNumber_);

      return 1; // return success
    }

    template <KERNEL k>
    int computeScalarField2D(double *outputData,
                             // int* idData,
                             const double *pointCoordiantes,
                             const double *amplitudes,
                             const double *spreads,
                             const float bandwidth,
                             const double *bounds,
                             const double *res,
                             const size_t &nPoints) const {
      ttk::Timer timer;

      this->printMsg("Computing Scalar Field 2D",
                     0, // progress form 0-1
                     0, // elapsed time so far
                     this->threadNumber_, ttk::debug::LineMode::REPLACE);

      const int nPixels = res[0] * res[1];

      const double width = bounds[1] - bounds[0];
      const double height = bounds[3] - bounds[2];

      const double resXm1 = res[0] - 1;
      const double resYm1 = res[1] - 1;
      const int iResX = res[0];
      const int iResY = res[1];
      const int iResXm1 = iResX - 1;
      const int iResYm1 = iResY - 1;

      // Calculate width and height for the output kernel, and the number of
      // points in each direction that should be given scalar values
      const double dx = width / resXm1;
      const double dy = height / resYm1;
      const double dx2 = dx / 2.0;
      const double dy2 = dy / 2.0;

      const double xBound = bounds[0] - dx2;
      const double yBound = bounds[2] - dy2;

      // const int kdx = floor(3 * sqrt(bandwidth)/dx+0.5);
      // const int kdy = floor(3 * sqrt(bandwidth)/dy+0.5);

      // clear
      for(int i = 0, j = nPixels; i < j; i++) {
        outputData[i] = 0.0;
        // idData[i] = 0;
      }

// compute scalar field
#ifdef TTK_ENABLE_OPENMP
#pragma omp parallel for num_threads(this->threadNumber_)
#endif
      for(size_t i = 0; i < nPoints; i++) {
        const double &xP = pointCoordiantes[i * 3 + 0];
        const double &yP = pointCoordiantes[i * 3 + 1];

        const int xi
          = std::min(resXm1, std::max(0.0, floor((xP - xBound) / dx)));
        const int yi
          = std::min(resYm1, std::max(0.0, floor((yP - yBound) / dy)));

        const int kdx = floor(3 * sqrt(spreads[i]) / dx + 0.5);
        const int kdy = floor(3 * sqrt(spreads[i]) / dy + 0.5);

        const int x0 = std::max(0, std::min(iResXm1, xi - kdx));
        const int x1 = std::max(0, std::min(iResXm1, xi + kdx));
        const int y0 = std::max(0, std::min(iResYm1, yi - kdy));
        const int y1 = std::max(0, std::min(iResYm1, yi + kdy));

        // for all points in the bandwidth interval, calculate scalar value
        for(int x = x0; x <= x1; x++) {
          for(int y = y0; y <= y1; y++) {

            double xxx = (x - xi) * dx;
            double yyy = (y - yi) * dy;
            const double u = (xxx * xxx + yyy * yyy);
            const double ku = k(u, spreads[i], amplitudes[i]);

#ifdef TTK_ENABLE_OPENMP
#pragma omp atomic update
#endif
            outputData[y * iResX + x] += ku;
            // idData[y * iResX + x] = i + 1;
          }
        }
      }

      // print the progress of the current subprocedure with elapsed time
      this->printMsg("Computing Scalar Field 2D",
                     1, // progress
                     timer.getElapsedTime(), this->threadNumber_);

      return 1; // return success
    }

    template <KERNEL k>
    int computeScalarField3D(double *outputData,
                             // int* idData,
                             const double *pointCoordiantes,
                             const double *amplitudes,
                             const double *spreads,
                             const float bandwidth,
                             const double *bounds,
                             const double *res,
                             const size_t &nPoints) const {
      ttk::Timer timer;

      this->printMsg("Computing Scalar Field 3D",
                     0, // progress form 0-1
                     0, // elapsed time so far
                     this->threadNumber_, ttk::debug::LineMode::REPLACE);

      const int nPixels = res[0] * res[1] * res[2];

      const double width = bounds[1] - bounds[0];
      const double height = bounds[3] - bounds[2];
      const double depth = bounds[5] - bounds[4];
      const double resXm1 = res[0] - 1;
      const double resYm1 = res[1] - 1;
      const double resZm1 = res[2] - 1;
      const int iResX = res[0];
      const int iResY = res[1];
      const int iResZ = res[2];
      const int iResXm1 = iResX - 1;
      const int iResYm1 = iResY - 1;
      const int iResZm1 = iResZ - 1;

      // Calculate width, height and depth for the output kernel, and the number
      // of points in each direction that should be given scalar values
      const double dx = width / resXm1;
      const double dy = height / resYm1;
      const double dz = depth / resZm1;
      const double dx2 = dx / 2.0;
      const double dy2 = dy / 2.0;
      const double dz2 = dz / 2.0;

      const double xBound = bounds[0] - dx2;
      const double yBound = bounds[2] - dy2;
      const double zBound = bounds[4] - dz2;

      // clear
      for(int i = 0, j = nPixels; i < j; i++) {
        outputData[i] = 0.0;
        // idData[i] = 0;
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
          = std::min(resXm1, std::max(0.0, floor((xP - xBound) / dx)));
        const int yi
          = std::min(resYm1, std::max(0.0, floor((yP - yBound) / dy)));
        const int zi
          = std::min(resZm1, std::max(0.0, floor((zP - zBound) / dz)));

        const int kdx = floor(3 * sqrt(spreads[i]) / dx + 0.5);
        const int kdy = floor(3 * sqrt(spreads[i]) / dy + 0.5);
        const int kdz = floor(3 * sqrt(spreads[i]) / dz + 0.5);

        const int x0 = std::max(0, std::min(iResXm1, xi - kdx));
        const int x1 = std::max(0, std::min(iResXm1, xi + kdx));
        const int y0 = std::max(0, std::min(iResYm1, yi - kdy));
        const int y1 = std::max(0, std::min(iResYm1, yi + kdy));
        const int z0 = std::max(0, std::min(iResZm1, zi - kdz));
        const int z1 = std::max(0, std::min(iResZm1, zi + kdz));

        // for all points in the bandwidth interval, calculate scalar value
        for(int x = x0; x <= x1; x++) {
          for(int y = y0; y <= y1; y++) {
            for(int z = z0; z <= z1; z++) {
              double xxx = (x - xi) * dx;
              double yyy = (y - yi) * dy;
              double zzz = (z - zi) * dz;
              const double u = (xxx * xxx + yyy * yyy + zzz * zzz);
              this->printMsg(std::to_string(std::sqrt(u)));
              const double ku = k(u, spreads[i], amplitudes[i]);

#ifdef TTK_ENABLE_OPENMP
#pragma omp atomic update
#endif
              outputData[z * iResY * iResX + y * iResX + x] += ku;
              // idData[z * iResY * iResX + y * iResX + x] = i + 1;
            }
          }
        }
      }

      // print the progress of the current subprocedure with elapsed time
      this->printMsg("Computing Scalar Field 3D",
                     1, // progress
                     timer.getElapsedTime(), this->threadNumber_);

      return 1; // return success
    }

    /*
    int computeCounts2D(
          int* counts,
          const double* pointCoordiantes,
          const double* bounds,
          const double* res,
          const size_t& nPoints,
          const size_t& nPixels
        ) const {
          ttk::Timer timer;

          this->printMsg("Computing Counts", 0, 0, this->threadNumber_,
    ttk::debug::LineMode::REPLACE);

          // clear
          size_t nTotal = nPixels;
          for(size_t i=0; i<nTotal; i++){
            counts[i] = 0;
          }

          const float width = bounds[1]-bounds[0];
          const float height = bounds[3]-bounds[2];
          const float resX = res[0];
          const float resXm1 = res[0]-1;
          const float resYm1 = res[1]-1;

          const int iResX = resX;
          const float dx = width / resXm1;
          const float dy = height / resYm1;
          const float dx2 = dx/2.0;
          const float dy2 = dy/2.0;

          const float x0 = bounds[0] - dx2;
          const float y0 = bounds[2] - dy2;

          #ifdef TTK_ENABLE_OPENMP
          #pragma omp parallel for num_threads(this->threadNumber_)
          #endif
          for(size_t i=0; i<nPoints; i++){

            const float& x = pointCoordiantes[i*3+0];
            const float& y = pointCoordiantes[i*3+1];

            const int xi = std::min(
              resXm1,
              std::max(
                0.0f,
                floor( (x - x0)/dx )
              )
            );
            const int yi = std::min(
              resYm1,
              std::max(
                0.0f,
                floor( (y - y0)/dy )
              )
            );

            const int pixelIndex = yi * iResX + xi;
            const int countIndex = pixelIndex;

            #ifdef TTK_ENABLE_OPENMP
            #pragma omp atomic update
            #endif
            counts[countIndex]++;
          }

          // print the progress of the current subprocedure with elapsed time
          this->printMsg("Computing Counts",
                         1, // progress
                         timer.getElapsedTime(), this->threadNumber_);

          return 1; // return success
        }

        int computeCounts3D(
          int* counts,
          const double* pointCoordiantes,
          const double* bounds,
          const double* res,
          const size_t& nPoints,
          const size_t& nPixels
        ) const {
          ttk::Timer timer;

          this->printMsg("Computing Counts", 0, 0, this->threadNumber_,
    ttk::debug::LineMode::REPLACE);

          // clear
          size_t nTotal = nPixels;
          for(size_t i=0; i<nTotal; i++){
            counts[i] = 0;
          }

          const float width = bounds[1]-bounds[0];
          const float height = bounds[3]-bounds[2];
          const float depth = (bounds[5] > 1) ? bounds[5]-bounds[4] : 0;
          const float resX = res[0];
          const float resXm1 = res[0]-1;
          const float resYm1 = res[1]-1;
          const float resZm1 = (res[2] > 1) ? res[2]-1 : 1;

          const int iResX = resX;
          const int iResY = res[1];
          const float dx = width / resXm1;
          const float dy = height / resYm1;
          const float dz = depth / resZm1;
          const float dx2 = dx/2.0;
          const float dy2 = dy/2.0;
          const float dz2 = dz/2.0;

          const float x0 = bounds[0] - dx2;
          const float y0 = bounds[2] - dy2;
          const float z0 = bounds[4] - dz2;



          #ifdef TTK_ENABLE_OPENMP
          #pragma omp parallel for num_threads(this->threadNumber_)
          #endif
          for(size_t i=0; i<nPoints; i++){

            const float& x = pointCoordiantes[i*3+0];
            const float& y = pointCoordiantes[i*3+1];
            const float& z = pointCoordiantes[i*3+2];

            const int xi = std::min(
              resXm1,
              std::max(
                0.0f,
                floor( (x - x0)/dx )
              )
            );
            const int yi = std::min(
              resYm1,
              std::max(
                0.0f,
                floor( (y - y0)/dy )
              )
            );
            const int zi = std::min(
              resZm1,
              std::max(
                0.0f,
                floor((z - z0)/dz)
              )
            );

            const int pixelIndex = zi * iResX * iResY + yi * iResX + xi;
            const int countIndex = pixelIndex;

            #ifdef TTK_ENABLE_OPENMP
            #pragma omp atomic update
            #endif
            counts[countIndex]++;
          }

          // print the progress of the current subprocedure with elapsed time
          this->printMsg("Computing Counts",
                         1, // progress
                         timer.getElapsedTime(), this->threadNumber_);

          return 1; // return success
        }
      */

  }; // ScalarFieldFromPointsNew class

} // namespace ttk
