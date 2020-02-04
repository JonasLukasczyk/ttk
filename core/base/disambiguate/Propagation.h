/// \ingroup base
/// \class ttk::Propagation
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date Feb 2020.
///
/// \brief TODO

#pragma once

#include <vector>
#include <boost/heap/fibonacci_heap.hpp>

namespace ttk {

  template<typename idType>
  struct Propagation {

    // union find members
    Propagation<idType>* parent{this};
    int rank{0};

    // propagation data
    idType extremumIndex{-1};
    idType lastEncounteredSaddle{-1};
    bool isTerminated{false};
    idType regionWriteIndex{0};
    idType regionSize{0};
    std::vector<idType> region;
    boost::heap::fibonacci_heap< std::pair<idType,idType> > queue;

    inline explicit Propagation() {
        this->parent = this;
    }

    // Propagation(const Propagation&) = delete;
    // Propagation& operator=(const Propagation&) = delete;

    inline Propagation *find(){
        if(this->parent == this)
            return this;
        else {
            decltype(this->parent) tmp = this->parent->find();

            #pragma omp atomic write
            this->parent = tmp;

            return this->parent;
        }
    }

    static inline Propagation<idType>* unify(
        Propagation<idType>* uf0,
        Propagation<idType>* uf1
    ){
        uf0 = uf0->find();
        uf1 = uf1->find();


        // if(uf0 == uf1) {
        //     return uf0;
        // } else if(uf0->rank > uf1->rank) {
            uf1->setParent(uf0);

            uf1->isTerminated = true;
            uf0->queue.merge(uf1->queue);
            uf0->regionSize += uf1->regionSize;

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

        // return uf0;
    }

    inline void setParent(Propagation<idType>* parent) {
        #pragma omp atomic write
        this->parent = parent;
    }

    inline void setRank(const idType& rank) {
        #pragma omp atomic write
        this->rank = rank;
    }
  };
} // namespace ttk