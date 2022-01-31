/// TODO 4: Provide your information
///
/// \ingroup vtk
/// \class ttkScalarFieldFromPointsNew
/// \author Emma Nilsson <emma.nilsson@liu.se>
/// \date 2021-10-11.
///
/// \brief TTK VTK-filter that wraps the ttk::ScalarFieldFromPointsNew module.
///
/// This VTK filter uses the ttk::ScalarFieldFromPointsNew module to compute the
/// scalar field from a set of points for each timestep in the timeseries.
///
/// \param Input vtkMultiBlockDataSet of vtkPolyData representing integrated
/// path lines, from which a scalar field is computed. \param Output
/// vtkMultiBlockDataSet of vtkImageData representing scalar fields.
///
/// \sa ttk::ScalarFieldFromPointsNew
/// \sa ttkAlgorithm

#pragma once

// VTK Module
#include <ttkScalarFieldFromPointsNewModule.h>

// VTK Includes
#include <ttkAlgorithm.h>

// TTK Base Includes
#include <ScalarFieldFromPointsNew.h>

class TTKSCALARFIELDFROMPOINTSNEW_EXPORT ttkScalarFieldFromPointsNew
  : public ttkAlgorithm // we inherit from the generic ttkAlgorithm class
  ,
    protected ttk::ScalarFieldFromPointsNew // and we inherit from the base
                                            // class
{
private:
  double ImageBounds[6]{0, 1, 0, 1, 0, 1};
  double Resolution[3]{1, 1, 1};
  double Bandwidth{1};
  int Kernel{0};

public:
  vtkSetVector6Macro(ImageBounds, double);
  vtkGetVector6Macro(ImageBounds, double);
  vtkSetVector3Macro(Resolution, double);
  vtkGetVector3Macro(Resolution, double);
  vtkSetMacro(Bandwidth, double);
  vtkGetMacro(Bandwidth, double);
  vtkSetMacro(Kernel, int);
  vtkGetMacro(Kernel, int);

  static ttkScalarFieldFromPointsNew *New();
  vtkTypeMacro(ttkScalarFieldFromPointsNew, ttkAlgorithm);

protected:
  ttkScalarFieldFromPointsNew();
  ~ttkScalarFieldFromPointsNew() override;

  int FillInputPortInformation(int port, vtkInformation *info) override;
  int FillOutputPortInformation(int port, vtkInformation *info) override;
  int RequestInformation(vtkInformation *request,
                         vtkInformationVector **inputVector,
                         vtkInformationVector *outputVector);
  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
};
