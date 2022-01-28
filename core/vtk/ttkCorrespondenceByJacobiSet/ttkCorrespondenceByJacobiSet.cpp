#include <ttkCorrespondenceByJacobiSet.h>

#include <vtkInformation.h>
#include <vtkObjectFactory.h>

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPointSet.h>

#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkStringArray.h>

#include <ttkUtils.h>
#include <ttkMacros.h>

#include <vtkUnstructuredGrid.h>

#include <vtkThreshold.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkStaticCleanPolyData.h>

// TTK Base Includes
#include <ttkJacobiSet.h>
#include <ttkConnectedComponents.h>

vtkStandardNewMacro(ttkCorrespondenceByJacobiSet);

ttkCorrespondenceByJacobiSet::ttkCorrespondenceByJacobiSet() {
  this->setDebugMsgPrefix("CorrespondenceByJacobiSet");

  this->SetNumberOfInputPorts(2);
  this->SetNumberOfOutputPorts(1);
}

ttkCorrespondenceByJacobiSet::~ttkCorrespondenceByJacobiSet() {
}

template<typename DT>
int computeStackedArray(
  DT* sd,
  const DT* d0,
  const DT* d1,
  const int n,
  const DT offset = 0
){
  for(int i=0; i<n; i++){
    sd[i] = d0[i];
  }
  for(int i=0, j=n; i<n; i++,j++){
    sd[j] = d1[i] + offset;
  }
  return 1;
}


using ComponentIdMap = std::vector< std::tuple<std::vector<int>,std::vector<int>> >;

template<typename IT, int mapIdx>
int computeComponentIdMap(
  ComponentIdMap& componentIdMap,
  const IT* vidPoints,
  const int n,
  const float* vidComponents,
  const int m,
  const int* componentIds,
  const IT vidOffset
){
  for(int i=0; i<n; i++){
    const IT vid = vidPoints[i] + vidOffset;

    // search for vid in seg
    for(int j=0; j<m; j++){
      if(static_cast<IT>(vidComponents[j])==vid){
        std::get<mapIdx>(componentIdMap[ componentIds[j] ]).push_back(i);
        break;
      }
    }
  }

  return 1;
}


int ttkCorrespondenceByJacobiSet::ComputeCorrespondences(
  vtkImageData *correspondenceMatrix,
  vtkDataObject *inputDataObjects0,
  vtkDataObject *inputDataObjects1) {

  // unpack input
  auto inputsAsMB0 = vtkMultiBlockDataSet::SafeDownCast(inputDataObjects0);
  auto inputsAsMB1 = vtkMultiBlockDataSet::SafeDownCast(inputDataObjects1);
  if(!inputsAsMB0 || !inputsAsMB1)
    return !this->printErr("Unable to retrieve input data objects.");

  auto image0 = vtkImageData::SafeDownCast(inputsAsMB0->GetBlock(0));
  auto image1 = vtkImageData::SafeDownCast(inputsAsMB1->GetBlock(0));
  if(!image0 || !image1)
    return !this->printErr("Unable to retrieve input grid data objects.");

  auto points0 = vtkPointSet::SafeDownCast(inputsAsMB0->GetBlock(1));
  auto points1 = vtkPointSet::SafeDownCast(inputsAsMB1->GetBlock(1));
  if(!points0 || !points1)
    return !this->printErr("Unable to retrieve input critical point data objects.");

  // check if input images are two dimensional
  int dim[3];
  {
    int dim_[3];
    image0->GetDimensions(dim);
    image1->GetDimensions(dim_);
    if(dim[0]!=dim_[0] || dim[1]!=dim_[1] || dim[2]!=dim_[2] || dim[2]!=1)
      return !this->printErr("Input grids need to be two dimensional and must have same dimension.");
  }

  // // retrieve scalar and order arrays
  // auto scalar0 = this->GetInputArrayToProcess(0,image0);
  // auto scalar1 = this->GetInputArrayToProcess(0,image1);
  // if(!scalar0 || !scalar1)
  //   return !this->printErr("Unable to retrieve scalar arrays.");

  // const int nTuplesPerImage = scalar0->GetNumberOfTuples();

  // auto stackedScalarArray = vtkSmartPointer<vtkDataArray>::Take(scalar0->NewInstance());
  // stackedScalarArray->SetName("Scalars");
  // stackedScalarArray->SetNumberOfTuples(2*nTuplesPerImage);
  // ttkTypeMacroA(
  //   stackedScalarArray->GetDataType(),
  //   computeStackedArray<T0>(
  //     ttkUtils::GetPointer<T0>(stackedScalarArray),
  //     ttkUtils::GetPointer<T0>(scalar0),
  //     ttkUtils::GetPointer<T0>(scalar1),
  //     nTuplesPerImage
  //   )
  // );

  // retrieve order arrays
  auto order0 = this->GetOrderArray(image0, 0);
  auto order1 = this->GetOrderArray(image1, 0);
  if(!order0 || !order1)
    return !this->printErr("Unable to retrieve order arrays.");
  const int nTuplesPerImage = order0->GetNumberOfTuples();

  // initialize data arrays of stacked image data object
  auto stackedOrderArray = vtkSmartPointer<vtkDataArray>::Take(order0->NewInstance());
  stackedOrderArray->SetName("ORDER");
  stackedOrderArray->SetNumberOfTuples(nTuplesPerImage*2);
  auto stackedOrderArrayData = ttkUtils::GetPointer<ttk::SimplexId>(stackedOrderArray);

  auto timeArray = vtkSmartPointer<vtkDataArray>::Take(order0->NewInstance());
  timeArray->SetName("TIME");
  timeArray->SetNumberOfTuples(nTuplesPerImage*2);
  auto timeArrayData = ttkUtils::GetPointer<ttk::SimplexId>(timeArray);
  {
    auto order0Data = ttkUtils::GetPointer<ttk::SimplexId>(order0);
    auto order1Data = ttkUtils::GetPointer<ttk::SimplexId>(order1);
    for(int i=0; i<nTuplesPerImage; i++){
      timeArrayData[i] = 0;
      stackedOrderArrayData[i] = order0Data[i];
    }
    for(int i=0,j=nTuplesPerImage; i<nTuplesPerImage; i++,j++){
      timeArrayData[j] = 1;
      stackedOrderArrayData[j] = order1Data[i];
    }
  }

  vtkSmartPointer<vtkDataArray> stackedVertexIdentifiers;
  {
    // stack vertex identifiers
    auto vidImage0 = this->GetInputArrayToProcess(1,image0);
    auto vidImage1 = this->GetInputArrayToProcess(1,image1);
    if(!vidImage0 || !vidImage1)
      return !this->printErr("Unable to retrieve vertex identifier arrays from input grid.");

    stackedVertexIdentifiers = vtkSmartPointer<vtkDataArray>::Take(vidImage1->NewInstance());
    stackedVertexIdentifiers->SetName(vidImage0->GetName());
    stackedVertexIdentifiers->SetNumberOfTuples(nTuplesPerImage*2);
    ttkTypeMacroA(
      stackedVertexIdentifiers->GetDataType(),
      computeStackedArray<T0>(
        ttkUtils::GetPointer<T0>(stackedVertexIdentifiers),
        ttkUtils::GetPointer<T0>(vidImage0),
        ttkUtils::GetPointer<T0>(vidImage1),
        nTuplesPerImage,
        nTuplesPerImage
      )
    );
  }

  // build stacked image data object
  auto stackedImage = vtkSmartPointer<vtkImageData>::New();
  stackedImage->SetDimensions(dim[0], dim[1], 2);

  auto stackedImagePD = stackedImage->GetPointData();
  stackedImagePD->AddArray(stackedOrderArray);
  stackedImagePD->AddArray(timeArray);
  stackedImagePD->AddArray(stackedVertexIdentifiers);


  // compute jacobi set on stacked image
  auto jacobiSetFilter = vtkSmartPointer<ttkJacobiSet>::New();
  jacobiSetFilter->SetInputDataObject(stackedImage);
  jacobiSetFilter->SetDebugLevel(this->debugLevel_);
  jacobiSetFilter->SetVertexScalars(true);
  jacobiSetFilter->SetInputArrayToProcess(0,0,0,0,"ORDER");
  jacobiSetFilter->SetInputArrayToProcess(1,0,0,0,"TIME");
  jacobiSetFilter->Update();

  auto jacobiSet = vtkUnstructuredGrid::SafeDownCast(jacobiSetFilter->GetOutputDataObject(0));

  // deriving connected components of temporal edges
  auto components = vtkSmartPointer<vtkPolyData>::New();
  {
    ttk::Timer t;
    const std::string msg = "Extracting Temporal Edges";
    this->printMsg(msg,0,0,1,ttk::debug::LineMode::REPLACE);

    // add mask for temporal edges
    {
      const int nJocobiSetEdges = jacobiSet->GetNumberOfCells();

      auto jsMask = vtkSmartPointer<vtkUnsignedCharArray>::New();
      jsMask->SetName("MASK");
      jsMask->SetNumberOfTuples(nJocobiSetEdges);
      auto jsMaskData = ttkUtils::GetPointer<unsigned char>(jsMask);
      jacobiSet->GetCellData()->AddArray(jsMask);

      const auto jsTimeData = ttkUtils::GetPointer<ttk::SimplexId>(jacobiSet->GetPointData()->GetArray("TIME"));
      for(int i=0; i<nJocobiSetEdges; i++){
        auto pointIds = jacobiSet->GetCell(i)->GetPointIds();
        jsMaskData[i] = jsTimeData[pointIds->GetId(0)]!=jsTimeData[pointIds->GetId(1)];
      }
    }

    this->printMsg(msg,0.1,t.getElapsedTime(),1,ttk::debug::LineMode::REPLACE);

    auto threshold = vtkSmartPointer<vtkThreshold>::New();
    threshold->SetInputDataObject(jacobiSet);
    threshold->SetInputArrayToProcess(0,0,0,1,"MASK");
    threshold->SetUpperThreshold(0.5);
    threshold->SetThresholdFunction( vtkThreshold::ThresholdType::THRESHOLD_UPPER );
    threshold->Update();
    this->printMsg(msg,0.3,t.getElapsedTime(),1,ttk::debug::LineMode::REPLACE);

    // vtkUnstructuredGrid to vtkPolyData
    auto dataSetSurfaceFilter = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
    dataSetSurfaceFilter->SetInputConnection(0, threshold->GetOutputPort(0));
    dataSetSurfaceFilter->Update();
    this->printMsg(msg,0.5,t.getElapsedTime(),1,ttk::debug::LineMode::REPLACE);

    // merge duplicate points
    auto cleanPolyData = vtkSmartPointer<vtkStaticCleanPolyData>::New();
    cleanPolyData->SetInputConnection(0, dataSetSurfaceFilter->GetOutputPort(0));
    cleanPolyData->Update();
    this->printMsg(msg,0.7,t.getElapsedTime(),1,ttk::debug::LineMode::REPLACE);

    auto connectedComponents = vtkSmartPointer<ttkConnectedComponents>::New();
    connectedComponents->SetInputConnection(0, cleanPolyData->GetOutputPort(0));
    connectedComponents->SetUseSeedIdAsComponentId(false);
    connectedComponents->Update();
    this->printMsg(msg,0.9,t.getElapsedTime(),1,ttk::debug::LineMode::REPLACE);

    const auto temp = vtkPolyData::SafeDownCast(connectedComponents->GetOutputDataObject(0));
    if(!temp)
      return !this->printErr("Unable to merge points and compute connected components of temporal Jacobi edges.");
    components->ShallowCopy(temp);

    this->printMsg(msg + " (#"+std::to_string(components->GetNumberOfCells())+")",1,t.getElapsedTime(),1);
  }

  // iterate over features and find for each critical points its corresponding connected component
  {
    ttk::Timer t;
    const std::string msg = "Computing Correspondence Matrix";
    this->printMsg(msg,0,0,1,ttk::debug::LineMode::REPLACE);

    auto componentIds = components->GetPointData()->GetArray("ComponentId");
    if(!componentIds)
      return !this->printErr("Unable to retrieve componentIds from temporal Jacobi edges");

    const auto componentIdsData = ttkUtils::GetPointer<int>(componentIds);

    ComponentIdMap componentIdToVertexIdMap(componentIds->GetRange()[1]+1);

    auto vidPoints0 = this->GetInputArrayToProcess(1,points0);
    auto vidPoints1 = this->GetInputArrayToProcess(1,points1);
    if(!vidPoints0 || !vidPoints1)
      return !this->printErr("Unable to retrieve vertex identifier arrays from input points.");

    const int nPoints0 = vidPoints0->GetNumberOfTuples();
    const int nPoints1 = vidPoints1->GetNumberOfTuples();

    // vtkStaticCleanPolyData turns this array into a float array, no matter the original data type
    auto vidComponents = this->GetInputArrayToProcess(1,components);
    if(!vidComponents)
      return !this->printErr("Unable to retrieve vertex identifier arrays from connected components.");
    const int nComponentVertices = vidComponents->GetNumberOfTuples();

    this->printMsg(msg,0.1,t.getElapsedTime(),this->threadNumber_,ttk::debug::LineMode::REPLACE);

    int status=0;
    ttkTypeMacroI(
      vidPoints0->GetDataType(),
      (
        status = computeComponentIdMap<T0,0>(
          componentIdToVertexIdMap,
          ttkUtils::GetPointer<T0>(vidPoints0),
          nPoints0,
          ttkUtils::GetPointer<float>(vidComponents),
          nComponentVertices,
          componentIdsData,
          0
        )
      )
    );
    if(!status)
      return 0;

    this->printMsg(msg,0.4,t.getElapsedTime(),this->threadNumber_,ttk::debug::LineMode::REPLACE);

    ttkTypeMacroI(
      vidPoints1->GetDataType(),
      (
        status = computeComponentIdMap<T0,1>(
          componentIdToVertexIdMap,
          ttkUtils::GetPointer<T0>(vidPoints1),
          nPoints1,
          ttkUtils::GetPointer<float>(vidComponents),
          nComponentVertices,
          componentIdsData,
          nTuplesPerImage
        )
      )
    );
    if(!status)
      return 0;

    this->printMsg(msg,0.8,t.getElapsedTime(),this->threadNumber_,ttk::debug::LineMode::REPLACE);

    correspondenceMatrix->SetDimensions(nPoints0, nPoints1, 1);
    correspondenceMatrix->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
    auto matrixArray = correspondenceMatrix->GetPointData()->GetArray(0);
    matrixArray->SetName("Match");

    const int n = nPoints0*nPoints1;

    auto matrixArrayData = ttkUtils::GetPointer<unsigned char>(matrixArray);

    for(int i=0; i<n; i++)
      matrixArrayData[i]=0;

    for(const auto& it: componentIdToVertexIdMap){
      for(const auto& i: std::get<0>(it)){
        for(const auto& j: std::get<1>(it)){
          matrixArrayData[j*nPoints0+i] = 1;
        }
      }
    }

    this->printMsg(msg,1,t.getElapsedTime(),this->threadNumber_);
  }

  // correspondenceMatrix->ShallowCopy(stackedImage);

  // Add Index Label Maps
  {
    int status = ttkCorrespondenceAlgorithm::AddIndexIdMaps(
      correspondenceMatrix,
      this->GetInputArrayToProcess(1, points0),
      this->GetInputArrayToProcess(1, points1)
    );
    if(!status)
      return 0;
  }

  return 1;
}
