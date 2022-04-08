#pragma once

// VTK Module
#include <ttkUmbrellaClusteringModule.h>

// VTK Includes
#include <ttkSimilarityAlgorithm.h>
#include <vtkPolyData.h>

// TTK Base Includes
#include <UmbrellaClustering.h>

// std includes
#include <map>
#include <utility>
#include <vector>

class TTKUMBRELLACLUSTERING_EXPORT ttkUmbrellaClustering
  : public ttkSimilarityAlgorithm,
    protected ttk::UmbrellaClustering {

private:
  int Kernel{0};
  double ScalarThreshold{0.0};

  // For clustering points into umbrellas
  std::vector<std::map<int, std::vector<int>>> umbrellasPerTimestep;
  std::vector<std::map<int, std::vector<int>>> threshUmbrellasPerTimestep;

public:
  vtkSetMacro(Kernel, int);
  vtkGetMacro(Kernel, int);

  vtkSetMacro(ScalarThreshold, double);
  vtkGetMacro(ScalarThreshold, double);

  static ttkUmbrellaClustering *New();
  vtkTypeMacro(ttkUmbrellaClustering, ttkSimilarityAlgorithm);

protected:
  ttkUmbrellaClustering();
  ~ttkUmbrellaClustering();

  int FillOutputPortInformation(int port, vtkInformation *info) override;

  int ComputeSimilarityMatrix(vtkImageData *similarityMatrix,
                              vtkDataObject *inputDataObjects0,
                              vtkDataObject *inputDataObjects1) override;

  int ComputeThresholdedClustering(vtkImageData *similarityMatrix,
                                   vtkDataObject *inputDataObjects0,
                                   vtkDataObject *inputDataObjects1);

  int AddUmbrellaIds(vtkDataObject *inputDataObjects, const size_t t);

  int FormatClusters(vtkDataObject *inputDataObjects,
                     vtkPolyData *outputPoints,
                     const size_t t);

  int FormatThresholdedClusters(vtkDataObject *inputDataObjects,
                                vtkPolyData *outputPoints,
                                const size_t t);

  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector);
};
