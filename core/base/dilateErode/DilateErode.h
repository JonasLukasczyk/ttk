/// \ingroup base
/// \class ttk::DilateErode
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 01.02.2019
///
/// \brief TTK %dilateErode processing package.
///
/// %DilateErode is a TTK processing package that TOOD

#pragma once

// base code includes
#include <Debug.h>
#include <Triangulation.h>

namespace ttk{

    class DilateErode : public virtual Debug {
        public:
            DilateErode(){
                this->setDebugMsgPrefix("DilateErode");
            }

            ~DilateErode(){
            }

            int preconditionTriangulation(ttk::AbstractTriangulation* triangulation) const {
              return triangulation->preconditionVertexNeighbors();
            };

            template <class dataType, class TriangulationType = ttk::AbstractTriangulation>
            int dilateErode(
                // Output
                dataType*           newLabels,

                // Input
                const int           mode,
                const dataType      value,
                TriangulationType* triangulation,
                const dataType*     oldLabels
            ) const;
    };
}

template <class dataType, class TriangulationType = ttk::AbstractTriangulation>
int ttk::DilateErode::dilateErode(
    // Output
    dataType*           newLabels,

    // Input
    const int           mode,
    const dataType      value,
    TriangulationType* triangulation,
    const dataType*     oldLabels
) const {

    Timer t;

    SimplexId nVertices = triangulation->getNumberOfVertices();

    std::string modeS = mode==0 ? "Dilating " : "Eroding";

    this->printMsg(modeS,0, debug::LineMode::REPLACE);

    if(mode==0){
        // Dilate
        #ifdef TTK_ENABLE_OPENMP
        #pragma omp parallel for num_threads(this->threadNumber_)
        #endif
        for(SimplexId i=0; i<nVertices; i++){
            // if current value is dilate value
            if( oldLabels[i]==value ){
                // assign current value to all neighbors
                const SimplexId nNeighbors = triangulation->getVertexNeighborNumber( i );
                SimplexId nIndex;
                for(SimplexId n=0; n<nNeighbors; n++){
                    triangulation->getVertexNeighbor(i, n, nIndex);
                    newLabels[nIndex] = value;
                }
            }
        }
    } else {
        // Erode
        #ifdef TTK_ENABLE_OPENMP
        #pragma omp parallel for num_threads(this->threadNumber_)
        #endif
        for(SimplexId i=0; i<nVertices; i++){
            const dataType& cValue = oldLabels[i];
            if( cValue==value ){
                const SimplexId nNeighbors = triangulation->getVertexNeighborNumber( i );
                SimplexId nIndex;
                dataType maxValue = value;
                for(SimplexId n=0; n<nNeighbors; n++){
                    triangulation->getVertexNeighbor(i, n, nIndex);
                    if(maxValue<oldLabels[nIndex])
                        maxValue = oldLabels[nIndex];
                }
                newLabels[i] = maxValue;
            }

            // // if current value is not erode value
            // const dataType& cValue = oldLabels[i];
            // if( cValue!=value ){
            //     // dilate current value to all neighbors
            //     const SimplexId nNeighbors = triangulation->getVertexNeighborNumber( i );
            //     SimplexId nIndex;
            //     for(SimplexId n=0; n<nNeighbors; n++){
            //         triangulation->getVertexNeighbor(i, n, nIndex);
            //         newLabels[nIndex] = cValue;
            //     }
            // }
        }
    }

    this->printMsg(modeS,1,t.getElapsedTime(),this->threadNumber_);

    return 1;
}