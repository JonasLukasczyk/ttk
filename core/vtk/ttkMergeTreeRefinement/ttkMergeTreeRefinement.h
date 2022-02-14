#pragma once

// VTK Module
#include <ttkMergeTreeRefinementModule.h>

// Base Includes
#include <MergeTreeRefinement.h>

// VTK Includes
#include <ttkAlgorithm.h>

class vtkUnstructuredGrid;
class vtkDataSet;

class TTKMERGETREEREFINEMENT_EXPORT ttkMergeTreeRefinement
  : public ttkAlgorithm,
    public ttk::MergeTreeRefinement {
private:
  std::string Interval{"100"};

public:
  vtkSetMacro(Interval, std::string);
  vtkGetMacro(Interval, std::string);

  static ttkMergeTreeRefinement *New();
  vtkTypeMacro(ttkMergeTreeRefinement, ttkAlgorithm);

protected:
  ttkMergeTreeRefinement();
  ~ttkMergeTreeRefinement() override;

  int FillInputPortInformation(int port, vtkInformation *info) override;
  int FillOutputPortInformation(int port, vtkInformation *info) override;
  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
  int RefineMergeTreeAndSegmentation(vtkUnstructuredGrid *o_mergeTree,
                                     vtkDataSet *o_domain,
                                     vtkUnstructuredGrid *i_mergeTree,
                                     vtkDataSet *i_domain,
                                     const double interval);
};