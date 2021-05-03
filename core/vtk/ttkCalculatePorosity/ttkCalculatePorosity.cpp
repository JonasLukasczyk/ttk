#include <ttkCalculatePorosity.h>

#include <vtkInformation.h>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkFloatArray.h>
#include <ttkMacros.h>
#include <ttkUtils.h>


// A VTK macro that enables the instantiation of this class via ::New()
// You do not have to modify this
vtkStandardNewMacro(ttkCalculatePorosity);

/**
* TODO 7: Implement the filter constructor and destructor in the cpp file.
*
* The constructor has to specify the number of input and output ports
* with the functions SetNumberOfInputPorts and SetNumberOfOutputPorts,
* respectively. It should also set default values for all filter
* parameters.
*
* The destructor is usually empty unless you want to manage memory
* explicitly, by for example allocating memory on the heap that needs
* to be freed when the filter is destroyed.
*/
ttkCalculatePorosity::ttkCalculatePorosity() {
  this->SetNumberOfInputPorts(2);
  this->SetNumberOfOutputPorts(1);
}

ttkCalculatePorosity::~ttkCalculatePorosity() {}

/**
* TODO 8: Specify the required input data type of each input port
*
* This method specifies the required input object data types of the
* filter by adding the vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE() key to
* the port information.
*/
int ttkCalculatePorosity::FillInputPortInformation(int port, vtkInformation *info) {
  if(port == 0 || port == 1) {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    return 1;
  }
  return 0;
}

/**
* TODO 9: Specify the data object type of each output port
*
* This method specifies in the port information object the data type of the
* corresponding output objects. It is possible to either explicitly
* specify a type by adding a vtkDataObject::DATA_TYPE_NAME() key:
*
*      info->Set( vtkDataObject::DATA_TYPE_NAME(), "vtkUnstructuredGrid" );
*
* or to pass a type of an input port to an output port by adding the
* ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT() key (see below).
*
* Note: prior to the execution of the RequestData method the pipeline will
* initialize empty output data objects based on this information.
*/
int ttkCalculatePorosity::FillOutputPortInformation(int port, vtkInformation *info) {
  if(port == 0) {
    info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 0);
    return 1;
  }
  return 0;
}

/**
* TODO 10: Pass VTK data to the base code and convert base code output to VTK
*
* This method is called during the pipeline execution to update the
* already initialized output data objects based on the given input
* data objects and filter parameters.
*
* Note:
*     1) The passed input data objects are validated based on the information
*        provided by the FillInputPortInformation method.
*     2) The output objects are already initialized based on the information
*        provided by the FillOutputPortInformation method.
*/
int ttkCalculatePorosity::RequestData(vtkInformation *request,
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector) {

  // Get input object from input vector
  // Note: has to be a vtkDataSet as required by FillInputPortInformation
  vtkImageData* inputDataSet = vtkImageData::GetData(inputVector[0]);

  if(!inputDataSet)
  return 0;


  // Get input array that will be processed
  //
  // Note: VTK provides abstract functionality to handle array selections, but
  //       this essential functionality is unfortunately not well documented.
  //       Before you read further, please keep in mind the the TTK developer
  //       team is not responsible for the existing VTK Api ;-)
  //
  //       In a nutshell, prior to the RequestData execution one has to call
  //
  //           SetInputArrayToProcess (
  //               int idx,
  //               int port,
  //               int connection,
  //               int fieldAssociation,
  //               const char *name
  //            )
  //
  //       The parameter 'idx' is often missunderstood: lets say the filter
  //       requires n arrays, then idx enumerates them from 0 to n-1.
  //
  //       The 'port' is the input port index at which the object is connected
  //       from which we want to get the array.
  //
  //       The 'connection' is the connection index at that port (we have to
  //       specify this because VTK allows multiple connections at the same
  //       input port).
  //
  //       The 'fieldAssociation' integer specifies if the array should be taken
  //       from 0: point data, 1: cell data, or 2: field data.
  //
  //       The final parameter is the 'name' of the array.
  //
  //       Example: SetInputArrayToProcess(3,1,0,1,"EdgeLength") will store that
  //                for the 3rd array the filter needs the cell data array named
  //                "EdgeLength" that it will retrieve from the vtkDataObject
  //                at input port 1 (first connection). During the RequestData
  //                method one can then actually retrieve the 3rd array it
  //                requires for its computation by calling
  //                GetInputArrayToProcess(3, inputVector)
  //
  //       If this filter is run within ParaView, then the UI will automatically
  //       call SetInputArrayToProcess (see CalculatePorosity.xml file).
  //
  //       During the RequestData execution one can then retrieve an actual
  //       array with the method "GetInputArrayToProcess".
  vtkDataArray *inputArray = this->GetInputArrayToProcess(0, inputVector);
  vtkDataArray *gradientInput = this->GetInputArrayToProcess(1, inputVector);
  vtkDataArray *divergence = this->GetInputArrayToProcess(2, inputVector);

  if(!inputArray || !gradientInput || !divergence) {
    this->printErr("Unable to retrieve input array.");
    return 0;
  }

  // To make sure that the selected array can be processed by this filter,
  // one should also check that the array association and format is correct.
  if(this->GetInputArrayAssociation(0, inputVector) != 0) {
    this->printErr("Input array needs to be a point data array.");
    return 0;
  }
  if(inputArray->GetNumberOfComponents() != 1) {
    this->printErr("Input array needs to be a scalar array.");
    return 0;
  }

  // If all checks pass then log which array is going to be processed.
  this->printMsg("Starting computation...");
  this->printMsg("  Scalar Array: " + std::string(inputArray->GetName()));
  this->printMsg("  Gradient Array: " + std::string(gradientInput->GetName()));
  // Create an output array that has the same data type as the input array
  // Note: vtkSmartPointers are well documented
  //       (https://vtk.org/Wiki/VTK/Tutorials/SmartPointers)
  auto outputArray = vtkSmartPointer < vtkFloatArray > ::New();
  outputArray->SetName("InversePyramidProbability"); // set array name
  outputArray->SetNumberOfComponents(1); // only one component per tuple
  outputArray->SetNumberOfTuples(inputArray->GetNumberOfTuples());

  int dim[3];
  inputDataSet->GetDimensions(dim);

  // ttk::Triangulation *triangulation
  //   = ttkAlgorithm::GetTriangulation(inputDataSet);
  // if(!triangulation)
  //   return 0;
  // // Precondition the triangulation (e.g., enable fetching of vertex neighbors)
  // this->preconditionTriangulation(triangulation); // implemented in base class

  // Templatize over the different input array data types and call the base code
  int status = 0; // this integer checks if the base code returns an error
  switch(inputArray->GetDataType()) {
    vtkTemplateMacro(
      status = this->computePorosity<VTK_TT>(
        ttkUtils::GetPointer<float>(outputArray),
        inputDataSet->GetNumberOfPoints(),
        ttkUtils::GetPointer<VTK_TT>(inputArray),
        ttkUtils::GetPointer<float>(gradientInput),
        ttkUtils::GetPointer<float>(divergence),
        this->Distance, this->Threshold, this->Margin, this->MaxThreshold, this->GradientThreshold, dim)
    );
  }
  // On error cancel filter execution
  if(status != 1)
  return 0;

  // Get output vtkDataSet (which was already instantiated based on the
  // information provided by FillOutputPortInformation)
  vtkDataSet *outputDataSet = vtkDataSet::GetData(outputVector, 0);

  // make a SHALLOW copy of the input
  outputDataSet->ShallowCopy(inputDataSet);

  // add to the output point data the computed output array
  outputDataSet->GetPointData()->AddArray(outputArray);

  // return success
  return 1;
}