/// \ingroup vtk
/// \class ttkCinemaShading
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 01.06.2019
///
/// \brief This filter uses OpenGL shader code to generate rgb images.
///
/// This filter uses OpenGL shader code to generate rgb images. The inputs of the filter (which must be vtkImageData objects or vtkMultiBlockDataSets holding vtkImageData objects) are fed to the shader as textures. This filter can be used for color mapping, to apply Screen Space Ambient Occlusion, or run custom shader code.

#pragma once

#include <unordered_map>

// VTK Module
#include <ttkCinemaShadingModule.h>

// VTK includes
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPolyData.h>
#include <vtkActor.h>
#include <vtkColorTransferFunction.h>

// TTK includes
#include <ttkAlgorithm.h>

// Custom Macros
#define vtkGetSetMacro(name,type) \
    private: type name;\
    public: vtkGetMacro(name, type); vtkSetMacro(name, type);

#define vtkGetSetMacro2(name,type) \
    private: type name[2];\
    public: vtkGetVector2Macro(name, type); vtkSetVector2Macro(name, type);

#define vtkGetSetMacro3(name,type) \
    private: type name[3];\
    public: vtkGetVector3Macro(name, type); vtkSetVector3Macro(name, type);

#define vtkGetSetMacro6(name,type) \
    private: type name[6];\
    public: vtkGetVector6Macro(name, type); vtkSetVector6Macro(name, type);

// Custom DataTypes

typedef std::unordered_map<std::string,std::vector<float>> UniformMap; // name -> (float values);
typedef std::unordered_map<double,std::tuple<unsigned char,unsigned char,unsigned char>> ColorMap;

class TTKCINEMASHADING_EXPORT ttkCinemaShading : public ttkAlgorithm {
    private:
        // Methods
        int InitFullScreenQuad();
        int SetMapper(std::string vertexShaderCode, std::string fragmentShaderCode);
        int InitRenderer();

        int InitPipeline(int resX, int resY);

        int RemoveTexture(std::string textureName="");
        int AddTexture(vtkImageData* image, std::string arrayName, std::string textureName, int* textureProperties);

        int MapScalars( vtkImageData* inputImage, std::string scalarArrayName, vtkImageData* outputImage, std::string outputArrayName );
        int ProcessPass( vtkImageData* image, std::string shaderName, std::string outputArrayName );
        int CopyArray(vtkImageData* image, std::string srcArrayName, std::string targetArrayName);

        // ColorMap
        std::string ColorTransferFunctionString;
        vtkSmartPointer<vtkColorTransferFunction> ColorTransferFunction;
        ColorMap colorMap;

        // Uniforms
        UniformMap Uniforms;

        // Quad
        vtkSmartPointer<vtkPolyData> FullScreenQuad;
        vtkSmartPointer<vtkActor> FullScreenQuadActor;

        // Rendering Tools
        vtkSmartPointer<vtkRenderer> Renderer;
        vtkSmartPointer<vtkRenderWindow> RenderWindow;
        vtkSmartPointer<vtkWindowToImageFilter> RenderWindowToImageFilter;

    public:

        // Global Modes
        enum ShaderMode { CUSTOM, COLOR_MAPPING, RESIZE, BLUR, SSDD, BLUR_SSDD, SSAO, JLUK };
        vtkGetSetMacro(Shader, int);
        vtkGetSetMacro(OutputArrayName, std::string);
        vtkGetSetMacro(OutputMode, int);
        vtkGetSetMacro2(Resolution, int);
        vtkGetSetMacro(CustomVertexShaderCode, std::string);
        vtkGetSetMacro(CustomFragmentShaderCode, std::string);

        // Uniforms
        vtkGetSetMacro(InterpolateColors, bool);
        vtkGetSetMacro3(NaNColor, double);
        vtkGetMacro(ColorTransferFunctionString, std::string)
        int SetColorTransferFunctionString(std::string ctfs);

        vtkGetSetMacro(uBlurRadius, double);

        vtkGetSetMacro(uSSDDStrength, double);

        vtkGetSetMacro(uSSAORadius, double);
        vtkGetSetMacro(uSSAOScale, double);
        vtkGetSetMacro(uSSAODiffArea, double);

        vtkGetSetMacro(uJLUKRadius, double);
        vtkGetSetMacro(uJLUKScale, double);
        vtkGetSetMacro(uJLUKDiffArea, double);
        vtkGetSetMacro(uJLUKNoise, double);
        vtkGetSetMacro(uJLUKAOFactor, double);
        vtkGetSetMacro(uJLUKNormalFactor, double);
        vtkGetSetMacro(uJLUKLuminanceFactor, double);

        // Textures
        vtkGetSetMacro(Tex0Array, std::string);
        vtkGetSetMacro3(Tex0Properties, int);

        vtkGetSetMacro(Tex1Array, std::string);
        vtkGetSetMacro3(Tex1Properties, int);

        vtkGetSetMacro(Tex2Array, std::string);
        vtkGetSetMacro3(Tex2Properties, int);

        vtkGetSetMacro(Tex3Array, std::string);
        vtkGetSetMacro3(Tex3Properties, int);

        std::string GetTextureArrayName(size_t textureID){
            switch (textureID) {
                case 0: return this->GetTex0Array();
                case 1: return this->GetTex1Array();
                case 2: return this->GetTex2Array();
                case 3: return this->GetTex3Array();
            }
            return "";
        }
        int* GetTextureProperties(size_t textureID){
            switch (textureID) {
                case 0: return this->GetTex0Properties();
                case 1: return this->GetTex1Properties();
                case 2: return this->GetTex2Properties();
                case 3: return this->GetTex3Properties();
            }
            return nullptr;
        }


        UniformMap* GetUniforms(){
            return &this->Uniforms;
        }

        int SetUniform(std::string name, float v0){
            std::vector<float> vec(1);
            vec[0] = v0;
            this->Uniforms[name] = vec;
            return 1;
        }
        int SetUniform(std::string name, float v0, float v1){
            std::vector<float> vec(2);
            vec[0] = v0;
            vec[1] = v1;
            this->Uniforms[name] = vec;
            return 1;
        }
        int SetUniform(std::string name, float v0, float v1, float v2){
            std::vector<float> vec(3);
            vec[0] = v0;
            vec[1] = v1;
            vec[2] = v2;
            this->Uniforms[name] = vec;
            return 1;
        }

        int RemoveUniform(std::string name=""){
            if( name.compare("")==0 )
                this->Uniforms.clear();
            else
                this->Uniforms.erase(name);
            return 1;
        }

        static ttkCinemaShading* New();
        vtkTypeMacro(ttkCinemaShading, ttkAlgorithm);

    protected:

        ttkCinemaShading();
        ~ttkCinemaShading();

        int FillInputPortInformation(int port, vtkInformation *info) override;
        int FillOutputPortInformation(int port, vtkInformation *info) override;

        // int RequestUpdateExtent(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;
        int RequestInformation(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;
        int RequestData(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;
};
