#include <ttkSimilarityByGradient.h>

#include <vtkDataSet.h>
#include <vtkInformation.h>
#include <vtkMultiBlockDataSet.h>

#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkSimilarityByGradient);

ttkSimilarityByGradient::ttkSimilarityByGradient() {
  this->SetNumberOfInputPorts(2);
  this->SetNumberOfOutputPorts(1);
}
ttkSimilarityByGradient::~ttkSimilarityByGradient() {
}

vtkIntArray *GetVertexIdArray(vtkDataSet *input) {
  return vtkIntArray::SafeDownCast(
    input->GetPointData()->GetArray("ttkVertexScalarField"));
}

int ttkSimilarityByGradient::ComputeSimilarityMatrix(
  vtkImageData *similarityMatrix,
  vtkDataObject *inputDataObjects0,
  vtkDataObject *inputDataObjects1) {
  auto inputs0AsMB = static_cast<vtkMultiBlockDataSet *>(inputDataObjects0);
  auto inputs1AsMB = static_cast<vtkMultiBlockDataSet *>(inputDataObjects1);

  auto domain0 = vtkDataSet::SafeDownCast(inputs0AsMB->GetBlock(0));
  auto domain1 = vtkDataSet::SafeDownCast(inputs1AsMB->GetBlock(0));
  auto seeds0 = vtkDataSet::SafeDownCast(inputs0AsMB->GetBlock(1));
  auto seeds1 = vtkDataSet::SafeDownCast(inputs1AsMB->GetBlock(1));

  int nFeatures0 = seeds0->GetNumberOfElements(0);
  int nFeatures1 = seeds1->GetNumberOfElements(0);

  // allocate correlation matrices
  similarityMatrix->SetDimensions(nFeatures0, nFeatures1, 1);
  similarityMatrix->AllocateScalars(VTK_INT, 1);

  auto forward = similarityMatrix->GetPointData()->GetArray(0);
  forward->SetName("Forward");

  auto backward = vtkSmartPointer<vtkIntArray>::New();
  backward->DeepCopy(forward);
  backward->SetName("Backward");
  similarityMatrix->GetPointData()->AddArray(backward);

  auto orderArray0 = ttkAlgorithm::GetOrderArray(domain0, 0);
  auto orderArray1 = ttkAlgorithm::GetOrderArray(domain1, 0);

  int status = 0;
  auto triangulation = this->GetTriangulation(domain0);
  this->preconditionTriangulation(triangulation);

  ttkTypeMacroT(
    triangulation->getType(),
    (status = this->computeSimilarityMatrix<ttk::SimplexId, T0>(
       ttkUtils::GetPointer<int>(forward),

       ttkUtils::GetPointer<ttk::SimplexId>(orderArray1),
       static_cast<T0 *>(triangulation->getData()),

       ttkUtils::GetPointer<ttk::SimplexId>(GetVertexIdArray(seeds0)),
       ttkUtils::GetPointer<ttk::SimplexId>(GetVertexIdArray(seeds1)),
       nFeatures0, nFeatures1, std::greater<ttk::SimplexId>{},
       [](ttk::SimplexId i, ttk::SimplexId j, ttk::SimplexId n,
          ttk::SimplexId) { return j * n + i; })));
  if(!status)
    return 0;

  ttkTypeMacroT(
    triangulation->getType(),
    (status = this->computeSimilarityMatrix<ttk::SimplexId, T0>(
       ttkUtils::GetPointer<int>(backward),

       ttkUtils::GetPointer<ttk::SimplexId>(orderArray0),
       static_cast<T0 *>(triangulation->getData()),

       ttkUtils::GetPointer<ttk::SimplexId>(GetVertexIdArray(seeds1)),
       ttkUtils::GetPointer<ttk::SimplexId>(GetVertexIdArray(seeds0)),
       nFeatures1, nFeatures0, std::greater<ttk::SimplexId>{},
       [](ttk::SimplexId i, ttk::SimplexId j, ttk::SimplexId,
          ttk::SimplexId m) { return i * m + j; })));
  if(!status)
    return 0;

  status = this->AddIndexIdMaps(similarityMatrix,
                                this->GetInputArrayToProcess(1, seeds0),
                                this->GetInputArrayToProcess(1, seeds1));
  if(!status)
    return 0;

  return 1;
}
