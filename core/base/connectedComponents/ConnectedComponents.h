/// \ingroup base
/// \class ttk::ConnectedComponents
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 01.02.2019
///
/// \brief TTK %connectedComponents processing package.
///
/// %ConnectedComponents is a TTK processing package that TOOD

#pragma once

#include <Debug.h>
#include <Triangulation.h>

typedef ttk::SimplexId intTTK;

namespace ttk {
    class ConnectedComponents : virtual public Debug {
        public:
            struct Component {
                float center[3];
                long size;
            };

            ConnectedComponents() {
                this->setDebugMsgPrefix("ConnectedComponents");
            }
            ~ConnectedComponents(){};

            int preconditionTriangulation(ttk::Triangulation *triangulation) const {
              return triangulation->preconditionVertexNeighbors();
            };

            template <class idType> int floodFill(
                // Output
                idType* labels,
                std::vector<ttk::ConnectedComponents::Component>& components,

                // Input
                const Triangulation* triangulation,
                const intTTK& firstVertexIndex,
                const idType& unlabeledLabel
            ) const;

            template <class idType, class idType2> int computeConnectedComponents(
                // Output
                idType* labels,
                std::vector<ttk::ConnectedComponents::Component>& components,

                // Input
                const Triangulation* triangulation,
                const idType2* backgroundLabels=nullptr,
                const idType2& backgroundLabel=-1
            ) const;
    };
}

template <class idType>
int ttk::ConnectedComponents::floodFill(
    // Output
    idType* labels,
    std::vector<ttk::ConnectedComponents::Component>& components,

    // Input
    const Triangulation* triangulation,
    const intTTK& firstVertexIndex,
    const idType& unlabeledLabel
) const {
    idType componentId = components.size();
    components.resize( components.size()+1 );

    std::vector<intTTK> stack;
    stack.push_back( firstVertexIndex );
    intTTK cIndex;
    intTTK nIndex;
    intTTK size=0;
    float x,y,z;
    float center[3] = {0,0,0};

    while(stack.size()>0){
        cIndex = stack.back();
        stack.pop_back();

        if(labels[cIndex]==unlabeledLabel){
            labels[cIndex] = componentId;
            size++;
            triangulation->getVertexPoint( cIndex, x,y,z );
            center[0]+=x; center[1]+=y; center[2]+=z;

            size_t nNeighbors = triangulation->getVertexNeighborNumber( cIndex );
            stack.resize( stack.size() + nNeighbors );
            for(size_t i=0; i<nNeighbors; i++){
                triangulation->getVertexNeighbor(cIndex, i, nIndex);
                stack.push_back( nIndex );
            }
        }
    }
    center[0]/=size; center[1]/=size; center[2]/=size;

    auto& c = components[components.size()-1];
    std::copy( center, center+3, c.center );
    c.size = size;

    return 1;
}

template <class idType, class idType2>
int ttk::ConnectedComponents::computeConnectedComponents(
    // Output
    idType* labels,
    std::vector<ttk::ConnectedComponents::Component>& components,

    // Input
    const Triangulation* triangulation,
    const idType2* backgroundLabels,
    const idType2& backgroundLabel
) const {
    Timer t;

    this->printMsg(debug::Separator::L1);
    this->printMsg("Computing Components",0,debug::LineMode::REPLACE);

    intTTK nVertices = triangulation->getNumberOfVertices();
    idType unlabeledLabel = backgroundLabel-1;

    if(backgroundLabels==nullptr){
        for(intTTK i=0; i<nVertices; i++)
            labels[i] = unlabeledLabel;
    } else {
        for(intTTK i=0; i<nVertices; i++)
            labels[i] = backgroundLabels[i] == backgroundLabel ? backgroundLabel : unlabeledLabel;
    }

    for(intTTK i=0; i<nVertices; i++){
        if(labels[i]==unlabeledLabel){
            this->floodFill<idType>(
                labels,
                components,

                triangulation,
                i,
                unlabeledLabel
            );
        }
    }

    this->printMsg("Computing Components (#"+std::to_string(components.size())+")",1,t.getElapsedTime());

    return 1;
}