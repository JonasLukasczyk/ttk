#include <ttkScalarFieldFromPoints.h>


#include <vtkInformation.h>
#include <vtkIntArray.h>
#include <vtkFloatArray.h>
#include <vtkDataSet.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkInformationVector.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkPointSet.h>
#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkDoubleArray.h>
#include <vtkPolyData.h>

#include <ttkMacros.h>
#include <ttkUtils.h>

vtkStandardNewMacro(ttkScalarFieldFromPoints);

ttkScalarFieldFromPoints::ttkScalarFieldFromPoints() {
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

ttkScalarFieldFromPoints::~ttkScalarFieldFromPoints() {
}

int ttkScalarFieldFromPoints::FillInputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
  else
    return 0;

  return 1;
}

int ttkScalarFieldFromPoints::FillOutputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 0);
  else
    return 0;

  return 1;
}

int ttkScalarFieldFromPoints::RequestInformation(vtkInformation *,
                                   vtkInformationVector **,
                                   vtkInformationVector * outputVector) {

  return 1;
}

int ttkScalarFieldFromPoints::RequestData(vtkInformation *request,
                               vtkInformationVector **inputVector,
                               vtkInformationVector *outputVector) {
  
  auto inputMB = vtkMultiBlockDataSet::GetData(inputVector[0]);
  if(!inputMB) {
    this->printErr("No input data available.");
    return 0;
  }

  for (unsigned int t = 0; t < inputMB->GetNumberOfBlocks(); t++) {
    // Get points for current timestep
    auto tBlock = inputMB->GetBlock(t);
    auto curPoints = vtkSmartPointer<vtkPolyData>::New();
    curPoints->ShallowCopy(tBlock);
    const size_t nPoints = curPoints->GetNumberOfPoints();

    // Get amplitude and spread for points in the timestep
    auto ampArray = GetInputArrayToProcess(2, curPoints);
    if(!ampArray) {
      this->printErr("No amplitude array was provided.");
      //return 0;
    }

    auto spreadArray = GetInputArrayToProcess(3, curPoints);
    if(!spreadArray) {
      this->printErr("No spread array was provided.");
      //return 0;
    }

    auto rateArray = GetInputArrayToProcess(4, curPoints);
    if(!rateArray) {
      this->printErr("No rate array was provided.");
      //return 0;
    }

    auto birthTimeArray = GetInputArrayToProcess(5, curPoints);
    if(!birthTimeArray) {
      this->printErr("No birth time array was provided.");
      //return 0;
    }

    auto deathTimeArray = GetInputArrayToProcess(6, curPoints);
    if(!deathTimeArray) {
      this->printErr("No death time array was provided.");
      //return 0;
    }

    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(
      this->Resolution[0],
      this->Resolution[1],
      this->Resolution[2]
    );
    image->SetOrigin(
      this->ImageBounds[0],
      this->ImageBounds[2],
      this->ImageBounds[4]
    );
    image->SetSpacing(
      this->Resolution[0]>1 ? (this->ImageBounds[1]-this->ImageBounds[0])/(this->Resolution[0]-1) : 0,
      this->Resolution[1]>1 ? (this->ImageBounds[3]-this->ImageBounds[2])/(this->Resolution[1]-1) : 0,
      this->Resolution[2]>1 ? (this->ImageBounds[5]-this->ImageBounds[4])/(this->Resolution[2]-1) : 0
    );
    image->AllocateScalars(VTK_DOUBLE, 1);

    auto scalarArray = image->GetPointData()->GetArray(0);
    scalarArray->SetName("Scalars");
    auto scalarArrayData = ttkUtils::GetPointer<double>(scalarArray);

    auto nPixels = scalarArray->GetNumberOfTuples();

    auto voronoiArray = vtkSmartPointer<vtkIntArray>::New();
    voronoiArray->SetName("Voronoi");
    voronoiArray->SetNumberOfComponents(1);
    voronoiArray->SetNumberOfTuples(nPixels);
    image->GetPointData()->AddArray(voronoiArray);
    auto voronoiArrayData = ttkUtils::GetPointer<int>(voronoiArray);

    auto weightedVoronoiArray = vtkSmartPointer<vtkIntArray>::New();
    weightedVoronoiArray->SetName("WeightedVoronoi");
    weightedVoronoiArray->SetNumberOfComponents(1);
    weightedVoronoiArray->SetNumberOfTuples(nPixels);
    image->GetPointData()->AddArray(weightedVoronoiArray);
    auto weightedVoronoiArrayData = ttkUtils::GetPointer<int>(weightedVoronoiArray);

    auto powerDiagramArray = vtkSmartPointer<vtkIntArray>::New();
    powerDiagramArray->SetName("PowerDiagram");
    powerDiagramArray->SetNumberOfComponents(1);
    powerDiagramArray->SetNumberOfTuples(nPixels);
    image->GetPointData()->AddArray(powerDiagramArray);
    auto powerDiagramArrayData = ttkUtils::GetPointer<int>(powerDiagramArray);


    // auto countArray = vtkSmartPointer<vtkIntArray>::New();
    // countArray->SetName("Count");
    // countArray->SetNumberOfComponents(1);
    // countArray->SetNumberOfTuples(nPixels);
    // image->GetPointData()->AddArray(countArray);
    // auto countArrayData = ttkUtils::GetPointer<int>(countArray);

    int status = 0;

    // status = this->computeCounts2D(
    //   countArrayData,
    //   ttkUtils::GetPointer<double>(curPoints->GetPoints()->GetData()),
    //   this->ImageBounds,
    //   this->Resolution,
    //   nPoints,
    //   nPixels
    // );
    // if(!status)
    //   return 0;

    ttk::Triangulation *triangulation = ttkAlgorithm::GetTriangulation(image);
    if(!triangulation)
      return 0;

    switch(this->Kernel){
      case 0: {
        // status = this->computeScalarField3D<ScalarFieldFromPoints::Gaussian>(
        //   scalarArrayData,
        //   ttkUtils::GetPointer<double>(curPoints->GetPoints()->GetData()),
        //   ttkUtils::GetPointer<double>(ampArray),
        //   ttkUtils::GetPointer<double>(spreadArray),
        //   this->Bandwidth,
        //   this->ImageBounds,
        //   this->Resolution,
        //   nPoints
        // );
        ttkVtkTemplateMacro(scalarArray->GetDataType(), triangulation->getType(),
          (status = this->computeScalarField<TTK_TT, ScalarFieldFromPoints::Gaussian>(
            scalarArrayData,
            voronoiArrayData,
            weightedVoronoiArrayData,
            powerDiagramArrayData,
            ttkUtils::GetPointer<double>(curPoints->GetPoints()->GetData()),
            ttkUtils::GetPointer<double>(ampArray),
            ttkUtils::GetPointer<double>(spreadArray),
            ttkUtils::GetPointer<double>(rateArray),
            ttkUtils::GetPointer<int>(birthTimeArray),
            ttkUtils::GetPointer<int>(deathTimeArray),
            nPoints,
            t,
            this->Bandwidth,
            (TTK_TT *)triangulation->getData()
            )
          )
        );
        break;
      }
      case 1: {
      //   status = this->computeScalarField3D<ScalarFieldFromPoints::Linear>(
      //     scalarArrayData,
      //     ttkUtils::GetPointer<double>(curPoints->GetPoints()->GetData()),
      //     ttkUtils::GetPointer<double>(ampArray),
      //     ttkUtils::GetPointer<double>(spreadArray),
      //     this->Bandwidth,
      //     this->ImageBounds,
      //     this->Resolution,
      //     nPoints
      //   );
        ttkVtkTemplateMacro(scalarArray->GetDataType(), triangulation->getType(),
          (status = this->computeScalarField<TTK_TT, ScalarFieldFromPoints::Linear>(
            scalarArrayData,
            voronoiArrayData,
            weightedVoronoiArrayData,
            powerDiagramArrayData,
            ttkUtils::GetPointer<double>(curPoints->GetPoints()->GetData()),
            ttkUtils::GetPointer<double>(ampArray),
            ttkUtils::GetPointer<double>(spreadArray),
            ttkUtils::GetPointer<double>(rateArray),
            ttkUtils::GetPointer<int>(birthTimeArray),
            ttkUtils::GetPointer<int>(deathTimeArray),           
            nPoints,
            t,
            this->Bandwidth,
            (TTK_TT *)triangulation->getData()
            )
          )
        );
        break;
      }
      case 2: {
        // status = this->computeScalarField3D<ScalarFieldFromPoints::Epanechnikov>(
        //   scalarArrayData,
        //   ttkUtils::GetPointer<double>(curPoints->GetPoints()->GetData()),
        //   ttkUtils::GetPointer<double>(ampArray),
        //   ttkUtils::GetPointer<double>(spreadArray),
        //   this->Bandwidth,
        //   this->ImageBounds,
        //   this->Resolution,
        //   nPoints
        // );
        ttkVtkTemplateMacro(scalarArray->GetDataType(), triangulation->getType(),
          (status = this->computeScalarField<TTK_TT, ScalarFieldFromPoints::Epanechnikov>(
            scalarArrayData,
            voronoiArrayData,
            weightedVoronoiArrayData,
            powerDiagramArrayData,
            ttkUtils::GetPointer<double>(curPoints->GetPoints()->GetData()),
            ttkUtils::GetPointer<double>(ampArray),
            ttkUtils::GetPointer<double>(spreadArray),
            ttkUtils::GetPointer<double>(rateArray),
            ttkUtils::GetPointer<int>(birthTimeArray),
            ttkUtils::GetPointer<int>(deathTimeArray),
            nPoints,
            t,
            this->Bandwidth,
            (TTK_TT *)triangulation->getData()
            )
          )
        );
        break;
      }
      case 3: {
        // status = this->computeScalarField3D<ScalarFieldFromPoints::Constant>(
        //   scalarArrayData,
        //   ttkUtils::GetPointer<double>(curPoints->GetPoints()->GetData()),
        //   ttkUtils::GetPointer<double>(ampArray),
        //   ttkUtils::GetPointer<double>(spreadArray),
        //   this->Bandwidth,
        //   this->ImageBounds,
        //   this->Resolution,
        //   nPoints
        // );
        ttkVtkTemplateMacro(scalarArray->GetDataType(), triangulation->getType(),
          (status = this->computeScalarField<TTK_TT, ScalarFieldFromPoints::Constant>(
            scalarArrayData,
            voronoiArrayData,
            weightedVoronoiArrayData,
            powerDiagramArrayData,
            ttkUtils::GetPointer<double>(curPoints->GetPoints()->GetData()),
            ttkUtils::GetPointer<double>(ampArray),
            ttkUtils::GetPointer<double>(spreadArray),
            ttkUtils::GetPointer<double>(rateArray),
            ttkUtils::GetPointer<int>(birthTimeArray),
            ttkUtils::GetPointer<int>(deathTimeArray),
            nPoints,
            t,
            this->Bandwidth,
            (TTK_TT *)triangulation->getData()
            )
          )
        );        
        break;
      }
    }

    // On error cancel filter execution
    if(status == 0)
      return 0;
    
    auto outputMB = vtkMultiBlockDataSet::GetData(outputVector);
    // Set image to a block in the output dataset
    size_t nBlocks = outputMB->GetNumberOfBlocks();
    outputMB->SetBlock(nBlocks, image);
  }

  return 1;
}
