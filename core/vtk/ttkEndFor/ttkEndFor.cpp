#include <ttkEndFor.h>

#include <vtkCompositeDataPipeline.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkMultiBlockDataSet.h>

#include <ttkForEach.h>

vtkStandardNewMacro(ttkEndFor);

ttkEndFor::ttkEndFor() {
  this->setDebugMsgPrefix("EndFor");

  SetNumberOfInputPorts(2);
  SetNumberOfOutputPorts(1);
}

ttkEndFor::~ttkEndFor() = default;
;

int ttkEndFor::FillInputPortInformation(int port, vtkInformation *info) {
  if(port == 0 || port == 1) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject", 1);
    if(port == 0)
      info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
    return 1;
  }
  return 0;
}

int removeFieldDataRecursively(vtkDataObject *object) {
  object->GetFieldData()->RemoveArray("_ttk_IterationInfo");
  if(object->IsA("vtkMultiBlockDataSet")) {
    auto objectAsMB = static_cast<vtkMultiBlockDataSet *>(object);
    for(size_t i = 0; i < objectAsMB->GetNumberOfBlocks(); i++)
      removeFieldDataRecursively(objectAsMB->GetBlock(i));
  }
  return 1;
}

int ttkEndFor::RequestData(vtkInformation *request,
                           vtkInformationVector **inputVector,
                           vtkInformationVector *outputVector) {

  // find for each head
  ttkForEach *forEach = nullptr;
  {
    auto inputAlgorithm = this->GetInputAlgorithm(1, 0);
    while(inputAlgorithm && !inputAlgorithm->IsA("ttkForEach")) {
      inputAlgorithm = inputAlgorithm->GetInputAlgorithm();
    }
    forEach = ttkForEach::SafeDownCast(inputAlgorithm);
  }

  if(!forEach) {
    this->printErr("Second input not connected to a ttkForEach filter.");
    return 0;
  }

  // get iteration info
  int i = forEach->GetIterationIdx() - 1;
  int n = forEach->GetIterationNumber();

  // if this is a repeated iteration
  if(this->LastIterationIdx == i && i > 0){
    this->printMsg("For Loop Modified -> Restarting Iterations", ttk::debug::Separator::BACKSLASH);
    forEach->SetIterationIdx(n+1); // to force restart in ttkForEach

    size_t nInputConnections = inputVector[0]->GetNumberOfInformationObjects();
    for(size_t c=0; c<nInputConnections; c++)
      this->GetInputAlgorithm(0, c)->Update();
    request->Set(vtkStreamingDemandDrivenPipeline::CONTINUE_EXECUTING(), 1);
    return 1;
  }

  this->LastIterationIdx = i;

  this->printMsg("Iteration ( " + std::to_string(i+1) + " / " + std::to_string(n) + " ) complete ", ttk::debug::Separator::BACKSLASH);

  ttkBlockAggregator::RequestData(request, inputVector, outputVector);

  if(i >= n - 1) {
    auto output = vtkDataObject::GetData(outputVector);
    removeFieldDataRecursively(output);
    request->Remove(vtkStreamingDemandDrivenPipeline::CONTINUE_EXECUTING());
  } else {
    // if this is an intermediate iteration
    forEach->Modified();
    size_t nInputConnections = inputVector[0]->GetNumberOfInformationObjects();
    for(size_t c=0; c<nInputConnections; c++)
      this->GetInputAlgorithm(0, c)->Update();

    request->Set(vtkStreamingDemandDrivenPipeline::CONTINUE_EXECUTING(), 1);
  }

  return 1;
}
