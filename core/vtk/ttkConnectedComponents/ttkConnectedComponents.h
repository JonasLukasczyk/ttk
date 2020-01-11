/// \ingroup vtk
/// \class ttkConnectedComponents
/// \author Jonas Lukasczyk (jl@jluk.de)
/// \date 01.02.2019
///
/// \brief TTK VTK-filter that TODO
///
/// VTK wrapping code for the @ConnectedComponents package.
///
/// This filter TODO
///
/// \sa ttk::ConnectedComponents

#pragma once

// VTK Module
#include <ttkConnectedComponentsModule.h>

// VTK Includes
#include <ttkAlgorithm.h>

// TTK Base Includes
#include <ConnectedComponents.h>

class TTKCONNECTEDCOMPONENTS_EXPORT ttkConnectedComponents
  : public ttkAlgorithm
  , public ttk::ConnectedComponents
{
    public:
        static ttkConnectedComponents* New();
        vtkTypeMacro(ttkConnectedComponents, ttkAlgorithm);

    protected:

        ttkConnectedComponents();
        ~ttkConnectedComponents();

      int FillInputPortInformation(int port, vtkInformation *info) override;
      int FillOutputPortInformation(int port, vtkInformation *info) override;
      int RequestData(vtkInformation *request,
                      vtkInformationVector **inputVector,
                      vtkInformationVector *outputVector) override;
};
