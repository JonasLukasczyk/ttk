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

  template<typename idType, typename ComparatorType>
  struct PropagationData {
    PropagationData<idType, ComparatorType>* parent;

    const idType extremumIndex;
    ComparatorType comperator;

    boost::heap::fibonacci_heap<idType, boost::heap::compare<ComparatorType>> queue;
    idType lastEncounteredSaddle{-1};
    bool isTerminated{false};

    std::vector<idType> region;
    idType regionWriteIndex;

    inline explicit PropagationData(
        const idType& extremumIndex,
        const idType* offsets
    ) : extremumIndex(extremumIndex) {
        this->parent = this;
        this->comperator.data = offsets;
        this->queue = boost::heap::fibonacci_heap<idType, boost::heap::compare<ComparatorType>>(this->comperator);
    }

    PropagationData() = delete;
    PropagationData(const PropagationData&) = delete;
    PropagationData& operator=(const PropagationData&) = delete;

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

    static inline PropagationData<idType, ComparatorType>* unify(
        PropagationData<idType, ComparatorType>* uf0,
        PropagationData<idType, ComparatorType>* uf1
    ){
        uf0 = uf0->find();
        uf1 = uf1->find();

        uf1->setParent(uf0);

        uf1->isTerminated = true;
        uf0->queue.merge(uf1->queue);

        uf1->find();

        return uf0;
    }

    inline void setParent(PropagationData<idType, ComparatorType>* parent) {
        #pragma omp atomic write
        this->parent = parent;
    };
  };
} // namespace ttk