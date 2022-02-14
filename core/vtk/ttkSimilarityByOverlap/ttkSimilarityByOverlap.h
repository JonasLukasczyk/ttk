#pragma once

// VTK Module
#include <ttkSimilarityByOverlapModule.h>

// VTK Includes
#include <ttkSimilarityAlgorithm.h>

// TTK Base Includes
#include <SimilarityByOverlap.h>

class TTKSIMILARITYBYOVERLAP_EXPORT ttkSimilarityByOverlap
  : public ttkSimilarityAlgorithm,
    protected ttk::SimilarityByOverlap {
public:
  static ttkSimilarityByOverlap *New();
  vtkTypeMacro(ttkSimilarityByOverlap, ttkSimilarityAlgorithm);

protected:
  ttkSimilarityByOverlap();
  ~ttkSimilarityByOverlap();

  int ComputeCorrespondences(vtkImageData *correspondenceMatrix,
                             vtkDataObject *inputDataObjects0,
                             vtkDataObject *inputDataObjects1) override;
};
