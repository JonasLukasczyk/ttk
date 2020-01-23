/// \ingroup base
/// \class ttk::PersistencePairInventory
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 1.09.2019
///
/// TODO

#pragma once

// ttk common includes
#include <Debug.h>

#include <limits.h>

namespace ttk {
    class PersistencePairInventory : virtual public Debug {
        public:
            PersistencePairInventory(){
                this->setDebugMsgPrefix("PPI");
            };
            ~PersistencePairInventory(){};

            template <class dataType> int ComputeScalarBounds(
                dataType scalarBounds[2],

                const std::vector<dataType*>& scalarsPerElement,
                const std::vector<size_t>& nEdgesPerElement
            ) const;


            template <class countType, class dataType, class idType> int ComputePersistenceCurves(
                countType* persistenceCurve,

                const dataType* persistenceThresholds,
                const size_t& nPersistenceThresholds,
                const std::vector<dataType*>& scalarsPerElement,
                const std::vector<idType*>& connectivityListPerElement,
                const std::vector<size_t>& nEdgesPerElement
            ) const;

            template <class binType, class dataType, class idType> int ComputePPI(
                binType* ppi,

                const size_t& nRows,
                const dataType scalarBounds[2],
                const std::vector<dataType*>& scalars,
                const dataType* persistenceThresholds,
                const size_t& nPersistenceThresholds,
                const std::vector<idType*>& connectivityLists,
                const std::vector<size_t>& nEdges
            ) const;

            template <class binType, class dataType, class idType> int ComputePPIColumn(
                binType* ppi,

                const size_t& nRows,
                const size_t& nCols,
                const size_t columnIndex,
                const dataType scalarBounds[2],
                const dataType* scalars,
                const dataType* persistenceThresholds,
                const size_t& nPersistenceThresholds,
                const idType* connectivityList,
                const size_t& nEdges
            ) const;

            template <class binType> int ComputeAPPI(
                binType* appi,

                const size_t& nRows,
                const size_t& nCols,
                const size_t& nPersistenceThresholds,
                const binType* ppi
            ) const;
    };
}

template <class dataType>
int ttk::PersistencePairInventory::ComputeScalarBounds(
    dataType scalarBounds[2],

    const std::vector<dataType*>& scalarsPerElement,
    const std::vector<size_t>& nEdgesPerElement
) const {
    ttk::Timer t;
    this->printMsg("Computing Scalar Bounds",0, ttk::debug::LineMode::REPLACE);

    size_t nElements = scalarsPerElement.size();

    scalarBounds[0] = std::numeric_limits<dataType>::max();
    scalarBounds[1] = std::numeric_limits<dataType>::min();

    for(size_t e=0; e<nElements; e++){
        dataType* scalars = scalarsPerElement[e];
        size_t nVertices = nEdgesPerElement[e]*2;

        for(size_t v=0; v<nVertices; v++){
            const dataType& scalar = scalars[v];
            if(scalarBounds[0]>scalar)
                scalarBounds[0]=scalar;
            if(scalarBounds[1]<scalar)
                scalarBounds[1]=scalar;
        }
    }

    this->printMsg(
        "Computing Scalar Bounds ["+std::to_string(scalarBounds[0])+","+std::to_string(scalarBounds[1])+"]",
        1, t.getElapsedTime()
    );

    return 1;
}

template <class countType, class dataType, class idType>
int ttk::PersistencePairInventory::ComputePersistenceCurves(
    countType* persistenceCurve,

    const dataType* persistenceThresholds,
    const size_t& nPersistenceThresholds,
    const std::vector<dataType*>& scalarsPerElement,
    const std::vector<idType*>& connectivityListPerElement,
    const std::vector<size_t>& nEdgesPerElement
) const {

    ttk::Timer t;
    {
        this->printMsg( ttk::debug::Separator::L1 );
        this->printMsg("Computing Persistence Curves");
        this->printMsg( ttk::debug::Separator::L2 );
        this->printMsg({
            {"#Threads", std::to_string(this->threadNumber_)},
            {"#Points", std::to_string(nPersistenceThresholds)},
        });
        this->printMsg( ttk::debug::Separator::L2 );
    }

    size_t nElements = scalarsPerElement.size();
    this->printMsg("Processing "+std::to_string(nElements)+" Elements",0,
        ttk::debug::LineMode::REPLACE);

    #ifdef TTK_ENABLE_OPENMP
    #pragma omp parallel for num_threads(threadNumber_)
    #endif
    for(size_t e=0; e<nElements; e++){
        dataType* scalars = scalarsPerElement[e];
        idType* connectivityList = connectivityListPerElement[e];
        size_t nEdges = nEdgesPerElement[e];

        size_t offset = e*nPersistenceThresholds;
        for(size_t p=0; p<nPersistenceThresholds; p++){
            size_t nPairsAboveThreshold = 0;

            const dataType& persistenceThreshold = persistenceThresholds[p];

            for(size_t i=0,j=1; i<nEdges; i++,j+=3){
                const idType& v0 = connectivityList[j];
                const idType& v1 = connectivityList[j+1];

                const dataType s0 = scalars[v0];
                const dataType s1 = scalars[v1];

                dataType persistence = s1>s0 ? s1-s0 : s0-s1;
                if(persistence>=persistenceThreshold)
                    nPairsAboveThreshold++;
            }

            persistenceCurve[offset++] = nPairsAboveThreshold;
        }
    }

    this->printMsg(
        "Processing "+std::to_string(nElements)+" Elements",
        1, t.getElapsedTime(), this->threadNumber_
    );

    this->printMsg( ttk::debug::Separator::L1 );

    return 1;
}

template <class binType, class dataType, class idType>
int ttk::PersistencePairInventory::ComputePPI(
    binType* ppi,

    const size_t& nRows,
    const dataType scalarBounds[2],
    const std::vector<dataType*>& scalars,
    const dataType* persistenceThresholds,
    const size_t& nPersistenceThresholds,
    const std::vector<idType*>& connectivityLists,
    const std::vector<size_t>& nEdges
) const {
    // -------------------------------------------------------------------------
    ttk::Timer globalTimer;

    size_t nCols = scalars.size();
    if(connectivityLists.size()!=nCols || nEdges.size()!=nCols){
        this->printErr("Scalars, ConnectivityLists, and nEdges must have the same size.");
        return 0;
    }

    // print header
    {
        this->printMsg( ttk::debug::Separator::L1 );
        this->printMsg("Computing Persistence Pair Inventory");
        this->printMsg( ttk::debug::Separator::L2 );
        this->printMsg({
            {"#Threads", std::to_string(this->threadNumber_)},
            {"#Rows", std::to_string(nRows)},
            {"#Cols", std::to_string(nCols)},
            {"#P.Intervals", std::to_string(nPersistenceThresholds)},
            {"Range", "["+std::to_string(scalarBounds[0])+", "+std::to_string(scalarBounds[1])+"]"}
        });
        this->printMsg( ttk::debug::Separator::L2 );
    }

    {
        ttk::Timer t;
        this->printMsg("Processing "+std::to_string(nCols)+" Elements",0,debug::LineMode::REPLACE);

        bool failed=false;
        #ifdef TTK_ENABLE_OPENMP
        #pragma omp parallel for num_threads(threadNumber_)
        #endif
        for(size_t i=0; i<nCols; i++){
            int status = this->ComputePPIColumn(
                ppi,

                nRows,
                nCols,
                i,
                scalarBounds,
                scalars[i],
                persistenceThresholds,
                nPersistenceThresholds,
                connectivityLists[i],
                nEdges[i]
            );

            if(!status)
                failed=true;
        }

        if(failed)
            return 0;

        this->printMsg(
            "Processing "+std::to_string(nCols)+" Elements",
            1, t.getElapsedTime(), this->threadNumber_
        );

        this->printMsg( ttk::debug::Separator::L1 ); // horizontal '=' Separator
    }

    return 1;
}

template <class binType, class dataType, class idType>
int ttk::PersistencePairInventory::ComputePPIColumn(
    binType* ppi,

    const size_t& nRows,
    const size_t& nCols,
    const size_t columnIndex,
    const dataType scalarBounds[2],
    const dataType* scalars,
    const dataType* persistenceThresholds,
    const size_t& nPersistenceThresholds,
    const idType* connectivityLists,
    const size_t& nEdges
) const {
    // clear ppi column
    {
        size_t rowOffset = nPersistenceThresholds*(nCols-1); // -1 since offset also increased in for loop by nPersistenceThresholds
        size_t offset = columnIndex*nPersistenceThresholds;
        for(size_t i=0; i<nRows; i++){
            for(size_t p=0; p<nPersistenceThresholds; p++)
                ppi[offset++] = 0;
            offset+=rowOffset;
        }
    }

    // process pairs
    {
        dataType range = scalarBounds[1]-scalarBounds[0];
        dataType nRowsM1 = nRows-1;

        size_t rowOffset = nPersistenceThresholds*nCols;
        for(size_t i=0,j=1; i<nEdges; i++,j+=3){
            const idType& v0 = connectivityLists[j];
            const idType& v1 = connectivityLists[j+1];

            const dataType& s0_ = scalars[v0];
            const dataType& s1_ = scalars[v1];

            // enforce order
            dataType s0 = s0_<s1_ ? s0_ : s1_;
            dataType s1 = s0_<s1_ ? s1_ : s0_;

            // skip if edge does not intersect interval
            if(s1<scalarBounds[0] || s0>scalarBounds[1]) continue;

            dataType persistence = s1-s0;

            // force bounds for bin index computation
            s0 = s0<scalarBounds[0] ? scalarBounds[0] : s0;
            s1 = s1>scalarBounds[1] ? scalarBounds[1] : s1;

            // compute bin indices
            size_t b0 = (s0-scalarBounds[0])/range*nRows;
            size_t b1 = (s1-scalarBounds[0])/range*nRows;
            b1 = b1>nRows ? nRows : b1;

            // compute last persistence interval index for which pair still exists
            size_t p1 = 0;
            while(p1<nPersistenceThresholds && persistenceThresholds[p1]<=persistence)
                p1++;

            size_t offset = columnIndex*nPersistenceThresholds + b0*rowOffset;
            for(size_t b=b0; b<b1; b++){
                for(size_t p=0; p<p1; p++)
                    ppi[offset+p]++;
                offset+=rowOffset;
            }
        }
    }

    return 1;
}

template <class binType>
int ttk::PersistencePairInventory::ComputeAPPI(
    binType* appi,

    const size_t& nRows,
    const size_t& nCols,
    const size_t& nPersistenceThresholds,
    const binType* ppi
) const {

    size_t rowOffset = nPersistenceThresholds*nCols;

    // for(size_t i=0; i<nRows; i++){
    //     size_t ppiOffset = i*rowOffset;
    //     for(size_t j-0; j<nCols; j++){


    //     }
    // }
}