#include <ttkLevelSets.h>

#include <vtkDataObject.h> // For port information
#include <vtkObjectFactory.h> // for new macro

#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

#include <vtkContourFilter.h>

#include <ttkUtils.h>

vtkStandardNewMacro(ttkLevelSets);

ttkLevelSets::ttkLevelSets() {
  this->setDebugMsgPrefix("LevelSets");

  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

ttkLevelSets::~ttkLevelSets() {
}

int ttkLevelSets::FillInputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  else
    return 0;

  return 1;
}

int ttkLevelSets::FillOutputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPolyData");
  else
    return 0;

  return 1;
}

int ttkLevelSets::RequestData(vtkInformation *request,
                               vtkInformationVector **inputVector,
                               vtkInformationVector *outputVector) {

  auto inputDataSet = vtkDataSet::GetData(inputVector[0]);
  auto inputArray = this->GetInputArrayToProcess(0, inputVector);

  std::string finalExpressionString;
  {
      std::string errorMsg;
      if( !ttkUtils::replaceVariables( this->Expression, inputDataSet->GetFieldData(), finalExpressionString, errorMsg ) ){
          this->printErr(errorMsg);
          return 0;
      }
  }
  std::vector<double> levels;
  ttkUtils::stringListToDoubleVector( finalExpressionString, levels );

  if(levels.size()<0){
    this->printErr("No level specified.");
    return 0;
  }

  ttk::Timer t;
  this->printMsg("Computing level sets ["+std::to_string(levels[0])+"]",0,0,ttk::debug::LineMode::REPLACE);

  auto cf = vtkSmartPointer<vtkContourFilter>::New();
  cf->SetInputData( inputDataSet );
  cf->SetInputArrayToProcess( 0, 0,0,0, inputArray->GetName() );
  cf->SetValue(0,levels[0]);
  cf->Update();

  this->printMsg("Computing level sets ["+std::to_string(levels[0])+"]",1,t.getElapsedTime());

  auto output = vtkDataSet::GetData(outputVector);
  output->ShallowCopy( cf->GetOutput() );

  return 1;
}
