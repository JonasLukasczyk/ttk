#pragma once

#include <vector>
#include <boost/heap/fibonacci_heap.hpp>

namespace ttk {

  template<typename idType>
  struct Propagation {

    // union find members
    Propagation<idType>* parent{this};

    std::vector<Propagation<idType>*> childBranches;
    Propagation<idType>* parentBranch{nullptr};
    int rank{0};
    signed char terminated{0};
    signed char simplified{0};
    signed char persistent{0};
    mutable idType nIterations{0};

    std::vector<idType> saddles;
    std::vector<idType> regionSizes;

    // propagation data
    idType extremumIndex{-1};
    idType lastEncounteredCriticalPoint{-1};
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
        if(this->parent == this)
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
        Propagation<idType>* master = uf0->find();
        Propagation<idType>* slave  = uf1->find();

        // determine master and slave based on rank
        if(uf0->rank == uf1->rank) {
            master->setRank(master->rank + 1);
        } else if(uf0->rank < uf1->rank) {
            Propagation<idType>* temp = master;
            master = slave;
            slave = temp;
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

    static inline Propagation<idType>* unify2(
        Propagation<idType>* uf0,
        Propagation<idType>* uf1
    ){
        Propagation<idType>* master = uf0->find();
        Propagation<idType>* slave  = uf1->find();

        // update union find tree
        slave->setParent(master);

        // update tree structure
        slave->parentBranch = master;
        master->childBranches.push_back(slave);

        // merge f. heaps
        master->queue.merge(slave->queue);

        // merge region sizes
        master->regionSize += slave->regionSize;

        // mark both as not terminated
        slave->terminated = 0;
        master->terminated = 0;

        return master;
    }

    static inline Propagation<idType>* unify3(
        Propagation<idType>* uf0,
        Propagation<idType>* uf1
    ){
        Propagation<idType>* master = uf0->find();
        Propagation<idType>* slave  = uf1->find();

        // update union find tree
        slave->setParent(master);

        // update tree structure
        slave->parentBranch = master;
        master->childBranches.push_back(slave);

        // merge region sizes
        master->regionSize += slave->regionSize;

        // mark both as not terminated
        slave->terminated = 0;
        master->terminated = 0;

        return master;
    }

    inline void setParent(Propagation<idType>* parent) {
        #pragma omp atomic write
        this->parent = parent;
    }

    inline void setParentRecursive(Propagation<idType>* parent) {
        this->parent = parent;
        for(auto c : this->childBranches)
            c->setParentRecursive(parent);
    }

    inline void setRank(const idType& rank) {
        #pragma omp atomic write
        this->rank = rank;
    }
  };
} // namespace ttk