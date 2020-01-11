#include <ttkTrackingFromLabels.h>

#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkUnstructuredGrid.h>
#include <vtkImageData.h>

#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkFloatArray.h>
#include <vtkLongLongArray.h>
#include <vtkCharArray.h>
#include <vtkIdTypeArray.h>
#include <vtkDoubleArray.h>
#include <vtkUnsignedCharArray.h>

#include <vtkCellArray.h>

#include <vtkInformationVector.h>

vtkStandardNewMacro(ttkTrackingFromLabels);

ttkTrackingFromLabels::ttkTrackingFromLabels() {
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

ttkTrackingFromLabels::~ttkTrackingFromLabels() {
}

int ttkTrackingFromLabels::FillInputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
  else
    return 0;

  return 1;
}

int ttkTrackingFromLabels::FillOutputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkUnstructuredGrid");
  else
    return 0;

  return 1;
}

// Function to fetch data of a specificd block
void unpackMB(vtkMultiBlockDataSet* mb, size_t time, size_t level, vtkImageData*& segmentation, vtkPointSet*& components){
    auto levelBlock = (vtkMultiBlockDataSet*) mb->GetBlock(level);
    auto segmentationsBlock = (vtkMultiBlockDataSet*) levelBlock->GetBlock(0);
    auto componentsBlock    = (vtkMultiBlockDataSet*) levelBlock->GetBlock(1);

    segmentation = vtkImageData::SafeDownCast( segmentationsBlock->GetBlock(time) );
    components   = vtkPointSet ::SafeDownCast( componentsBlock->GetBlock(time) );
};

// Function to compute number of levels and timesteps contained in a vtkMultiBlockDataSet
void getNumberOfLevelsAndTimesteps(vtkMultiBlockDataSet* mb, size_t& nL, size_t& nT){
    nL = mb->GetNumberOfBlocks();
    nT = (
        (vtkMultiBlockDataSet*)
        (
            (vtkMultiBlockDataSet*) mb->GetBlock(0)
        )->GetBlock(0)
    )->GetNumberOfBlocks();
};

// =============================================================================
// Finalize
// =============================================================================
template<typename labelType> int finalize(
    std::vector<std::vector<std::vector<ttk::TrackingFromLabels::Node>>>& levelTimeNodesMap,
    std::vector<std::vector<std::vector<ttk::TrackingFromLabels::Edge>>>& levelTimeEdgesTMap,
    std::vector<std::vector<std::vector<ttk::TrackingFromLabels::Edge>>>& timeLevelEdgesNMap,

    std::string labelArrayName,
    int labelTypeId,

    vtkDataObject* trackingGraphObject
){
    auto trackingGraph = vtkUnstructuredGrid::SafeDownCast( trackingGraphObject );

    size_t nL = levelTimeNodesMap.size();
    size_t nT = levelTimeNodesMap[0].size();

    auto prepArray = [](vtkAbstractArray* array, std::string name, size_t nComponents, size_t nValues){
        array->SetName(name.data());
        array->SetNumberOfComponents( nComponents );
        array->SetNumberOfTuples( nValues );
    };

    // Add Points
    {
        size_t nNodes = 0;
        for(size_t t=0; t<nT; t++)
            for(size_t l=0; l<nL; l++)
                nNodes += levelTimeNodesMap[l][t].size();

        auto points = vtkSmartPointer<vtkPoints>::New();
        points->SetNumberOfPoints( nNodes );
        auto pointCoords = (float*) points->GetVoidPointer(0);

        auto sequence = vtkSmartPointer<vtkLongLongArray>::New();
        prepArray(sequence, "SequenceIndex", 1, nNodes);
        auto sequenceData = (long long*) sequence->GetVoidPointer(0);

        auto level = vtkSmartPointer<vtkLongLongArray>::New();
        prepArray(level, "LevelIndex", 1, nNodes);
        auto levelData = (long long*) level->GetVoidPointer(0);

        auto size = vtkSmartPointer<vtkFloatArray>::New();
        prepArray(size, "Size", 1, nNodes);
        auto sizeData = (float*) size->GetVoidPointer(0);

        auto type = vtkSmartPointer<vtkUnsignedCharArray>::New();
        prepArray(type, "Type", 1, nNodes);
        auto typeData = (unsigned char*) type->GetVoidPointer(0);

        auto branch = vtkSmartPointer<vtkLongLongArray>::New();
        prepArray(branch, "BranchId", 1, nNodes);
        auto branchData = (long long*) branch->GetVoidPointer(0);

        // auto label = vtkSmartPointer<vtkDataArray>::Take(
        //     vtkDataArray::CreateDataArray( labelTypeId )
        // );
        auto label = vtkSmartPointer<vtkLongLongArray>::New();
        prepArray(label, labelArrayName, 1, nNodes);
        auto labelData = (long long*) label->GetVoidPointer(0);

        {
            size_t coordIndex=0, dataIndex=0;
            for(size_t t=0; t<nT; t++){
                for(size_t l=0 ; l<nL; l++){
                    for(const auto& node: levelTimeNodesMap[l][t]){
                        std::copy( node.center, node.center+3, pointCoords+coordIndex);
                        coordIndex+=3;

                        sequenceData[dataIndex] = t;
                        levelData   [dataIndex] = l;
                        sizeData    [dataIndex] = node.size;
                        typeData    [dataIndex] = node.type;
                        branchData  [dataIndex] = node.branchId;
                        labelData   [dataIndex] = node.label;

                        dataIndex++;
                    }
                }
            }
        }

        trackingGraph->SetPoints(points);

        auto pointData = trackingGraph->GetPointData();
        pointData->AddArray( sequence );
        pointData->AddArray( level );
        pointData->AddArray( size );
        pointData->AddArray( type );
        pointData->AddArray( branch );
        pointData->AddArray( label );
    }

    // Add Cells
    {
        // Build node index offset vector
        std::vector<size_t> timeLevelOffsetMap(nT*nL+1);
        {
            timeLevelOffsetMap[0] = 0;
            size_t q = 1;
            for(size_t t=0; t<nT; t++)
                for(size_t l=0; l<nL; l++){
                    timeLevelOffsetMap[q] = timeLevelOffsetMap[q-1] + levelTimeNodesMap[l][t].size();
                    q++;
                }
        }

        size_t nEdgesT = 0;
        if(nT>1)
            for(size_t t=0; t<nT-1; t++)
                for(size_t l=0; l<nL; l++)
                    nEdgesT += levelTimeEdgesTMap[l][t].size();

        size_t nEdgesN = 0;
        // if(nL>1)
        //     for(size_t l=0; l<nL-1; l++)
        //         for(size_t t=0; t<nT; t++)
        //             nEdgesN += timeLevelEdgesNMap[t][l].size();

        auto cells = vtkSmartPointer<vtkIdTypeArray>::New();
        cells->SetNumberOfValues( 3*nEdgesT + 3*nEdgesN );
        auto cellIds = (vtkIdType*) cells->GetVoidPointer(0);

        auto overlap = vtkSmartPointer<vtkFloatArray>::New();
        prepArray(overlap, "Overlap", 1, nEdgesT+nEdgesN);
        auto overlapData = (float*) overlap->GetVoidPointer(0);

        auto branch = vtkSmartPointer<vtkLongLongArray>::New();
        prepArray(branch, "BranchId", 1, nEdgesT+nEdgesN);
        auto branchData = (long long*) branch->GetVoidPointer(0);

        // Tracking graphs
        {
            size_t topoIndex=0, dataIndex=0;
            if(nT>1){
                for(size_t t=1; t<nT; t++){
                    for(size_t l=0; l<nL; l++){
                        size_t offsetIndex0 = (t-1)*nL+l;
                        size_t offsetIndex1 = (t  )*nL+l;

                        const auto& edges = levelTimeEdgesTMap[l][t-1];
                        for(size_t i=0, j=edges.size(); i<j; i++){
                            const auto& edge = edges[i];
                            cellIds[topoIndex++] = 2;
                            cellIds[topoIndex++] = (vtkIdType) (timeLevelOffsetMap[ offsetIndex0 ] + edge.n0);
                            cellIds[topoIndex++] = (vtkIdType) (timeLevelOffsetMap[ offsetIndex1 ] + edge.n1);
                            overlapData[dataIndex] = edge.overlap;
                            branchData [dataIndex] = edge.branchId;
                            dataIndex++;
                        }
                    }
                }
            }
        }

        // // Nesting trees
        // if(nL>1)
        //     for(size_t l=1; l<nL; l++){
        //         for(size_t t=0; t<nT; t++){
        //             auto& edges = timeLevelEdgesNMap[t][l-1];
        //             size_t temp = t*nL;
        //             for(size_t i=0, j=edges.size(); i<j; ){
        //                 cellIds[q0++] = 2;
        //                 cellIds[q0++] = (vtkIdType) (timeLevelOffsetMap[ temp+(l-1) ] + edges[i++]);
        //                 cellIds[q0++] = (vtkIdType) (timeLevelOffsetMap[ temp+(l  ) ] + edges[i++]);
        //                 typeData[q1] = 1;
        //                 overlapData[q1] = edges[i++];
        //                 branchData[q1] = edges[i++];
        //                 q1++;
        //             }
        //         }
        //     }

        auto cellArray = vtkSmartPointer<vtkCellArray>::New();
        cellArray->SetCells(nEdgesT + nEdgesN, cells);
        trackingGraph->SetCells(VTK_LINE, cellArray);

        auto cellData = trackingGraph->GetCellData();
        cellData->AddArray( overlap );
        cellData->AddArray( branch );
    }

    return 1;
}

// =============================================================================
// Mesh Nested Tracking Graph
// =============================================================================
int ttkTrackingFromLabels::meshNestedTrackingGraph(vtkDataObject* trackingGraph){
    ttk::Timer t;

    dMsg(cout, "[ttkTrackingFromLabels] =======================================================\n" , infoMsg);
    dMsg(cout, "[ttkTrackingFromLabels] Meshing nested tracking graph\n" , infoMsg);

    switch( this->LabelDataType ){
        vtkTemplateMacro({
            finalize<VTK_TT>(
                this->levelTimeNodesMap,
                this->levelTimeEdgesTMap,
                this->timeLevelEdgesNMap,
                this->LabelArrayName,
                this->LabelDataType,
                trackingGraph
            );
        });
    }

    std::stringstream msg;
    msg << "[ttkTrackingFromLabels] -------------------------------------------------------"<<endl
        << "[ttkTrackingFromLabels] Nested tracking graph meshed in " << t.getElapsedTime() << " s. (" << threadNumber_ << " thread(s))." << endl;
    dMsg(cout, msg.str(), timeMsg);

    return 1;
}

// =============================================================================
// Reset
// =============================================================================
int ttkTrackingFromLabels::reset(){
    this->LabelDataType = -1;

    this->levelTimeNodesMap  = std::vector<std::vector<std::vector<ttk::TrackingFromLabels::Node>>>();
    this->levelTimeEdgesTMap = std::vector<std::vector<std::vector<ttk::TrackingFromLabels::Edge>>>();
    this->timeLevelEdgesNMap = std::vector<std::vector<std::vector<ttk::TrackingFromLabels::Edge>>>();

    this->previousIterationData = vtkSmartPointer<vtkMultiBlockDataSet>::New();

    return 1;
}

// =============================================================================
// Pack Data
// =============================================================================
int ttkTrackingFromLabels::packInputData(vtkDataObject* inputDataObject, vtkMultiBlockDataSet* packedInput) const {
    /* Enforce following vtkMultiBlockDataSet structure:
        {
            level0: {
                segmentations: {vtkImageData0,vtkImageData1,...,vtkImageDataT},
                components:    {vtkPointSet0, vtkPointSet1, ...,vtkPointSetT}
            },
            ...
            levelL: {
                segmentations: {vtkImageData0,vtkImageData1,...,vtkImageDataT},
                components:    {vtkPointSet0, vtkPointSet1, ...,vtkPointSetT}
            },
        }
    */

    // Check inputDataObject depth: 2->(level->time), 1->(-,time), 0->(-,-)
    size_t depth = 0;
    {
        auto cObject = inputDataObject;
        auto cObjectAsMB = vtkMultiBlockDataSet::SafeDownCast( cObject );
        while( cObjectAsMB ){
            cObject = cObjectAsMB->GetBlock(0);
            cObjectAsMB = vtkMultiBlockDataSet::SafeDownCast( cObject );
            depth++;
        }
    }

    if(depth==1){ // {vtkImageData,vtkPointSet}
        auto inputDataObjectAsMB = vtkMultiBlockDataSet::SafeDownCast( inputDataObject );

        auto level = vtkSmartPointer<vtkMultiBlockDataSet>::New();
        auto segmentations = vtkSmartPointer<vtkMultiBlockDataSet>::New();
        auto components = vtkSmartPointer<vtkMultiBlockDataSet>::New();
        segmentations->SetBlock( 0, inputDataObjectAsMB->GetBlock(0) );
        components->SetBlock( 0, inputDataObjectAsMB->GetBlock(1) );
        level->SetBlock(0, segmentations);
        level->SetBlock(1, components);
        packedInput->SetBlock( 0, level );
    } else if(depth==2) {
        packedInput->SetBlock( 0, inputDataObject);
    } else if(depth==3) {
        packedInput->ShallowCopy( inputDataObject );
    } else {
        dMsg(cout, "[ttkTrackingFromLabels] ERROR: Unable to convert input into '{vtkImageData,vtkPointSet}' sequence.\n", fatalMsg);
        return 0;
    }

    return 1;
}

// =============================================================================
// Check Data
// =============================================================================
int ttkTrackingFromLabels::checkData(vtkMultiBlockDataSet* data){
    size_t nL = data->GetNumberOfBlocks();
    size_t nT = 0;

    if(nL<1){
        dMsg(cout, "[ttkTrackingFromLabels] ERROR: Input must have at least one '{vtkImageData,vtkPointSet}' pair.\n", fatalMsg);
        return 0;
    }

    for(size_t l=0; l<nL; l++){
        auto level = vtkMultiBlockDataSet::SafeDownCast( data->GetBlock(l) );
        if(level->GetNumberOfBlocks()!=2){
            this->printErr("Each level must contain a sequence of 'vtkImageData' and 'vtkPointSet' objects of equal length.");
            return 0;
        }

        auto segmentations = vtkMultiBlockDataSet::SafeDownCast( level->GetBlock(0) );
        auto components    = vtkMultiBlockDataSet::SafeDownCast( level->GetBlock(1) );

        if(!segmentations || !components || segmentations->GetNumberOfBlocks()!=components->GetNumberOfBlocks()){
            this->printErr("Each level must contain a sequence of 'vtkImageData' and 'vtkPointSet' objects of equal length.");
            return 0;
        }

        size_t n = segmentations->GetNumberOfBlocks();
        if(nT==0) nT = n;
        if(nT!=n) {
            this->printErr("Each level must contain a sequence of 'vtkImageData' and 'vtkPointSet' objects of equal length.");
            return 0;
        }

        for(size_t t=0; t<nT; t++){
            auto segmentation = vtkImageData::SafeDownCast( segmentations->GetBlock(t) );
            auto component    = vtkPointSet ::SafeDownCast( components->GetBlock(t) );

            if( !segmentation || !component ){
                dMsg(cout, "[ttkTrackingFromLabels] ERROR: Input must consist of '{vtkImageData,vtkPointSet}' pairs.\n", fatalMsg);
                return 0;
            }

            auto labels = this->GetInputArrayToProcess(0, segmentation);
            this->LabelArrayName = labels->GetName();
            if( !labels ){
                dMsg(cout, "[ttkTrackingFromLabels] ERROR: Unable to retrieve label array.\n" , fatalMsg);
                return 0;
            }

            int labelDataType = labels->GetDataType();
            if(this->LabelDataType<0) this->LabelDataType = labelDataType;
            if(this->LabelDataType != labelDataType){
                dMsg(cout, "[ttkTrackingFromLabels] ERROR: Segmentation labels do not have same type across sequences.\n" , fatalMsg);
                return 0;
            }
        }
    }

    return 1;
}

// =============================================================================
// Pack Streamed Data
// =============================================================================
int ttkTrackingFromLabels::packStreamedData(vtkMultiBlockDataSet* streamedData, vtkMultiBlockDataSet* packedStreamedData) const {
    size_t nL_PI = this->previousIterationData->GetNumberOfBlocks();
    size_t nL_CI = streamedData->GetNumberOfBlocks();
    if( nL_PI!=nL_CI ){
        dMsg(cout, "[ttkTrackingFromLabels] ERROR: Number of levels differ over time.\n" , fatalMsg);
        return 0;
    }
    for(size_t l=0; l<nL_PI; l++){
        auto pLevel = (vtkMultiBlockDataSet*) this->previousIterationData->GetBlock(l);
        auto sLevel = (vtkMultiBlockDataSet*) streamedData->GetBlock(l);

        auto pSegmentations = (vtkMultiBlockDataSet*) pLevel->GetBlock(0);
        auto pComponents    = (vtkMultiBlockDataSet*) pLevel->GetBlock(1);

        auto sSegmentations = (vtkMultiBlockDataSet*) sLevel->GetBlock(0);
        auto sComponents    = (vtkMultiBlockDataSet*) sLevel->GetBlock(1);

        auto sT = sSegmentations->GetNumberOfBlocks();

        auto packedLevel = vtkSmartPointer<vtkMultiBlockDataSet>::New();
        auto packedSegmentations = vtkSmartPointer<vtkMultiBlockDataSet>::New();
        auto packedComponents = vtkSmartPointer<vtkMultiBlockDataSet>::New();

        packedSegmentations->SetBlock(0, pSegmentations->GetBlock(0));
        packedComponents->SetBlock(0, pComponents->GetBlock(0));

        for(size_t t=0; t<sT; t++){
            packedSegmentations->SetBlock(t+1, sSegmentations->GetBlock(t));
            packedComponents->SetBlock(t+1, sComponents->GetBlock(t));
        }

        packedLevel->SetBlock(0, packedSegmentations);
        packedLevel->SetBlock(1, packedComponents);
        packedStreamedData->SetBlock(l, packedLevel);
    }

    return 1;
}

// =============================================================================
// Store Streamed Data
// =============================================================================
int ttkTrackingFromLabels::storeStreamedData(vtkMultiBlockDataSet* streamedData){
    auto temp = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    size_t nL = streamedData->GetNumberOfBlocks();
    for(size_t l=0; l<nL; l++){
        auto timestepsSD = vtkMultiBlockDataSet::SafeDownCast( streamedData->GetBlock(l) );
        size_t nT = timestepsSD->GetNumberOfBlocks();

        vtkSmartPointer<vtkMultiBlockDataSet> lastTimestep = vtkSmartPointer<vtkMultiBlockDataSet>::New();
        lastTimestep->SetBlock(0, timestepsSD->GetBlock(nT-1));

        temp->SetBlock(l, lastTimestep);
    }

    this->previousIterationData->DeepCopy( temp );

    return 1;
}

// =============================================================================
// Compute Nodes
// =============================================================================
int ttkTrackingFromLabels::computeNodes(vtkMultiBlockDataSet* data){

    ttk::Timer timer;

    size_t nL, nT;
    getNumberOfLevelsAndTimesteps(data, nL, nT);

    // Reusable variables
    vtkImageData* segmentation=nullptr;
    vtkPointSet*  components=nullptr;

    // Compute Nodes
    {
        dMsg(cout, "[ttkTrackingFromLabels] =======================================================\n" , infoMsg);
        dMsg(cout, "[ttkTrackingFromLabels] Computing nodes\n" , infoMsg);

        if(this->levelTimeNodesMap.size()!=nL)
            this->levelTimeNodesMap.resize(nL);

        for(size_t l=0; l<nL; l++){
            {
                std::stringstream msg;
                msg << "[ttkTrackingFromLabels] -------------------------------------------------------" << endl
                    << "[ttkTrackingFromLabels] Level Index: " << l << endl;
                dMsg(cout, msg.str(), infoMsg);
            }

            auto& timeNodesMap = this->levelTimeNodesMap[ l ];
            size_t timeOffset = timeNodesMap.size();
            timeNodesMap.resize( timeOffset+nT );

            for(size_t t=0; t<nT; t++){
                unpackMB(data, t, l, segmentation, components);

                // Copy components into nodes
                auto& nodes = timeNodesMap[ timeOffset + t ];
                size_t nComponents = components->GetNumberOfPoints();
                auto componentCenters = (float*) components->GetPoints()->GetVoidPointer(0);
                nodes.resize( nComponents );

                for(size_t i=0,j=0; i<nComponents; i++,j+=3){
                    auto& node = nodes[i];
                    node.label = i;
                    std::copy(componentCenters+j, componentCenters+j+3, node.center);
                }

                // Copy component size if exists
                auto componentSizeArray = components->GetPointData()->GetAbstractArray( "ComponentSize" );
                if(componentSizeArray){
                    switch( componentSizeArray->GetDataType() ){
                        vtkTemplateMacro({
                            auto componentSizeArrayData = (VTK_TT*) componentSizeArray->GetVoidPointer(0);
                            for(size_t i=0; i<nComponents; i++)
                                nodes[i].size = componentSizeArrayData[i];
                        });
                    }
                }
            }
        }

        {
            std::stringstream msg;
            msg << "[ttkTrackingFromLabels] -------------------------------------------------------"<<endl
                << "[ttkTrackingFromLabels] Nodes computed in " << timer.getElapsedTime() << " s." << endl;
            dMsg(cout, msg.str(), timeMsg);
        }

    }

    return 1;
}

// =============================================================================
// Compute Tracking Graphs
// =============================================================================
int ttkTrackingFromLabels::computeTrackingGraphs(vtkMultiBlockDataSet* data){

    ttk::Timer timer;

    size_t nL, nT;
    getNumberOfLevelsAndTimesteps(data, nL, nT);

    if(nT<2) return 1;

    // Reusable variables
    vtkImageData*     segmentation0=nullptr;
    vtkImageData*     segmentation1=nullptr;
    vtkPointSet*      components0  =nullptr;
    vtkPointSet*      components1  =nullptr;
    vtkAbstractArray* labels0      =nullptr;
    vtkAbstractArray* labels1      =nullptr;

    dMsg(cout, "[ttkTrackingFromLabels] =======================================================\n" , infoMsg);
    dMsg(cout, "[ttkTrackingFromLabels] Computing tracking graphs\n" , infoMsg);

    if(this->levelTimeEdgesTMap.size()!=nL)
        this->levelTimeEdgesTMap.resize(nL);

    for(size_t l=0; l<nL; l++){
        {
            std::stringstream msg;
            msg << "[ttkTrackingFromLabels] -------------------------------------------------------" << endl
                << "[ttkTrackingFromLabels] Level Index: " << l << endl;
            dMsg(cout, msg.str(), infoMsg);
        }

        auto& timeEdgesTMap = this->levelTimeEdgesTMap[ l ];
        auto& timeNodesMap  = this->levelTimeNodesMap [ l ];
        size_t timeOffset = timeEdgesTMap.size();
        timeEdgesTMap.resize( timeOffset+nT-1 );

        for(size_t t=1; t<nT; t++){
            unpackMB(data, t-1, l, segmentation0, components0);
            labels0 = this->GetInputArrayToProcess(0, segmentation0);

            unpackMB(data, t  , l, segmentation1, components1);
            labels1 = this->GetInputArrayToProcess(0, segmentation1);

            auto dim0 = segmentation0->GetDimensions();
            auto dim1 = segmentation1->GetDimensions();
            if(dim0[0]!=dim1[0] || dim0[1]!=dim1[1] || dim0[2]!=dim1[2]){
                dMsg(cout, "[ttkTrackingFromLabels] ERROR: Segmentations do not have the same dimensions.\n");
                return 0;
            }
            size_t nPoints = segmentation0->GetNumberOfPoints();

            switch( this->LabelDataType ){
                vtkTemplateMacro({
                    int status = TrackingFromLabels::ComputeOverlap<VTK_TT>(
                        (VTK_TT*) labels0->GetVoidPointer(0),
                        (VTK_TT*) labels1->GetVoidPointer(0),
                        nPoints,
                        timeNodesMap [ timeOffset+t-1 ],
                        timeEdgesTMap[ timeOffset+t-1 ]
                    );
                    if(status!=1){
                        return 0;
                    }
                });
            }
        }
    }

    {
        std::stringstream msg;
        msg << "[ttkTrackingFromLabels] -------------------------------------------------------"<<endl
            << "[ttkTrackingFromLabels] Tracking graphs computed in " << timer.getElapsedTime() << " s. (" << threadNumber_ << " thread(s))." << endl;
        dMsg(cout, msg.str(), timeMsg);
    }
    return 1;
}

// =============================================================================
// Compute Nesting Trees
// =============================================================================
int ttkTrackingFromLabels::computeNestingTrees(vtkMultiBlockDataSet* data){

    // Timer timer;

    // size_t nL, nT;
    // getNumberOfLevelsAndTimesteps(data, nL, nT);

    // if(nL<2) return 1;

    // // Reusable variables
    // vtkImageData* pointSet0=nullptr;
    // vtkImageData* pointSet1=nullptr;
    // vtkAbstractArray* labels0=nullptr;
    // vtkAbstractArray* labels1=nullptr;

    // dMsg(cout, "[ttkTrackingFromLabels] =======================================================\n" , infoMsg);
    // dMsg(cout, "[ttkTrackingFromLabels] Computing nesting trees\n" , infoMsg);

    // size_t timeOffset = this->timeLevelEdgesNMap.size();
    // this->timeLevelEdgesNMap.resize( timeOffset+nT );

    // for(size_t t=0; t<nT; t++){
    //     {
    //         std::stringstream msg;
    //         msg << "[ttkTrackingFromLabels] -------------------------------------------------------" << endl
    //             << "[ttkTrackingFromLabels] Time Index: " << t << endl;
    //         dMsg(cout, msg.str(), infoMsg);
    //     }

    //     auto& levelEdgesNMap = this->timeLevelEdgesNMap[ timeOffset+t ];
    //     levelEdgesNMap.resize(nL-1);

    //     for(size_t l=1; l<nL; l++){
    //         getData(data, t, l-1, this->GetLabelArrayName(), pointSet0, labels0);
    //         getData(data, t, l  , this->GetLabelArrayName(), pointSet1, labels1);

    //         size_t nPoints0 = pointSet0->GetNumberOfPoints();
    //         size_t nPoints1 = pointSet1->GetNumberOfPoints();
    //         if(nPoints0<1 || nPoints1<1) continue;

    //         switch( this->LabelDataType ){
    //             vtkTemplateMacro({
    //                 // this->trackingFromLabels.computeOverlap<VTK_TT>(
    //                 //     (float*)  pointSet0->GetPoints()->GetVoidPointer(0),
    //                 //     (float*)  pointSet1->GetPoints()->GetVoidPointer(0),
    //                 //     (VTK_TT*) labels0->GetVoidPointer(0),
    //                 //     (VTK_TT*) labels1->GetVoidPointer(0),
    //                 //     nPoints0,
    //                 //     nPoints1,

    //                 //     levelEdgesNMap[ l-1 ]
    //                 // );
    //             });
    //         }
    //     }
    // }

    // {
    //     std::stringstream msg;
    //     msg << "[ttkTrackingFromLabels] -------------------------------------------------------"<<endl
    //         << "[ttkTrackingFromLabels] Nesting trees computed in " << timer.getElapsedTime() << " s. (" << threadNumber_ << " thread(s))." << endl;
    //     dMsg(cout, msg.str(), timeMsg);
    // }

    return 1;
}

// =============================================================================
// Compute Branches
// =============================================================================
int ttkTrackingFromLabels::computeBranches(){

    size_t nL = this->levelTimeEdgesTMap.size();

    for(size_t l=0; l<nL; l++)
        ttk::TrackingFromLabels::ComputeBranchDecomposition(
            this->levelTimeNodesMap[l],
            this->levelTimeEdgesTMap[l]
        );

    return 1;
}

// =============================================================================
// Request Data
// =============================================================================
int ttkTrackingFromLabels::RequestData(
    vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector
){
    ttk::Timer timer;

    // Print Status
    {
        std::stringstream msg;
        msg << "================================================================================" << endl
            << "[ttkTrackingFromLabels] RequestData" << endl;
        dMsg(cout, msg.str(), infoMsg);
    }

    // Get Input Object
    vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
    auto inputObject = inInfo->Get(vtkDataObject::DATA_OBJECT());

    // Get iteration information
    auto iterationInformation = vtkDoubleArray::SafeDownCast( inputObject->GetFieldData()->GetAbstractArray("_ttk_IterationInfo") );

    bool useStreamingOverTime = iterationInformation!=nullptr;

    double iteration = 0;
    double nIterations = 0;
    if(useStreamingOverTime){
        iteration = iterationInformation->GetValue(0);
        nIterations = iterationInformation->GetValue(1);
    }

    // On first iteration reset
    if(!useStreamingOverTime || iteration==0)
        this->reset();

    // -------------------------------------------------------------------------
    // Prepare Input
    // -------------------------------------------------------------------------
    auto packedInput = vtkSmartPointer<vtkMultiBlockDataSet>::New();

    if( !this->packInputData(inputObject, packedInput) )
        return 0;

    // Check input integrity
    if( !this->checkData(packedInput) )
        return 0;

    // During streaming append data to previous timestep; Otherwise pass through
    auto packedStreamedData = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    if( useStreamingOverTime && iteration>0 ){
        if( !this->packStreamedData(packedInput, packedStreamedData) )
            return 0;
    } else packedStreamedData->ShallowCopy(packedInput);

    // -------------------------------------------------------------------------
    // Process Input
    // -------------------------------------------------------------------------
    // Compute nodes only for current input (previous iterations have already been processed)
    if( !this->computeNodes(packedInput) )
        return 0;

    this->UpdateProgress(0.3);

    // Compute tracking graphs
    if( !this->computeTrackingGraphs(packedStreamedData) )
        return 0;

    this->UpdateProgress(0.8);
    // // Compute nesting trees
    // if( !this->computeNestingTrees(packedInput) )
    //     return 0;

    // Store last timestep for next iteration
    if( useStreamingOverTime && !this->storeStreamedData(packedInput) )
        return 0;

    // -------------------------------------------------------------------------
    // Generate Output
    // -------------------------------------------------------------------------
    if( !useStreamingOverTime || iteration==nIterations-1 ){
        // Get Output
        vtkInformation* outInfo = outputVector->GetInformationObject(0);
        auto trackingGraph = outInfo->Get(vtkDataObject::DATA_OBJECT());

        // Compute Branches
        if( !this->computeBranches() )
            return 0;

        // Mesh Graph
        if( !this->meshNestedTrackingGraph(trackingGraph) )
            return 0;

        this->UpdateProgress(1);
    }

    // -------------------------------------------------------------------------
    // Print total performance
    // -------------------------------------------------------------------------
    if(!useStreamingOverTime){
        std::stringstream msg;
        msg << "[ttkTrackingFromLabels] ======================================================="<<endl
            << "[ttkTrackingFromLabels] Nested tracking graph generated in " << timer.getElapsedTime() << " s. (" << threadNumber_ << " thread(s))." << endl;
        dMsg(cout, msg.str(), timeMsg);
    }

    return 1;
}