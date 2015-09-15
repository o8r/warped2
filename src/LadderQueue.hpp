#ifndef LADDER_QUEUE_HPP
#define LADDER_QUEUE_HPP

/* Ladder Queue Library */

/* Include section */
#include <iostream>
#include <algorithm>
#include <list>
#include <set>
#include <vector>

#include "Event.hpp"

/* Configurable Ladder Queue parameters */
#define MAX_RUNG_CNT       8          //ref. sec 2.4 of ladderq paper
#define THRESHOLD          50         //ref. sec 2.3 of ladderq paper
#define MAX_BUCKET_CNT     THRESHOLD  //ref. sec 2.4 of ladderq paper
#define MIN_BUCKET_WIDTH   1

#define RUNG_BUCKET_CNT(x) (((x)==0) ? (rung_0_length_) : (MAX_BUCKET_CNT))

namespace warped {

class LadderQueue {
public:
    LadderQueue();
    std::vector<std::shared_ptr<Event>> begin(unsigned int count);
    bool erase(std::shared_ptr<Event> event);
    void insert(std::shared_ptr<Event> event);

#ifdef PARTIALLY_UNSORTED_EVENT_SET
    unsigned int lowestTimestamp();
#endif

private:
    bool createNewRung(unsigned int num_events, 
                        unsigned int init_start_and_cur_val, 
                        bool *is_bucket_width_static);
    void createRungForBottomTransfer(unsigned int start_val);
    bool recurseRung(unsigned int *index);


    /* Top variables */
    std::list<std::shared_ptr<Event>> top_;
    unsigned int max_ts_    = 0;
    unsigned int min_ts_    = 0;
    unsigned int top_start_ = 0;

    /* Rungs */
    std::vector<std::shared_ptr<std::list<std::shared_ptr<Event>>>> rung_[MAX_RUNG_CNT];

    //first rung. ref. sec 2.4 of ladderq paper
    unsigned int rung_0_length_ = 0;

    unsigned int n_rung_ = 0;
    unsigned int bucket_width_[MAX_RUNG_CNT];
    unsigned int last_nonempty_bucket_[MAX_RUNG_CNT];
    unsigned int r_start_[MAX_RUNG_CNT];
    unsigned int r_current_[MAX_RUNG_CNT];

    /* Bottom */
#ifdef PARTIALLY_UNSORTED_EVENT_SET
    std::list<std::shared_ptr<Event>> bottom_;
    unsigned int bottom_start_ = 0;
#else
    std::multiset<std::shared_ptr<Event>, compareEvents> bottom_;
#endif
};

} // namespace warped

#endif /* LADDER_QUEUE_HPP */

