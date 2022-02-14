#include <ttkMapData.h>

#include <vtkInformation.h>

#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkMapData);

ttkMapData::ttkMapData() {
  this->SetNumberOfInputPorts(2);
  this->SetNumberOfOutputPorts(1);
}

ttkMapData::~ttkMapData() {
}

int ttkMapData::FillInputPortInformation(int port, vtkInformation *info) {
  if(port == 0) {
    info->Remove(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE());
    info->Append(
      vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
    info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    return 1;
  } else if(port == 1) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    return 1;
  }
  return 0;
}

int ttkMapData::FillOutputPortInformation(int port, vtkInformation *info) {
  if(port == 0) {
    info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 0);
    return 1;
  }
  return 0;
}

int ttkMapData::ProcessSingle(vtkDataSet *output,
                              ttk::MapData::Map &map,
                              vtkDataArray *codomain) {
  auto lookup0 = this->GetInputArrayToProcess(3, output);
  auto lookup1 = this->GetInputArrayToProcess(4, output);
  if(!lookup0)
    return !this->printErr("Unable to retrieve lookup arrays.");

  if(!lookup1)
    lookup1 = lookup0;

  const int m = output->GetNumberOfPoints();
  auto outputArray
    = vtkSmartPointer<vtkDataArray>::Take(codomain->NewInstance());
  outputArray->SetName(codomain->GetName());
  outputArray->SetNumberOfTuples(m);

  int status = 0;
  ttkTypeMacroAAA(outputArray->GetDataType(), lookup0->GetDataType(),
                  lookup1->GetDataType(),
                  (status = this->performLookup<T0, T1, T2>(
                     ttkUtils::GetPointer<T0>(outputArray), map, m,
                     ttkUtils::GetConstPointer<const T1>(lookup0),
                     lookup0->GetNumberOfValues() - 1,
                     ttkUtils::GetConstPointer<const T2>(lookup1),
                     lookup1->GetNumberOfValues() - 1, this->MissingValue)));
  if(!status)
    return 0;

  output->GetPointData()->AddArray(outputArray);

  return 1;
};

int ttkMapData::RequestData(vtkInformation *,
                            vtkInformationVector **inputVector,
                            vtkInformationVector *outputVector) {
  // generate map
  auto source = vtkDataSet::GetData(inputVector[1]);
  ttk::MapData::Map map;
  auto codomain = this->GetInputArrayToProcess(2, source);
  {
    auto domain0 = this->GetInputArrayToProcess(0, source);
    auto domain1 = this->GetInputArrayToProcess(1, source);
    if(!domain0 || !codomain)
      return !this->printErr("Unable to retrieve domain and codomain arrays.");

    if(!domain1)
      domain1 = domain0;

    const int n = domain0->GetNumberOfValues();
    if(n != domain1->GetNumberOfValues() || n != codomain->GetNumberOfValues())
      return !this->printErr(
        "Domain and codomain arrays must have same length.");

    int status = 0;
    ttkTypeMacroAAA(codomain->GetDataType(), domain0->GetDataType(),
                    domain1->GetDataType(),
                    (status = this->generateMap<T1, T2, T0>(
                       map, n, ttkUtils::GetConstPointer<const T1>(domain0),
                       ttkUtils::GetConstPointer<const T2>(domain1),
                       ttkUtils::GetConstPointer<const T0>(codomain))));
    if(!status)
      return 0;
  }

  // map data
  {
    auto target = vtkDataObject::GetData(inputVector[0]);
    auto targetAsMB = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    if(target->IsA("vtkMultiBlockDataSet"))
      targetAsMB->ShallowCopy(target);
    else
      targetAsMB->SetBlock(0, target);

    auto outputAsMB = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    const int nBlocks = targetAsMB->GetNumberOfBlocks();
    for(int b = 0; b < nBlocks; b++) {
      auto targetAsDS = vtkDataSet::SafeDownCast(targetAsMB->GetBlock(b));
      if(!targetAsDS)
        return !this->printErr("Input Data Objects must be vtkDataSets.");

      auto copy = vtkSmartPointer<vtkDataSet>::Take(targetAsDS->NewInstance());
      copy->ShallowCopy(targetAsDS);

      if(!this->ProcessSingle(copy, map, codomain))
        return 0;

      outputAsMB->SetBlock(b, copy);
    }

    auto output = vtkDataObject::GetData(outputVector);
    if(target->IsA("vtkMultiBlockDataSet"))
      output->ShallowCopy(outputAsMB);
    else
      output->ShallowCopy(outputAsMB->GetBlock(0));
  }

  // return success
  return 1;
}
