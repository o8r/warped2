#ifndef STATE_MANAGER_HPP
#define STATE_MANAGER_HPP

/* This class provides an interface for a specific State Manager 
 * that implements a specific algorithm for saving and restoring 
 * the state.
 */

#include <memory>
#include <deque>
#include <list>

#include "TimeWarpEventDispatcher.hpp"
#include "LogicalProcess.hpp"
#include "LPState.hpp"
#include "serialization.hpp"

namespace warped {

class TimeWarpStateManager {
public:

    TimeWarpStateManager() = default;

    // Creates a state queue for each lps as well as a lock for each state queue
    virtual void initialize(unsigned int num_local_lps);

    // Removes states from state queues before or equal to the gvt for the specified lp
    unsigned int fossilCollect(unsigned int gvt, unsigned int local_lp_id);

    // Restores a state based on rollback time for the given lp.
    std::shared_ptr<Event> restoreState(std::shared_ptr<Event> rollback_event, unsigned int local_lp_id,
        LogicalProcess *lp);

    // Number of states in the state queue for the specified lp
    std::size_t size(unsigned int local_lp_id);

    virtual void saveState(std::shared_ptr<Event> current_event, unsigned int local_lp_id,
        LogicalProcess *lp) = 0;

protected:

    struct SavedState {
        /** default ctor, required for serialization.
	 * @author O'HARA Mamoru
	 * @date 2016 Mar 16
	 */
        SavedState() : state_event_{nullptr}, lp_state_{nullptr}, rng_state_{}
        {}
        SavedState(std::shared_ptr<Event> state_event, std::unique_ptr<LPState> lp_state,
            std::list<std::shared_ptr<std::stringstream> > rng_state)
                : state_event_(state_event), lp_state_(std::move(lp_state)),
                rng_state_(rng_state) {}

        std::shared_ptr<Event> state_event_;
        std::unique_ptr<LPState> lp_state_;
        std::list<std::shared_ptr<std::stringstream> > rng_state_;

        template <typename Archive>
	void save(Archive& ar) const {
	  ar(state_event_, lp_state_);
	  ar(rng_state_.size());
	  for (auto const& p : rng_state_)
	    ar(p->str());
	}
        template <typename Archive>
	void load(Archive& ar) {
	  ar(state_event_, lp_state_);

	  std::size_t size;
	  ar(size);

	  for (std::size_t i=0; i<size; ++i) {
	    std::string str;
	    ar(str);
	    rng_state_.push_back(std::make_shared<std::stringstream>(str));	    
	  }
	}
#if 0
        template <typename Archive>
	static void load_and_construct(Archive& ar, cereal::construct<SavedState>& construct) {
	  std::shared_ptr<Event> state_event;
	  ar(state_event);

	  std::unique_ptr<LPState> lp_state;
	  ar(lp_state);

	  std::list<std::shared_ptr<std::stringstream>> rng_state;
	  std::size_t n;
	  ar(n);
	  std::string str;
	  for (unsigned i=0; i<n; ++i) {
	    ar(str);
	    rng_state.push_back(std::make_shared<std::stringstream>(str));
	  }

	  construct(state_event, lp_state, rng_state);
	}
#endif
    };

    // Array of vectors (Array of states queues), one for each lp
    std::unique_ptr<std::deque<SavedState> []> state_queue_;

    unsigned int num_local_lps_;

    friend class cereal::access;
    template <typename Archive>
    void save(Archive& ar) const {
      ar(num_local_lps_);
      for (unsigned int i=0; i<num_local_lps_; ++i)
	ar(state_queue_[i]);
    }
    template <typename Archive>
    void load(Archive& ar) {
      unsigned int num_local_lps;
      ar(num_local_lps);

      initialize(num_local_lps);

      for (unsigned int i=0; i<num_local_lps; ++i)
	ar(state_queue_[i]);
    }
};

} // warped namespace

#endif
