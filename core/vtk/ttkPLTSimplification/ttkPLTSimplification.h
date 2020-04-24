#pragma once

// VTK Module
#include <ttkPLTSimplificationModule.h>

// VTK Includes
#include <ttkAlgorithm.h>

// TTK Base Includes
#include <PLTSimplification.h>

class TTKPLTSIMPLIFICATION_EXPORT ttkPLTSimplification
    : public ttkAlgorithm    // we inherit from the generic ttkAlgorithm class
    , public ttk::PLTSimplification // and we inherit from the base class
{
    private:
        bool AddPerturbation{false};
        bool UseRegionBasedIterations{true};
        bool UseDeallocation{false};
        double PersistenceThreshold{0};
        int EscapeInterval{1000};

    public:
        vtkSetMacro(AddPerturbation, bool);
        vtkGetMacro(AddPerturbation, bool);

        vtkSetMacro(UseRegionBasedIterations, bool);
        vtkGetMacro(UseRegionBasedIterations, bool);

        vtkSetMacro(UseDeallocation, bool);
        vtkGetMacro(UseDeallocation, bool);

        vtkGetMacro(PersistenceThreshold,double);
        vtkSetMacro(PersistenceThreshold,double);

        vtkGetMacro(EscapeInterval,int);
        vtkSetMacro(EscapeInterval,int);

        static ttkPLTSimplification *New();
        vtkTypeMacro(ttkPLTSimplification, ttkAlgorithm);

    protected:
        ttkPLTSimplification();
        ~ttkPLTSimplification() override;

        int FillInputPortInformation(int port, vtkInformation* info) override;
        int FillOutputPortInformation(int port, vtkInformation* info) override;
        int RequestData(
            vtkInformation* request,
            vtkInformationVector** inputVector,
            vtkInformationVector* outputVector
        ) override;
};