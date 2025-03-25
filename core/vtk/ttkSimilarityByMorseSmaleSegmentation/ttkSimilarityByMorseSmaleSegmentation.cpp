#include <ttkSimilarityByMorseSmaleSegmentation.h>

#include <vtkInformation.h>
#include <vtkObjectFactory.h>

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPointSet.h>

#include <vtkPointData.h>
#include <vtkStringArray.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkSimilarityByMorseSmaleSegmentation);

ttkSimilarityByMorseSmaleSegmentation::ttkSimilarityByMorseSmaleSegmentation() {
  this->SetNumberOfInputPorts(2);
}
ttkSimilarityByMorseSmaleSegmentation::~ttkSimilarityByMorseSmaleSegmentation() {}


int indexFunc0(int i, int j, int n){
  return j*n+i;
};
int indexFunc1(int i, int j, int n){
  return i*n+j;
};

int ttkSimilarityByMorseSmaleSegmentation::RequestData(vtkInformation *,
                                        vtkInformationVector **inputVector,
                                        vtkInformationVector *outputVector) {

  auto inputSegmentations = vtkMultiBlockDataSet::GetData(inputVector[0]);
  auto inputPoints = vtkMultiBlockDataSet::GetData(inputVector[1]);
  const size_t n = inputPoints->GetNumberOfBlocks();

  auto output = vtkMultiBlockDataSet::GetData(outputVector);

  for(size_t t=1; t<n; t++){

    // segmentation data
    auto s0 = vtkDataSet::SafeDownCast(inputSegmentations->GetBlock(t-1));
    auto s1 = vtkDataSet::SafeDownCast(inputSegmentations->GetBlock(t));
    if(!s0 || !s1)
      return !this->printErr("Input segmentations need to be vtkDataSets.");

    auto a0 = this->GetInputArrayToProcess(0, s0);
    auto a1 = this->GetInputArrayToProcess(0, s1);
    auto d0 = this->GetInputArrayToProcess(1, s0);
    auto d1 = this->GetInputArrayToProcess(1, s1);

    // critical point data
    auto p0 = vtkPointSet::SafeDownCast(inputPoints->GetBlock(t-1));
    auto p1 = vtkPointSet::SafeDownCast(inputPoints->GetBlock(t));
    if(!p0 || !p1)
      return !this->printErr("Input points need to be vtkPointSets.");

    const int nPoints0 = p0->GetNumberOfPoints();
    const int nPoints1 = p1->GetNumberOfPoints();

    auto p0types = p0->GetPointData()->GetArray("CriticalType");
    auto p1types = p1->GetPointData()->GetArray("CriticalType");

    auto p0vertexIds = p0->GetPointData()->GetArray("ttkVertexScalarField");
    auto p1vertexIds = p1->GetPointData()->GetArray("ttkVertexScalarField");

    // matrix data
    auto matrix = ttkSimilarityAlgorithm::InitializeMatrix(
      "Forward", VTK_INT, nPoints0, nPoints1
    );
    auto matrixFData = matrix->GetPointData()->GetArray(0);
    matrixFData->Fill(0);
    auto matrixBData = vtkSmartPointer<vtkDataArray>::Take(matrixFData->NewInstance());
    matrixBData->DeepCopy(matrixFData);
    matrixBData->SetName("Backward");
    matrix->GetPointData()->AddArray(matrixBData);

    std::unordered_map<int, int> idIndexMap0;
    std::unordered_map<int, int> idIndexMap1;
    ttkSimilarityAlgorithm::BuildIdIndexMap(idIndexMap0, p0vertexIds);
    ttkSimilarityAlgorithm::BuildIdIndexMap(idIndexMap1, p1vertexIds);

    int status = 0;

    // add id arrays
    {
      status = ttkSimilarityAlgorithm::AddIndexIdMaps(
        matrix,
        p0vertexIds,
        p1vertexIds
      );
      if(!status)
        return 0;
    }

    // forward maps
    for (const auto& pair : {std::pair{0, a1}, std::pair{3, d1}}) {
      status = this->performLookup<int,indexFunc0>(
          ttkUtils::GetPointer<int>(matrixFData),
          nPoints0,
          ttkUtils::GetPointer<unsigned char>(p0types), pair.first,
          ttkUtils::GetPointer<int>(p0vertexIds),
          nPoints0,
          ttkUtils::GetPointer<int>(pair.second),
          idIndexMap0,
          idIndexMap1
      );
      if(!status)
        return 0;
    }
    for (const auto& pair : {std::pair{0, a0}, std::pair{3, d0}}) {
      status = this->performLookup<int,indexFunc1>(
          ttkUtils::GetPointer<int>(matrixBData),
          nPoints0,
          ttkUtils::GetPointer<unsigned char>(p1types), pair.first,
          ttkUtils::GetPointer<int>(p1vertexIds),
          nPoints1,
          ttkUtils::GetPointer<int>(pair.second),
          idIndexMap1,
          idIndexMap0
      );
      if(!status)
        return 0;
    }

    output->SetBlock(t-1,matrix);
  }

  return 1;
}
