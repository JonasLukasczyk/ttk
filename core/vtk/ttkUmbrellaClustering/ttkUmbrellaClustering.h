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

  // For clustering points into umbrellas
  std::vector<std::map<int, std::vector<int>>> umbrellasPerTimestep;

public:
  vtkSetMacro(Kernel, int);
  vtkGetMacro(Kernel, int);
  static ttkUmbrellaClustering *New();
  vtkTypeMacro(ttkUmbrellaClustering, ttkSimilarityAlgorithm);

protected:
  ttkUmbrellaClustering();
  ~ttkUmbrellaClustering();

  int FillOutputPortInformation(int port, vtkInformation *info) override;

  int ComputeSimilarityMatrix(vtkImageData *similarityMatrix,
                              vtkDataObject *inputDataObjects0,
                              vtkDataObject *inputDataObjects1) override;

  int AddUmbrellaIds(vtkDataObject *inputDataObjects, const size_t t);

  int FormatClusters(vtkDataObject *inputDataObjects,
                     vtkPolyData *outputPoints,
                     const size_t t);

  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector);
};
