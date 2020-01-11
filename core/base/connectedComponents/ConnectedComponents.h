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
                idType* newLabels,
                std::vector<ttk::ConnectedComponents::Component>& components,
                const Triangulation* triangulation,
                const intTTK& firstVertexIndex
            ) const;

            template <class idType> int computeConnectedComponents(
                // Output
                idType*            newLabels,
                std::vector<ttk::ConnectedComponents::Component>& components,

                // Input
                const Triangulation*     triangulation,
                const idType*      oldLabels
            ) const;
    };
}

template <class idType>
int ttk::ConnectedComponents::floodFill(
    idType* newLabels,
    std::vector<ttk::ConnectedComponents::Component>& components,
    const Triangulation* triangulation,
    const intTTK& firstVertexIndex
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

    idType unlabeledLabel  = -2;

    while(stack.size()>0){
        cIndex = stack.back();
        stack.pop_back();

        if(newLabels[cIndex]==unlabeledLabel){
            newLabels[cIndex] = componentId;
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

template <class idType>
int ttk::ConnectedComponents::computeConnectedComponents(
    // Output
    idType* newLabels,
    std::vector<ttk::ConnectedComponents::Component>& components,

    // Input
    const Triangulation* triangulation,
    const idType* oldLabels
) const {
    Timer t;

    this->printMsg(debug::Separator::L1);
    this->printMsg("Computing Components",0,debug::LineMode::REPLACE);

    intTTK nVertices = triangulation->getNumberOfVertices();
    idType backgroundLabel = -1;
    idType unlabeledLabel  = -2;

    for(intTTK i=0; i<nVertices; i++)
        newLabels[i] = oldLabels[i] == backgroundLabel ? backgroundLabel : unlabeledLabel;

    for(intTTK i=0; i<nVertices; i++){
        if(newLabels[i]==unlabeledLabel){
            this->floodFill<idType>(
                newLabels,
                components,
                triangulation,
                i
            );
        }
    }

    this->printMsg("Computing Components",1,t.getElapsedTime());

    return 1;
}