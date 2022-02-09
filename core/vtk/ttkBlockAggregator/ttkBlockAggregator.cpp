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
  this->SetInputArrayToProcess(0, 0, 0, 2, "_ttk_IterationInfo");

  this->Reset();

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
  this->OutputMultiBlockDataSet = vtkSmartPointer<vtkMultiBlockDataSet>::New();
  return 1;
}

// int copyObjects(vtkDataObject *source, vtkDataObject *copy) {
//   if(source->IsA("vtkMultiBlockDataSet")) {
//     auto sourceAsMB = vtkMultiBlockDataSet::SafeDownCast(source);
//     auto copyAsMB = vtkMultiBlockDataSet::SafeDownCast(copy);

//     if(sourceAsMB == nullptr || copyAsMB == nullptr) {
//       return 0;
//     }

//     const auto sourceFD = sourceAsMB->GetFieldData();
//     auto copyFD = copyAsMB->GetFieldData();

//     if(sourceFD == nullptr || copyFD == nullptr) {
//       return 0;
//     }

//     copyFD->ShallowCopy(sourceFD);

//     for(size_t i = 0; i < sourceAsMB->GetNumberOfBlocks(); i++) {
//       auto block = sourceAsMB->GetBlock(i);
//       auto blockCopy
//         = vtkSmartPointer<vtkDataObject>::Take(block->NewInstance());

//       copyObjects(block, blockCopy);
//       copyAsMB->SetBlock(i, blockCopy);
//     }
//   } else {
//     copy->ShallowCopy(source);
//   }

//   return 1;
// }

// int ttkBlockAggregator::AggregateBlock(vtkDataObject *dataObject) {
//   // ttk::Timer t;
//   size_t nBlocks = this->AggregatedMultiBlockDataSet->GetNumberOfBlocks();
//   // this->printMsg("Adding object add index " + std::to_string(nBlocks), 0,
//                 ttk::debug::LineMode::REPLACE);

//   auto copy =
//   vtkSmartPointer<vtkDataObject>::Take(dataObject->NewInstance());
//   copyObjects(dataObject, copy);

//   // this->AggregatedMultiBlockDataSet->SetBlock(nBlocks, copy);

//   // this->printMsg("Adding object at block index " +
//   std::to_string(nBlocks), 1,
//                 t.getElapsedTime());

//   return 1;
// }

vtkSmartPointer<vtkDataObject> makeShallowCopy(vtkDataObject *i) {
  auto o = vtkSmartPointer<vtkDataObject>::Take(i->NewInstance());
  o->ShallowCopy(i);
  return o;
}

#include <vtkDataSet.h>

int aggregate(vtkMultiBlockDataSet *list, vtkDataObject *obj) {
  if(!obj)
    return 1;

  if(obj->IsA("vtkMultiBlockDataSet")) {
    auto objAsMB = static_cast<vtkMultiBlockDataSet *>(obj);
    for(size_t b = 0; b < objAsMB->GetNumberOfBlocks(); b++)
      aggregate(list, objAsMB->GetBlock(b));
  } else {
    list->SetBlock(list->GetNumberOfBlocks(), makeShallowCopy(obj));
  }

  return 1;
}

int ttkBlockAggregator::RequestData(vtkInformation *ttkNotUsed(request),
                                    vtkInformationVector **inputVector,
                                    vtkInformationVector *outputVector) {
  // Get iteration information
  double iterationIndex = 0;
  auto iterationInformation = vtkDoubleArray::SafeDownCast(
    this->GetInputArrayToProcess(0, inputVector));
  if(iterationInformation) {
    iterationIndex = iterationInformation->GetValue(0);
    this->OutputMultiBlockDataSet->GetFieldData()->AddArray(
      iterationInformation);
  }

  // Check if AggregatedMultiBlockDataSet needs to be reset
  if(!iterationInformation || !this->EnableStreaming || iterationIndex == 0)
    this->Reset();

  // Get number of inputs
  size_t nInputs = inputVector[0]->GetNumberOfInformationObjects();

  if(this->MergeInputs) {
    for(size_t b = 0; b < nInputs; b++) {
      auto input = vtkDataObject::GetData(inputVector[0], b);
      aggregate(this->OutputMultiBlockDataSet, input);
    }
  } else {
    for(size_t b = 0; b < nInputs; b++) {
      auto input = vtkDataObject::GetData(inputVector[0], b);
      if(!this->OutputMultiBlockDataSet->GetBlock(b))
        this->OutputMultiBlockDataSet->SetBlock(
          b, vtkSmartPointer<vtkMultiBlockDataSet>::New());
      auto list = vtkMultiBlockDataSet::SafeDownCast(
        this->OutputMultiBlockDataSet->GetBlock(b));
      aggregate(list, input);
    }
  }

  vtkMultiBlockDataSet::GetData(outputVector)
    ->ShallowCopy(this->OutputMultiBlockDataSet);

  return 1;
}
