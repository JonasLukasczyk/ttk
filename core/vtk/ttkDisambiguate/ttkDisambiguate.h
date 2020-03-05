/// \ingroup vtk
/// \class ttkDisambiguate
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 01.09.2019.
///
/// \brief TTK VTK-filter that wraps the ttk::Disambiguate module.
///
/// This VTK filter uses the ttk::Disambiguate module to compute the bounding box of a vtkDataSet, which is returned as a vtkUnstructuredGrid.
///
/// \param Input vtkDataSet whose bounding box will be computed.
/// \param Output vtkUnstructuredGrid that corresponds to bounding box of the input.
///
/// This filter can be used as any other VTK filter (for instance, by using the
/// sequence of calls SetInputData(), Update(), GetOutputDataObject()).
///
/// See the related ParaView example state files for usage examples within a
/// VTK pipeline.
///
/// \sa ttk::Disambiguate
/// \sa ttkAlgorithm

#pragma once

// VTK Module
#include <ttkDisambiguateModule.h>

// VTK Includes
#include <ttkAlgorithm.h>

// TTK Base Includes
#include <Disambiguate.h>

class TTKDISAMBIGUATE_EXPORT ttkDisambiguate
    : public ttkAlgorithm    // we inherit from the generic ttkAlgorithm class
    , public ttk::Disambiguate // and we inherit from the base class
{
    private:
        bool AddPerturbation{false};
        bool UseRegionBasedIterations{false};
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

        static ttkDisambiguate *New();
        vtkTypeMacro(ttkDisambiguate, ttkAlgorithm);

    protected:
        ttkDisambiguate();
        ~ttkDisambiguate() override;

        int FillInputPortInformation(int port, vtkInformation* info) override;
        int FillOutputPortInformation(int port, vtkInformation* info) override;
        int RequestData(
            vtkInformation* request,
            vtkInformationVector** inputVector,
            vtkInformationVector* outputVector
        ) override;
};