#pragma once

// VTK Module
#include <ttkSimilarityByGradientModule.h>

// VTK Includes
#include <ttkSimilarityAlgorithm.h>

// TTK Base Includes
#include <SimilarityByGradient.h>

class TTKSIMILARITYBYGRADIENT_EXPORT ttkSimilarityByGradient
  : public ttkSimilarityAlgorithm,
    protected ttk::SimilarityByGradient {

public:
  static ttkSimilarityByGradient *New();
  vtkTypeMacro(ttkSimilarityByGradient, ttkSimilarityAlgorithm);

protected:
  ttkSimilarityByGradient();
  ~ttkSimilarityByGradient();

  int ComputeSimilarityMatrix(vtkImageData *similarityMatrix,
                             vtkDataObject *inputDataObjects0,
                             vtkDataObject *inputDataObjects1) override;
};