/// \ingroup vtk
/// \class ttkForEach
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 01.01.2019
///
/// \brief TTK VTK-filter that iterates over points, cells, blocks, rows, or field data arrays.
///
/// This filter works in conjunction with the ttkEndFor filter to iterate over points, cells, blocks, rows, or field data arrays
///
/// \param Input vktObject either a vtkMultiBlockDataSet, vtkTable, or vtkDataSet
/// \param Output vktObject one element of the input

#pragma once

// VTK Module
#include <ttkForEachModule.h>

// TTK includes
#include <ttkExtract.h>

class TTKFOREACH_EXPORT ttkForEach : public ttkExtract {

    public:
        static ttkForEach* New();
        vtkTypeMacro(ttkForEach, ttkExtract);

    protected:
        ttkForEach();
        ~ttkForEach();

        int RequestInformation(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;
        int RequestData(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;
};