#include <ttkPointAdvection.h>

#include <vtkInformation.h>

#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

#include <vtkDoubleArray.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPolyData.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

// std includes
#include <random>

// A VTK macro that enables the instantiation of this class via ::New()
// You do not have to modify this
vtkStandardNewMacro(ttkPointAdvection);

ttkPointAdvection::ttkPointAdvection() {
  this->SetNumberOfInputPorts(0);
  this->SetNumberOfOutputPorts(1);
}

ttkPointAdvection::~ttkPointAdvection() {
}

int ttkPointAdvection::FillInputPortInformation(int port,
                                                vtkInformation *info) {
  return 0;
}

int ttkPointAdvection::FillOutputPortInformation(int port,
                                                 vtkInformation *info) {
  if(port == 0) {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
    return 1;
  }
  return 0;
}

int ttkPointAdvection::initializePoints() {
  // Re-size first timestep for number of points to generate
  this->pointsPerTimestep[0].resize(this->nPoints);

  // Get random number engine and seed it with user provided seed
  std::mt19937 randGen(RandomSeed);

  // Position distributions
  std::uniform_real_distribution<> disX(0.0, 1.0);
  std::uniform_real_distribution<> disY(0.0, 1.0);
  std::uniform_real_distribution<> disZ(0.0, 1.0);

  // Time data distributions
  std::uniform_int_distribution<> disBirth(
    -floor(this->nTimesteps / 2), this->nTimesteps);
  std::uniform_int_distribution<> disDeath(0, this->nTimesteps);
  std::uniform_real_distribution<> disRate(0, 1);

  // Attributes distributions
  std::uniform_real_distribution<> disWeight(
    this->PointWeight[0], this->PointWeight[1]);
  std::uniform_real_distribution<> disConstant(
    this->PointConstant[0], this->PointConstant[1]);

  // Create all new points
  for(int i = 0; i < this->nPoints; i++) {
    auto &p = pointsPerTimestep[0][i];

    // Positions
    p.x = disX(randGen);
    p.y = disY(randGen);
    p.z = disZ(randGen);

    // Point data
    p.pointId = i;

    p.timestep = 0;
    p.birth = disBirth(randGen);
    p.death = disDeath(randGen);
    p.rate = disRate(randGen);

    p.weight = disWeight(randGen);
    p.constant = disConstant(randGen);
  }

  return 1;
}

int ttkPointAdvection::RequestData(vtkInformation *request,
                                   vtkInformationVector **inputVector,
                                   vtkInformationVector *outputVector) {
  // Create output
  auto outputMB = vtkMultiBlockDataSet::GetData(outputVector);

  // Clear previous data and format for number of timesteps
  this->pointsPerTimestep.clear();
  this->pointsPerTimestep.resize(this->nTimesteps);

  // Create initial points
  initializePoints();

  // Determine what vector field to use
  PointAdvection::VectorField vf = PointAdvection::VectorField::PerlinPerturbed;

  switch(this->VecField) {
    case 0: {
      vf = PointAdvection::VectorField::PerlinPerturbed;
      break;
    }
    case 1: {
      vf = PointAdvection::VectorField::PerlinGradient;
      break;
    }
    case 2: {
      vf = PointAdvection::VectorField::PosDiagonal;
      break;
    }
    case 3: {
      vf = PointAdvection::VectorField::PosX;
    }
  }

  // *CALL ADVECTION IN BASE LAYER*
  int status = 0;
  switch(VTK_DOUBLE) {
    vtkTemplateMacro(status = this->integrate<VTK_TT>(
                       pointsPerTimestep, this->nTimesteps, this->TimeInterval,
                       this->StepLength, this->PerlinScaleFactor, vf));
  }
  if(!status) {
    this->printErr("Integration could not be executed");
    return 0;
  }

  // Function for formatting data arrays
  auto prepArray
    = [](vtkDataArray *array, std::string name, int nTuples, int nComponents) {
        array->SetName(name.data());
        array->SetNumberOfComponents(nComponents);
        array->SetNumberOfTuples(nTuples);
        return ttkUtils::GetVoidPointer(array);
      };

  // Format output
  for(int t = 0; t < this->nTimesteps; t++) {
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    auto dataPoints = vtkSmartPointer<vtkPoints>::New();

    // Set point initial data
    int nPointsTimestep = pointsPerTimestep[t].size();
    dataPoints->SetDataType(VTK_DOUBLE);
    dataPoints->SetNumberOfPoints(nPointsTimestep);

    // Create cell arrays off offset array and connectivity array.
    // Our cells are just vertices
    auto offsetArray = vtkSmartPointer<vtkIntArray>::New();
    offsetArray->SetNumberOfTuples(nPointsTimestep + 1);
    auto offsetArrayData
      = static_cast<int *>(ttkUtils::GetVoidPointer(offsetArray));

    auto connectivityArray = vtkSmartPointer<vtkIntArray>::New();
    connectivityArray->SetNumberOfTuples(nPointsTimestep);
    auto connectivityArrayData
      = static_cast<int *>(ttkUtils::GetVoidPointer(connectivityArray));

    auto cellArray = vtkSmartPointer<vtkCellArray>::New();
    cellArray->SetData(offsetArray, connectivityArray);

    // Create arrays for point data
    auto idArray = vtkSmartPointer<vtkIntArray>::New();
    auto idArrayData
      = static_cast<int *>(prepArray(idArray, "PointId", nPointsTimestep, 1));

    auto birthArray = vtkSmartPointer<vtkIntArray>::New();
    auto birthArrayData
      = static_cast<int *>(prepArray(birthArray, "Birth", nPointsTimestep, 1));
    auto deathArray = vtkSmartPointer<vtkIntArray>::New();
    auto deathArrayData
      = static_cast<int *>(prepArray(deathArray, "Death", nPointsTimestep, 1));

    auto weightArray = vtkSmartPointer<vtkDoubleArray>::New();
    auto weightArrayData = static_cast<double *>(
      prepArray(weightArray, "PointWeight", nPointsTimestep, 1));
    auto constantArray = vtkSmartPointer<vtkDoubleArray>::New();
    auto constantArrayData = static_cast<double *>(
      prepArray(constantArray, "PointConstant", nPointsTimestep, 1));

    // Create arrays for field data
    auto timeArray = vtkSmartPointer<vtkDoubleArray>::New();
    auto timeArrayData
      = static_cast<double *>(prepArray(timeArray, "Time", 1, 1));
    timeArrayData[0] = t * this->TimeInterval;

    // Add data to points and arrays
    int idx = 0;
    for(int j = 0; j < nPoints; j++) {
      auto p = pointsPerTimestep[t][j];

      // Set position
      double pos[3] = {p.x, p.y, p.z};
      dataPoints->SetPoint(idx, pos);

      // Calculate the point weight in current timestep
      double pw = (1 / (1 + std::exp(-1 * p.rate * (t - p.birth))))
                  + (1 / (1 + std::exp(-1 * p.rate * (p.death - t)))) - 1;

      // Set data arrays
      idArrayData[idx] = p.pointId;
      birthArrayData[idx] = p.birth;
      deathArrayData[idx] = p.death;
      weightArrayData[idx] = pw * p.weight;
      constantArrayData[idx] = p.constant;

      // Set connectivity and offset array
      connectivityArrayData[idx] = idx;
      offsetArrayData[idx] = idx;
      idx++;
    }

    // Set offset array last index to the number of elements in the connectivity
    // array
    offsetArrayData[nPoints] = nPoints;

    // Format the output data structure into a dataset of vtkPolyData
    polyData->SetPoints(dataPoints);
    polyData->SetVerts(cellArray);

    auto pointData = polyData->GetPointData();
    pointData->AddArray(idArray);
    pointData->AddArray(birthArray);
    pointData->AddArray(deathArray);

    pointData->AddArray(weightArray);
    pointData->AddArray(constantArray);

    polyData->GetFieldData()->AddArray(timeArray);

    // Set data to a block in the output dataset
    size_t nBlocks = outputMB->GetNumberOfBlocks();
    outputMB->SetBlock(nBlocks, polyData);
  }

  // return success
  return 1;
}
