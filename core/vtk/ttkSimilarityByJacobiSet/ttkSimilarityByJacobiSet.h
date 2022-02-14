#pragma once

// VTK Module
#include <ttkSimilarityByJacobiSetModule.h>

// VTK Includes
#include <ttkSimilarityAlgorithm.h>

class TTKSIMILARITYBYJACOBISET_EXPORT ttkSimilarityByJacobiSet
  : public ttkSimilarityAlgorithm {

public:
  static ttkSimilarityByJacobiSet *New();
  vtkTypeMacro(ttkSimilarityByJacobiSet, ttkSimilarityAlgorithm);

protected:
  ttkSimilarityByJacobiSet();
  ~ttkSimilarityByJacobiSet();

  int ComputeCorrespondences(vtkImageData *correspondenceMatrix,
                             vtkDataObject *inputDataObjects0,
                             vtkDataObject *inputDataObjects1) override;
};
