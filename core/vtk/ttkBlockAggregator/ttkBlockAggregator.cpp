#include <ttkBlockAggregator.h>

#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkSmartPointer.h>

#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkMultiBlockDataSet.h>

vtkStandardNewMacro(ttkBlockAggregator);

ttkBlockAggregator::ttkBlockAggregator() {
  this->setDebugMsgPrefix("BlockAggregator");

  this->Reset();
  this->SetInputArrayToProcess(0, 0, 0, 2, "_ttk_IterationInfo");

  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

ttkBlockAggregator::~ttkBlockAggregator() {
}

int ttkBlockAggregator::FillInputPortInformation(int port,
                                                 vtkInformation *info) {
  if(port == 0) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
    info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
    return 1;
  }
  return 0;
}

int ttkBlockAggregator::FillOutputPortInformation(int port,
                                                  vtkInformation *info) {
  if(port == 0) {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
    return 1;
  }
  return 0;
}

int ttkBlockAggregator::Reset() {
  this->AggregatedMultiBlockDataSet
    = vtkSmartPointer<vtkMultiBlockDataSet>::New();
  return 1;
}

int copyObjects(vtkDataObject *source, vtkDataObject *copy) {
  if(source->IsA("vtkMultiBlockDataSet")) {
    auto sourceAsMB = vtkMultiBlockDataSet::SafeDownCast(source);
    auto copyAsMB = vtkMultiBlockDataSet::SafeDownCast(copy);

    if(sourceAsMB == nullptr || copyAsMB == nullptr) {
      return 0;
    }

    const auto sourceFD = sourceAsMB->GetFieldData();
    auto copyFD = copyAsMB->GetFieldData();

    if(sourceFD == nullptr || copyFD == nullptr) {
      return 0;
    }

    copyFD->ShallowCopy(sourceFD);

    for(size_t i = 0; i < sourceAsMB->GetNumberOfBlocks(); i++) {
      auto block = sourceAsMB->GetBlock(i);
      auto blockCopy
        = vtkSmartPointer<vtkDataObject>::Take(block->NewInstance());

      copyObjects(block, blockCopy);
      copyAsMB->SetBlock(i, blockCopy);
    }
  } else {
    copy->ShallowCopy(source);
  }

  return 1;
}

int ttkBlockAggregator::AggregateBlock(vtkMultiBlockDataSet* collection, vtkDataObject *item){
  auto itemAsMB = vtkMultiBlockDataSet::SafeDownCast(item);
  if(itemAsMB){
    for(size_t b=0,n=itemAsMB->GetNumberOfBlocks(); b<n; b++)
      this->AggregateBlock(collection, itemAsMB->GetBlock(b));
  } else {
    auto copy = vtkSmartPointer<vtkDataObject>::Take(item->NewInstance());
    copy->ShallowCopy(item);
    const auto nBlocks = collection->GetNumberOfBlocks();
    collection->SetBlock(nBlocks, copy);
    this->printMsg("Adding object at block index " + std::to_string(nBlocks), 1);
  }

  return 1;
}

int ttkBlockAggregator::RequestData(vtkInformation *ttkNotUsed(request),
                                    vtkInformationVector **inputVector,
                                    vtkInformationVector *outputVector) {
  // Get iteration information
  double iterationIndex = -1;
  auto iterationInformation = vtkDoubleArray::SafeDownCast(
    this->GetInputArrayToProcess(0, inputVector));
  if(iterationInformation) {
    iterationIndex = iterationInformation->GetValue(0);
    this->AggregatedMultiBlockDataSet->GetFieldData()->AddArray(
      iterationInformation);
  }

  // Check if AggregatedMultiBlockDataSet needs to be reset
  if(!this->GetStreaming() || iterationIndex < 1)
    this->Reset();

  size_t nInputs = inputVector[0]->GetNumberOfInformationObjects();
  if(nInputs==1){
    // if there is only one input connection aggregate input directly into MBA
    this->AggregateBlock(
      this->AggregatedMultiBlockDataSet,
      vtkDataObject::GetData(inputVector[0], 0)
    );
  } else if(nInputs>1) {
    // if there is more than one input connection aggregate each connection in an own MB
    if(iterationIndex<1){
      // in the first iteration initialize the list of each input connection
      for(size_t i = 0; i < nInputs; i++)
        this->AggregatedMultiBlockDataSet->SetBlock(i, vtkSmartPointer<vtkMultiBlockDataSet>::New());
    }

    // add each object to the list of the corresponding connection
    for(size_t i = 0; i < nInputs; i++)
      this->AggregateBlock(
        static_cast<vtkMultiBlockDataSet*>(this->AggregatedMultiBlockDataSet->GetBlock(i)),
        vtkDataObject::GetData(inputVector[0], i)
      );
  }

  // Prepare output
  auto output = vtkMultiBlockDataSet::GetData(outputVector);
  output->ShallowCopy(this->AggregatedMultiBlockDataSet);

  return 1;
}
