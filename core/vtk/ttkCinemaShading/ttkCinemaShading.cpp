#include <ttkCinemaShading.h>

#include <vtkInformationVector.h>

#include <vtkDataObject.h>
#include <vtkImageData.h>
#include <vtkMultiBlockDataSet.h>

#include <vtkPlaneSource.h>
#include <vtkTexture.h>

#include <vtkCommand.h>
#include <vtkShaderProgram.h>
#include <vtkCamera.h>

#include <vtkPointData.h>

#include <vtkUnsignedCharArray.h>

#include <vtkRenderStepsPass.h>
#include <vtkFramebufferPass.h>
#include <vtkOpenGLRenderer.h>
#include <vtkTextureObject.h>
#include <vtkOpenGLTexture.h>

#include <vtkOpenGLPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkShaderProperty.h>

#include <vtkStreamingDemandDrivenPipeline.h>

#include <ttkUtils.h>

vtkStandardNewMacro(ttkCinemaShading);

ttkCinemaShading::ttkCinemaShading(){
    this->ColorTransferFunction = vtkSmartPointer<vtkColorTransferFunction>::New();
    this->SetColorTransferFunctionString("");

    this->InitFullScreenQuad();
    this->InitRenderer();

    this->SetNumberOfInputPorts(4);
    this->SetNumberOfOutputPorts(1);
}

ttkCinemaShading::~ttkCinemaShading(){
}


// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// VTK
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
int ttkCinemaShading::FillInputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  else if(port > 0 && port < 4){
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  } else
    return 0;

  return 1;
}

int ttkCinemaShading::FillOutputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 0);
  else
    return 0;

  return 1;
}


// =============================================================================
// RequestInformation
// =============================================================================
int ttkCinemaShading::RequestInformation(
    vtkInformation* info,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector
){
    if( this->GetOutputMode()==1 ){
        vtkInformation* outInfo = outputVector->GetInformationObject(0);

        // Extent
        auto resolution = this->GetResolution();
        int wholeExtent[6] = {
            0, resolution[0]-1,
            0, resolution[1]-1,
            0,0
        };
        outInfo->Set(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), wholeExtent, 6);

        return 1;
    }

    return ttkAlgorithm::RequestInformation( info, inputVector, outputVector );
}

// // =============================================================================
// // RequestUpdateExtent
// // =============================================================================
// int ttkCinemaShading::RequestUpdateExtent(
//     vtkInformation* info,
//     vtkInformationVector** inputVector,
//     vtkInformationVector* outputVector
// ){
//     for(size_t i=0, j=this->GetNumberOfInputPorts(); i<j; i++){
//         if(inputVector[i]->GetNumberOfInformationObjects()>0){
//             vtkInformation* inInfo = inputVector[i]->GetInformationObject(0);
//             if(inInfo->Has(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT())){
//                 inInfo->Set(
//                     vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(),
//                     inInfo->Get( vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT() ),
//                     6
//                 );
//             }
//         }
//     }

//     return 1;
// }

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

int ttkCinemaShading::SetColorTransferFunctionString(std::string ctfs){
    this->Modified();

    std::vector<double> values;
    if( !ttkUtils::stringListToDoubleVector(ctfs, values) ){
        dMsg(cout, "[ttkCinemaShading] ERROR: Unable to parse 'ColorTransferFunctionString'.\n", fatalMsg);
        return 0;
    }

    if(values.size()<1){
        values.resize(8);
        values[0]=0;
        values[1]=0; values[2]=0; values[3]=0;
        values[4]=1;
        values[5]=1; values[6]=1; values[7]=1;
    }

    if(values.size()%4 != 0){
        dMsg(cout, "[ttkCinemaShading] ERROR: 'ColorTransferFunctionString' must be a list of 4-tuples (x0,r0,g0,b0, x1,r1,g1,b1, ...).\n", fatalMsg);
        return 0;
    }

    // Interpolated Colors
    this->ColorTransferFunction->RemoveAllPoints();
    for(size_t i=0, j=values.size(); i<j; i+=4)
        this->ColorTransferFunction->AddRGBPoint(  values[i], values[i+1], values[i+2], values[i+3]);

    // Non-Interpolated Colors
    this->colorMap.clear();
    for(size_t i=0, j=values.size(); i<j; i+=4)
        this->colorMap[ (long)values[i] ] = std::make_tuple(
            floor(values[i+1]*255),
            floor(values[i+2]*255),
            floor(values[i+3]*255)
        );

    return 1;
}

// =============================================================================
// Process Pass
// =============================================================================
int ttkCinemaShading::ProcessPass( vtkImageData* image, std::string shaderName, std::string outputArrayName ){
    dMsg(cout, " "+shaderName+" ", infoMsg);
    ttk::Timer t;

    this->RenderWindowToImageFilter->Modified();
    this->RenderWindowToImageFilter->Update();

    auto windowImage = this->RenderWindowToImageFilter->GetOutput();

    auto windowImageArray = vtkUnsignedCharArray::SafeDownCast( windowImage->GetPointData()->GetAbstractArray(0) );
    auto passArray = vtkSmartPointer<vtkAbstractArray>::Take( windowImageArray->NewInstance() );
    passArray->DeepCopy( windowImageArray );
    passArray->SetName( outputArrayName.data() );

    image->GetPointData()->AddArray( passArray );

    dMsg(cout, "("+std::to_string(t.getElapsedTime())+"s);", infoMsg);
    return 1;
}

template<class dataType> int MapScalarsToInterpolatedColors(
    int             nThreads,
    vtkColorTransferFunction* colorTransferFunction,
    double*         NaNColor,
    dataType*       scalarData,
    size_t          nScalars,
    unsigned char*  rgbData
){
    #ifdef TTK_ENABLE_OPENMP
    #pragma omp parallel for num_threads( nThreads )
    #endif
    for(size_t i=0; i<nScalars; i++){
        double color[3];
        double value = (double)scalarData[i];
        if(isnan(value)){
            color[0] = NaNColor[0];
            color[1] = NaNColor[1];
            color[2] = NaNColor[2];
        } else {
            colorTransferFunction->GetColor( value, color );
        }

        size_t rgbOffset = i*3;
        rgbData[rgbOffset++] = color[0]*255;
        rgbData[rgbOffset++] = color[1]*255;
        rgbData[rgbOffset  ] = color[2]*255;
    }

    return 1;
}

template<class dataType> int MapScalarsToConstantColors(
    int             nThreads,
    ColorMap&       colorMap,
    double*         NaNColor,
    dataType*       scalarData,
    size_t          nScalars,
    unsigned char*  rgbData
){
    unsigned char NaNColorAsUC[3] = {
        (unsigned char) floor(NaNColor[0]*255),
        (unsigned char) floor(NaNColor[1]*255),
        (unsigned char) floor(NaNColor[2]*255)
    };

    #ifdef TTK_ENABLE_OPENMP
    #pragma omp parallel for num_threads( nThreads )
    #endif
    for(size_t i=0; i<nScalars; i++){
        auto scalar = scalarData[i];
        double scalarAsLong = (double) scalar;
        size_t rgbOffset = i*3;
        auto it = colorMap.find( scalarAsLong );

        if(isnan(scalar) || it==colorMap.end()) {
            rgbData[rgbOffset++] = NaNColorAsUC[0];
            rgbData[rgbOffset++] = NaNColorAsUC[1];
            rgbData[rgbOffset  ] = NaNColorAsUC[2];
        } else {
            rgbData[rgbOffset++] = std::get<0>(it->second);
            rgbData[rgbOffset++] = std::get<1>(it->second);
            rgbData[rgbOffset  ] = std::get<2>(it->second);
        }
    }

    return 1;
}

// =============================================================================
// Map Scalars
// =============================================================================
int ttkCinemaShading::MapScalars( vtkImageData* inputImage, std::string scalarArrayName, vtkImageData* outputImage, std::string outputArrayName ){
    dMsg(cout, " Map Scalars ", infoMsg);
    ttk::Timer t;

    {
        int dimI[3];
        int dimO[3];
        inputImage->GetDimensions(dimI);
        outputImage->GetDimensions(dimO);

        if(dimI[0]!=dimO[0] || dimI[1]!=dimO[1] || dimI[2]!=dimO[2]){
            dMsg(cout, "failed\n[ttkCinemaShading] ERROR: input image ("+std::to_string(dimI[0])+"x"+std::to_string(dimI[1])+"x"+std::to_string(dimI[2])+") and output image ("+std::to_string(dimO[0])+"x"+std::to_string(dimO[1])+"x"+std::to_string(dimO[2])+") do not have the same resolution.\n", fatalMsg);
            return 0;
        }
        cout<<"x"<<endl;
    }

    auto scalarArray = inputImage->GetPointData()->GetAbstractArray( scalarArrayName.data() );
    if(!scalarArray){
        dMsg(cout, "failed\n[ttkCinemaShading] ERROR: input point data does not have scalar array '"+scalarArrayName+"'.\n", fatalMsg);
        return 0;
    }

    size_t n = scalarArray->GetNumberOfTuples();
    size_t m = scalarArray->GetNumberOfComponents();
    if( m!=1 ){
        dMsg(cout, "failed\n[ttkCinemaShading] ERROR: Color mapping can only be applied to scalars.\n", fatalMsg);
        return 0;
    }

    auto rgbArray = vtkSmartPointer<vtkUnsignedCharArray>::New();
    rgbArray->SetName( outputArrayName.data() );
    rgbArray->SetNumberOfComponents(3);
    rgbArray->SetNumberOfTuples( n );
    auto rgbData = (unsigned char*) rgbArray->GetVoidPointer(0);

    switch(scalarArray->GetDataType()){
        vtkTemplateMacro({
            auto scalarData = (VTK_TT*) scalarArray->GetVoidPointer(0);

            if(this->InterpolateColors){
                MapScalarsToInterpolatedColors<VTK_TT>(
                    this->threadNumber_,
                    this->ColorTransferFunction,
                    this->NaNColor,
                    scalarData,
                    n,
                    rgbData
                );
            } else {
                MapScalarsToConstantColors<VTK_TT>(
                    this->threadNumber_,
                    this->colorMap,
                    this->NaNColor,
                    scalarData,
                    n,
                    rgbData
                );
            }
        });
    }

    outputImage->GetPointData()->AddArray( rgbArray );

    dMsg(cout, "("+std::to_string(t.getElapsedTime())+"s);", infoMsg);
    return 1;
}

// =============================================================================
// InitFullScreenQuad
// =============================================================================
int ttkCinemaShading::InitFullScreenQuad(){

    auto ps = vtkSmartPointer<vtkPlaneSource>::New();
    ps->SetOrigin(-1, -1, 0);
    ps->SetPoint1( 1, -1, 0);
    ps->SetPoint2(-1,  1, 0);
    ps->Update();

    this->FullScreenQuad = vtkSmartPointer<vtkPolyData>::New();
    this->FullScreenQuad->DeepCopy( ps->GetOutput() );
    this->FullScreenQuadActor = vtkSmartPointer<vtkActor>::New();

    return 1;
}

// =============================================================================
// InitMappers
// =============================================================================
int ttkCinemaShading::SetMapper(std::string vertexShaderCode, std::string fragmentShaderCode){
    auto mapper = vtkSmartPointer<vtkOpenGLPolyDataMapper>::New();
    mapper->SetInputData( this->FullScreenQuad );
    // mapper->SetVertexShaderCode( vertexShaderCode.data() );
    // mapper->SetFragmentShaderCode( fragmentShaderCode.data() );

    // Set uniforms in a totally inconvenient way
    {
        class vtkShaderCallback : public vtkCommand{
            public:
                static vtkShaderCallback* New(){ return new vtkShaderCallback; }
                ttkCinemaShading* wrapper;
                void Execute(vtkObject*, unsigned long, void* calldata) override {
                    auto program = reinterpret_cast<vtkShaderProgram*>(calldata);

                    UniformMap* uniforms = this->wrapper->GetUniforms();

                    for(auto it: *uniforms){
                        switch( it.second.size() ){
                            case 1:
                                program->SetUniformf(it.first.data(), it.second[0]);
                                break;
                            case 2:
                                program->SetUniform2f(it.first.data(), it.second.data());
                                break;
                            case 3:
                                program->SetUniform3f(it.first.data(), it.second.data());
                                break;
                        }
                    }
                }
        };

        auto uniformCallback = vtkSmartPointer<vtkShaderCallback>::New();
        uniformCallback->wrapper = this;
        mapper->AddObserver(vtkCommand::UpdateShaderEvent, uniformCallback);
    }

    this->FullScreenQuadActor->SetMapper( mapper );
    this->FullScreenQuadActor->GetShaderProperty()->SetVertexShaderCode( vertexShaderCode.data() );
    this->FullScreenQuadActor->GetShaderProperty()->SetFragmentShaderCode( fragmentShaderCode.data() );

    return 1;
}

// =============================================================================
// InitRenderer
// =============================================================================
int ttkCinemaShading::InitRenderer(){
    // Renderer
    this->Renderer = vtkSmartPointer<vtkRenderer>::New();
    this->Renderer->AddActor( this->FullScreenQuadActor );
    this->Renderer->SetBackground(0,0,0);

    // Camera
    auto camera = vtkSmartPointer<vtkCamera>::New();
    camera->SetParallelProjection(true);
    camera->SetClippingRange( 0, 2 );
    camera->SetPosition( 0, 0, 1 );
    camera->SetFocalPoint( 0, 0, 0 );
    camera->SetParallelScale( 1 ); // Will be ignored because quad positions are fixed
    this->Renderer->SetActiveCamera( camera );


    // create the basic VTK render steps
//  vtkNew<vtkRenderStepsPass> basicPasses;

//  vtkNew<vtkFramebufferPass> fop;
//  fop->SetDelegatePass(basicPasses);
//  fop->SetDepthFormat(vtkTextureObject::Fixed24);
//  fop->SetColorFormat(vtkTextureObject::Float32);

//  // tell the renderer to use our render pass pipeline
//  vtkOpenGLRenderer::SafeDownCast(this->Renderer)->SetPass(fop);


    // Window
    this->RenderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    this->RenderWindow->AddRenderer( this->Renderer );
    this->RenderWindow->SetMultiSamples( 0 ); // Disable AA
    this->RenderWindow->OffScreenRenderingOn();

    // Window to Image Filter
    this->RenderWindowToImageFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
    this->RenderWindowToImageFilter->SetInput( this->RenderWindow );

    return 1;
}

// =============================================================================
// InitPipeline
// =============================================================================
int ttkCinemaShading::InitPipeline(int resX, int resY){
    // Clear Textures and Uniforms
    this->RemoveTexture();
    this->RemoveUniform();

    // Set Render Size
    this->RenderWindow->SetSize( resX, resY );
    this->SetUniform("uResolution", resX, resY);

    return 1;
}

// =============================================================================
// Texture Functions
// =============================================================================
int ttkCinemaShading::RemoveTexture(std::string textureName){
    if(textureName.compare("")==0)
        this->FullScreenQuadActor->GetProperty()->RemoveAllTextures();
    else
        this->FullScreenQuadActor->GetProperty()->RemoveTexture( textureName.data() );
    return 1;
}

int ttkCinemaShading::AddTexture(vtkImageData* image, std::string arrayName, std::string textureName, int* textureProperties){
    auto imagePD = image->GetPointData();
    if( !imagePD->HasArray(arrayName.data()) ){
        dMsg(cout, "failed\n[ttkCinemaShading] ERROR: input point data does not have array '"+arrayName+"'.\n", fatalMsg);
        return 0;
    }

    // get properties
    auto properties = this->FullScreenQuadActor->GetProperty();

    // if texture already exists remove it
    if( properties->GetTexture(textureName.data()) )
        properties->RemoveTexture( textureName.data() );

    // create texture
    auto texture = vtkSmartPointer<vtkTexture>::New();
    texture->SetInputData( image );
    texture->SetInputArrayToProcess(
        0,0,0,
        vtkDataObject::FieldAssociations::FIELD_ASSOCIATION_POINTS,
        arrayName.data()
    );
    texture->SetColorModeToDirectScalars();
    texture->SetEdgeClamp( textureProperties[1] );
    texture->SetRepeat( !textureProperties[2] );
    texture->SetQualityTo32Bit();
    if(textureProperties[0]==1)
        vtkOpenGLTexture::SafeDownCast( texture )->SetIsDepthTexture(1);

    // update texture
    texture->Update();

    // add texture to properties
    properties->SetTexture( textureName.data(), texture);

    return 1;
}

// =============================================================================
// Misc Array Functions
// =============================================================================
int ttkCinemaShading::CopyArray(vtkImageData* image, std::string srcArrayName, std::string targetArrayName){
    auto imagePD = image->GetPointData();
    if( !imagePD->HasArray(srcArrayName.data()) ){
        dMsg(cout, "failed\n[ttkCinemaShading] ERROR: input point data does not have array '"+srcArrayName+"'.\n", fatalMsg);
        return 0;
    }

    auto srcArray = imagePD->GetAbstractArray( srcArrayName.data() );
    auto targetArray = vtkSmartPointer<vtkAbstractArray>::Take( srcArray->NewInstance() );
    targetArray->DeepCopy(srcArray);
    targetArray->SetName( targetArrayName.data() );

    imagePD->AddArray( targetArray );

    return 1;
}

// =============================================================================
// RequestData
// =============================================================================
int ttkCinemaShading::RequestData(
    vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector
){
    // Print Status
    {
        std::stringstream msg;
        msg<<"================================================================================"<<endl;
        msg<<"[ttkCinemaShading] RequestData"<<endl;
        dMsg(cout, msg.str(), infoMsg);
    }

    ttk::Timer t;

    // -------------------------------------------------------------------------
    // Prepare Input
    // -------------------------------------------------------------------------
    std::vector< vtkSmartPointer<vtkMultiBlockDataSet> > inputAsMBs;
    size_t nInputPorts = this->GetNumberOfInputPorts();
    size_t nBlocks = 0;
    {
        inputAsMBs.resize(nInputPorts);

        for(size_t i=0, j=nInputPorts; i<j; i++){
            inputAsMBs[i] = vtkSmartPointer<vtkMultiBlockDataSet>::New();

            if(inputVector[i]->GetNumberOfInformationObjects()==1){
                bool error = false;

                auto inputObject = inputVector[i]->GetInformationObject(0)->Get(vtkDataObject::DATA_OBJECT());

                if( auto inputAsMB = vtkMultiBlockDataSet::GetData(inputVector[i]) ){
                    // check if all blocks are vtkImageData objects
                    for(size_t j=0, k=inputAsMB->GetNumberOfBlocks(); j<k; j++)
                        if( !vtkImageData::SafeDownCast(inputAsMB->GetBlock(j)) )
                            error = true;

                    inputAsMBs[i]->ShallowCopy( inputAsMB );
                } else if( vtkImageData::SafeDownCast(inputObject) ) {
                    inputAsMBs[i]->SetBlock( 0, inputObject );
                } else {
                    error = true;
                }

                if(error){
                    dMsg(cout, "[ttkCinemaShading] ERROR: Input at port "+std::to_string(i)+" is not a 'vtkImageData' nor a 'vtkMultiBlockDataSet' object holding 'vtkImageData'.\n", fatalMsg);
                    return 0;
                }
            }
        }

        nBlocks = inputAsMBs[0]->GetNumberOfBlocks();

        // Check if MBs have the same number of blocks
        for(size_t i=0; i<nInputPorts; i++)
            if(inputAsMBs[i]->GetNumberOfBlocks()>1 && inputAsMBs[i]->GetNumberOfBlocks()!=nBlocks){
                dMsg(cout, "[ttkCinemaShading] ERROR: Input vtkMultiBlockDataSets at port 0 and port "+std::to_string(i)+" do not have the same number of blocks.\n", fatalMsg);
                return 0;
            }
    }

    // -------------------------------------------------------------------------
    // Prepare Output
    // -------------------------------------------------------------------------
    auto outputAsMB = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    {
        auto outputObject = outputVector->GetInformationObject(0)->Get(vtkDataObject::DATA_OBJECT());

        if(this->GetOutputMode()==0){
            // Add Array
            auto outputObjectAsMB = vtkMultiBlockDataSet::SafeDownCast(outputObject);
            auto outputObjectAsID = vtkImageData::SafeDownCast(outputObject);

            if( outputObjectAsMB ){
                for(size_t i=0; i<nBlocks; i++){
                    auto copy = vtkSmartPointer<vtkImageData>::New();
                    copy->ShallowCopy(
                        vtkImageData::SafeDownCast(inputAsMBs[0]->GetBlock(i))
                    );
                    outputAsMB->SetBlock( i, copy );
                }
                outputObject->ShallowCopy(outputAsMB);
            } else if( outputObjectAsID ){
                outputObjectAsID->ShallowCopy(
                    vtkImageData::SafeDownCast( inputAsMBs[0]->GetBlock(0) )
                );
                outputAsMB->SetBlock(0, outputObjectAsID);
            } else {
                dMsg(cout, "[ttkCinemaShading] ERROR: Output is not a 'vtkImageData' nor a 'vtkMultiBlockDataSet' object holding 'vtkImageData'.\n", fatalMsg);
                return 0;
            }
        } else {
            // New Image
            auto resolution = this->GetResolution();

            auto prepImage = [](int res[2]){
                auto image = vtkSmartPointer<vtkImageData>::New();
                image->SetExtent(
                    0, res[0]-1,
                    0, res[1]-1,
                    0, 0
                );
                return image;
            };
            auto outputObjectAsMB = vtkMultiBlockDataSet::SafeDownCast(outputObject);
            auto outputObjectAsID = vtkImageData::SafeDownCast(outputObject);

            if(outputObjectAsMB){
                for(size_t i=0; i<nBlocks; i++)
                    outputObjectAsMB->SetBlock( i, prepImage(resolution) );
                outputAsMB->ShallowCopy( outputObjectAsMB );
            } else if(outputObjectAsID) {
                outputObjectAsID->ShallowCopy( prepImage(resolution) );
                outputAsMB->SetBlock(0, outputObjectAsID);
            } else {
                dMsg(cout, "[ttkCinemaShading] ERROR: Output is not a 'vtkImageData' nor a 'vtkMultiBlockDataSet' object holding 'vtkImageData'.\n", fatalMsg);
                return 0;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Init Pipeline
    // -------------------------------------------------------------------------
    {
        // Renderer Resolution
        if(this->GetOutputMode()==0){
            // Add Array
            if( nBlocks>0 ) {
                auto dims = vtkImageData::SafeDownCast( inputAsMBs[0]->GetBlock(0) )->GetDimensions();
                if( !this->InitPipeline( dims[0], dims[1] ) ) return 0;
            };
        } else {
            // New Image
            if( !this->InitPipeline( this->Resolution[0], this->Resolution[1] ) ) return 0;
        }
    }

    // -------------------------------------------------------------------------
    // Process Images
    // -------------------------------------------------------------------------
    for(size_t i=0; i<nBlocks; i++){
        dMsg(cout, "[ttkCinemaShading] "+std::to_string(i)+"/"+std::to_string(nBlocks)+": ", infoMsg);

        auto outputImage = vtkImageData::SafeDownCast( outputAsMB->GetBlock(i) );

        // Prepare textures if shader mode is not set to 'color mapping'
        if(this->GetShader()!=1){
            for(size_t j=0; j<nInputPorts; j++){
                auto nBlocksAtPortI = inputAsMBs[j]->GetNumberOfBlocks();
                if(nBlocksAtPortI<1 || (nBlocksAtPortI==1 && i>0)) continue;

                this->AddTexture(
                    vtkImageData::SafeDownCast( inputAsMBs[j]->GetBlock(i) ),
                    this->GetTextureArrayName(j),
                    "tex"+std::to_string(j),
                    this->GetTextureProperties(j)
                );
            }
        }

        // ---------------------------------------------------------------------
        // Run Shader
        // ---------------------------------------------------------------------
        switch( this->GetShader() ){
            // -----------------------------------------------------------------
            // Custom Shader
            // -----------------------------------------------------------------
            case ttkCinemaShading::ShaderMode::CUSTOM: {
                if( !this->SetMapper(
                    this->GetCustomVertexShaderCode(),
                    this->GetCustomFragmentShaderCode()
                ) ) return 0;

                if( !this->ProcessPass(outputImage, "C.Shader", this->GetOutputArrayName()) ) return 0;

                break;
            }

            // -----------------------------------------------------------------
            // Color Mapping
            // -----------------------------------------------------------------
            case ttkCinemaShading::ShaderMode::COLOR_MAPPING: {
                if( !this->MapScalars(
                    vtkImageData::SafeDownCast( inputAsMBs[0]->GetBlock(i) ),
                    this->GetTextureArrayName(0),
                    outputImage,
                    this->GetOutputArrayName()
                ) ) return 0;

                break;
            }

            // -----------------------------------------------------------------
            // Resize
            // -----------------------------------------------------------------
            case ttkCinemaShading::ShaderMode::RESIZE: {
                if( !this->SetMapper(
                    #include "./shaders/FullScreenQuad_VS.glsl"
                    ,
                    #include "./shaders/PassTrough_FS.glsl"
                ) ) return 0;

                if( !this->ProcessPass(outputImage, "Resize", this->GetOutputArrayName()) ) return 0;

                break;
            }

            // -----------------------------------------------------------------
            // Blur
            // -----------------------------------------------------------------
            case ttkCinemaShading::ShaderMode::BLUR: {
                // Set BLUR Shader
                if( !this->SetMapper(
                    #include "./shaders/FullScreenQuad_VS.glsl"
                    ,
                    #include "./shaders/Blur_FS.glsl"
                ) ) return 0;

                // General Uniforms
                if( !this->SetUniform("uBlurRadius", this->uBlurRadius) ) return 0;

                // Horizontal Pass
                if( !this->SetUniform("uBlurDirection", 1, 0) ) return 0;
                if( !this->ProcessPass(outputImage, "Blur", this->GetOutputArrayName()) ) return 0;
                this->AddTexture(
                    outputImage,
                    this->GetOutputArrayName(),
                    "tex0",
                    this->GetTextureProperties(0)
                );

                // Vertical Pass
                if( !this->SetUniform("uBlurDirection", 0, 1) ) return 0;
                if( !this->ProcessPass(outputImage, "Blur", this->GetOutputArrayName()) ) return 0;

                break;
            }

            // -----------------------------------------------------------------
            // SSDD
            // -----------------------------------------------------------------
            case ttkCinemaShading::ShaderMode::SSDD: {
                // Set BLUR Shader
                if( !this->SetMapper(
                    #include "./shaders/FullScreenQuad_VS.glsl"
                    ,
                    #include "./shaders/SSDD_FS.glsl"
                ) ) return 0;

                if( !this->SetUniform("uSSDDStrength", this->uSSDDStrength) ) return 0;
                if( !this->ProcessPass(outputImage, "SSDD", this->GetOutputArrayName()) ) return 0;

                break;
            }

            // -----------------------------------------------------------------
            // SSAO
            // -----------------------------------------------------------------
            case ttkCinemaShading::ShaderMode::SSAO: {
                // Set BLUR Shader
                if( !this->SetMapper(
                    #include "./shaders/FullScreenQuad_VS.glsl"
                    ,
                    #include "./shaders/SSAO_FS.glsl"
                ) ) return 0;

                if( !this->SetUniform("uSSAORadius", this->uSSAORadius) ) return 0;
                if( !this->SetUniform("uSSAOScale", this->uSSAOScale) ) return 0;
                if( !this->SetUniform("uSSAODiffArea", this->uSSAODiffArea) ) return 0;
                if( !this->ProcessPass(outputImage, "SSAO", this->GetOutputArrayName()) ) return 0;

                break;
            }

            // -----------------------------------------------------------------
            // JLUK
            // -----------------------------------------------------------------
            case ttkCinemaShading::ShaderMode::JLUK: {
                if( !this->SetMapper(
                    #include "./shaders/FullScreenQuad_VS.glsl"
                    ,
                    #include "./shaders/JLUK_FS.glsl"
                ) ) return 0;

                if( !this->SetUniform("uJLUKRadius", this->uJLUKRadius) ) return 0;
                if( !this->SetUniform("uJLUKScale", this->uJLUKScale) ) return 0;
                if( !this->SetUniform("uJLUKDiffArea", this->uJLUKDiffArea) ) return 0;
                if( !this->SetUniform("uJLUKNoise", this->uJLUKNoise) ) return 0;
                if( !this->SetUniform("uJLUKAOFactor", this->uJLUKAOFactor) ) return 0;
                if( !this->SetUniform("uJLUKNormalFactor", this->uJLUKNormalFactor) ) return 0;
                if( !this->SetUniform("uJLUKLuminanceFactor", this->uJLUKLuminanceFactor) ) return 0;
                if( !this->ProcessPass(outputImage, "JLUK", this->GetOutputArrayName()) ) return 0;

                break;
            }

            // -----------------------------------------------------------------
            // Unsupported Shader
            // -----------------------------------------------------------------
            default: {
                dMsg(cout, "failed\n[ttkCinemaShading] ERROR: Currently unsupported shader.\n", fatalMsg);
                return 0;
            }

        }

        // Close Output
        dMsg(cout, "\n", infoMsg);
    }

    // -------------------------------------------------------------------------
    // Clear Data
    // -------------------------------------------------------------------------
    {
        this->RemoveTexture();
        this->RemoveUniform();
    }

    // -------------------------------------------------------------------------
    // Output Performance
    // -------------------------------------------------------------------------
    {
        std::stringstream msg;
        msg << "[ttkCinemaShading] -------------------------------------------------------------" << endl;
        msg << "[ttkCinemaShading]   time: " << t.getElapsedTime() << " s" << endl;
        dMsg(cout, msg.str(), timeMsg);
    }

    return 1;
}