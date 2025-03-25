#pragma once

// VTK Module
#include <ttkSimilarityByEuclideanDistanceModule.h>

// VTK Includes
#include <ttkSimilarityAlgorithm.h>

// TTK Base Includes
#include <SimilarityByEuclideanDistance.h>

class TTKSIMILARITYBYEUCLIDEANDISTANCE_EXPORT ttkSimilarityByEuclideanDistance
  : public ttkSimilarityAlgorithm,
    protected ttk::SimilarityByEuclideanDistance {

private:
  bool NormalizeMatrix{true};

public:
  static ttkSimilarityByEuclideanDistance *New();
  vtkTypeMacro(ttkSimilarityByEuclideanDistance, ttkSimilarityAlgorithm);

  vtkSetMacro(NormalizeMatrix, bool);
  vtkGetMacro(NormalizeMatrix, bool);

protected:
  ttkSimilarityByEuclideanDistance();
  ~ttkSimilarityByEuclideanDistance();

  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;

};
