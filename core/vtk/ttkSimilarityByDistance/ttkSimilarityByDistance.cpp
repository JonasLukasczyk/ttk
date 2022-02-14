#include <ttkSimilarityByDistance.h>

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

vtkStandardNewMacro(ttkSimilarityByDistance);

ttkSimilarityByDistance::ttkSimilarityByDistance() {
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

ttkSimilarityByDistance::~ttkSimilarityByDistance() {
}

int ttkSimilarityByDistance::ComputeCorrespondences(
  vtkImageData *correspondenceMatrix,
  vtkDataObject *inputDataObjects0,
  vtkDataObject *inputDataObjects1) {
  // unpack input
  auto p0 = vtkPointSet::SafeDownCast(inputDataObjects0);
  auto p1 = vtkPointSet::SafeDownCast(inputDataObjects1);
  if(!p0 || !p1)
    return !this->printErr("Input data objects need to be vtkPointSets.");

  const int nPoints0 = p0->GetNumberOfPoints();
  const int nPoints1 = p1->GetNumberOfPoints();

  // get point coordinates

  auto temp = vtkSmartPointer<vtkFloatArray>::New();
  auto coords0 = nPoints0 > 0 ? p0->GetPoints()->GetData() : temp;
  auto coords1 = nPoints1 > 0 ? p1->GetPoints()->GetData() : temp;

  if(coords0->GetDataType() != coords1->GetDataType())
    return !this->printErr("Input vtkPointSet need to have same precision.");

  // initialize correspondence matrix i.e., distance matrix
  correspondenceMatrix->SetDimensions(nPoints0, nPoints1, 1);
  correspondenceMatrix->AllocateScalars(coords0->GetDataType(), 1);
  auto matrixData = correspondenceMatrix->GetPointData()->GetArray(0);
  matrixData->SetName("Distance");

  int status = 0;

  // compute distance matrix
  ttkTypeMacroR(
    matrixData->GetDataType(),
    (status = this->computeDistanceMatrix<T0>(
       ttkUtils::GetPointer<T0>(matrixData),
       ttkUtils::GetPointer<const T0>(coords0),
       ttkUtils::GetPointer<const T0>(coords1), nPoints0, nPoints1)));
  if(!status)
    return 0;

  // normalize distance matrix
  if(this->NormalizeMatrix) {
    ttkTypeMacroR(
      matrixData->GetDataType(),
      (status = this->normalizeDistanceMatrix<T0>(
         ttkUtils::GetPointer<T0>(matrixData), nPoints0, nPoints1)));
    if(!status)
      return 0;
  }

  status = ttkSimilarityAlgorithm::AddIndexIdMaps(
    correspondenceMatrix, this->GetInputArrayToProcess(0, p0),
    this->GetInputArrayToProcess(0, p1));
  if(!status)
    return 0;

  return 1;
}
