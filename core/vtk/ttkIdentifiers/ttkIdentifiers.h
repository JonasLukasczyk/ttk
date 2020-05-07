/// \ingroup vtk
/// \class ttkIdentifiers
/// \author Julien Tierny <julien.tierny@lip6.fr>
/// \date August 2015.
///
/// \brief TTK VTK-filter that computes the global identifiers for each vertex
/// and each cell as point data and cell data scalar fields.
///
/// This filter is useful to retrieve the global identifiers of vertices or
/// cells in subsequent filters throughout the VTK pipeline.
///
/// \param Input Input data-set (vtkDataSet)
/// \param Output Output data-set with identifier fields (vtkDataSet)
///
/// This filter can be used as any other VTK filter (for instance, by using the
/// sequence of calls SetInputData(), Update(), GetOutput()).
///
/// See the related ParaView example state files for usage examples within a
/// VTK pipeline.
///

#pragma once

#include <ttkIdentifiersModule.h>
#include <ttkAlgorithm.h>

class TTKIDENTIFIERS_EXPORT ttkIdentifiers : public ttkAlgorithm {

private:
    std::string VertexFieldName{ttk::VertexScalarFieldName};
    std::string CellFieldName{"CellIdentifiers"};

public:
  vtkSetMacro(CellFieldName, std::string);
  vtkGetMacro(CellFieldName, std::string);

  vtkSetMacro(VertexFieldName, std::string);
  vtkGetMacro(VertexFieldName, std::string);

  vtkTypeMacro(ttkIdentifiers, ttkAlgorithm);
  static ttkIdentifiers *New();

protected:
  ttkIdentifiers();
  ~ttkIdentifiers();

  int FillInputPortInformation(int port, vtkInformation *info) override;
  int FillOutputPortInformation(int port, vtkInformation *info) override;

  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
};
