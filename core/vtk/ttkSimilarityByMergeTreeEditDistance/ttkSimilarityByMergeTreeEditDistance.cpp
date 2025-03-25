#include <ttkSimilarityByMergeTreeEditDistance.h>

#include <vtkInformation.h>
#include <vtkObjectFactory.h>

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPointSet.h>

#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkStringArray.h>

#include <ttkMacros.h>
#include <ttkUtils.h>
#include <ttkMergeTreeFeatureTracking.h>

vtkStandardNewMacro(ttkSimilarityByMergeTreeEditDistance);

ttkSimilarityByMergeTreeEditDistance::ttkSimilarityByMergeTreeEditDistance() {
  this->SetNumberOfInputPorts(2);
  this->SetNumberOfOutputPorts(2);
}
ttkSimilarityByMergeTreeEditDistance::~ttkSimilarityByMergeTreeEditDistance() {}

int ttkSimilarityAlgorithm::FillOutputPortInformation(int port,
                                                      vtkInformation *info) {
  if(port == 0) {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
    return 1;
  }
  if(port == 1) {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
    return 1;
  }
  // if(port == 2) {
  //   info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
  //   return 1;
  // }
  return 0;
}

int ttkSimilarityByMergeTreeEditDistance::RequestData(vtkInformation *,
                                        vtkInformationVector **inputVector,
                                        vtkInformationVector *outputVector) {

  auto inputNodes = vtkMultiBlockDataSet::GetData(inputVector[0]);
  auto inputArcs = vtkMultiBlockDataSet::GetData(inputVector[1]);
  auto output = vtkMultiBlockDataSet::GetData(outputVector, 0);
  const size_t n = inputNodes->GetNumberOfBlocks();

  // auto outputNodes = vtkMultiBlockDataSet::GetData(outputVector, 1);
  // auto outputArcs = vtkMultiBlockDataSet::GetData(outputVector, 2);
  // outputNodes->SetNumberOfBlocks(n);
  // outputArcs->SetNumberOfBlocks(n);
  // outputNodes->ShallowCopy(inputNodes);
  // outputArcs->ShallowCopy(inputArcs);

  vtkNew<vtkMultiBlockDataSet> mtmb;
  mtmb->SetNumberOfBlocks(2);
  mtmb->SetBlock(0, inputNodes);
  mtmb->SetBlock(1, inputArcs);
  vtkNew<ttkMergeTreeFeatureTracking> ft;
  ft->SetBackend(backend);
  if(this->backend==3){
    ft->SetBranchMetric(branchMetric);
  }

  ft->SetInputDataObject(0,mtmb.GetPointer());
  ft->SetImportantPairs(0);
  if(this->backend==0){
    ft->SetEpsilonTree1(0);
  } else {
    ft->SetEpsilonTree1(0);
  }
  ft->SetEpsilon2Tree1(100);
  ft->SetEpsilon3Tree1(100);
  //ft->SetPlanarLayout(true);
  //ft->SetNormalizedWasserstein(false);
  ft->Update();

  vtkNew<vtkMultiBlockDataSet> matchings_mb;
  matchings_mb->ShallowCopy(vtkMultiBlockDataSet::SafeDownCast(ft->GetOutputDataObject(1)));

  auto memberTrees = vtkMultiBlockDataSet::SafeDownCast(ft->GetOutputDataObject(0));
  auto memberNodes = vtkMultiBlockDataSet::SafeDownCast(memberTrees->GetBlock(0));
  auto memberArcs = vtkMultiBlockDataSet::SafeDownCast(memberTrees->GetBlock(1));

  auto outputTrees = vtkMultiBlockDataSet::GetData(outputVector, 1);
  outputTrees->ShallowCopy(memberTrees);

  for(size_t t=1; t<n; t++){

    auto nodes0 = vtkUnstructuredGrid::SafeDownCast(inputNodes->GetBlock(t-1));
    auto arcs0 = vtkUnstructuredGrid::SafeDownCast(inputArcs->GetBlock(t-1));
    auto nodes1 = vtkUnstructuredGrid::SafeDownCast(inputNodes->GetBlock(t));
    auto arcs1 = vtkUnstructuredGrid::SafeDownCast(inputArcs->GetBlock(t));

    
    auto memberNodes0 = vtkUnstructuredGrid::SafeDownCast(memberNodes->GetBlock(t-1));
    auto memberArcs0 = vtkUnstructuredGrid::SafeDownCast(memberArcs->GetBlock(t-1));
    auto memberNodes1 = vtkUnstructuredGrid::SafeDownCast(memberNodes->GetBlock(t));
    auto memberArcs1 = vtkUnstructuredGrid::SafeDownCast(memberArcs->GetBlock(t));

    // initialize similarity matrix i.e., distance matrix
    auto matrix = ttkSimilarityAlgorithm::InitializeMatrix(
      "Matched", VTK_INT, memberNodes0->GetNumberOfPoints(), memberNodes1->GetNumberOfPoints()
    );
    auto matrixData = matrix->GetPointData()->GetArray(0);
    for(ttk::SimplexId i=0; i<memberNodes0->GetNumberOfPoints()*memberNodes1->GetNumberOfPoints(); i++)
      matrixData->SetTuple1(i,0);

    // auto matching = std::vector<ttk::SimplexId>(nodes1->GetNumberOfPoints(),-1);
    auto matchingi_vtk = vtkUnstructuredGrid::SafeDownCast(matchings_mb->GetBlock(t-1));
    for(ttk::SimplexId cellIdx = 0; cellIdx < matchingi_vtk->GetNumberOfCells(); cellIdx++) {
      ttk::SimplexId id1 = matchingi_vtk->GetCellData()->GetArray("tree1NodeId")->GetComponent(cellIdx,0);
      ttk::SimplexId id2 = matchingi_vtk->GetCellData()->GetArray("tree2NodeId")->GetComponent(cellIdx,0);
      ttk::SimplexId n1 = memberNodes0->GetPointData()->GetArray("NodeId")->GetComponent(id1,0);
      ttk::SimplexId n2 = memberNodes1->GetPointData()->GetArray("NodeId")->GetComponent(id2,0);
      matrixData->SetTuple1(id1 + id2*nodes0->GetNumberOfPoints(),1);
      // matching[n2] = n1;
    }
    int status = 0;

    // if(t==1){
    //   vtkNew<vtkMultiBlockDataSet> outputNodes0;
    //   vtkNew<vtkMultiBlockDataSet> outputArcs0;
    //   outputNodes0->DeepCopy(memberNodes0);
    //   outputArcs0->DeepCopy(memberArcs0);
    //   outputNodes->SetBlock(t-1,outputNodes0);
    //   outputArcs->SetBlock(t-1,outputArcs0);
    // }
    // vtkNew<vtkMultiBlockDataSet> outputNodesi;
    // vtkNew<vtkMultiBlockDataSet> outputArcsi;
    // outputNodesi->DeepCopy(memberNodes0);
    // outputArcsi->DeepCopy(memberArcs0);
    // outputNodes->SetBlock(t-1,outputNodesi);
    // outputArcs->SetBlock(t-1,outputArcsi);

    auto fidArr0 = this->GetInputArrayToProcess(0, nodes0);
    auto fidArr1 = this->GetInputArrayToProcess(0, nodes1);
    if(!fidArr0 || !fidArr1)
      return !this->printErr("Unable to retrieve feature IDs.");
    std::unordered_map<ttk::SimplexId, ttk::SimplexId> indexIdMap0;
    std::unordered_map<ttk::SimplexId, ttk::SimplexId> indexIdMap1;
    for(ttk::SimplexId i=0; i<memberNodes0->GetNumberOfPoints(); i++){
      ttk::SimplexId nid = memberNodes0->GetPointData()->GetArray("NodeId")->GetComponent(i,0);
      ttk::SimplexId fid = fidArr0->GetComponent(i,0);
      indexIdMap0[fid] = nid;
    }
    for(ttk::SimplexId i=0; i<memberNodes1->GetNumberOfPoints(); i++){
      ttk::SimplexId nid = memberNodes1->GetPointData()->GetArray("NodeId")->GetComponent(i,0);
      ttk::SimplexId fid = fidArr1->GetComponent(i,0);
      indexIdMap1[fid] = nid;
    }

    status = ttkSimilarityAlgorithm::AddIndexIdMaps(
      matrix, indexIdMap0, indexIdMap1, "FeatureId");
    if(!status)
      return 0;

    output->SetBlock(t-1,matrix);
  }

  return 1;
}
