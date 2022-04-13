/// \ingroup vtk
/// \class ttkPointAdvection
/// \author Emma Nilsson <emma.nilsson@liu.se>
/// \date 2022-01-24.
///
/// \brief TTK VTK-filter that wraps the ttk::PointAdvection module.
///
/// This VTK filter uses the ttk::PointAdvection module to generate points in
/// the unit cube and advects the points in a chosen vector field.
///
/// \param Output vtkMultiBlockData of vtkPolyData.
///
/// This filter can be used as any other VTK filter (for instance, by using the
/// sequence of calls SetInputData(), Update(), GetOutputDataObject()).
///
/// See the corresponding standalone program for a usage example:
///   - standalone/PointAdvection/main.cpp
///
/// See the related ParaView example state files for usage examples within a
/// VTK pipeline.
///
/// \sa ttk::PointAdvection
/// \sa ttkAlgorithm

#pragma once

// VTK Module
#include <ttkPointAdvectionModule.h>

// VTK Includes
#include <ttkAlgorithm.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

// TTK Base Includes
#include <PointAdvection.h>

class TTKPOINTADVECTION_EXPORT ttkPointAdvection
  : public ttkAlgorithm // we inherit from the generic ttkAlgorithm class
  ,
    protected ttk::PointAdvection // and we inherit from the base class
{
private:
  // Keeps track of Points
  std::vector<ttk::PointAdvection::Point> allPoints;

  // Point parameters
  int NumberOfPoints{1};
  int RandomSeed{0};
  double Amplitude[2]{0.0, 1.0};
  double Variance[2]{0.0, 1.0};
  int Lifetime[2]{0, 1};
  int RespawnTime[2]{0, 1};

  // Advection parameters
  int NumberOfTimesteps{1};
  double TimeInterval{0.0};
  double StepLength{0.0};
  double PerlinScaleFactor{0.0};
  int VecField{0};

public:
  // Properties macros
  vtkSetMacro(NumberOfPoints, int);
  vtkGetMacro(NumberOfPoints, int);

  vtkSetMacro(RandomSeed, int);
  vtkGetMacro(RandomSeed, int);

  vtkSetVector2Macro(Amplitude, double);
  vtkGetVector2Macro(Amplitude, double);

  vtkSetVector2Macro(Variance, double);
  vtkGetVector2Macro(Variance, double);

  vtkSetVector2Macro(Lifetime, int);
  vtkGetVector2Macro(Lifetime, int);

  vtkSetVector2Macro(RespawnTime, int);
  vtkGetVector2Macro(RespawnTime, int);

  vtkSetMacro(NumberOfTimesteps, int);
  vtkGetMacro(NumberOfTimesteps, int);

  vtkSetMacro(TimeInterval, double);
  vtkGetMacro(TimeInterval, double);

  vtkSetMacro(StepLength, double);
  vtkGetMacro(StepLength, double);

  vtkSetMacro(PerlinScaleFactor, double);
  vtkGetMacro(PerlinScaleFactor, double);

  vtkSetMacro(VecField, int);
  vtkGetMacro(VecField, int);

  static ttkPointAdvection *New();
  vtkTypeMacro(ttkPointAdvection, ttkAlgorithm);

protected:
  ttkPointAdvection();
  ~ttkPointAdvection() override;

  int FillInputPortInformation(int port, vtkInformation *info) override;
  int FillOutputPortInformation(int port, vtkInformation *info) override;

  int rampFunction(const int t, const int lifetime, double &y);
  int formatOutput(vtkPolyData *pd,
                   std::vector<int> &aliveIds,
                   const int timestep);

  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
};
