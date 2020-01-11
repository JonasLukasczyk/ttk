/// \ingroup base
/// \class ttk::TrackingFromLabels
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 01.09.2018
///
/// \brief TTK %trackingFromLabels processing package that tracks labled point sets.
///
/// %TrackingFromLabels is a TTK processing package that provides algorithms to track labled point sets across time (and optionally levels) based on spatial overlap, where two points overlap iff their corresponding coordinates are equal.
///
/// \b Related \b publication: \n
/// 'Nested Tracking Graphs'
/// Jonas Lukasczyk, Gunther Weber, Ross Maciejewski, Christoph Garth, and Heike Leitte.
/// Computer Graphics Forum (Special Issue, Proceedings Eurographics / IEEE Symposium on Visualization). Vol. 36. No. 3. 2017.
///

#pragma once

#include <Debug.h>

#include <unordered_map>

typedef ttk::SimplexId intTTK;

namespace ttk {
    class TrackingFromLabels : public virtual ttk::Debug {

        public:
            struct Node {
                intTTK        label;
                intTTK        size;
                float         center[3];
                unsigned char type;
                intTTK        branchId;
            };

            struct Edge {
                intTTK n0;
                intTTK n1;
                intTTK overlap;
                intTTK branchId;
            };

            TrackingFromLabels(){
                this->setDebugMsgPrefix("TrackingFromLabels");
            }
            ~TrackingFromLabels(){}

            // This function computes the overlap between two labeled segmentations
            template<class labelType> int ComputeOverlap(
                const labelType* segmentation0,
                const labelType* segmentation1,
                const size_t nPoints,

                std::vector<TrackingFromLabels::Node>& nodes,
                std::vector<TrackingFromLabels::Edge>& edges
            ) const;

            int ComputeBranchDecomposition(
                std::vector<std::vector<TrackingFromLabels::Node>>& timeNodesMap,
                std::vector<std::vector<TrackingFromLabels::Edge>>& timeEdgesMap
            ) const;
    };
}

// =============================================================================
// Track Components
// =============================================================================
template<typename labelType> int ttk::TrackingFromLabels::ComputeOverlap(
    const labelType* segmentation0,
    const labelType* segmentation1,
    const size_t nPoints,

    std::vector<TrackingFromLabels::Node>& nodes,
    std::vector<TrackingFromLabels::Edge>& edges
) const {
    // -------------------------------------------------------------------------
    // Track Nodes
    // -------------------------------------------------------------------------
    // dMsg(cout, "[ttkTrackingFromLabels] Tracking .............. ", timeMsg);
    Timer t;

    labelType backgroundLabel = -1;

    size_t nEdges = 0;
    std::unordered_map<size_t, std::unordered_map<size_t, size_t>> edgesMap;
    for(size_t i=0; i<nPoints; i++){
        const auto& l0 = segmentation0[i];
        const auto& l1 = segmentation1[i];
        if( l0!=backgroundLabel && l1!=backgroundLabel ){

            // Find edge and increase overlap counter
            auto edgesL0 = edgesMap.find( l0 ); // Edges from label0

            // If map does not exist then create it
            if(edgesL0 == edgesMap.end()){
                edgesMap[ l0 ] = std::unordered_map<size_t, size_t>();
                edgesL0 = edgesMap.find( l0 );
            }

            // Find edge label0 -> label1
            auto edge = edgesL0->second.find( l1 );

            // If edge does not exist then create it
            if(edge == edgesL0->second.end()){
                edgesL0->second[ l1 ] = 0;
                edge = edgesL0->second.find( l1 );
                nEdges++;
            }

            // Increase overlap
            edge->second++;
        }
    }

    // -------------------------------------------------------------------------
    // Pack Output
    // -------------------------------------------------------------------------
    {
        edges.resize( nEdges );
        size_t edgeIndex=0;
        for(const auto& it0: edgesMap){
            for(const auto& it1: it0.second){
                auto& edge = edges[edgeIndex++];
                edge.n0 = it0.first;
                edge.n1 = it1.first;
                edge.overlap = it1.second;
            }
        }
    }

    // Print Status
    {
        // stringstream msg;
        // msg << "done (#" << nEdges << " in " << t.getElapsedTime() <<" s)." <<endl;
        // dMsg(cout, msg.str(), timeMsg);
    }

    return 1;
}

int ttk::TrackingFromLabels::ComputeBranchDecomposition(
    std::vector<std::vector<TrackingFromLabels::Node>>& timeNodesMap,
    std::vector<std::vector<TrackingFromLabels::Edge>>& timeEdgesMap
) const {
    size_t nT = timeNodesMap.size();

    struct nodeData {
        intTTK maxPredId = -1;
        intTTK maxSuccId = -1;
        intTTK inDegree = 0;
        intTTK outDegree = 0;
    };

    std::vector<std::vector<nodeData>> timeNodeDataMap(nT);
    for(size_t t=0; t<nT; t++)
        timeNodeDataMap[t].resize( timeNodesMap[t].size() );

    // Compute max pred and succ
    for(size_t t=1; t<nT; t++){
        auto& nodes0 = timeNodesMap[t-1];
        auto& nodes1 = timeNodesMap[t];
        auto& nodes0Data = timeNodeDataMap[t-1];
        auto& nodes1Data = timeNodeDataMap[t];
        auto& edges  = timeEdgesMap[t-1];

        std::vector<intTTK> max0Overlap(nodes0.size(), 0);
        std::vector<intTTK> max1Overlap(nodes1.size(), 0);

        size_t nE = edges.size();

        for(size_t i=0; i<nE; i++){
            const auto& edge = edges[i];

            nodes0Data[ edge.n0 ].outDegree++;
            nodes1Data[ edge.n1 ].inDegree++;

            const intTTK& n0MaxSuccSize = max0Overlap[ edge.n0 ];
            const intTTK& n1MaxPredSize = max1Overlap[ edge.n1 ];

            if( n0MaxSuccSize < edge.overlap ){
                nodes0Data[ edge.n0 ].maxSuccId = edge.n1;
                max0Overlap[ edge.n0 ] = edge.overlap;
            }
            if( n1MaxPredSize < edge.overlap ){
                nodes1Data[ edge.n1 ].maxPredId = edge.n0;
                max1Overlap[ edge.n1 ] = edge.overlap;
            }
        }
    }

    // Determine type
    // 0: regular
    // 1: birth
    // 2: death
    // 3: saddle
    // 12: birth, death
    // 13: birth, saddle
    // 23: death, saddle
    for(size_t t=0; t<nT; t++){
        auto&       nodes     = timeNodesMap[t];
        const auto& nodesData = timeNodeDataMap[t];
        for(size_t i=0, j=nodes.size(); i<j; i++){
            const auto& data = nodesData[i];

            if(data.inDegree==0) {
                if(data.outDegree==0){
                    nodes[i].type = 12;
                } else if(data.outDegree==1) {
                    nodes[i].type = 1;
                } else {
                    nodes[i].type = 13;
                }
            } else if(data.inDegree==1){
                if(data.outDegree==0){
                    nodes[i].type = 2;
                } else if(data.outDegree==1) {
                    nodes[i].type = 0;
                } else {
                    nodes[i].type = 3;
                }
            } else {
                if(data.outDegree==0){
                    nodes[i].type = 23;
                } else if(data.outDegree==1) {
                    nodes[i].type = 3;
                } else {
                    nodes[i].type = 3;
                }
            }
        }
    }

    // Label first nodes of branches
    intTTK branchCounter = 0;

    for(size_t t=0; t<nT; t++){
        auto&       nodes     = timeNodesMap[t];
        const auto& nodesData = timeNodeDataMap[t];
        for(size_t i=0, j=nodes.size(); i<j; i++)
            nodes[i].branchId = nodesData[i].maxPredId==-1 ? branchCounter++ : -1;
    }

    for(size_t t=1; t<nT; t++){
        auto&       nodes1     = timeNodesMap[t];
        const auto& nodes0Data = timeNodeDataMap[t-1];
        const auto& nodes1Data = timeNodeDataMap[t];

        for(intTTK i=0,j=nodes1.size(); i<j; i++){
            auto& n1Data = nodes1Data[i];

            if( n1Data.maxPredId!=-1 && i!=nodes0Data[ n1Data.maxPredId ].maxSuccId )
                nodes1[i].branchId = branchCounter++;
        }
    }

    // Propagate branch labels
    for(size_t t=1; t<nT; t++){
        auto&       nodes0     = timeNodesMap[t-1];
        auto&       nodes1     = timeNodesMap[t];
        const auto& nodes1Data = timeNodeDataMap[t];
        auto&       edges      = timeEdgesMap[t-1];

        size_t nE = edges.size();

        for(size_t i=0; i<nE; i++){
            const auto& edge = edges[i];
            const auto& n0   = nodes0[ edge.n0 ];
            auto&       n1   = nodes1[ edge.n1 ];

            if(n1.branchId==-1 && edge.n0==nodes1Data[edge.n1].maxPredId)
                n1.branchId = n0.branchId;
        }
    }

    // Label edges
    for(size_t t=1; t<nT; t++){
        const auto& nodes0     = timeNodesMap[t-1];
        const auto& nodes0Data = timeNodeDataMap[t-1];
        const auto& nodes1     = timeNodesMap[t];
        auto&       edges      = timeEdgesMap[t-1];

        size_t nE = edges.size();

        for(size_t i=0; i<nE; i++){
            auto& edge = edges[i];
            auto& n0 = nodes0[ edge.n0 ];
            auto& n1 = nodes1[ edge.n1 ];

            edge.branchId = n0.branchId == n1.branchId
                ? n0.branchId
                : nodes0Data[edge.n0].maxSuccId == edge.n1
                    ? n0.branchId
                    : n1.branchId;
        }
    }

    return 1;
}