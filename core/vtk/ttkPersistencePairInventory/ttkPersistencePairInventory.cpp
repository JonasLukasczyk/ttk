#include <ttkPersistencePairInventory.h>

#include <vtkDataObject.h> // For port info
#include <vtkObjectFactory.h> // for new macro

#include <vtkSmartPointer.h>
#include <vtkPointData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkInformation.h>
#include <vtkDoubleArray.h>
#include <vtkUnstructuredGrid.h>
#include <vtkCellArray.h>

#include <limits.h>

#include <ttkUtils.h>

vtkStandardNewMacro(ttkPersistencePairInventory);

ttkPersistencePairInventory::ttkPersistencePairInventory(){
    this->SetNumberOfInputPorts(1);
    this->SetNumberOfOutputPorts(1);
}

ttkPersistencePairInventory::~ttkPersistencePairInventory(){}

// see ttkAlgorithm::FillInputPortInformation for details about this method
int ttkPersistencePairInventory::FillInputPortInformation(int port, vtkInformation* info) {
    if (port==0)
        info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
    else
        return 0;
    return 1;
}

// see ttkAlgorithm::FillOutputPortInformation for details about this method
int ttkPersistencePairInventory::FillOutputPortInformation(int port, vtkInformation* info) {
    if (port==0)
        info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
    else
        return 0;
    return 1;
}

int ttkPersistencePairInventory::RequestData(
    vtkInformation*,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector
){
    // Get the input
    auto inputAsMB = vtkMultiBlockDataSet::GetData( inputVector[0] );

    if(!inputAsMB){
        this->printErr("Input is not a 'vtkMultiBlockDataSet' object.");
        return 0;
    }

    if(this->UseBinning && this->NumberOfScalarValues<2){
        this->printErr("Binning requires at least two scalar values.");
        return 0;
    }

    size_t nRows = this->UseBinning ? this->NumberOfScalarValues-1 : this->NumberOfScalarValues;
    size_t nCols = inputAsMB->GetNumberOfBlocks();
    if(nCols<1){
        this->printErr("Input 'vtkMultiBlockDataSet' object has no blocks.");
        return 0;
    }

    // get first scalar field
    auto scalarArray = this->GetInputArrayToProcess(0,inputAsMB->GetBlock(0));

    if(!scalarArray){
        this->printErr("Unable to retrieve input array.");
        return 0;
    }

    // TODO: Add APPIArray
    // TODO: PeristenceCurve based Persistence intervals
    // Prepare output point buffer
    auto ppiArray = vtkSmartPointer<vtkIntArray>::New();
    ppiArray->SetName("PersistencePairInventory");
    ppiArray->SetNumberOfComponents( this->NumberOfPersistenceThresholds );
    ppiArray->SetNumberOfTuples( nRows*nCols );

    auto pcArray = vtkSmartPointer<vtkIntArray>::New();
    pcArray->SetName("PersistenceCurves");
    pcArray->SetNumberOfComponents( this->NumberOfPersistenceThresholds );
    pcArray->SetNumberOfTuples( nCols );

    auto scalarBoundsArray = vtkSmartPointer<vtkDoubleArray>::New();
    scalarBoundsArray->SetName("ScalarBounds");
    scalarBoundsArray->SetNumberOfComponents( 1 );
    scalarBoundsArray->SetNumberOfTuples( 2 );

    auto persistenceThresholdArray = vtkSmartPointer<vtkDataArray>::Take( scalarArray->NewInstance() );
    persistenceThresholdArray->SetName("PersistenceThresholds");
    persistenceThresholdArray->SetNumberOfComponents( 1 );
    persistenceThresholdArray->SetNumberOfTuples( this->NumberOfPersistenceThresholds );

    auto imageObject = vtkSmartPointer<vtkImageData>::New();
    imageObject->SetExtent(
        0, nCols-1,
        0, nRows-1,
        0, 0
    );
    imageObject->SetSpacing(1,1,0);
    imageObject->SetOrigin(0,0,0);
    imageObject->GetPointData()->AddArray( ppiArray );
    auto imageObject_FieldData = imageObject->GetFieldData();
    imageObject_FieldData->AddArray( pcArray );
    imageObject_FieldData->AddArray( scalarBoundsArray );
    imageObject_FieldData->AddArray( persistenceThresholdArray );

    // copy field data
    {
        auto firstBlockAsUG = vtkUnstructuredGrid::SafeDownCast( inputAsMB->GetBlock(0) );
        if(!firstBlockAsUG){
            this->printErr("Input 'vtkMultiBlockDataSet' block not of type 'vtkUnstructuredGrid'.");
            return 0;
        }
        auto firstBlockAsUG_FieldData = firstBlockAsUG->GetFieldData();
        for(int i=0; i<firstBlockAsUG_FieldData->GetNumberOfArrays(); i++){
            auto arrayTemplate = firstBlockAsUG_FieldData->GetAbstractArray(i);
            auto arrayCopy = vtkSmartPointer<vtkAbstractArray>::Take(arrayTemplate->NewInstance());
            arrayCopy->SetName(arrayTemplate->GetName());
            arrayCopy->SetNumberOfComponents(arrayTemplate->GetNumberOfComponents());
            arrayCopy->SetNumberOfTuples(nCols);

            for(size_t b=0; b<nCols; b++){
                auto blockAsUG = vtkUnstructuredGrid::SafeDownCast( inputAsMB->GetBlock(b) );
                if(!blockAsUG){
                    this->printErr("Input 'vtkMultiBlockDataSet' block not of type 'vtkUnstructuredGrid'.");
                    return 0;
                }
                auto array = blockAsUG->GetFieldData()->GetAbstractArray( arrayTemplate->GetName() );
                if(!array){
                    this->printWrn("Unalbe to retrieve field data array '"+std::string(arrayTemplate->GetName())+"' from all blocks.");
                    continue;
                }
                if(array->GetNumberOfTuples()>1){
                    this->printWrn("Field data array '"+std::string(arrayTemplate->GetName())+"' from block "+std::to_string(b)+" has more than one tuple.");
                    continue;
                }
                arrayCopy->SetTuple(b, 0, array);
            }
            imageObject_FieldData->AddArray( arrayCopy );
        }
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

                    auto blockScalarArray = this->GetInputArrayToProcess(0, blockAsUG);
                    if(!blockScalarArray){
                        this->printErr("Unable to retrieve input array.");
                        return 0;
                    }

                    scalarsPerElement[i] = ttkUtils::GetPointer<VTK_TT>(blockScalarArray);
                    //connectivityListPerElement[i] = blockAsUG->GetCells()->GetPointer();
                    connectivityListPerElement[i] = ttkUtils::GetPointer<long long>(blockAsUG->GetCells()->GetConnectivityArray());
                    nEdgesPerElement[i] = blockAsUG->GetNumberOfCells();
                }

                int status = 1;

                VTK_TT scalarBounds[2];
                if(this->UseEntireScalarRange){
                    status = this->ComputeScalarBounds(
                        scalarBounds,

                        scalarsPerElement,
                        nEdgesPerElement
                    );
                    if(!status) return 0;
                } else {
                    scalarBounds[0] = this->ScalarRange[0];
                    scalarBounds[1] = this->ScalarRange[1];
                }

                scalarBoundsArray->SetValue(0, scalarBounds[0]);
                scalarBoundsArray->SetValue(1, scalarBounds[1]);

                auto persistenceThresholdArrayData = ttkUtils::GetPointer<VTK_TT>(persistenceThresholdArray);
                for(int i=0; i<this->NumberOfPersistenceThresholds; i++)
                    persistenceThresholdArrayData[i] = i*this->PersistenceDelta;

                status = this->ComputePersistenceCurves(
                    ttkUtils::GetPointer<int>(pcArray),

                    persistenceThresholdArrayData,
                    this->NumberOfPersistenceThresholds,
                    scalarsPerElement,
                    connectivityListPerElement,
                    nEdgesPerElement
                );
                if(!status) return 0;

                status = this->ComputePPI(
                    ttkUtils::GetPointer<int>(ppiArray),

                    nRows,
                    scalarBounds,
                    this->UseBinning,
                    scalarsPerElement,
                    persistenceThresholdArrayData,
                    this->NumberOfPersistenceThresholds,
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
