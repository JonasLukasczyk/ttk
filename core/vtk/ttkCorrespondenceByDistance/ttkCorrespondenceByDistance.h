#pragma once

// VTK Module
#include <ttkCorrespondenceByDistanceModule.h>

// VTK Includes
#include <ttkCorrespondenceAlgorithm.h>

// TTK Base Includes
#include <CorrespondenceByDistance.h>

class TTKCORRESPONDENCEBYDISTANCE_EXPORT ttkCorrespondenceByDistance
  : public ttkCorrespondenceAlgorithm,
    protected ttk::CorrespondenceByDistance {

private:
  bool NormalizeMatrix{true};

public:
  static ttkCorrespondenceByDistance *New();
  vtkTypeMacro(ttkCorrespondenceByDistance, ttkCorrespondenceAlgorithm);

  vtkSetMacro(NormalizeMatrix, bool);
  vtkGetMacro(NormalizeMatrix, bool);

protected:
  ttkCorrespondenceByDistance();
  ~ttkCorrespondenceByDistance();

  int ComputeCorrespondences(vtkImageData *correspondenceMatrix,
                             vtkDataObject *inputDataObjects0,
                             vtkDataObject *inputDataObjects1) override;
};
