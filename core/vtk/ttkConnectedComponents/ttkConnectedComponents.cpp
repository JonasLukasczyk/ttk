#include <ttkConnectedComponents.h>

#include <vtkObjectFactory.h> // for new macro

#include <vtkDataSet.h>
#include <vtkPolyData.h>

#include <vtkPointData.h>
#include <vtkAbstractArray.h>
#include <vtkSmartPointer.h>

#include <vtkCellArray.h>

#include <vtkIdTypeArray.h>
#include <vtkLongArray.h>

vtkStandardNewMacro(ttkConnectedComponents);

ttkConnectedComponents::ttkConnectedComponents() {
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(2);
}

ttkConnectedComponents::~ttkConnectedComponents() {
}

int ttkConnectedComponents::FillInputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  else
    return 0;

  return 1;
}

int ttkConnectedComponents::FillOutputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 0);
  else if(port == 1)
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPolyData");
  else
    return 0;

  return 1;
}

// =============================================================================
// RequestData
// =============================================================================
int ttkConnectedComponents::RequestData(
    vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector
){
    // Prepare Input and Output
    auto input = vtkDataSet::GetData( inputVector[0] );

    auto outputSegmentation = vtkDataSet::GetData( outputVector, 0 );
    auto outputComponents   = vtkPolyData::GetData( outputVector, 1 );

    outputSegmentation->ShallowCopy( input );

    auto labelArray = vtkSmartPointer<vtkLongArray>::New();
    labelArray->SetName( this->OutputArrayName.data() );
    labelArray->SetNumberOfComponents(1);
    labelArray->SetNumberOfTuples(input->GetNumberOfPoints());

    std::vector<ttk::ConnectedComponents::Component> components;

    vtkSmartPointer<vtkAbstractArray> backgroundLabelArray = this->UsePrelabeledBackground
        ? this->GetInputArrayToProcess(0, inputVector)
        : vtkSmartPointer<vtkLongArray>::New();

    ttk::Triangulation *triangulation = ttkAlgorithm::GetTriangulation(input);
    this->preconditionTriangulation(triangulation);

    int status = 0;

    switch(
        vtkTemplate2PackMacro(
            labelArray->GetDataType(),
            backgroundLabelArray->GetDataType()
        )
    ){
        vtkTemplate2Macro(
            status = this->computeConnectedComponents(
                (VTK_T1*) labelArray->GetVoidPointer(0),
                components,

                triangulation,
                this->UsePrelabeledBackground
                    ? (VTK_T2*) backgroundLabelArray->GetVoidPointer(0)
                    : nullptr,
                this->UsePrelabeledBackground
                    ? (VTK_T2) this->BackgroundLabel
                    : (VTK_T2) -1
            )
        );
    }

    if(!status)
        return 0;

    // Finalize Output
    {
        // Segmentation
        {
            outputSegmentation->GetPointData()->AddArray( labelArray );
        }

        // Components
        {
            size_t nComponents = components.size();

            auto componentPoints = vtkSmartPointer<vtkPoints>::New();
            componentPoints->SetNumberOfPoints( nComponents );
            auto pointCoords = (float*) componentPoints->GetVoidPointer(0);

            auto componentCells = vtkSmartPointer<vtkCellArray>::New();
            auto connectivityList = componentCells->WritePointer(nComponents, nComponents * 2);

            auto componentSizeArray = vtkSmartPointer<vtkLongArray>::New();
            componentSizeArray->SetName( "ComponentSize" );
            componentSizeArray->SetNumberOfComponents(1);
            componentSizeArray->SetNumberOfTuples( nComponents );
            auto componentSizeArrayData = (long*) componentSizeArray->GetVoidPointer(0);

            for(size_t i=0,j=0,k=0; i<nComponents; i++){
                const auto& c = components[i];

                pointCoords[j++] = c.center[0];
                pointCoords[j++] = c.center[1];
                pointCoords[j++] = c.center[2];

                connectivityList[k++] = 1;
                connectivityList[k++] = i;

                componentSizeArrayData[i] = c.size;
            }

            outputComponents->SetPoints( componentPoints );
            outputComponents->SetVerts( componentCells );
            outputComponents->GetPointData()->AddArray( componentSizeArray );
        }
    }

    return 1;
}