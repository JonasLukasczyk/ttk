#include <ttkCorrespondenceByMTS.h>

#include <vtkObjectFactory.h>

#include <vtkInformation.h>

#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>

#include <vtkIntArray.h>
#include <vtkPointData.h>

#include <ttkUtils.h>

vtkStandardNewMacro(ttkCorrespondenceByMTS);

ttkCorrespondenceByMTS::ttkCorrespondenceByMTS() {
  this->SetNumberOfInputPorts(2);
  this->SetNumberOfOutputPorts(1);
}

ttkCorrespondenceByMTS::~ttkCorrespondenceByMTS() {
}

int ttkCorrespondenceByMTS::ComputeCorrespondences(
  vtkImageData *correspondenceMatrix,
  vtkDataObject *inputDataObjects0,
  vtkDataObject *inputDataObjects1) {

  // unpack inputs
  auto inputsAsMB0 = static_cast<vtkMultiBlockDataSet *>(inputDataObjects0);
  auto inputsAsMB1 = static_cast<vtkMultiBlockDataSet *>(inputDataObjects1);

  auto d0 = vtkDataSet::SafeDownCast(inputsAsMB0->GetBlock(0));
  auto d1 = vtkDataSet::SafeDownCast(inputsAsMB1->GetBlock(0));

  auto m0 = vtkDataSet::SafeDownCast(inputsAsMB0->GetBlock(1));
  auto m1 = vtkDataSet::SafeDownCast(inputsAsMB1->GetBlock(1));

  const int nEdges0 = m0->GetNumberOfPoints() - 1;
  const int nEdges1 = m1->GetNumberOfPoints() - 1;

  // extract arrays
  auto seg0 = vtkIntArray::SafeDownCast(d0->GetPointData()->GetArray("NodeId"));
  auto seg1 = vtkIntArray::SafeDownCast(d1->GetPointData()->GetArray("NodeId"));
  if(!seg0 || !seg1)
    return !this->printErr(
      "Unable to retrieve `NodeId` arrays from segmentations.");

  auto next0
    = vtkIntArray::SafeDownCast(m0->GetPointData()->GetArray("NextId"));
  auto next1
    = vtkIntArray::SafeDownCast(m1->GetPointData()->GetArray("NextId"));
  if(!next0 || !next1)
    return !this->printErr(
      "Unable to retrieve `NextId` arrays from merge trees.");

  auto scalars0 = this->GetInputArrayToProcess(0, m0);
  auto scalars1 = this->GetInputArrayToProcess(0, m1);
  if(!scalars0 || !scalars1)
    return !this->printErr("Unable to retrieve merge tree scalar arrays.");

  // initialize correspondence matrix
  correspondenceMatrix->SetDimensions(nEdges0, nEdges1, 1);
  correspondenceMatrix->AllocateScalars(VTK_INT, 1);
  auto matrixData = correspondenceMatrix->GetPointData()->GetArray(0);
  matrixData->SetName("Overlap");

  // compute overlap of segments
  int status = 0;
  switch(scalars0->GetDataType()) {
    vtkTemplateMacro(
      (status = this->computeSegmentationOverlap<int, VTK_TT>(
         ttkUtils::GetPointer<int>(matrixData),

         ttkUtils::GetPointer<const int>(seg0),
         ttkUtils::GetPointer<const int>(seg1), seg0->GetNumberOfTuples(),
         ttkUtils::GetPointer<const int>(next0),
         ttkUtils::GetPointer<const int>(next1),
         ttkUtils::GetPointer<const VTK_TT>(scalars0),
         ttkUtils::GetPointer<const VTK_TT>(scalars1), nEdges0, nEdges1)));
  }
  if(!status)
    return 0;

  // add index label maps
  int a = 0;
  auto fd = correspondenceMatrix->GetFieldData();
  for(auto &it : std::vector<vtkDataSet *>({m0, m1})) {
    auto labels = this->GetInputArrayToProcess(1, it);
    if(!labels)
      return !this->printErr("Unable to retrieve labels.");
    auto array = vtkSmartPointer<vtkDataArray>::Take(labels->NewInstance());
    array->ShallowCopy(labels);
    array->SetName(("IndexLabelMap" + std::to_string(a++)).data());
    fd->AddArray(array);
  }

  return 1;
}