#include <ttkSimilarityByEuclideanDistance.h>

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

vtkStandardNewMacro(ttkSimilarityByEuclideanDistance);

ttkSimilarityByEuclideanDistance::ttkSimilarityByEuclideanDistance() {}
ttkSimilarityByEuclideanDistance::~ttkSimilarityByEuclideanDistance() {}


int ttkSimilarityByEuclideanDistance::RequestData(vtkInformation *,
                                        vtkInformationVector **inputVector,
                                        vtkInformationVector *outputVector) {

  auto input = vtkMultiBlockDataSet::GetData(inputVector[0]);
  auto output = vtkMultiBlockDataSet::GetData(outputVector);
  const size_t n = input->GetNumberOfBlocks();

  for(size_t t=1; t<n; t++){
    auto p0 = vtkPointSet::SafeDownCast(input->GetBlock(t-1));
    auto p1 = vtkPointSet::SafeDownCast(input->GetBlock(t));
    if(!p0 || !p1)
      return !this->printErr("Input data objects need to be vtkPointSets.");

    const int nPoints0 = p0->GetNumberOfPoints();
    const int nPoints1 = p1->GetNumberOfPoints();

    // get point coordinates
    auto coords0 = p0->GetPoints()->GetData();
    auto coords1 = p1->GetPoints()->GetData();

    const int type = coords0->GetDataType();
    if(type != coords1->GetDataType())
      return !this->printErr("Input vtkPointSet need to have same precision.");

    // initialize similarity matrix i.e., distance matrix
    auto matrix = ttkSimilarityAlgorithm::InitializeMatrix(
      "Distance", type, nPoints0, nPoints1
    );
    auto matrixData = matrix->GetPointData()->GetArray(0);

    int status = 0;
    // compute distance matrix
    ttkTypeMacroR(
      type,
      (status = this->computeDistanceMatrix<T0>(
         ttkUtils::GetPointer<T0>(matrixData),
         ttkUtils::GetPointer<const T0>(coords0),
         ttkUtils::GetPointer<const T0>(coords1), nPoints0, nPoints1)));
    if(!status)
      return 0;

    // normalize distance matrix
    if(this->NormalizeMatrix) {
      ttkTypeMacroR(
        type,
        (status = this->normalizeDistanceMatrix<T0>(
           ttkUtils::GetPointer<T0>(matrixData), nPoints0, nPoints1)));
      if(!status)
        return 0;
    }

    auto indexIdMap0 = this->GetInputArrayToProcess(0, p0);
    auto indexIdMap1 = this->GetInputArrayToProcess(0, p1);
    if(!indexIdMap0 || !indexIdMap1)
      return !this->printErr("Unable to retrieve feature IDs.");

    status = ttkSimilarityAlgorithm::AddIndexIdMaps(
      matrix, indexIdMap0, indexIdMap1);
    if(!status)
      return 0;

    output->SetBlock(t-1,matrix);
  }

  return 1;
}
