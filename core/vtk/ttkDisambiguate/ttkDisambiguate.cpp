#include <ttkDisambiguate.h>

#include <vtkDataObject.h> // For port info
#include <vtkObjectFactory.h> // for new macro

#include <vtkSmartPointer.h>
#include <vtkPointData.h>
#include <vtkIdTypeArray.h>

#include <vtkIntArray.h>
#include <vtkFloatArray.h>
#include <vtkDoubleArray.h>


vtkStandardNewMacro(ttkDisambiguate);

ttkDisambiguate::ttkDisambiguate(){
    this->SetNumberOfInputPorts(1);
    this->SetNumberOfOutputPorts(1);
}

ttkDisambiguate::~ttkDisambiguate(){}

int ttkDisambiguate::FillInputPortInformation(int port, vtkInformation* info) {
    if (port==0)
        info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    else
        return 0;
    return 1;
}

int ttkDisambiguate::FillOutputPortInformation(int port, vtkInformation* info) {
    if (port==0)
        info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 0);
    else
        return 0;
    return 1;
}

int ttkDisambiguate::RequestData(
    vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector
){
    // Get the input
    auto input = vtkDataSet::GetData( inputVector[0] );
    size_t nVertices = input->GetNumberOfPoints();

    // Get triangulation of the input object (will create one if does not exist already)
    auto triangulation = ttkAlgorithm::GetTriangulation( input );

    // Precondition triangulation
    this->PreconditionTriangulation( triangulation );

    auto inputPD = input->GetPointData();
    auto scalars = this->GetInputArrayToProcess(0, inputVector);

    auto offsetScalarField = vtkSmartPointer<vtkIntArray>::New();
    offsetScalarField->SetName( "Offsets" );
    offsetScalarField->SetNumberOfComponents(1);
    offsetScalarField->SetNumberOfTuples( nVertices );

    auto temp2 = vtkSmartPointer<vtkIntArray>::New();
    temp2->SetName( "Iteration" );
    temp2->SetNumberOfComponents(1);
    temp2->SetNumberOfTuples( nVertices );

    auto temp = vtkSmartPointer<vtkFloatArray>::New();
    temp->SetName( "ShortestPaths" );
    temp->SetNumberOfComponents(1);
    temp->SetNumberOfTuples( nVertices );

    // Compute Segmentation Mask
    {
        int status = -1;
        switch (scalars->GetDataType()) {
            vtkTemplateMacro(
                status = this->DisambiguateAllPlateaus(
                    (int*) offsetScalarField->GetVoidPointer(0),
                    (float*) temp->GetVoidPointer(0),
                    (int*) temp2->GetVoidPointer(0),

                    nVertices,
                    triangulation,
                    (VTK_TT*) scalars->GetVoidPointer(0)
                )
            );
        }
        if(!status)
            return 0;
    }

    // Get the output
    auto output = vtkDataSet::GetData( outputVector );
    output->ShallowCopy( input );

    auto outputPD = output->GetPointData();
    outputPD->AddArray( offsetScalarField );
    outputPD->AddArray( temp );
    outputPD->AddArray( temp2 );

    return 1;
}