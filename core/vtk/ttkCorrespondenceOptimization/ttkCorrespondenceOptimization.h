#pragma once

// VTK Module
#include <ttkCorrespondenceOptimizationModule.h>

// VTK Includes
#include <ttkAlgorithm.h>

#include <CorrespondenceOptimization.h>
#include <ttkMacros.h>

class TTKCORRESPONDENCEOPTIMIZATION_EXPORT ttkCorrespondenceOptimization
  : public ttkAlgorithm,
    protected ttk::CorrespondenceOptimization
{
public:
  enum class OPTIMIZATION_METHOD {
    N_SMALLEST_CORRESPONDENCES_PER_FEATURE = 0,
    N_LARGEST_CORRESPONDENCES_PER_FEATURE = 1,
    THRESHOLD_BELOW = 10,
    THRESHOLD_ABOVE = 11,
    TWO_PASS = 20
  };

private:

  OPTIMIZATION_METHOD OptimizationMethod{OPTIMIZATION_METHOD::N_LARGEST_CORRESPONDENCES_PER_FEATURE};
  int NumberOfLargestCorrespondencesPerFeature{1};
  double Threshold{0};

public:
  ttkSetEnumMacro(OptimizationMethod, OPTIMIZATION_METHOD);
  vtkGetEnumMacro(OptimizationMethod, OPTIMIZATION_METHOD);

  vtkSetMacro(NumberOfLargestCorrespondencesPerFeature, int);
  vtkGetMacro(NumberOfLargestCorrespondencesPerFeature, int);
  vtkSetMacro(Threshold, double);
  vtkGetMacro(Threshold, double);

  static ttkCorrespondenceOptimization *New();
  vtkTypeMacro(ttkCorrespondenceOptimization, ttkAlgorithm);

protected:
  ttkCorrespondenceOptimization();
  ~ttkCorrespondenceOptimization() override;

  int FillInputPortInformation(int port, vtkInformation *info) override;
  int FillOutputPortInformation(int port, vtkInformation *info) override;
  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
};
