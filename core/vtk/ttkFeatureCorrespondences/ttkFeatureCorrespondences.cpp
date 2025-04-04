#include <ttkFeatureCorrespondences.h>

#include <vtkInformation.h>

#include <vtkDataArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkMultiBlockDataSet.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkFeatureCorrespondences);

ttkFeatureCorrespondences::ttkFeatureCorrespondences() {
  this->SetNumberOfInputPorts(2);
  this->SetNumberOfOutputPorts(1);
}

ttkFeatureCorrespondences::~ttkFeatureCorrespondences() {
}

int ttkFeatureCorrespondences::FillInputPortInformation(int port,
                                                        vtkInformation *info) {
  if(port == 0) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
    return 1;
  } else if(port == 1) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
    return 1;
  }
  return 0;
}

int ttkFeatureCorrespondences::FillOutputPortInformation(int port,
                                                         vtkInformation *info) {
  if(port == 0) {
    info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 0);
    return 1;
  }
  return 0;
}

std::string getIdArrayName(vtkFieldData *fieldData) {
  std::string result;

  int found = 0;
  for(int a = 0; a < fieldData->GetNumberOfArrays(); a++) {
    auto array = fieldData->GetAbstractArray(a);
    const auto name = std::string(array->GetName());
    const int n = name.size();

    if(name.substr(std::max(0,n - 4)).compare("_t-1") == 0) {
      result = name.substr(0, n - 4);
      found++;
    } else if(name.substr(std::max(0,n - 2)).compare("_t") == 0) {
      found++;
    }
  }

  return found == 2 ? result : "";
}

int ttkFeatureCorrespondences::RequestData(vtkInformation *ttkNotUsed(request),
                                           vtkInformationVector **inputVector,
                                           vtkInformationVector *outputVector) {

  auto iMatrices = vtkMultiBlockDataSet::GetData(inputVector[0]);
  if(!iMatrices)
    return !this->printErr("Unable to retrieve input data objects.");

  auto oMatrices = vtkMultiBlockDataSet::GetData(outputVector);
  const size_t n = iMatrices->GetNumberOfBlocks();

  for(size_t t=0; t<n; t++){
    auto iMatrix_ = vtkImageData::SafeDownCast(iMatrices->GetBlock(t));

    int dim[3];
    iMatrix_->GetDimensions(dim);

    auto iMatrix = this->GetInputArrayToProcess(0, iMatrix_);
    if(!iMatrix)
      return !this->printErr("Unable to retrieve input matrix.");

    // auto oMatrix = vtkSmartPointer<vtkDataArray>::Take(iMatrix->NewInstance());
    // oMatrix->DeepCopy(iMatrix);
    auto oMatrix = vtkSmartPointer<vtkFloatArray>::New();
    oMatrix->SetName(iMatrix->GetName());
    oMatrix->SetNumberOfTuples(iMatrix->GetNumberOfTuples());

    int status = 0;

    // compute output matrix
    switch(this->OptimizationMethod) {
      case OPTIMIZATION_METHOD::N_SMALLEST_CORRESPONDENCES_PER_FEATURE:
      case OPTIMIZATION_METHOD::N_LARGEST_CORRESPONDENCES_PER_FEATURE: {
        ttkTypeMacroA(
          iMatrix->GetDataType(),
          (status = this->sortAndReduceCorrespondencesPerFeature<T0>(
             ttkUtils::GetPointer<T0>(oMatrix), ttkUtils::GetPointer<T0>(iMatrix),
             dim[0], dim[1], this->NumberOfLargestCorrespondencesPerFeature,
             this->OptimizationMethod
               == OPTIMIZATION_METHOD::N_SMALLEST_CORRESPONDENCES_PER_FEATURE)));
        break;
      }
      case OPTIMIZATION_METHOD::THRESHOLD_ABOVE: {
        ttkTypeMacroA(
          iMatrix->GetDataType(),
          (status = this->mapEachElement<T0>(
             ttkUtils::GetPointer<T0>(oMatrix), ttkUtils::GetPointer<T0>(iMatrix),
             dim[0], dim[1],
             [=](const T0 &v) { return v >= this->Threshold ? v : 0; })));
        break;
      }

      case OPTIMIZATION_METHOD::THRESHOLD_BELOW: {
        ttkTypeMacroA(
          iMatrix->GetDataType(),
          (status = this->mapEachElement<T0>(
             ttkUtils::GetPointer<T0>(oMatrix), ttkUtils::GetPointer<T0>(iMatrix),
             dim[0], dim[1],
             [=](const T0 &v) { return v <= this->Threshold ? v : 0; })));
        break;
      }
      case OPTIMIZATION_METHOD::TWO_PASS: {

        auto iFeatures = vtkMultiBlockDataSet::GetData(inputVector[1]);

        auto features0 = vtkDataSet::SafeDownCast(iFeatures->GetBlock(t));
        auto features1 = vtkDataSet::SafeDownCast(iFeatures->GetBlock(t+1));

        auto idArrayName = getIdArrayName(iMatrix_->GetFieldData());

        ttkTypeMacroA(
          iMatrix->GetDataType(),
          (status = this->twoPassOptimization<T0>(
            ttkUtils::GetPointer<float>(oMatrix),
            ttkUtils::GetPointer<T0>(iMatrix),
            ttkUtils::GetPointer<int>(iMatrix_->GetFieldData()->GetArray((idArrayName+"_t-1").data())),
            ttkUtils::GetPointer<int>(iMatrix_->GetFieldData()->GetArray((idArrayName+"_t").data())),
            dim[0],
            dim[1],

            ttkUtils::GetPointer<float>(features0->GetPointData()->GetArray("Size")),
            ttkUtils::GetPointer<float>(features1->GetPointData()->GetArray("Size")),
            ttkUtils::GetPointer<int>(features0->GetPointData()->GetArray(idArrayName.data())),
            ttkUtils::GetPointer<int>(features1->GetPointData()->GetArray(idArrayName.data())),
            features0->GetNumberOfPoints(),
            features1->GetNumberOfPoints()
        )));
        break;
      }
      default: {
        return !this->printErr("Unsupported Optimization Method");
      }
    }
    if(!status)
      return 0;

    vtkNew<vtkImageData> oMatrix_;
    oMatrix_->ShallowCopy(iMatrix_);
    oMatrix_->GetPointData()->AddArray(oMatrix);
    oMatrices->SetBlock(t,oMatrix_);
  }

  return 1;
}
