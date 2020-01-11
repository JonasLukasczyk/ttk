const fs = require('fs');

let xml = `<?xml version="1.0" encoding="UTF-8"?>
<ServerManagerConfiguration>
    <ProxyGroup name="filters">
        <SourceProxy name="MapArray" class="ttkMapArray" label="TTK MapArray">
            <Documentation long_help="TTK MapArray" short_help="TTK MapArray">
                TODO
            </Documentation>

            <InputProperty name="Source" port_index="0" command="SetInputConnection">
                <ProxyGroupDomain name="groups">
                    <Group name="sources" />
                    <Group name="filters" />
                </ProxyGroupDomain>
                <InputArrayDomain name="source_arrays" attribute_type="any" number_of_components="1"/>
                <Documentation>vtkDataSet that is used to build the map.</Documentation>
            </InputProperty>
            <InputProperty name="Target" port_index="1" command="SetInputConnection">
                <ProxyGroupDomain name="groups">
                    <Group name="sources" />
                    <Group name="filters" />
                </ProxyGroupDomain>
                <InputArrayDomain name="target_arrays" attribute_type="any" number_of_components="1"/>
                <Documentation>vtkDataSet or vtkMultiBlockDataSet that queries the map.</Documentation>
            </InputProperty>

            <!-- Map Type -->
            <IntVectorProperty name="Implementation" label="Implementation" command="SetImplementation" number_of_elements="1" default_values="0">
                <EnumerationDomain name="enum">
                    <Entry value="0" text="Map"/>
                    <Entry value="1" text="Array"/>
                </EnumerationDomain>
                <Documentation>TODO.</Documentation>
            </IntVectorProperty>
`;


const nMaxKeys = 2;

xml += `
            <!-- Build Map -->
`;
for(let i=0; i<nMaxKeys; i++){
    const keyLabel = 'Key'+i;

    xml += `
            <IntVectorProperty name="${keyLabel}Mode" label="${keyLabel} Mode" command="Set${keyLabel}Mode" number_of_elements="1" default_values="0">
                <EnumerationDomain name="enum">
                    <Entry value="0" text="Disabled"/>
                    <Entry value="1" text="Const"/>
                    <Entry value="2" text="BlockIndex"/>
                    <Entry value="3" text="Field"/>
                </EnumerationDomain>
                <Documentation>TODO.</Documentation>
            </IntVectorProperty>
            <StringVectorProperty command="Set${keyLabel}Const" label="${keyLabel}" name="${keyLabel}Const" number_of_elements="1">
                <Hints>
                    <PropertyWidgetDecorator type="GenericDecorator" mode="visibility" property="${keyLabel}Mode" value="1" />
                </Hints>
                <Documentation>TODO</Documentation>
            </StringVectorProperty>
            <StringVectorProperty command="SetInputArrayToProcess" element_types="0 0 0 0 2" label="${keyLabel}" name="${keyLabel}Array" number_of_elements="5" default_values="${1+i} 0">
                <ArrayListDomain attribute_type="Scalars" name="array_list">
                    <RequiredProperties>
                        <Property function="Input" name="Source" />
                    </RequiredProperties>
                </ArrayListDomain>
                <Hints>
                    <PropertyWidgetDecorator type="GenericDecorator" mode="visibility" property="${keyLabel}Mode" value="3" />
                </Hints>
                <Documentation>TODO</Documentation>
            </StringVectorProperty>
    `;
}

xml += `
            <StringVectorProperty command="SetInputArrayToProcess" element_types="0 0 0 0 2" label="Value" name="ValueArray" number_of_elements="5">
                <ArrayListDomain attribute_type="Scalars" name="array_list">
                    <RequiredProperties>
                        <Property function="Input" name="Source" />
                    </RequiredProperties>
                </ArrayListDomain>
                <Documentation>TODO</Documentation>
            </StringVectorProperty>
`;

xml += `
            <!-- Map Lookup -->
            <IntVectorProperty name="TargetDomain" label="Domain" command="SetTargetDomain" number_of_elements="1" default_values="0">
                <EnumerationDomain name="enum">
                    <Entry value="0" text="Point"/>
                    <Entry value="1" text="Cell"/>
                    <Entry value="2" text="Field"/>
                </EnumerationDomain>
                <Documentation>TODO.</Documentation>
            </IntVectorProperty>
`;
for(let i=0; i<nMaxKeys; i++){
    const lookupLabel = 'Lookup'+i;

    xml += `
            <IntVectorProperty name="${lookupLabel}Mode" label="${lookupLabel} Mode" command="Set${lookupLabel}Mode" number_of_elements="1" default_values="0">
                <EnumerationDomain name="enum">
                    <Entry value="0" text="Disabled"/>
                    <Entry value="1" text="Const"/>
                    <Entry value="2" text="BlockIndex"/>
                    <Entry value="3" text="Field"/>
                </EnumerationDomain>
                <Documentation>TODO.</Documentation>
            </IntVectorProperty>
            <StringVectorProperty command="Set${lookupLabel}Const" label="${lookupLabel}" name="${lookupLabel}Const" number_of_elements="1">
                <Hints>
                    <PropertyWidgetDecorator type="GenericDecorator" mode="visibility" property="${lookupLabel}Mode" value="1" />
                </Hints>
                <Documentation>TODO</Documentation>
            </StringVectorProperty>
            <StringVectorProperty command="SetInputArrayToProcess" element_types="0 0 0 0 2" label="${lookupLabel}" name="${lookupLabel}Array" number_of_elements="5" default_values="${1+i+nMaxKeys} 1">
                <ArrayListDomain attribute_type="Scalars" name="array_list">
                    <RequiredProperties>
                        <Property function="Input" name="Target" />
                    </RequiredProperties>
                </ArrayListDomain>
                <Hints>
                    <PropertyWidgetDecorator type="GenericDecorator" mode="visibility" property="${lookupLabel}Mode" value="3" />
                </Hints>
                <Documentation>TODO</Documentation>
            </StringVectorProperty>
    `;
}
xml += `
            <DoubleVectorProperty name="DefaultValue" label="Default Value" command="SetDefaultValue" number_of_elements="1" default_values="0">
                <Documentation>Fallback value for missing keys.</Documentation>
            </DoubleVectorProperty>
`;

let keyGroup = '';
let lookupGroup = '';
for(let i=0;i<nMaxKeys;i++){
    keyGroup    += `<Property name="Key${i}Mode" /> <Property name="Key${i}Const" /> <Property name="Key${i}Array" />`;
    lookupGroup += `<Property name="Lookup${i}Mode" /> <Property name="Lookup${i}Const" /> <Property name="Lookup${i}Array" />`;
}

xml += `
            <PropertyGroup panel_widget="Line" label="Source">
                <Property name="Implementation" />
                ${keyGroup}
                <Property name="ValueArray" />
            </PropertyGroup>
            <PropertyGroup panel_widget="Line" label="Target">
                <Property name="TargetDomain" />
                ${lookupGroup}
                <Property name="DefaultValue" />
            </PropertyGroup>

            \$\{DEBUG_WIDGETS\}

            <Hints>
                <ShowInMenu category="TTK - Pipeline" />
            </Hints>
        </SourceProxy>
    </ProxyGroup>
</ServerManagerConfiguration>
`;

fs.writeFileSync("./MapArray.xml", xml);

