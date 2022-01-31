/// TODO 1: Provide your information
///
/// \ingroup base
/// \class ttk::PointAdvection
/// \author Your Name Here <Your Email Address Here>
/// \date The Date Here.
///
/// This module defines the %PointAdvection class that computes for each vertex
/// of a triangulation the average scalar value of itself and its direct
/// neighbors.
///
/// \b Related \b publication: \n
/// '???'
///
///

#pragma once

// ttk common includes
#include <Debug.h>
#include <PerlinNoise.h>
#include <Triangulation.h>

namespace ttk {

  /**
   * The PointAdvection class provides methods to advect points in a vector
   * field.
   */
  class PointAdvection : virtual public Debug {

  public:
    PointAdvection();

    // Struct Point used for integration along the vector field
    struct Point {
      int pointId{-1};
      int timestep{-1};
      int birth{-1};
      int death{-1};
      double x{0.0};
      double y{0.0};
      double z{0.0};
      double v[3]{0.0, 0.0, 0.0};
      double weight{0.0};
      double constant{0.0};
      double rate{0.0};
      bool outsideDomain{false};

      Point() {
      }

      Point(const double &px, const double &py, const double &pz) {
        x = px;
        y = py;
        z = pz;
      }

      Point(const Point &p) {
        pointId = p.pointId;
        timestep = p.timestep;
        birth = p.birth;
        death = p.death;
        x = p.x;
        y = p.y;
        z = p.z;
        v[0] = p.v[0];
        v[1] = p.v[1];
        v[2] = p.v[2];
        weight = p.weight;
        constant = p.constant;
        rate = p.rate;
        outsideDomain = p.outsideDomain;
      }

      Point &operator=(const Point &p) {
        pointId = p.pointId;
        timestep = p.timestep;
        birth = p.birth;
        death = p.death;
        x = p.x;
        y = p.y;
        z = p.z;
        v[0] = p.v[0];
        v[1] = p.v[1];
        v[2] = p.v[2];
        weight = p.weight;
        constant = p.constant;
        rate = p.rate;
        outsideDomain = p.outsideDomain;
        return *this;
      }

      Point operator+(const Point &a) const {
        Point p;
        p.pointId = pointId;
        p.timestep = timestep;
        p.birth = birth;
        p.death = death;
        p.weight = weight;
        p.constant = constant;
        p.rate = rate;
        p.outsideDomain = outsideDomain;
        p.x = x + a.x;
        p.y = y + a.y;
        p.z = z + a.z;

        return p;
      }

      Point operator*(double k) {
        Point p;
        p.pointId = pointId;
        p.timestep = timestep;
        p.birth = birth;
        p.death = death;
        p.weight = weight;
        p.constant = constant;
        p.rate = rate;
        p.outsideDomain = outsideDomain;
        p.x = k * x;
        p.y = k * y;
        p.z = k * z;
        return p;
      }

      void setVelocity(const double vel[3]) {
        v[0] = vel[0];
        v[1] = vel[1];
        v[2] = vel[2];
      }
    };

    // Class used to determine which vector fields exist and can be used
    enum class VectorField {
      PerlinPerturbed,
      PerlinGradient,
      PosDiagonal,
      PosX
    };

    void setStepLength(const double stepLength) {
      h_ = stepLength;
    }

    void setPerlinScaleFactor(const double psf) {
      psf_[0] = psf;
      psf_[1] = psf;
      psf_[2] = psf;
    }

    void setVectorField(const PointAdvection::VectorField &vf) {
      vf_ = vf;
    }

    Point sampleVectorField(const Point &p, double t) {
      Point dv(p);
      switch(vf_) {
        case PointAdvection::VectorField::PerlinPerturbed: {
          // Use perturbation in the 3D domain for the vector field
          pn.perlin4D<double>(
            p.x / psf_[0], p.y / psf_[1], p.z / psf_[2], t, dv.x);
          pn.perlin4D<double>(
            p.z / psf_[2], p.x / psf_[0], p.y / psf_[1], t, dv.y);
          pn.perlin4D<double>(
            p.y / psf_[1], p.z / psf_[2], p.x / psf_[0], t, dv.z);
          break;
        }
        case PointAdvection::VectorField::PerlinGradient: {
          // Calculate gradient using finite differences
          double s1, s2 = 0.0;
          pn.perlin4D<double>(
            (p.x - h_) / psf_[0], p.y / psf_[1], p.z / psf_[2], t, s1);
          pn.perlin4D<double>(
            (p.x + h_) / psf_[0], p.y / psf_[1], p.z / psf_[2], t, s2);
          dv.x = (s2 - s1) / (2 * h_);

          pn.perlin4D<double>(
            p.x / psf_[0], (p.y - h_) / psf_[1], p.z / psf_[2], t, s1);
          pn.perlin4D<double>(
            p.x / psf_[0], (p.y + h_) / psf_[1], p.z / psf_[2], t, s2);
          dv.y = (s2 - s1) / (2 * h_);

          pn.perlin4D<double>(
            p.x / psf_[0], p.y / psf_[1], (p.z - h_) / psf_[2], t, s1);
          pn.perlin4D<double>(
            p.x / psf_[0], p.y / psf_[1], (p.z + h_) / psf_[2], t, s2);
          dv.z = (s2 - s1) / (2 * h_);
          break;
        }
        case PointAdvection::VectorField::PosDiagonal: {
          // Go (1, 1, 1) along positive diagonal
          dv.x = 1.0;
          dv.y = 1.0;
          dv.z = 1.0;
          break;
        }
        case PointAdvection::VectorField::PosX: {
          // Go (1, 0, 0)
          dv.x = 1.0;
          dv.y = 0.0;
          dv.z = 0.0;
        }
      }

      return dv;
    }

    int RK4(Point &prevP, Point &newP, double time) {
      // RK4 integration
      Point q1 = sampleVectorField(prevP, time) * h_;
      Point q2 = sampleVectorField(prevP + (q1 * 0.5), time) * h_;
      Point q3 = sampleVectorField(prevP + (q2 * 0.5), time) * h_;
      Point q4 = sampleVectorField(prevP + q3, time) * h_;

      Point vel = (q1 + q2 * 2 + q3 * 2 + q4) * (1.0 / 6);
      newP = prevP + vel;

      // Set velocity of previous point
      double v[3] = {vel.x, vel.y, vel.z};
      prevP.setVelocity(v);

      return 1;
    }

    template <class dataType>
    int integrate(std::vector<std::vector<Point>> &outPoints,
                  const int nTimesteps,
                  const double timeInterval,
                  const double stepLength,
                  const double psf,
                  const VectorField &vf) {
      // Set class variables
      setStepLength(stepLength);
      setPerlinScaleFactor(psf);
      setVectorField(vf);

      // Integrate the paths of the initial points by moving the points
      // along the vector field for all timesteps
      ttk::Timer timer;
      this->printMsg("Integrating " + std::to_string(outPoints[0].size())
                       + " particles along vector field",
                     0, 0, this->threadNumber_, debug::LineMode::REPLACE);

      for(int i = 0; i < nTimesteps - 1; i++) {
        std::vector<Point> &curPoints = outPoints[i];
        double time = i * timeInterval;
        for(size_t j = 0; j < outPoints[i].size(); j++) {
          Point newP;
          auto &curPoint = curPoints[j];

          // Integrate using RK4
          RK4(curPoint, newP, time);

          // Add point to the next time-step
          newP.timestep = i + 1;
          newP.pointId = curPoint.pointId;
          outPoints[i + 1].push_back(newP);
        }
      }

      this->printMsg("Integrating " + std::to_string(outPoints[0].size())
                       + " particles along vector field",
                     1, timer.getElapsedTime(), this->threadNumber_);

      return 1;
    }

  protected:
    double h_{};
    double psf_[3]{};
    VectorField vf_{};
    PerlinNoise pn;
  }; // PointAdvection class

} // namespace ttk
