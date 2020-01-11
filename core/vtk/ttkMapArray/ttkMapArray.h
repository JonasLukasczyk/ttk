/// \ingroup vtk
/// \class ttkMapArray
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 1.12.2018
///
/// \brief TTK VTK-filter that maps scalar fields of a source to a target.
///
/// This filter maps point/cell/field data of a "Source" to point/cell data of a "target" by utilizing a map. First, the filter generates a map by mapping values of one or more scalar fields ("Domain Fields") of the source to values of one of the source's scalar fields ("Codomain Field"). Next, the filter uses one or more scalar fields of the target ("Lookup Fields") to query the map. The mapped values are then attached as new a new scalar field of the target.
///
/// \param Input
/// \param Output

#pragma once

// VTK Module
#include <ttkMapArrayModule.h>

// VTK Includes
#include <ttkAlgorithm.h>

class TTKMAPARRAY_EXPORT ttkMapArray : public ttkAlgorithm {
    public:
        static ttkMapArray* New();
        vtkTypeMacro(ttkMapArray, ttkAlgorithm);

        enum class KeyMode {
            Disabled   = 0,
            Const      = 1,
            BlockIndex = 2,
            Field      = 3
        };

        vtkSetMacro(Implementation, int);
        vtkGetMacro(Implementation, int);

        vtkSetMacro(Key0Mode, int);
        vtkGetMacro(Key0Mode, int);
        vtkSetMacro(Key1Mode, int);
        vtkGetMacro(Key1Mode, int);
        vtkSetMacro(Key2Mode, int);
        vtkGetMacro(Key2Mode, int);
        vtkSetMacro(Key0Const, std::string);
        vtkGetMacro(Key0Const, std::string);
        vtkSetMacro(Key1Const, std::string);
        vtkGetMacro(Key1Const, std::string);
        vtkSetMacro(Key2Const, std::string);
        vtkGetMacro(Key2Const, std::string);

        vtkSetMacro(TargetDomain, int);
        vtkGetMacro(TargetDomain, int);

        vtkSetMacro(Lookup0Mode, int);
        vtkGetMacro(Lookup0Mode, int);
        vtkSetMacro(Lookup1Mode, int);
        vtkGetMacro(Lookup1Mode, int);
        vtkSetMacro(Lookup2Mode, int);
        vtkGetMacro(Lookup2Mode, int);
        vtkSetMacro(Lookup0Const, std::string);
        vtkGetMacro(Lookup0Const, std::string);
        vtkSetMacro(Lookup1Const, std::string);
        vtkGetMacro(Lookup1Const, std::string);
        vtkSetMacro(Lookup2Const, std::string);
        vtkGetMacro(Lookup2Const, std::string);

        vtkSetMacro(DefaultValue, double);
        vtkGetMacro(DefaultValue, double);

protected:
  ttkMapArray();
  ~ttkMapArray() override;

  int FillInputPortInformation(int port, vtkInformation *info) override;
  int FillOutputPortInformation(int port, vtkInformation *info) override;
  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;

private:
    int Implementation = 0;

    int                        Key0Mode = 0;
    std::string                Key0Const = "";
    std::pair<int,std::string> Key0Array;
    int                        Key1Mode = 0;
    std::string                Key1Const = "";
    std::pair<int,std::string> Key1Array;
    int                        Key2Mode = 0;
    std::string                Key2Const = "";
    std::pair<int,std::string> Key2Array;

    std::pair<int,std::string> ValueArray;

    int TargetDomain = 0;
    int                        Lookup0Mode = 0;
    std::string                Lookup0Const = "";
    std::pair<int,std::string> Lookup0Array;
    int                        Lookup1Mode = 0;
    std::string                Lookup1Const = "";
    std::pair<int,std::string> Lookup1Array;
    int                        Lookup2Mode = 0;
    std::string                Lookup2Const = "";
    std::pair<int,std::string> Lookup2Array;
    double DefaultValue = 0;
};