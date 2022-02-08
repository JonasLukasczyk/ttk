#include <ttkScalarFieldFromPointsNew.h>

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

vtkStandardNewMacro(ttkScalarFieldFromPointsNew);

ttkScalarFieldFromPointsNew::ttkScalarFieldFromPointsNew() {
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

ttkScalarFieldFromPointsNew::~ttkScalarFieldFromPointsNew() {
}

int ttkScalarFieldFromPointsNew::FillInputPortInformation(
  int port, vtkInformation *info) {
  if(port == 0)
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  else
    return 0;

  return 1;
}

int ttkScalarFieldFromPointsNew::FillOutputPortInformation(
  int port, vtkInformation *info) {
  if(port == 0)
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkImageData");
  else
    return 0;

  return 1;
}

int ttkScalarFieldFromPointsNew::RequestInformation(
  vtkInformation *,
  vtkInformationVector **,
  vtkInformationVector *outputVector) {

  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  if(this->CellSpacing[0] > 0 && this->CellSpacing[1] > 0
     && this->CellSpacing[2] > 0) {
    // Check what dimension the domain is in
    int dimXmin, dimXmax, dimYmin, dimYmax, dimZmin, dimZmax;
    if(this->ImageBounds[4] == this->ImageBounds[5]) {
      dimZmin = dimZmax = 0;
    } else {
      dimZmin = floor(this->ImageBounds[4] * 1 / this->CellSpacing[2]);
      dimZmax = ceil(this->ImageBounds[5] * 1 / this->CellSpacing[2]);
    }

    // Define and set all required attributes of the vtkImageData output
    // The extent is the number of data points in each dimension and is
    // therefore scaled by the cell spacing
    dimXmin = floor(this->ImageBounds[0] * 1 / this->CellSpacing[0]);
    dimXmax = ceil(this->ImageBounds[1] * 1 / this->CellSpacing[0]);
    dimYmin = floor(this->ImageBounds[2] * 1 / this->CellSpacing[1]);
    dimYmax = ceil(this->ImageBounds[3] * 1 / this->CellSpacing[1]);
    int extent[6] = {dimXmin, dimXmax, dimYmin, dimYmax, dimZmin, dimZmax};
    double spacing[3]
      = {this->CellSpacing[0], this->CellSpacing[1], this->CellSpacing[2]};
    double origin[3] = {0, 0, 0};

    outInfo->Set(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), extent, 6);
    outInfo->Set(vtkDataObject::SPACING(), spacing, 3);
    outInfo->Set(vtkDataObject::ORIGIN(), origin, 3);
    vtkDataObject::SetPointDataActiveScalarInfo(outInfo, VTK_DOUBLE, 1);
  } else {
    this->printErr("The Cell Spacing contains zeros.");
    return 0;
  }

  return 1;
}

int ttkScalarFieldFromPointsNew::RequestData(
  vtkInformation *request,
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

  // Format output
  auto output = vtkImageData::GetData(outputVector);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  if(this->CellSpacing[0] > 0 && this->CellSpacing[1] > 0
     && this->CellSpacing[2] > 0) {
    // do nothing
  } else {
    this->printErr("The Cell Spacing contains zeros.");
    return 0;
  }

  // Check what dimension the output domain should be in
  int dimXmin, dimXmax, dimYmin, dimYmax, dimZmin, dimZmax;
  bool dim2D = false;
  if(this->ImageBounds[4] == this->ImageBounds[5]) {
    dimZmin = dimZmax = 0;
    dim2D = true;
  } else {
    dimZmin = floor(this->ImageBounds[4] * 1 / this->CellSpacing[2]);
    dimZmax = ceil(this->ImageBounds[5] * 1 / this->CellSpacing[2]);
  }

  // Set the necessary data to format the vtkImageData output
  // The extent is the number of data points in each dimension and is therefore
  // scaled by the cell spacing
  dimXmin = floor(this->ImageBounds[0] * 1 / this->CellSpacing[0]);
  dimXmax = ceil(this->ImageBounds[1] * 1 / this->CellSpacing[0]);
  dimYmin = floor(this->ImageBounds[2] * 1 / this->CellSpacing[1]);
  dimYmax = ceil(this->ImageBounds[3] * 1 / this->CellSpacing[1]);
  int extent[6] = {dimXmin, dimXmax, dimYmin, dimYmax, dimZmin, dimZmax};
  output->SetOrigin(0, 0, 0);
  output->SetExtent(extent);
  outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), extent);
  output->SetSpacing(
    this->CellSpacing[0], this->CellSpacing[1], this->CellSpacing[2]);
  output->AllocateScalars(VTK_DOUBLE, 1);

  int dims[3] = {0, 0, 0};
  output->GetDimensions(dims);

  // Get data array to put the filter results in
  auto scalarArray = output->GetPointData()->GetArray(0);
  scalarArray->SetName("Scalars");
  auto scalarArrayData = ttkUtils::GetPointer<double>(scalarArray);

  auto nPixels = scalarArray->GetNumberOfTuples();

  // Used to check base layer execution status
  int status = 0;

  // Execute either 2D or 3D case for the chosen kernel
  switch(this->Kernel) {
    case 0: {
      if(dim2D) {
        status = this->computeScalarField2D<ScalarFieldFromPointsNew::Gaussian>(
          scalarArrayData,
          ttkUtils::GetPointer<double>(input->GetPoints()->GetData()),
          ttkUtils::GetPointer<double>(pwArray),
          ttkUtils::GetPointer<double>(pcArray), this->ImageBounds,
          this->CellSpacing, dims, nPoints, nPixels);
      } else {
        status = this->computeScalarField3D<ScalarFieldFromPointsNew::Gaussian>(
          scalarArrayData,
          ttkUtils::GetPointer<double>(input->GetPoints()->GetData()),
          ttkUtils::GetPointer<double>(pwArray),
          ttkUtils::GetPointer<double>(pcArray), this->ImageBounds,
          this->CellSpacing, dims, nPoints, nPixels);
      }

      break;
    }
    case 1: {
      if(dim2D) {
        status = this->computeScalarField2D<ScalarFieldFromPointsNew::Linear>(
          scalarArrayData,
          ttkUtils::GetPointer<double>(input->GetPoints()->GetData()),
          ttkUtils::GetPointer<double>(pwArray),
          ttkUtils::GetPointer<double>(pcArray), this->ImageBounds,
          this->CellSpacing, dims, nPoints, nPixels);
      } else {
        status = this->computeScalarField3D<ScalarFieldFromPointsNew::Linear>(
          scalarArrayData,
          ttkUtils::GetPointer<double>(input->GetPoints()->GetData()),
          ttkUtils::GetPointer<double>(pwArray),
          ttkUtils::GetPointer<double>(pcArray), this->ImageBounds,
          this->CellSpacing, dims, nPoints, nPixels);
      }
      break;
    }
    case 2: {
      if(dim2D) {
        status = this->computeScalarField2D<ScalarFieldFromPointsNew::Constant>(
          scalarArrayData,
          ttkUtils::GetPointer<double>(input->GetPoints()->GetData()),
          ttkUtils::GetPointer<double>(pwArray),
          ttkUtils::GetPointer<double>(pcArray), this->ImageBounds,
          this->CellSpacing, dims, nPoints, nPixels);
      } else {
        status = this->computeScalarField3D<ScalarFieldFromPointsNew::Constant>(
          scalarArrayData,
          ttkUtils::GetPointer<double>(input->GetPoints()->GetData()),
          ttkUtils::GetPointer<double>(pwArray),
          ttkUtils::GetPointer<double>(pcArray), this->ImageBounds,
          this->CellSpacing, dims, nPoints, nPixels);
      }
      break;
    }
  }

  // On error cancel filter execution
  if(status == 0)
    return 0;

  return 1;
}
