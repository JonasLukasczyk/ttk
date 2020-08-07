#pragma once

#include <vector>
#include <boost/heap/fibonacci_heap.hpp>

namespace ttk {

  template<typename IT>
  struct Propagation {

    // union find members
    Propagation<IT>* parent{this};

    // propagation data
    std::vector<IT> criticalPoints;
    boost::heap::fibonacci_heap< std::pair<IT,IT> > queue;

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

    static inline Propagation<IT>* unify(
        Propagation<IT>* uf0,
        Propagation<IT>* uf1,
        const IT* orderField
    ){
        Propagation<IT>* master = uf0->find();
        Propagation<IT>* slave  = uf1->find();

        // determine master and slave based on rank
        if(orderField[master->criticalPoints[0]] < orderField[slave->criticalPoints[0]]) {
            Propagation<IT>* temp = master;
            master = slave;
            slave = temp;
        }

        // update union find tree
        #pragma omp atomic write
        slave->parent = master;

        // merge f. heaps
        master->queue.merge(slave->queue);

        return master;
    }

  };
} // namespace ttk