#include <ttkCorrespondenceAlgorithm.h>

#include <vtkInformation.h>

#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPointSet.h>

#include <vtkStringArray.h>

#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkCorrespondenceAlgorithm);

ttkCorrespondenceAlgorithm::ttkCorrespondenceAlgorithm() {
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);

  this->PreviousInputs = vtkSmartPointer<vtkMultiBlockDataSet>::New();
}

ttkCorrespondenceAlgorithm::~ttkCorrespondenceAlgorithm() {
}

int ttkCorrespondenceAlgorithm::FillInputPortInformation(int port,
                                                         vtkInformation *info) {
  if(port >= 0 && port < this->GetNumberOfInputPorts()) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    info->Append(
      vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
    return 1;
  }
  return 0;
}

int ttkCorrespondenceAlgorithm::FillOutputPortInformation(
  int port, vtkInformation *info) {
  if(port == 0) {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
    return 1;
  }
  return 0;
}

template <typename DT>
void BuildIdIndexMapDT(std::unordered_map<ttk::SimplexId, ttk::SimplexId> &map,
                       const int n,
                       const DT *indexIdMapData) {
  for(int i = 0; i < n; i++)
    map.emplace(
      std::make_pair(static_cast<ttk::SimplexId>(indexIdMapData[i]), i));
}

int ttkCorrespondenceAlgorithm::BuildIdIndexMap(
  std::unordered_map<ttk::SimplexId, ttk::SimplexId> &map,
  const vtkDataArray *indexIdMap) {
  if(!indexIdMap)
    return 0;

  switch(indexIdMap->GetDataType()) {
    vtkTemplateMacro(
      BuildIdIndexMapDT<VTK_TT>(map, indexIdMap->GetNumberOfValues(),
                                ttkUtils::GetConstPointer<VTK_TT>(indexIdMap)));
  }

  return 1;
}

int ttkCorrespondenceAlgorithm::RequestData(
  vtkInformation *,
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector) {

  auto output = vtkMultiBlockDataSet::GetData(outputVector);

  const int nInputs = this->GetNumberOfInputPorts();
  std::vector<vtkDataObject *> inputs(nInputs);
  for(int i = 0; i < nInputs; i++) {
    auto input = vtkDataObject::GetData(inputVector[i]);
    if(!input)
      return !this->printErr("Unable to retrieve input data objects.");
    inputs[i] = input;
  }

  auto firstInput = inputs[0];
  const bool streamingMode = !firstInput->IsA("vtkMultiBlockDataSet");

  std::vector<vtkSmartPointer<vtkMultiBlockDataSet>> sequence;

  if(streamingMode) {
    auto iterationInformation = vtkDoubleArray::SafeDownCast(
      firstInput->GetFieldData()->GetArray("_ttk_IterationInfo"));
    if(this->PreviousInputs->GetNumberOfBlocks() > 0
       && (!iterationInformation || iterationInformation->GetValue(0) > 0)) {
      sequence.resize(2);
      sequence[0] = vtkSmartPointer<vtkMultiBlockDataSet>::New();
      sequence[0]->ShallowCopy(this->PreviousInputs);

      sequence[1] = vtkSmartPointer<vtkMultiBlockDataSet>::New();
      for(int i = 0; i < nInputs; i++)
        sequence[1]->SetBlock(i, inputs[i]);
    }
  } else {
    const int nSteps
      = static_cast<vtkMultiBlockDataSet *>(firstInput)->GetNumberOfBlocks();
    sequence.resize(nSteps);
    for(int s = 0; s < nSteps; s++) {
      sequence[s] = vtkSmartPointer<vtkMultiBlockDataSet>::New();
      for(int i = 0; i < nInputs; i++) {
        sequence[s]->SetBlock(
          i, static_cast<vtkMultiBlockDataSet *>(inputs[i])->GetBlock(s));
      }
    }
  }

  for(size_t s = 1; s < sequence.size(); s++) {
    auto correspondenceMatrix = vtkSmartPointer<vtkImageData>::New();
    auto data0 = sequence[s - 1];
    auto data1 = sequence[s + 0];
    bool singleInput = data0->GetNumberOfBlocks() == 1;

    if(!this->ComputeCorrespondences(correspondenceMatrix,
                                     singleInput ? data0->GetBlock(0) : data0,
                                     singleInput ? data1->GetBlock(0) : data1))
      return 0;

    output->SetBlock(s - 1, correspondenceMatrix);
  }

  if(streamingMode) {
    this->PreviousInputs = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    for(int i = 0; i < nInputs; i++) {
      auto copy = vtkSmartPointer<vtkDataSet>::Take(
        static_cast<vtkDataSet *>(inputs[i])->NewInstance());
      copy->ShallowCopy(inputs[i]);
      this->PreviousInputs->SetBlock(i, copy);
    }
  }

  return 1;
}

std::string
  ttkCorrespondenceAlgorithm::GetIdArrayName(vtkFieldData *fieldData) {
  std::string result;
  int found = 0;
  for(int a = 0; a < fieldData->GetNumberOfArrays(); a++) {
    auto array = fieldData->GetArray(a);
    const auto name = std::string(array->GetName());
    const int n = name.size();

    if(name.substr(n - 4, 4).compare("_t-1") == 0) {
      result = name.substr(0, n - 4);
      found++;
    } else if(name.substr(n - 2, 2).compare("_t") == 0) {
      found++;
    }
  }

  return found == 2 ? result : "";
};

int ttkCorrespondenceAlgorithm::GetIndexIdMaps(vtkDataArray *&indexIdMapP,
                                               vtkDataArray *&indexIdMapC,
                                               vtkFieldData *fieldData) {
  std::string idArrayName
    = ttkCorrespondenceAlgorithm::GetIdArrayName(fieldData);

  if(idArrayName.size() < 1)
    return 0;

  indexIdMapP = fieldData->GetArray((idArrayName + "_t-1").data());
  indexIdMapC = fieldData->GetArray((idArrayName + "_t").data());

  return 1;
};

int ttkCorrespondenceAlgorithm::AddIndexIdMap(
  vtkImageData *correspondenceMatrix,
  vtkDataArray *idArray,
  const bool isMapForCurrentTimestep) {
  auto fd = correspondenceMatrix->GetFieldData();
  auto array = vtkSmartPointer<vtkDataArray>::Take(idArray->NewInstance());
  array->ShallowCopy(idArray);
  array->SetName((std::string(idArray->GetName())
                  + (isMapForCurrentTimestep ? "_t" : "_t-1"))
                   .data());
  fd->AddArray(array);
  return 1;
}

int ttkCorrespondenceAlgorithm::AddIndexIdMaps(
  vtkImageData *correspondenceMatrix,
  vtkDataArray *indexIdMapP,
  vtkDataArray *indexIdMapC) {
  int status = 0;
  status = ttkCorrespondenceAlgorithm::AddIndexIdMap(
    correspondenceMatrix, indexIdMapP, false);
  if(!status)
    return 0;

  status = ttkCorrespondenceAlgorithm::AddIndexIdMap(
    correspondenceMatrix, indexIdMapC, true);
  if(!status)
    return 0;

  return 1;
}

int ttkCorrespondenceAlgorithm::AddIndexIdMaps(
  vtkImageData *correspondenceMatrix,
  const std::unordered_map<ttk::SimplexId, ttk::SimplexId> &idIndexMapP,
  const std::unordered_map<ttk::SimplexId, ttk::SimplexId> &idIndexMapC,
  const std::string &idArrayName) {
  auto fd = correspondenceMatrix->GetFieldData();
  int a = 0;
  const std::string suffix[2]{"_t-1", "_t"};
  for(auto map :
      std::vector<const std::unordered_map<ttk::SimplexId, ttk::SimplexId> *>(
        {&idIndexMapP, &idIndexMapC})) {
    auto array = vtkSmartPointer<ttkSimplexIdTypeArray>::New();
    array->SetName((idArrayName + suffix[a++]).data());
    array->SetNumberOfTuples(map->size());
    auto arrayData = ttkUtils::GetPointer<ttk::SimplexId>(array);
    for(auto it : (*map))
      arrayData[it.second] = it.first;

    fd->AddArray(array);
  }

  return 1;
}
