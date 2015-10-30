#ifndef SPINNING_THREAD_BARRIER_HPP
#define SPINNING_THREAD_BARRIER_HPP

#include <atomic>

namespace warped {

class SpinningThreadBarrier {
public:
    SpinningThreadBarrier() : nwait_ (0), step_(0) {}

    void init(unsigned int n) { n_ = n; }

    bool wait() {
        unsigned int step = step_.load ();

        if (nwait_.fetch_add (1) == n_ - 1) {
            nwait_.store (0);
            step_.fetch_add (1);
            return true;
        } else {
            while (step_.load () == step);
            return false;
        }
    }

protected:
    /* Number of synchronized threads. */
    unsigned int n_;

    /* Number of threads currently spinning.  */
    std::atomic<unsigned int> nwait_;

    /* Number of barrier syncronizations completed so far,
     * it's OK to wrap.  */
    std::atomic<unsigned int> step_;
};

}

#endif
