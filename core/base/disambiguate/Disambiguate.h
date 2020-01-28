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

#include <limits>
#include <queue>
#include <unordered_set>
#include <unordered_map>

typedef ttk::SimplexId ttkInt;

namespace ttk {

    class Disambiguate : virtual public Debug {

        public:

            template<typename T1, typename T2>
            struct LessComparator {
                const T1* data;
                LessComparator(){};
                LessComparator(const T1* data):data(data){};
                int operator() (const T2& i, const T2& j) const {
                    const T1& iData = this->data[i];
                    const T1& jData = this->data[j];
                    return iData==jData
                            ? i<j
                            : iData<jData;
                }
            };

            template<typename T1, typename T2>
            struct GreaterComparator {
                const T1* data;
                GreaterComparator(){};
                GreaterComparator(const T1* data):data(data){};
                int operator() (const T2& i, const T2& j) const {
                    const T1& iData = this->data[i];
                    const T1& jData = this->data[j];
                    return iData==jData
                            ? i>j
                            : iData>jData;
                }
            };

            template<typename T1, typename T2>
            struct DataMinOffsetComparator {
                const T1* data;
                const T2* offsets;
                DataMinOffsetComparator(){};
                DataMinOffsetComparator(const T1* data, const T2* offsets):data(data),offsets(offsets){};
                int operator() (const T2& i, const T2& j) const {
                    const T1& iData = this->data[i];
                    const T1& jData = this->data[j];
                    const T2& iOffset = this->offsets[i];
                    const T2& jOffset = this->offsets[j];
                    return iData==jData
                            ? iOffset<jOffset
                            : iData<jData;
                }
            };

            template<typename T1, typename T2>
            struct DataMaxOffsetComparator {
                const T1* data;
                const T2* offsets;
                DataMaxOffsetComparator(){};
                DataMaxOffsetComparator(const T1* data, const T2* offsets):data(data),offsets(offsets){};
                int operator() (const T2& i, const T2& j) const {
                    const T1& iData = this->data[i];
                    const T1& jData = this->data[j];
                    const T2& iOffset = this->offsets[i];
                    const T2& jOffset = this->offsets[j];
                    return iData==jData
                            ? iOffset>jOffset
                            : iData<jData;
                }
            };

            Disambiguate(){
                this->setDebugMsgPrefix("Disambiguate"); // inherited from Debug: prefix will be printed at the beginning of every msg
            };
            ~Disambiguate(){};

            template<typename dataType,typename idType, typename ComparatorType>
            int computeGlobalOffsets(
                idType* outputOffsets,

                const idType& nVertices,
                const ComparatorType comperator
            ) const {
                ttk::Timer t;
                this->printMsg(
                    "Computing offset scalar field",
                    0,
                    0,
                    this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                std::vector<idType> sortedIndices(nVertices);
                for(idType i=0; i<nVertices; i++)
                    sortedIndices[i] = i;

                std::sort(sortedIndices.begin(), sortedIndices.end(), comperator);

                for(idType i=0; i<nVertices; i++)
                    outputOffsets[sortedIndices[i]] = i;

                this->printMsg(
                    "Computing offset scalar field",
                    1,
                    t.getElapsedTime(),
                    this->threadNumber_
                );

                return 1;
            }

            template<typename idType>
            int classifyExtrema(
                std::vector<idType>& discardedMinima,
                std::vector<idType>& discardedMaxima,

                const ttk::Triangulation* triangulation,
                const idType* inputOffsets,
                const idType* preservedCriticalPointIndices,
                const size_t& nPreservedCriticalPointIndices
            ) const {
                ttk::Timer t;
                this->printMsg("Classifying extrema",0,0,this->threadNumber_,debug::LineMode::REPLACE);

                const idType nVertices = triangulation->getNumberOfVertices();

                #ifdef TTK_ENABLE_OPENMP
                #pragma omp parallel for num_threads(this->threadNumber_)
                #endif
                for(idType v=0; v<nVertices; v++){

                    bool hasSmallerNeighbor = false;
                    bool hasLargerNeighbor = false;
                    bool ambiguous = false;

                    const idType& vOffset = inputOffsets[v];

                    idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                    for(size_t n=0; n<nNeighbors; n++){
                        idType u;
                        triangulation->getVertexNeighbor(v,n,u);
                        const idType& uOffset = inputOffsets[u];

                        if( uOffset<vOffset )
                            hasSmallerNeighbor = true;
                        else if( uOffset > vOffset )
                            hasLargerNeighbor = true;
                        else
                            ambiguous = true;
                    }

                    bool isMaximum = hasSmallerNeighbor && !hasLargerNeighbor;
                    bool isMinimum = !hasSmallerNeighbor && hasLargerNeighbor;
                    bool isExtremum = isMaximum || isMinimum;

                    bool hasToBePreserved = false;
                    if(isExtremum){
                        for(size_t p=0; p<nPreservedCriticalPointIndices; p++){
                            if(v==preservedCriticalPointIndices[p]){
                                hasToBePreserved = true;
                                break;
                            }
                        }
                    }

                    auto& discardedExtrema = isMaximum ? discardedMaxima : discardedMinima;

                    if(isExtremum){
                        if(!hasToBePreserved){
                            #pragma omp critical
                            discardedExtrema.push_back(v);
                        }
                    }
                }

                this->printMsg("Classifying extrema",1,t.getElapsedTime(),this->threadNumber_);

                return 1;
            }

            template<typename dataType, typename idType, typename PropagationType>
            int computeOutputScalars(
                dataType* outputScalars,

                const dataType* inputScalars,
                const std::unordered_map<idType, PropagationType>& extremumIndexToPropagationDataMap
            ) const {
                ttk::Timer t;
                this->printMsg("Computing scalar field",0,0,this->threadNumber_,debug::LineMode::REPLACE);

                #pragma omp parallel num_threads(this->threadNumber_)
                #pragma omp single
                for(const auto& it: extremumIndexToPropagationDataMap){
                    if(it.second.isTerminated)
                        continue;

                    const auto* propagationData = &(it.second);

                    #pragma omp task firstprivate(propagationData)
                    {
                        const auto& region = propagationData->region;
                        const auto& saddleScalar = inputScalars[propagationData->lastEncounteredSaddle];
                        for(size_t i=0, j=region.size(); i<j; i++)
                            outputScalars[region[i]] = saddleScalar;
                    }
                }

                this->printMsg("Computing scalar field",1,t.getElapsedTime(),this->threadNumber_);

                return 1;
            }

            /**
             * Current Limitation: monkey saddle handing (especially during disambiguation)
             *
             * Possible improvements:
             *      + add vertices only once to queue
             *      + each thread has its own temporary data
             */
            template<typename idType, typename PropagationType>
            int computeRegion(
                idType* regionMask, // used here to store registered larger vertices
                PropagationType* propagationData,
                std::vector<PropagationType*>& propagationMask,

                const ttk::Triangulation* triangulation
            ) const {

                // add extremumIndex to queue
                propagationData->queue.push(propagationData->extremumIndex);

                // grow region until it reaches a saddle and then decide if it should continue
                while(!propagationData->queue.empty()){
                    const idType v = propagationData->queue.top();
                    propagationData->queue.pop();

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
                        if( propagationData->comperator(u,v)){
                            propagationData->queue.push( u );
                        } else {
                            numberOfLargerNeighbors++;
                            if(propagationMask[u]==nullptr || propagationData!=propagationMask[u]->find())
                                isSaddle = true;
                            else
                                numberOfLargerNeighborsThisThreadVisited++;
                        }
                    }

                    // if v is a saddle we have to check if the current thread is the last visitor
                    if(isSaddle){

                        propagationData->lastEncounteredSaddle = v;

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
                            if(propagationData->comperator(v,u) && propagationData!=propagationMask[u]->find())
                                PropagationType::unify( propagationData, propagationMask[u]->find() );
                        }
                    }

                    // mark vertex as visited and continue
                    propagationMask[v] = propagationData;
                }

                return 1;
            }

            template<typename idType, typename PropagationType>
            int computeRegions(
                idType* regionMask,
                std::vector<PropagationType*>& propagationList,

                const ttk::Triangulation* triangulation,
                const idType* offsets,
                const std::vector<idType>& extremaIDs
            ) const {
                ttk::Timer t;
                this->printMsg(
                    "Computing "+std::to_string(extremaIDs.size())+" regions",
                    0,
                    0,
                    this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                const idType nVertices = triangulation->getNumberOfVertices();
                const idType nEtrema = extremaIDs.size();

                // init region mask
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++)
                    regionMask[i] = -1;

                // init propagation list
                propagationList.resize(nEtrema, nullptr);
                #pragma omp parallel for num_threads(this->threadNumber_)
                for(size_t i=0; i<nEtrema; i++)
                    propagationList[i] = new PropagationType( extremaIDs[i], offsets );

                // init propagation mask
                std::vector<PropagationType*> propagationMask(nVertices,nullptr);

                // compute regions
                #pragma omp parallel num_threads(this->threadNumber_)
                #pragma omp single
                for(idType i=0; i<nEtrema; i++){
                    #pragma omp task firstprivate(i)
                    this->computeRegion<idType,PropagationType>(
                        regionMask,
                        propagationList[i],
                        propagationMask,

                        triangulation
                    );
                }

                // t.reset();
                // this->printMsg(
                //     "Merging monkey saddles",
                //     0,
                //     t.getElapsedTime(),
                //     this->threadNumber_
                // );

                // // merge monkey saddles
                // #pragma omp parallel for num_threads(this->threadNumber_)
                // for(idType i=0; i<nEtrema; i++){
                //     for(idType j=i+1; j<nEtrema; j++){
                //         if(
                //             !propagationList[i]->isTerminated && !propagationList[j]->isTerminated
                //             && propagationList[i]->lastEncounteredSaddle==propagationList[j]->lastEncounteredSaddle
                //         ){
                //             PropagationType::unify(propagationList[i],propagationList[j]);
                //         }
                //     }
                // }

                // this->printMsg(
                //     "Merging monkey saddles",
                //     1,
                //     t.getElapsedTime(),
                //     this->threadNumber_
                // );

                // add last encountered saddle to currently unterminated propagations
                // #pragma omp parallel for num_threads(this->threadNumber_)
                // for(idType i=0; i<nEtrema; i++){
                //     auto* propagation = propagationList[i];
                //     if(!propagation->isTerminated){
                //         propagationMask[propagation->lastEncounteredSaddle] = propagation;
                //         regionMask[propagation->lastEncounteredSaddle] = propagation->extremumIndex;
                //     }
                // }

                // build region lists
                for(idType i=0; i<nVertices; i++){
                    if(propagationMask[i]==nullptr)
                        continue;
                    PropagationType* propagation = propagationMask[i]->find();
                    propagation->region.push_back(i);
                }

                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nEtrema; i++){
                    auto* propagation = propagationList[i];
                    if(!propagation->isTerminated){
                        for(const auto& j : propagation->region)
                            regionMask[j] = propagation->extremumIndex;
                    }
                }

                this->printMsg(
                    "Computing "+std::to_string(extremaIDs.size())+" regions",
                    1,
                    t.getElapsedTime(),
                    this->threadNumber_
                );

                return 1;
            }

            template<typename idType>
            int computeBreadthFirstSearchField(
                idType* localOffsets,

                const ttk::Triangulation* triangulation,
                const idType* regionMask,
                const idType& regionID,
                const std::vector<idType>& region,
                const idType& seedIndex
            ) const {

                // init temp mask
                for(size_t i=0, j=region.size(); i<j; i++)
                    localOffsets[region[i]] = regionID;

                // init queue with seedIndices and mark as added
                std::queue<idType> queue;
                {
                    // queue.push(seedIndex);
                    idType nNeighbors = triangulation->getVertexNeighborNumber( seedIndex );
                    for(idType nIndex=0; nIndex<nNeighbors; nIndex++){
                        idType u;
                        triangulation->getVertexNeighbor(seedIndex,nIndex,u);
                        if(localOffsets[u]==regionID){
                            localOffsets[u] = -1;
                            queue.push(u);
                        }
                    }
                }

                std::vector<idType> localVertexSequence(region.size());

                // execute breadth-first-sweep
                idType q = 0;
                while(!queue.empty()){
                    idType v = queue.front();
                    queue.pop();

                    // update vertex sequence
                    localVertexSequence[q++] = v;

                    idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                    for(idType nIndex=0; nIndex<nNeighbors; nIndex++){
                        idType u;
                        triangulation->getVertexNeighbor(v,nIndex,u);

                        // if not already added...
                        if(localOffsets[u]==regionID){
                            localOffsets[u] = -1;
                            queue.push( u );
                        }
                    }
                }

                // set local offsets
                for(size_t i=0, j=region.size(); i<j; i++)
                    localOffsets[localVertexSequence[i]] = i;

                return 1;
            }

            template<typename idType>
            int growRegionBasedOnFieldMaximum(
                idType* localOffsets,

                const ttk::Triangulation* triangulation,
                const idType* regionMask,
                const idType& regionID,
                const std::vector<idType>& region,
                const idType& seedIndex,
                const idType* distanceField
            ) const {

                // NOTE: this function uses the localOffsets during computation to add every region vertex exectly once

                // init priority queue
                const LessComparator<idType,idType> comperator(distanceField);
                std::priority_queue<
                    idType,
                    std::vector<idType>,
                    LessComparator<idType,idType>
                > queue(comperator);
                queue.push( seedIndex );
                localOffsets[seedIndex] = -1;

                std::vector<idType> localVertexSequence(region.size());

                idType q=0;
                while(!queue.empty()){
                    idType v = queue.top();
                    queue.pop();

                    localVertexSequence[q++] = v;

                    idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                    for(idType n=0; n<nNeighbors; n++){
                        idType u;
                        triangulation->getVertexNeighbor(v,n,u);

                        // if u has not been popped from the queue add to queue
                        if(regionMask[u]==regionID && localOffsets[u]!=-1){
                            localOffsets[u] = -1;
                            queue.push( u );
                        }
                    }
                }

                // finalize tempOffsets and reset outputOffsets
                for(size_t i=0, j=region.size(); i<j; i++)
                    localOffsets[ localVertexSequence[i] ] = i;

                return 1;
            }

            // template<typename idType>
            // int computeSeparatrixPath(
            //     std::vector<idType>& separatrix,

            //     const ttk::Triangulation* triangulation,
            //     const idType& regionID,
            //     const idType* regionMask,
            //     const idType* offsets,
            //     const idType& saddleIndex,
            //     const std::vector<idType>& upperLinkComponent,
            //     const idType& maximumValue
            // ) const {

            //     // seed leading towards extremum A
            //     const idType seedIndex = *std::max_element(
            //         upperLinkComponent.begin(),
            //         upperLinkComponent.end(),
            //         LessComparator<idType,idType>(offsets)
            //     );

            //     // init separatrix
            //     separatrix.push_back(seedIndex);
            //     idType lastVisitedVertex = seedIndex;

            //     // while the last visited separatrix vertex is smaller than the maximumValue
            //     while(offsets[lastVisitedVertex]<maximumValue){
            //         // add largest neighbor from inside the region
            //         idType maxNeighborIndex=-1;
            //         idType maxNeighborOffset=-1;
            //         std::vector<idType> largerNeighborsInsideRegion;
            //         const idType nNeighbors = triangulation->getVertexNeighborNumber( lastVisitedVertex );
            //         for(idType i=0; i<nNeighbors; i++){
            //             idType u;
            //             triangulation->getVertexNeighbor(lastVisitedVertex,i,u);

            //             const idType& uOffset = offsets[u];

            //             // only if the u belongs to the region and is larger than the current maximum
            //             if(regionMask[u]==regionID && offsets[lastVisitedVertex]<uOffset && maxNeighborOffset<uOffset){
            //                 maxNeighborOffset=uOffset;
            //                 maxNeighborIndex=u;
            //             }
            //         }

            //         if(maxNeighborIndex<0){
            //             this->printWrn("WHAT@?????");
            //             return 0;
            //         }

            //         // add largest neighbor to separatrix
            //         separatrix.push_back(maxNeighborIndex);
            //         lastVisitedVertex = maxNeighborIndex;
            //     }

            //     return 1;
            // }

            // template<typename idType>
            // int findSaddleInsideRegion(
            //     idType& saddleIndex,
            //     std::vector<idType>& visitedUpperLink,
            //     std::vector<idType>& unvisitedUpperLink,

            //     const ttk::Triangulation* triangulation,
            //     const idType& regionID,
            //     const idType* regionMask,
            //     const idType* offsets,
            //     const idType& extremumIndex
            // ) const {

            //     saddleIndex = -1;
            //     std::unordered_set<idType> processed;

            //     // init priority queue
            //     LessComparator<idType,idType> comperator(offsets);
            //     std::priority_queue<
            //         idType,
            //         std::vector<idType>,
            //         LessComparator<idType,idType>
            //     > queue(comperator);

            //     // add artifact to queue
            //     queue.push(extremumIndex);

            //     // add all vertices inside plateau next to seed vertices to queue
            //     while(!queue.empty()){
            //         idType v = queue.top();
            //         queue.pop();

            //         if(processed.count(v))
            //             continue;

            //         processed.insert(v);

            //         // check if v is saddle
            //         bool isSaddle = false;

            //         const idType& vOffset = offsets[v];
            //         const size_t nNeighbors = triangulation->getVertexNeighborNumber( v );
            //         for(idType i=0; i<nNeighbors; i++){
            //             idType u;
            //             triangulation->getVertexNeighbor(v,i,u);

            //             const idType& uOffset = offsets[u];

            //             // only if the u belongs to the region
            //             if(regionMask[u]==regionID){

            //                 // if u has not been processed and is larger
            //                 if(processed.count(u)==0 && uOffset>vOffset){
            //                     isSaddle = true;
            //                     break;
            //                 }

            //                 if(uOffset<vOffset)
            //                     queue.push(u);
            //             }
            //         }

            //         if(isSaddle){
            //             // set saddle index
            //             saddleIndex = v;

            //             // get visited and unvisited upper link
            //             for(idType i=0; i<nNeighbors; i++){
            //                 idType u;
            //                 triangulation->getVertexNeighbor(v,i,u);
            //                 const idType& uOffset = offsets[u];

            //                 // only if the u belongs to the region
            //                 if(regionMask[u]==regionID){
            //                     if(uOffset>vOffset){
            //                         if(processed.count(u)==0)
            //                             unvisitedUpperLink.push_back(u);
            //                         else
            //                             visitedUpperLink.push_back(u);
            //                     }
            //                 }
            //             }

            //             // return sucess
            //             return 1;
            //         }
            //     }

            //     this->printErr("Unable to find saddle ????");
            //     return 0;
            // }

            // template<typename idType>
            // int removeExtremumInsideRegionWithCarving(
            //     idType* offsets,

            //     const ttk::Triangulation* triangulation,
            //     const idType& regionID,
            //     const idType* regionMask,
            //     const idType& extremumIndex
            // ) const {
            //     // find saddle
            //     idType saddleIndex=-1;
            //     std::vector<idType> visitedUpperLink;
            //     std::vector<idType> unvisitedUpperLink;
            //     this->findSaddleInsideRegion<idType>(
            //         saddleIndex,
            //         visitedUpperLink,
            //         unvisitedUpperLink,

            //         triangulation,
            //         regionID,
            //         regionMask,
            //         offsets,
            //         extremumIndex
            //     );

            //     // NOTE: this can even handle monkey saddles as long as extrema are removed in acending persistence order

            //     // get separatrix towards extremum
            //     std::vector<idType> separatrix0;
            //     this->computeSeparatrixPath<idType>(
            //         separatrix0,

            //         triangulation,
            //         regionID,
            //         regionMask,
            //         offsets,
            //         saddleIndex,
            //         visitedUpperLink,
            //         offsets[extremumIndex]
            //     );

            //     // get separatrix towards other extremum
            //     std::vector<idType> separatrix1;
            //     this->computeSeparatrixPath<idType>(
            //         separatrix1,

            //         triangulation,
            //         regionID,
            //         regionMask,
            //         offsets,
            //         saddleIndex,
            //         unvisitedUpperLink,
            //         offsets[extremumIndex]
            //     );

            //     idType orderIdx = offsets[extremumIndex];
            //     // invert oder on first separatrix
            //     for(idType i=separatrix0.size()-1; i>=0; i--)
            //         offsets[separatrix0[i]] = orderIdx++;

            //     // set saddle offset
            //     offsets[saddleIndex] = orderIdx++;

            //     // add offsets of other separatrix except last element
            //     for(idType i=0, j=separatrix1.size()-1; i<j; i++)
            //         offsets[separatrix1[i]] = orderIdx++;

            //     return 1;
            // }

            /**
             * This function computes the local offsets for a region that corresponds to a removed extremum which implies special boundary conditions, i.e., either the positive (negative) boundary contains only the saddle value, and the negative (positive) boundary contains all vertices that have a neighbor outside the region
             */
            template<typename idType, typename PropagationType>
            int computeLocalOffsetsOfRegion(
                idType* localOffsets,
                idType* tempOffsets,

                const ttk::Triangulation* triangulation,
                const idType* regionMask,
                const idType* inputOffsets,
                const PropagationType* propagationData,
                const bool removeMaxima
            ) const {

                std::vector<idType> boundaryWithoutLastSaddle(propagationData->region.size(),-1);
                {
                    size_t nBoundaryVertices = 0;
                    for(size_t i=0, j=propagationData->region.size(); i<j; i++){
                        const idType& v = propagationData->region[i];

                        idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                        // if some neighbor u of v is outside the region add v
                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);
                            if(regionMask[u]!=propagationData->extremumIndex && u!=propagationData->lastEncounteredSaddle){
                                boundaryWithoutLastSaddle[nBoundaryVertices++]=v;
                                break;
                            }
                        }
                    }

                    // check if there is at least one boundary vertex
                    if(nBoundaryVertices>0){
                        boundaryWithoutLastSaddle.resize(nBoundaryVertices);
                    } else {
                        this->printWrn("Unable to determine complementary saddle boundary.");
                        for(size_t i=0, j=propagationData->region.size(); i<j; i++)
                            boundaryWithoutLastSaddle[i] = propagationData->region[i];
                    }
                }

                // compute breadth first search distance from last encountered saddle
                this->computeBreadthFirstSearchField<idType>(
                    localOffsets,

                    triangulation,
                    regionMask,
                    propagationData->extremumIndex,
                    propagationData->region,
                    propagationData->lastEncounteredSaddle
                );

                // grow connected region from most distant vertex while maximizing distance towards saddle
                const idType& maxBoundaryIndex = *std::max_element(boundaryWithoutLastSaddle.begin(), boundaryWithoutLastSaddle.end(), LessComparator<idType,idType>(localOffsets));

                for(size_t i=0, j=propagationData->region.size(); i<j; i++)
                    tempOffsets[propagationData->region[i]] = localOffsets[propagationData->region[i]];

                this->growRegionBasedOnFieldMaximum<idType>(
                    localOffsets,

                    triangulation,
                    regionMask,
                    propagationData->extremumIndex,
                    propagationData->region,
                    maxBoundaryIndex,
                    tempOffsets
                );

                // int it = 0;
                // while(true){
                //     // check if we created new unwanted maxima
                //     std::vector<idType> regionMinima;
                //     std::vector<idType> regionMaxima;
                //     for(idType i=0; i<propagationData->region.size(); i++){
                //         const idType& v = propagationData->region[i];
                //         const idType& vOffset = localOffsets[v];

                //         bool hasSmallerNeighbor = false;
                //         bool hasLargerNeighbor = false;
                //         bool ambiguous = false;

                //         idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                //         for(size_t n=0; n<nNeighbors; n++){
                //             idType u;
                //             triangulation->getVertexNeighbor(v,n,u);

                //             if(outputOffsets[u]!=propagationData->extremumIndex)
                //                 continue;

                //             const idType& uOffset = localOffsets[u];
                //             if( uOffset<vOffset )
                //                 hasSmallerNeighbor = true;
                //             else if( uOffset > vOffset )
                //                 hasLargerNeighbor = true;
                //             else
                //                 ambiguous = true;
                //         }

                //         if(!hasSmallerNeighbor && hasLargerNeighbor){
                //             regionMinima.push_back(v);
                //         } else if(hasSmallerNeighbor && !hasLargerNeighbor)
                //             regionMaxima.push_back(v);
                //     }

                //     std::vector<idType> regionMinimaArtifacts(regionMinima.size()-1);
                //     for(size_t i=0,q=0; i<regionMinima.size(); i++){
                //         const idType& v = regionMinima[i];
                //         if(v!=maxBoundaryIndex && v!=propagationData->lastEncounteredSaddle)
                //             regionMinimaArtifacts[q++] = v;
                //     }

                //     std::vector<idType> regionMaximaArtifacts(regionMaxima.size()-1);
                //     for(size_t i=0,q=0; i<regionMaxima.size(); i++){
                //         const idType& v = regionMaxima[i];
                //         if(v!=maxBoundaryIndex && v!=propagationData->lastEncounteredSaddle)
                //             regionMaximaArtifacts[q++] = v;
                //     }

                //     // this->printWrn(std::to_string(regionMinima.size())+" "+std::to_string(regionMaxima.size()));

                //     if( (removeMaxima && regionMaximaArtifacts.size()==0) || (!removeMaxima && regionMinimaArtifacts.size()==0) )
                //         break;

                //     this->printWrn("TODO: Disambiguation introduced new artifacts that have to be removed with an additional iteration:"+std::to_string(it++));
                //     // this->printWrn(std::to_string(regionMinimaArtifacts.size())+" "+std::to_string(regionMaximaArtifacts.size()));
                //     // this->printWrn(std::to_string(propagationData->region.size()));
                //     // this->printErr(std::to_string(propagationData->extremumIndex));
                //     // this->printErr(std::to_string(propagationData->lastEncounteredSaddle));

                //     // if(it>1) break;

                //     // printVector<idType>("region", propagationData->region);
                //     // for(size_t i=0; i<propagationData->region.size(); i++){
                //     //     auto v = propagationData->region[i];
                //     //     this->printWrn(std::to_string(v)+" = "+std::to_string(localOffsets[v]));
                //     //     size_t nNeighbors = triangulation->getVertexNeighborNumber(v);
                //     //     for(size_t j=0; j<nNeighbors; j++){
                //     //         idType u;
                //     //         triangulation->getVertexNeighbor(v,j,u);

                //     //         if(outputOffsets[u]==propagationData->extremumIndex){
                //     //             this->printWrn("    -> "+std::to_string(u));
                //     //         }
                //     //     }
                //     // }

                //     // CASE REGION.SIZE = 1 // not possible?

                //     // requires carving
                //     {
                //         idType regionSize = propagationData->region.size();
                //         idType maxIndex = regionSize*(regionMaximaArtifacts.size()+2);

                //         // force global extrema
                //         localOffsets[maxBoundaryIndex] = -1;
                //         localOffsets[propagationData->lastEncounteredSaddle] = maxIndex;

                //         // force persistence ordering
                //         for(size_t i=0; i<regionMaximaArtifacts.size(); i++)
                //             localOffsets[regionMaximaArtifacts[i]] = (i+1)*regionSize;

                //         //  remove maxima in acending persistence order
                //         for(size_t m=0; m<regionMaximaArtifacts.size(); m++){
                //             this->removeExtremumInsideRegionWithCarving<idType>(
                //                 localOffsets,

                //                 triangulation,
                //                 propagationData->extremumIndex,
                //                 outputOffsets,
                //                 regionMaximaArtifacts[m]
                //             );
                //         }
                //     }
                // }

                return 1;
            }

            template<typename idType, typename PropagationType>
            int computeLocalOffsetsOfRegions(
                idType* localOffsets,
                idType* tempOffsets,

                const ttk::Triangulation* triangulation,
                const idType* regionMask,
                const idType* inputOffsets,
                const std::vector<PropagationType*>& propagationList,
                const bool removeMaxima
            ) const {
                ttk::Timer t;
                this->printMsg( "Computing local order of regions", 0, 0, this->threadNumber_, debug::LineMode::REPLACE );

                // init local offsets
                const idType nVertices = triangulation->getNumberOfVertices();
                const idType nPropagations = propagationList.size();

                #pragma omp parallel for num_threads(this->threadNumber_)
                for(idType i=0; i<nVertices; i++)
                    localOffsets[i] = -1;

                #pragma omp parallel num_threads(this->threadNumber_)
                #pragma omp single
                for(size_t i=0; i<nPropagations; i++){
                    if(propagationList[i]->isTerminated)
                        continue;

                    #pragma omp task firstprivate(i)
                    this->computeLocalOffsetsOfRegion<idType,PropagationType>(
                        localOffsets,
                        tempOffsets,

                        triangulation,
                        regionMask,
                        inputOffsets,
                        propagationList[i],
                        removeMaxima
                    );
                }

                this->printMsg(
                    "Computing local order of regions",
                    1,
                    t.getElapsedTime(),
                    this->threadNumber_
                );

                return 1;
            }

            template<typename idType, typename PropagationType>
            int removeExtrema(
                idType* outputOffsets,
                idType* regionMask,
                bool& hasRemovedExtrema,

                const ttk::Triangulation* triangulation,
                const idType* preservedCriticalPointIndices,
                const size_t& nPreservedCriticalPointIndices,
                const idType* inputOffsets,
                const bool& removeMaxima
            ) const {

                std::vector<idType> discardedMinima;
                std::vector<idType> discardedMaxima;

                const idType nVertices = triangulation->getNumberOfVertices();

                // Classify Critical Points
                this->classifyExtrema<idType>(
                    discardedMinima,
                    discardedMaxima,

                    triangulation,
                    inputOffsets,
                    preservedCriticalPointIndices,
                    nPreservedCriticalPointIndices
                );

                if(
                    (removeMaxima && discardedMaxima.size()<1)
                    || (!removeMaxima && discardedMinima.size()<1)
                ){
                    return 1;
                }

                hasRemovedExtrema = true;

                // compute regions
                std::vector<PropagationType*> propagationList;
                this->computeRegions<idType,PropagationType>(
                    regionMask,
                    propagationList,

                    triangulation,
                    inputOffsets,
                    removeMaxima ? discardedMaxima : discardedMinima
                );

                // compute local order of regions
                std::vector<idType> localOffsets(nVertices,-1);
                this->computeLocalOffsetsOfRegions<idType,PropagationType>(
                    localOffsets.data(),
                    outputOffsets,

                    triangulation,
                    regionMask,
                    inputOffsets,
                    propagationList,
                    removeMaxima
                );

                // for(size_t i=0; i<discardedMaxima.size(); i++)
                //     outputOffsets[discardedMaxima[i]] = 999999;


                // compute output offsets
                {
                    // use mask as temporary array
                    #pragma omp parallel for num_threads(this->threadNumber_)
                    for(idType i=0; i<nVertices; i++)
                        regionMask[i] = inputOffsets[i];

                    const idType nPropagations = propagationList.size();

                    #pragma omp parallel for num_threads(this->threadNumber_)
                    for(size_t p=0; p<nPropagations; p++){
                        const auto* propagation = propagationList[p];
                        if(!propagation->isTerminated){
                            for(const auto& i : propagation->region)
                                regionMask[i] = regionMask[propagation->lastEncounteredSaddle];

                            // if(removeMaxima)
                                localOffsets[propagation->lastEncounteredSaddle]=std::numeric_limits<idType>::max();
                            // else
                            //     localOffsets[propagation->lastEncounteredSaddle]=std::numeric_limits<idType>::min();
                        }
                    }

                    // for(size_t i=0; i<nVertices; i++)
                    //     outputOffsets[i] = localOffsets[i];

                    // TODO: store flag in propagation data
                    if(removeMaxima){
                        this->computeGlobalOffsets<idType,idType>(
                            outputOffsets,

                            triangulation->getNumberOfVertices(),
                            DataMinOffsetComparator<idType,idType>(regionMask, localOffsets.data())
                        );
                    } else {
                        this->computeGlobalOffsets<idType,idType>(
                            outputOffsets,

                            triangulation->getNumberOfVertices(),
                            DataMaxOffsetComparator<idType,idType>(regionMask, localOffsets.data())
                        );
                    }
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
                const idType& nPreservedCriticalPointIndices
            ) const {

                ttk::Timer globalTimer;

                idType nVertices = triangulation->getNumberOfVertices();

                std::vector<idType> inputOffsets(nVertices);
                std::vector<idType> regionMask(nVertices);

                // compute initial offsets if not exisitng (currently this done by default)
                this->computeGlobalOffsets<dataType,idType>(
                    inputOffsets.data(),

                    nVertices,
                    DataMinOffsetComparator<dataType,idType>(inputScalars,inputOffsetsCorrupted)
                    // LessComparator<dataType,idType>(inputScalars)
                );

                size_t iteration=0;
                std::vector<dataType> inputScalars2(nVertices);

                while(true){
                    this->printWrn("Iteration: "+std::to_string(iteration++));

                    bool hasExtremaToRemove = false;

                    // Maxima
                    this->removeExtrema<idType, PropagationData<idType,LessComparator<idType,idType>>>(
                        outputOffsets,
                        regionMask.data(),
                        hasExtremaToRemove,

                        triangulation,
                        preservedCriticalPointIndices,
                        nPreservedCriticalPointIndices,
                        inputOffsets.data(),
                        true
                    );

                    // init offsets
                    std::vector<dataType> inputScalars2(nVertices);
                    #ifdef TTK_ENABLE_OPENMP
                    #pragma omp parallel for num_threads(this->threadNumber_)
                    #endif
                    for(idType v=0; v<nVertices; v++){
                        inputOffsets[v] = outputOffsets[v];
                        inputScalars2[v] = outputScalars[v];
                    }

                    // Minima
                    this->removeExtrema<idType, PropagationData<idType,GreaterComparator<idType,idType>>>(
                        outputOffsets,
                        regionMask.data(),
                        hasExtremaToRemove,

                        triangulation,
                        preservedCriticalPointIndices,
                        nPreservedCriticalPointIndices,
                        inputOffsets.data(),
                        false
                    );

                    for(idType v=0; v<nVertices; v++){
                        inputOffsets[v] = outputOffsets[v];
                        inputScalars2[v] = outputScalars[v];
                    }

                    if(!hasExtremaToRemove)
                        break;
                }

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
