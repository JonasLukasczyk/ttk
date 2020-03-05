#include <ttkDisambiguate.h>

#include <vtkDataObject.h> // For port info
#include <vtkObjectFactory.h> // for new macro

#include <vtkSmartPointer.h>
#include <vtkPointData.h>
#include <vtkIdTypeArray.h>

#include <vtkIntArray.h>
#include <vtkFloatArray.h>
#include <vtkDoubleArray.h>

#include <ttkMacros.h>

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

    // Get input array
    auto inputPD = input->GetPointData();
    auto inputScalars = this->GetInputArrayToProcess(0, inputVector);

    // Create output arrays
    auto outputScalars = vtkSmartPointer<vtkDataArray>::Take( inputScalars->NewInstance() );
    outputScalars->SetName( inputScalars->GetName() );
    outputScalars->SetNumberOfComponents(1);
    outputScalars->SetNumberOfTuples( nVertices );

    auto outputOffsets = vtkSmartPointer<vtkIntArray>::New();
    outputOffsets->SetName( "ttkOffsetScalarField" );
    outputOffsets->SetNumberOfComponents(1);
    outputOffsets->SetNumberOfTuples( nVertices );

    // Compute persistence-based simplification
    int status = -1;
    switch(inputScalars->GetDataType()) {
        vtkTemplateMacro(
            (status = this->removeExtremaByPersistence<int, VTK_TT>(
                (VTK_TT*) outputScalars->GetVoidPointer(0),
                (int*) outputOffsets->GetVoidPointer(0),

                triangulation,
                (VTK_TT*) inputScalars->GetVoidPointer(0),
                (VTK_TT) this->PersistenceThreshold,
                this->UseRegionBasedIterations,
                this->AddPerturbation,
                this->UseDeallocation,
                this->EscapeInterval
            ))
        );
    }
    if(!status)
        return 0;

    // Create the output
    auto output = vtkDataSet::GetData( outputVector );
    output->ShallowCopy( input );

    auto outputPD = output->GetPointData();
    outputPD->AddArray( outputOffsets );
    outputPD->AddArray( outputScalars );

    return 1;
}