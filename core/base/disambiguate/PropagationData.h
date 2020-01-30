/// \ingroup base
/// \class ttk::PropagationData
/// \author Julien Tierny <julien.tierny@lip6.fr>
/// \date July 2011.
///
/// \brief Union Find implementation for connectivity tracking.

#pragma once

#include <vector>
#include <boost/heap/fibonacci_heap.hpp>

namespace ttk {

  template<typename idType>
  struct PropagationData {

    // union find members
    PropagationData<idType>* parent;
    idType rank;

    // propagation data
    idType extremumIndex;
    boost::heap::fibonacci_heap< std::pair<idType,idType> > queue;
    idType lastEncounteredSaddle{-1};
    bool isTerminated{false};
    std::vector<idType> region;
    idType regionWriteIndex;

    inline explicit PropagationData() {
        this->parent = this;
        this->rank = 0;
    }

    // PropagationData(const PropagationData&) = delete;
    // PropagationData& operator=(const PropagationData&) = delete;

    inline PropagationData *find(){
        if(this->parent == this)
            return this;
        else {
            decltype(this->parent) tmp = this->parent->find();

            #pragma omp atomic write
            this->parent = tmp;

            return this->parent;
        }
    }

    static inline PropagationData<idType>* unify(
        PropagationData<idType>* uf0,
        PropagationData<idType>* uf1
    ){
        uf0 = uf0->find();
        uf1 = uf1->find();

        // if(uf0 == uf1) {
        //     return uf0;
        // } else if(uf0->rank > uf1->rank) {
            uf1->setParent(uf0);

            uf1->isTerminated = true;
            uf0->queue.merge(uf1->queue);

            return uf0;
        // } else if(uf0->rank < uf1->rank) {
        //     uf0->setParent(uf1);

        //     uf0->isTerminated = true;
        //     uf1->queue.merge(uf0->queue);

        //     return uf1;
        // } else {
        //     uf1->setParent(uf0);
        //     uf0->setRank(uf0->rank + 1);

        //     uf1->isTerminated = true;
        //     uf0->queue.merge(uf1->queue);

        //     return uf0;
        // }

        return uf0;
    }

    inline void setParent(PropagationData<idType>* parent) {
        #pragma omp atomic write
        this->parent = parent;
    }

    inline void setRank(const idType& rank) {
        #pragma omp atomic write
        this->rank = rank;
    }

  };
} // namespace ttk