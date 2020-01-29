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
    PropagationData<idType>* parent;

    const idType extremumIndex;

    boost::heap::fibonacci_heap< std::pair<idType,idType> > queue;
    idType lastEncounteredSaddle{-1};
    bool isTerminated{false};

    std::vector<idType> region;
    idType regionWriteIndex;

    inline explicit PropagationData(
        const idType& extremumIndex
    ) : extremumIndex(extremumIndex) {
        this->parent = this;
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

    static inline PropagationData<idType>* unify(
        PropagationData<idType>* uf0,
        PropagationData<idType>* uf1
    ){
        uf0 = uf0->find();
        uf1 = uf1->find();

        uf1->setParent(uf0);

        uf1->isTerminated = true;
        uf0->queue.merge(uf1->queue);

        uf1->find();

        return uf0;
    }

    inline void setParent(PropagationData<idType>* parent) {
        #pragma omp atomic write
        this->parent = parent;
    };
  };
} // namespace ttk