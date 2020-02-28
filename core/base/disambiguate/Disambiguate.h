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

typedef ttk::SimplexId ttkInt;

int TODO_TASKSUBDIVISION = 1;

namespace ttk {

    class Disambiguate : virtual public Debug {

        public:

            Disambiguate(){
                this->setDebugMsgPrefix("Disambiguate"); // inherited from Debug: prefix will be printed at the beginning of every msg
            };
            ~Disambiguate(){};

            int PreconditionTriangulation(
                ttk::Triangulation* triangulation
            ) const {
                triangulation->preconditionVertexNeighbors();
                return 1;
            }

            template<typename T,typename idType>
            int computeGlobalOffsets(
                idType* outputOffsets,
                std::vector<std::tuple<T,idType,idType>>& sortedIndices,

                const T* rank1,
                const idType* rank2,
                const idType& nVertices
            ) const {
                ttk::Timer timer;

                // init tuples
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++){
                    auto& t = sortedIndices[i];
                    std::get<0>(t) = rank1[i];
                    std::get<1>(t) = rank2[i];
                    std::get<2>(t) = i;
                }

                this->printMsg(
                    "Computing global offsets",
                    0.2, timer.getElapsedTime(), this->threadNumber_,
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
                    "Computing global offsets",
                    0.8, timer.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // compute actual global output offsets
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++)
                    outputOffsets[std::get<2>(sortedIndices[i])] = i;

                this->printMsg(
                    "Computing global offsets",
                    1,
                    timer.getElapsedTime(),
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
                ttk::Timer timer;
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
                    1,timer.getElapsedTime(),this->threadNumber_
                );

                return 1;
            }

            template<typename dataType, typename idType>
            int flattenScalars(
                dataType* scalars,

                const std::vector<Propagation<idType>*>& masterPropagations
            ) const {
                ttk::Timer timer;
                this->printMsg("Flattening scalar field",0,0,this->threadNumber_,debug::LineMode::REPLACE);

                const idType nMasterPropagations = masterPropagations.size();

                #pragma omp parallel for schedule(dynamic) num_threads(this->threadNumber_)
                for(idType p=0; p<nMasterPropagations; p++){
                    const auto& propagation = *masterPropagations[p];
                    const idType s = propagation.lastEncounteredCriticalPoint;
                    const dataType sScalar = scalars[s];
                    for(auto v : propagation.region)
                        scalars[v] = sScalar;
                }

                this->printMsg("Flattening scalar field",1,timer.getElapsedTime(),this->threadNumber_);

                return 1;
            }

            template<typename idType>
            int detectUnauthorizedMaxima(
                std::vector<idType>& unauthorizedMaxima,
                idType* authorizationMask,

                const ttk::Triangulation* triangulation,
                const idType* inputOffsets,
                const idType* authorizedExtremaIndices,
                const idType& nAuthorizedExtremaIndices
            ) const {

                ttk::Timer timer;
                this->printMsg(
                    "Detecting unauthorized maxima",
                    0,0,this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                const idType nVertices = triangulation->getNumberOfVertices();

                // make room for the maximal number of maxima
                unauthorizedMaxima.resize(nVertices);

                // a synchronized write index used to store discarded maxima
                idType maximaWriteIndex=0;

                // init preservation mask
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++)
                    authorizationMask[i]=-1;

                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nAuthorizedExtremaIndices; i++)
                    authorizationMask[authorizedExtremaIndices[i]]=-2;

                // find discareded maxima
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType v=0; v<nVertices; v++){

                    // if v needs to be preserved then skip
                    if(authorizationMask[v]==-2)
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
                this->printMsg(
                    "Detecting unauthorized maxima ("+std::to_string(maximaWriteIndex)+"|"+std::to_string(nVertices)+")",
                    1,timer.getElapsedTime(),
                    this->threadNumber_
                );

                return 1;
            }

            template<typename idType>
            int detectMaxima(
                std::vector<idType>& maxima,

                const ttk::Triangulation* triangulation,
                const idType* inputOffsets
            ) const {

                ttk::Timer timer;
                this->printMsg(
                    "Detecting maxima",
                    0,0,this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                const idType nVertices = triangulation->getNumberOfVertices();

                // make room for the maximal number of maxima
                maxima.resize(nVertices);

                // a synchronized write index used to store discarded maxima
                idType maximaWriteIndex=0;

                // find discareded maxima
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType v=0; v<nVertices; v++){
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
                    maxima[localWriteIndex] = v;
                }

                // resize to the actual number of discarded maxima
                maxima.resize(maximaWriteIndex);
                this->printMsg(
                    "Detecting maxima ("+std::to_string(maximaWriteIndex)+"|"+std::to_string(nVertices)+")",
                    1,timer.getElapsedTime(),this->threadNumber_
                );

                return 1;
            }

            template<typename idType>
            int computeRegion(
                idType* regionMask,
                Propagation<idType>** propagationMask,
                Propagation<idType>* propagation,

                const ttk::Triangulation* triangulation
            ) const {

                // this->printErr("Region size: "+std::to_string(propagation->regionSize)+" "+std::to_string(propagation->extremumIndex)+" -> "+std::to_string(propagation->lastEncounteredCriticalPoint));

                const idType& extremumIndex = propagation->extremumIndex;

                // collect region
                auto& region = propagation->region;

                region.resize(propagation->regionSize);
                idType regionIndex = 0;
                {
                    std::vector<idType> queue(propagation->regionSize,-1);
                    idType queueIndex = 0;
                    {
                        const idType& saddleIndex = propagation->lastEncounteredCriticalPoint;
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

                        // if(regionIndex>=propagation->regionSize)
                        //     this->printErr("Region size incorrect: "+std::to_string(regionIndex)+ " "+std::to_string(propagation->regionSize));
                        region[regionIndex++] = v;

                        idType nNeighbors = triangulation->getVertexNeighborNumber(v);
                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);
                            if(regionMask[u]!=extremumIndex && propagationMask[u]!=nullptr && propagationMask[u]->find()==propagation){
                                // if(queueIndex>=propagation->regionSize-1)
                                //     this->printErr("Region size incorrect: "+std::to_string(regionIndex)+ " "+std::to_string(propagation->regionSize));
                                queue[queueIndex++]=u;
                                regionMask[u] = extremumIndex;
                            }
                        }
                    }
                }

                if(regionIndex!=propagation->regionSize)
                    this->printErr("Region size incorrect: "+std::to_string(regionIndex)+ " "+std::to_string(propagation->regionSize));

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

                ttk::Timer timer;
                this->printMsg(
                    "Computing regions ("+std::to_string(nPropagations)+")",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                int status = 1;

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
                    1, timer.getElapsedTime(), this->threadNumber_
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
                        propagationP->lastEncounteredCriticalPoint = v;
                        propagationP->terminated = 1;

                        idType numberOfRegisteredLargerVertices=0;
                        #pragma omp atomic capture
                        {
                            saddleMask[v] -= numberOfLargerNeighborsThisThreadVisited;
                            numberOfRegisteredLargerVertices = saddleMask[v];
                        }

                        // if this thread did not register the last remaining larger vertices then terminate propagation
                        if(numberOfRegisteredLargerVertices != -numberOfLargerNeighbors-1)
                            return 1;

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
                        propagationP->lastEncounteredCriticalPoint = v;

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

            template<typename idType, typename dataType>
            int computePropagationIII(
                idType* saddleMask, // used here to store registered larger vertices
                idType* queueMask, // used to mark vertices that have already been added to the queue by this thread
                Propagation<idType>** propagationMask,
                Propagation<idType>& propagation,
                idType& nActivePropagations,

                const ttk::Triangulation* triangulation,
                const idType* offsets,
                const dataType* scalars,
                const dataType& persistenceThreshold
            ) const {

                // pointer used to compare against representative
                auto* propagationP = &propagation;

                // frequently used propagation members
                const idType& extremumIndex = propagationP->extremumIndex;
                propagationP->lastEncounteredCriticalPoint = extremumIndex;
                auto* queue = &propagationP->queue;

                // largest maximum
                dataType elderScalar = scalars[extremumIndex];

                // add extremumIndex to queue
                queue->emplace(offsets[extremumIndex],extremumIndex);
                queueMask[extremumIndex] = extremumIndex;

                idType counter = 0;

                // grow region until it reaches a saddle and then decide if it should continue
                idType v = -1;
                while(!queue->empty()){
                    v = std::get<1>(queue->top());
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
                        }
                        else if(queueMask[u] != extremumIndex) {
                            queue->emplace(offsetU,u);
                            queueMask[u] = extremumIndex;
                        }
                    }

                    // if v is a saddle we have to check if the current thread is the last visitor
                    if(isSaddle){
                        propagationP->lastEncounteredCriticalPoint = v;
                        propagationP->terminated = 1;

                        idType numberOfRegisteredLargerVertices=0;
                        #pragma omp atomic capture
                        {
                            saddleMask[v] -= numberOfLargerNeighborsThisThreadVisited;
                            numberOfRegisteredLargerVertices = saddleMask[v];
                        }

                        // if this thread did not register the last remaining larger vertices then terminate propagation
                        if(numberOfRegisteredLargerVertices != -numberOfLargerNeighbors-1){
                            #pragma omp atomic update
                            nActivePropagations--;

                            return 1;
                        }

                        // Otherwise merge propagation data
                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);

                            if(offsetV<offsets[u]){
                                auto* propagationPu = propagationMask[u]->find();

                                if(propagationP!=propagationPu){

                                    propagationP = Propagation<idType>::unify2(
                                        propagationP,
                                        propagationPu,
                                        offsets
                                    );

                                    queue = &propagationP->queue;
                                    elderScalar = scalars[propagationP->extremumIndex];
                                }
                            }
                        }
                    }

                    // mark vertex as visited and continue
                    propagationP->regionSize++;
                    propagationMask[v] = propagationP;

                    if(counter++>100){
                        counter = 0;

                        idType nActivePropagations_;
                        #pragma omp atmic read
                        nActivePropagations_ = nActivePropagations;

                        if(nActivePropagations_==1){
                            propagationP->terminated = 1;
                            return 1;
                        }
                    }
                }

                // if thread reached the global minimum finish propagation
                propagationP->terminated = 1;
                propagationP->lastEncounteredCriticalPoint = v;

                this->printWrn("reached GM");

                return 1;
            }

            template<typename idType>
            int computeTrunk(
                Propagation<idType>** propagationMask,
                std::vector<Propagation<idType>>& propagations,

                const ttk::Triangulation* triangulation,
                const idType* saddleMask,
                const idType* offsets
            ) const {
                ttk::Timer timer;
                this->printMsg(
                    "Computing trunk",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                const idType nVertices = triangulation->getNumberOfVertices();
                const idType nPropagations = propagations.size();

                // collect and sort saddles
                // [gMin,s0,s1,...,sN,gMax]
                std::vector<std::pair<idType,idType>> saddleIndices(1, {-nVertices-1, -1});
                {
                    for(idType v=0; v<nVertices; v++){
                        if(propagationMask[v]!=nullptr)
                            continue;

                        if(saddleMask[v]<-1)
                            saddleIndices.push_back( {offsets[v],v} );
                    }
                    std::sort(saddleIndices.begin()+1,saddleIndices.end());
                    saddleIndices.push_back({nVertices+1,-1});
                }
                const idType nSaddles = saddleIndices.size();

                // get largest unfinished propagation
                Propagation<idType>* eldestUnfinishedPropagation = nullptr;
                {
                    idType maxUnfinishedSaddleValue = -nVertices-1;
                    for(idType p=0; p<nPropagations; p++){
                        Propagation<idType>* propagationP = &propagations[p];
                        if(propagationP->terminated==1 && offsets[propagationP->lastEncounteredCriticalPoint]>maxUnfinishedSaddleValue){
                            maxUnfinishedSaddleValue = offsets[propagationP->lastEncounteredCriticalPoint];
                            eldestUnfinishedPropagation = propagationP;
                        }
                    }
                }
                if(eldestUnfinishedPropagation==nullptr)
                    this->printErr("xxxxxx");


                this->printMsg(
                    "Computing trunk",
                    0.5, timer.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // get and merge saddle propagations in correct order
                std::vector<Propagation<idType>*> saddlePropagations(nSaddles);
                {
                    // the last unfinished propagation is the last unfinsih
                    saddlePropagations[nSaddles-1] = eldestUnfinishedPropagation;
                    Propagation<idType>* propagationP = eldestUnfinishedPropagation;
                    for(idType i=nSaddles-2; i>0; i--){
                        const idType& v = std::get<1>(saddleIndices[i]);
                        const idType& offsetV = offsets[v];

                        propagationP->lastEncounteredCriticalPoint = v;
                        propagationP->terminated = 1;

                        // this->printMsg(std::to_string(v)+" -> "+std::to_string(offsetV));

                        idType nNeighbors = triangulation->getVertexNeighborNumber(v);

                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);

                            if(propagationMask[u]!=nullptr){
                                Propagation<idType>* propagationPu = propagationMask[u]->find();

                                if(propagationP!=propagationPu){
                                    propagationP = Propagation<idType>::unify3(
                                        propagationP,
                                        propagationPu,
                                        offsets
                                    );
                                }
                            }
                        }

                        saddlePropagations[i] = propagationP;
                    }

                    saddlePropagations[0] = propagationP;
                    // mark main branch as terminated
                    propagationP->terminated = 1;
                }

                this->printMsg(
                    "Computing trunk",
                    0.8, timer.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // map remaining domain vertices to propagations
                std::vector<idType> regionSizedPerInterval(nSaddles,0);
                {
                    #pragma omp parallel for num_threads(this->threadNumber_)
                    for(idType v=0; v<nVertices; v++){
                        if(propagationMask[v]!=nullptr)
                                continue;

                        const idType& offsetV = offsets[v];

                        // this->printErr("----------------------");

                        // idType saddleIndex = nSaddles/2;
                        idType saddleIndex =0;
                        idType left = 0;
                        idType right = nSaddles-1;

                        while(true){
                            saddleIndex = (left + right)/2;
                            // this->printErr(
                            //     std::to_string(left)+" "+std::to_string(right)+" "+std::to_string(saddleIndex)
                            // +" -> "+std::to_string(std::get<0>(saddleIndices[saddleIndex]))+" "+std::to_string(offsetV)+" "+std::to_string(std::get<0>(saddleIndices[saddleIndex+1])));

                            const bool smallerEqualRight = offsetV <= std::get<0>(saddleIndices[saddleIndex+1]);
                            if(!smallerEqualRight){
                                left = saddleIndex;
                                continue;
                            }

                            const bool largerLeft = offsetV > std::get<0>(saddleIndices[saddleIndex]);
                            if(largerLeft)
                                break;
                            else
                                right = saddleIndex;
                        }
                        saddleIndex++;

                        #pragma omp atomic update
                        regionSizedPerInterval[saddleIndex]++;

                        propagationMask[v] = saddlePropagations[saddleIndex];
                    }
                }

                for(idType i=nSaddles-1; i>0; i--){
                    Propagation<idType>* parentBranch = saddlePropagations[i];
                    while(parentBranch!=nullptr){
                        parentBranch->regionSize+=regionSizedPerInterval[i];
                        parentBranch = parentBranch->parentBranch;
                    }
                }

                this->printMsg(
                    "Computing trunk (#saddles: "+std::to_string(nSaddles-2)+")",
                    1, timer.getElapsedTime(), this->threadNumber_
                );

                return 1;
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
                // const idType nPropagations = propagations.size();
                // const idType nVertices = triangulation->getNumberOfVertices();

                // idType pivot = 0;
                // // idType lastActivePropagation = 0;
                // // idType sanity = 0;
                // // for(idType i=0; i<this->threadNumber_; i++){
                // //     #pragma omp atomic read
                // //     pivot=progress[i];

                // //     if(pivot!=-nVertices-1){
                // //         lastActivePropagation = i;
                // //         sanity++;
                // //     }
                // // }

                // do {
                //     #pragma omp atomic read
                //     pivot = progress[0];

                //     // get progress of slowest propagation
                //     for(idType t=1; t<this->threadNumber_; t++){
                //         idType progress_ = 0;
                //         #pragma omp atomic read
                //         progress_ = progress[t];
                //         if(pivot<progress_)
                //             pivot=progress_;
                //     }

                //     // #pragma omp atomic read
                //     // pivot = progress[lastActivePropagation];

                //     for(idType p=0; p<nPropagations; p++){
                //         auto propagation = &propagations[p];

                //         signed char terminated;
                //         // #pragma omp atomic read
                //         terminated = propagation->terminated;

                //         idType saddleIndex;
                //         // #pragma omp atomic read
                //         saddleIndex = propagation->lastEncounteredCriticalPoint;

                //         // Propagation<idType>* parent;
                //         // // #pragma omp atomic read
                //         // parent = propagation->parent;

                //         // if the propagation is complete
                //         if(terminated==0 || propagation->temp==1)
                //             continue;

                //         if(inputOffsets[saddleIndex]<=pivot)
                //             continue;

                //         propagation->temp = 1;

                //         #pragma omp task firstprivate(propagation) priority(1)
                //         {
                //             this->computeRegion<idType>(
                //                 regionMask,
                //                 propagationMask,
                //                 propagation,

                //                 triangulation
                //             );

                //             this->computeLocalOffsetsOfRegion<idType>(
                //                 localOffsets,
                //                 distanceField,

                //                 propagation,
                //                 triangulation,
                //                 regionMask,
                //                 inputOffsets,
                //                 useRegionBasedIterations
                //             );
                //         }
                //     }
                // } while(pivot!=-nVertices-1);

                return 0;
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

            template<typename idType, typename dataType>
            int initializeScalars(
                dataType* outputScalars,

                const dataType* inputScalars,
                const idType& nVertices
            ) const {
                ttk::Timer timer;

                this->printMsg(
                    "Initialize scalars ("+std::to_string(nVertices)+")",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // init region/queue/propagation mask
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType v=0; v<nVertices; v++){
                    outputScalars[v] = inputScalars[v];
                }

                this->printMsg(
                    "Initialize scalars ("+std::to_string(nVertices)+")",
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
                    if(propagation.parent == nullptr && propagation.terminated==1){
                        nRegionVertices = nRegionVertices + propagation.regionSize;
                        masterPropagations[nMasterPropagations++] = &propagation;
                    }
                }
                masterPropagations.resize(nMasterPropagations);

                std::stringstream pFraction, vFraction;
                pFraction << std::fixed << std::setprecision(2) << ((float)nMasterPropagations/(float)nPropagations);
                vFraction << std::fixed << std::setprecision(2) << ((float)nRegionVertices/(float)nVertices);

                this->printMsg(
                    "Finalizing propagations ("+std::to_string(nMasterPropagations)+"|"+pFraction.str()+"|"+vFraction.str()+")",
                    1, timer.getElapsedTime(), this->threadNumber_
                );

                return 1;
            };

            template<typename idType, typename dataType>
            int finalizePropagationsByPersistence(
                std::vector<Propagation<idType>*>& masterPropagations,
                std::vector<Propagation<idType>>& propagations,

                const dataType* scalars,
                const dataType& persistenceThreshold,
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
                    auto* propagation = &propagations[p];

                    // skip if the current propagation is not persistent (main branch must be persistent)
                    if( propagation->parentBranch && (scalars[propagation->extremumIndex]-scalars[propagation->lastEncounteredCriticalPoint])<=persistenceThreshold )
                        continue;

                    // otherwise add all childBranches that are not persistent to the masterPropagations
                    for(auto* c : propagation->childBranches){
                        // skip if childBranch is persistent
                        if(scalars[c->extremumIndex]-scalars[c->lastEncounteredCriticalPoint]>persistenceThreshold)
                            continue;

                        c->setParentRecursive(c);
                        c->parent = nullptr;
                        c->temp = 1;

                        nRegionVertices = nRegionVertices + c->regionSize;
                        masterPropagations[nMasterPropagations++] = c;
                    }
                }
                masterPropagations.resize(nMasterPropagations);

                std::stringstream pFraction, vFraction;
                pFraction << std::fixed << std::setprecision(2) << ((float)nMasterPropagations/(float)nPropagations);
                vFraction << std::fixed << std::setprecision(2) << ((float)nRegionVertices/(float)nVertices);

                this->printMsg(
                    "Finalizing propagations ("+std::to_string(nMasterPropagations)+"|"+pFraction.str()+"|"+vFraction.str()+")",
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

                const ttk::Triangulation* triangulation,
                const idType* offsets
            ) const {
                ttk::Timer timer;

                const idType nPropagations = propagations.size();
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

            template<typename idType, typename dataType>
            int computeDynamicPropagations(
                idType* saddleMask,
                idType* queueMask,
                Propagation<idType>** propagationMask,
                std::vector<Propagation<idType>>& propagations,

                const ttk::Triangulation* triangulation,
                const idType* offsets,
                const dataType* scalars,
                const dataType& persistenceThreshold
            ) const {
                ttk::Timer timer;

                int status = 1;
                const idType nPropagations = propagations.size();
                idType nActivePropagations = nPropagations;

                this->printMsg(
                    "Computing dynamic propagations ("+std::to_string(nPropagations)+")",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // compute propagations
                #pragma omp parallel for schedule(dynamic,1) num_threads(this->threadNumber_)
                for(idType p=0; p<nPropagations; p++){
                    int localStatus = this->computePropagationIII<idType>(
                        saddleMask,
                        queueMask,
                        propagationMask,
                        propagations[p],
                        nActivePropagations,

                        triangulation,
                        offsets,
                        scalars,
                        persistenceThreshold
                    );
                    if(!localStatus)
                        status = 0;

                    // idType nActivePropagations_;
                    // #pragma omp atomic read
                    // nActivePropagations_ = nActivePropagations;

                    // if(nActivePropagations_<100 || nActivePropagations_%10000==0){
                    //     #pragma omp critical
                    //     this->printMsg(
                    //         "Computing dynamic propagations ("+std::to_string(nActivePropagations_)+")",
                    //         1.0 - ((float)nActivePropagations_)/((float)nPropagations), timer.getElapsedTime(), this->threadNumber_,
                    //         debug::LineMode::REPLACE
                    //     );
                    // }
                }
                if(!status) return 0;

                this->printMsg(
                    "Computing dynamic propagations ("+std::to_string(nPropagations)+")",
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
                const idType& saddleIndex = propagation->lastEncounteredCriticalPoint;

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

                ttk::Timer timer;
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
                    1, timer.getElapsedTime(), this->threadNumber_
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
                idType& nRemovedMaxima,

                const ttk::Triangulation* triangulation,
                const idType* authorizedExtremaIndices,
                const idType& nAuthorizedExtremaIndices,
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
                    authorizedExtremaIndices,
                    nAuthorizedExtremaIndices
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
                    nVertices
                );
                if(!status) return 0;

                if(!useInterleaving){
                    // compute propagations
                    status = this->computePropagations<idType>(
                        regionMask,
                        queueMask,
                        propagationMask,
                        propagations,

                        triangulation,
                        inputOffsets
                    );
                    if(!status) return 0;

                    // finalize master propagations
                    status = this->finalizePropagations<idType>(
                        masterPropagations,
                        propagations,

                        nVertices
                    );
                    if(!status) return 0;
                    nRemovedMaxima = masterPropagations.size();

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

                        nVertices
                    );
                    if(!status) return 0;
                    nRemovedMaxima = masterPropagations.size();
                }

                // use region mask as temporary array
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType v=0; v<nVertices; v++)
                    regionMask[v] = inputOffsets[v];

                // flatten regions to offset of last encountered saddles
                // and force that saddles are last in the local offset order
                #pragma omp parallel for schedule(static,4) num_threads(this->threadNumber_)
                for(idType p=0; p<nRemovedMaxima; p++){
                    const auto* propagation = masterPropagations[p];
                    for(const auto& v : propagation->region)
                        regionMask[v] = inputOffsets[propagation->lastEncounteredCriticalPoint];

                    // enforce that each saddle has the largest local offset
                    localOffsets[propagation->lastEncounteredCriticalPoint]=0;
                }

                // compute global offsets
                status = this->computeGlobalOffsets<idType,idType>(
                    outputOffsets,
                    sortedIndices,

                    regionMask,
                    localOffsets,
                    nVertices
                );
                if(!status) return 0;

                return 1;
            };

            template<typename idType, typename dataType>
            int detectAndRemoveMaximaByPersistence(
                std::vector<idType>& maxima,
                idType* outputOffsets,
                idType* localOffsets,
                idType* regionMask,
                idType* queueMask,
                Propagation<idType>** propagationMask,
                std::vector<Propagation<idType>>& propagations,
                std::vector<Propagation<idType>*>& masterPropagations,
                std::vector<std::tuple<idType,idType,idType>>& sortedIndices,
                idType& nRemovedMaxima,

                const ttk::Triangulation* triangulation,
                const dataType& persistenceThreshold,
                const dataType* inputScalars,
                const idType* inputOffsets,
                const bool&   useRegionBasedIterations
            ) const {
                const idType nVertices = triangulation->getNumberOfVertices();

                int status = 0;

                // Classify Critical Points
                status = this->detectMaxima<idType>(
                    maxima,

                    triangulation,
                    inputOffsets
                );
                if(!status) return 0;

                // init propagations
                status = this->initializePropagations<idType>(
                    propagations,
                    regionMask,
                    localOffsets,
                    queueMask,
                    propagationMask,

                    maxima,
                    nVertices
                );
                if(!status) return 0;

                // compute propagations
                status = this->computeDynamicPropagations<idType,dataType>(
                    regionMask, // used here as saddle mask
                    queueMask,
                    propagationMask,
                    propagations,

                    triangulation,
                    inputOffsets,
                    inputScalars,
                    persistenceThreshold
                );
                if(!status) return 0;

                status = this->computeTrunk<idType>(
                    propagationMask,
                    propagations,

                    triangulation,
                    regionMask, // used here as saddle mask
                    inputOffsets
                );
                if(!status) return 0;

                // // // TODO
                // #pragma omp parallel for num_threads(this->threadNumber_)
                // for(idType v=0; v<nVertices; v++)
                //     outputOffsets[v] = propagationMask[v]==nullptr
                //         ? -1
                //         : propagationMask[v]->find()->extremumIndex;
                // //         // : propagationMask[v]->find()->terminated==0
                // //         //     ? -2
                // //         //     : propagationMask[v]->find()->extremumIndex;
                // //         // :
                // //         // propagationMask[v]->find()->temp==1
                // //         //     ? -3

                // return 1;

                // finalize master propagations
                status = this->finalizePropagationsByPersistence<idType,dataType>(
                    masterPropagations,
                    propagations,

                    inputScalars,
                    persistenceThreshold,
                    nVertices
                );
                if(!status) return 0;
                const idType nMasterPropagations = masterPropagations.size();

                // // TODO
                // if(TODO_TASKSUBDIVISION++>0){
                //     #pragma omp parallel for num_threads(this->threadNumber_)
                //     for(idType v=0; v<nVertices; v++)
                //         outputOffsets[v] = propagationMask[v]==nullptr
                //             ? -1
                //             : propagationMask[v]->find()->temp==1
                //                 ? 1
                //                 : -2;
                //             // : propagationMask[v]->find()->extremumIndex;

                //     TODO_TASKSUBDIVISION = 1;
                //     return 1;
                // }

                // // TODO
                // #pragma omp parallel for num_threads(this->threadNumber_)
                // for(idType v=0; v<nVertices; v++)
                //     outputOffsets[v] = -1;

                // compute regions
                status = this->computeRegions<idType>(
                    regionMask,
                    propagationMask,
                    masterPropagations,

                    triangulation
                );
                if(!status) return 0;

                // TODO
                // return 1;

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
                        regionMask[v] = inputOffsets[propagation->lastEncounteredCriticalPoint];

                    // enforce that each saddle has the largest local offset
                    localOffsets[propagation->lastEncounteredCriticalPoint]=0;
                }

                // compute global offsets
                status = this->computeGlobalOffsets<idType,idType>(
                    outputOffsets,
                    sortedIndices,

                    regionMask,
                    localOffsets,
                    nVertices
                );
                if(!status) return 0;

                nRemovedMaxima = masterPropagations.size();

                return 1;
            };

            template<typename idType, typename dataType>
            int allocateMemory(
                std::vector<idType>& inputOffsets,
                std::vector<idType>& unauthorizedExtrema,
                std::vector<idType>& regionMask,
                std::vector<idType>& queueMask,
                std::vector<Propagation<idType>*>& propagationMask,
                std::vector<idType>& localOffsets,
                std::vector<std::tuple<idType,idType,idType>>& sortedIndices,
                std::vector<std::tuple<dataType,idType,idType>>& sortedIndicesII,
                std::vector<Propagation<idType>>& propagationsMax,
                std::vector<Propagation<idType>*>& masterPropagationsMax,
                std::vector<Propagation<idType>>& propagationsMin,
                std::vector<Propagation<idType>*>& masterPropagationsMin,

                const idType& nVertices
            ) const {
                ttk::Timer timer;

                // allocate memory
                this->printMsg(
                    "Allocating memory",
                    0,0,this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                inputOffsets.resize(nVertices);
                unauthorizedExtrema.resize(nVertices);
                regionMask.resize(nVertices);
                queueMask.resize(nVertices);
                propagationMask.resize(nVertices);
                localOffsets.resize(nVertices);
                sortedIndices.resize(nVertices);
                sortedIndicesII.resize(nVertices);

                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType v=0; v<nVertices; v++)
                    inputOffsets[v] = v;

                this->printMsg(
                    "Allocating memory",
                    1,timer.getElapsedTime(),this->threadNumber_
                );
                this->printMsg(debug::Separator::L2);

                return 1;
            }

            template<typename idType, typename dataType>
            int deallocateMemory(
                std::vector<idType>& inputOffsets,
                std::vector<idType>& unauthorizedExtrema,
                std::vector<idType>& regionMask,
                std::vector<idType>& queueMask,
                std::vector<Propagation<idType>*>& propagationMask,
                std::vector<idType>& localOffsets,
                std::vector<std::tuple<idType,idType,idType>>& sortedIndices,
                std::vector<std::tuple<dataType,idType,idType>>& sortedIndicesII,
                std::vector<Propagation<idType>>& propagationsMax,
                std::vector<Propagation<idType>*>& masterPropagationsMax,
                std::vector<Propagation<idType>>& propagationsMin,
                std::vector<Propagation<idType>*>& masterPropagationsMin
            ) const {
                ttk::Timer timer;

                this->printMsg(debug::Separator::L2);
                this->printMsg(
                    "Deallocating memory",
                    0,0,this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                #pragma omp parallel num_threads(this->threadNumber_)
                #pragma omp single
                {
                    #pragma omp task
                    propagationsMin.clear();
                    #pragma omp task
                    propagationsMax.clear();
                    #pragma omp task
                    inputOffsets.clear();
                    #pragma omp task
                    unauthorizedExtrema.clear();
                    #pragma omp task
                    regionMask.clear();
                    #pragma omp task
                    queueMask.clear();
                    #pragma omp task
                    propagationMask.clear();
                    #pragma omp task
                    localOffsets.clear();
                    #pragma omp task
                    sortedIndices.clear();
                    #pragma omp task
                    sortedIndicesII.clear();
                    #pragma omp task
                    masterPropagationsMax.clear();
                    #pragma omp task
                    masterPropagationsMin.clear();
                }

                this->printMsg(
                    "Deallocating memory",
                    1,timer.getElapsedTime(),this->threadNumber_
                );
            }

            template<typename dataType, typename idType>
            int removeUnauthorizedExtrema(
                dataType* outputScalars,
                idType* outputOffsets,

                const ttk::Triangulation* triangulation,
                const dataType* inputScalars,
                const idType* authorizedExtremaIndices,
                const idType& nAuthorizedExtremaIndices,
                const bool&   useRegionBasedIterations,
                const bool&   useInterleaving,
                const bool&   addPerturbation,
                const bool&   useDeallocation
            ) const {

                this->printMsg(debug::Separator::L1);
                this->printMsg({
                    {"Use Region-Based Iterations", std::string(useRegionBasedIterations ? "true" : "false")},
                    {"Use Interleaving", std::string(useInterleaving ? "true" : "false")},
                    {"Add Perturbation", std::string(addPerturbation ? "true" : "false")},
                    {"Use Explicit Deallocation", std::string(useDeallocation ? "true" : "false")}
                });
                this->printMsg(debug::Separator::L2);

                idType nVertices = triangulation->getNumberOfVertices();

                // Allocating Memory
                std::vector<idType> inputOffsets;
                std::vector<idType> unauthorizedExtrema;
                std::vector<idType> regionMask;
                std::vector<idType> queueMask;
                std::vector<Propagation<idType>*> propagationMask;
                std::vector<idType> localOffsets;
                std::vector<std::tuple<idType,idType,idType>> sortedIndices;
                std::vector<std::tuple<dataType,idType,idType>> sortedIndicesII;
                std::vector<Propagation<idType>> propagationsMax;
                std::vector<Propagation<idType>*> masterPropagationsMax;
                std::vector<Propagation<idType>> propagationsMin;
                std::vector<Propagation<idType>*> masterPropagationsMin;

                this->allocateMemory<idType>(
                    inputOffsets,
                    unauthorizedExtrema,
                    regionMask,
                    queueMask,
                    propagationMask,
                    localOffsets,
                    sortedIndices,
                    sortedIndicesII,
                    propagationsMax,
                    masterPropagationsMax,
                    propagationsMin,
                    masterPropagationsMin,

                    nVertices
                );

                // Initialize offsets and scalars
                ttk::Timer timer;
                int status = 0;
                {
                    status = this->computeGlobalOffsets<dataType,idType>(
                        outputOffsets,
                        sortedIndicesII,

                        inputScalars,
                        inputOffsets.data(),
                        nVertices
                    );
                    if(!status) return 0;

                    status = this->initializeScalars<idType,dataType>(
                        outputScalars,

                        inputScalars,
                        nVertices
                    );
                    if(!status) return 0;
                }

                // execute iterations
                size_t iteration=0;
                int sortDirection = 0;
                while(true){
                    if(!useRegionBasedIterations){
                        this->printMsg(
                            "Iteration: "+std::to_string(iteration++),
                            ttk::debug::Separator::L2
                        );
                    } else {
                        this->printMsg(ttk::debug::Separator::L2);
                    }

                    idType nRemovedMinima=0;
                    idType nRemovedMaxima=0;

                    // Minima
                    {
                        // invert offsets to first remove minima (now maxima)
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++){
                            idType& outputOffsetV = outputOffsets[v];
                            inputOffsets[v] = -outputOffsetV;
                            outputOffsetV = -outputOffsetV;
                        }

                        status = this->detectAndRemoveUnauthorizedMaxima<idType>(
                            unauthorizedExtrema,
                            outputOffsets,
                            localOffsets.data(),
                            regionMask.data(),
                            queueMask.data(),
                            propagationMask.data(),
                            propagationsMin,
                            masterPropagationsMin,
                            sortedIndices,
                            nRemovedMinima,

                            triangulation,
                            authorizedExtremaIndices,
                            nAuthorizedExtremaIndices,
                            inputOffsets.data(),
                            useRegionBasedIterations,
                            useInterleaving
                        );
                        if(!status) return 0;

                        if(nRemovedMinima){
                            sortDirection=-1;
                            status = this->flattenScalars<dataType,idType>(
                                outputScalars,

                                masterPropagationsMin
                            );
                            if(!status) return 0;
                        }
                    }

                    // Maxima
                    {
                        // invert offsets again to now remove maxima
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++){
                            idType& outputOffsetV = outputOffsets[v];
                            inputOffsets[v] = -outputOffsetV;
                            outputOffsetV = -outputOffsetV;
                        }

                        status = this->detectAndRemoveUnauthorizedMaxima<idType>(
                            unauthorizedExtrema,
                            outputOffsets,
                            localOffsets.data(),
                            regionMask.data(),
                            queueMask.data(),
                            propagationMask.data(),
                            propagationsMax,
                            masterPropagationsMax,
                            sortedIndices,
                            nRemovedMaxima,

                            triangulation,
                            authorizedExtremaIndices,
                            nAuthorizedExtremaIndices,
                            inputOffsets.data(),
                            useRegionBasedIterations,
                            useInterleaving
                        );
                        if(!status) return 0;

                        if(nRemovedMaxima){
                            sortDirection=+1;
                            status = this->flattenScalars<dataType,idType>(
                                outputScalars,

                                masterPropagationsMax
                            );
                            if(!status) return 0;
                        }
                    }

                    if(nRemovedMinima>0 && nRemovedMaxima<1){
                        const idType maxOffset = nVertices-1;
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++)
                            outputOffsets[v] = maxOffset-outputOffsets[v];
                    }

                    if(useRegionBasedIterations || (nRemovedMinima+nRemovedMaxima)==0)
                        break;if(nRemovedMinima>0 && nRemovedMaxima<1){
                        const idType maxOffset = nVertices-1;
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++)
                            outputOffsets[v] = maxOffset-outputOffsets[v];
                    }
                }

                // optionally add perturbation
                if(addPerturbation && sortDirection!=0){
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
                    this->deallocateMemory(
                        inputOffsets,
                        unauthorizedExtrema,
                        regionMask,
                        queueMask,
                        propagationMask,
                        localOffsets,
                        sortedIndices,
                        sortedIndicesII,
                        propagationsMax,
                        masterPropagationsMax,
                        propagationsMin,
                        masterPropagationsMin
                    );
                }

                this->printMsg(debug::Separator::L1);

                return 1;
            };

            template<typename dataType, typename idType>
            int removeExtremaByPersistence(
                dataType* outputScalars,
                idType* outputOffsets,

                const ttk::Triangulation* triangulation,
                const dataType* inputScalars,
                const dataType  persistenceThreshold,
                const bool& useRegionBasedIterations,
                const bool& addPerturbation,
                const bool& useDeallocation
            ) const {
                this->printMsg(debug::Separator::L1);
                this->printMsg({
                    {"Use Region-Based Iterations", std::string(useRegionBasedIterations ? "true" : "false")},
                    {"Add Perturbation", std::string(addPerturbation ? "true" : "false")},
                    {"Use Explicit Deallocation", std::string(useDeallocation ? "true" : "false")}
                });
                this->printMsg(debug::Separator::L2);

                idType nVertices = triangulation->getNumberOfVertices();

                // Allocating Memory
                std::vector<idType> inputOffsets;
                std::vector<idType> unauthorizedExtrema;
                std::vector<idType> regionMask;
                std::vector<idType> queueMask;
                std::vector<Propagation<idType>*> propagationMask;
                std::vector<idType> localOffsets;
                std::vector<std::tuple<idType,idType,idType>> sortedIndices;
                std::vector<std::tuple<dataType,idType,idType>> sortedIndicesII;
                std::vector<Propagation<idType>> propagationsMax;
                std::vector<Propagation<idType>*> masterPropagationsMax;
                std::vector<Propagation<idType>> propagationsMin;
                std::vector<Propagation<idType>*> masterPropagationsMin;

                this->allocateMemory<idType>(
                    inputOffsets,
                    unauthorizedExtrema,
                    regionMask,
                    queueMask,
                    propagationMask,
                    localOffsets,
                    sortedIndices,
                    sortedIndicesII,
                    propagationsMax,
                    masterPropagationsMax,
                    propagationsMin,
                    masterPropagationsMin,

                    nVertices
                );

                // Initialize offsets and scalars
                ttk::Timer timer;
                int status = 0;
                {
                    status = this->computeGlobalOffsets<dataType,idType>(
                        outputOffsets,
                        sortedIndicesII,

                        inputScalars,
                        inputOffsets.data(),
                        nVertices
                    );
                    if(!status) return 0;

                    status = this->initializeScalars<idType,dataType>(
                        outputScalars,

                        inputScalars,
                        nVertices
                    );
                    if(!status) return 0;
                }


                // execute iterations
                size_t iteration=0;
                int sortDirection = 0;
                while(true){

                    if(!useRegionBasedIterations){
                        this->printMsg(
                            "Iteration: "+std::to_string(iteration++),
                            ttk::debug::Separator::L2
                        );
                    } else {
                        this->printMsg(ttk::debug::Separator::L2);
                    }

                    idType nRemovedMinima=0;
                    idType nRemovedMaxima=0;

                    // Minima
                    {
                        // invert offsets to first remove minima (now maxima)
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++){
                            idType& outputOffsetV = outputOffsets[v];
                            inputOffsets[v] = -outputOffsetV;
                            outputOffsetV = -outputOffsetV;
                            outputScalars[v] = -outputScalars[v];
                        }

                        status = this->detectAndRemoveMaximaByPersistence<idType,dataType>(
                            unauthorizedExtrema,
                            outputOffsets,
                            localOffsets.data(),
                            regionMask.data(),
                            queueMask.data(),
                            propagationMask.data(),
                            propagationsMin,
                            masterPropagationsMin,
                            sortedIndices,
                            nRemovedMinima,

                            triangulation,
                            persistenceThreshold,
                            outputScalars,
                            inputOffsets.data(),
                            useRegionBasedIterations
                        );
                        if(!status) return 0;
                        // // TODO
                        // return 1;

                        if(nRemovedMinima){
                            sortDirection=-1;
                            status = this->flattenScalars<dataType,idType>(
                                outputScalars,

                                masterPropagationsMin
                            );
                            if(!status) return 0;
                        }
                    }

                    // Maxima
                    {
                        // invert offsets again to now remove maxima
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++){
                            idType& outputOffsetV = outputOffsets[v];
                            inputOffsets[v] = -outputOffsetV;
                            outputOffsetV = -outputOffsetV;
                            outputScalars[v] = -outputScalars[v];
                        }

                        status = this->detectAndRemoveMaximaByPersistence<idType,dataType>(
                            unauthorizedExtrema,
                            outputOffsets,
                            localOffsets.data(),
                            regionMask.data(),
                            queueMask.data(),
                            propagationMask.data(),
                            propagationsMax,
                            masterPropagationsMax,
                            sortedIndices,
                            nRemovedMaxima,

                            triangulation,
                            persistenceThreshold,
                            outputScalars,
                            inputOffsets.data(),
                            useRegionBasedIterations
                        );
                        if(!status) return 0;

                        if(nRemovedMaxima){
                            sortDirection=+1;
                            status = this->flattenScalars<dataType,idType>(
                                outputScalars,

                                masterPropagationsMax
                            );
                            if(!status) return 0;
                        }
                    }

                    if(nRemovedMinima>0 && nRemovedMaxima<1){
                        const idType maxOffset = nVertices-1;
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++)
                            outputOffsets[v] = maxOffset-outputOffsets[v];
                    }

                    if(useRegionBasedIterations || (nRemovedMinima+nRemovedMaxima)==0)
                        break;if(nRemovedMinima>0 && nRemovedMaxima<1){
                        const idType maxOffset = nVertices-1;
                        #pragma omp parallel for num_threads(this->threadNumber_)
                        for(idType v=0; v<nVertices; v++)
                            outputOffsets[v] = maxOffset-outputOffsets[v];
                    }
                }

                // optionally add perturbation
                if(addPerturbation && sortDirection!=0){
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
                    this->deallocateMemory(
                        inputOffsets,
                        unauthorizedExtrema,
                        regionMask,
                        queueMask,
                        propagationMask,
                        localOffsets,
                        sortedIndices,
                        sortedIndicesII,
                        propagationsMax,
                        masterPropagationsMax,
                        propagationsMin,
                        masterPropagationsMin
                    );
                }

                this->printMsg(debug::Separator::L1);

                return 1;
            }
    };
}