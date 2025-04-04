#include <ttkSimilarityByJacobiSet.h>

#include <vtkInformation.h>
#include <vtkObjectFactory.h>

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPointSet.h>

#include <vtkCellData.h>
#include <vtkPointData.h>
#include <vtkStringArray.h>
#include <vtkUnsignedCharArray.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

#include <vtkUnstructuredGrid.h>

#include <vtkDataSetSurfaceFilter.h>
#include <vtkStaticCleanPolyData.h>
#include <vtkThreshold.h>

// TTK Base Includes
#include <ttkConnectedComponents.h>
#include <ttkJacobiSet.h>

vtkStandardNewMacro(ttkSimilarityByJacobiSet);

ttkSimilarityByJacobiSet::ttkSimilarityByJacobiSet() {
  this->setDebugMsgPrefix("SimilarityByJacobiSet");
  this->SetNumberOfInputPorts(2);
}

ttkSimilarityByJacobiSet::~ttkSimilarityByJacobiSet() {
}

template <typename DT>
int computeStackedArray(
  DT *sd, const DT *d0, const DT *d1, const int n, const DT offset = 0) {
  for(int i = 0; i < n; i++) {
    sd[i] = d0[i];
  }
  for(int i = 0, j = n; i < n; i++, j++) {
    // sd[j] = d1[i];
    sd[j] = d1[i] + offset;
  }
  return 1;
}

using ComponentIdMap
  = std::vector<std::tuple<std::vector<int>, std::vector<int>>>;

template <typename IT, int mapIdx>
int computeComponentIdMap(ComponentIdMap &componentIdMap,
                          const IT *vidPoints,
                          const int n,
                          const float *vidComponents,
                          const int m,
                          const int *componentIds,
                          const IT vidOffset) {
  for(int i = 0; i < n; i++) {
    const IT vid = vidPoints[i] + vidOffset;

    // search for vid in seg
    for(int j = 0; j < m; j++) {
      if(static_cast<IT>(vidComponents[j]) == vid) {
        std::get<mapIdx>(componentIdMap[componentIds[j]]).push_back(i);
        break;
      }
    }
  }

  return 1;
}

int ttkSimilarityByJacobiSet::RequestData(vtkInformation *,
                                        vtkInformationVector **inputVector,
                                        vtkInformationVector *outputVector) {
  auto inputImages = vtkMultiBlockDataSet::GetData(inputVector[0]);
  auto inputPoints = vtkMultiBlockDataSet::GetData(inputVector[1]);
  auto output = vtkMultiBlockDataSet::GetData(outputVector);
  const size_t n = inputImages->GetNumberOfBlocks();

  for(size_t t=1; t<n; t++){
    auto image0 = vtkImageData::SafeDownCast(inputImages->GetBlock(t-1));
    auto image1 = vtkImageData::SafeDownCast(inputImages->GetBlock(t));
    if(!image0 || !image1)
      return !this->printErr("Unable to retrieve input grid data objects.");

    auto points0 = vtkPointSet::SafeDownCast(inputPoints->GetBlock(t-1));
    auto points1 = vtkPointSet::SafeDownCast(inputPoints->GetBlock(t));
    if(!points0 || !points1)
      return !this->printErr(
        "Unable to retrieve input critical point data objects.");

    // check if input images are two dimensional
    int dim[3];
    {
      int dim_[3];
      image0->GetDimensions(dim);
      image1->GetDimensions(dim_);
      if(dim[0] != dim_[0] || dim[1] != dim_[1] || dim[2] != dim_[2]
        || dim[2] != 1)
        return !this->printErr(
          "Input grids need to be two dimensional and must have same dimension.");
    }

    // retrieve order arrays
    auto order0 = this->GetOrderArray(image0, 0, nullptr);
    auto order1 = this->GetOrderArray(image1, 0, nullptr);
    if(!order0 || !order1)
      return !this->printErr("Unable to retrieve order arrays.");
    const int nTuplesPerImage = order0->GetNumberOfTuples();

    // initialize data arrays of stacked image data object
    auto stackedOrderArray
      = vtkSmartPointer<vtkDataArray>::Take(order0->NewInstance());
    stackedOrderArray->SetName("ORDER");
    stackedOrderArray->SetNumberOfTuples(nTuplesPerImage * 2);
    auto stackedOrderArrayData
      = ttkUtils::GetPointer<ttk::SimplexId>(stackedOrderArray);

    auto timeArray = vtkSmartPointer<vtkDataArray>::Take(order0->NewInstance());
    timeArray->SetName("TIME");
    timeArray->SetNumberOfTuples(nTuplesPerImage * 2);
    auto timeArrayData = ttkUtils::GetPointer<ttk::SimplexId>(timeArray);
    {
      auto order0Data = ttkUtils::GetPointer<ttk::SimplexId>(order0);
      auto order1Data = ttkUtils::GetPointer<ttk::SimplexId>(order1);
      for(int i = 0; i < nTuplesPerImage; i++) {
        timeArrayData[i] = 0;
        stackedOrderArrayData[i] = order0Data[i];
      }
      for(int i = 0, j = nTuplesPerImage; i < nTuplesPerImage; i++, j++) {
        timeArrayData[j] = 1;
        stackedOrderArrayData[j] = order1Data[i];
      }
    }

    vtkSmartPointer<vtkDataArray> stackedVertexIdentifiers;
    {
      // stack vertex identifiers
      auto vidImage0 = this->GetInputArrayToProcess(1, image0);
      auto vidImage1 = this->GetInputArrayToProcess(1, image1);
      if(!vidImage0 || !vidImage1)
        return !this->printErr(
          "Unable to retrieve vertex identifier arrays from input grid.");

      stackedVertexIdentifiers
        = vtkSmartPointer<vtkDataArray>::Take(vidImage1->NewInstance());
      stackedVertexIdentifiers->SetName(vidImage0->GetName());
      stackedVertexIdentifiers->SetNumberOfTuples(nTuplesPerImage * 2);
      ttkTypeMacroA(
        stackedVertexIdentifiers->GetDataType(),
        computeStackedArray<T0>(
          ttkUtils::GetPointer<T0>(stackedVertexIdentifiers),
          ttkUtils::GetPointer<T0>(vidImage0),
          ttkUtils::GetPointer<T0>(vidImage1), nTuplesPerImage, nTuplesPerImage));
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
    jacobiSetFilter->SetInputArrayToProcess(0, 0, 0, 0, "ORDER");
    jacobiSetFilter->SetInputArrayToProcess(1, 0, 0, 0, "TIME");
    jacobiSetFilter->Update();

    auto jacobiSet = vtkUnstructuredGrid::SafeDownCast(
      jacobiSetFilter->GetOutputDataObject(0));

    // auto copy = vtkSmartPointer<vtkUnstructuredGrid>::New();
    // copy->ShallowCopy(jacobiSet);
    // output->SetBlock(t-1, copy);
    // continue;

    // deriving connected components of temporal edges
    auto components = vtkSmartPointer<vtkPolyData>::New();
    {
      ttk::Timer timer;
      const std::string msg = "Extracting Temporal Edges";
      this->printMsg(msg, 0, 0, 1, ttk::debug::LineMode::REPLACE);

      // add mask for temporal edges
      {
        const int nJocobiSetEdges = jacobiSet->GetNumberOfCells();

        auto jsMask = vtkSmartPointer<vtkUnsignedCharArray>::New();
        jsMask->SetName("MASK");
        jsMask->SetNumberOfTuples(nJocobiSetEdges);
        auto jsMaskData = ttkUtils::GetPointer<unsigned char>(jsMask);
        jacobiSet->GetCellData()->AddArray(jsMask);

        const auto jsTimeData = ttkUtils::GetPointer<ttk::SimplexId>(
          jacobiSet->GetPointData()->GetArray("TIME"));
        for(int i = 0; i < nJocobiSetEdges; i++) {
          auto pointIds = jacobiSet->GetCell(i)->GetPointIds();
          jsMaskData[i]
            = jsTimeData[pointIds->GetId(0)] != jsTimeData[pointIds->GetId(1)];
        }
      }

      this->printMsg(
        msg, 0.1, timer.getElapsedTime(), 1, ttk::debug::LineMode::REPLACE);

      auto threshold = vtkSmartPointer<vtkThreshold>::New();
      threshold->SetInputDataObject(jacobiSet);
      threshold->SetInputArrayToProcess(0, 0, 0, 1, "MASK");
      threshold->SetUpperThreshold(0.5);
      threshold->SetThresholdFunction(
        vtkThreshold::ThresholdType::THRESHOLD_UPPER);
      threshold->Update();
      this->printMsg(
        msg, 0.3, timer.getElapsedTime(), 1, ttk::debug::LineMode::REPLACE);

      // vtkUnstructuredGrid to vtkPolyData
      auto dataSetSurfaceFilter = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
      dataSetSurfaceFilter->SetInputConnection(0, threshold->GetOutputPort(0));
      dataSetSurfaceFilter->Update();
      this->printMsg(
        msg, 0.5, timer.getElapsedTime(), 1, ttk::debug::LineMode::REPLACE);

      // merge duplicate points
      auto cleanPolyData = vtkSmartPointer<vtkStaticCleanPolyData>::New();
      cleanPolyData->SetInputConnection(
        0, dataSetSurfaceFilter->GetOutputPort(0));
      cleanPolyData->Update();
      this->printMsg(
        msg, 0.7, timer.getElapsedTime(), 1, ttk::debug::LineMode::REPLACE);

      auto connectedComponents = vtkSmartPointer<ttkConnectedComponents>::New();
      connectedComponents->SetInputConnection(0, cleanPolyData->GetOutputPort(0));
      connectedComponents->Update();
      this->printMsg(
        msg, 0.9, timer.getElapsedTime(), 1, ttk::debug::LineMode::REPLACE);

      const auto temp
        = vtkPolyData::SafeDownCast(connectedComponents->GetOutputDataObject(0));
      if(!temp)
        return !this->printErr("Unable to merge points and compute connected "
                              "components of temporal Jacobi edges.");
      components->ShallowCopy(temp);

      this->printMsg(
        msg + " (#" + std::to_string(components->GetNumberOfCells()) + ")", 1,
        timer.getElapsedTime(), 1);
    }

    output->SetBlock(t-1, components);
    continue;

    // auto copy = vtkSmartPointer<vtkUnstructuredGrid>::New();
    // copy->ShallowCopy(jacobiSet);
    // output->SetBlock(t-1, components);

    // iterate over features and find for each critical point its corresponding
    // connected component
    {
      ttk::Timer timer;
      const std::string msg = "Computing Similarity Matrix";
      this->printMsg(msg, 0, 0, 1, ttk::debug::LineMode::REPLACE);

      auto componentIds = components->GetPointData()->GetArray("ComponentId");
      if(!componentIds)
        return !this->printErr(
          "Unable to retrieve componentIds from temporal Jacobi edges");
      auto componentIdData = ttkUtils::GetPointer<int>(componentIds);
      double range[2];
      componentIds->GetRange(range);
      const int nComponents = (int)(range[1]+1);
      std::vector<std::unordered_set<int>> componentsVertexSet(nComponents);

      auto vidComponents = this->GetInputArrayToProcess(1, components);
      if(!vidComponents)
        return !this->printErr("Unable to retrieve vertex identifier arrays from "
                              "connected components.");
      auto vidComponentsData = ttkUtils::GetPointer<int>(vidComponents);

      const int nComponentVertices = vidComponents->GetNumberOfTuples();
      for(int v=0; v<nComponentVertices; v++)
        componentsVertexSet[componentIdData[v]].insert(vidComponentsData[v]);

      auto vidPoints0 = this->GetInputArrayToProcess(1, points0);
      auto vidPoints1 = this->GetInputArrayToProcess(1, points1);
      if(!vidPoints0 || !vidPoints1)
        return !this->printErr(
          "Unable to retrieve vertex identifier arrays from input points.");
      auto vidPoints0Data = ttkUtils::GetPointer<int>(vidPoints0);
      auto vidPoints1Data = ttkUtils::GetPointer<int>(vidPoints1);

      const int nPoints0 = vidPoints0->GetNumberOfTuples();
      const int nPoints1 = vidPoints1->GetNumberOfTuples();

      auto matrix = ttkSimilarityAlgorithm::InitializeMatrix(
        "Match", VTK_UNSIGNED_CHAR, nPoints0, nPoints1
      );
      auto matrixData = ttkUtils::GetPointer<unsigned char>(matrix->GetPointData()->GetArray(0));

      for(int r=0; r<nPoints0; r++)
        for(int c=0; c<nPoints1; c++)
          matrixData[c*nPoints0+r] = 0;

      for(int r=0; r<nPoints0; r++){
        auto vid0 = vidPoints0Data[r];
        for(int j=0; j<nComponents; j++){
          if(componentsVertexSet[j].find(vid0) == componentsVertexSet[j].end()) continue;

          for(int c=0; c<nPoints1; c++){
            auto vid1 = vidPoints1Data[c];
            if(componentsVertexSet[j].find(vid1+nTuplesPerImage) != componentsVertexSet[j].end())
              matrixData[c*nPoints0+r] = 1;
          }
        }
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

      ttkSimilarityAlgorithm::AddIndexIdMaps( matrix, vidPoints0, vidPoints1 );

      output->SetBlock(t-1, matrix);
    }
  }

  return 1;
}
