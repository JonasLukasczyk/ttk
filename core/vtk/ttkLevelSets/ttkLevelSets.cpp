#include <ttkLevelSets.h>

#include <vtkDataObject.h> // For port information
#include <vtkObjectFactory.h> // for new macro

#include <vtkInformation.h>

#include <vtkDataArray.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

#include <vtkContourFilter.h>
#include <vtkDataSetTriangleFilter.h>
#include <vtkTableBasedClipDataSet.h>
#include <vtkDataSetSurfaceFilter.h>

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
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkUnstructuredGrid");
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

  auto output = vtkUnstructuredGrid::GetData(outputVector);

  if(levels.size()<1){
    this->printErr("No level specified.");
    return 0;
  }

  if(this->LevelSetType==0){ // Level Set
      ttk::Timer t;
      this->printMsg("Computing level sets ["+std::to_string(levels[0])+"]",0,0,ttk::debug::LineMode::REPLACE);
      auto l = vtkSmartPointer<vtkContourFilter>::New();
      l->SetInputData( inputDataSet );
      l->SetGenerateTriangles( true );
      l->SetInputArrayToProcess( 0, 0,0,0, inputArray->GetName() );
      l->SetValue(0,levels[0]);
      l->Update();
      this->printMsg("Computing level sets ["+std::to_string(levels[0])+"]",1,t.getElapsedTime());

      t.reStart();
      this->printMsg("Triangulating output",0,0,ttk::debug::LineMode::REPLACE);
      auto triangleFilter = vtkSmartPointer<vtkDataSetTriangleFilter>::New();
      triangleFilter->SetInputData( l->GetOutput() );
      triangleFilter->Update();
      this->printMsg("Triangulating output",1,t.getElapsedTime());

      output->ShallowCopy( triangleFilter->GetOutput() );
  } else if(this->LevelSetType==2) { // Superlevel set
      ttk::Timer t;
      this->printMsg("Computing superlevel sets ["+std::to_string(levels[0])+"]",0,0,ttk::debug::LineMode::REPLACE);
      auto l = vtkSmartPointer<vtkTableBasedClipDataSet>::New();
      l->SetInputData( inputDataSet );
      l->SetValue(levels[0]);
      l->SetMergeTolerance(0.5);
      l->SetInputArrayToProcess( 0, 0,0,0, inputArray->GetName() );
      l->Update();
      this->printMsg("Computing superlevel sets ["+std::to_string(levels[0])+"]",1,t.getElapsedTime());

      t.reStart();
      this->printMsg("Computing surface",0,0,ttk::debug::LineMode::REPLACE);
      auto e = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
      e->SetInputData( l->GetOutput() );
      e->Update();
      this->printMsg("Computing surface",1,t.getElapsedTime());

      t.reStart();
      this->printMsg("Triangulating output",0,0,ttk::debug::LineMode::REPLACE);
      auto triangleFilter = vtkSmartPointer<vtkDataSetTriangleFilter>::New();
      triangleFilter->SetInputData( e->GetOutput() );
      triangleFilter->Update();
      this->printMsg("Triangulating output",1,t.getElapsedTime());

      output->ShallowCopy( triangleFilter->GetOutput() );
  }

  return 1;
}
