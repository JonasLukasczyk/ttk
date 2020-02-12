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
    std::vector<idType> region;
    boost::heap::fibonacci_heap< std::pair<idType,idType> > queue;

    inline explicit Propagation() {
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

        if(uf0 == uf1)
            return uf0;

        Propagation<idType>* master = nullptr;
        Propagation<idType>* slave  = nullptr;

        // determine master and slave based on rank
        // if(uf0 == uf1) {
        //     return uf0;
        // } else if(uf0->rank > uf1->rank) {
        //     master = uf0;
        //     slave = uf1;
        // } else if(uf0->rank < uf1->rank) {
        //     master = uf1;
        //     slave = uf0;
        // } else {
        //     master = uf0;
        //     slave = uf1;
        //     master->setRank(master->rank + 1);
        // }

        // determine master and slave based on region size
        if(uf0->region.size() > uf1->region.size()) {
            master = uf0;
            slave = uf1;
        } else {
            master = uf1;
            slave = uf0;
        }

        // update union find tree
        slave->setParent(master);

        // mark slave as terminated
        slave->isTerminated = true;

        // merge f. heaps
        master->queue.merge(slave->queue);

        // merge regions
        idType oldSize = master->region.size();
        idType newSize = oldSize + slave->region.size();
        master->region.resize(newSize);
        for(idType i=oldSize,j=0; i<newSize; i++,j++)
            master->region[i] = slave->region[j];

        return master;
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