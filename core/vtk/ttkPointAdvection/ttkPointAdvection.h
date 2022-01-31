/// \ingroup vtk
/// \class ttkPointAdvection
/// \author Emma Nilsson <emma.nilsson@liu.se>
/// \date 2022-01-24.
///
/// \brief TTK VTK-filter that wraps the ttk::PointAdvection module.
///
/// This VTK filter uses the ttk::PointAdvection module to generate points in
/// the unit cube and advects the points in a chosen vector.
///
/// \param Output vtkPolyData.
///
/// This filter can be used as any other VTK filter (for instance, by using the
/// sequence of calls SetInputData(), Update(), GetOutputDataObject()).
///
/// The input data array needs to be specified via the standard VTK call
/// vtkAlgorithm::SetInputArrayToProcess() with the following parameters:
/// \param idx 0 (FIXED: the first array the algorithm requires)
/// \param port 0 (FIXED: first port)
/// \param connection 0 (FIXED: first connection)
/// \param fieldAssociation 0 (FIXED: point data)
/// \param arrayName (DYNAMIC: string identifier of the input array)
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
  // Keeps track of Points for each timestep
  std::vector<std::vector<ttk::PointAdvection::Point>> pointsPerTimestep;

  // Point parameters
  int nPoints{0};
  int RandomSeed{0};
  double PointWeight[2]{0.0, 1.0};
  double PointConstant[2]{0.0, 1.0};
  double Birth[2]{0.0, 1.0};
  double Death[2]{0.0, 1.0};

  // Advection parameters
  int nTimesteps{0};
  double TimeInterval{0.0};
  double StepLength{0.0};
  double PerlinScaleFactor{0.0};
  int VecField{0};

public:
  // Properties macros
  vtkSetMacro(nPoints, int);
  vtkGetMacro(nPoints, int);

  vtkSetMacro(RandomSeed, int);
  vtkGetMacro(RandomSeed, int);

  vtkSetVector2Macro(PointWeight, double);
  vtkGetVector2Macro(PointWeight, double);

  vtkSetVector2Macro(PointConstant, double);
  vtkGetVector2Macro(PointConstant, double);

  vtkSetVector2Macro(Birth, double);
  vtkGetVector2Macro(Birth, double);

  vtkSetVector2Macro(Death, double);
  vtkGetVector2Macro(Death, double);

  vtkSetMacro(nTimesteps, int);
  vtkGetMacro(nTimesteps, int);

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

  int initializePoints();

  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
};
