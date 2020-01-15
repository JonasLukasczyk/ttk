#include <ttkPersistencePairInventory.h>

#include <vtkDataObject.h> // For port info
#include <vtkObjectFactory.h> // for new macro

#include <vtkSmartPointer.h>
#include <vtkPointData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkDoubleArray.h>
#include <vtkUnstructuredGrid.h>
#include <vtkCellArray.h>

#include <limits.h>

vtkStandardNewMacro(ttkPersistencePairInventory);

ttkPersistencePairInventory::ttkPersistencePairInventory(){
    this->SetNumberOfInputPorts(1);
    this->SetNumberOfOutputPorts(1);
}

ttkPersistencePairInventory::~ttkPersistencePairInventory(){}

// see ttkAlgorithm::FillInputPortInformation for details about this method
int ttkPersistencePairInventory::FillInputPortInformation(int port, vtkInformation* info) {
    switch (port) {
        case 0:
            info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
            break;
        default:
            return 0;
    }
    return 1;
}

// see ttkAlgorithm::FillOutputPortInformation for details about this method
int ttkPersistencePairInventory::FillOutputPortInformation(int port, vtkInformation* info) {
    switch (port) {
        case 0:
            info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
            break;
        default:
            return 0;
    }
    return 1;
}

int ttkPersistencePairInventory::RequestData(
    vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector
){
    // Get the input
    auto inputAsMB = vtkMultiBlockDataSet::GetData( inputVector[0] );

    size_t nRows = this->GetNumberOfScalarBins();
    size_t nCols = inputAsMB->GetNumberOfBlocks();

    // TODO: Add APPIArray
    // TODO: PeristenceCurve based Persistence intervals
    // Prepare output point buffer
    auto ppiArray = vtkSmartPointer<vtkIntArray>::New();
    ppiArray->SetName("PersistencePairInventory");
    ppiArray->SetNumberOfComponents( this->GetNumberOfPersistenceIntervals() );
    ppiArray->SetNumberOfTuples( nRows*nCols );

    auto pcArray = vtkSmartPointer<vtkIntArray>::New();
    pcArray->SetName("PersistenceCurves");
    pcArray->SetNumberOfComponents( this->NumberOfPersistenceCurvePoints );
    pcArray->SetNumberOfTuples( nCols );

    auto scalarBoundsArray = vtkSmartPointer<vtkDoubleArray>::New();
    scalarBoundsArray->SetName("ScalarBounds");
    scalarBoundsArray->SetNumberOfComponents( 1 );
    scalarBoundsArray->SetNumberOfTuples( 2 );

    auto imageObject = vtkSmartPointer<vtkImageData>::New();
    imageObject->SetExtent(
        0, nCols-1,
        0, nRows-1,
        0, 0
    );
    imageObject->SetSpacing(1,1,0);
    imageObject->SetOrigin(0,0,0);
    imageObject->GetPointData()->AddArray( ppiArray );
    imageObject->GetFieldData()->AddArray( pcArray );
    imageObject->GetFieldData()->AddArray( scalarBoundsArray );

    // get first scalar field
    auto scalarArray = this->GetInputArrayToProcess(0,inputAsMB->GetBlock(0));

    if(!scalarArray){
        this->printErr("Unable to retrieve input array.");
        return 0;
    }

    switch( scalarArray->GetDataType() ){
        vtkTemplateMacro(
            {
                std::vector<VTK_TT*> scalarsPerElement(nCols);
                std::vector<vtkIdType*> connectivityListPerElement(nCols);
                std::vector<size_t> nEdgesPerElement(nCols);

                for(size_t i=0; i<nCols; i++){
                    auto blockAsUG = vtkUnstructuredGrid::SafeDownCast( inputAsMB->GetBlock(i) );
                    if(!blockAsUG){
                        this->printErr("Block " +std::to_string(i)+ " is not a vtkDataSet object.");
                        return 0;
                    }

                    auto scalarArray = this->GetInputArrayToProcess(0, blockAsUG);
                    if(!scalarArray){
                        this->printErr("Unable to retrieve input array.");
                        return 0;
                    }

                    scalarsPerElement[i] = (VTK_TT*) scalarArray->GetVoidPointer(0);
                    connectivityListPerElement[i] = blockAsUG->GetCells()->GetPointer();
                    nEdgesPerElement[i] = blockAsUG->GetNumberOfCells();
                }

                int status = 1;

                VTK_TT scalarBounds[2];
                status = this->ComputeScalarBounds(
                    scalarBounds,

                    scalarsPerElement,
                    nEdgesPerElement
                );
                if(!status) return 0;
                scalarBoundsArray->SetValue(0, scalarBounds[0]);
                scalarBoundsArray->SetValue(1, scalarBounds[1]);

                status = this->ComputePersistenceCurves(
                    (int*) pcArray->GetVoidPointer(0),

                    this->NumberOfPersistenceCurvePoints,
                    scalarBounds,
                    scalarsPerElement,
                    connectivityListPerElement,
                    nEdgesPerElement
                );
                if(!status) return 0;

                status = this->ComputePPI(
                    (int*) ppiArray->GetVoidPointer(0),

                    nRows,
                    scalarBounds,
                    scalarsPerElement,
                    this->GetNumberOfPersistenceIntervals(),
                    connectivityListPerElement,
                    nEdgesPerElement
                );
                if(!status) return 0;
            }
        );
    }

    // Get the output
    auto output = vtkMultiBlockDataSet::GetData( outputVector );
    output->SetBlock(0, imageObject);

    return 1;
}