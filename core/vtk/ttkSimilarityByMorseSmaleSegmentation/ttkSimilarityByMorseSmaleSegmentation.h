#pragma once

// VTK Module
#include <ttkSimilarityByMorseSmaleSegmentationModule.h>

// VTK Includes
#include <ttkSimilarityAlgorithm.h>

// TTK Base Includes
#include <SimilarityByMorseSmaleSegmentation.h>

class TTKSIMILARITYBYMORSESMALESEGMENTATION_EXPORT ttkSimilarityByMorseSmaleSegmentation
  : public ttkSimilarityAlgorithm,
    protected ttk::SimilarityByMorseSmaleSegmentation {

private:
  bool NormalizeMatrix{true};

public:
  static ttkSimilarityByMorseSmaleSegmentation *New();
  vtkTypeMacro(ttkSimilarityByMorseSmaleSegmentation, ttkSimilarityAlgorithm);

  vtkSetMacro(NormalizeMatrix, bool);
  vtkGetMacro(NormalizeMatrix, bool);

protected:
  ttkSimilarityByMorseSmaleSegmentation();
  ~ttkSimilarityByMorseSmaleSegmentation();

  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;

};
