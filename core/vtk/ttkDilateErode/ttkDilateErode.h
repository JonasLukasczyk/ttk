/// \ingroup vtk
/// \class ttkDilateErode
/// \author Jonas Lukasczyk (jl@jluk.de)
/// \date 01.02.2019
///
/// \brief TTK VTK-filter that TODO
///
/// VTK wrapping code for the @DilateErode package.
///
/// This filter TODO
///
/// \sa ttk::DilateErode

#pragma once

// VTK Module
#include <ttkDilateErodeModule.h>

// VTK includes
#include <ttkAlgorithm.h>

// TTK includes
#include <DilateErode.h>

class TTKDILATEERODE_EXPORT ttkDilateErode
  : public ttkAlgorithm,
    public ttk::DilateErode {

    private:
        int Mode{0};
        double Value{0};

    public:
        static ttkDilateErode* New();
        vtkTypeMacro(ttkDilateErode, ttkAlgorithm);

        vtkSetMacro(Mode, int);
        vtkGetMacro(Mode, int);
        vtkSetMacro(Value, double);
        vtkGetMacro(Value, double);

    protected:

        ttkDilateErode();
        ~ttkDilateErode();

      int FillInputPortInformation(int port, vtkInformation *info) override;
      int FillOutputPortInformation(int port, vtkInformation *info) override;

      int RequestData(vtkInformation *request,
                      vtkInformationVector **inputVector,
                      vtkInformationVector *outputVector) override;

};
