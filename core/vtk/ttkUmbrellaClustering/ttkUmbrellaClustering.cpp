#include <ttkUmbrellaClustering.h>

#include <vtkInformation.h>
#include <vtkObjectFactory.h>

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPointSet.h>

#include <vtkIntArray.h>
#include <vtkPointData.h>
#include <vtkStringArray.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkUmbrellaClustering);

ttkUmbrellaClustering::ttkUmbrellaClustering() {
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(2);
}

ttkUmbrellaClustering::~ttkUmbrellaClustering() {
}

int ttkUmbrellaClustering::FillOutputPortInformation(int port,
                                                     vtkInformation *info) {
  if(port == 0 || port == 1) {
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
  auto temp = vtkSmartPointer<vtkFloatArray>::New();
  auto coords0 = nPoints0 > 0 ? p0->GetPoints()->GetData() : temp;
  auto coords1 = nPoints1 > 0 ? p1->GetPoints()->GetData() : temp;

  if(coords0->GetDataType() != coords1->GetDataType())
    return this->printErr("Input vtkPointSet need to have same precision.");

  // Check if both data objects feature ids should be calculated
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

  int numumbrellaIds0
    = umbrellasPerTimestep[umbrellasPerTimestep.size() - 2].size();
  int numumbrellaIds1
    = umbrellasPerTimestep[umbrellasPerTimestep.size() - 1].size();

  // initialize similarity matrix i.e., umbrella overlap matrix
  similarityMatrix->SetDimensions(numumbrellaIds0, numumbrellaIds1, 1);
  similarityMatrix->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  auto matrixData = similarityMatrix->GetPointData()->GetArray(0);
  matrixData->SetName("Umbrella");

  // compute umbrella matrix
  int status = 0;
  status = this->computeUmbrellaMatrix(
    ttkUtils::GetPointer<unsigned char>(matrixData),
    umbrellasPerTimestep[umbrellasPerTimestep.size() - 2],
    umbrellasPerTimestep[umbrellasPerTimestep.size() - 1], numumbrellaIds0,
    numumbrellaIds1);
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
  for(const auto &i : umbrellasPerTimestep[umbrellasPerTimestep.size() - 2]) {
    umbrellaIds0->SetTuple1(index, ids0->GetTuple1(i.first));
    index++;
  }
  index = 0;
  for(const auto &i : umbrellasPerTimestep[umbrellasPerTimestep.size() - 1]) {
    umbrellaIds1->SetTuple1(index, ids1->GetTuple1(i.first));
    index++;
  }

  status = ttkSimilarityAlgorithm::AddIndexIdMaps(
    similarityMatrix, umbrellaIds0, umbrellaIds1);
  if(!status)
    return 0;

  return 1;
}

int ttkUmbrellaClustering::AddUmbrellaIds(vtkDataObject *inputDataObjects,
                                          const size_t t) {
  // unpack input
  auto p0 = vtkPointSet::SafeDownCast(inputDataObjects);
  if(!p0)
    return !this->printErr("No points");

  // Get number of points
  const int nPoints = p0->GetNumberOfPoints();

  if(nPoints == 0)
    return !this->printErr("Zero points");

  // Get ids
  auto ids = GetInputArrayToProcess(0, p0);
  if(!ids)
    return !this->printErr("Input data is missing array containing ids.");

  // add umbrella ids to point output
  auto umbrellaIds = vtkSmartPointer<vtkIntArray>::New();
  umbrellaIds->SetName("UmbrellaId");
  umbrellaIds->SetNumberOfComponents(1);
  umbrellaIds->SetNumberOfTuples(nPoints);

  for(const auto &u : umbrellasPerTimestep[t]) {
    for(long unsigned int j = 0; j < u.second.size(); j++) {
      umbrellaIds->SetTuple1(u.second[j], ids->GetTuple1(u.first));
    }
  }
  p0->GetPointData()->AddArray(umbrellaIds);

  return 1;
}

int ttkUmbrellaClustering::RequestData(vtkInformation *request,
                                       vtkInformationVector **inputVector,
                                       vtkInformationVector *outputVector) {

  // Clear previous data
  umbrellasPerTimestep.clear();

  // Calculate matrices
  this->ttkSimilarityAlgorithm::RequestData(request, inputVector, outputVector);

  // Copy input and add umbrella ids to all blocks
  ttk::Timer timer;
  const std::string msg = "Add Umbrella Ids to Points";
  this->printMsg(msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);

  auto pointsOut = vtkMultiBlockDataSet::GetData(outputVector, 1);
  pointsOut->DeepCopy(vtkMultiBlockDataSet::GetData(inputVector[0]));

  for(size_t t = 0; t < pointsOut->GetNumberOfBlocks(); t++) {
    if(!AddUmbrellaIds(pointsOut->GetBlock(t), t))
      return 0;
  }

  this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

  return 1;
}
