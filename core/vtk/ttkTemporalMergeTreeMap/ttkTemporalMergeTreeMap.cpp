#include <ttkTemporalMergeTreeMap.h>

#include <vtkInformation.h>

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkFloatArray.h>
#include <Debug.h>
#include <vtkCellData.h>

#include <FTMTreeUtils.h>
#include <MergeTreeBarycenter.h>
#include <ttkMacros.h>
#include <ttkMergeTreeClustering.h>
#include <ttkMergeTreeFeatureTracking.h>
#include <ttkMergeTreeUtils.h>
#include <ttkUtils.h>

// A VTK macro that enables the instantiation of this class via ::New()
// You do not have to modify this
vtkStandardNewMacro(ttkTemporalMergeTreeMap);

/**
 * TODO 7: Implement the filter constructor and destructor in the cpp file.
 *
 * The constructor has to specify the number of input and output ports
 * with the functions SetNumberOfInputPorts and SetNumberOfOutputPorts,
 * respectively. It should also set default values for all filter
 * parameters.
 *
 * The destructor is usually empty unless you want to manage memory
 * explicitly, by for example allocating memory on the heap that needs
 * to be freed when the filter is destroyed.
 */
ttkTemporalMergeTreeMap::ttkTemporalMergeTreeMap() {
  this->SetNumberOfInputPorts(3);
  this->SetNumberOfOutputPorts(3);
}

/**
 * TODO 8: Specify the required input data type of each input port
 *
 * This method specifies the required input object data types of the
 * filter by adding the vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE() key to
 * the port information.
 */
int ttkTemporalMergeTreeMap::FillInputPortInformation(int port,
                                                       vtkInformation *info) {
  if(port == 0) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
    return 1;
  }
  if(port == 1) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
    return 1;
  }
  if(port == 2) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
    return 1;
  }
  // if(port == 3) {
  //   info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
  //   return 1;
  // }
  return 0;
}

/**
 * TODO 9: Specify the data object type of each output port
 *
 * This method specifies in the port information object the data type of the
 * corresponding output objects. It is possible to either explicitly
 * specify a type by adding a vtkDataObject::DATA_TYPE_NAME() key:
 *
 *      info->Set( vtkDataObject::DATA_TYPE_NAME(), "vtkUnstructuredGrid" );
 *
 * or to pass a type of an input port to an output port by adding the
 * ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT() key (see below).
 *
 * Note: prior to the execution of the RequestData method the pipeline will
 * initialize empty output data objects based on this information.
 */
int ttkTemporalMergeTreeMap::FillOutputPortInformation(int port,
                                                        vtkInformation *info) {
  if(port == 0 || port == 1 || port == 2) {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
    return 1;
  }
  return 0;
}

void ttkTemporalMergeTreeMap::dfs_linearization(
  ttk::SimplexId curr_node,
  std::vector<double> &lin,
  std::vector<ttk::SimplexId> &seg,
  std::vector<double> &nodePositions,
  std::vector<std::vector<ttk::SimplexId>> &memiChildren,
  std::vector<std::vector<double>> &memiSegmentScalars,
  std::vector<ttk::SimplexId> &memiSizes,
  std::vector<ttk::SimplexId> &memiSegs,
  std::vector<double> &memiScalars,
  std::vector<double> &memiOrdering,
  std::vector<ttk::SimplexId> prevMatching,
  std::vector<double> prevOrdering,
  ttk::SimplexId timeStep) {
  if(memiChildren[curr_node].size() == 0) {
    auto curr_size = memiSizes[curr_node];
    auto segment = memiSegmentScalars[memiSegs[curr_node]];
    for(ttk::SimplexId s = 0; s < segment.size(); s += 2) {
      lin.push_back(segment[s]);
      seg.push_back(memiSegs[curr_node]);
    }
    nodePositions[curr_node] = lin.size();
    for(ttk::SimplexId s = segment.size() - 1 - (segment.size() % 2); s >= 0;
        s -= 2) {
      // for(ttk::SimplexId s=1; s<segment.size(); s+=2){
      lin.push_back(segment[s]);
      seg.push_back(memiSegs[curr_node]);
    }
  } else {
    std::vector<ttk::SimplexId> curr_children = memiChildren[curr_node];
    if(!memiOrdering.empty()){
      std::sort(curr_children.begin(), curr_children.end(),
                [memiOrdering](ttk::SimplexId x1, ttk::SimplexId x2) -> bool {
                  return memiOrdering[x1] < memiOrdering[x2];
                });
      // std::cout << "sorting by barycenter layout" << std::endl;
    }
    else if(!prevMatching.empty()){
      std::sort(curr_children.begin(), curr_children.end(),
                [prevMatching,prevOrdering](ttk::SimplexId x1, ttk::SimplexId x2) -> bool {
                  if(prevMatching[x1]<0) return true;
                  if(prevMatching[x2]<0) return false;
                  return prevOrdering[prevMatching[x1]] < prevOrdering[prevMatching[x2]];
                });
      // std::cout << "sorting by previous step" << std::endl;
    }
    // else{
    //   std::cout << "no sorting at all" << std::endl;
    // }
    // std::vector<ttk::SimplexId> curr_children;
    auto curr_size = ttk::SimplexId(memiSizes[curr_node]);
    auto segment = memiSegmentScalars[memiSegs[curr_node]];
    auto cs1 = ttk::SimplexId(curr_size / 2);
    auto cs2 = curr_size - cs1;
    for(ttk::SimplexId s = 0; s < segment.size() - 1; s += 2) {
      lin.push_back(segment[s]);
      seg.push_back(memiSegs[curr_node]);
    }
    if(curr_children.size() > 2) {
      this->printWrn(std::to_string(curr_children.size())+" children in a node, multisaddle at step " + std::to_string(timeStep) + "!!");
    }
    for(ttk::SimplexId ci = 0; ci < curr_children.size(); ci++) {
      auto c = curr_children[ci];
      dfs_linearization(c, lin, seg, nodePositions, memiChildren, memiSegmentScalars,
                        memiSizes, memiSegs, memiScalars,
                        memiOrdering,prevMatching,prevOrdering,timeStep);
      if(ci < curr_children.size() - 1) {
        lin.push_back(memiScalars[curr_node]);
        seg.push_back(memiSegs[curr_node]);
      }
      if(ci == 0) {
        nodePositions[curr_node] = lin.size();
      }
    }
    for(ttk::SimplexId s = segment.size() - 2 - (1 - (segment.size() % 2));
        s >= 0; s -= 2) {
      // for(ttk::SimplexId s=1; s<segment.size()-1; s+=2){
      lin.push_back(segment[s]);
      seg.push_back(memiSegs[curr_node]);
    }
  }
}

/**
 * TODO 10: Pass VTK data to the base code and convert base code output to VTK
 *
 * This method is called during the pipeline execution to update the
 * already initialized output data objects based on the given input
 * data objects and filter parameters.
 *
 * Note:
 *     1) The passed input data objects are validated based on the information
 *        provided by the FillInputPortInformation method.
 *     2) The output objects are already initialized based on the information
 *        provided by the FillOutputPortInformation method.
 */
int ttkTemporalMergeTreeMap::RequestData(vtkInformation *ttkNotUsed(request),
                                          vtkInformationVector **inputVector,
                                          vtkInformationVector *outputVector) {

  ttk::Timer completeTimer;
  auto correspondences = vtkMultiBlockDataSet::GetData(inputVector[0]);
  if(!correspondences)
    return 0;
  auto inputTrees = vtkMultiBlockDataSet::GetData(inputVector[1]);
  if(!inputTrees)
    return 0;
  // auto inputNodes = vtkMultiBlockDataSet::GetData(inputVector[1]);
  // if(!inputNodes)
  //   return 0;
  // auto inputArcs = vtkMultiBlockDataSet::GetData(inputVector[2]);
  // if(!inputArcs)
  //   return 0;
  auto domains = vtkMultiBlockDataSet::GetData(inputVector[2]);
  if(!domains)
    return 0;

  auto inputNodes = vtkMultiBlockDataSet::SafeDownCast(inputTrees->GetBlock(0));
  auto inputArcs = vtkMultiBlockDataSet::SafeDownCast(inputTrees->GetBlock(1));

  bool isJoinTree = false;
  // std::cout << inputNodes->GetNumberOfBlocks() << std::endl;
  // if(inputNodes->GetNumberOfBlocks()>0){
  //   auto inputNodes0 = vtkUnstructuredGrid::SafeDownCast(inputNodes->GetBlock(0));
  //   isJoinTree = inputNodes0->GetPointData()->GetArray("Scalar")->GetComponent(0,0) < inputNodes0->GetPointData()->GetArray("Scalar")->GetComponent(1,0);
  // }
  // std::cout << "isJoinTree: " << isJoinTree << std::endl;

  std::vector<double> ordering_branches;
  vtkNew<vtkMultiBlockDataSet> members;
  std::vector<std::vector<ttk::SimplexId>> matchings;
  std::vector<double> distances;
  // auto matchings_mb = vtkMultiBlockDataSet::New();
  // auto outputMatchings = vtkMultiBlockDataSet::GetData(outputVector, 2);

  // vtkNew<vtkMultiBlockDataSet> mtmb;
  // mtmb->SetNumberOfBlocks(2);
  // mtmb->SetBlock(0, inputNodes);
  // mtmb->SetBlock(1, inputArcs);
  // vtkNew<ttkMergeTreeFeatureTracking> ft;
  // ft->SetBackend(backend);
  // if(this->backend==3){
  //   ft->SetBranchMetric(branchMetric);
  // }

  // ft->SetInputDataObject(0,mtmb.GetPointer());
  // ft->SetImportantPairs(0);
  // if(this->backend==0){
  //   ft->SetEpsilonTree1(0);
  // } else {
  //   ft->SetEpsilonTree1(0);
  // }
  // ft->SetEpsilon2Tree1(100);
  // ft->SetEpsilon3Tree1(100);
  // //ft->SetPlanarLayout(true);
  // //ft->SetNormalizedWasserstein(false);
  // ft->Update();
  // members->DeepCopy(
  //   vtkMultiBlockDataSet::SafeDownCast(ft->GetOutputDataObject(0)));
  // auto distances_vtk = vtkUnstructuredGrid::SafeDownCast(ft->GetOutputDataObject(2));
  // for(ttk::SimplexId i=0; i<distances_vtk->GetNumberOfPoints(); i++){
  //   distances.push_back(distances_vtk->GetPoint(i)[1]);
  // }

  // matchings_mb->ShallowCopy(vtkMultiBlockDataSet::SafeDownCast(ft->GetOutputDataObject(1)));
  // outputMatchings->ShallowCopy(vtkMultiBlockDataSet::SafeDownCast(ft->GetOutputDataObject(1)));
  // matchings = std::vector<std::vector<ttk::SimplexId>>(matchings_mb->GetNumberOfBlocks());
  // for(ttk::SimplexId i=0; i<matchings.size(); i++){
  //   auto matchingi_vtk = vtkUnstructuredGrid::SafeDownCast(matchings_mb->GetBlock(i));
  //   auto memberNodes = vtkMultiBlockDataSet::SafeDownCast(members->GetBlock(0));
  //   auto currmemberNodes = vtkUnstructuredGrid::SafeDownCast(memberNodes->GetBlock(i+1));
  //   auto prevmemberNodes = vtkUnstructuredGrid::SafeDownCast(memberNodes->GetBlock(i));
  //   matchings[i] = std::vector<ttk::SimplexId>(currmemberNodes->GetNumberOfPoints(),-1);
  //   for(ttk::SimplexId cellIdx = 0; cellIdx < matchingi_vtk->GetNumberOfCells(); cellIdx++) {
  //     ttk::SimplexId id1 = matchingi_vtk->GetCellData()->GetArray("tree1NodeId")->GetComponent(cellIdx,0);
  //     ttk::SimplexId id2 = matchingi_vtk->GetCellData()->GetArray("tree2NodeId")->GetComponent(cellIdx,0);
  //     ttk::SimplexId n1 = prevmemberNodes->GetPointData()->GetArray("NodeId")->GetComponent(id1,0);
  //     ttk::SimplexId n2 = currmemberNodes->GetPointData()->GetArray("NodeId")->GetComponent(id2,0);
  //     matchings[i][n2] = n1;
  //   }
  // }
  

  // auto memberNodes = vtkMultiBlockDataSet::SafeDownCast(members->GetBlock(0));
  // auto memberArcs = vtkMultiBlockDataSet::SafeDownCast(members->GetBlock(1));

  matchings = std::vector<std::vector<ttk::SimplexId>>(correspondences->GetNumberOfBlocks());
  for(ttk::SimplexId i=0; i<matchings.size(); i++){
    auto correspondencei = vtkImageData::SafeDownCast(correspondences->GetBlock(i));
    auto currmemberNodes = vtkUnstructuredGrid::SafeDownCast(inputNodes->GetBlock(i+1));
    auto prevmemberNodes = vtkUnstructuredGrid::SafeDownCast(inputNodes->GetBlock(i));
    matchings[i] = std::vector<ttk::SimplexId>(currmemberNodes->GetNumberOfPoints(),-1);
    for(ttk::SimplexId prevIdx = 0; prevIdx < prevmemberNodes->GetNumberOfPoints(); prevIdx++) {
      for(ttk::SimplexId currIdx = 0; currIdx < currmemberNodes->GetNumberOfPoints(); currIdx++) {
        auto matched = correspondencei->GetPointData()->GetArray("Matched")->GetTuple1(prevIdx + currIdx*prevmemberNodes->GetNumberOfPoints());
        if(matched) matchings[i][currIdx] = prevIdx;
      }
    }
  }

  std::vector<std::vector<double>> linearizations;
  std::vector<std::vector<ttk::SimplexId>> segmentations;
  ttk::SimplexId maxlen = 0;
  std::vector<std::vector<ttk::SimplexId>> memberParents;
  std::vector<std::vector<std::vector<ttk::SimplexId>>> memberChildren;

  auto prevMatching = std::vector<ttk::SimplexId>();
  auto prevOrdering = std::vector<double>();
  for(ttk::SimplexId blockIdx = 0; blockIdx < inputNodes->GetNumberOfBlocks();
      blockIdx++) {

    ttk::SimplexId memberIdx = blockIdx;

    ttk::SimplexId totalSize = 0;
    std::vector<ttk::SimplexId> arcRegions;
    auto memiNodes
      = vtkUnstructuredGrid::SafeDownCast(inputNodes->GetBlock(memberIdx));
    auto memiArcs
      = vtkUnstructuredGrid::SafeDownCast(inputArcs->GetBlock(memberIdx));
    auto memiDomain = vtkDataSet::SafeDownCast(domains->GetBlock(blockIdx));

    auto scalarArrayDomain = this->GetInputArrayToProcess(0, memiDomain);
    // # print(" ")

    auto nodePositions = std::vector<double>(memiNodes->GetNumberOfPoints(),-1);
    if(blockIdx>0){
      prevMatching = matchings[blockIdx-1];
    }

    // prepare scalar values in segments
    std::vector<std::vector<double>> memiSegmentScalars(
      memiDomain->GetPointData()->GetArray("SegmentationId")->GetRange()[1]+1);

    // get node properties of member tree
    ttk::SimplexId numNodesi = 0;
    ttk::SimplexId nnmti
      = vtkUnstructuredGrid::SafeDownCast(inputArcs->GetBlock(blockIdx))
          ->GetNumberOfPoints();
    std::vector<ttk::SimplexId> memiNodeIsDummy(memiNodes->GetNumberOfPoints());
    std::vector<double> memiScalars(memiNodes->GetNumberOfPoints());
    for(ttk::SimplexId i = 0; i < memiNodes->GetNumberOfPoints(); i++) {
      auto nId = vtkIntArray::SafeDownCast(
                    memiNodes->GetPointData()->GetArray("NodeId"))
                    ->GetValue(i);
      auto isDummy = vtkIntArray::SafeDownCast(
                        memiNodes->GetPointData()->GetArray("isDummyNode"))
                        ->GetValue(i);
      // auto isDummy = false;
      auto scalar
        = memiNodes->GetPointData()->GetArray("Scalar")->GetComponent(i, 0);
      if(!isDummy) {
        memiNodeIsDummy[nId] = 1;
        memiScalars[nId] = scalar;
        numNodesi += 1;
      }
    }

    // create tree structure of member tree (parent pointers)
    std::vector<ttk::SimplexId> memiParents(memiNodes->GetNumberOfPoints(), -1);
    for(ttk::SimplexId i = 0; i < memiArcs->GetNumberOfCells(); i++) {
      auto childId = vtkIntArray::SafeDownCast(
                        memiArcs->GetCellData()->GetArray("downNodeId"))
                        ->GetValue(i);
      auto parentId = vtkIntArray::SafeDownCast(
                        memiArcs->GetCellData()->GetArray("upNodeId"))
                        ->GetValue(i);
      memiParents[childId] = parentId;
    }

    //  create tree structure of member tree (children lists)
    std::vector<std::vector<ttk::SimplexId>> memiChildren(
      memiNodes->GetNumberOfPoints());
    ttk::SimplexId root = -1;
    ttk::SimplexId maxDegree = 0;
    for(ttk::SimplexId i = 0; i < memiParents.size(); i++) {
      auto parentId = memiParents[i];
      if(parentId >= 0) {
        memiChildren[parentId].push_back(i);
        maxDegree
          = std::max(maxDegree, (ttk::SimplexId)memiChildren[parentId].size());
        // std::cout << i << "/" << memiChildren[parentId].size() << " ; ";
      }

      if(parentId == -1 && memiNodeIsDummy[i])
        root = i;
    }
    memberParents.push_back(memiParents);
    memberChildren.push_back(memiChildren);
    if(memiScalars[root] > memiScalars[memiChildren[root][0]]){
      isJoinTree = true;
    }

    for(ttk::SimplexId j = 0; j < memiDomain->GetNumberOfPoints(); j++) {
      auto scalar = scalarArrayDomain->GetComponent(j, 0);
      auto segId = ttkSimplexIdTypeArray::SafeDownCast(
                     memiDomain->GetPointData()->GetArray("SegmentationId"))
                     ->GetValue(j);
      memiSegmentScalars[segId].push_back(scalar);
    }
    for(ttk::SimplexId i = 0; i < memiSegmentScalars.size(); i++) {
      // sort in descending order if we're a join tree, ascending if we're a split tree
      if(isJoinTree){
        std::sort(memiSegmentScalars[i].begin(),memiSegmentScalars[i].end(),std::greater<double>());
      }
      else{
        std::sort(memiSegmentScalars[i].begin(),memiSegmentScalars[i].end());
      }
    }

    // compute ordering of member tree nodes for layout (derived from barycenter
    // branchIDs of leaves through dfs)
    std::vector<double> memiOrdering;
    if(blockIdx>0){
      memiOrdering = std::vector<double>(memiNodes->GetNumberOfPoints(), std::numeric_limits<double>::infinity());
    }
    std::stack<ttk::SimplexId> s1;
    std::stack<ttk::SimplexId> s2;
    std::vector<ttk::SimplexId> postorder;
    std::vector<ttk::SimplexId> preorder;
    s1.push(root);
    while(!s1.empty()) {
      auto node = s1.top();
      s1.pop();
      s2.push(node);
      preorder.push_back(node);
      for(auto child : memiChildren[node]) {
        s1.push(child);
      }
    }
    while(!s2.empty()) {
      auto node = s2.top();
      s2.pop();
      postorder.push_back(node);
    }
    for(auto node : postorder) {
      if(memiChildren[node].size() == 0) {
        if(blockIdx>0 && prevMatching[node]>=0){
          memiOrdering[node] = prevOrdering[prevMatching[node]];
        }
        // std:: cout << memiOrdering[node] << std::endl;
        // return memiOrdering[node];
      } else {
        if(blockIdx>0){
          double oidx = std::numeric_limits<double>::infinity();
          for(auto child : memiChildren[node]) {
            auto cidx = memiOrdering[child];
            if(cidx < oidx)
              oidx = cidx;
          }
          memiOrdering[node] = oidx;
          // std:: cout << memiOrdering[node] << std::endl;
          // return oidx;
        }
      }
    }

    // # get area sizes and segmentation ids for member tree nodes (from parent
    // edge)
    std::vector<ttk::SimplexId> memiSizes(memiNodes->GetNumberOfPoints());
    std::vector<ttk::SimplexId> memiSegs(memiNodes->GetNumberOfPoints());
    for(ttk::SimplexId j = 0; j < memiArcs->GetNumberOfCells(); j++) {
      auto rS
        = memiArcs->GetCellData()->GetArray("RegionSize")->GetComponent(j, 0);
      auto isDummy = vtkIntArray::SafeDownCast(
                       memiArcs->GetCellData()->GetArray("isDummyArc"))
                       ->GetValue(j);
      auto downNodeId = vtkIntArray::SafeDownCast(
                          memiArcs->GetCellData()->GetArray("downNodeId"))
                          ->GetValue(j);
      auto cd = memiArcs->GetCellData();
      auto segarr = cd->GetArray("SegmentationId");
      ttk::SimplexId segId = segarr->GetTuple1(j);
      memiSizes[downNodeId] = ttk::SimplexId(rS);
      memiSegs[downNodeId] = ttk::SimplexId(segId);
    }

    // # compute linearization based on odering
    std::vector<double> linearization;
    std::vector<ttk::SimplexId> segmentation;
    dfs_linearization(memiChildren[root][0], linearization, segmentation,
                      nodePositions, memiChildren, memiSegmentScalars,
                      memiSizes, memiSegs, memiScalars,
                      memiOrdering,prevMatching,prevOrdering,blockIdx);
    linearizations.push_back(linearization);
    segmentations.push_back(segmentation);
    // print(len(linearization),len(segmentation))
    if(linearization.size() > maxlen) {
      maxlen = linearization.size();
    }
    prevOrdering = nodePositions;
  }
  // write linearization to output vti

  auto outputmb = vtkMultiBlockDataSet::GetData(outputVector, 0);
  outputmb->SetNumberOfBlocks(1);
  auto outputMembers = vtkMultiBlockDataSet::GetData(outputVector, 1);
  outputMembers->ShallowCopy(members);
  vtkNew<vtkImageData> tmtm;
  tmtm->SetDimensions(linearizations.size()+1,maxlen+1,1);
  tmtm->SetSpacing(std::ceil(maxlen/linearizations.size())*scaling,1,1);
  // tmtm->SetSpacing(1024,1,1);
  tmtm->SetOrigin(0,0,0);

  vtkNew<vtkFloatArray> linArray{};
  vtkNew<vtkIntArray> segArray{};
  vtkNew<vtkIntArray> blockArray{};
  vtkNew<vtkFloatArray> distArray{};


  linArray->SetName("Scalar");
  linArray->SetNumberOfComponents(1);
  linArray->SetNumberOfTuples(linearizations.size() * maxlen);

  segArray->SetName("SegmentationId");
  segArray->SetNumberOfComponents(1);
  segArray->SetNumberOfTuples(linearizations.size() * maxlen);

  blockArray->SetName("BlockId");
  blockArray->SetNumberOfComponents(1);
  blockArray->SetNumberOfTuples(linearizations.size() * maxlen);

  distArray->SetName("Distance");
  distArray->SetNumberOfComponents(1);
  distArray->SetNumberOfTuples(distances.size());



  ttk::SimplexId k = 0;
  for(ttk::SimplexId j = 0; j < maxlen; j++) {
    for(ttk::SimplexId i = 0; i < linearizations.size(); i++) {
      auto v = j < linearizations[i].size() ? linearizations[i][j] : 0;
      auto s = j < linearizations[i].size() ? segmentations[i][j] : 0;
      linArray->SetValue(k, v);
      segArray->SetValue(k, s);
      blockArray->SetValue(k, i);
      k += 1;
    }
  }
  for(ttk::SimplexId i = 0; i < distances.size(); i++) {
    distArray->SetValue(i, distances[i]);
  }

  tmtm->GetCellData()->AddArray(linArray);
  tmtm->GetCellData()->AddArray(segArray);
  tmtm->GetCellData()->AddArray(blockArray);
  tmtm->GetFieldData()->AddArray(distArray);


  outputmb->SetBlock(0,tmtm);
  this->printMsg("Computed temporal merge tree map", 1, completeTimer.getElapsedTime());
  return 1;
}
