#pragma once

// VTK Module
#include <ttkMapDataModule.h>

// VTK Includes
#include <ttkAlgorithm.h>

// TTK Includes
#include <MapData.h>

class vtkDataSet;

class TTKMAPDATA_EXPORT ttkMapData : public ttkAlgorithm,
                                     protected ttk::MapData {
private:
  double MissingValue{-1};

public:
  vtkSetMacro(MissingValue, double);
  vtkGetMacro(MissingValue, double);

  static ttkMapData *New();
  vtkTypeMacro(ttkMapData, ttkAlgorithm);

protected:
  ttkMapData();
  ~ttkMapData() override;

  int ProcessSingle(vtkDataSet *output,
                    ttk::MapData::Map &map,
                    vtkDataArray *codomain);

  int FillInputPortInformation(int port, vtkInformation *info) override;
  int FillOutputPortInformation(int port, vtkInformation *info) override;
  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
};
