/// \ingroup base
/// \class ttk::Disambiguate
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 1.09.2019
///
/// TODO

#pragma once

// ttk common includes
#include <Debug.h>
#include <Triangulation.h>
#include <Propagation.h>
#include <ParallelMergeSort.h>

#include <limits>
#include <queue>
#include <unordered_set>
#include <set>
#include <unordered_map>

#include <sys/time.h>

#if(defined(__GNUC__) && !defined(__clang__))
#include <parallel/algorithm>
#endif

#include <boost/math/special_functions/next.hpp>

// #if defined(ENABLE_OPENMP)
// #include <omp.h>
// #else
// typedef int omp_int_t;
// inline omp_int_t omp_get_thread_num() { return 0;}
// inline omp_int_t omp_get_max_threads() { return 1;}
// #endif

typedef ttk::SimplexId ttkInt;

int TODO_TASKSUBDIVISION = 1;

namespace ttk {

    class Disambiguate : virtual public Debug {

        public:

            Disambiguate(){
                this->setDebugMsgPrefix("Disambiguate"); // inherited from Debug: prefix will be printed at the beginning of every msg
            };
            ~Disambiguate(){};

            template<typename T,typename idType>
            int computeGlobalOffsets(
                idType* outputOffsets,
                std::vector<std::tuple<T,idType,idType>>& sortedIndices,

                const idType& nVertices,
                const T* rank1,
                const idType* rank2
            ) const {
                ttk::Timer t;

                // init tuples
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++){
                    auto& t = sortedIndices[i];
                    std::get<0>(t) = rank1[i];
                    std::get<1>(t) = rank2[i];
                    std::get<2>(t) = i;
                }

                this->printMsg(
                    "Computing offset scalar field",
                    0.2, t.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                #ifdef TTK_ENABLE_OPENMP
                    #ifdef __clang__
                        this->printWrn("Caution, outside GCC, sequential sort");
                        std::sort(sortedIndices.begin(), sortedIndices.end());
                    #else
                        omp_set_num_threads(this->threadNumber_);
                        __gnu_parallel::sort(sortedIndices.begin(), sortedIndices.end());
                        omp_set_num_threads(1);
                    #endif
                #else
                    this->printWrn("Caution, outside GCC, sequential sort");
                    std::sort(sortedIndices.begin(), sortedIndices.end());
                #endif

                this->printMsg(
                    "Computing offset scalar field",
                    0.8, t.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // compute actual global output offsets
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++)
                    outputOffsets[std::get<2>(sortedIndices[i])] = i;

                this->printMsg(
                    "Computing offset scalar field",
                    1,
                    t.getElapsedTime(),
                    this->threadNumber_
                );

                return 1;
            }

            template<typename dataType, typename idType>
            int computeNumericalPerturbation(
                dataType* outputScalars,

                const idType* offsets,
                const std::vector<std::tuple<idType,idType,idType>>& sortedIndices,
                const int sortDirection
            ) const {
                ttk::Timer t;
                this->printMsg(
                    "Applying numerical perturbation",
                    0,0,this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                const idType nVertices = sortedIndices.size();
                if(sortDirection>0){
                    for(idType i=1; i<nVertices; i++){
                        const idType& v0 = std::get<2>(sortedIndices[i-1]);
                        const idType& v1 = std::get<2>(sortedIndices[i]);
                        if(outputScalars[v0]>=outputScalars[v1])
                            outputScalars[v1] = boost::math::float_next(outputScalars[v0]);
                    }
                } else if(sortDirection<0) {
                    for(idType i=nVertices-1; i>1; i--){
                        const idType& v1 = std::get<2>(sortedIndices[i-1]);
                        const idType& v0 = std::get<2>(sortedIndices[i]);
                        if(outputScalars[v0]>=outputScalars[v1])
                            outputScalars[v1] = boost::math::float_next(outputScalars[v0]);
                    }
                }

                this->printMsg(
                    "Applying numerical perturbation",
                    1,t.getElapsedTime(),this->threadNumber_
                );

                return 1;
            }

            template<typename dataType, typename idType>
            int flattenScalars(
                dataType* scalars,

                const std::vector<Propagation<idType>*>& masterPropagations
            ) const {
                ttk::Timer t;
                this->printMsg("Flattening scalar field",0,0,this->threadNumber_,debug::LineMode::REPLACE);

                const idType nMasterPropagations = masterPropagations.size();

                #pragma omp parallel for schedule(dynamic) num_threads(this->threadNumber_)
                for(idType p=0; p<nMasterPropagations; p++){
                    const auto& propagation = *masterPropagations[p];
                    const idType s = propagation.lastEncounteredSaddle;
                    const dataType sScalar = scalars[s];
                    for(auto v : propagation.region)
                        scalars[v] = sScalar;
                }

                this->printMsg("Flattening scalar field",1,t.getElapsedTime(),this->threadNumber_);

                return 1;
            }

            template<typename idType>
            int detectUnauthorizedMaxima(
                std::vector<idType>& unauthorizedMaxima,
                idType* preservationMask,

                const ttk::Triangulation* triangulation,
                const idType* inputOffsets,
                const idType* preservedCriticalPointIndices,
                const idType& nPreservedCriticalPointIndices
            ) const {

                ttk::Timer t;
                this->printMsg("Detecting unauthorized maxima",0,0,this->threadNumber_,debug::LineMode::REPLACE);

                const idType nVertices = triangulation->getNumberOfVertices();

                // make room for the maximal number of maxima
                unauthorizedMaxima.resize(nVertices);

                // a synchronized write index used to store discarded maxima
                idType maximaWriteIndex=0;

                // init preservation mask
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++)
                    preservationMask[i]=-1;

                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nPreservedCriticalPointIndices; i++)
                    preservationMask[preservedCriticalPointIndices[i]]=-2;

                // find discareded maxima
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType v=0; v<nVertices; v++){

                    // if v needs to be preserved then skip
                    if(preservationMask[v]==-2)
                        continue;

                    // check if v has larger neighbors
                    bool hasLargerNeighbor = false;
                    const idType& vOffset = inputOffsets[v];
                    idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                    for(idType n=0; n<nNeighbors; n++){
                        idType u;
                        triangulation->getVertexNeighbor(v,n,u);
                        if( vOffset<inputOffsets[u] ){
                            hasLargerNeighbor = true;
                            break;
                        }
                    }

                    // if v has larger neighbors then v can not be maximum
                    if(hasLargerNeighbor)
                        continue;

                    // get local write index for this thread
                    idType localWriteIndex = 0;
                    #pragma omp atomic capture
                    localWriteIndex = maximaWriteIndex++;

                    // write maximum index
                    unauthorizedMaxima[localWriteIndex] = v;
                }

                // resize to the actual number of discarded maxima
                unauthorizedMaxima.resize(maximaWriteIndex);
                this->printMsg("Detecting unauthorized maxima ("+std::to_string(maximaWriteIndex)+")",1,t.getElapsedTime(),this->threadNumber_);

                return 1;
            }

            template<typename idType>
            int computeRegion(
                idType* regionMask,
                Propagation<idType>** propagationMask,
                Propagation<idType>* propagation,

                const ttk::Triangulation* triangulation
            ) const {

                const idType& extremumIndex = propagation->extremumIndex;

                // collect region
                auto& region = propagation->region;
                region.resize(propagation->regionSize);
                idType regionIndex = 0;
                {
                    std::vector<idType> queue(propagation->regionSize,-1);
                    idType queueIndex = 0;
                    {
                        const idType& saddleIndex = propagation->lastEncounteredSaddle;
                        idType nNeighbors = triangulation->getVertexNeighborNumber(saddleIndex);
                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(saddleIndex,n,u);
                            if(propagationMask[u]!=nullptr && propagationMask[u]->find()==propagation){
                                queue[queueIndex++]=u;
                                regionMask[u] = extremumIndex;
                            }
                        }
                    }

                    while(queueIndex>0){
                        const idType v = queue[--queueIndex];

                        region[regionIndex++] = v;

                        idType nNeighbors = triangulation->getVertexNeighborNumber(v);
                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);
                            if(regionMask[u]!=extremumIndex && propagationMask[u]!=nullptr && propagationMask[u]->find()==propagation){
                                queue[queueIndex++]=u;
                                regionMask[u] = extremumIndex;
                            }
                        }
                    }
                }

                return 1;
            }

            template<typename idType>
            int computeRegions(
                idType* regionMask,
                Propagation<idType>** propagationMask,
                std::vector<Propagation<idType>*>& propagations,

                const ttk::Triangulation* triangulation
            ) const {

                const idType nPropagations = propagations.size();
                const idType nVertices = triangulation->getNumberOfVertices();

                ttk::Timer t;
                this->printMsg(
                    "Computing regions ("+std::to_string(nPropagations)+")",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                int status = 1;

                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++){
                    regionMask[i] = -1;
                }

                #pragma omp parallel for schedule(dynamic) num_threads(this->threadNumber_)
                for(idType p=0; p<nPropagations; p++){
                    int localStatus = this->computeRegion<idType>(
                        regionMask,
                        propagationMask,
                        propagations[p],

                        triangulation
                    );
                    if(!localStatus)
                        status = 0;
                }
                if(!status) return 0;

                this->printMsg(
                    "Computing regions ("+std::to_string(nPropagations)+")",
                    1, t.getElapsedTime(), this->threadNumber_
                );

                return 1;
            }

            template<typename idType>
            int computePropagation(
                idType* saddleMask, // used here to store registered larger vertices
                idType* queueMask, // used to mark vertices that have already been added to the queue by this thread
                Propagation<idType>** propagationMask,
                Propagation<idType>& propagation,

                const ttk::Triangulation* triangulation,
                const idType* offsets
            ) const {

                // pointer used to compare against representative
                auto* propagationP = &propagation;

                // frequently used propagation members
                const idType& extremumIndex = propagationP->extremumIndex;
                auto* queue = &propagationP->queue;

                // add extremumIndex to queue
                queue->emplace(offsets[extremumIndex],extremumIndex);
                queueMask[extremumIndex] = extremumIndex;

                // grow region until it reaches a saddle and then decide if it should continue
                while(!queue->empty()){
                    const idType v = std::get<1>(queue->top());
                    queue->pop();

                    // continue if this thread has already seen this vertex
                    if(propagationMask[v]!=nullptr)
                        continue;

                    // add neighbors to queue AND check if v is a saddle
                    bool isSaddle = false;
                    idType nNeighbors = triangulation->getVertexNeighborNumber( v );

                    idType numberOfLargerNeighbors = 0;
                    idType numberOfLargerNeighborsThisThreadVisited = 0;
                    for(idType n=0; n<nNeighbors; n++){
                        idType u;
                        triangulation->getVertexNeighbor(v,n,u);

                        // if larger neighbor
                        if( offsets[u]>offsets[v] ){
                            numberOfLargerNeighbors++;

                            if(propagationMask[u]==nullptr || propagationP!=propagationMask[u]->find())
                                isSaddle = true;
                            else
                                numberOfLargerNeighborsThisThreadVisited++;

                        } else if(queueMask[u] != extremumIndex){
                            queue->emplace(offsets[u],u);
                            queueMask[u] = extremumIndex;
                        }
                    }

                    // if v is a saddle we have to check if the current thread is the last visitor
                    if(isSaddle){

                        propagationP->lastEncounteredSaddle = v;

                        // * this check is performed by synchronously adding the number of larger vertices that the current thread visited to the saddle outputOffset
                        // * if after this synchronous operation the outputOffset at the saddle equals the total number of larger vertices then this must be the last thread that visited the saddle
                        // * Note: the offset stores the temporary value -1 for unvisited vertices and only values >=0 for processed vertices -> the number of larger neighbors of v visited by this thread is actually substracted from the offset to prevent the creation of a separate mask
                        idType numberOfRegisteredLargerVertices=0;
                        #pragma omp atomic capture
                        {
                            saddleMask[v] -= numberOfLargerNeighborsThisThreadVisited;
                            numberOfRegisteredLargerVertices = saddleMask[v];
                        }

                        // if this thread did not register the last remaining larger vertices then terminate propagation
                        if(numberOfRegisteredLargerVertices != -numberOfLargerNeighbors-1){
                            propagationP->terminated = 1;
                            return 1;
                        }

                        // Otherwise merge propagation data
                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);
                            if(offsets[v]<offsets[u] && propagationP!=propagationMask[u]->find()){
                                propagationP = Propagation<idType>::unify(
                                    propagationP,
                                    propagationMask[u]
                                );
                                queue = &propagationP->queue;
                            }
                        }
                    }

                    // mark vertex as visited and continue
                    propagationMask[v] = propagationP;
                    propagationP->regionSize++;
                }

                this->printErr("propagation reached global minimum");

                return 0;
            }

            template<typename idType>
            int computePropagationII(
                idType* saddleMask, // used here to store registered larger vertices
                idType* queueMask, // used to mark vertices that have already been added to the queue by this thread
                Propagation<idType>** propagationMask,
                Propagation<idType>& propagation,
                idType& currentStatus,

                const ttk::Triangulation* triangulation,
                const idType* offsets
            ) const {

                // pointer used to compare against representative
                auto* propagationP = &propagation;

                // frequently used propagation members
                const idType& extremumIndex = propagationP->extremumIndex;
                auto* queue = &propagationP->queue;

                // add extremumIndex to queue
                queue->emplace(offsets[extremumIndex],extremumIndex);
                queueMask[extremumIndex] = extremumIndex;

                idType interval =9999999;

                // grow region until it reaches a saddle and then decide if it should continue
                while(!queue->empty()){
                    const idType v = std::get<1>(queue->top());
                    queue->pop();

                    // continue if this thread has already seen this vertex
                    if(propagationMask[v]!=nullptr)
                        continue;

                    const idType& offsetV = offsets[v];

                    // add neighbors to queue AND check if v is a saddle
                    bool isSaddle = false;
                    idType nNeighbors = triangulation->getVertexNeighborNumber( v );

                    idType numberOfLargerNeighbors = 0;
                    idType numberOfLargerNeighborsThisThreadVisited = 0;
                    for(idType n=0; n<nNeighbors; n++){
                        idType u;
                        triangulation->getVertexNeighbor(v,n,u);

                        const idType& offsetU = offsets[u];

                        // if larger neighbor
                        if( offsetU>offsetV ){
                            numberOfLargerNeighbors++;

                            if(propagationMask[u]==nullptr || propagationP!=propagationMask[u]->find())
                                isSaddle = true;
                            else
                                numberOfLargerNeighborsThisThreadVisited++;

                        } else if(queueMask[u] != extremumIndex){
                            queue->emplace(offsetU,u);
                            queueMask[u] = extremumIndex;
                        }
                    }

                    // if v is a saddle we have to check if the current thread is the last visitor
                    if(isSaddle){
                        propagationP->lastEncounteredSaddle = v;

                        idType numberOfRegisteredLargerVertices=0;
                        #pragma omp atomic capture
                        {
                            saddleMask[v] -= numberOfLargerNeighborsThisThreadVisited;
                            numberOfRegisteredLargerVertices = saddleMask[v];
                        }

                        // if this thread did not register the last remaining larger vertices then terminate propagation
                        if(numberOfRegisteredLargerVertices != -numberOfLargerNeighbors-1){
                            propagationP->terminated = 1;
                            return 1;
                        }

                        // Otherwise merge propagation data
                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);

                            if(offsetV<offsets[u] && propagationP!=propagationMask[u]->find()){
                                propagationP = Propagation<idType>::unify(
                                    propagationP,
                                    propagationMask[u]
                                );
                                queue = &propagationP->queue;
                            }
                        }
                    }

                    // mark vertex as visited and continue
                    propagationMask[v] = propagationP;
                    propagationP->regionSize++;

                    if(interval++>1000){
                        #pragma omp atomic write
                        currentStatus = offsetV;
                        interval = 0;
                    }
                }

                this->printErr("propagation reached global minimum");

                return 0;
            }

            template<typename idType>
            int spawnInterleavedTasks(
                idType* localOffsets,
                idType* regionMask,
                Propagation<idType>** propagationMask,
                std::vector<Propagation<idType>>& propagations,
                idType* distanceField,

                const ttk::Triangulation* triangulation,
                const idType* inputOffsets,
                const bool& useRegionBasedIterations,
                const std::vector<idType>& progress
            )const {
                const idType nPropagations = propagations.size();
                const idType nVertices = triangulation->getNumberOfVertices();

                idType pivot = 0;
                // idType lastActivePropagation = 0;
                // idType sanity = 0;
                // for(idType i=0; i<this->threadNumber_; i++){
                //     #pragma omp atomic read
                //     pivot=progress[i];

                //     if(pivot!=-nVertices-1){
                //         lastActivePropagation = i;
                //         sanity++;
                //     }
                // }

                do {
                    #pragma omp atomic read
                    pivot = progress[0];

                    // get progress of slowest propagation
                    for(idType t=1; t<this->threadNumber_; t++){
                        idType progress_ = 0;
                        #pragma omp atomic read
                        progress_ = progress[t];
                        if(pivot<progress_)
                            pivot=progress_;
                    }

                    // #pragma omp atomic read
                    // pivot = progress[lastActivePropagation];

                    for(idType p=0; p<nPropagations; p++){
                        auto propagation = &propagations[p];

                        signed char terminated;
                        // #pragma omp atomic read
                        terminated = propagation->terminated;

                        idType saddleIndex;
                        // #pragma omp atomic read
                        saddleIndex = propagation->lastEncounteredSaddle;

                        // Propagation<idType>* parent;
                        // // #pragma omp atomic read
                        // parent = propagation->parent;

                        // if the propagation is complete
                        if(terminated==0 || propagation->temp==1)
                            continue;

                        if(inputOffsets[saddleIndex]<=pivot)
                            continue;

                        propagation->temp = 1;

                        #pragma omp task firstprivate(propagation) priority(1)
                        {
                            this->computeRegion<idType>(
                                regionMask,
                                propagationMask,
                                propagation,

                                triangulation
                            );

                            this->computeLocalOffsetsOfRegion<idType>(
                                localOffsets,
                                distanceField,

                                propagation,
                                triangulation,
                                regionMask,
                                inputOffsets,
                                useRegionBasedIterations
                            );
                        }
                    }
                } while(pivot!=-nVertices-1);

                return 1;
            }

            template<typename idType>
            int computeInterleaving(
                idType* localOffsets,
                idType* regionMask,
                idType* queueMask,
                Propagation<idType>** propagationMask,
                std::vector<Propagation<idType>>& propagations,
                idType* distanceField,

                const std::vector<idType>& unauthorizedMaxima,
                const ttk::Triangulation* triangulation,
                const idType* inputOffsets,
                const bool& useRegionBasedIterations
            ) const {
                const idType nVertices = triangulation->getNumberOfVertices();
                const idType nPropagations = unauthorizedMaxima.size();

                ttk::Timer timer;
                this->printMsg(
                    "Interleaved Computation ("+std::to_string(nPropagations)+")",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                std::vector<idType> progress(this->threadNumber_,nVertices+1);
                idType nActivePropagations = nPropagations;

                int status = 1;
                #pragma omp parallel num_threads(this->threadNumber_)
                #pragma omp single
                for(idType t=0; t<this->threadNumber_; t++){

                    #pragma omp task firstprivate(t) priority(2)
                    {
                        while(true) {
                            idType nActivePropagations_ = 0;
                            #pragma omp atomic capture
                            {
                                nActivePropagations--;
                                nActivePropagations_ = nActivePropagations;
                            }

                            if(nActivePropagations_<0){
                                progress[t] = -nVertices-1;

                                if(
                                    // (this->threadNumber_>1 && nActivePropagations_==-this->threadNumber_+1)
                                    (this->threadNumber_>1 && nActivePropagations_==-1)
                                    ||
                                    (this->threadNumber_<2)
                                ){
                                    this->spawnInterleavedTasks<idType>(
                                        localOffsets,
                                        regionMask,
                                        propagationMask,
                                        propagations,
                                        distanceField,

                                        triangulation,
                                        inputOffsets,
                                        useRegionBasedIterations,
                                        progress
                                    );
                                }

                                break;
                            }

                            this->computePropagationII<idType>(
                                regionMask,
                                queueMask,
                                propagationMask,
                                propagations[nActivePropagations_],
                                progress[t],

                                triangulation,
                                inputOffsets
                            );
                        }
                    }
                }
                if(!status)
                    return 0;

                this->printMsg(
                    "Interleaved Computation ("+std::to_string(nPropagations)+")",
                    1, timer.getElapsedTime(), this->threadNumber_
                );

                return 1;
            }

            template<typename idType>
            int sortPropagations(
                std::vector<Propagation<idType>>& propagations,

                const idType* inputOffsets
            ) const {
                ttk::Timer timer;

                const idType nPropagations = propagations.size();

                this->printMsg(
                    "Sort propagations ("+std::to_string(nPropagations)+")",
                    0, timer.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                std::vector<std::tuple<idType,idType>> sortedPropagations(nPropagations);
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType p=0; p<nPropagations; p++){
                    std::get<0>(sortedPropagations[p]) = -inputOffsets[propagations[p].extremumIndex];
                    std::get<1>(sortedPropagations[p]) = propagations[p].extremumIndex;
                }

                {
                    #ifdef TTK_ENABLE_OPENMP
                        #ifdef __clang__
                            this->printWrn("Caution, outside GCC, sequential sort");
                            std::sort(sortedPropagations.begin(), sortedPropagations.end());
                        #else
                            omp_set_num_threads(this->threadNumber_);
                            __gnu_parallel::sort(sortedPropagations.begin(), sortedPropagations.end());
                            omp_set_num_threads(1);
                        #endif
                    #else
                        this->printWrn("Caution, outside GCC, sequential sort");
                        std::sort(sortedPropagations.begin(), sortedPropagations.end());
                    #endif
                }

                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType p=0; p<nPropagations; p++){
                    propagations[p].extremumIndex = std::get<1>(sortedPropagations[p]);
                }

                this->printMsg(
                    "Sort propagations ("+std::to_string(nPropagations)+")",
                    1, timer.getElapsedTime(), this->threadNumber_
                );

                return 1;
            };

            template<typename idType>
            int initializePropagations(
                std::vector<Propagation<idType>>& propagations,
                idType* regionMask,
                idType* localOffsets,
                idType* queueMask,
                Propagation<idType>** propagationMask,

                const std::vector<idType>& unauthorizedExtrema,
                const idType& nVertices
            ) const {
                ttk::Timer timer;

                const idType nPropagations = unauthorizedExtrema.size();

                this->printMsg(
                    "Initialize propagations ("+std::to_string(nPropagations)+")",
                    0, timer.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // init region/queue/propagation mask
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++){
                    regionMask[i] = -1;
                    localOffsets[i] = 1;
                    queueMask[i] = -1;
                    propagationMask[i] = nullptr;
                }

                propagations.clear();
                propagations.resize(nPropagations);
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nPropagations; i++)
                    propagations[i].extremumIndex = unauthorizedExtrema[i];

                this->printMsg(
                    "Initialize propagations ("+std::to_string(nPropagations)+")",
                    1, timer.getElapsedTime(), this->threadNumber_
                );

                return 1;
            };

            template<typename idType>
            int finalizePropagations(
                std::vector<Propagation<idType>*>& masterPropagations,
                std::vector<Propagation<idType>>& propagations,

                const idType nVertices
            ) const {
                ttk::Timer timer;

                const idType nPropagations = propagations.size();

                this->printMsg(
                    "Finalizing propagations ("+std::to_string(nPropagations)+")",
                    0, timer.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                idType nRegionVertices = 0;
                idType nMasterPropagations=0;
                masterPropagations.clear();
                masterPropagations.resize(nPropagations);
                for(idType p=0; p<nPropagations; p++){
                    auto& propagation = propagations[p];
                    if(propagation.parent == nullptr){
                        nRegionVertices = nRegionVertices + propagation.regionSize;
                        masterPropagations[nMasterPropagations++] = &propagation;
                    }
                }
                masterPropagations.resize(nMasterPropagations);

                std::stringstream pFraction, vFraction;
                pFraction << std::fixed << std::setprecision(2) << ((float)nMasterPropagations/(float)nPropagations);
                vFraction << std::fixed << std::setprecision(2) << ((float)nRegionVertices/(float)nVertices);

                this->printMsg(
                    "Finalizing propagations ("+std::to_string(nPropagations)+"|"+pFraction.str()+"|"+vFraction.str()+")",
                    1, timer.getElapsedTime(), this->threadNumber_
                );

                return 1;
            };

            template<typename idType>
            int computePropagations(
                idType* saddleMask,
                idType* queueMask,
                Propagation<idType>** propagationMask,
                std::vector<Propagation<idType>>& propagations,

                const std::vector<idType>& unauthorizedMaxima,
                const ttk::Triangulation* triangulation,
                const idType* offsets
            ) const {
                ttk::Timer timer;

                const idType nPropagations = unauthorizedMaxima.size();
                this->printMsg(
                    "Computing propagations ("+std::to_string(nPropagations)+")",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                int status = 1;
                // compute propagations
                #pragma omp parallel for schedule(dynamic,1) num_threads(this->threadNumber_)
                for(idType p=0; p<nPropagations; p++){
                    int localStatus = this->computePropagation<idType>(
                        saddleMask,
                        queueMask,
                        propagationMask,
                        propagations[p],

                        triangulation,
                        offsets
                    );

                    if(!localStatus)
                        status = 0;
                }

                if(!status) return 0;

                this->printMsg(
                    "Computing propagations ("+std::to_string(nPropagations)+")",
                    1, timer.getElapsedTime(), this->threadNumber_
                );

                return 1;
            }

            template<typename idType>
            int computeLevelSetComponents(
                idType* localOffsets,

                const ttk::Triangulation* triangulation,
                const idType* regionMask,
                const idType& regionID,
                const std::vector<idType>& region,
                const std::vector<idType>& seedVertices,
                const idType* distanceField,
                const idType& localOffsetSortingDirection // controls if the first visited vertex is the last or first element of the local offsets
            ) const {

                // NOTE: this function uses the localOffsets during computation to add every region vertex exectly once
                for(const auto& v: region)
                    localOffsets[v] = regionID;

                // init priority queue
                std::priority_queue<
                    std::pair<idType,idType>,
                    std::vector<std::pair<idType,idType>>
                > queue;

                for(const auto& v: seedVertices){
                    queue.emplace( distanceField[v], v );
                    localOffsets[v] = -1;
                }

                std::vector<idType> localVertexSequence(region.size());

                idType q=0;
                while(!queue.empty()){
                    idType v = std::get<1>(queue.top());
                    queue.pop();

                    localVertexSequence[q++] = v;

                    idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                    for(idType n=0; n<nNeighbors; n++){
                        idType u;
                        triangulation->getVertexNeighbor(v,n,u);

                        // if u has not been popped from the queue add to queue
                        if(localOffsets[u]==regionID){
                            queue.emplace( distanceField[u], u );
                            localOffsets[u] = -1;
                        }
                    }
                }

                idType localOffset = -1;
                if(localOffsetSortingDirection<0){
                    for(idType i=0, j=region.size(); i<j; i++)
                        localOffsets[ localVertexSequence[i] ] = localOffset--;
                } else {
                    for(idType i=region.size()-1; i>=0; i--)
                        localOffsets[ localVertexSequence[i] ] = localOffset--;
                }

                return 1;
            }

            template<typename idType>
            int computeLocalOffsetsOfRegion(
                idType* localOffsets,
                idType* distanceField,

                const Propagation<idType>* propagation,
                const ttk::Triangulation* triangulation,
                const idType* regionMask,
                const idType* inputOffsets,
                const bool& useRegionBasedIterations
            ) const {

                if(propagation->regionSize==1){
                    localOffsets[ propagation->region[0] ] = -1;
                    return 1;
                }

                const idType& extremumIndex = propagation->extremumIndex;
                const idType& saddleIndex = propagation->lastEncounteredSaddle;

                // init distance field
                for(const auto& v: propagation->region)
                    distanceField[v] = inputOffsets[v];

                // there is always only one authorized maximum, which is next to the saddle
                std::vector<idType> saddleNeighbors;
                {
                    // get all saddle neighbors inside region
                    idType nNeighbors = triangulation->getVertexNeighborNumber( saddleIndex );
                    for(idType n=0; n<nNeighbors; n++){
                        idType u;
                        triangulation->getVertexNeighbor(saddleIndex,n,u);
                        if(regionMask[u]==extremumIndex){
                            saddleNeighbors.push_back( u );
                            distanceField[u] = std::numeric_limits<idType>::max(); // force that there is always one maximum next to the saddle
                        }
                    }
                }

                int status = 1;

                // start first iteration from saddle neighbors
                status = this->computeLevelSetComponents<idType>(
                    localOffsets,

                    triangulation,
                    regionMask,
                    saddleIndex,
                    propagation->region,
                    saddleNeighbors,
                    distanceField,
                    -1
                );
                if(!status)
                    return 0;

                propagation->nIterations = 1;

                if(!useRegionBasedIterations){
                    return 1;
                }

                bool containsResidualExtrema = false;

                // get all vertices on the boundary
                std::vector<idType> regionBoundary(propagation->regionSize);
                {
                    idType boundaryWriteIndex = 0;

                    for(const auto& v: propagation->region){
                        bool isOnRegionBoundary = false;
                        bool hasSmallerNeighbor = false;

                        idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);

                            // if u is not inside region -> v is on region boundary
                            if(regionMask[u]!=extremumIndex){
                                isOnRegionBoundary = true;
                            } else if (localOffsets[u]<localOffsets[v]){
                                hasSmallerNeighbor = true;
                            }
                        }

                        if(isOnRegionBoundary)
                            regionBoundary[boundaryWriteIndex++] = v;
                        else if(!hasSmallerNeighbor)
                            containsResidualExtrema = true;
                    }

                    regionBoundary.resize(boundaryWriteIndex);
                }

                idType localOffsetSortingDirection = -1;
                while(containsResidualExtrema){
                    localOffsetSortingDirection*=-1;

                    // set seed vertices and init distance field
                    std::vector<idType>* seedVertices;
                    if(localOffsetSortingDirection>0){
                        seedVertices = &regionBoundary;
                        for(const auto& v: propagation->region)
                            distanceField[v] = -localOffsets[v];
                    } else {
                        seedVertices = &saddleNeighbors;
                        for(const auto& v: propagation->region)
                            distanceField[v] = localOffsets[v];
                    }

                    status = this->computeLevelSetComponents<idType>(
                        localOffsets,

                        triangulation,
                        regionMask,
                        saddleIndex,
                        propagation->region,
                        *seedVertices,
                        distanceField,
                        localOffsetSortingDirection
                    );
                    if(!status)
                        return 0;

                    // check if number of extrema correpsonds to number of authorized extrema
                    idType nUnauthorizedMinima = 0;
                    idType nUnauthorizedMaxima = 0;

                    for(const auto& v: propagation->region){
                        // check if v is on the boundary and if it has no smaller neighbors inside region
                        bool hasSmallerNeighbor = false;
                        bool hasLargerNeighbor = false;
                        bool isOnRegionBoundary = false;
                        bool isNextToSaddle = false;

                        idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);

                            if(regionMask[u]!=extremumIndex){
                                isOnRegionBoundary = true;

                                if(u==saddleIndex)
                                    isNextToSaddle = true;

                                continue;
                            }

                            if(localOffsets[u]<localOffsets[v]) {
                                hasSmallerNeighbor = true;
                            } else {
                                hasLargerNeighbor = true;
                            }
                        }

                        if(!hasLargerNeighbor && !isNextToSaddle){
                            nUnauthorizedMaxima++;
                        } else if(!hasSmallerNeighbor && !isOnRegionBoundary){
                            nUnauthorizedMinima++;
                        }
                    }

                    propagation->nIterations++;

                    containsResidualExtrema = nUnauthorizedMaxima>0 || nUnauthorizedMinima>0;
                }

                return 1;
            }

            template<typename idType>
            int computeLocalOffsetsOfRegions(
                idType* localOffsets,
                idType* distanceField,

                const ttk::Triangulation* triangulation,
                const idType* regionMask,
                const idType* inputOffsets,
                const std::vector<Propagation<idType>*>& masterPropagations,
                const bool& useRegionBasedIterations
            ) const {

                ttk::Timer t;
                this->printMsg(
                    "Computing local order of regions",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                const idType nMasterPropagations=masterPropagations.size();
                int status = 1;
                #pragma omp parallel for schedule(dynamic) num_threads(this->threadNumber_)
                for(idType p=0; p<nMasterPropagations; p++){
                    int localStatus = this->computeLocalOffsetsOfRegion<idType>(
                        localOffsets,
                        distanceField,

                        masterPropagations[p],
                        triangulation,
                        regionMask,
                        inputOffsets,
                        useRegionBasedIterations
                    );
                    if(!localStatus)
                        status = 0;
                }
                if(!status)
                    return 0;

                this->printMsg( "Computing local order of regions ("+std::to_string(nMasterPropagations)+")",
                    1, t.getElapsedTime(), this->threadNumber_
                );

                return 1;
            }

            template<typename idType>
            int detectAndRemoveUnauthorizedMaxima(
                std::vector<idType>& unauthorizedMaxima,
                idType* outputOffsets,
                idType* localOffsets,
                idType* regionMask,
                idType* queueMask,
                Propagation<idType>** propagationMask,
                std::vector<Propagation<idType>>& propagations,
                std::vector<Propagation<idType>*>& masterPropagations,
                std::vector<std::tuple<idType,idType,idType>>& sortedIndices,

                const ttk::Triangulation* triangulation,
                const idType* preservedCriticalPointIndices,
                const idType& nPreservedCriticalPointIndices,
                const idType* inputOffsets,
                const bool&   useRegionBasedIterations,
                const bool&   useInterleaving
            ) const {
                const idType nVertices = triangulation->getNumberOfVertices();

                int status = 0;

                // Classify Critical Points
                status = this->detectUnauthorizedMaxima<idType>(
                    unauthorizedMaxima,
                    localOffsets, // used here to temporarily store preservation mask

                    triangulation,
                    inputOffsets,
                    preservedCriticalPointIndices,
                    nPreservedCriticalPointIndices
                );
                if(!status) return 0;

                // if nothing to remove return
                if(unauthorizedMaxima.size()<1)
                    return 1;

                // init propagations
                status = this->initializePropagations<idType>(
                    propagations,
                    regionMask,
                    localOffsets,
                    queueMask,
                    propagationMask,

                    unauthorizedMaxima,
                    triangulation->getNumberOfVertices()
                );
                if(!status) return 0;

                idType nMasterPropagations = 0;
                if(!useInterleaving){
                    // sort propagations
                    status = this->sortPropagations<idType>(
                        propagations,

                        inputOffsets
                    );
                    if(!status) return 0;

                    // compute propagations
                    status = this->computePropagations<idType>(
                        regionMask,
                        queueMask,
                        propagationMask,
                        propagations,

                        unauthorizedMaxima,
                        triangulation,
                        inputOffsets
                    );
                    if(!status) return 0;

                    // finalize master propagations
                    status = this->finalizePropagations<idType>(
                        masterPropagations,
                        propagations,

                        triangulation->getNumberOfVertices()
                    );
                    if(!status) return 0;
                    nMasterPropagations = masterPropagations.size();

                    // compute regions
                    status = this->computeRegions<idType>(
                        regionMask,
                        propagationMask,
                        masterPropagations,

                        triangulation
                    );
                    if(!status) return 0;

                    // compute local order of regions
                    status = this->computeLocalOffsetsOfRegions<idType>(
                        localOffsets,
                        outputOffsets, // used here to temporarily store distance field

                        triangulation,
                        regionMask,
                        inputOffsets,
                        masterPropagations,
                        useRegionBasedIterations
                    );
                    if(!status) return 0;

                } else {

                    // sort propagations
                    status = this->sortPropagations<idType>(
                        propagations,

                        inputOffsets
                    );
                    if(!status) return 0;

                    // compute propagations
                    status = this->computeInterleaving<idType>(
                        localOffsets,
                        regionMask,
                        queueMask,
                        propagationMask,
                        propagations,
                        outputOffsets, // used here to temporarily store distance field

                        unauthorizedMaxima,
                        triangulation,
                        inputOffsets,
                        useRegionBasedIterations
                    );
                    if(!status) return 0;

                    // finalize master propagations
                    status = this->finalizePropagations<idType>(
                        masterPropagations,
                        propagations,

                        triangulation->getNumberOfVertices()
                    );
                    if(!status) return 0;
                    nMasterPropagations = masterPropagations.size();
                }

                // use region mask as temporary array
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType v=0; v<nVertices; v++)
                    regionMask[v] = inputOffsets[v];

                // flatten regions to offset of last encountered saddles
                // and force that saddles are last in the local offset order
                #pragma omp parallel for schedule(static,4) num_threads(this->threadNumber_)
                for(idType p=0; p<nMasterPropagations; p++){
                    const auto* propagation = masterPropagations[p];
                    for(const auto& v : propagation->region)
                        regionMask[v] = inputOffsets[propagation->lastEncounteredSaddle];

                    // enforce that each saddle has the largest local offset
                    localOffsets[propagation->lastEncounteredSaddle]=0;
                }

                // compute global offsets
                status = this->computeGlobalOffsets<idType,idType>(
                    outputOffsets,
                    sortedIndices,

                    triangulation->getNumberOfVertices(),
                    regionMask,
                    localOffsets
                );
                if(!status) return 0;

                return 1;
            };

            template<typename dataType, typename idType>
            int simplify(
                dataType* outputScalars,
                idType* outputOffsets,

                const ttk::Triangulation* triangulation,
                const dataType* inputScalars,
                const idType* inputOffsetsCorrupted,
                const idType* preservedCriticalPointIndices,
                const idType& nPreservedCriticalPointIndices,
                const bool&   useRegionBasedIterations,
                const bool&   useInterleaving,
                const bool&   addPerturbation,
                const bool&   useDeallocation
            ) const {

                ttk::Timer timer;

                this->printMsg(debug::Separator::L1);
                this->printMsg({
                    {"Use Region-Based Iterations", std::string(useRegionBasedIterations ? "true" : "false")},
                    {"Use Interleaving", std::string(useInterleaving ? "true" : "false")},
                    {"Add Perturbation", std::string(addPerturbation ? "true" : "false")},
                });
                this->printMsg(debug::Separator::L2);

                idType nVertices = triangulation->getNumberOfVertices();

                // allocate memory
                this->printMsg(
                    "Allocating memory",
                    0,0,this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                std::vector<idType> inputOffsets(nVertices);
                std::vector<idType> unauthorizedExtrema(nVertices);
                std::vector<idType> regionMask(nVertices);
                std::vector<Propagation<idType>*> propagationMask(nVertices);
                std::vector<idType> localOffsets(nVertices);
                std::vector<std::tuple<idType,idType,idType>> sortedIndices(nVertices);
                std::vector<Propagation<idType>> propagationsMax;
                std::vector<Propagation<idType>*> masterPropagationsMax;
                std::vector<Propagation<idType>> propagationsMin;
                std::vector<Propagation<idType>*> masterPropagationsMin;

                std::vector<idType> queueMask;
                if(useInterleaving){
                    queueMask.resize(nVertices,-1);
                }

                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++)
                    outputScalars[i] = inputScalars[i];

                this->printMsg(
                    "Allocating memory",
                    1,timer.getElapsedTime(),this->threadNumber_
                );
                this->printMsg(debug::Separator::L2);
                timer.reStart();

                // compute initial offsets if not exisitng (currently this done by default)
                {
                    std::vector<std::tuple<dataType,idType,idType>> temp(nVertices);
                    this->computeGlobalOffsets<dataType,idType>(
                        outputOffsets,
                        temp,

                        nVertices,
                        inputScalars,
                        inputOffsetsCorrupted
                    );
                }

                size_t iteration=0;
                int status = 0;
                int sortDirection = 0;
                while(true){
                    this->printMsg(
                        "Iteration: "+std::to_string(iteration++),
                        ttk::debug::Separator::L2
                    );

                    idType nUnauthorizedMinima=0;
                    idType nUnauthorizedMaxima=0;

                    // Minima
                    {
                        // invert offsets to first remove minima (now maxima)
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++)
                            inputOffsets[v] = -outputOffsets[v];
                    }

                    status = this->detectAndRemoveUnauthorizedMaxima<idType>(
                        unauthorizedExtrema,
                        outputOffsets,
                        localOffsets.data(),
                        regionMask.data(),
                        useInterleaving ? queueMask.data() : localOffsets.data(),
                        propagationMask.data(),
                        propagationsMin,
                        masterPropagationsMin,
                        sortedIndices,

                        triangulation,
                        preservedCriticalPointIndices,
                        nPreservedCriticalPointIndices,
                        inputOffsets.data(),
                        useRegionBasedIterations,
                        useInterleaving
                    );
                    if(!status) return 0;
                    nUnauthorizedMinima = unauthorizedExtrema.size();

                    if(nUnauthorizedMinima){
                        sortDirection=-1;
                        status = this->flattenScalars<dataType,idType>(
                            outputScalars,

                            masterPropagationsMin
                        );
                        if(!status) return 0;
                    }

                    // Maxima
                    {
                        // invert offsets again to now remove maxima
                        if(nUnauthorizedMinima>0){
                            #pragma omp parallel for num_threads(this->threadNumber_)
                            for(idType v=0; v<nVertices; v++)
                                inputOffsets[v] = -outputOffsets[v];
                        } else {
                            #pragma omp parallel for num_threads(this->threadNumber_)
                            for(idType v=0; v<nVertices; v++)
                                inputOffsets[v] = outputOffsets[v];
                        }
                    }

                    status = this->detectAndRemoveUnauthorizedMaxima<idType>(
                        unauthorizedExtrema,
                        outputOffsets,
                        localOffsets.data(),
                        regionMask.data(),
                        useInterleaving ? queueMask.data() : localOffsets.data(),
                        propagationMask.data(),
                        propagationsMax,
                        masterPropagationsMax,
                        sortedIndices,

                        triangulation,
                        preservedCriticalPointIndices,
                        nPreservedCriticalPointIndices,
                        inputOffsets.data(),
                        useRegionBasedIterations,
                        useInterleaving
                    );
                    if(!status) return 0;
                    nUnauthorizedMaxima = unauthorizedExtrema.size();

                    if(nUnauthorizedMaxima){
                        sortDirection=+1;
                        status = this->flattenScalars<dataType,idType>(
                            outputScalars,

                            masterPropagationsMax
                        );
                        if(!status) return 0;
                    }

                    if(nUnauthorizedMinima>0 && nUnauthorizedMaxima<1){
                        const idType maxOffset = nVertices-1;
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++)
                            outputOffsets[v] = maxOffset-outputOffsets[v];
                    }

                    if(useRegionBasedIterations || (nUnauthorizedMinima+nUnauthorizedMaxima)==0)
                        break;if(nUnauthorizedMinima>0 && nUnauthorizedMaxima<1){
                        const idType maxOffset = nVertices-1;
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++)
                            outputOffsets[v] = maxOffset-outputOffsets[v];
                    }

                    // break;
                }

                if(sortDirection!=0 && addPerturbation){
                    this->printMsg(debug::Separator::L2);
                    this->computeNumericalPerturbation<dataType,idType>(
                        outputScalars,

                        outputOffsets,
                        sortedIndices,
                        sortDirection
                    );
                }

                this->printMsg(debug::Separator::L2);
                this->printMsg("Complete", 1, timer.getElapsedTime(), this->threadNumber_);

                if(useDeallocation){
                    // Deallocating memory
                    timer.reStart();
                    this->printMsg(debug::Separator::L2);
                    this->printMsg(
                        "Deallocating memory",
                        0,0,this->threadNumber_,
                        debug::LineMode::REPLACE
                    );

                    // min propagations
                    {
                        idType nPropagations = propagationsMin.size();
                        #pragma omp parallel for schedule(dynamic) num_threads(this->threadNumber_)
                        for(idType p=0; p<nPropagations; p++){
                            propagationsMin[p].region = std::vector<idType>();
                            propagationsMin[p].queue = boost::heap::fibonacci_heap< std::pair<idType,idType> >();
                        }
                        propagationsMin.clear();
                    }

                    // max propagations
                    {
                        idType nPropagations = propagationsMax.size();
                        #pragma omp parallel for schedule(dynamic) num_threads(this->threadNumber_)
                        for(idType p=0; p<nPropagations; p++){
                            propagationsMax[p].region = std::vector<idType>();
                            propagationsMax[p].queue = boost::heap::fibonacci_heap< std::pair<idType,idType> >();
                        }
                        propagationsMax.clear();
                    }

                    // vectors
                    {
                        inputOffsets.clear();
                        unauthorizedExtrema.clear();
                        regionMask.clear();
                        propagationMask.clear();
                        localOffsets.clear();
                        sortedIndices.clear();
                        masterPropagationsMax.clear();
                        masterPropagationsMin.clear();
                    }

                    this->printMsg(
                        "Deallocating memory",
                        1,timer.getElapsedTime(),this->threadNumber_
                    );
                }

                this->printMsg(debug::Separator::L1);

                return 1;
            };


// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

            template<typename dataType, typename idType>
            int removeZeroPersistencePairs(
                dataType* outputScalars,
                idType* outputOffsets,

                const ttk::Triangulation* triangulation,
                const dataType* inputScalars
            ) const {
                ttk::Timer timer;

                idType nVertices = triangulation->getNumberOfVertices();

                // allocate memory
                this->printMsg(
                    "Allocating memory",
                    0,0,this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                std::vector<idType> unauthorizedExtrema(nVertices);

                std::vector<idType> localOffsets(nVertices);
                std::vector<idType> regionMask(nVertices);
                std::vector<idType> inputOffsets(nVertices);

                // std::vector<idType> inputOffsets(nVertices);
                // std::vector<Propagation<idType>*> propagationMask(nVertices);
                std::vector<std::tuple<idType,idType,idType>> sortedIndices(nVertices);
                // std::vector<Propagation<idType>> propagationsMax;
                // std::vector<Propagation<idType>*> masterPropagationsMax;
                // std::vector<Propagation<idType>> propagationsMin;
                // std::vector<Propagation<idType>*> masterPropagationsMin;
                // std::vector<idType> queueMask;


                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++){
                    outputScalars[i] = inputScalars[i];
                    inputOffsets[i] = i;
                }

                this->printMsg(
                    "Allocating memory",
                    1,timer.getElapsedTime(),this->threadNumber_
                );
                this->printMsg(debug::Separator::L2);
                timer.reStart();

                std::vector<std::tuple<dataType,idType,idType>> sortedVertices(nVertices);
                // compute initial offsets if not exisitng (currently this done by default)
                {
                    this->computeGlobalOffsets<dataType,idType>(
                        outputOffsets,
                        sortedVertices,

                        nVertices,
                        inputScalars,
                        inputOffsets.data()
                    );
                }

                idType it = 0;
                while(it<1){
                    it++;

                    // Maxima
                    {
                        // invert offsets to first remove minima (now maxima)
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++){
                            inputOffsets[v] = outputOffsets[v];
                            outputScalars[v] = outputScalars[v];
                        }
                    }

                    // identify maxima that need to be removed
                    {
                        std::vector<idType>& unauthorizedMaxima = unauthorizedExtrema;
                        unauthorizedMaxima.resize(nVertices);

                        ttk::Timer t;
                        this->printMsg(
                            "Detecting unauthorized extrema",
                            0,0,this->threadNumber_,debug::LineMode::REPLACE
                        );

                        // a synchronized write index used to store discarded maxima
                        idType writeIndex=0;

                        // find unauthorized extrema
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++){

                            // check if v has larger neighbors and if is part of a plateau
                            bool hasLargerNeighbor = false;
                            bool isAmbiguous = false;

                            const dataType& vScalar = outputScalars[v];
                            const idType& vOffset = inputOffsets[v];

                            idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                            for(idType n=0; n<nNeighbors; n++){
                                idType u;
                                triangulation->getVertexNeighbor(v,n,u);

                                if(vOffset<inputOffsets[u])
                                    hasLargerNeighbor = true;

                                if(vScalar==outputScalars[u])
                                    isAmbiguous = true;
                            }

                            // if v has larger neighbors or is not part of a plateau
                            if(hasLargerNeighbor || !isAmbiguous)
                                continue;

                            // get local write index for this thread
                            idType localWriteIndex = 0;
                            #pragma omp atomic capture
                            localWriteIndex = writeIndex++;

                            // write maximum index
                            unauthorizedMaxima[localWriteIndex] = v;
                        }

                        // resize to the actual number of discarded maxima
                        unauthorizedMaxima.resize(writeIndex);
                        this->printMsg(
                            "Detecting unauthorized extrema ("+std::to_string(writeIndex)+")",
                            1,t.getElapsedTime(),this->threadNumber_
                        );
                    }

                    // compute regions
                    std::vector<Propagation<idType>> propagations;
                    {
                        std::vector<idType>& unauthorizedMaxima = unauthorizedExtrema;

                        const idType nPropagations = unauthorizedMaxima.size();
                        propagations.resize(nPropagations);

                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++)
                            regionMask[v] = -1;

                        for(idType p=0; p<nPropagations; p++){

                            const idType& extremumIndex = unauthorizedMaxima[p];

                            if(regionMask[extremumIndex]!=-1)
                                continue;

                            propagations[p].extremumIndex = extremumIndex;

                            const dataType& plateauScalar = outputScalars[extremumIndex];

                            // compute plateau
                            auto& region = propagations[p].region;
                            std::vector<idType> queue;
                            queue.push_back(extremumIndex);
                            regionMask[extremumIndex] = extremumIndex;

                            bool plateauIsNextToLargerVertex = false;
                            std::vector<idType> maxima;

                            idType largerPlateauNeighbor = -1;

                            while(!queue.empty()){
                                idType v = queue.back();
                                queue.pop_back();

                                region.push_back(v);

                                bool hasLargerNeighbor = false;
                                idType nNeighbors = triangulation->getVertexNeighborNumber(v);
                                for(idType n=0; n<nNeighbors; n++){
                                    idType u;
                                    triangulation->getVertexNeighbor(v,n,u);
                                    if(plateauScalar==outputScalars[u] && regionMask[u]!=extremumIndex){
                                        queue.push_back(u);
                                        regionMask[u] = extremumIndex;
                                    }

                                    if(regionMask[u]!=extremumIndex){
                                        if(plateauScalar<outputScalars[u]){
                                            plateauIsNextToLargerVertex = true;
                                            largerPlateauNeighbor = u;
                                        } else {
                                            // smallerPlateauNeighbor = u;
                                        }
                                    }

                                    if(regionMask[u]==extremumIndex && inputOffsets[v]<inputOffsets[u])
                                        hasLargerNeighbor = true;
                                }

                                if(!hasLargerNeighbor)
                                    maxima.push_back(v);
                            }

                            // if(largerPlateauNeighbor==-1)
                            //     this->printErr("XXXXXX");

                            propagations[p].lastEncounteredSaddle = largerPlateauNeighbor;
                            propagations[p].regionSize = region.size();

                            // if we are at a maximum plateau leave only one extremum
                            if(!plateauIsNextToLargerVertex || largerPlateauNeighbor==-1){
                                propagations[p].extremumIndex = -1;
                                for(auto& x : region)
                                    regionMask[x] = -2;
                            }
                        }
                    }

                    std::vector<Propagation<idType>*> masterPropagations;
                    for(auto& p : propagations)
                        if(p.extremumIndex!=-1)
                            masterPropagations.push_back(&p);

                    {
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++)
                            localOffsets[v] = 1;

                        // compute local order of regions
                        this->computeLocalOffsetsOfRegions<idType>(
                            localOffsets.data(),
                            outputOffsets, // used here to temporarily store distance field

                            triangulation,
                            regionMask.data(),
                            inputOffsets.data(),
                            masterPropagations,
                            false
                        );

                        // for(size_t i=0; i<nVertices; i++)
                        //     outputOffsets[i] = localOffsets[i];
                    }

                    {
                        // compute global offsets
                        this->computeGlobalOffsets<dataType,idType>(
                            outputOffsets,
                            sortedVertices,

                            triangulation->getNumberOfVertices(),
                            outputScalars,
                            localOffsets.data()
                        );
                    }
                }





                // flood fill plateaus and find at least one maxima inside
                // std::vector<Propagation<idType>> propagations(nVertices);
                // {
                //     #pragma omp parallel for num_threads(this->threadNumber_)
                //     for(idType v=0; v<nVertices; v++){
                //         regionMask[v] = -1;
                //     }

                //     dataType pivot;
                //     #pragma omp parallel num_threads(this->threadNumber_)
                //     #pragma omp single
                //     for(idType s=0; s<nVertices; s++){

                //         if(s!=0 && pivot==std::get<0>(sortedVertices[s]))
                //             continue;
                //         else {
                //             pivot=std::get<0>(sortedVertices[s]);

                //             // spawn task
                //             #pragma omp task firstprivate(s)
                //             {
                //                 idType scalar=std::get<0>(sortedVertices[s]);

                //                 while(s<nVertices && std::get<0>(sortedVertices[s])==scalar){
                //                     const idType& seed = std::get<2>(sortedVertices[s++]);

                //                     // if the current vertex is not already part of a plateau
                //                     if(regionMask[seed]!=-1)
                //                         continue;

                //                     propagations[seed].extremumIndex = seed;

                //                     // check if seed is part of a plateau
                //                     bool isAmbiguous = false;
                //                     idType nNeighbors = triangulation->getVertexNeighborNumber( seed );
                //                     for(idType n=0; n<nNeighbors; n++){
                //                         idType u;
                //                         triangulation->getVertexNeighbor(seed,n,u);

                //                         if( inputScalars[u]==scalar )
                //                             isAmbiguous = true;
                //                     }

                //                     // if yes start floodfill and find positive and negative boundary
                //                     if(isAmbiguous){
                //                         std::vector<idType>& region = propagations[seed].region;
                //                         std::vector<idType> queue;

                //                         queue.push_back(seed);
                //                         regionMask[seed] = seed;

                //                         while(!queue.empty()){
                //                             idType v = queue.back();
                //                             queue.pop_back();

                //                             region.push_back(v);

                //                             idType nNeighbors = triangulation->getVertexNeighborNumber(v);
                //                             for(idType n=0; n<nNeighbors; n++){
                //                                 idType u;
                //                                 triangulation->getVertexNeighbor(v,n,u);
                //                                 if(scalar==inputScalars[u] && regionMask[u]==-1){
                //                                     queue.push_back(u);
                //                                     regionMask[u] = seed;
                //                                 }
                //                             }
                //                         }

                //                         std::cout<<(" "+std::to_string(region.size())+" \n");
                //                     }
                //                 }
                //             }
                //         }
                //     }
                // }









                // compute local order of propagations
                {

                }


                // {
                //     // Minima
                //     {
                //         // invert offsets to first remove minima (now maxima)
                //         #pragma omp parallel for num_threads(this->threadNumber_)
                //         for(idType v=0; v<nVertices; v++)
                //             inputOffsets[v] = -outputOffsets[v];
                //     }

                //     // identify maxima that need to be removed
                //     {
                //         ttk::Timer t;
                //         this->printMsg("Detecting authorized extrema",0,0,this->threadNumber_,debug::LineMode::REPLACE);

                //         // a synchronized write index used to store discarded maxima
                //         idType writeIndex=0;

                //         // find authorized extrema
                //         #pragma omp parallel for num_threads(this->threadNumber_)
                //         for(idType v=0; v<nVertices; v++){

                //             // check if v has larger neighbors and if is part of a plateau
                //             bool hasLargerNeighbor = false;
                //             bool isAmbiguous = false;

                //             const dataType& vScalar = inputScalars[v];
                //             const dataType& vOffset = inputOffsets[v];

                //             idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                //             for(idType n=0; n<nNeighbors; n++){
                //                 idType u;
                //                 triangulation->getVertexNeighbor(v,n,u);

                //                 if( vOffset<inputOffsets[u] )
                //                     hasLargerNeighbor = true;

                //                 if( vScalar==inputScalars[u] )
                //                     isAmbiguous = true;
                //             }

                //             // if v has larger neighbors then v can not be maximum
                //             if(hasLargerNeighbor || !isAmbiguous)
                //                 continue;

                //             // get local write index for this thread
                //             idType localWriteIndex = 0;
                //             #pragma omp atomic capture
                //             {
                //                 localWriteIndex = maximaWriteIndex;
                //                 maximaWriteIndex += 1;
                //             }

                //             // write maximum index
                //             unauthorizedMaxima[localWriteIndex] = v;
                //         }

                //         // resize to the actual number of discarded maxima
                //         unauthorizedMaxima.resize(maximaWriteIndex);
                //         this->printMsg(
                //             "Detecting authorized extrema ("+std::to_string(maximaWriteIndex)+")",
                //             1,t.getElapsedTime(),this->threadNumber_
                //         );
                //     }

                    // // determine region
                    // sstd::vector<std::vector<idType>> regions;

                    // {
                    //     std::vector<idType>& unauthorizedMaxima = unauthorizedExtrema;
                    //     idType nPropagations = unauthorizedMaxima.size();

                    //     ttk::Timer t;
                    //     this->printMsg(
                    //         "Computing regions ("+std::to_string(nPropagations)+")",
                    //         0,0,this->threadNumber_,debug::LineMode::REPLACE
                    //     );

                    //     #pragma omp parallel for num_threads(this->threadNumber_)
                    //     for(idType v=0; v<nVertices; v++){
                    //         regionMask[v] = -1;
                    //     }

                    //     size_t count = 0;
                    //     for(idType p=0; p<nPropagations; p++){
                    //         const idType& extremumIndex = unauthorizedMaxima[p];

                    //         if(regionMask[extremumIndex]!=-1)
                    //             continue;

                    //         regions.resize( regions.size() + 1 );
                    //         std::vector<idType>& region = regions.back();

                    //         const dataType& plateauValue = inputScalars[extremumIndex];

                    //         // collect region
                    //         std::vector<idType> queue;

                    //         queue.push_back(extremumIndex);
                    //         while(!queue.empty()){
                    //             idType v = queue.back();
                    //             queue.pop_back();

                    //             region.push_back(v);

                    //             idType nNeighbors = triangulation->getVertexNeighborNumber(v);
                    //             for(idType n=0; n<nNeighbors; n++){
                    //                 idType u;
                    //                 triangulation->getVertexNeighbor(v,n,u);
                    //                 if(plateauValue==inputScalars[u] && regionMask[u]<extremumIndex){
                    //                     queue.push_back(u);
                    //                     regionMask[u] = extremumIndex;
                    //                 }
                    //             }
                    //         }

                    //         count++;
                    //     }
                    //     this->printMsg(
                    //         "Computing regions ("+std::to_string(count)+")",
                    //         1,t.getElapsedTime(),this->threadNumber_
                    //     );

                    //     for(size_t i=0; i<nVertices; i++)
                    //         outputOffsets[i] = regionMask[i];
                    // }

                    // // compute local offsets
                    // {
                    //     #pragma omp parallel for num_threads(this->threadNumber_)
                    //     for(idType v=0; v<nVertices; v++)
                    //         localOffsets[v] = 1;

                    //     this->computeLocalOffsetsOfRegion<idType>(
                    //         localOffsets,
                    //         distanceField,

                    //         propagation,
                    //         triangulation,
                    //         regionMask,
                    //         inputOffsets,
                    //         useRegionBasedIterations
                    //     );
                    // }

                    // // compute global offsets
                    // {

                    // }
                // }

                return 1;
            }

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%



            int PreconditionTriangulation(
                ttk::Triangulation* triangulation
            ) const;

            template <class dataType>
            int SortVertexIdsByScalarAndPlateauId(
                ttkInt* sortedVertexIds,

                const ttkInt* offsetScalarField,
                const size_t& nVertices,
                const dataType* scalars
            ) const;

            template <class dataType>
            int IdentifyPlateaus(
                ttkInt* offsetScalarField,
                std::vector<std::unordered_set<ttkInt>>& plateauSmallerNeighbors,
                std::vector<std::unordered_set<ttkInt>>& plateauLargerNeighbors,
                std::vector<std::unordered_set<ttkInt>>& plateauBoundaries,

                const size_t& nVertices,
                const dataType* scalars,
                const ttk::Triangulation* triangulation
            ) const;

            int ComputePlateauDistanceField(
                float* distanceField,

                const ttkInt* offsetScalarField,
                const ttkInt& plateauId,
                const ttk::Triangulation* triangulation,
                const std::unordered_set<ttkInt>& seedVertices
            ) const;

            int FillPlateau(
                float* distanceField,
                ttkInt* sortedVertexIds,
                ttkInt* offsetScalarField,

                const ttkInt& plateauId,
                const size_t& plateauStartIndex,
                const ttk::Triangulation* triangulation,
                const std::unordered_set<ttkInt>& seedVertices
            ) const;

            int DisambiguatePlateau(
                float* distanceField,
                ttkInt* sortedVertexIds,
                ttkInt* offsetScalarField,

                const size_t& nVertices,
                const size_t& plateauIndex0,
                const size_t& plateauIndexN,
                const ttk::Triangulation* triangulation,
                const std::unordered_set<ttkInt>& smallerNeighbors,
                const std::unordered_set<ttkInt>& largerNeighbors,
                const std::unordered_set<ttkInt>& boundary
            ) const;

            template <class dataType> int DisambiguateAllPlateaus(
                ttkInt* offsetScalarField, // maps vertexId to index in sorted field
                float* shortestPaths,
                ttkInt* iterations,

                const size_t& nVertices,
                const ttk::Triangulation* triangulation,
                const dataType* scalars
            ) const;

        private:

    };
}

















int ttk::Disambiguate::PreconditionTriangulation(
    ttk::Triangulation* triangulation
) const {
    triangulation->preconditionVertexNeighbors();
    triangulation->preconditionBoundaryVertices();
    return 0;
}

template <class dataType>
int ttk::Disambiguate::SortVertexIdsByScalarAndPlateauId(
    ttkInt* sortedVertexIds,

    const ttkInt* offsetScalarField,
    const size_t& nVertices,
    const dataType* scalars
) const {
    // init array
    for(size_t i=0; i<nVertices; i++)
        sortedVertexIds[i] = i;

    // init comparator
    struct Comparator {
        const dataType* scalars_;
        const ttkInt* offsetScalarField_;
        int operator() (const ttkInt& i, const ttkInt& j){
            const dataType& sI = scalars_[i];
            const dataType& sJ = scalars_[j];
            return sI==sJ ? offsetScalarField_[i]<offsetScalarField_[j] : sI<sJ;
        }
    };
    Comparator comparator;
    comparator.scalars_ = scalars;
    comparator.offsetScalarField_ = offsetScalarField;

    // sort vertex ids
    std::sort(
        sortedVertexIds,
        sortedVertexIds+nVertices,
        comparator
    );

    return 1;
}

template <class dataType>
int ttk::Disambiguate::IdentifyPlateaus(
    ttkInt* offsetScalarField,
    std::vector<std::unordered_set<ttkInt>>& plateauSmallerNeighbors,
    std::vector<std::unordered_set<ttkInt>>& plateauLargerNeighbors,
    std::vector<std::unordered_set<ttkInt>>& plateauBoundaries,

    const size_t& nVertices,
    const dataType* scalars,
    const ttk::Triangulation* triangulation
) const {
    // init offsets (used as temporary storage of plateauIds)
    for(size_t i=0; i<nVertices; i++)
        offsetScalarField[i] = 0;

    ttkInt plateauId = 0;

    // iterate over vertices and search for plateaus
    for(size_t v=0; v<nVertices; v++){

        // get scalar value of current vertex
        const dataType& vScalar = scalars[v];

        // if already has plateau id skip
        if(offsetScalarField[v]<0) continue;

        // determine if vertex is ambiguous
        bool ambiguous = false;
        {
            size_t nNeighbors = triangulation->getVertexNeighborNumber( v );
            ttkInt u;
            for(size_t i=0; i<nNeighbors; i++){
                triangulation->getVertexNeighbor(v,i,u);
                if( vScalar == scalars[u] ){
                    ambiguous = true;
                    break;
                }
            }
        }

        // if ambiguous mark plateau and extract boundary regions
        if(ambiguous){
            plateauId--;
            plateauSmallerNeighbors.resize(plateauSmallerNeighbors.size()+1);
            plateauLargerNeighbors.resize(plateauLargerNeighbors.size()+1);
            plateauBoundaries.resize(plateauBoundaries.size()+1);

            auto& smallerNeighbors = plateauSmallerNeighbors.back();
            auto& largerNeighbors = plateauLargerNeighbors.back();
            auto& boundary = plateauBoundaries.back();

            // only vertices with
            //     + the same scalar as v and
            //     + which not yet have been processed
            // are added to the stack
            std::vector<ttkInt> stack(1,v);

            while(stack.size()){
                // pop vertex from stack
                const ttkInt a = stack.back();
                stack.pop_back();

                // if(boundary.size()<1 && triangulation->isVertexOnBoundary(a))
                if(triangulation->isVertexOnBoundary(a))
                    boundary.insert(a);

                // check neighbors for seed candidates
                ttkInt b;
                const size_t nNeighbors = triangulation->getVertexNeighborNumber( a );
                for(size_t i=0; i<nNeighbors; i++){
                    triangulation->getVertexNeighbor(a,i,b);
                    const dataType& bScalar = scalars[b];
                    if(bScalar<vScalar)
                        smallerNeighbors.insert(b);
                    else if (bScalar>vScalar)
                        largerNeighbors.insert(b);
                    else if(offsetScalarField[b]>=0){
                        // mark every vertex inside plateau with negative id
                        offsetScalarField[b] = plateauId;
                        stack.push_back(b);
                    }
                }
            }
        }
    }

    return 1;
}

int ttk::Disambiguate::ComputePlateauDistanceField(
    float* distanceField,

    const ttkInt* offsetScalarField,
    const ttkInt& plateauId,
    const ttk::Triangulation* triangulation,
    const std::unordered_set<ttkInt>& seedVertices
) const {
    ttkInt plateauIdAsStoredInOffsetScalarField = -plateauId-1;

    // init comparator
    struct Comparator {
        const float* distanceField_;
        int operator() (const ttkInt& i, const ttkInt& j){
            return distanceField_[i]>distanceField_[j];
        }
    };
    Comparator comparator;
    comparator.distanceField_ = distanceField;

    // init priority queue
    std::priority_queue<
        ttkInt,
        std::vector<ttkInt>,
        Comparator
    > queue(comparator);

    // add all vertices inside plateau next to seed vertices to queue
    {
        ttkInt u;
        for(const auto& v : seedVertices ){
            // check neighbors for seed candidates
            const size_t nNeighbors = triangulation->getVertexNeighborNumber( v );
            for(size_t i=0; i<nNeighbors; i++){
                triangulation->getVertexNeighbor(v,i,u);

                // if neighbor is inside plateau and has not been added yet
                if(distanceField[u]!=0 && offsetScalarField[u]==plateauIdAsStoredInOffsetScalarField){
                    // init distance and use as marker
                    distanceField[u]=0;
                    // add to queue
                    queue.push( u );
                }
            }
        }
    }

    // compute distance field
    {
        ttkInt v,u;
        while(!queue.empty()){
            v = queue.top();
            queue.pop();

            size_t nNeighbors = triangulation->getVertexNeighborNumber( v );

            for(size_t i=0; i<nNeighbors; i++){
                triangulation->getVertexNeighbor(v,i,u);
                if(offsetScalarField[u]!=plateauIdAsStoredInOffsetScalarField)
                    continue;

                float alt = distanceField[v] + 1;
                if(alt<distanceField[u]){
                    distanceField[u] = alt;
                    queue.push( u );
                }
            }
        }
    }

    return 1;
}

int ttk::Disambiguate::FillPlateau(
    float* distanceField,
    ttkInt* sortedVertexIds,
    ttkInt* offsetScalarField,

    const ttkInt& plateauId,
    const size_t& plateauStartIndex,
    const ttk::Triangulation* triangulation,
    const std::unordered_set<ttkInt>& seedVertices
) const {
    ttkInt plateauIdAsStoredInOffsetScalarField = -plateauId-1;

    // auto willDisconnect = [](
    //     const ttkInt& u,
    //     const ttkInt& plateauIdAsStoredInOffsetScalarField,
    //     const ttk::Triangulation* triangulation,
    //     const ttkInt* offsetScalarField
    // ){
    //     ttkInt edgeId, neighborId;
    //     const size_t nLinks = triangulation->getVertexLinkNumber( u );
    //     const size_t nNeighbors = triangulation->getVertexNeighborNumber( u );

    //     std::unordered_map<ttkInt,ttkInt> vertexIdToSetId;
    //     for(size_t i=0; i<nNeighbors; i++){
    //         triangulation->getVertexNeighbor(u,i,neighborId);
    //         // if vertex is currently unprocessed
    //         if(offsetScalarField[neighborId]==plateauIdAsStoredInOffsetScalarField)
    //             vertexIdToSetId.insert({neighborId,neighborId});
    //     }

    //     // union adjacent unprocessed vertices into one set
    //     ttkInt a,b;
    //     for(size_t i=0; i<nLinks; i++){
    //         triangulation->getVertexLink(u,i,edgeId);

    //         triangulation->getEdgeVertex(edgeId,0,a);
    //         triangulation->getEdgeVertex(edgeId,1,b);

    //         // if edge connects two unprocessed vertices
    //         if(
    //               offsetScalarField[a]==plateauIdAsStoredInOffsetScalarField
    //             && offsetScalarField[b]==plateauIdAsStoredInOffsetScalarField
    //         ) {
    //             // union sets
    //             ttkInt minId = std::min(a, b);
    //             for(auto& it: vertexIdToSetId){
    //                 if(it.second==a || it.second==b)
    //                     it.second = minId;
    //             }
    //         }
    //     }

    //     std::unordered_set<ttkInt> uniqueKeys;
    //     // determine number of components
    //     for(const auto& it: vertexIdToSetId)
    //         uniqueKeys.insert(it.second);

    //     if(u==2937){
    //         std::cout<<std::endl;
    //         std::cout<<"2937: "<<uniqueKeys.size()<<std::endl;
    //         for(auto& it: vertexIdToSetId)
    //             std::cout<<it.first<<"->"<<it.second<<std::endl;
    //     }

    //     return uniqueKeys.size()!=1;
    // };

    // // std::vector<ttkInt> candidates(seedVertices.size());
    // std::unordered_set<ttkInt> candidates;
    // // std::vector<ttkInt> candidates(seedVertices.size());
    // {
    //     ttkInt u;
    //     for(const auto& v : seedVertices ){
    //         // check neighbors for seed candidates
    //         const size_t nNeighbors = triangulation->getVertexNeighborNumber( v );
    //         for(size_t i=0; i<nNeighbors; i++){
    //             triangulation->getVertexNeighbor(v,i,u);

    //             // if vertex is inside plateau
    //             if(offsetScalarField[u]==plateauIdAsStoredInOffsetScalarField){
    //                 // add to candidates
    //                 candidates.insert( u );
    //                 // mark as candidate
    //                 offsetScalarField[u]=0;
    //             }
    //         }
    //     }
    // }

    // size_t sequenceIndex = plateauStartIndex;
    // while(candidates.size()){
    //     // search for first candidate that does not diconnect inverse component
    //     ttkInt v = -1;

    //     for(const auto& u : candidates){
    //         if(
    //             !willDisconnect(
    //                 u,
    //                 plateauIdAsStoredInOffsetScalarField,
    //                 triangulation,
    //                 offsetScalarField
    //             )
    //         ){
    //             v = u;
    //             break;
    //         }
    //     }

    //     // if we have to add a saddle just take first candiate
    //     if(v==-1)
    //         v = *candidates.begin();

    //     sortedVertexIds[sequenceIndex++] = v;
    //     // mark as processed
    //     offsetScalarField[v]=1;

    //     // delete from candidates
    //     candidates.erase(v);

    //     // add new candidates
    //     {
    //         ttkInt u;
    //         const size_t nNeighbors = triangulation->getVertexNeighborNumber( v );
    //         for(size_t i=0; i<nNeighbors; i++){
    //             triangulation->getVertexNeighbor(v,i,u);

    //             // if vertex is inside plateau
    //             if(offsetScalarField[u]==plateauIdAsStoredInOffsetScalarField){
    //                 // add to candidates
    //                 candidates.insert( u );
    //                 // mark as candidate
    //                 offsetScalarField[u]=0;
    //             }
    //         }
    //     }
    // }

    // -------------------------------------------------------------------------

    // init comparator
    struct Comparator {
        const float* distanceField_;
        int operator() (const ttkInt& i, const ttkInt& j){
            return distanceField_[i]<distanceField_[j];
        }
    };
    Comparator comparator;
    comparator.distanceField_ = distanceField;

    // init priority queue
    std::priority_queue<
        ttkInt,
        std::vector<ttkInt>,
        Comparator
    > queue(comparator);

    // add seed candidate vertex with largest distance to queue
    {
        ttkInt maxCandidate;
        float maxDistance = -1;

        for(const auto& v : seedVertices ){
            if(maxDistance<distanceField[v]){
                maxDistance = distanceField[v];
                maxCandidate = v;
            }
        }
        // add to queue
        queue.push( maxCandidate );
        // mark as added
        offsetScalarField[maxCandidate]=0;
    }
    // // add all vertices inside plateau next to seed vertices to queue
    // {
    //     ttkInt u;
    //     for(const auto& v : seedVertices ){
    //         // check neighbors for seed candidates
    //         const size_t nNeighbors = triangulation->getVertexNeighborNumber( v );
    //         for(size_t i=0; i<nNeighbors; i++){
    //             triangulation->getVertexNeighbor(v,i,u);

    //             // if vertex is inside plateau
    //             if(offsetScalarField[u]==plateauIdAsStoredInOffsetScalarField){
    //                 // add to queue
    //                 queue.push( u );
    //                 // mark as added
    //                 offsetScalarField[u]=0;
    //             }
    //         }
    //     }
    // }

    {
        size_t sequenceIndex = plateauStartIndex;
        ttkInt v,u;
        while(!queue.empty()){
            v = queue.top();
            queue.pop();

            sortedVertexIds[sequenceIndex++] = v;

            // add neighbors
            size_t nNeighbors = triangulation->getVertexNeighborNumber( v );
            for(size_t i=0; i<nNeighbors; i++){
                triangulation->getVertexNeighbor(v,i,u);
                if(offsetScalarField[u]==plateauIdAsStoredInOffsetScalarField){
                    // add to queue
                    queue.push( u );
                    // mark as added
                    offsetScalarField[u]=0;
                }
            }
        }
    }

    return 1;
}

int ttk::Disambiguate::DisambiguatePlateau(
    float* distanceField,
    ttkInt* sortedVertexIds,
    ttkInt* offsetScalarField,

    const size_t& nVertices,
    const size_t& plateauIndex0,
    const size_t& plateauIndexN,
    const ttk::Triangulation* triangulation,
    const std::unordered_set<ttkInt>& smallerNeighbors,
    const std::unordered_set<ttkInt>& largerNeighbors,
    const std::unordered_set<ttkInt>& boundary
) const {
    ttkInt plateauIdAsStoredInOffsetScalarField = offsetScalarField[
        sortedVertexIds[plateauIndex0]
    ];
    ttkInt plateauId = -plateauIdAsStoredInOffsetScalarField-1;

    std::unordered_set<ttkInt> plateauVertices;
    {
        for(size_t i=0; i<nVertices; i++){
            if(offsetScalarField[i]==plateauIdAsStoredInOffsetScalarField){
                plateauVertices.insert(i);
                // offsetScalarField[i] = plateauId;
            }
        }
    }

    // determine type
    // size_t type = (smallerNeighbors.size()>0 && largerNeighbors.size()>0)
    //     ? 0 // regular (possible saddle)
    //     : (smallerNeighbors.size()<1 && largerNeighbors.size()<1)
    //         ? 3 // plateau is isolated (e.g., entire region)
    //         : smallerNeighbors.size()>0
    //             ? 1 // maximum region
    //             : 2 // minimum region
    // ;

    // this->printMsg("Disambiguating region ["+std::to_string(plateauId)+", "+std::to_string(type)+", "+std::to_string(plateauIndex0)+" - "+std::to_string(plateauIndexN)+"]");

    // std::unordered_set<ttkInt> singleSmallerNeighbor;
    // if(smallerNeighbors.size()>0)
    //     singleSmallerNeighbor.insert( *smallerNeighbors.begin() );

    // std::unordered_set<ttkInt> singleLargerNeighbor;
    // if(largerNeighbors.size()>0)
    //     singleLargerNeighbor.insert( *largerNeighbors.begin() );

    // std::unordered_set<ttkInt> isolated;
    // isolated.insert( 0 );
    std::unordered_set<ttkInt> randomVertex;
    randomVertex.insert( plateauIndex0 );

    this->printMsg("P["+std::to_string(plateauId)+"] "+std::to_string(plateauVertices.size())+" "+std::to_string(smallerNeighbors.size())+" "+std::to_string(largerNeighbors.size())+" "+std::to_string(boundary.size()));

    this->ComputePlateauDistanceField(
        distanceField,

        offsetScalarField,
        plateauId,
        triangulation,

        largerNeighbors.size()>0
            ? largerNeighbors
            : smallerNeighbors.size()>0
                ? smallerNeighbors
                : randomVertex
        // type==0
        //     ? largerNeighbors
        //     : type==1
        //         ? singleSmallerNeighbor
        //         : type==2
        //             ? singleLargerNeighbor
        //             : isolated
    );

    // std::cout<<std::endl;
    // for(size_t i=i0; i<iN; i++)
    //     std::cout<<sortedVertexIds[i]<<" ";
    // std::cout<<std::endl;
    // if(type==2){
    //     std::cout<<std::endl;
    //     std::cout<<"xxxxxxxxxxxxxxx"<<std::endl;
    //     if(singleLargerNeighbor.size()>0)
    //         std::cout<<"------> "<<(*singleLargerNeighbor.begin())<<std::endl;
    //     else
    //     std::cout<<"WTF"<<std::endl;
    // }

    // if(type==1){
    //     for(const auto& it : boundary)

    // }

    this->FillPlateau(
        distanceField,
        sortedVertexIds,
        offsetScalarField,

        plateauId,
        plateauIndex0,
        triangulation,
        plateauVertices
        // largerNeighbors.size()<1 // nothing to grow towards
        //     ? plateauVertices
        //     : smallerNeighbors.size()>0
        //         ? plateauVertices
        //         : boundary

        // type==0
        //     ? smallerNeighbors
        //     : type==1
        //         ? (boundary.size() ? boundary : smallerNeighbors)
        //         : type==2
        //             ? (boundary.size() ? boundary : largerNeighbors)
        //             : isolated
    );

    // if necessary invert order
    if(largerNeighbors.size()<1){
        int i = plateauIndex0;
        int j = plateauIndex0+plateauVertices.size()-1;
        while(i<j){
            const auto temp = sortedVertexIds[i];
            sortedVertexIds[i] = sortedVertexIds[j];
            sortedVertexIds[j] = temp;
            i++;
            j--;
        }
    }

    // std::cout<<std::endl;
    // for(size_t i=i0; i<iN; i++)
    //     std::cout<<sortedVertexIds[i]<<" ";
    // std::cout<<std::endl;

    return 1;
}

/**
 * 1. Sort all vertex indicies by scalar value
 * 2. Iterate over vertices in scalar order
 *      + If scalar value different then last last iteration
 *          - Sort last subset based on shortest path
 *      + For each vertex that is part of an unprocessed flat plateau:
 *          - Find largest vertex neighbor v of the plateau
 *          - Compute shortest path from all vertices inside plateau to v
*/
template <class dataType>
int ttk::Disambiguate::DisambiguateAllPlateaus(
    ttkInt* offsetScalarField, // maps vertexId to index in sorted field
    float* shortestPaths,
    ttkInt* iterations,

    const size_t& nVertices,
    const ttk::Triangulation* triangulation,
    const dataType* scalars
) const {
    ttk::Timer timer;

    this->printMsg(
        "Disambiguating PL scalar field",
        0,
        ttk::debug::LineMode::REPLACE
    );

    // Init data structures
    std::vector<float> distanceField(
        nVertices,
        std::numeric_limits<float>::infinity()
    );
    std::vector<ttkInt> sortedVertexIds(nVertices, 0);

    // Identify plateaus and their boundary regions (plateaus ids are stored as negative numbers in the distanceField)
    std::vector<std::unordered_set<ttkInt>> plateauSmallerNeighbors;
    std::vector<std::unordered_set<ttkInt>> plateauLargerNeighbors;
    std::vector<std::unordered_set<ttkInt>> plateauBoundaries;
    this->IdentifyPlateaus<dataType>(
        offsetScalarField,
        plateauSmallerNeighbors,
        plateauLargerNeighbors,
        plateauBoundaries,

        nVertices,
        scalars,
        triangulation
    );
    size_t nPlateaus = plateauBoundaries.size();

    // sort vertices based on scalar and plateau id
    this->SortVertexIdsByScalarAndPlateauId<dataType>(
        sortedVertexIds.data(),

        offsetScalarField,
        nVertices,
        scalars
    );

    std::vector<size_t> plateauIntervals(nPlateaus*2);
    {
        size_t q=0;
        // check if first vertex is interval boundary
        if(offsetScalarField[sortedVertexIds[0]]<0)
            plateauIntervals[q++] = 0;

        // check intermediate interval boundaries
        for(size_t i=1; i<nVertices; i++){
            const auto& plateauIdP = offsetScalarField[sortedVertexIds[i-1]];
            const auto& plateauIdC = offsetScalarField[sortedVertexIds[i]];
            if(plateauIdP!=plateauIdC){
                if(plateauIdP<0)
                    plateauIntervals[q++] = i;
                if(plateauIdC<0)
                    plateauIntervals[q++] = i;
            }
        }

        // check if last vertex is interval boundary
        if(offsetScalarField[sortedVertexIds[nVertices-1]]<0)
            plateauIntervals[q++] = nVertices;
    }

    #ifdef TTK_ENABLE_OPENMP
    #pragma omp parallel for num_threads(threadNumber_)
    #endif
    for(size_t i=0; i<nPlateaus; i++){
        ttkInt plateauId = -offsetScalarField[
            sortedVertexIds[
                plateauIntervals[i*2]
            ]
        ]-1;
        this->DisambiguatePlateau(
            distanceField.data(),
            sortedVertexIds.data(),
            offsetScalarField,

            nVertices,
            plateauIntervals[i*2],
            plateauIntervals[i*2+1],
            triangulation,
            plateauSmallerNeighbors[plateauId],
            plateauLargerNeighbors[plateauId],
            plateauBoundaries[plateauId]
        );
    }

    // // update offsetScalarField
    for(size_t i=0; i<nVertices; i++)
        offsetScalarField[ sortedVertexIds[i] ] = i;

    for(size_t i=0; i<nVertices; i++)
        shortestPaths[i] = distanceField[i];

    // // Initialize data structures
    // std::vector<ttkInt> floodFillOrder(nVertices, -999999);

    // std::vector<ttkInt> sortedScalarField(nVertices, 0); // corresponds to sorted list of vertexIds based on scalars
    // const float fInfinity = std::numeric_limits<float>::infinity();
    // // const float fInfinity = 40000;
    // for(size_t i=0; i<nVertices; i++){
    //     shortestPaths[i] = fInfinity;
    //     iterations[i] = -1;
    //     sortedScalarField[i] = i;
    // }

    // // initially sort vertexIds based on original scalar values in ascending order
    // {
    //     struct ScalarComparator {
    //         const dataType* scalars_;
    //         int operator() (const ttkInt& i, const ttkInt& j){
    //             return scalars_[i]<scalars_[j];
    //         }
    //     };

    //     ScalarComparator scalarComparator;
    //     scalarComparator.scalars_ = scalars;

    //     std::sort( sortedScalarField.begin(), sortedScalarField.end(), scalarComparator );

    //     // update offsetScalarField
    //     for(size_t i=0; i<nVertices; i++)
    //         offsetScalarField[ sortedScalarField[i] ] = i;
    // }

    // // iterate over each vertex and disambiguate if necessary
    // size_t nAmbiguateCases=0;
    // {
    //     // init comparators
    //     struct ComparatorShortestPath {
    //         const float* shortestPaths_;
    //         int operator() (const ttkInt& i, const ttkInt& j){
    //             // const auto& fI = shortestPaths_[i];
    //             // const auto& fJ = shortestPaths_[j];
    //             return shortestPaths_[i]>shortestPaths_[j];
    //         }
    //     };
    //     ComparatorShortestPath comparatorShortestPath;
    //     comparatorShortestPath.shortestPaths_ = shortestPaths;

    //     struct ComparatorShortestPath2 {
    //         const float* shortestPaths_;
    //         int operator() (const ttkInt& i, const ttkInt& j){
    //             return shortestPaths_[i]<shortestPaths_[j];
    //         }
    //     };
    //     ComparatorShortestPath2 comparatorShortestPath2;
    //     comparatorShortestPath2.shortestPaths_ = shortestPaths;

    //     struct ComparatorFloodFill {
    //         const ttkInt* floodFillOrder_;
    //         int operator() (const ttkInt& i, const ttkInt& j){
    //             return floodFillOrder_[i]<floodFillOrder_[j];
    //         }
    //     };
    //     ComparatorFloodFill comparatorFloodFill;
    //     comparatorFloodFill.floodFillOrder_ = floodFillOrder.data();

    //     // iterate over vertices ordered by scalar value and resolve ambiguate cases
    //     dataType previousScalar = scalars[ sortedScalarField[0] ];
    //     bool previousScalarWasAmbigeous = false;
    //     ttkInt lastIndexUntilSorted = 0;
    //     for(size_t s=0; s<nVertices; s++){
    //         // get vertexId in sorted order
    //         const ttkInt& v = sortedScalarField[s];

    //         // get scalar value of current vertex
    //         const dataType& vScalar = scalars[v];

    //         if(vScalar!=previousScalar){
    //             previousScalar = vScalar;
    //             if(previousScalarWasAmbigeous){
    //                 // this->printMsg("Sort from "+std::to_string(lastIndexUntilSorted)+" to " + std::to_string(s));

    //                 std::sort(
    //                     sortedScalarField.begin()+lastIndexUntilSorted,
    //                     sortedScalarField.begin()+s,
    //                     comparatorFloodFill
    //                 );
    //                 for(size_t i=lastIndexUntilSorted; i<s; i++)
    //                     offsetScalarField[ sortedScalarField[i] ] = i;
    //             }
    //             lastIndexUntilSorted = s;
    //             previousScalarWasAmbigeous = false;
    //         }

    //         // if already disambiguated skip
    //         if(shortestPaths[v]!=fInfinity) continue;

    //         // determine if vertex is ambiguous
    //         bool ambiguous = false;
    //         {
    //             size_t nNeighbors = triangulation->getVertexNeighborNumber( v );
    //             ttkInt u;
    //             for(size_t i=0; i<nNeighbors; i++){
    //                 triangulation->getVertexNeighbor(v,i,u);
    //                 if( vScalar == scalars[u] ){
    //                     ambiguous = true;
    //                     break;
    //                 }
    //             }
    //         }

    //         // if ambiguous
    //         if(ambiguous){
    //             // plateau Ids
    //             std::vector<ttkInt> plateau;

    //             // search for vertex inside plateau that is next to a vertex with larger scalar
    //             ttkInt maxSeedId = -1;
    //             {
    //                 // only vertices with
    //                 //     + the same scalar as v and
    //                 //     + which not yet have been processed
    //                 // are added to the stack
    //                 std::vector<ttkInt> stack(1,v);
    //                 ttkInt b;

    //                 while(stack.size()){
    //                     // pop vertex from stack
    //                     const ttkInt a = stack.back();
    //                     plateau.push_back(a);
    //                     stack.pop_back();

    //                     // check neighbors for seed candidates
    //                     {
    //                         const size_t nNeighbors = triangulation->getVertexNeighborNumber( a );
    //                         for(size_t i=0; i<nNeighbors; i++){
    //                             triangulation->getVertexNeighbor(a,i,b);
    //                             const dataType& bScalar = scalars[b];
    //                             if(bScalar>vScalar)
    //                                 maxSeedId = a;
    //                             else if(bScalar==vScalar && offsetScalarField[b]>=0){
    //                                 offsetScalarField[b] = -offsetScalarField[b];
    //                                 stack.push_back(b);
    //                             }
    //                         }
    //                     }

    //                 }

    //                 if(maxSeedId<0){
    //                     this->printMsg("Warning: No max neighbor found");
    //                     maxSeedId = v;
    //                 }
    //             }

    //             // start shortest path computation from maxSeedId inside plateau
    //             {
    //                 // init priority queue
    //                 std::priority_queue<
    //                     ttkInt,
    //                     std::vector<ttkInt>,
    //                     ComparatorShortestPath
    //                 > queue(comparatorShortestPath);

    //                 shortestPaths[maxSeedId] = 0;
    //                 queue.push( maxSeedId );

    //                 ttkInt a,b;
    //                 while(!queue.empty()){
    //                     a = queue.top();
    //                     queue.pop();

    //                     iterations[a] = nAmbiguateCases;
    //                     size_t nNeighbors = triangulation->getVertexNeighborNumber( a );

    //                     for(size_t i=0; i<nNeighbors; i++){
    //                         triangulation->getVertexNeighbor(a,i,b);
    //                         if(vScalar!=scalars[b])
    //                             continue;

    //                         float alt = shortestPaths[a] + 1;
    //                         if(alt<shortestPaths[b]){
    //                             shortestPaths[b] = alt;
    //                             queue.push( b );
    //                         }
    //                     }

    //                 }
    //             }

    //             // find vertex most distant from maxSeedId that is also next to a vertex with smaller value than the plateau
    //             ttkInt minSeedId = plateau[plateau.size()-1];
    //             {
    //                 float maxDistance = shortestPaths[ minSeedId ];
    //                 ttkInt a,b;
    //                 for(size_t p=0, n=plateau.size()-1; p<n; p++){
    //                     a = plateau[p];
    //                     const size_t nNeighbors = triangulation->getVertexNeighborNumber( a );

    //                     // if we found a vertex that is furhter away
    //                     if(maxDistance<shortestPaths[a]){

    //                         // check if it is next to a vertex with smaller value
    //                         bool hasSmallerNeighbor = false;
    //                         for(size_t i=0; i<nNeighbors; i++){
    //                             triangulation->getVertexNeighbor(a,i,b);
    //                             if(scalars[b]<vScalar){
    //                                 hasSmallerNeighbor = true;
    //                                 break;
    //                             }
    //                         }

    //                         // if yes then update candidate
    //                         if(hasSmallerNeighbor){
    //                             maxDistance = shortestPaths[a];
    //                             minSeedId = a;
    //                         }
    //                     }
    //                 }
    //             }

    //             // this->printMsg( std::to_string(nAmbiguateCases) + ": " + std::to_string(v) + " -> " + std::to_string(minSeedId) + " -> " + std::to_string(maxSeedId) );

    //             // std::cout<<std::endl;
    //             // for(size_t i=0; i<plateau.size(); i++)
    //             //     std::cout<< plateau[i]<< " ";
    //             // std::cout<<std::endl;

    //             // for(size_t i=0; i<nVertices; i++)
    //             //     if(offsetScalarField[i]<0) offsetScalarField[i]*=-1;

    //             // start flood fill from minSeedId
    //             {

    //                 // init priority queue
    //                 std::priority_queue<
    //                     ttkInt,
    //                     std::vector<ttkInt>,
    //                     ComparatorShortestPath2
    //                 > queue(comparatorShortestPath2);

    //                 offsetScalarField[minSeedId] = -offsetScalarField[minSeedId];
    //                 queue.push(minSeedId);

    //                 // process queue
    //                 ttkInt floodFillIndex = 0;
    //                 ttkInt a,b;
    //                 // std::cout<<std::endl<<queue.size()<<": "<<std::endl;
    //                 while(!queue.empty()){
    //                     a = queue.top();
    //                     queue.pop();
    //                     // std::cout<<a<<" "<<floodFillIndex<<" "<<shortestPaths[a]<<" "<<offsetScalarField[a]<<std::endl;

    //                     // std::cout<<"    processing "<<a<<std::endl;
    //                     floodFillOrder[a] = floodFillIndex++;

    //                     size_t nNeighbors = triangulation->getVertexNeighborNumber( a );
    //                     for(size_t i=0; i<nNeighbors; i++){
    //                         triangulation->getVertexNeighbor(a,i,b);
    //                         if(offsetScalarField[b]<0){
    //                             offsetScalarField[b] = -offsetScalarField[b];
    //                             queue.push(b);
    //                             // std::cout<<"        adding "<<b<<std::endl;
    //                         }
    //                     }
    //                 }
    //             }

    //             nAmbiguateCases++;
    //             previousScalarWasAmbigeous = true;
    //         }
    //     }
    // }

    // for(size_t i=0; i<nVertices; i++)
    //     iterations[i] = floodFillOrder[i];

    // this->printMsg(
    //     "Disambiguating triangulation ("+std::to_string(plateauSmallerNeighbors.size())+")",
    //     1,
    //     timer.getElapsedTime(),
    //     ttk::DEBUG::LINEMODE::REPLACE
    // );
    this->printMsg(
        "Disambiguating PL scalar field ("+std::to_string(plateauSmallerNeighbors.size())+")",
        1,
        timer.getElapsedTime()
    );
    return 1;
};
