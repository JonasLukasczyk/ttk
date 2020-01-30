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
#include <PropagationData.h>
#include <ParallelMergeSort.h>

#include <limits>
#include <queue>
#include <unordered_set>
#include <unordered_map>

#include <sys/time.h>

#if(defined(__GNUC__) && !defined(__clang__))
#include <parallel/algorithm>
#endif

typedef ttk::SimplexId ttkInt;

int TODO_TASKSUBDIVISION = 1;

namespace ttk {

    class Disambiguate : virtual public Debug {

        public:

            Disambiguate(){
                this->setDebugMsgPrefix("Disambiguate"); // inherited from Debug: prefix will be printed at the beginning of every msg
            };
            ~Disambiguate(){};

            template<typename T1,typename T2>
            int computeGlobalOffsets(
                T2* outputOffsets,
                std::vector<std::tuple<T1,T2,T2>>& sortedIndices,

                const T2& nVertices,
                const T1* rank1,
                const T2* rank2
            ) const {
                ttk::Timer t;
                this->printMsg(
                    "Computing offset scalar field",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // init tuples
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(T2 i=0; i<nVertices; i++){
                    auto& t = sortedIndices[i];
                    std::get<0>(t) = rank1[i];
                    std::get<1>(t) = rank2[i];
                    std::get<2>(t) = i;
                }

                // std::vector<T2> sortedIndices(nVertices);
                // #pragma omp parallel for num_threads(this->threadNumber_)
                // for(T2 i=0; i<nVertices; i++)
                //     sortedIndices[i] = i;

                this->printMsg(
                    "Computing offset scalar field",
                    0.2, t.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // ttk::ParallelMergeSort::sort<T2,T1,T2>(
                //     sortedIndices.data(),
                //     nVertices,
                //     rank1,
                //     rank2,
                //     this->threadNumber_
                // );

                // sort tuples
                // std::sort(sortedIndices.begin(), sortedIndices.end());

                #ifdef TTK_ENABLE_OPENMP
                    #ifdef __clang__
                        this->printWrn("Caution, outside GCC, sequential sort");
                        std::sort(sortedIndices.begin(), sortedIndices.end());
                    #else
                        __gnu_parallel::sort(sortedIndices.begin(), sortedIndices.end());
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
                for(T2 i=0; i<nVertices; i++)
                    outputOffsets[std::get<2>(sortedIndices[i])] = i;
                    // outputOffsets[sortedIndices[i]] = i;

                this->printMsg(
                    "Computing offset scalar field",
                    1,
                    t.getElapsedTime(),
                    this->threadNumber_
                );

                return 1;
            }

            template<typename dataType, typename idType>
            int computeOutputScalars(
                dataType* outputScalars,

                const dataType* inputScalars,
                const idType* offsets,
                const std::vector<std::tuple<idType,idType,idType>>& sortedIndices
            ) const {
                ttk::Timer t;
                this->printMsg("Computing scalar field",0,0,this->threadNumber_,debug::LineMode::REPLACE);

                const idType nVertices = sortedIndices.size();

                for(idType i=0; i<nVertices; i++)
                    outputScalars[i] = inputScalars[i];

                for(idType i=1; i<nVertices; i++){
                    const idType& v0 = std::get<2>(sortedIndices[i-1]);
                    const idType& v1 = std::get<2>(sortedIndices[i]);

                    const dataType& s0 = outputScalars[v0];
                    const dataType& s1 = outputScalars[v1];

                    if(s0>=s1)
                        outputScalars[v1] = s0;
                }

                // for(idType i=0; i<nVertices-1; i++){
                //     const idType& o0 = offsets[i];
                //     const idType& o1 = offsets[i+1];
                //     const dataType& s0 = outputScalars[i];
                //     const dataType& s1 = outputScalars[i+1];

                //     if(o0<o1 && s0>s1)
                //         outputScalars[i+1] = s0;
                //     // else if(o0>o1 && s0<s1)
                //     //     outputScalars[i+1] = s0;
                // }

                this->printMsg("Computing scalar field",1,t.getElapsedTime(),this->threadNumber_);

                return 1;
            }

            template<typename idType>
            int findDiscardedMaxima(
                std::vector<idType>& discardedMaxima,
                idType* preservationMask,

                const ttk::Triangulation* triangulation,
                const idType* inputOffsets,
                const idType* preservedCriticalPointIndices,
                const idType& nPreservedCriticalPointIndices
            ) const {
                ttk::Timer t;
                this->printMsg("Detecting discarded maxima",0,0,this->threadNumber_,debug::LineMode::REPLACE);

                const idType nVertices = triangulation->getNumberOfVertices();

                // make room for the maximal number of maxima
                discardedMaxima.resize(nVertices);

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
                    discardedMaxima[localWriteIndex] = v;
                }

                // resize to the actual number of discarded maxima
                discardedMaxima.resize(maximaWriteIndex);

                this->printMsg("Detecting discarded maxima ("+std::to_string(maximaWriteIndex)+")",1,t.getElapsedTime(),this->threadNumber_);

                return 1;
            }

            /**
             * Current Limitation: monkey saddle handing (especially during disambiguation)
             *
             * Possible improvements:
             *      + add vertices only once to queue
             *      + each thread has its own temporary data
             */
            template<typename idType>
            int computeRegion(
                idType* regionMask, // used here to store registered larger vertices
                idType* queueMask, // used to mark vertices that have already been added to the queue by this thread
                PropagationData<idType>& propagation,
                std::vector<PropagationData<idType>*>& propagationMask,

                const ttk::Triangulation* triangulation,
                const idType* offsets
            ) const {

                // add extremumIndex to queue
                propagation.queue.push({offsets[propagation.extremumIndex],propagation.extremumIndex});
                queueMask[propagation.extremumIndex] = propagation.extremumIndex;

                auto* propagationRepresentative = &propagation;

                // grow region until it reaches a saddle and then decide if it should continue
                while(!propagationRepresentative->queue.empty()){
                    const idType v = std::get<1>(propagationRepresentative->queue.top());
                    propagationRepresentative->queue.pop();

                    // continue if this has already seen this vertex
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

                        // if lower neighbor
                        if( offsets[u]<offsets[v] ){
                            // here I really want to use propagation instead of propagationRepresentative to label the queue with a unique value
                            if(queueMask[u] != propagation.extremumIndex){
                                propagationRepresentative->queue.push( {offsets[u],u} );
                                queueMask[u] = propagation.extremumIndex;
                            }
                        } else {
                            numberOfLargerNeighbors++;
                            if(propagationMask[u]==nullptr || propagationRepresentative!=propagationMask[u]->find())
                                isSaddle = true;
                            else
                                numberOfLargerNeighborsThisThreadVisited++;
                        }
                    }

                    // if v is a saddle we have to check if the current thread is the last visitor
                    if(isSaddle){

                        propagationRepresentative->lastEncounteredSaddle = v;

                        // * this check is performed by synchronously adding the number of larger vertices that the current thread visited to the saddle outputOffset
                        // * if after this synchronous operation the outputOffset at the saddle equals the total number of larger vertices then this must be the last thread that visited the saddle
                        // * Note: the offset stores the temporary value -1 for unvisited vertices and only values >=0 for processed vertices -> the number of larger neighbors of v visited by this thread is actually substracted from the offset to prevent the creation of a separate mask
                        idType numberOfRegisteredLargerVerticesAsStoredInOffset=0;
                        #pragma omp atomic capture
                        {
                            regionMask[v] -= numberOfLargerNeighborsThisThreadVisited;
                            numberOfRegisteredLargerVerticesAsStoredInOffset = regionMask[v];
                        }

                        // if this thread did not register the last remaining larger vertices then terminate propagation
                        if(numberOfRegisteredLargerVerticesAsStoredInOffset!=-numberOfLargerNeighbors-1)
                            break;

                        // Otherwise merge propagation data
                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);
                            if(offsets[v]<offsets[u] && propagationRepresentative!=propagationMask[u]->find()){
                                propagationRepresentative = PropagationData<idType>::unify(
                                    propagationRepresentative,
                                    propagationMask[u]
                                );
                            }
                        }
                    }

                    // mark vertex as visited and continue
                    propagationMask[v] = propagationRepresentative;
                }

                return 1;
            }

            template<typename idType>
            int computeRegions(
                idType* regionMask,
                std::vector<PropagationData<idType>>& propagationList,

                const ttk::Triangulation* triangulation,
                const idType* offsets,
                const std::vector<idType>& discardedMaxima
            ) const {
                const idType nVertices = triangulation->getNumberOfVertices();
                const idType nDiscardedMaxima = discardedMaxima.size();

                ttk::Timer t;
                this->printMsg(
                    "Computing "+std::to_string(nDiscardedMaxima)+" regions",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // init region mask
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++)
                    regionMask[i] = -1;

                // init propagation mask
                std::vector<PropagationData<idType>*> propagationMask(nVertices,nullptr);

                // init propagation list
                propagationList.resize(nDiscardedMaxima);
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nDiscardedMaxima; i++)
                    propagationList[i].extremumIndex = discardedMaxima[i];

                this->printMsg(
                    "Computing regions ("+std::to_string(nDiscardedMaxima)+")",
                    0.1, t.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // compute tasks bounds
                const idType nTaskRange = std::max(
                    (int)ceil((float)nDiscardedMaxima/(float)this->threadNumber_) / TODO_TASKSUBDIVISION,
                    1
                );

                // std::vector<std::pair<double,double>> timings(ceil((float)nDiscardedMaxima/(float)nTaskRange));

                std::vector<idType> queueMask(nVertices,-1);

                // compute regions
                #pragma omp parallel num_threads(this->threadNumber_)
                #pragma omp single
                for(idType i=0; i<nDiscardedMaxima; i+=nTaskRange){
                    #pragma omp task firstprivate(i)
                    {
                        // struct timeval stamp;
                        // gettimeofday(&stamp, NULL);

                        // timings[ i/nTaskRange ].first = (stamp.tv_sec * 1000000 + stamp.tv_usec) / 1000000.0;

                        idType t0 = i;
                        idType tN = std::min(t0+nTaskRange,nDiscardedMaxima);

                        for(idType t=t0; t<tN; t++){
                            this->computeRegion<idType>(
                                regionMask,
                                queueMask.data(),
                                propagationList[t],
                                propagationMask,

                                triangulation,
                                offsets
                            );
                        }

                        // gettimeofday(&stamp, NULL);
                        // timings[ i/nTaskRange ].second = (stamp.tv_sec * 1000000 + stamp.tv_usec) / 1000000.0;
                    }
                }
                // #pragma omp parallel for num_threads(this->threadNumber_)
                // for(idType i=0; i<nDiscardedMaxima; i++){
                //     this->computeRegion<idType>(
                //         regionMask,
                //         propagationList[i],
                //         propagationMask,

                //         triangulation,
                //         offsets
                //     );
                // }

                // #pragma omp parallel num_threads(this->threadNumber_)
                // #pragma omp single
                // for(idType i=0; i<nDiscardedMaxima; i++){
                //     #pragma omp task firstprivate(i)
                //     this->computeRegion<idType>(
                //         regionMask,
                //         propagationList[i],
                //         propagationMask,

                //         triangulation,
                //         offsets
                //     );
                // }

                this->printMsg(
                    "Computing regions ("+std::to_string(nDiscardedMaxima)+")",
                    0.9, t.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // build region lists
                idType nRegionVertices = 0;
                for(idType i=0; i<nVertices; i++){
                    if(propagationMask[i]==nullptr)
                        continue;
                    propagationMask[i]->find()->region.push_back(i);
                    nRegionVertices++;
                }

                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nDiscardedMaxima; i++){
                    auto& propagation = propagationList[i];
                    if(!propagation.isTerminated){
                        for(const auto& j : propagation.region)
                            regionMask[j] = propagation.extremumIndex;
                    }
                }

                this->printMsg(
                    "Computing regions ("+std::to_string(nDiscardedMaxima)+"|"+std::to_string(nRegionVertices)+")",
                    1, t.getElapsedTime(), this->threadNumber_
                );

                // for(idType i=0; i<timings.size(); i++){
                //     this->printMsg("Task "+std::to_string(i)+": "+std::to_string(timings[i].first)+" - "+std::to_string(timings[i].second) + " -> " +std::to_string(timings[i].second-timings[i].first));
                // }

                return 1;
            }

            template<typename idType>
            int computeLocalOffsetsOfRegion(
                idType* localOffsets,

                const ttk::Triangulation* triangulation,
                const idType* regionMask,
                const idType& regionID,
                const std::vector<idType>& region,
                const idType& seedIndex,
                const idType* distanceField
            ) const {

                // NOTE: this function uses the localOffsets during computation to add every region vertex exectly once
                for(size_t i=0, j=region.size(); i<j; i++)
                    localOffsets[region[i]] = regionID;

                // init priority queue
                std::priority_queue<
                    std::pair<idType,idType>,
                    std::vector<std::pair<idType,idType>>
                > queue;
                queue.push( {distanceField[seedIndex],seedIndex} );
                localOffsets[seedIndex] = -1;

                std::vector<idType> localVertexSequence(region.size()+1);

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
                        if(regionMask[u]==regionID && localOffsets[u]!=-1){
                            localOffsets[u] = -1;
                            queue.push( {distanceField[u],u} );
                        }
                    }
                }

                // finalize tempOffsets and reset outputOffsets
                for(size_t i=0, j=region.size(); i<j; i++)
                    localOffsets[ localVertexSequence[i+1] ] = -i;

                return 1;
            }

            template<typename idType>
            int computeLocalOffsetsOfRegions(
                idType* localOffsets,
                idType* tempOffsets,

                const ttk::Triangulation* triangulation,
                const idType* regionMask,
                const idType* inputOffsets,
                const std::vector<PropagationData<idType>>& propagationList
            ) const {

                ttk::Timer t;
                this->printMsg(
                    "Computing local order of regions",
                    0, 0, this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // init local offsets
                const idType nVertices = triangulation->getNumberOfVertices();
                const idType nPropagations = propagationList.size();

                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++)
                    tempOffsets[i] = -1;

                idType nNonTerminatedPropagations=0;
                std::vector<const PropagationData<idType>*> nonTerminatedPropagations(nPropagations);
                for(idType i=0; i<nPropagations; i++){
                    if(!propagationList[i].isTerminated)
                        nonTerminatedPropagations[nNonTerminatedPropagations++] = &propagationList[i];
                }

                this->printMsg( "Computing local order of regions ("+std::to_string(nNonTerminatedPropagations)+")",
                    0.1, t.getElapsedTime(), this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // compute tasks bounds
                const idType nTaskRange = std::max(
                    (int)ceil((float)nNonTerminatedPropagations/(float)this->threadNumber_) / TODO_TASKSUBDIVISION,
                    1
                );

                #pragma omp parallel num_threads(this->threadNumber_)
                #pragma omp single
                for(idType i=0; i<nNonTerminatedPropagations; i+=nTaskRange){
                    #pragma omp task firstprivate(i)
                    {
                        idType t0 = i;
                        idType tN = std::min(t0+nTaskRange,nNonTerminatedPropagations);

                        for(idType t=t0; t<tN; t++){
                            const auto& propagation = *nonTerminatedPropagations[t];
                            this->computeLocalOffsetsOfRegion<idType>(
                                localOffsets,

                                triangulation,
                                regionMask,
                                propagation.extremumIndex,
                                propagation.region,
                                propagation.lastEncounteredSaddle,
                                inputOffsets
                            );
                        }
                    }
                }

                this->printMsg( "Computing local order of regions ("+std::to_string(nNonTerminatedPropagations)+")",
                    1, t.getElapsedTime(), this->threadNumber_
                );

                return 1;
            }

            template<typename idType>
            int removeMaxima(
                idType* outputOffsets,
                idType* regionMask,
                idType& nDiscardedMaxima,
                std::vector<std::tuple<idType,idType,idType>>& sortedIndices,

                const ttk::Triangulation* triangulation,
                const idType* preservedCriticalPointIndices,
                const size_t& nPreservedCriticalPointIndices,
                const idType* inputOffsets
            ) const {

                std::vector<idType> discardedMaxima;

                const idType nVertices = triangulation->getNumberOfVertices();

                // Classify Critical Points
                this->findDiscardedMaxima<idType>(
                    discardedMaxima,
                    regionMask,

                    triangulation,
                    inputOffsets,
                    preservedCriticalPointIndices,
                    nPreservedCriticalPointIndices
                );
                nDiscardedMaxima = discardedMaxima.size();

                // if nothign to remove return
                if(nDiscardedMaxima<1)
                    return 1;

                // compute regions
                std::vector<PropagationData<idType>> propagationList;
                this->computeRegions<idType>(
                    regionMask,
                    propagationList,

                    triangulation,
                    inputOffsets,
                    discardedMaxima
                );

                // compute local order of regions
                std::vector<idType> localOffsets(nVertices,-1);
                this->computeLocalOffsetsOfRegions<idType>(
                    localOffsets.data(),
                    outputOffsets,

                    triangulation,
                    regionMask,
                    inputOffsets,
                    propagationList
                );

                // compute output offsets
                {
                    // use mask as temporary array
                    #pragma omp parallel for num_threads(this->threadNumber_)
                    for(idType i=0; i<nVertices; i++)
                        regionMask[i] = inputOffsets[i];

                    #pragma omp parallel for num_threads(this->threadNumber_)
                    for(size_t p=0; p<nDiscardedMaxima; p++){
                        const auto& propagation = propagationList[p];
                        if(!propagation.isTerminated){
                            for(const auto& i : propagation.region)
                                regionMask[i] = regionMask[propagation.lastEncounteredSaddle];

                            localOffsets[propagation.lastEncounteredSaddle]=std::numeric_limits<idType>::max();
                        }
                    }

                    this->computeGlobalOffsets<idType,idType>(
                        outputOffsets,
                        sortedIndices,

                        triangulation->getNumberOfVertices(),
                        regionMask,
                        localOffsets.data()
                    );
                }

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
                const idType& nTaskSubdivions
            ) const {

                TODO_TASKSUBDIVISION = nTaskSubdivions;

                ttk::Timer globalTimer;
                this->printMsg(debug::Separator::L1);

                idType nVertices = triangulation->getNumberOfVertices();

                std::vector<idType> inputOffsets(nVertices);
                std::vector<idType> regionMask(nVertices);
                std::vector<std::tuple<idType,idType,idType>> sortedIndices(nVertices);

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
                while(true){
                    this->printMsg(
                        "Iteration: "+std::to_string(iteration++),
                        ttk::debug::Separator::L2
                    );

                    #pragma omp parallel for num_threads(this->threadNumber_)
                    for(idType v=0; v<nVertices; v++)
                        inputOffsets[v] = -outputOffsets[v];

                    idType nDiscardedMinima=0;

                    // Minima
                    this->removeMaxima<idType>(
                        outputOffsets,
                        regionMask.data(),
                        nDiscardedMinima,
                        sortedIndices,

                        triangulation,
                        preservedCriticalPointIndices,
                        nPreservedCriticalPointIndices,
                        inputOffsets.data()
                    );

                    // Maxima
                    #pragma omp parallel for num_threads(this->threadNumber_)
                    for(idType v=0; v<nVertices; v++)
                        inputOffsets[v] = -outputOffsets[v];

                    idType nDiscardedMaxima=0;
                    this->removeMaxima<idType>(
                        outputOffsets,
                        regionMask.data(),
                        nDiscardedMaxima,
                        sortedIndices,

                        triangulation,
                        preservedCriticalPointIndices,
                        nPreservedCriticalPointIndices,
                        inputOffsets.data()
                    );

                    if((nDiscardedMaxima+nDiscardedMinima)==0)
                        break;
                }

                this->computeOutputScalars<dataType,idType>(
                    outputScalars,

                    inputScalars,
                    outputOffsets,
                    sortedIndices
                );

                this->printMsg(debug::Separator::L2);
                this->printMsg("Complete", 1, globalTimer.getElapsedTime(), this->threadNumber_);
                this->printMsg(debug::Separator::L1);

                return 1;
            };











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
