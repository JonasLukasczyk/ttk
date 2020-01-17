#include <ttkExtract.h>

#include <vtkDataObject.h> // For port info
#include <vtkObjectFactory.h> // for new macro

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkTable.h>

#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkInformationVector.h>

#include <vtkIdTypeArray.h>
#include <vtkFieldData.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkCellArray.h>

#include <ttkUtils.h>

#include <set>

vtkStandardNewMacro(ttkExtract);

ttkExtract::ttkExtract(){
    this->setDebugMsgPrefix( "Extract" );

    this->SetNumberOfInputPorts(1);
    this->SetNumberOfOutputPorts(1);
}
ttkExtract::~ttkExtract(){};

int ttkExtract::GetVtkDataTypeName( std::string& dataTypeName, const int outputType ) const {
    switch (outputType) {
        case -1: // in case of auto return vtkMultiBlockDataSet
        case VTK_MULTIBLOCK_DATA_SET: {
            dataTypeName = "vtkMultiBlockDataSet";
            break;
        } case VTK_UNSTRUCTURED_GRID: {
            dataTypeName = "vtkUnstructuredGrid";
            break;
        } case VTK_IMAGE_DATA: {
            dataTypeName = "vtkImageData";
            break;
        } case VTK_TABLE: {
            dataTypeName = "vtkTable";
            break;
        } default:
            return 0;
    }

    return 1;
}

int ttkExtract::FillInputPortInformation(int port, vtkInformation* info) {
    if (port==0){
        info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet", 0);
        info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkTable", 1);
        info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkUnstructuredGrid", 2);
    } else
        return 0;
    return 1;
}

int ttkExtract::FillOutputPortInformation(int port, vtkInformation* info) {
    if (port==0){
        if(this->OutputType!=-1){
            std::string outputDataTypeName="";
            if( !this->GetVtkDataTypeName(outputDataTypeName, this->OutputType) ){
                this->printErr("Unsupported output type");
                return 0;
            }
            info->Set(vtkDataObject::DATA_TYPE_NAME(), outputDataTypeName.data());
        } else
            info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 0);
    } else
        return 0;
    return 1;
}

// =============================================================================
// RequestInformation
// =============================================================================
int ttkExtract::RequestInformation(
    vtkInformation*,
    vtkInformationVector**,
    vtkInformationVector* outputVector
){
    if(this->ExtractionMode==0 && this->GetOutputType()==VTK_IMAGE_DATA){
        vtkInformation* outInfo = outputVector->GetInformationObject(0);

        // Bounds
        auto imageBounds = this->GetImageBounds();
        outInfo->Set(vtkStreamingDemandDrivenPipeline::BOUNDS(), imageBounds, 6);

        // Extent
        int wholeExtent[6] = {
            (int)imageBounds[0], (int)(imageBounds[1]-imageBounds[0]),
            (int)imageBounds[2], (int)(imageBounds[3]-imageBounds[2]),
            (int)imageBounds[4], (int)(imageBounds[5]-imageBounds[4])
        };
        outInfo->Set(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), wholeExtent, 6);
    }

    return 1;
}

template <class dataType>
int markVertices(
    std::vector<int>& oIndex_to_mIndex_map,
    vtkAbstractArray* inputArray,
    const size_t& nValues,
    const std::vector<double>& labels
){
    auto data = (dataType*) inputArray->GetVoidPointer(0);

    std::vector<dataType> labelsT( nValues );
    for(size_t i=0; i<nValues; i++)
        labelsT[i] = (dataType) labels[i];

    for(size_t i=0, nPoints=inputArray->GetNumberOfTuples(); i<nPoints; i++){
        const auto& dataValue = data[i];
        bool contained = false;
        for(size_t j=0; j<nValues; j++)
            if(labelsT[j] == dataValue){
                contained = true;
                break;
            }
        if(contained)
            oIndex_to_mIndex_map[i] = -2;
    }
    return 1;
}

int doubleVectorToString(std::string& str, const std::vector<double>& vec){
    std::stringstream ss;
    for(auto& v: vec)
        ss<<v<<",";
    ss.seekp(-1,ss.cur);
    str = ss.str().substr(0,ss.str().size()-1);
    return 1;
}

int ttkExtract::ExtractBlocks(
    vtkDataObject* output,
    vtkDataObject* input,
    const std::vector<double>& indices
) const {
    // print state
    std::string indicesString = "";
    doubleVectorToString(indicesString, indices);
    {
        std::string outputDataTypeName = "";
        this->GetVtkDataTypeName( outputDataTypeName, this->OutputType );

        this->printMsg( ttk::debug::Separator::L1 );
        this->printMsg({
            {"Extraction Mode", "Block"},
            {"Output Type", outputDataTypeName},
            {"Indicies", "["+indicesString+"]"}
        });
        this->printMsg( ttk::debug::Separator::L2 );
    }

    this->printMsg("Extracting blocks ["+indicesString+"]", 0, ttk::debug::LineMode::REPLACE);

    auto inputAsMB = vtkMultiBlockDataSet::SafeDownCast(input);
    if(!inputAsMB){
        this->printErr("Block mode requires 'vtkMultiBlockDataSet' input.");
        return 0;
    }

    if(this->OutputType==-1){
        // extract multiple blocks (vtkMultiBlockDataSet input/output only)
        auto outputAsMB = vtkMultiBlockDataSet::SafeDownCast( output );

        for(size_t i=0; i<indices.size(); i++){
            size_t blockIndex = (size_t) indices[i];
            if( 0<=blockIndex && blockIndex<inputAsMB->GetNumberOfBlocks() ){
                auto block = inputAsMB->GetBlock(blockIndex);
                auto copy = vtkSmartPointer<vtkDataObject>::Take( block->NewInstance() );
                copy->ShallowCopy(block);
                outputAsMB->SetBlock( i, copy );
            } else {
                this->printErr("Index out of range ("+std::to_string(blockIndex)+"/"+std::to_string(inputAsMB->GetNumberOfBlocks())+").");
                return 0;
            }
        }
    } else {
        // extract a single block of specified output type
        if( indices.size()!=1 ){
            this->printErr("If OutputType is specified then only one block can be extracted.");
            return 0;
        }

        size_t blockIndex = (size_t) indices[0];
        if( 0<=blockIndex && blockIndex<inputAsMB->GetNumberOfBlocks() ){
            auto block = inputAsMB->GetBlock(blockIndex);
            if(output->GetDataObjectType() != block->GetDataObjectType()){
                this->printErr("BlockType does not match OutputType");
                return 0;
            }
            output->ShallowCopy( block );
        } else {
            this->printErr("Index out of range ("+std::to_string(blockIndex)+"/"+std::to_string(inputAsMB->GetNumberOfBlocks())+").");
            return 0;
        }
    }

    this->printMsg("Extracting blocks ["+indicesString+"]", 1);

    return 1;
}

int ttkExtract::ExtractRows(
    vtkDataObject* output,
    vtkDataObject* input,
    const std::vector<double>& indices
) const {
    // print state
    std::string indicesString = "";
    doubleVectorToString(indicesString, indices);
    {
        this->printMsg( ttk::debug::Separator::L1 );
        this->printMsg({
            {"Extraction Mode", "Rows"},
            {"Indicies", "["+indicesString+"]"}
        });
        this->printMsg( ttk::debug::Separator::L2 );
    }

    ttk::Timer t;
    this->printMsg("Extracting rows ["+indicesString+"]", 0, ttk::debug::LineMode::REPLACE);

    size_t nValues = indices.size();
    auto inputAsT = vtkTable::SafeDownCast( input );
    auto outputAsT = vtkTable::SafeDownCast( output );
    if(!inputAsT || !outputAsT){
        this->printErr("Row mode requires 'vtkTable' input/output.");
        return 0;
    }

    size_t nRows = inputAsT->GetNumberOfRows();
    size_t nCols = inputAsT->GetNumberOfColumns();

    for(size_t j=0; j<nValues; j++)
        if( ((size_t)indices[j])>=nRows ){
            this->printErr("Index out of range ("+std::to_string((size_t)indices[j])+"/"+std::to_string(nRows)+").");
            return 0;
        }

    // Extract row at index
    for(size_t i=0; i<nCols; i++){
        auto iColumn = inputAsT->GetColumn(i);

        auto oColumn = vtkSmartPointer<vtkAbstractArray>::Take( iColumn->NewInstance() );
        oColumn->SetName( iColumn->GetName() );
        oColumn->SetNumberOfComponents( iColumn->GetNumberOfComponents() );
        oColumn->SetNumberOfTuples( nValues );
        for(size_t j=0; j<nValues; j++)
            oColumn->SetTuple(j, indices[j], iColumn);

        outputAsT->AddColumn( oColumn );
    }

    outputAsT->GetFieldData()->ShallowCopy( inputAsT->GetFieldData() );

    this->printMsg("Extracting rows ["+indicesString+"]", 1, t.getElapsedTime());

    return 1;
}

int ttkExtract::ExtractGeometry(
    vtkDataObject* output,
    vtkDataObject* input,
    const std::vector<double>& labels
) {
    ttk::Timer globalTimer;

    size_t nValues = labels.size();

    auto inputArray = this->GetInputArrayToProcess(0, input);
    if(!inputArray){
        this->printErr("Unable to retrieve input array.");
        return 0;
    }
    std::string inputArrayName = inputArray->GetName();

    std::string labelsString = "";
    doubleVectorToString(labelsString, labels);

    // print status
    {
        this->printMsg({
            {"Ext. Mode", "Geometry"},
            {"Cell Mode", std::string(this->CellMode==0 ? "All" : this->CellMode==1 ? "Any" : "Sub")},
            {"Condition", "'"+inputArrayName+"' in ["+labelsString+"]"}
        });
        this->printMsg( ttk::debug::Separator::L1 );
    }

    // check input/output object validity
    auto inputAsUG  = vtkUnstructuredGrid::SafeDownCast(input);
    auto outputAsUG = vtkUnstructuredGrid::SafeDownCast(output);
    if(!inputAsUG || !outputAsUG){
        this->printErr("Geometry mode requires 'vtkUnstructuredGrid' input/output.");
        return 0;
    }

    // check input array validity
    if(this->GetInputArrayAssociation(0,input)!=0){
        this->printErr("Extraction is currently only supported based on point data.");
        return 0;
    }
    if(inputArray->GetNumberOfComponents()!=1){
        this->printErr("Data array '"+inputArrayName+"' must have only one component.");
        return 0;
    }

    // get points and cells
    const size_t nPoints = inputArray->GetNumberOfTuples();
    auto inputPD = inputAsUG->GetPointData();
    auto inputCD = inputAsUG->GetCellData();

    // Input Topo
    size_t nInCells = inputAsUG->GetNumberOfCells();
    auto inTopologyData = inputAsUG->GetCells()->GetPointer();

    // Marked Points:
    //     -1: does not satisfy condition
    //     -2: satisfies condition
    //     -3: satisfies condition extended to cell mode
    //    >=0: mIndex
    std::vector<int> oIndex_to_mIndex_map(nPoints,-1);
    std::vector<bool> markedCells( nInCells, false );
    int nMarkedPoints = 0;

    // Output Topo Stats
    size_t outTopologyDataSize=0;
    size_t nOutCells=0;

    // ---------------------------------------------------------------------
    // Marking Vertices
    // ---------------------------------------------------------------------
    {
        this->printMsg("Marking "+std::string(this->CellMode==0 ? "all" : this->CellMode==1 ? "any" : "sub")+" cells with '"+inputArrayName+"' in ["+labelsString+"]", 0, ttk::debug::LineMode::REPLACE);
        ttk::Timer t;

        // Mark vertices that satisfy condition
        switch( inputArray->GetDataType() ){
            vtkTemplateMacro(
                markVertices<VTK_TT>(
                    oIndex_to_mIndex_map,
                    inputArray,
                    nValues,
                    labels
                )
            );
        }

        // Mark vertices based on cell mode and determine outTopo stats
        {
            if( this->CellMode==0 ){
                // All
                for(size_t i=0, inTopoIndex=0; i<nInCells; i++){
                    size_t nVertices = inTopologyData[ inTopoIndex ];

                    bool all = true;
                    for(size_t j=1; j<=nVertices; j++)
                        if( oIndex_to_mIndex_map[inTopologyData[inTopoIndex+j]]==-1 ){
                            all = false;
                            break;
                        }

                    if(all){
                        nOutCells++;
                        markedCells[i] = true;
                        for(size_t j=1; j<=nVertices; j++)
                            oIndex_to_mIndex_map[ inTopologyData[inTopoIndex+j] ] = -3;
                        outTopologyDataSize += 1 + nVertices;
                    }

                    inTopoIndex += nVertices+1;
                }

            } else if( this->CellMode==1 ){
                // Any

                // Mark Border Vertices
                for(size_t i=0, inTopoIndex=0; i<nInCells; i++){
                    size_t nVertices = inTopologyData[ inTopoIndex ];
                    size_t mVertices = 0;

                    for(size_t j=1; j<=nVertices; j++){
                        auto& mIndex = oIndex_to_mIndex_map[inTopologyData[inTopoIndex+j]];
                        if( mIndex==-2 || mIndex==-3 )
                            mVertices++;
                    }

                    // Mark border vertices
                    if( mVertices>0 ){
                        nOutCells++;
                        markedCells[i] = true;
                        for(size_t j=1; j<=nVertices; j++){
                            auto& mIndex = oIndex_to_mIndex_map[inTopologyData[inTopoIndex+j]];
                            if(mIndex==-1) mIndex = -4; // Border Vertex
                            else if(mIndex==-2) mIndex = -3; // Inner Vertex
                        }
                        outTopologyDataSize += 1 + nVertices;
                    }

                    inTopoIndex += nVertices+1;
                }

                for(size_t i=0; i<nPoints; i++){
                    auto& mIndex = oIndex_to_mIndex_map[ i ];
                    if( mIndex==-4 )
                        mIndex=-3;
                }

            } else if ( this->CellMode==2 ){
                // Sub
                for(size_t i=0, inTopoIndex=0; i<nInCells; i++){
                    size_t nVertices = inTopologyData[ inTopoIndex ];
                    size_t mVertices = 0;

                    for(size_t j=1; j<=nVertices; j++){
                        const auto& mIndex = oIndex_to_mIndex_map[ inTopologyData[inTopoIndex+j] ];
                        if ( mIndex==-2 || mIndex==-3  )
                            mVertices++;
                    }

                    if ( mVertices>0 ){
                        nOutCells++;
                        markedCells[i] = true;
                        for(size_t j=1; j<=nVertices; j++){
                            auto& mIndex = oIndex_to_mIndex_map[ inTopologyData[inTopoIndex+j] ];
                            if( mIndex==-2 )
                                mIndex = -3;
                        }
                        outTopologyDataSize += 1 + mVertices;
                    }

                    inTopoIndex += nVertices+1;
                }
            }
        }

        for(size_t i=0; i<nPoints; i++)
            if( oIndex_to_mIndex_map[i]==-2 || oIndex_to_mIndex_map[i]==-3 )
                oIndex_to_mIndex_map[i] = nMarkedPoints++;

        this->printMsg(
            "Marking "+std::string(this->CellMode==0 ? "all" : this->CellMode==1 ? "any" : "sub")+" cells with '"+inputArrayName+"' in ["+labelsString+"]",
            1, t.getElapsedTime()
        );
    }

    // ---------------------------------------------------------------------
    // Extracting Points
    // ---------------------------------------------------------------------
    {
        ttk::Timer t;
        this->printMsg("Extracting marked vertices", 0, ttk::debug::LineMode::REPLACE);

        auto inPoints = inputAsUG->GetPoints();
        auto inPointCoords = (float*) inPoints->GetVoidPointer(0);

        auto outPoints = vtkSmartPointer<vtkPoints>::New();
        outPoints->SetNumberOfPoints( nMarkedPoints );
        auto outPointCoords = (float*) outPoints->GetVoidPointer(0);
        outputAsUG->SetPoints(outPoints);

        // Extract point coordinates
        {
            for(size_t i=0, j=0; i<nPoints; i++){
                const auto& mIndex = oIndex_to_mIndex_map[i];
                if(  mIndex>=0 ){
                    int iOffset = i*3;
                    outPointCoords[j++] = inPointCoords[iOffset++];
                    outPointCoords[j++] = inPointCoords[iOffset++];
                    outPointCoords[j++] = inPointCoords[iOffset];
                }
            }
        }

        // Extract point data
        {
            size_t nArrays = inputPD->GetNumberOfArrays();
            auto outputPD = outputAsUG->GetPointData();
            for(size_t arrayIndex=0; arrayIndex<nArrays; arrayIndex++){
                auto iArray = inputPD->GetAbstractArray( arrayIndex );
                auto oArray = vtkSmartPointer<vtkAbstractArray>::Take( iArray->NewInstance() );
                oArray->SetName( iArray->GetName() );
                oArray->SetNumberOfComponents( iArray->GetNumberOfComponents() );
                oArray->SetNumberOfTuples( nMarkedPoints );

                switch( iArray->GetDataType() ){
                    vtkTemplateMacro({
                        auto iArrayData = (VTK_TT*) iArray->GetVoidPointer(0);
                        auto oArrayData = (VTK_TT*) oArray->GetVoidPointer(0);

                        for(size_t i=0, j=0; i<nPoints; i++){
                            const auto& mIndex = oIndex_to_mIndex_map[i];
                            if( mIndex>=0 )
                                oArrayData[j++] = iArrayData[ i ];
                        }
                    });
                }

                outputPD->AddArray( oArray );
            }
        }

        this->printMsg(
            "Extracting marked vertices (#"+std::to_string(nMarkedPoints)+")",
            1, t.getElapsedTime()
        );
    }

    // -------------------------------------------------------------------------
    // Extracting Cells
    // -------------------------------------------------------------------------
    {
        ttk::Timer t;
        this->printMsg( "Extracting marked cells", 0, ttk::debug::LineMode::REPLACE );

        const int types[20] = {
            VTK_EMPTY_CELL,
            VTK_VERTEX,
            VTK_LINE,
            VTK_TRIANGLE,
            VTK_TETRA,
            VTK_CONVEX_POINT_SET, // 5
            VTK_CONVEX_POINT_SET, // 6
            VTK_CONVEX_POINT_SET, // 7
            VTK_VOXEL,
            VTK_CONVEX_POINT_SET,
            VTK_CONVEX_POINT_SET,
            VTK_CONVEX_POINT_SET,
            VTK_CONVEX_POINT_SET,
            VTK_CONVEX_POINT_SET,
            VTK_CONVEX_POINT_SET,
            VTK_CONVEX_POINT_SET,
            VTK_CONVEX_POINT_SET,
            VTK_CONVEX_POINT_SET,
            VTK_CONVEX_POINT_SET,
            VTK_CONVEX_POINT_SET
        };

        std::vector<int> markedCellTypes( nOutCells );
        auto outTopology = vtkSmartPointer<vtkIdTypeArray>::New();
        outTopology->SetNumberOfValues( outTopologyDataSize );
        auto outTopologyData = (vtkIdType*) outTopology->GetVoidPointer(0);

        if(this->CellMode==0 || this->CellMode==1){
            for(size_t i=0, inTopoIndex=0, outTopoIndex=0, outCellIndex=0; i<nInCells; i++){
                const size_t nVertices = inTopologyData[ inTopoIndex ];
                if( markedCells[i] ){
                    markedCellTypes[ outCellIndex++ ] = types[nVertices];
                    outTopologyData[ outTopoIndex ] = nVertices;
                    for(size_t j=1; j<=nVertices; j++)
                        outTopologyData[ outTopoIndex+j ] = oIndex_to_mIndex_map[ inTopologyData[inTopoIndex+j] ];
                    outTopoIndex += nVertices+1;
                }

                inTopoIndex += nVertices+1;
            }
        } else {
            // Sub
            for(size_t i=0, inTopoIndex=0, outTopoIndex=0, outCellIndex=0; i<nInCells; i++){
                const size_t nVertices = inTopologyData[ inTopoIndex ];

                if( markedCells[i] ){
                    size_t mVertices = 0;
                    for(size_t j=1; j<=nVertices; j++){
                        const auto& mIndex = oIndex_to_mIndex_map[ inTopologyData[inTopoIndex+j] ];
                        if(mIndex>-1){
                            outTopologyData[ outTopoIndex + 1 + mVertices ] = mIndex;
                            mVertices++;
                        }
                    }
                    outTopologyData[ outTopoIndex ] = mVertices;
                    markedCellTypes[ outCellIndex++ ] = types[mVertices];

                    outTopoIndex += mVertices+1;
                }

                inTopoIndex += nVertices+1;
            }
        }

        auto cellArray = vtkSmartPointer<vtkCellArray>::New();
        cellArray->SetCells(nOutCells, outTopology);
        outputAsUG->SetCells(markedCellTypes.data(), cellArray);

        // Extract cell data
        {
            size_t nArrays = inputCD->GetNumberOfArrays();
            auto outputCD = outputAsUG->GetCellData();
            for(size_t arrayIndex=0; arrayIndex<nArrays; arrayIndex++){
                auto iArray = inputCD->GetAbstractArray( arrayIndex );
                auto oArray = vtkSmartPointer<vtkAbstractArray>::Take( iArray->NewInstance() );
                oArray->SetName( iArray->GetName() );
                oArray->SetNumberOfComponents( iArray->GetNumberOfComponents() );
                oArray->SetNumberOfTuples( nOutCells );

                switch( iArray->GetDataType() ){
                    vtkTemplateMacro({
                        auto iArrayData = (VTK_TT*) iArray->GetVoidPointer(0);
                        auto oArrayData = (VTK_TT*) oArray->GetVoidPointer(0);

                        for(size_t i=0, j=0; i<nInCells; i++){
                            if(markedCells[i])
                                oArrayData[j++] = iArrayData[ i ];
                        }
                    });
                }

                outputCD->AddArray( oArray );
            }
        }

        this->printMsg(
            "Extracting marked cells (#"+std::to_string(nOutCells)+")",
            1, t.getElapsedTime()
        );
    }

    this->printMsg( ttk::debug::Separator::L2 );
    this->printMsg( "Complete ("+std::to_string(nMarkedPoints)+" vertices, "+std::to_string(nOutCells)+" cells)", 1, globalTimer.getElapsedTime() );

    outputAsUG->GetFieldData()->ShallowCopy( inputAsUG->GetFieldData() );

    return 1;
}

template <class dataType>
int createUniqueValueArray(
    vtkDataArray* uniqueValueArray,
    vtkDataArray* valueArray
){
    std::set<dataType> uniqueValues;

    if(uniqueValueArray->GetDataType()!=valueArray->GetDataType())
        return 0;

    size_t nValues = valueArray->GetNumberOfTuples()*valueArray->GetNumberOfComponents();
    auto valueArrayData = (dataType*) valueArray->GetVoidPointer(0);
    for(size_t i=0; i<nValues; i++)
        uniqueValues.insert( valueArrayData[i] );

    size_t nUniqueValues = uniqueValues.size();

    uniqueValueArray->SetNumberOfComponents( 1 );
    uniqueValueArray->SetNumberOfTuples( nUniqueValues );

    auto uniqueValueArrayData = (dataType*) uniqueValueArray->GetVoidPointer(0);
    auto it = uniqueValues.begin();
    for(size_t i=0; i<nUniqueValues; i++){
        uniqueValueArrayData[i] = *it;
        it++;
    }

    return 1;
}

int ttkExtract::ExtractArrayValues(
    vtkDataObject* output,
    vtkDataObject* input,
    const std::vector<double>& indices
) {
    size_t nValues = indices.size();

    auto inputArray = this->GetInputArrayToProcess(0, input);
    if(!inputArray){
        this->printErr("Unable to retrieve input array.");
        return 0;
    }
    std::string inputArrayName = inputArray->GetName();

    output->ShallowCopy(input);

    if(this->ExtractUniqueValues){
        ttk::Timer t;
        this->printMsg(
            "Extracting unique values from '"+inputArrayName+"'",
            0,
            ttk::debug::LineMode::REPLACE
        );

        auto uniqueValueArray = vtkSmartPointer<vtkDataArray>::Take( inputArray->NewInstance() );
        uniqueValueArray->SetName( ("Unique"+std::string(inputArrayName.data())).data() );

        int status=0;
        switch(inputArray->GetDataType()){
            vtkTemplateMacro(
                status = createUniqueValueArray<VTK_TT>(
                    uniqueValueArray,
                    inputArray
                )
            );
        }
        if(!status){
            this->printErr("Unable to compute unique values.");
            return 0;
        }

        output->GetFieldData()->AddArray(uniqueValueArray);

        this->printMsg(
            "Extracting unique values from '"+inputArrayName+"'",
            1,
            t.getElapsedTime()
        );
    } else {
        ttk::Timer t;
        std::string indicesString = "";
        doubleVectorToString(indicesString, indices);

        this->printMsg(
            "Extracting values at ["+indicesString+"] from '"+inputArrayName+"'",
            0,
            ttk::debug::LineMode::REPLACE
        );

        auto outputArray = vtkSmartPointer<vtkDataArray>::Take( inputArray->NewInstance() );
        outputArray->SetName( ("Extracted"+std::string(inputArray->GetName())).data() );
        outputArray->SetNumberOfComponents( inputArray->GetNumberOfComponents() );
        outputArray->SetNumberOfTuples( indices.size() );

        for(size_t i=0; i<nValues; i++){
            size_t index = (size_t) indices[i];
            size_t inputArraySize = inputArray->GetNumberOfTuples();
            if(0<=index && index<inputArraySize){
                outputArray->SetTuple(i, index, inputArray);
            } else {
                this->printErr("Index out of range ("+std::to_string(i)+"/"+std::to_string(inputArraySize)+").");
                return 0;
            }
        }

        output->GetFieldData()->AddArray( outputArray );

        this->printMsg(
            "Extracting values at ["+indicesString+"] from '"+inputArrayName+"'",
            1,
            t.getElapsedTime()
        );
    }

    return 1;
}

// =============================================================================
// RequestData
// =============================================================================
int ttkExtract::RequestData(
    vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector
){
    // ttk::Timer globalTimer;

    // Get Input to Output
    auto input  = vtkDataObject::GetData( inputVector[0] );
    auto output = vtkDataObject::GetData( outputVector );

    // -------------------------------------------------------------------------
    // Replace Variables in ExpressionString (e.g. {time[2]})
    // -------------------------------------------------------------------------
    std::string finalExpressionString;
    {
        std::string errorMsg;
        if( !ttkUtils::replaceVariables( this->GetExpressionString(), input->GetFieldData(), finalExpressionString, errorMsg ) ){
            this->printErr(errorMsg);
            return 0;
        }
    }

    std::vector<std::string> valuesAsStrings;
    ttkUtils::stringListToVector( finalExpressionString, valuesAsStrings );

    const size_t nValues = valuesAsStrings.size();
    std::vector<double> valuesAsD( nValues );

    for(size_t i=0; i<nValues; i++){
        try {
            double value = stod( valuesAsStrings[i] );
            valuesAsD[i] = value;
        } catch(std::invalid_argument& e){
            this->printErr("Unable to convert element '"+valuesAsStrings[i]+"' of input std::string '"+finalExpressionString+"' to double.");
            return 0;
        }
    }

    auto mode = this->ExtractionMode;
    if(mode<0){
        if(input->IsA("vtkMultiBlockDataSet"))
            mode = 0;
        else if(input->IsA("vtkTable"))
            mode = 1;
        else {
            this->printErr("Unable to automatically determine extraction mode.");
            return 0;
        }
    }

    if(mode==0){
        if(!this->ExtractBlocks( output, input, valuesAsD ))
            return 0;
    } else if(mode==1){
        if(!this->ExtractRows( output, input, valuesAsD ))
            return 0;
    } else if(mode==2){
        if(!this->ExtractArrayValues( output, input, valuesAsD ))
            return 0;
    } else if(mode==3){
        if(!this->ExtractGeometry( output, input, valuesAsD ))
            return 0;
    }

    this->printMsg( ttk::debug::Separator::L1 );

    return 1;
}