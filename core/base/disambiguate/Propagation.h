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
    Propagation<idType>* parent{nullptr};
    int rank{0};
    signed char terminated{0};
    signed char temp{0};
    idType rep{0};
    mutable idType nIterations{0};

    std::vector<idType> saddles;

    // propagation data
    idType extremumIndex{-1};
    idType lastEncounteredSaddle{-1};
    idType regionWriteIndex{0};
    idType regionSize{0};
    std::vector<idType> region;
    boost::heap::fibonacci_heap< std::pair<idType,idType> > queue;

    // inline explicit Propagation() {
    // }

    // Propagation(const Propagation& that){
    //     this->extremumIndex = that.extremumIndex;
    // };

    // Propagation& operator=(const Propagation& that){
    //     this->extremumIndex = that.extremumIndex;
    // };

    inline Propagation *find(){
        if(this->parent == nullptr)
            return this;
        else {
            auto tmp = this->parent->find();

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

        Propagation<idType>* master = uf0;
        Propagation<idType>* slave  = uf1;

        // determine master and slave based on rank
        if(uf0->rank == uf1->rank) {
            master->setRank(master->rank + 1);
        } else if(uf0->rank < uf1->rank) {
            master = uf1;
            slave = uf0;
        }

        // update union find tree
        slave->setParent(master);

        // merge f. heaps
        master->queue.merge(slave->queue);

        // merge regions
        master->regionSize += slave->regionSize;

        slave->terminated = 0;
        master->terminated = 0;

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