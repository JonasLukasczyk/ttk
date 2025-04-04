#pragma once

#include <ttkMergeTreeFeaturesModule.h>

#include <ttkAlgorithm.h>

#include <MergeTreeFeatures.h>

class TTKMERGETREEFEATURES_EXPORT ttkMergeTreeFeatures
  : public ttkAlgorithm,
    protected ttk::MergeTreeFeatures
{
private:

  std::string Levels{""};

public:
  vtkSetMacro(Levels, const std::string &);
  vtkGetMacro(Levels, std::string);

  static ttkMergeTreeFeatures *New();
  vtkTypeMacro(ttkMergeTreeFeatures, ttkAlgorithm);

protected:
  ttkMergeTreeFeatures();
  ~ttkMergeTreeFeatures() override = default;

  int FillInputPortInformation(int port, vtkInformation *info) override;
  int FillOutputPortInformation(int port, vtkInformation *info) override;
  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
};
