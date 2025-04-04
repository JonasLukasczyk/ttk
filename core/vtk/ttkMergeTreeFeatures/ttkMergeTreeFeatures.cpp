#include <ttkMergeTreeFeatures.h>

#include <vtkInformation.h>

#include <vtkDataArray.h>
#include <vtkFloatArray.h>
#include <vtkPolyData.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkMergeTreeFeatures);

ttkMergeTreeFeatures::ttkMergeTreeFeatures() {
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

int ttkMergeTreeFeatures::FillInputPortInformation(int port, vtkInformation *info) {
  if(port == 0) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPolyData");
    return 1;
  }
  return 0;
}

int ttkMergeTreeFeatures::FillOutputPortInformation(int port, vtkInformation *info) {
  if(port == 0) {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPolyData");
    return 1;
  }
  return 0;
}

int ttkMergeTreeFeatures::RequestData(vtkInformation *ttkNotUsed(request),
                               vtkInformationVector **inputVector,
                               vtkInformationVector *outputVector) {

  vtkPolyData *mergeTree = vtkPolyData::GetData(inputVector[0]);
  if(!mergeTree)
    return 0;

  const vtkIdType nEdges = mergeTree->GetNumberOfCells();

  std::vector<double> levels;
  ttkUtils::stringListToDoubleVector(this->Levels, levels);
  if(levels.size()<1)
    return 1;

  auto scalarArray = this->GetInputArrayToProcess(0, mergeTree);
  auto i_nodeIds = mergeTree->GetPointData()->GetArray("NodeId");
  auto i_nextIds = mergeTree->GetPointData()->GetArray("NextId");
  auto i_size = mergeTree->GetPointData()->GetArray("Size");

  vtkIdType nNodes = 0;
  int status = 0;

  const auto prepArray
  = [](vtkDataArray *array, const int nTuples, const int nComponents=1,
       const std::string &name = "") {
      if(name.length() > 0)
        array->SetName(name.data());
      array->SetNumberOfComponents(nComponents);
      array->SetNumberOfTuples(nTuples);
      return array;
    };

  // compute number of nodes
  {
    ttkTypeMacroA(
      scalarArray->GetDataType(),
      (status = this->computeNumberOfNodes<T0, vtkIdType>(
        nNodes,
        levels,
        ttkUtils::GetPointer<const T0>(scalarArray),
        ttkUtils::GetPointer<const vtkIdType>(mergeTree->GetLines()->GetConnectivityArray()),
        nEdges
      ))
    );
    if(!status)
      return 0;
  }

  // init output
  auto nodeCoordsArray = vtkSmartPointer<vtkDataArray>::Take(mergeTree->GetPoints()->GetData()->NewInstance());
  prepArray(nodeCoordsArray, nNodes,3);

  auto offsets = vtkSmartPointer<vtkIdTypeArray>::New();
  auto offsets_ = ttkUtils::GetPointer<vtkIdType>(prepArray(offsets, nNodes+1));
  for(vtkIdType i=0; i<=nNodes; i++)
    offsets_[i]=i;

  auto connectivity = vtkSmartPointer<vtkIdTypeArray>::New();
  auto connectivity_ = ttkUtils::GetPointer<vtkIdType>(prepArray(connectivity, nNodes));
  for(vtkIdType i=0; i<nNodes; i++)
    connectivity_[i]=i;

  auto nodeScalars = vtkSmartPointer<vtkDataArray>::Take(scalarArray->NewInstance());
  prepArray(nodeScalars,nNodes,1,scalarArray->GetName());

  auto nodeLevels = vtkSmartPointer<vtkIntArray>::New();
  prepArray(nodeLevels,nNodes,1,"Level");

  auto nodeParentIds = vtkSmartPointer<vtkIntArray>::New();
  prepArray(nodeParentIds, nNodes,1,"ParentId");

  auto o_nodeIds = vtkSmartPointer<vtkIntArray>::New();
  prepArray(o_nodeIds, nNodes,1,"NodeId");

  auto o_size = vtkSmartPointer<vtkFloatArray>::New();
  prepArray(o_size, nNodes,1,"Size");

  // compute nodes
  {
    ttkTypeMacroRR(
      scalarArray->GetDataType(),
      nodeCoordsArray->GetDataType(),
      (status = this->computeNodes<T0,T1,vtkIdType>(
        ttkUtils::GetPointer<int>(o_nodeIds),
        ttkUtils::GetPointer<T0>(nodeScalars),
        ttkUtils::GetPointer<float>(o_size),
        ttkUtils::GetPointer<int>(nodeLevels),
        ttkUtils::GetPointer<T1>(nodeCoordsArray),

        levels,
        ttkUtils::GetPointer<const T0>(scalarArray),
        ttkUtils::GetPointer<int>(i_size),
        ttkUtils::GetPointer<const vtkIdType>(mergeTree->GetLines()->GetConnectivityArray()),
        ttkUtils::GetPointer<const T1>(mergeTree->GetPoints()->GetData()),
        nEdges
      ))
    );
    if(!status)
      return 0;
  }

  // compute parents
  {
    ttkTypeMacroR(
      scalarArray->GetDataType(),
      (status = this->computeParents<T0>(
        ttkUtils::GetPointer<int>(o_nodeIds),
        ttkUtils::GetPointer<int>(nodeParentIds),

        levels,
        ttkUtils::GetPointer<int>(nodeLevels),
        ttkUtils::GetPointer<T0>(scalarArray),
        ttkUtils::GetPointer<int>(i_nodeIds),
        ttkUtils::GetPointer<int>(i_nextIds),
        nNodes
      ))
    );
    if(!status)
      return 0;
  }

  // create output
  {
    auto output = vtkPolyData::GetData(outputVector);
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    cells->SetData(offsets, connectivity);
    output->SetVerts(cells);

    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetData(nodeCoordsArray);
    output->SetPoints(points);

    auto outputPD = output->GetPointData();
    outputPD->AddArray(nodeScalars);
    outputPD->AddArray(o_nodeIds);
    outputPD->AddArray(nodeParentIds);
    outputPD->AddArray(nodeLevels);
    outputPD->AddArray(o_size);
  }

  return 1;
}
