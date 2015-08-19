#include "LadderQueue.hpp"
#include <cassert>

namespace warped {

LadderQueue::LadderQueue() {

    /* Initialize the variables */
    std::fill_n(bucket_width_, MAX_RUNG_CNT, 0);
    std::fill_n(last_nonempty_bucket_, MAX_RUNG_CNT, 0);
    std::fill_n(r_start_, MAX_RUNG_CNT, 0);
    std::fill_n(r_current_, MAX_RUNG_CNT, 0);

    /* Create buckets for 2nd rung onwards */
    for (unsigned int rung_index = 1; rung_index < MAX_RUNG_CNT; rung_index++) {
        for (unsigned int bucket_index = 0; bucket_index < MAX_BUCKET_CNT; bucket_index++) {
            rung_[rung_index].push_back(std::make_shared<std::list<std::shared_ptr<Event>>>());
        }
    }
}

std::vector<std::shared_ptr<Event>> LadderQueue::begin(unsigned int count) {

    std::cout << count << std::endl;
    std::vector<std::shared_ptr<Event>> event_list;

    /* Remove from bottom if not empty */
    if (!bottom_.empty()) {
        auto iter = bottom_.begin();
        while (iter != bottom_.end() && count) {
            event_list.push_back(*iter);
            count--;
            iter++;
        }
        return event_list;
    }

    /* If rungs exist, remove from rungs */
    unsigned int bucket_index = 0;
    if (!recurseRung(&bucket_index)) {
        /* Check whether rungs still exist */
        if (n_rung_) assert(0);
    }

    /* Check required because rung recursion can affect n_rung_ value */
    if (n_rung_) {

#ifdef PARTIALLY_UNSORTED_EVENT_SET
        for (auto event : *rung_[n_rung_-1][bucket_index]) {
            bottom_.push_front(event);
        }
        bottom_start_ = r_current_[n_rung_-1];
#else
        for (auto event : *rung_[n_rung_-1][bucket_index]) {
            bottom_.insert(event);
        }
#endif

        rung_[n_rung_-1][bucket_index]->clear();

        /* If bucket returned is the last valid bucket of the rung */
        if (last_nonempty_bucket_[n_rung_-1] == bucket_index+1) {
            do {
                last_nonempty_bucket_[n_rung_-1] = 0;
                r_start_[n_rung_-1]         = 0;
                r_current_[n_rung_-1]       = 0;
                bucket_width_[n_rung_-1]    = 0;
                n_rung_--;
            } while(n_rung_ && !last_nonempty_bucket_[n_rung_-1]);

        } else {
            while((++bucket_index < last_nonempty_bucket_[n_rung_-1]) && 
                                    rung_[n_rung_-1][bucket_index]->empty());
            if (bucket_index < last_nonempty_bucket_[n_rung_-1]) {
                r_current_[n_rung_-1] = 
                    r_start_[n_rung_-1] + bucket_index*bucket_width_[n_rung_-1];
            } else {
                assert(0);
            }
        }

        if (!bottom_.empty()) {
            auto iter = bottom_.begin();
            while (iter != bottom_.end() && count) {
                event_list.push_back(*iter);
                count--;
                iter++;
            }
        }
        return event_list;
    }

    /* Check if top has any events before proceeding further*/
    if (top_.empty()) return event_list;

    /* Move from top to top of empty ladder */
    /* Check if failed to create the first rung */
    bool is_bucket_width_static = false;
    if (!createNewRung(top_.size(), min_ts_, &is_bucket_width_static)) assert(0);

    /* Transfer events from Top to 1st rung of Ladder */
    /* Note: No need to update rCur[0] since it will be equal to rStart[0] initially. */
    for (auto iter = top_.begin(); iter != top_.end();) {
        assert((*iter)->timestamp() >= r_start_[0]);
        bucket_index = std::min(
                (unsigned int)((*iter)->timestamp()-r_start_[0]) / bucket_width_[0], 
                                                                        RUNG_BUCKET_CNT(0)-1);
        rung_[0][bucket_index]->push_front(*iter);
        iter = top_.erase(iter);

        /* Update the numBucket parameter */
        if (last_nonempty_bucket_[0] < bucket_index+1) {
            last_nonempty_bucket_[0] = bucket_index+1;
        }
    }

    /* Copy events from bucket_k into Bottom */
    if (!recurseRung(&bucket_index)) {
        assert(0);
    }

#ifdef PARTIALLY_UNSORTED_EVENT_SET
    for (auto event : *rung_[n_rung_-1][bucket_index]) {
        bottom_.push_front(event);
    }
    bottom_start_ = r_current_[n_rung_-1];
#else
    for (auto event : *rung_[n_rung_-1][bucket_index]) {
        bottom_.insert(event);
    }
#endif

    rung_[n_rung_-1][bucket_index]->clear();

    /* If bucket returned is the last valid rung of the bucket */
    if (last_nonempty_bucket_[n_rung_-1] == bucket_index+1) {
        last_nonempty_bucket_[n_rung_-1] = 0;
        r_start_[n_rung_-1]         = 0;
        r_current_[n_rung_-1]       = 0;
        bucket_width_[n_rung_-1]    = 0;
        n_rung_--;

    } else {
        while((++bucket_index < last_nonempty_bucket_[n_rung_-1]) && 
                (rung_[n_rung_-1][bucket_index]->empty()));
        if (bucket_index < last_nonempty_bucket_[n_rung_-1]) {
            r_current_[n_rung_-1] = 
                r_start_[n_rung_-1] + bucket_index*bucket_width_[n_rung_-1];
        } else {
            last_nonempty_bucket_[n_rung_-1] = 0;
            r_start_[n_rung_-1]         = 0;
            r_current_[n_rung_-1]       = 0;
            bucket_width_[n_rung_-1]    = 0;
            n_rung_--;
        }
    }

    if (!bottom_.empty()) {
        auto iter = bottom_.begin();
        while (iter != bottom_.end() && count) {
            event_list.push_back(*iter);
            count--;
            iter++;
        }
    }
    return event_list;
}

bool LadderQueue::erase(std::shared_ptr<Event> event) {

    bool status = false;
    assert(event != nullptr);
    auto timestamp = event->timestamp();

    /* Check and erase from top, if found */
    if (timestamp > top_start_) {
        if (top_.empty()) assert(0);
        for (auto iter = top_.begin(); iter != top_.end(); iter++) {
            if (iter == top_.end()) assert(0);
            if (*iter == event) {
                (void) top_.erase(iter);
                status = true;
                break;
            }
        }
        return status;
    }

    /* Step through rungs */
    unsigned int rung_index = 0;
    for (rung_index = 0; rung_index < n_rung_; rung_index++) {
        if (timestamp >= r_current_[rung_index]) break;
    }

    if (rung_index < n_rung_) {  /* found a rung */
        unsigned int bucket_index = std::min(
            (unsigned int)(timestamp - r_start_[rung_index]) / bucket_width_[rung_index],
                                                            RUNG_BUCKET_CNT(rung_index)-1);
        auto rung_bucket = rung_[rung_index][bucket_index];
        if (!rung_bucket->empty()) {
            for (auto iter = rung_bucket->begin(); ; iter++) {
                if (iter == rung_bucket->end()) assert(0);
                if ((*iter) == event) {
                    (void) rung_bucket->erase(iter);
                    status = true;
                    break;
                }
            }

            /* If bucket is empty after deletion */
            if (rung_bucket->empty()) {
                /* Check whether rung bucket count needs adjustment */
                if (last_nonempty_bucket_[rung_index] == bucket_index+1) {
                    bool is_rung_empty = false;
                    do {
                        if (!bucket_index) {
                            is_rung_empty = true;
                            last_nonempty_bucket_[rung_index] = 0;
                            r_current_[rung_index] = r_start_[rung_index];
                            break;
                        }
                        bucket_index--;
                        rung_bucket = rung_[rung_index][bucket_index];
                    } while (rung_bucket->empty());

                    if (!is_rung_empty) {
                        last_nonempty_bucket_[rung_index] = bucket_index+1;
                    }
                }
            }
        } else {
            assert(0);
        }
        return status;
    }

    /* Check and erase from bottom, if present */
    if (bottom_.empty()) assert(0);

#ifdef PARTIALLY_UNSORTED_EVENT_SET
    (void) bottom_.remove(event);
#else
    (void) bottom_.erase(event);
#endif

    return true;
}

void LadderQueue::insert(std::shared_ptr<Event> event) {

    assert(event != nullptr);
    auto timestamp = event->timestamp();

    /* Insert into top, if valid */
    if (timestamp > top_start_) {  //deviation from APPENDIX of ladderq
        if(top_.empty()) {
            max_ts_ = min_ts_ = timestamp;
        } else {
            if (min_ts_ > timestamp) min_ts_ = timestamp;
            if (max_ts_ < timestamp) max_ts_ = timestamp;
        }
        top_.push_front(event);
        return;
    }

    /* Step through rungs */
    unsigned int rung_index = 0;
    while ((rung_index < n_rung_) && (timestamp < r_current_[rung_index])) {
        rung_index++;
    }

    if (rung_index < n_rung_) {  /* found a rung */
        assert(timestamp >= r_start_[rung_index]);
        unsigned int bucket_index = std::min(
            (unsigned int)(timestamp - r_start_[rung_index]) / bucket_width_[rung_index],
                                                            RUNG_BUCKET_CNT(rung_index)-1);

        /* Adjust rung parameters */
        if (last_nonempty_bucket_[rung_index] < bucket_index+1) {
            last_nonempty_bucket_[rung_index] = bucket_index+1;
        }
        unsigned int temp_ts = r_start_[rung_index] + bucket_index*bucket_width_[rung_index];
        if (r_current_[rung_index] > temp_ts) {
            r_current_[rung_index] = temp_ts;
        }

        rung_[rung_index][bucket_index]->push_front(event);
        return;
    }

    /* If bottom exceeds threshold */
    if (bottom_.size() >= THRESHOLD) {
        /* If no additional rungs can be created */
        if (n_rung_ >= MAX_RUNG_CNT) {
            /* Intentionally let the bottom continue to overflow */
            //ref sec 2.4 of ladderq + when bucket width becomes static

#ifdef PARTIALLY_UNSORTED_EVENT_SET
            bottom_.push_front(event);
#else
            bottom_.insert(event);
#endif

            return;
        }

        /* Check if new event to be inserted is smaller than what is present in BOTTOM */
#ifdef PARTIALLY_UNSORTED_EVENT_SET
        if (bottom_start_ > timestamp) {
            bottom_start_ = timestamp;
        }
        createRungForBottomTransfer(bottom_start_);
#else
        auto bucket_start_ts = (*bottom_.begin())->timestamp();
        if (bucket_start_ts > timestamp) {
            bucket_start_ts = timestamp;
        }
        createRungForBottomTransfer(bucket_start_ts);
#endif

        /* Transfer bottom to new rung */
        for (auto iter = bottom_.begin(); iter != bottom_.end(); iter++) {
            assert( (*iter)->timestamp() >= r_start_[n_rung_-1] );
            unsigned int bucket_index = std::min( 
                (unsigned int)(((*iter)->timestamp()-r_start_[n_rung_-1]) / 
                                                    bucket_width_[n_rung_-1]),
                                                            RUNG_BUCKET_CNT(n_rung_-1)-1 );

            /* Adjust rung parameters */
            if (last_nonempty_bucket_[n_rung_-1] < bucket_index+1) {
                last_nonempty_bucket_[n_rung_-1] = bucket_index+1;
            }
            rung_[n_rung_-1][bucket_index]->push_front(*iter);
        }
        bottom_.clear();

        /* Insert new element in the new and populated rung */
        assert(timestamp >= r_start_[n_rung_-1]);
        unsigned int bucket_index = std::min( 
                (unsigned int)((timestamp-r_start_[n_rung_-1]) / bucket_width_[n_rung_-1]),
                                                            RUNG_BUCKET_CNT(n_rung_-1)-1 );
        if (last_nonempty_bucket_[n_rung_-1] < bucket_index+1) {
            last_nonempty_bucket_[n_rung_-1] = bucket_index+1;
        }
        unsigned int temp_ts = r_start_[n_rung_-1] + bucket_index*bucket_width_[n_rung_-1];
        if (r_current_[n_rung_-1] > temp_ts) {
            r_current_[n_rung_-1] = temp_ts;
        }
        rung_[n_rung_-1][bucket_index]->push_front(event);

    } else { /* If BOTTOM is within threshold */
#ifdef PARTIALLY_UNSORTED_EVENT_SET
        bottom_.push_front(event);
#else
        bottom_.insert(event);
#endif
    }
}

#ifdef PARTIALLY_UNSORTED_EVENT_SET
unsigned int LadderQueue::lowestTimestamp() {

    return bottom_start_;
}
#endif

bool LadderQueue::createNewRung(unsigned int num_events, 
                                unsigned int init_start_and_cur_val, 
                                bool *is_bucket_width_static) {

    assert(is_bucket_width_static);
    assert(num_events);

    *is_bucket_width_static = false;

    /* Check if this is the first rung creation */
    if (!n_rung_) {
        assert(max_ts_ >= min_ts_);
        bucket_width_[0] = (min_ts_ == max_ts_) ? MIN_BUCKET_WIDTH : 
                                    (max_ts_ - min_ts_ + num_events -1) / num_events;
        top_start_          = max_ts_;
        r_start_[0]         = min_ts_;
        r_current_[0]       = min_ts_;
        last_nonempty_bucket_[0] = 0;
        n_rung_++;

        /* Create the actual rungs */
        //create double of required no of buckets. ref sec 2.4 of ladderq
        unsigned int bucket_index = 0;
        for (bucket_index = rung_0_length_; bucket_index < 2*num_events; bucket_index++) {
            rung_[0].push_back(std::make_shared<std::list<std::shared_ptr<Event>>>());
        }
        rung_0_length_ = bucket_index;

    } else { // When rungs already exist
        /* Check if bucket width has reached the min limit */
        if (bucket_width_[n_rung_-1] <= MIN_BUCKET_WIDTH) {
            *is_bucket_width_static = true;
            return false;
        }
        /* Check whether new rungs can be created */
        assert(n_rung_ < MAX_RUNG_CNT);
        n_rung_++;
        bucket_width_[n_rung_-1] = (bucket_width_[n_rung_-2] + num_events - 1) / num_events;
        r_start_[n_rung_-1] = r_current_[n_rung_-1] = init_start_and_cur_val;
        last_nonempty_bucket_[n_rung_-1] = 0;
    }
    return true;
}

void LadderQueue::createRungForBottomTransfer(unsigned int start_val) {

    n_rung_++;
    r_start_[n_rung_-1] = r_current_[n_rung_-1] = start_val;
    last_nonempty_bucket_[n_rung_-1] = 0;
    if(n_rung_ == 1) {
        assert(top_start_ >= start_val);
        bucket_width_[0] = std::max(
                (unsigned int) ((top_start_ - start_val)/bottom_.size()), 
                (unsigned int) MIN_BUCKET_WIDTH);

        //create double of required no of buckets. ref sec 2.4 of ladderq
        unsigned int bucket_index = 0;
        for (bucket_index = rung_0_length_; bucket_index < 2*bottom_.size(); bucket_index++) {
            rung_[0].push_back(std::make_shared<std::list<std::shared_ptr<Event>>>());
        }
        rung_0_length_ = bucket_index;

    } else { /* When not the top rung */
        assert(r_current_[n_rung_-2] >= start_val);
        bucket_width_[n_rung_-1] = std::max(
                (unsigned int) ((r_current_[n_rung_-2] - start_val)/THRESHOLD), 
                (unsigned int) MIN_BUCKET_WIDTH);
    }
}

bool LadderQueue::recurseRung(unsigned int *index) {

    bool status = false;
    *index = 0;

    /* Find_bucket label */
    while(1) {
        if (!n_rung_) break;
        if (n_rung_ > MAX_RUNG_CNT) assert(0);

        unsigned int bucket_index = 0;
        r_current_[n_rung_-1] = r_start_[n_rung_-1];
        while ((bucket_index < RUNG_BUCKET_CNT(n_rung_-1)) && 
                            rung_[n_rung_-1][bucket_index]->empty()) {
            bucket_index++;
        }

        // If the last rung is empty
        if (bucket_index == RUNG_BUCKET_CNT(n_rung_-1)) {
            r_start_[n_rung_-1]         = 0;
            r_current_[n_rung_-1]       = 0;
            bucket_width_[n_rung_-1]    = 0;
            last_nonempty_bucket_[n_rung_-1] = 0;
            n_rung_--;

        } else {
            r_current_[n_rung_-1] += bucket_index*bucket_width_[n_rung_-1];

            // If the bucket does not have any bucket overflow
            if (rung_[n_rung_-1][bucket_index]->size() <= THRESHOLD) {
                *index = bucket_index;
                status = true;
                break;
            }

            /* When there is a bucket overflow */
            bool is_bucket_width_static = false;

            // Try to create a new rung
            if (!createNewRung( rung_[n_rung_-1][bucket_index]->size(),
                                r_current_[n_rung_-1], 
                                &is_bucket_width_static )) {
                if (is_bucket_width_static) {
                    *index = bucket_index;
                    status = true;
                }
                break;
            }

            // Move events from the bucket in penultimate rung to the new rung
            for (auto iter = rung_[n_rung_-2][bucket_index]->begin(); 
                        iter != rung_[n_rung_-2][bucket_index]->end(); iter++) {
                assert((*iter)->timestamp() >= r_start_[n_rung_-1] );
                unsigned int new_bucket_index = std::min( 
                    (unsigned int) ((*iter)->timestamp() - r_start_[n_rung_-1]) / 
                                    bucket_width_[n_rung_-1], RUNG_BUCKET_CNT(n_rung_-1)-1);
                rung_[n_rung_-1][new_bucket_index]->push_front(*iter);

                /* Calculate bucket count for new rung */
                if (last_nonempty_bucket_[n_rung_-1] < new_bucket_index+1) {
                    last_nonempty_bucket_[n_rung_-1] = new_bucket_index+1;
                }
            }
            rung_[n_rung_-2][bucket_index]->clear();

            /* Re-calculate r_current and last_nonempty_bucket_ of old rung */
            bool bucket_found = false;
            for (unsigned int index = bucket_index+1; 
                            index < RUNG_BUCKET_CNT(n_rung_-2); index++) {
                if (!rung_[n_rung_-2][index]->empty()) {
                    if (!bucket_found) {
                        bucket_found = true;
                        r_current_[n_rung_-2] = 
                            r_start_[n_rung_-2] + index*bucket_width_[n_rung_-2];
                    }
                    last_nonempty_bucket_[n_rung_-2] = index+1;
                }
            }
            if (!bucket_found) {
                last_nonempty_bucket_[n_rung_-2] = 0;
                r_current_[n_rung_-2] = r_start_[n_rung_-2];
            }
        }
    }
    return status;
}

} // namespace warped
