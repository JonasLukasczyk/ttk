/// TODO 4: Provide your information
///
/// \ingroup vtk
/// \class ttkLevelSets
/// \author Your Name Here <Your Email Address Here>
/// \date The Date Here.
///
/// \brief TTK VTK-filter that wraps the ttk::LevelSets module.
///
/// This VTK filter uses the ttk::LevelSets module to compute the bounding box
/// of a vtkDataSet, which is returned as a vtkUnstructuredGrid.
///
/// \param Input vtkDataSet whose bounding box will be computed.
/// \param Output vtkUnstructuredGrid that corresponds to bounding box of the
/// input.
///
/// This filter can be used as any other VTK filter (for instance, by using the
/// sequence of calls SetInputData(), Update(), GetOutputDataObject()).
///
/// See the related ParaView example state files for usage examples within a
/// VTK pipeline.
///
/// \sa ttk::LevelSets
/// \sa ttkAlgorithm

#pragma once

// VTK Module
#include <ttkLevelSetsModule.h>

// VTK Includes
#include <ttkAlgorithm.h>

class TTKLEVELSETS_EXPORT ttkLevelSets : public ttkAlgorithm {
private:
  std::string Expression{"0"};

public:
  vtkSetMacro(Expression, std::string);
  vtkGetMacro(Expression, std::string);

  static ttkLevelSets *New();
  vtkTypeMacro(ttkLevelSets, ttkAlgorithm);

protected:
  ttkLevelSets();
  ~ttkLevelSets() override;

  int FillInputPortInformation(int port, vtkInformation *info) override;
  int FillOutputPortInformation(int port, vtkInformation *info) override;

  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
};
