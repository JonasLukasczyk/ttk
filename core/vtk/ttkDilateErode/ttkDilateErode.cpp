#include <ttkDilateErode.h>

#include <DilateErode.h>

#include <vtkObjectFactory.h>

#include <vtkDataSet.h>

#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkSmartPointer.h>

#include <ttkMacros.h>

vtkStandardNewMacro(ttkDilateErode);

ttkDilateErode::ttkDilateErode(){
    this->SetNumberOfInputPorts(1);
    this->SetNumberOfOutputPorts(1);
}

ttkDilateErode::~ttkDilateErode(){
}

int ttkDilateErode::FillInputPortInformation(
  int port, vtkInformation *info) {
  if(port == 0)
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  else
    return 0;
  return 1;
}

int ttkDilateErode::FillOutputPortInformation(
  int port, vtkInformation *info) {
  if(port == 0)
    info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 0);
  else
    return 0;
  return 1;
}

// =============================================================================
// RequestData
// =============================================================================
int ttkDilateErode::RequestData(
    vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector
){
    // =========================================================================
    // Prepare Input and Output
    // =========================================================================
    auto input = vtkDataSet::GetData( inputVector[0] );
    auto output = vtkDataSet::GetData( outputVector );
    output->ShallowCopy( input );

    auto oldLabels = this->GetInputArrayToProcess(0, inputVector);
    auto newLabels = vtkSmartPointer<vtkDataArray>::Take( oldLabels->NewInstance() );
    newLabels->DeepCopy( oldLabels );
    output->GetPointData()->AddArray( newLabels );

    auto triangulation = ttkAlgorithm::GetTriangulation( input );
    this->preconditionTriangulation(triangulation);

    int status = 0;
    ttkVtkTemplateMacro(
      triangulation->getType(),
      oldLabels->GetDataType(),
      (status = this->dilateErode<VTK_TT, TTK_TT>(
        // Output
        (VTK_TT*) newLabels->GetVoidPointer(0),

        // Input
        this->Mode,
        (VTK_TT) this->Value,
        (TTK_TT*) triangulation->getData(),
        (VTK_TT*) oldLabels->GetVoidPointer(0)
      ))
    );

    if(!status) return 0;

    return 1;
}
