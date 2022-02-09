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

vtkStandardNewMacro(ttkPointAdvection);

ttkPointAdvection::ttkPointAdvection() {
  this->SetNumberOfInputPorts(0);
  this->SetNumberOfOutputPorts(1);
}

ttkPointAdvection::~ttkPointAdvection() {
}

int ttkPointAdvection::FillInputPortInformation(int, vtkInformation *) {
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

int ttkPointAdvection::rampFunction(const int t,
                                    const int lifetime,
                                    double &y) {
  // variables for ramp function, deciding slope, cutoff for constant value
  double cut = std::min(1.0 / 3, 10.0 / lifetime);
  double offset = 0.001;
  double invCut = (1 - offset) / cut;

  double x = double(t) / lifetime;

  if(x < cut || x == cut) {
    y = offset + invCut * x;
  } else if(x > cut && x < (1 - cut)) {
    y = 1.0;
  } else {
    y = offset + invCut - invCut * x;
  }

  return 1;
}

int ttkPointAdvection::formatOutput(vtkPolyData *pd,
                                    std::vector<int> &aliveIds,
                                    const int timestep) {
  // Function for formatting data arrays
  auto prepArray
    = [](vtkDataArray *array, std::string name, int nTuples, int nComponents) {
        array->SetName(name.data());
        array->SetNumberOfComponents(nComponents);
        array->SetNumberOfTuples(nTuples);
        return ttkUtils::GetVoidPointer(array);
      };

  auto dataPoints = vtkSmartPointer<vtkPoints>::New();

  // Set point initial data
  int nPs = aliveIds.size();
  dataPoints->SetDataType(VTK_DOUBLE);
  dataPoints->SetNumberOfPoints(nPs);

  // Create cell arrays off offset array and connectivity array.
  // Our cells are just vertices
  auto offsetArray = vtkSmartPointer<vtkIntArray>::New();
  offsetArray->SetNumberOfTuples(nPs + 1);
  auto offsetArrayData
    = static_cast<int *>(ttkUtils::GetVoidPointer(offsetArray));

  auto connectivityArray = vtkSmartPointer<vtkIntArray>::New();
  connectivityArray->SetNumberOfTuples(nPs);
  auto connectivityArrayData
    = static_cast<int *>(ttkUtils::GetVoidPointer(connectivityArray));

  auto cellArray = vtkSmartPointer<vtkCellArray>::New();
  cellArray->SetData(offsetArray, connectivityArray);

  // Create arrays for point data
  auto idArray = vtkSmartPointer<vtkIntArray>::New();
  auto idArrayData = static_cast<int *>(prepArray(idArray, "PointId", nPs, 1));

  auto birthArray = vtkSmartPointer<vtkIntArray>::New();
  auto birthArrayData
    = static_cast<int *>(prepArray(birthArray, "Birth", nPs, 1));
  auto deathArray = vtkSmartPointer<vtkIntArray>::New();
  auto deathArrayData
    = static_cast<int *>(prepArray(deathArray, "Death", nPs, 1));

  auto weightArray = vtkSmartPointer<vtkDoubleArray>::New();
  auto weightArrayData
    = static_cast<double *>(prepArray(weightArray, "PointWeight", nPs, 1));
  auto constantArray = vtkSmartPointer<vtkDoubleArray>::New();
  auto constantArrayData
    = static_cast<double *>(prepArray(constantArray, "PointConstant", nPs, 1));

  // Create arrays for field data
  auto timeArray = vtkSmartPointer<vtkDoubleArray>::New();
  auto timeArrayData
    = static_cast<double *>(prepArray(timeArray, "Time", 1, 1));
  timeArrayData[0] = timestep * this->TimeInterval;

  // Add data to points and arrays
  int idx = 0;
  for(int j = 0; j < nPs; j++) {
    auto p = this->allPoints[aliveIds[j]];

    // Set position
    double pos[3] = {p.x, p.y, p.z};
    dataPoints->SetPoint(idx, pos);

    // Calculate the point weight in current timestep
    double pw = 0.0;
    rampFunction(timestep - p.birth, p.death - p.birth, pw);

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
  offsetArrayData[nPs] = nPs;

  // Format the output data structure into a dataset of vtkPolyData
  pd->SetPoints(dataPoints);
  pd->SetVerts(cellArray);

  auto pointData = pd->GetPointData();
  pointData->AddArray(idArray);
  pointData->AddArray(birthArray);
  pointData->AddArray(deathArray);

  pointData->AddArray(weightArray);
  pointData->AddArray(constantArray);

  pd->GetFieldData()->AddArray(timeArray);

  return 1;
}

int ttkPointAdvection::RequestData(vtkInformation *,
                                   vtkInformationVector **,
                                   vtkInformationVector *outputVector) {
  // Create output
  auto outputMB = vtkMultiBlockDataSet::GetData(outputVector);

  /* Create sampling distributions */

  // Get random number engine and seed it with user provided seed
  std::mt19937 randGen(RandomSeed);

  // Position distributions
  std::uniform_real_distribution<> disX(0.0, 1.0);
  std::uniform_real_distribution<> disY(0.0, 1.0);
  std::uniform_real_distribution<> disZ(0.0, 1.0);

  // Time data distributions
  std::uniform_int_distribution<> disLifetime(
    this->Lifetime[0], this->Lifetime[1]);
  std::uniform_int_distribution<> disRespawnTime(
    this->RespawnTime[0], this->RespawnTime[1]);
  std::uniform_real_distribution<> disRate(1, 1);

  // Attributes distributions
  std::uniform_real_distribution<> disWeight(
    this->PointWeight[0], this->PointWeight[1]);
  std::uniform_real_distribution<> disConstant(
    this->PointConstant[0], this->PointConstant[1]);

  /* Create initial points */

  // Delete old data and create space for new
  this->allPoints.clear();
  this->allPoints.resize(this->NumberOfPoints);

  // Create all new points
  for(int i = 0; i < this->NumberOfPoints; i++) {
    auto &p = this->allPoints[i];

    // Positions
    p.x = disX(randGen);
    p.y = disY(randGen);
    p.z = disZ(randGen);

    // Point data
    p.pointId = i;

    p.timestep = 0;
    p.birth = 0 + disRespawnTime(randGen);
    p.death = p.birth + disLifetime(randGen);
    p.rate = disRate(randGen);

    p.weight = disWeight(randGen);
    p.constant = disConstant(randGen);
  }

  // maximum point id
  int maxPointId = this->NumberOfPoints - 1;

  /* set base layer variables */

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

  this->setVariables(this->StepLength, this->PerlinScaleFactor, vf);

  /* Loop through all timesteps and advect points */
  std::vector<int> alivePointsIds;
  ttk::Timer timer;
  const std::string msg = "Advecting " + std::to_string(this->NumberOfPoints)
                          + " points in vector field for "
                          + std::to_string(this->NumberOfTimesteps)
                          + " timesteps";
  this->printMsg(msg, 0, 0, this->threadNumber_, ttk::debug::LineMode::REPLACE);
  for(int t = 0; t < this->NumberOfTimesteps; t++) {
    alivePointsIds.clear();

    // Go through all points
    for(int i = 0; i < this->NumberOfPoints; i++) {
      auto &p = this->allPoints[i];

      // Check what points are alive
      if(t >= p.birth && t <= p.death) {
        alivePointsIds.push_back(i);
      } else if(t > p.death) {
        // re-spawn
        ++maxPointId;

        // Positions
        p.x = disX(randGen);
        p.y = disY(randGen);
        p.z = disZ(randGen);

        // Point data
        p.pointId = maxPointId;

        p.timestep = t;
        p.birth = t + disRespawnTime(randGen);
        p.death = p.birth + disLifetime(randGen);
        p.rate = disRate(randGen);

        p.weight = disWeight(randGen);
        p.constant = disConstant(randGen);

        // check case re-spawn time is zero, so the point is immediately added
        if(p.birth == t) {
          alivePointsIds.push_back(i);
        }
      } else if(t < p.birth) {
        // do nothing
        p.timestep = t;
      }
    }

    // Format output
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    this->formatOutput(polyData, alivePointsIds, t);

    // Advect points
    this->advect(this->allPoints, alivePointsIds, t, this->TimeInterval);

    // Set data to a block in the output dataset
    size_t nBlocks = outputMB->GetNumberOfBlocks();
    outputMB->SetBlock(nBlocks, polyData);
  }

  this->printMsg(msg, 1, timer.getElapsedTime(), this->threadNumber_);

  // return success
  return 1;
}
