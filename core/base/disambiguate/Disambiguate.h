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

#include <limits>
#include <queue>
#include <unordered_set>
#include <unordered_map>

typedef ttk::SimplexId ttkInt;

template <typename dataType>
void printVector(std::string prefix, std::vector<dataType>& v){
    std::cout<<prefix<<" ("<<v.size()<<"): ";
    for(size_t i=0, j=v.size(); i<j; i++)
        std::cout<<v[i]<<" ";
    std::cout<<std::endl<<std::endl;
}

namespace ttk {

    class Disambiguate : virtual public Debug {

        public:

            template<typename idType>
            struct OffsetComparator {
                const idType* offsets;
                OffsetComparator(){};
                OffsetComparator(const idType* o) : offsets(o){};
                int operator() (const idType& i, const idType& j){
                    return this->offsets[i]<this->offsets[j];
                }
            };

            // struct to store propagation data such as the visited region and saddles
            template<typename idType>
            struct PropagationData{
                idType extremumIndex;
                std::priority_queue< idType, std::vector<idType>, OffsetComparator<idType> > queue;
                std::vector<idType> region;
                idType lastEncounteredSaddle;
                bool isTerminated{false};

                PropagationData(idType extremumIndex, idType* offsets){
                    this->extremumIndex = extremumIndex;
                    this->queue = std::priority_queue< idType, std::vector<idType>, OffsetComparator<idType>>(offsets);
                };
            };

            Disambiguate(){
                this->setDebugMsgPrefix("Disambiguate"); // inherited from Debug: prefix will be printed at the beginning of every msg
            };
            ~Disambiguate(){};

            template<typename dataType,typename idType>
            int computeGlobalOffsets(
                idType* outputOffsets,

                const idType& nVertices,
                const dataType* scalars,
                const idType* inputOffsets=nullptr
            ) const {
                ttk::Timer t;
                this->printMsg(
                    "Computing global offset scalar field",
                    0,
                    0,
                    this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                std::vector<idType> sortedIndices(nVertices);
                for(idType i=0; i<nVertices; i++)
                    sortedIndices[i] = i;

                struct MorseComparator {
                    const dataType* scalars;
                    MorseComparator(const dataType* s) : scalars(s){};
                    int operator() (const idType& i, const idType& j){
                        const dataType& iScalar = this->scalars[i];
                        const dataType& jScalar = this->scalars[j];
                        return iScalar==jScalar
                                ? i<j
                                : iScalar<jScalar;
                    }
                };

                struct MorseComparator2 {
                    const dataType* scalars;
                    const idType* offsets;
                    MorseComparator2(const dataType* s, const idType* o) : scalars(s), offsets(o){};
                    int operator() (const idType& i, const idType& j){
                        const dataType& iScalar = this->scalars[i];
                        const dataType& jScalar = this->scalars[j];
                        const idType& iOffset = this->offsets[i];
                        const idType& jOffset = this->offsets[j];
                        return iScalar==jScalar
                                ? iOffset==jOffset
                                    ? i<j
                                    : iOffset<jOffset
                                : iScalar<jScalar;
                    }
                };

                MorseComparator c(scalars);
                MorseComparator2 c2(scalars, inputOffsets);

                if(!inputOffsets)
                    std::sort(sortedIndices.begin(), sortedIndices.end(), c);
                else
                    std::sort(sortedIndices.begin(), sortedIndices.end(), c2);

                for(idType i=0; i<nVertices; i++)
                    outputOffsets[sortedIndices[i]] = i;

                this->printMsg(
                    "Computing global offset scalar field",
                    1,
                    t.getElapsedTime(),
                    this->threadNumber_
                );

                return 1;
            }

            template<typename idType>
            int classifyExtrema(
                std::vector<idType>& preservedMinima,
                std::vector<idType>& discardedMinima,
                std::vector<idType>& preservedMaxima,
                std::vector<idType>& discardedMaxima,

                const ttk::Triangulation* triangulation,
                const idType* inputOffsets,
                const idType* preservedCriticalPointIndices,
                const size_t& nPreservedCriticalPointIndices
            ) const {
                ttk::Timer t;
                this->printMsg("Classifying extrema",0,0,this->threadNumber_,debug::LineMode::REPLACE);

                const idType nVertices = triangulation->getNumberOfVertices();

                // Identify critical points (TODO: Currently not parallel for determinism)
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

                    auto& presrevedExtrema = isMaximum ? preservedMaxima : preservedMinima;
                    auto& discardedExtrema = isMaximum ? discardedMaxima : discardedMinima;

                    if(isExtremum){
                        if(hasToBePreserved){
                            #pragma omp critical
                            presrevedExtrema.push_back(v);
                        } else {
                            #pragma omp critical
                            discardedExtrema.push_back(v);
                        }
                    }
                }

                this->printMsg("Classifying extrema",1,t.getElapsedTime(),this->threadNumber_);

                return 1;
            }

            template<typename dataType, typename idType>
            int computeOutputScalars(
                dataType* outputScalars,

                const dataType* inputScalars,
                const std::unordered_map<idType, PropagationData<idType>>& extremumIndexToPropagationDataMap
            ) const {
                ttk::Timer t;
                this->printMsg("Computing output scalar field",0,0,this->threadNumber_,debug::LineMode::REPLACE);

                #pragma omp parallel num_threads(this->threadNumber_)
                #pragma omp single
                for(const auto& it: extremumIndexToPropagationDataMap){
                    if(it.second.isTerminated)
                        continue;

                    const ttk::Disambiguate::PropagationData<idType>* propagationData = &(it.second);

                    #pragma omp task firstprivate(propagationData)
                    {
                        const auto& region = propagationData->region;
                        const auto& saddleScalar = inputScalars[propagationData->lastEncounteredSaddle];
                        for(size_t i=0, j=region.size(); i<j; i++)
                            outputScalars[region[i]] = saddleScalar;
                    }
                }

                this->printMsg("Computing output scalar field",1,t.getElapsedTime(),this->threadNumber_);

                return 1;
            }

            /**
             * Current Limitation: monkey saddle handing (especially during disambiguation)
             *
             * Possible improvements:
             *      + add vertices only once to queue
             *      + replace queue with fibonachi heap
             *      + build tree structure to prevent region copy
             *      + each thread has its own temporary data
             */
            template<typename idType>
            int computeRegion(
                idType* outputOffsets,
                std::unordered_map<idType, PropagationData<idType>>& extremumIndexToPropagationDataMap,

                const ttk::Triangulation* triangulation,
                const idType* inputOffsets,
                const idType& extremumIndex
            ) const {

                // retrieve propagation data of current extrema
                auto propagationDataIt = extremumIndexToPropagationDataMap.find(extremumIndex);
                if(propagationDataIt == extremumIndexToPropagationDataMap.end()){
                    this->printErr("Unable to retrieve propagation information for extrema "+std::to_string(extremumIndex));
                    return 0;
                }
                auto& propagationData = propagationDataIt->second;

                // add extremumIndex to queue
                auto& queue = propagationData.queue;
                queue.push(extremumIndex);

                // grow region until it reaches a saddle and then decide if it should continue
                while(!queue.empty()){
                    const idType v = queue.top();
                    queue.pop();

                    if(outputOffsets[v]==extremumIndex)
                        continue;

                    // get scalar value of current vertex
                    const idType& vOffset = inputOffsets[v];

                    // add neighbors to queue AND check if v is a saddle
                    bool isSaddle = false;
                    idType nNeighbors = triangulation->getVertexNeighborNumber( v );

                    idType numberOfLargerNeighbors = 0;
                    idType numberOfLargerNeighborsThisThreadVisited = 0;
                    for(idType n=0; n<nNeighbors; n++){
                        idType u;
                        triangulation->getVertexNeighbor(v,n,u);

                        const idType& uOffset = inputOffsets[u];

                        // if lower neighbor
                        if(uOffset<vOffset)
                            queue.push( u );
                        else {
                            numberOfLargerNeighbors++;
                            if(outputOffsets[u]!=extremumIndex)
                                isSaddle = true;
                            else
                                numberOfLargerNeighborsThisThreadVisited++;
                        }
                    }

                    // if v is a saddle we have to check if the current thread is the last visitor
                    if(isSaddle){

                        propagationData.lastEncounteredSaddle = v;

                        // * this check is performed by synchronously adding the number of larger vertices that the current thread visited to the saddle outputOffset
                        // * if after this synchronous operation the outputOffset at the saddle equals the total number of larger vertices then this must be the last thread that visited the saddle
                        // * Note: the offset stores the temporary value -1 for unvisited vertices and only values >=0 for processed vertices -> the number of larger neighbors of v visited by this thread is actually substracted from the offset to prevent the creation of a separate mask
                        idType numberOfRegisteredLargerVerticesAsStoredInOffset=0;
                        #pragma omp atomic capture
                        {
                            outputOffsets[v] -= numberOfLargerNeighborsThisThreadVisited;
                            numberOfRegisteredLargerVerticesAsStoredInOffset = outputOffsets[v];
                        }

                        // if this thread did not register the last remaining larger vertices then terminate propagation
                        if(numberOfRegisteredLargerVerticesAsStoredInOffset!=-numberOfLargerNeighbors-1)
                            break;

                        // Otherwise merge propagation data
                        {
                            // get all extrema indices that reach the current saddle
                            std::unordered_set<idType> terminatedExtremaIndices;
                            for(idType n=0; n<nNeighbors; n++){
                                idType u;
                                triangulation->getVertexNeighbor(v,n,u);
                                const idType& uOffset = inputOffsets[u];
                                const idType& extremumIndex2 = outputOffsets[u];
                                if(uOffset>vOffset && extremumIndex2!=extremumIndex)
                                    terminatedExtremaIndices.insert(extremumIndex2);
                            }

                            // merge propagation data
                            for(const idType& terminatedExtremaIndex : terminatedExtremaIndices){
                                auto terminatedPropagationDataIt = extremumIndexToPropagationDataMap.find(terminatedExtremaIndex);
                                if(terminatedPropagationDataIt==extremumIndexToPropagationDataMap.end()){
                                    this->printErr("Unable to retrieve propagation information for extrema "+std::to_string(terminatedExtremaIndex));
                                    return 0;
                                }

                                auto& terminatedPropagationData = terminatedPropagationDataIt->second;
                                terminatedPropagationData.isTerminated = true;

                                // add terminated region to current propagation region and override old labels
                                for(const auto& u: terminatedPropagationData.region){
                                    outputOffsets[u] = extremumIndex;
                                    propagationData.region.push_back(u);
                                }

                                // add terminated queue elements to current queue
                                while(!terminatedPropagationData.queue.empty()){
                                    const idType u = terminatedPropagationData.queue.top();
                                    terminatedPropagationData.queue.pop();
                                    queue.push(u);
                                }
                            }
                        }
                    }

                    // grow region
                    propagationData.region.push_back(v);

                    // mark vertex as visited and continue
                    outputOffsets[v] = extremumIndex;
                }

                return 1;
            }

            template<typename idType>
            int computeRegions(
                idType* outputOffsets,
                std::unordered_map<idType, PropagationData<idType>>& extremumIndexToPropagationDataMap,

                const ttk::Triangulation* triangulation,
                const std::vector<idType>& discardedMaxima,
                const idType* inputOffsets
            ) const {
                ttk::Timer t;
                this->printMsg(
                    "Processing "+std::to_string(discardedMaxima.size())+" points",
                    0,
                    0,
                    this->threadNumber_,
                    debug::LineMode::REPLACE
                );

                // compute regions
                #pragma omp parallel num_threads(this->threadNumber_)
                #pragma omp single
                for(size_t i=0; i<discardedMaxima.size(); i++){
                    #pragma omp task firstprivate(i)
                    {
                        this->computeRegion<idType>(
                            outputOffsets,
                            extremumIndexToPropagationDataMap,

                            triangulation,
                            inputOffsets,
                            discardedMaxima[i]
                        );
                    }
                }

                // add last encountered saddle to unterminated regions
                for(auto& it : extremumIndexToPropagationDataMap){
                    if(!it.second.isTerminated)
                        it.second.region.push_back(it.second.lastEncounteredSaddle);
                }

                this->printMsg(
                    "Processing "+std::to_string(discardedMaxima.size())+" points",
                    1,
                    t.getElapsedTime(),
                    this->threadNumber_
                );
            }

            template<typename idType>
            int computeBreadthFirstSearchIndices(
                idType* outputOffsets,
                idType* localOffsets,

                const ttk::Triangulation* triangulation,
                const idType& extremumIndex,
                const std::vector<idType>& vertices,
                const idType& originIndex
            ) const {
                std::queue<idType> queue;
                queue.push(originIndex);

                idType q = 0;
                while(!queue.empty()){
                    idType v = queue.front();
                    queue.pop();
                    localOffsets[v] = q++;

                    idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                    for(idType nIndex=0; nIndex<nNeighbors; nIndex++){
                        idType u;
                        triangulation->getVertexNeighbor(v,nIndex,u);
                        // not yet discovered
                        if(outputOffsets[u]==extremumIndex){
                            outputOffsets[u] = -1;
                            queue.push( u );
                        }
                    }
                }

                // restore outputOffsets
                for(size_t i=0, j=vertices.size(); i<j; i++)
                    outputOffsets[vertices[i]] = extremumIndex;

                return 1;
            }

            template<typename idType>
            int growRegionBasedOnFieldMaximum(
                idType* outputOffsets,
                idType* localOffsets,

                const ttk::Triangulation* triangulation,
                const idType& extremumIndex,
                const std::vector<idType>& vertices
            ) const {
                // init comparator
                struct ArrayComparator {
                    const idType* array;
                    ArrayComparator(const idType* array) : array(array){};
                    int operator() (const idType& i, const idType& j){
                        return this->array[i]<this->array[j];
                    }
                };
                ArrayComparator arrayComparator(localOffsets);

                // init priority queue
                std::priority_queue<
                    idType,
                    std::vector<idType>,
                    ArrayComparator
                > queue(arrayComparator);

                // compute boundary
                std::vector<idType> boundaryVertices;
                {
                    for(size_t i=0, j=vertices.size(); i<j; i++){
                        const idType& v = vertices[i];
                        idType nNeighbors = triangulation->getVertexNeighborNumber( v );

                        for(idType n=0; n<nNeighbors; n++){
                            idType u;
                            triangulation->getVertexNeighbor(v,n,u);

                            if(outputOffsets[u]!=extremumIndex){
                                boundaryVertices.push_back(v);
                                break;
                            }
                        }
                    }
                }

                // add maximum index to queue
                {
                    const auto& vertices_ = boundaryVertices.size()>0 ? boundaryVertices : vertices;
                    idType maxIndex=-1;
                    idType maxDistance = -1;
                    for(size_t i=0, j=vertices_.size(); i<j; i++){
                        const idType& v = vertices_[i];
                        if(maxDistance<localOffsets[v]){
                            maxDistance=localOffsets[v];
                            maxIndex = v;
                        }
                    }
                    queue.push( maxIndex );
                }

                idType q=-1;
                while(!queue.empty()){
                    idType v = queue.top();
                    queue.pop();
                    outputOffsets[v] = --q;

                    idType nNeighbors = triangulation->getVertexNeighborNumber( v );
                    for(idType n=0; n<nNeighbors; n++){
                        idType u;
                        triangulation->getVertexNeighbor(v,n,u);

                        // if u was not discovered yet
                        if(outputOffsets[u]==extremumIndex){
                            outputOffsets[u]=-1;
                            queue.push( u );
                        }
                    }
                }

                // finalize outputOffsets
                for(size_t i=0, j=vertices.size(); i<j; i++){
                    const idType& v = vertices[i];
                    localOffsets[v] = -outputOffsets[v]-1;
                    outputOffsets[v] = extremumIndex;
                }

                return 1;
            }

            template<typename idType>
            int computeLocalOffsetsOfRegion(
                idType* outputOffsets,
                idType* localOffsets,

                const ttk::Triangulation* triangulation,
                const idType* inputOffsets,
                const PropagationData<idType>& propagationData
            ) const {
                // compute breadth first search distance from last encountered saddle
                this->computeBreadthFirstSearchIndices<idType>(
                    outputOffsets,
                    localOffsets,

                    triangulation,
                    propagationData.extremumIndex,
                    propagationData.region,
                    propagationData.lastEncounteredSaddle
                );

                // grow connected region from most distant vertex while maximizing distance towards saddle
                this->growRegionBasedOnFieldMaximum<idType>(
                    outputOffsets,
                    localOffsets,

                    triangulation,
                    propagationData.extremumIndex,
                    propagationData.region
                );

                return 1;
            }

            template<typename idType>
            int computeLocalOffsetsOfRegions(
                idType* localOffsets,
                idType* outputOffsets,

                const ttk::Triangulation* triangulation,
                const idType* inputOffsets,
                const std::unordered_map<idType, PropagationData<idType>>& extremumIndexToPropagationDataMap
            ) const {
                ttk::Timer t;
                this->printMsg( "Computing local order of regions", 0, 0, this->threadNumber_, debug::LineMode::REPLACE );

                // #pragma omp parallel num_threads(this->threadNumber_)
                // #pragma omp single
                for(const auto& it : extremumIndexToPropagationDataMap){
                    if(it.second.isTerminated)
                        continue;

                    const ttk::Disambiguate::PropagationData<idType>* propagationData = &(it.second);

                    // #pragma omp task firstprivate(propagationData)
                    this->computeLocalOffsetsOfRegion(
                        outputOffsets,
                        localOffsets,

                        triangulation,
                        inputOffsets,
                        *propagationData
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

            template<typename dataType,typename idType>
            int simplify(
                dataType* outputScalars,
                idType* outputOffsets,

                const ttk::Triangulation* triangulation,
                const dataType* inputScalars,
                const idType* preservedCriticalPointIndices,
                const size_t& nPreservedCriticalPointIndices
            ) const {
                size_t nVertices = triangulation->getNumberOfVertices();

                std::vector<idType> inputOffsets(nVertices);

                // compute initial offsets if not exisitng (currently this done by default)
                this->computeGlobalOffsets<dataType,idType>(
                    inputOffsets.data(),

                    nVertices,
                    inputScalars
                );

                std::vector<idType> preservedMinima;
                std::vector<idType> discardedMinima;
                std::vector<idType> preservedMaxima;
                std::vector<idType> discardedMaxima;

                // Classify Critical Points
                this->classifyExtrema<idType>(
                    preservedMinima,
                    discardedMinima,
                    preservedMaxima,
                    discardedMaxima,

                    triangulation,
                    inputOffsets.data(),
                    preservedCriticalPointIndices,
                    nPreservedCriticalPointIndices
                );

                // init offsets
                #ifdef TTK_ENABLE_OPENMP
                #pragma omp parallel for num_threads(this->threadNumber_)
                #endif
                for(ttkInt v=0; v<nVertices; v++)
                    outputOffsets[v] = -1;

                // init critical point informations
                std::unordered_map<idType, PropagationData<idType>> extremumIndexToPropagationDataMap;
                for(size_t i=0; i<discardedMaxima.size(); i++)
                    extremumIndexToPropagationDataMap.insert({
                        discardedMaxima[i],
                        PropagationData<idType>(discardedMaxima[i],inputOffsets.data())
                    });

                // compute regions
                this->computeRegions<idType>(
                    outputOffsets,
                    extremumIndexToPropagationDataMap,

                    triangulation,
                    discardedMaxima,
                    inputOffsets.data()
                );

                // compute local order of regions
                std::vector<idType> localOffsets(triangulation->getNumberOfVertices(),-1);
                this->computeLocalOffsetsOfRegions<idType>(
                    localOffsets.data(),
                    outputOffsets,

                    triangulation,
                    inputOffsets.data(),
                    extremumIndexToPropagationDataMap
                );

                // for(size_t i=0; i<localOffsets.size(); i++){
                //     outputOffsets[i] = localOffsets[i];
                //     outputScalars[i] = 0;
                // }

                // for(auto it : extremumIndexToPropagationDataMap)
                //     outputScalars[it.second.lastEncounteredSaddle]=1;

                // compute output scalars
                this->computeOutputScalars<dataType,idType>(
                    outputScalars,

                    inputScalars,
                    extremumIndexToPropagationDataMap
                );

                // compute final global offset scalar field
                this->computeGlobalOffsets<dataType, idType>(
                    outputOffsets,

                    nVertices,
                    outputScalars,
                    localOffsets.data()
                );

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

// int ttk::Disambiguate::test(
// ) const {
//     this->printMsg("Test",0.5,12,this->threadNumber_);
//     return 1;
// }

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
