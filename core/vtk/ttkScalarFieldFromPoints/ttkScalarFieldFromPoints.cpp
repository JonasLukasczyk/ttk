#include <ttkScalarFieldFromPoints.h>

#include <vtkDataSet.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkIntArray.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStreamingDemandDrivenPipeline.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkScalarFieldFromPoints);

ttkScalarFieldFromPoints::ttkScalarFieldFromPoints() {
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

ttkScalarFieldFromPoints::~ttkScalarFieldFromPoints() {
}

int ttkScalarFieldFromPoints::FillInputPortInformation(int port,
                                                       vtkInformation *info) {
  if(port == 0)
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  else
    return 0;

  return 1;
}

int ttkScalarFieldFromPoints::FillOutputPortInformation(int port,
                                                        vtkInformation *info) {
  if(port == 0)
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkImageData");
  else
    return 0;

  return 1;
}

int ttkScalarFieldFromPoints::RequestInformation(
  vtkInformation *,
  vtkInformationVector **,
  vtkInformationVector *outputVector) {

  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  // Extent
  int wholeExtent[6]
    = {0, (int)this->Resolution[0] - 1, 0, (int)this->Resolution[1] - 1,
       0, (int)this->Resolution[2] - 1};
  outInfo->Set(
    vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), wholeExtent, 6);

  return 1;
}

int ttkScalarFieldFromPoints::RequestData(vtkInformation *,
                                          vtkInformationVector **inputVector,
                                          vtkInformationVector *outputVector) {
  // Get input
  auto input = vtkPointSet::GetData(inputVector[0], 0);
  if(!input) {
    this->printErr("There is no input provided.");
    return 0;
  }

  const size_t nPoints = input->GetNumberOfPoints();

  // Get point attributes arrays from input
  auto pwArray = GetInputArrayToProcess(2, input);
  if(!pwArray) {
    this->printErr("No point weight array was provided.");
    return 0;
  }

  auto pcArray = GetInputArrayToProcess(3, input);
  if(!pcArray) {
    this->printErr("No point constant array was provided.");
    return 0;
  }

  auto output = vtkImageData::GetData(outputVector);
  output->SetDimensions(
    this->Resolution[0], this->Resolution[1], this->Resolution[2]);
  output->SetOrigin(
    this->ImageBounds[0], this->ImageBounds[2], this->ImageBounds[4]);

  const double spacing[3]{
    this->Resolution[0] > 1 ? (this->ImageBounds[1] - this->ImageBounds[0])
                                / (this->Resolution[0] - 1)
                            : 0,
    this->Resolution[1] > 1 ? (this->ImageBounds[3] - this->ImageBounds[2])
                                / (this->Resolution[1] - 1)
                            : 0,
    this->Resolution[2] > 1 ? (this->ImageBounds[5] - this->ImageBounds[4])
                                / (this->Resolution[2] - 1)
                            : 0};
  output->SetSpacing(spacing);

  output->AllocateScalars(VTK_DOUBLE, 1);

  if(this->Resolution[0] < 1 || this->Resolution[1] < 1
     || this->Resolution[2] < 1)
    return !this->printErr("Resolution contains zeros.");

  // Check what dimension the output domain should be in

  bool dim2D = this->Resolution[2] == 1;

  // Get data array to put the filter results in
  auto scalarArray = output->GetPointData()->GetArray(0);
  scalarArray->SetName("Scalars");
  auto scalarArrayData = ttkUtils::GetPointer<double>(scalarArray);

  auto nPixels = scalarArray->GetNumberOfTuples();

  // Create array to store feature ids in
  auto maxIdArray = vtkSmartPointer<vtkIntArray>::New();
  maxIdArray->SetName("MaxId");
  maxIdArray->SetNumberOfTuples(nPixels);
  maxIdArray->SetNumberOfComponents(1);

  // Used to check base layer execution status
  int status = 0;

  // Execute either 2D or 3D case for the chosen kernel
  switch(this->Kernel) {
    case 0: {
      if(dim2D) {
        status = this->computeScalarField2D<ScalarFieldFromPoints::Gaussian>(
          scalarArrayData, ttkUtils::GetPointer<int>(maxIdArray),
          ttkUtils::GetPointer<double>(input->GetPoints()->GetData()),
          ttkUtils::GetPointer<double>(pwArray),
          ttkUtils::GetPointer<double>(pcArray), this->ImageBounds, spacing,
          this->Resolution, nPoints, nPixels);
      } else {
        status = this->computeScalarField3D<ScalarFieldFromPoints::Gaussian>(
          scalarArrayData, ttkUtils::GetPointer<int>(maxIdArray),
          ttkUtils::GetPointer<double>(input->GetPoints()->GetData()),
          ttkUtils::GetPointer<double>(pwArray),
          ttkUtils::GetPointer<double>(pcArray), this->ImageBounds, spacing,
          this->Resolution, nPoints, nPixels);
      }

      break;
    }
    case 1: {
      if(dim2D) {
        status = this->computeScalarField2D<ScalarFieldFromPoints::Linear>(
          scalarArrayData, ttkUtils::GetPointer<int>(maxIdArray),
          ttkUtils::GetPointer<double>(input->GetPoints()->GetData()),
          ttkUtils::GetPointer<double>(pwArray),
          ttkUtils::GetPointer<double>(pcArray), this->ImageBounds, spacing,
          this->Resolution, nPoints, nPixels);
      } else {
        status = this->computeScalarField3D<ScalarFieldFromPoints::Linear>(
          scalarArrayData, ttkUtils::GetPointer<int>(maxIdArray),
          ttkUtils::GetPointer<double>(input->GetPoints()->GetData()),
          ttkUtils::GetPointer<double>(pwArray),
          ttkUtils::GetPointer<double>(pcArray), this->ImageBounds, spacing,
          this->Resolution, nPoints, nPixels);
      }
      break;
    }
  }

  // On error cancel filter execution
  if(status == 0)
    return 0;

  output->GetPointData()->AddArray(maxIdArray);

  return 1;
}
