const fs = require('fs');

const nT = 4;

const inputSources = [];
const inputPDSources = [];

let xml = `<?xml version="1.0" encoding="UTF-8"?>
<ServerManagerConfiguration>
    <ProxyGroup name="filters">
        <SourceProxy name="CinemaShading" class="ttkCinemaShading" label="TTK CinemaShading">
            <Documentation long_help="TTK CinemaShading" short_help="TTK CinemaShading">This filter uses OpenGL shader code to generate rgb images. The inputs of the filter (which must be vtkImageData objects or vtkMultiBlockDataSets holding vtkImageData objects) are fed to the shader as textures. This filter can be used for color mapping, to apply Screen Space Ambient Occlusion, or run custom shader code.</Documentation>
`;

for(let i=0; i<nT; i++){
    inputSources.push( "Image"+i );
    inputPDSources.push( "Image"+i+"_PointDataArrays" );
    xml += `
            <InputProperty name="${inputSources[i]}" port_index="${i}" command="SetInputConnection">
                <ProxyGroupDomain name="groups">
                    <Group name="sources" />
                    <Group name="filters" />
                </ProxyGroupDomain>
                <DataTypeDomain name="input_type">
                    <DataType value="vtkImageData" />
                    <DataType value="vtkMultiBlockDataSet" />
                </DataTypeDomain>
                <InputArrayDomain name="${inputPDSources[i]}" attribute_type="point" />
                    ${i>0?"<Hints><Optional/></Hints>":""}
                <Documentation>Auxiliary image that can be used to initialize textures.</Documentation>
            </InputProperty>`;
}
xml += "\n";

xml += `
            <IntVectorProperty name="Shader" command="SetShader" number_of_elements="1" animateable="0" label="Shader" default_values="1">
                <EnumerationDomain name="enum">
                    <Entry value="0" text="Custom Shader"/>
                    <Entry value="1" text="Color Mapping"/>
                    <Entry value="2" text="Resize"/>
                    <Entry value="3" text="Blur"/>
                    <Entry value="4" text="SSDD"/>
                    <Entry value="5" text="Blur + SSDD"/>
                    <Entry value="6" text="SSAO"/>
                    <Entry value="7" text="JLUK"/>
                </EnumerationDomain>
                <Documentation>
Different build-in and customizable shaders.

Custom Shader: Write your own vertex and fragment shader code (see the help for example code).

Color Mapping: Apply a color map to a scalar array.

Resize: Resize input images to a new resolution.

Blur: Gaussian blur.

SSDD: Screen Space Depth Darkening [Luft, T., Colditz, C. and Deussen, O., 2006. Image enhancement by unsharp masking the depth buffer (Vol. 25, No. 3, pp. 1206-1213). ACM.]

Blur + SSDD: Combination of Blur and SSDD

SSAO: Screen Space Ambient Occlusion [Ritschel, T., Grosch, T. and Seidel, H.P., 2009, February. Approximating dynamic global illumination in image space. In Proceedings of the 2009 symposium on Interactive 3D graphics and games (pp. 75-82). ACM.]

JLUK: A custom shader by Jonas Lukasczyk for illustrative rendering [Lukasczyk, J., Weber, G., Maciejewski, R., Garth, C. and Leitte, H., 2017, June. Nested tracking graphs. In Computer Graphics Forum (Vol. 36, No. 3, pp. 12-22).]
                </Documentation>
            </IntVectorProperty>
            <StringVectorProperty name="OutputArrayName" command="SetOutputArrayName" number_of_elements="1" animateable="0" label="Array Name" default_values="Shading">
                <Documentation>Name of the output array.</Documentation>
            </StringVectorProperty>
            <IntVectorProperty name="OutputMode" command="SetOutputMode" number_of_elements="1" animateable="0" label="Mode" default_values="0">
                <EnumerationDomain name="enum">
                    <Entry value="0" text="Add Array"/>
                    <Entry value="1" text="New Image"/>
                </EnumerationDomain>
                <Documentation>
AddArray: add the output Array to the images at the first port (input and output images have to have the same resolution)

NewImage: create a new image for each image at the input port.
                </Documentation>
            </IntVectorProperty>

            <IntVectorProperty command="SetResolution" default_values="512 512" name="Resolution" label="Resolution" number_of_elements="2">
                <Documentation>Resolution of the output image.</Documentation>
                <Hints>
                    <PropertyWidgetDecorator type="GenericDecorator" mode="visibility" property="OutputMode" value="1" />
                </Hints>
            </IntVectorProperty>

            <StringVectorProperty name="CustomVertexShaderCode" command="SetCustomVertexShaderCode" number_of_elements="1" animateable="0" label="Custom Vertex Shader" default_values="//VTK::System::Dec  // always start with this line&#xa;&#xa;attribute vec4 vertexMC;&#xa;varying vec4 vPos;&#xa;&#xa;void main () {&#xa;    vPos = vertexMC/2. + vec4(0.5,0.5,0.5,0);&#xa;    gl_Position = vertexMC;&#xa;}">
                <Hints>
                    <Widget type="multi_line" />
                    <PropertyWidgetDecorator type="GenericDecorator" mode="visibility" property="Shader" value="0" />
                </Hints>
            </StringVectorProperty>
            <StringVectorProperty name="CustomFragmentShaderCode" command="SetCustomFragmentShaderCode" number_of_elements="1" animateable="0" label="Custom Fragment Shader" default_values="//VTK::System::Dec  // always start with this line&#xa;//VTK::Output::Dec  // always have this line in your FS&#xa;&#xa;uniform sampler2D tex0;&#xa;uniform sampler2D tex1;&#xa;uniform sampler2D tex2;&#xa;uniform sampler2D tex3;&#xa;varying vec4 vPos;&#xa;&#xa;void main(void) {&#xa;    gl_FragData[0] = texture2D( tex0, vPos.xy );&#xa;}">
                <Hints>
                    <Widget type="multi_line" />
                    <PropertyWidgetDecorator type="GenericDecorator" mode="visibility" property="Shader" value="0" />
                </Hints>
            </StringVectorProperty>
`;

xml += `
                <PropertyGroup panel_widget="Line" label="Shader">
                    <Property name="Shader" />
                    <Property name="OutputArrayName" />
                    <Property name="OutputMode" />
                    <Property name="Resolution" />
                    <Property name="CustomVertexShaderCode" />
                    <Property name="CustomFragmentShaderCode" />
                </PropertyGroup>
`;

// Color Map
{
    xml += `
        <StringVectorProperty command="SetColorTransferFunctionString" name="ColorMap" number_of_elements="1" animateable="0" label="Color Map" default_values="">
            <Documentation>TODO</Documentation>
            <Hints>
                <PropertyWidgetDecorator type="GenericDecorator" mode="visibility" property="Shader" value="1" />
            </Hints>
        </StringVectorProperty>
    `;
}


// Uniforms
{
    const uniforms = [
        // [Type, nElements, DefaultValues, Name, Docu, ShaderID]
        [['NaNColor','NaN Color', 'color_selector'], ['Double'], 3, ["0 0 0",0,1], 1, 'TODO'],
        [['InterpolateColors','Interpolate'], ['Int',1], 1, [1], 1, 'TODO'],

        [['uBlurRadius',5], ['Double'], 1, [1], 3, 'TODO'],

        [['uSSDDStrength',5], ['Double'], 1, [1], 4, 'TODO'],

        [['uSSAORadius',5], ['Double'], 1, [3,0,40], 6, 'TODO'],
        [['uSSAOScale',5], ['Double'], 1, [10,2,100], 6, 'TODO'],
        [['uSSAODiffArea',5], ['Double'], 1, [0.3,0,1], 6, 'TODO'],

        [['uJLUKRadius',5], ['Double'], 1, [3,0,40], 7, 'TODO'],
        [['uJLUKScale',5], ['Double'], 1, [10,2,1000], 7, 'TODO'],
        [['uJLUKDiffArea',5], ['Double'], 1, [0.3,0,1], 7, 'TODO'],
        [['uJLUKNoise',5], ['Double'], 1, [1,0,10], 7, 'TODO'],
        [['uJLUKAOFactor',5], ['Double'], 1, [1,0,2], 7, 'TODO'],
        [['uJLUKNormalFactor',5], ['Double'], 1, [1,0,1000], 7, 'TODO'],
        [['uJLUKLuminanceFactor',5], ['Double'], 1, [0.6,0,2], 7, 'TODO'],
    ];


    let uniformEntries = "";

    for(let i=0; i<uniforms.length; i++){
        const u = uniforms[i];
        xml += `
                <${u[1][0]}VectorProperty name="${u[0][0]}" label="${isNaN(u[0][1]) ? u[0][1] : u[0][0].slice(u[0][1])}" number_of_elements="${u[2]}" default_values="${u[3][0]}" command="Set${u[0][0]}" ${u[0].length>2 ? `panel_widget="${u[0][2]}"` : ''}>
                    ${ u[3].length>1
                        ? `<${u[1][0]}RangeDomain name="range" min="${u[3][1]}" max="${u[3][2]}" />`
                        : u[1][0]==='Int' && u[1][1]===1
                            ? `<BooleanDomain name="bool" />`
                        :``
                    }
                    <Documentation>${u[5]}</Documentation>
                    <Hints>
                        <PropertyWidgetDecorator type="GenericDecorator" mode="visibility" property="Shader" value="${u[4]}" />
                    </Hints>
                </${u[1][0]}VectorProperty>
        `;
        uniformEntries += `<Property name="${u[0][0]}" />`;
    }

    xml += `
                <PropertyGroup panel_widget="Line" label="Uniforms">
                    ${uniformEntries}
                    <Property name="ColorMap" />
                </PropertyGroup>
    `;
}

// Textures
{
    for(let i=0; i<nT; i++){
        xml += `
                <!-- Tex${i} -->
                <StringVectorProperty name="Tex${i}Array" command="SetTex${i}Array" number_of_elements="1" animateable="0" label="tex${i} Array">
                    <ArrayListDomain name="array_list" default_values="Depth" input_domain_name="${inputPDSources[i]}">
                        <RequiredProperties>
                            <Property name="${inputSources[i]}" function="Input" />
                        </RequiredProperties>
                    </ArrayListDomain>
                    <Documentation>Array of port ${i} used for Tex${i}.</Documentation>
                </StringVectorProperty>
                <IntVectorProperty name="Tex${i}Properties" label="tex${i} Flags" command="SetTex${i}Properties" number_of_elements="3" default_values="1 1 1">
                    <IntRangeDomain name="range" min="0" max="1" />
                    <Documentation>FloatingPointTexture, ClampEdge, NoRepeat</Documentation>
                </IntVectorProperty>
        `;
    }

    xml += `            <PropertyGroup panel_widget="Line" label="Textures">`;
    for(let i=0; i<nT; i++){
        xml += `
                        <Property name="Tex${i}Array" />
                        <Property name="Tex${i}Properties" />
        `;
    }
    xml += '            </PropertyGroup>';
}


xml += `

            \$\{DEBUG_WIDGETS\}

            <Hints>
                <ShowInMenu category="TTK - Cinema" />
            </Hints>
        </SourceProxy>
    </ProxyGroup>
</ServerManagerConfiguration>
`;

fs.writeFileSync("./CinemaShading.xml", xml);

