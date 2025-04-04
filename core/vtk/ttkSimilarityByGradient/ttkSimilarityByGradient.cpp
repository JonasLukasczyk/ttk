#include <ttkSimilarityByGradient.h>

#include <vtkDataSet.h>
#include <vtkInformation.h>
#include <vtkMultiBlockDataSet.h>

#include <vtkIdTypeArray.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkDoubleArray.h>

#include <vtkPointData.h>
#include <vtkPointSet.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkSimilarityByGradient);

ttkSimilarityByGradient::ttkSimilarityByGradient() {
  this->SetNumberOfInputPorts(2);
  this->SetNumberOfOutputPorts(1);
}
ttkSimilarityByGradient::~ttkSimilarityByGradient() {
}

ttkSimplexIdTypeArray *GetVertexIdArray(vtkDataSet *input) {
  return ttkSimplexIdTypeArray::SafeDownCast(
    input->GetPointData()->GetArray("ttkVertexScalarField"));
}

int ttkSimilarityByGradient::RequestData(vtkInformation *,
                                      vtkInformationVector **inputVector,
                                      vtkInformationVector *outputVector) {
  auto inputSegmentations = vtkMultiBlockDataSet::GetData(inputVector[0]);
  auto inputPoints = vtkMultiBlockDataSet::GetData(inputVector[1]);
  const size_t n = inputPoints->GetNumberOfBlocks();

  auto output = vtkMultiBlockDataSet::GetData(outputVector);

  for(size_t t=1; t<n; t++){
    // segmentation data
    auto domain0 = vtkDataSet::SafeDownCast(inputSegmentations->GetBlock(t-1));
    auto domain1 = vtkDataSet::SafeDownCast(inputSegmentations->GetBlock(t));
    if(!domain0 || !domain1)
      return !this->printErr("Input segmentations need to be vtkDataSets.");

    auto seeds0 = vtkPointSet::SafeDownCast(inputPoints->GetBlock(t-1));
    auto seeds1 = vtkPointSet::SafeDownCast(inputPoints->GetBlock(t));
    if(!seeds0 || !seeds1)
      return !this->printErr("Input points need to be vtkPointSets.");

    int totalFeatures0 = seeds0->GetNumberOfElements(0);
    int totalFeatures1 = seeds1->GetNumberOfElements(0);

    auto triangulation = this->GetTriangulation(domain0);
    this->preconditionTriangulation(triangulation);
    int totalVertices = triangulation->getNumberOfVertices();

    auto manifoldSeg0 = ttkAlgorithm::GetInputArrayToProcess(2, domain0);
    auto manifoldSeg1 = ttkAlgorithm::GetInputArrayToProcess(2, domain1);

    auto vertexIds0 = GetVertexIdArray(seeds0);
    auto vertexIds1 = GetVertexIdArray(seeds1);

    if(!manifoldSeg0 || !manifoldSeg1)
      return !this->printErr("No manifold arrays.");

    // allocate similarity matrices
    auto similarityMatrix = ttkSimilarityAlgorithm::InitializeMatrix(
      "Forward", VTK_INT, totalFeatures0, totalFeatures1
    );

    auto forward = similarityMatrix->GetPointData()->GetArray(0);

    auto backward = vtkSmartPointer<vtkIntArray>::New();
    backward->DeepCopy(forward);
    backward->SetName("Backward");
    similarityMatrix->GetPointData()->AddArray(backward);

    auto forwardCombi = vtkSmartPointer<vtkIntArray>::New();
    forwardCombi->DeepCopy(forward);
    forwardCombi->SetName("ForwardCombi");
    similarityMatrix->GetPointData()->AddArray(forwardCombi);

    auto backwardCombi = vtkSmartPointer<vtkIntArray>::New();
    backwardCombi->DeepCopy(forward);
    backwardCombi->SetName("BackwardCombi");
    similarityMatrix->GetPointData()->AddArray(backwardCombi);

    auto forwardDist = vtkSmartPointer<vtkIntArray>::New();
    forwardDist->DeepCopy(forward);
    forwardDist->SetName("ForwardDist");
    similarityMatrix->GetPointData()->AddArray(forwardDist);

    auto backwardDist = vtkSmartPointer<vtkIntArray>::New();
    backwardDist->DeepCopy(forward);
    backwardDist->SetName("BackwardDist");
    similarityMatrix->GetPointData()->AddArray(backwardDist);

    auto forManifold = vtkSmartPointer<vtkDoubleArray>::New();
    forManifold->SetNumberOfTuples(totalFeatures0*totalFeatures1);
    forManifold->SetNumberOfComponents(1);
    forManifold->SetName("ForwardManifold");
    similarityMatrix->GetPointData()->AddArray(forManifold);

    auto backManifold = vtkSmartPointer<vtkDoubleArray>::New();
    backManifold->SetNumberOfTuples(totalFeatures0*totalFeatures1);
    backManifold->SetNumberOfComponents(1);
    backManifold->SetName("BackwardManifold");
    similarityMatrix->GetPointData()->AddArray(backManifold);

    // Create mappings between ids and row/column indices for matrix creation
    std::unordered_map<ttk::SimplexId, ttk::SimplexId> map0{};
    std::unordered_map<ttk::SimplexId, ttk::SimplexId> map1{};

    int status = ttkSimilarityAlgorithm::BuildIdIndexMap(map0, vertexIds0);
    status = ttkSimilarityAlgorithm::BuildIdIndexMap(map1, vertexIds1);

    // one-to-one
    std::vector<std::map<ttk::SimplexId, int>> forwardMap(totalFeatures0);
    std::vector<std::map<ttk::SimplexId, int>> backwardMap(totalFeatures1);

    // combinatorial
    std::vector<std::map<ttk::SimplexId, int>> forwardCombiMap(totalFeatures0);
    std::vector<std::map<ttk::SimplexId, int>> backwardCombiMap(totalFeatures1);

    // euclidean grid distance
    std::vector<std::map<ttk::SimplexId, int>> forwardDistMap(totalFeatures0);
    std::vector<std::map<ttk::SimplexId, int>> backwardDistMap(totalFeatures1);

    // Forward
    ttkTypeMacroT(
      triangulation->getType(),
      (status = this->computeMapCombinatorial<ttk::SimplexId>(
          forwardMap, (T0 *)triangulation->getData(),
          ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg1),
          ttkUtils::GetPointer<ttk::SimplexId>(vertexIds0), totalFeatures0, 0)));
    if(!status)
      return 0;

    // Backward
    ttkTypeMacroT(
      triangulation->getType(),
      (status = this->computeMapCombinatorial<ttk::SimplexId>(
          backwardMap, (T0 *)triangulation->getData(),
          ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg0),
          ttkUtils::GetPointer<ttk::SimplexId>(vertexIds1), totalFeatures1, 0)));
    if(!status)
      return 0;

    // Forward combi
    ttkTypeMacroT(
      triangulation->getType(),
      (status = this->computeMapCombinatorial<ttk::SimplexId>(
          forwardCombiMap, (T0 *)triangulation->getData(),
          ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg1),
          ttkUtils::GetPointer<ttk::SimplexId>(vertexIds0), totalFeatures0, this->NeighbourhoodSize)));
    if(!status)
      return 0;

    // Backward combi
    ttkTypeMacroT(
      triangulation->getType(),
      (status = this->computeMapCombinatorial<ttk::SimplexId>(
          backwardCombiMap, (T0 *)triangulation->getData(),
          ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg0),
          ttkUtils::GetPointer<ttk::SimplexId>(vertexIds1), totalFeatures1, this->NeighbourhoodSize)));
    if(!status)
      return 0;

    // Forward dist
    ttkTypeMacroT(
      triangulation->getType(),
      (status = this->computeMapDistance<ttk::SimplexId>(
          forwardDistMap, (T0 *)triangulation->getData(),
          ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg1),
          ttkUtils::GetPointer<ttk::SimplexId>(vertexIds0), totalFeatures0, this->NeighbourhoodSize, this->NeighbourhoodDistance)));
    if(!status)
      return 0;

    // Backward dist
    ttkTypeMacroT(
      triangulation->getType(),
      (status = this->computeMapDistance<ttk::SimplexId>(
          backwardDistMap, (T0 *)triangulation->getData(),
          ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg0),
          ttkUtils::GetPointer<ttk::SimplexId>(vertexIds1), totalFeatures1, this->NeighbourhoodSize, this->NeighbourhoodDistance)));
    if(!status)
      return 0;

    status = this->computeSimilarityMatrix<ttk::SimplexId>(
        ttkUtils::GetPointer<int>(forward),
        forwardMap,
        totalFeatures0, totalFeatures1, map1,
        [](ttk::SimplexId i, ttk::SimplexId j, ttk::SimplexId n,
            ttk::SimplexId) { return j * n + i; },
        "Computing Forward Similarity Matrix");
    if(!status)
      return 0;

    status = this->computeSimilarityMatrix<ttk::SimplexId>(
        ttkUtils::GetPointer<int>(backward),
        backwardMap,
        totalFeatures1, totalFeatures0, map0,
        [](ttk::SimplexId i, ttk::SimplexId j, ttk::SimplexId,
            ttk::SimplexId m) { return i * m + j; },
        "Computing Backward Similarity Matrix");
    if(!status)
      return 0;

    status = this->computeSimilarityMatrix<ttk::SimplexId>(
        ttkUtils::GetPointer<int>(forwardCombi),
        forwardCombiMap,
        totalFeatures0, totalFeatures1, map1,
        [](ttk::SimplexId i, ttk::SimplexId j, ttk::SimplexId n,
            ttk::SimplexId) { return j * n + i; },
        "Computing Forward Combi Similarity Matrix");
    if(!status)
      return 0;

    status = this->computeSimilarityMatrix<ttk::SimplexId>(
        ttkUtils::GetPointer<int>(backwardCombi),
        backwardCombiMap,
        totalFeatures1, totalFeatures0, map0,
        [](ttk::SimplexId i, ttk::SimplexId j, ttk::SimplexId,
            ttk::SimplexId m) { return i * m + j; },
        "Computing Backward Combi Similarity Matrix");
    if(!status)
      return 0;

    status = this->computeSimilarityMatrix<ttk::SimplexId>(
        ttkUtils::GetPointer<int>(forwardDist),
        forwardDistMap,
        totalFeatures0, totalFeatures1, map1,
        [](ttk::SimplexId i, ttk::SimplexId j, ttk::SimplexId n,
            ttk::SimplexId) { return j * n + i; },
        "Computing Distance Forward Similarity Matrix");
    if(!status)
      return 0;

    status = this->computeSimilarityMatrix<ttk::SimplexId>(
        ttkUtils::GetPointer<int>(backwardDist),
        backwardDistMap,
        totalFeatures1, totalFeatures0, map0,
        [](ttk::SimplexId i, ttk::SimplexId j, ttk::SimplexId,
            ttk::SimplexId m) { return i * m + j; },
        "Computing Distance Backward Similarity Matrix");
    if(!status)
      return 0;

    // MANIFOLDS //
    std::map<ttk::SimplexId, std::map<ttk::SimplexId, int>> forwardMapManifold;
    std::map<ttk::SimplexId, std::map<ttk::SimplexId, int>> backwardMapManifold;

    status = this->computeMapManifoldOverlap<ttk::SimplexId>(
        forwardMapManifold, ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg0), ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg1),
        totalVertices);
    if(!status)
      return 0;

    status = this->computeMapManifoldOverlap<ttk::SimplexId>(
        backwardMapManifold, ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg1), ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg0),
        totalVertices);
    if(!status)
      return 0;

    // MANIFOLD SIZES
    std::map<ttk::SimplexId, int> manifoldSize0;
    std::map<ttk::SimplexId, int> manifoldSize1;

    status = this->computeManifoldSize<ttk::SimplexId>(
        manifoldSize0, ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg0),
        totalVertices);

    status = this->computeManifoldSize<ttk::SimplexId>(
        manifoldSize1, ttkUtils::GetPointer<ttk::SimplexId>(manifoldSeg1),
        totalVertices);

    status = this->computeSimilarityManifoldMatrix<ttk::SimplexId>(
        ttkUtils::GetPointer<double>(forManifold),
        forwardMapManifold,
        manifoldSize0,
        manifoldSize1,
        ttkUtils::GetPointer<ttk::SimplexId>(vertexIds0),
        totalFeatures0, totalFeatures1, map1,
        [](ttk::SimplexId i, ttk::SimplexId j, ttk::SimplexId n,
            ttk::SimplexId) { return j * n + i; },
        "Computing Manifold Forward Similarity Matrix");
    if(!status)
      return 0;
    status = this->computeSimilarityManifoldMatrix<ttk::SimplexId>(
        ttkUtils::GetPointer<double>(backManifold),
        backwardMapManifold,
        manifoldSize1,
        manifoldSize0,
        ttkUtils::GetPointer<ttk::SimplexId>(vertexIds1),
        totalFeatures1, totalFeatures0, map0,
        [](ttk::SimplexId i, ttk::SimplexId j, ttk::SimplexId,
            ttk::SimplexId m) { return i * m + j; },
        "Computing Manifold Backward Similarity Matrix");
    if(!status)
      return 0;

    status = this->AddIndexIdMaps(similarityMatrix,
                                  this->GetInputArrayToProcess(1, seeds0),
                                  this->GetInputArrayToProcess(1, seeds1));
    if(!status)
      return 0;

    output->SetBlock(t-1,similarityMatrix);
  }

  return 1;
}
