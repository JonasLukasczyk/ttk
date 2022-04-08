#include <ttkUmbrellaClustering.h>

#include <vtkInformation.h>
#include <vtkObjectFactory.h>

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPointSet.h>
#include <vtkPolyData.h>

#include <vtkCellData.h>
#include <vtkIdList.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>
#include <vtkStringArray.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkUmbrellaClustering);

ttkUmbrellaClustering::ttkUmbrellaClustering() {
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(5);
}

ttkUmbrellaClustering::~ttkUmbrellaClustering() {
}

int ttkUmbrellaClustering::FillOutputPortInformation(int port,
                                                     vtkInformation *info) {
  if(port == 0 || port == 1 || port == 2 || port == 3 || port == 4) {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
    return 1;
  }
  return 0;
}

int ttkUmbrellaClustering::ComputeSimilarityMatrix(
  vtkImageData *similarityMatrix,
  vtkDataObject *inputDataObjects0,
  vtkDataObject *inputDataObjects1) {
  // unpack input
  auto p0 = vtkPointSet::SafeDownCast(inputDataObjects0);
  auto p1 = vtkPointSet::SafeDownCast(inputDataObjects1);
  if(!p0 || !p1)
    return !this->printErr("Input data objects need to be vtkPointSets.");

  const int nPoints0 = p0->GetNumberOfPoints();
  const int nPoints1 = p1->GetNumberOfPoints();

  // Get ids
  auto ids0 = GetInputArrayToProcess(0, p0);
  auto ids1 = GetInputArrayToProcess(0, p1);
  if(!ids0 || !ids1)
    return !this->printErr("Input data is missing array containing ids.");

  // Get point weights
  auto pws0 = GetInputArrayToProcess(2, p0);
  auto pws1 = GetInputArrayToProcess(2, p1);
  if(!pws0 || !pws1)
    return !this->printErr(
      "Input data is missing array containing point weights.");

  // Get point constants
  auto pcs0 = GetInputArrayToProcess(3, p0);
  auto pcs1 = GetInputArrayToProcess(3, p1);
  if(!pcs0 || !pcs1)
    return !this->printErr(
      "Input data is missing array containing point constants.");

  // Get point coords
  auto coords0 = p0->GetPoints()->GetData();
  auto coords1 = p1->GetPoints()->GetData();

  if(coords0->GetDataType() != coords1->GetDataType())
    return !this->printErr("Input vtkPointSet need to have same precision.");

  // Check if both data objects umbrella clusters should be calculated
  if(umbrellasPerTimestep.size() == 0) {
    std::map<int, std::vector<int>> umb0;
    std::map<int, std::vector<int>> umb1;

    int status = 0;

    ttkTypeMacroR(coords0->GetDataType(),
                  (status = this->computeUmbrellas<T0>(
                     umb0, ttkUtils::GetPointer<const T0>(coords0),
                     ttkUtils::GetPointer<const T0>(pws0),
                     ttkUtils::GetPointer<const T0>(pcs0), nPoints0)));
    if(!status)
      return 0;

    ttkTypeMacroR(coords1->GetDataType(),
                  (status = this->computeUmbrellas<T0>(
                     umb1, ttkUtils::GetPointer<const T0>(coords1),
                     ttkUtils::GetPointer<const T0>(pws1),
                     ttkUtils::GetPointer<const T0>(pcs1), nPoints1)));

    if(!status)
      return 0;

    umbrellasPerTimestep.push_back(umb0);
    umbrellasPerTimestep.push_back(umb1);
  } else {
    std::map<int, std::vector<int>> umb1;

    int status = 0;

    ttkTypeMacroR(coords1->GetDataType(),
                  (status = this->computeUmbrellas<T0>(
                     umb1, ttkUtils::GetPointer<const T0>(coords1),
                     ttkUtils::GetPointer<const T0>(pws1),
                     ttkUtils::GetPointer<const T0>(pcs1), nPoints1)));

    if(!status)
      return 0;

    umbrellasPerTimestep.push_back(umb1);
  }
  auto &umb0 = umbrellasPerTimestep[umbrellasPerTimestep.size() - 2];
  auto &umb1 = umbrellasPerTimestep[umbrellasPerTimestep.size() - 1];

  int numumbrellaIds0 = umb0.size();
  int numumbrellaIds1 = umb1.size();

  // initialize similarity matrix i.e., umbrella overlap matrix
  similarityMatrix->SetDimensions(numumbrellaIds0, numumbrellaIds1, 1);
  similarityMatrix->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  auto matrixData = similarityMatrix->GetPointData()->GetArray(0);
  matrixData->SetName("Umbrella");

  // compute umbrella matrix
  int status = 0;
  ttkTypeMacroI(
    ids0->GetDataType(),
    status = this->computeUmbrellaMatrix<T0>(
      ttkUtils::GetPointer<unsigned char>(matrixData), umb0, umb1,
      ttkUtils::GetPointer<const T0>(ids0),
      ttkUtils::GetPointer<const T0>(ids1), numumbrellaIds0, numumbrellaIds1));
  if(!status)
    return 0;

  // create id vector for umbrellas connected to the values in the matrix
  auto umbrellaIds0 = vtkSmartPointer<vtkDataArray>::Take(ids0->NewInstance());
  auto umbrellaIds1 = vtkSmartPointer<vtkDataArray>::Take(ids1->NewInstance());
  umbrellaIds0->SetName(ids0->GetName());
  umbrellaIds1->SetName(ids1->GetName());
  umbrellaIds0->SetNumberOfComponents(1);
  umbrellaIds1->SetNumberOfComponents(1);
  umbrellaIds0->SetNumberOfTuples(numumbrellaIds0);
  umbrellaIds1->SetNumberOfTuples(numumbrellaIds1);

  int index = 0;
  for(const auto &i : umb0) {
    umbrellaIds0->SetTuple1(index, ids0->GetTuple1(i.first));
    index++;
  }

  index = 0;
  for(const auto &i : umb1) {
    umbrellaIds1->SetTuple1(index, ids1->GetTuple1(i.first));
    index++;
  }

  status = ttkSimilarityAlgorithm::AddIndexIdMaps(
    similarityMatrix, umbrellaIds0, umbrellaIds1);
  if(!status)
    return 0;

  return 1;
}

int ttkUmbrellaClustering::ComputeThresholdedClustering(
  vtkImageData *similarityMatrix,
  vtkDataObject *inputDataObjects0,
  vtkDataObject *inputDataObjects1) {

  // unpack input
  auto p0 = vtkPointSet::SafeDownCast(inputDataObjects0);
  auto p1 = vtkPointSet::SafeDownCast(inputDataObjects1);
  if(!p0 || !p1)
    return !this->printErr("Input data objects need to be vtkPointSets.");

  const int nPoints0 = p0->GetNumberOfPoints();
  const int nPoints1 = p1->GetNumberOfPoints();

  // Get ids
  auto ids0 = GetInputArrayToProcess(0, p0);
  auto ids1 = GetInputArrayToProcess(0, p1);
  if(!ids0 || !ids1)
    return !this->printErr("Input data is missing array containing ids.");

  // Get point weights
  auto pws0 = GetInputArrayToProcess(2, p0);
  auto pws1 = GetInputArrayToProcess(2, p1);
  if(!pws0 || !pws1)
    return !this->printErr(
      "Input data is missing array containing point weights.");

  // Get point constants
  auto pcs0 = GetInputArrayToProcess(3, p0);
  auto pcs1 = GetInputArrayToProcess(3, p1);
  if(!pcs0 || !pcs1)
    return !this->printErr(
      "Input data is missing array containing point constants.");

  // Get point coords
  auto coords0 = p0->GetPoints()->GetData();
  auto coords1 = p1->GetPoints()->GetData();

  if(coords0->GetDataType() != coords1->GetDataType())
    return !this->printErr("Input vtkPointSet need to have same precision.");

  // Check if both data objects thresholded umbrella ids should be calculated
  if(threshUmbrellasPerTimestep.size() == 0) {
    std::map<int, std::vector<int>> tumb0;
    std::map<int, std::vector<int>> tumb1;

    int status = 0;

    ttkTypeMacroR(
      coords0->GetDataType(), (status = this->computeThresholdedUmbrellas<T0>(
                                 tumb0, ttkUtils::GetPointer<const T0>(coords0),
                                 ttkUtils::GetPointer<const T0>(pws0),
                                 ttkUtils::GetPointer<const T0>(pcs0),
                                 this->ScalarThreshold, nPoints0)));
    if(!status)
      return 0;

    ttkTypeMacroR(
      coords1->GetDataType(), (status = this->computeThresholdedUmbrellas<T0>(
                                 tumb1, ttkUtils::GetPointer<const T0>(coords1),
                                 ttkUtils::GetPointer<const T0>(pws1),
                                 ttkUtils::GetPointer<const T0>(pcs1),
                                 this->ScalarThreshold, nPoints1)));
    if(!status)
      return 0;

    threshUmbrellasPerTimestep.push_back(tumb0);
    threshUmbrellasPerTimestep.push_back(tumb1);
  } else {
    std::map<int, std::vector<int>> tumb1;

    int status = 0;

    ttkTypeMacroR(
      coords1->GetDataType(), (status = this->computeThresholdedUmbrellas<T0>(
                                 tumb1, ttkUtils::GetPointer<const T0>(coords1),
                                 ttkUtils::GetPointer<const T0>(pws1),
                                 ttkUtils::GetPointer<const T0>(pcs1),
                                 this->ScalarThreshold, nPoints1)));
    if(!status)
      return 0;

    threshUmbrellasPerTimestep.push_back(tumb1);
  }

  auto &tumb0
    = threshUmbrellasPerTimestep[threshUmbrellasPerTimestep.size() - 2];
  auto &tumb1
    = threshUmbrellasPerTimestep[threshUmbrellasPerTimestep.size() - 1];

  int ntumb0 = 0;
  int ntumb1 = 0;
  for(const auto &u : tumb0) {
    if(u.second[0] != -1)
      ntumb0++;
  }
  for(const auto &u : tumb1) {
    if(u.second[0] != -1)
      ntumb1++;
  }

  // initialize similarity matrix i.e., umbrella overlap matrix
  similarityMatrix->SetDimensions(ntumb0, ntumb1, 1);
  similarityMatrix->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  auto matrixData = similarityMatrix->GetPointData()->GetArray(0);
  matrixData->SetName("ThreshUmbrella");

  // compute thresholded umbrella matrix
  int status = 0;
  ttkTypeMacroI(ids0->GetDataType(),
                status = this->computeThresholdedUmbrellaMatrix<T0>(
                  ttkUtils::GetPointer<unsigned char>(matrixData), tumb0, tumb1,
                  ttkUtils::GetPointer<const T0>(ids0),
                  ttkUtils::GetPointer<const T0>(ids1), ntumb0, ntumb1));
  if(!status)
    return 0;

  // create id vector for thresholded umbrellas connected to the values in the
  // matrix
  auto tumbrellaIds0 = vtkSmartPointer<vtkDataArray>::Take(ids0->NewInstance());
  auto tumbrellaIds1 = vtkSmartPointer<vtkDataArray>::Take(ids1->NewInstance());
  tumbrellaIds0->SetName(ids0->GetName());
  tumbrellaIds1->SetName(ids1->GetName());
  tumbrellaIds0->SetNumberOfComponents(1);
  tumbrellaIds1->SetNumberOfComponents(1);
  tumbrellaIds0->SetNumberOfTuples(ntumb0);
  tumbrellaIds1->SetNumberOfTuples(ntumb1);

  int index = 0;
  for(const auto &i : tumb0) {
    if(i.second[0] != -1) {
      tumbrellaIds0->SetTuple1(index, ids0->GetTuple1(i.first));
      index++;
    }
  }

  index = 0;
  for(const auto &i : tumb1) {
    if(i.second[0] != -1) {
      tumbrellaIds1->SetTuple1(index, ids1->GetTuple1(i.first));
      index++;
    }
  }

  status = ttkSimilarityAlgorithm::AddIndexIdMaps(
    similarityMatrix, tumbrellaIds0, tumbrellaIds1);
  if(!status)
    return 0;

  return 1;
}

int ttkUmbrellaClustering::AddUmbrellaIds(vtkDataObject *inputDataObjects,
                                          const size_t t) {
  auto p0 = vtkPointSet::SafeDownCast(inputDataObjects);
  if(!p0)
    return !this->printErr("No points");

  // Get number of points
  const int nPoints = p0->GetNumberOfPoints();

  // Do nothing if there are no points
  if(nPoints == 0)
    return 1;

  // Get ids
  auto ids = GetInputArrayToProcess(0, p0);
  if(!ids)
    return !this->printErr("Input data is missing array containing ids.");

  // Add umbrella and tresholded umbrella ids to point output
  auto umbrellaIds = vtkSmartPointer<vtkIntArray>::New();
  umbrellaIds->SetName("UmbrellaId");
  umbrellaIds->SetNumberOfComponents(1);
  umbrellaIds->SetNumberOfTuples(nPoints);

  auto tumbrellaIds = vtkSmartPointer<vtkIntArray>::New();
  tumbrellaIds->SetName("ThreshUmbrellaId");
  tumbrellaIds->SetNumberOfComponents(1);
  tumbrellaIds->SetNumberOfTuples(nPoints);

  for(const auto &u : umbrellasPerTimestep[t]) {
    for(long unsigned int j = 0; j < u.second.size(); j++) {
      umbrellaIds->SetTuple1(u.second[j], ids->GetTuple1(u.first));
    }
  }

  for(const auto &u : threshUmbrellasPerTimestep[t]) {
    for(long unsigned int j = 0; j < u.second.size(); j++) {
      if(u.second[j] == -1)
        tumbrellaIds->SetTuple1(u.first, -1);
      else
        tumbrellaIds->SetTuple1(u.second[j], ids->GetTuple1(u.first));
    }
  }

  p0->GetPointData()->AddArray(umbrellaIds);
  p0->GetPointData()->AddArray(tumbrellaIds);

  return 1;
}

int ttkUmbrellaClustering::FormatClusters(vtkDataObject *inputDataObjects,
                                          vtkPolyData *outputPoints,
                                          const size_t t) {
  auto inPointSet = vtkPointSet::SafeDownCast(inputDataObjects);
  if(!inPointSet)
    return !this->printErr("No points");

  auto inPD = inPointSet->GetPointData();
  auto inCD = inPointSet->GetCellData();

  int nPoints = inPointSet->GetNumberOfPoints();
  // Do nothing if there are no points
  if(nPoints == 0)
    return 1;

  // Get ids
  auto ids = GetInputArrayToProcess(0, inPointSet);
  if(!ids)
    return !this->printErr("Input data is missing array containing ids.");

  // Create new points and allocate point data
  auto newPoints = vtkSmartPointer<vtkPoints>::New();
  auto outPD = outputPoints->GetPointData();
  auto outCD = outputPoints->GetCellData();
  outPD->CopyAllocate(inPD);
  outCD->CopyAllocate(inCD);
  outputPoints->Allocate(inPointSet->GetNumberOfCells());
  newPoints->SetDataType(inPointSet->GetPoints()->GetDataType());

  // Copy all data from points to the new clusters
  double pPos[3];
  auto vertId = vtkSmartPointer<vtkIdList>::New();
  for(const auto &u : umbrellasPerTimestep[t]) {
    inPointSet->GetPoint(u.first, pPos);
    int p = newPoints->InsertNextPoint(pPos);
    vertId->InsertId(0, p);
    int c = outputPoints->InsertNextCell(VTK_VERTEX, vertId);
    outPD->CopyData(inPD, u.first, p);
    outCD->CopyData(inCD, u.first, c);
  }

  outputPoints->SetPoints(newPoints);
  outputPoints->Squeeze();
  outputPoints->GetFieldData()->DeepCopy(inPointSet->GetFieldData());

  return 1;
}

int ttkUmbrellaClustering::FormatThresholdedClusters(
  vtkDataObject *inputDataObjects, vtkPolyData *outputPoints, const size_t t) {
  auto inPointSet = vtkPolyData::SafeDownCast(inputDataObjects);
  if(!inPointSet)
    return !this->printErr("No points");

  auto inPD = inPointSet->GetPointData();
  auto inCD = inPointSet->GetCellData();

  int nClusters = inPointSet->GetNumberOfPoints();
  // Do nothing if there are no points
  if(nClusters == 0)
    return 1;

  // Get ids
  auto ids = GetInputArrayToProcess(0, inPointSet);
  if(!ids)
    return !this->printErr("Input data is missing array containing ids.");

  // Create new points and allocate point data
  auto newPoints = vtkSmartPointer<vtkPoints>::New();
  auto outPD = outputPoints->GetPointData();
  auto outCD = outputPoints->GetCellData();
  outPD->CopyAllocate(inPD);
  outCD->CopyAllocate(inCD);
  outputPoints->Allocate(inPointSet->GetNumberOfCells());
  newPoints->SetDataType(inPointSet->GetPoints()->GetDataType());

  // Copy all data from points to the new clusters
  double pPos[3];
  auto vertId = vtkSmartPointer<vtkIdList>::New();
  for(const auto &u : threshUmbrellasPerTimestep[t]) {
    if(u.second[0] != -1) {
      inPointSet->GetPoint(u.first, pPos);
      int p = newPoints->InsertNextPoint(pPos);
      vertId->InsertId(0, p);
      int c = outputPoints->InsertNextCell(VTK_VERTEX, vertId);
      outPD->CopyData(inPD, u.first, p);
      outCD->CopyData(inCD, u.first, c);
    }
  }

  outputPoints->SetPoints(newPoints);
  outputPoints->Squeeze();
  outputPoints->GetFieldData()->DeepCopy(inPointSet->GetFieldData());

  return 1;
}

int ttkUmbrellaClustering::RequestData(vtkInformation *request,
                                       vtkInformationVector **inputVector,
                                       vtkInformationVector *outputVector) {

  // Clear previous data
  umbrellasPerTimestep.clear();
  threshUmbrellasPerTimestep.clear();

  // Calculate umbrella matrices
  this->ttkSimilarityAlgorithm::RequestData(request, inputVector, outputVector);

  // Copy input
  auto input = vtkMultiBlockDataSet::GetData(inputVector[0]);
  auto pointsOut = vtkMultiBlockDataSet::GetData(outputVector, 1);
  pointsOut->DeepCopy(input);

  // Calculate thresholded umbrella matrices
  auto threshMatrices = vtkMultiBlockDataSet::GetData(outputVector, 4);
  for(size_t t = 1; t < input->GetNumberOfBlocks(); t++) {
    auto similarityMatrix = vtkSmartPointer<vtkImageData>::New();
    if(!ComputeThresholdedClustering(
         similarityMatrix, pointsOut->GetBlock(t - 1), pointsOut->GetBlock(t)))
      return 0;
    threshMatrices->SetBlock(t - 1, similarityMatrix);
  }

  /* FORMATTING OUTPUT */
  ttk::Timer timer;
  const std::string msg = "Add Feature Arrays to Points";
  this->printMsg(msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);
  for(size_t t = 0; t < pointsOut->GetNumberOfBlocks(); t++) {
    if(!AddUmbrellaIds(pointsOut->GetBlock(t), t))
      return 0;
  }
  this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

  const std::string msg2 = "Create Umbrella Cluster Output";
  this->printMsg(
    msg2, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

  // Create and format all clusters via function
  auto clustersOut = vtkMultiBlockDataSet::GetData(outputVector, 2);
  for(size_t t = 0; t < pointsOut->GetNumberOfBlocks(); t++) {
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    if(!FormatClusters(pointsOut->GetBlock(t), polyData, t))
      return 0;
    clustersOut->SetBlock(t, polyData);
  }
  this->printMsg(msg2, 1, timer.getElapsedTime(), this->threadNumber_);

  const std::string msg3 = "Create Thresholded Umbrella Cluster Output";
  this->printMsg(
    msg3, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

  // Create and format all thresholded clusters via function
  auto threshClustersOut = vtkMultiBlockDataSet::GetData(outputVector, 3);
  for(size_t t = 0; t < input->GetNumberOfBlocks(); t++) {
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    if(!FormatThresholdedClusters(pointsOut->GetBlock(t), polyData, t))
      return 0;
    threshClustersOut->SetBlock(t, polyData);
  }
  this->printMsg(msg3, 1, timer.getElapsedTime(), this->threadNumber_);

  return 1;
}
