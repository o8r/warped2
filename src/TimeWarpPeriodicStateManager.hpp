#ifndef PERIODIC_STATE_MANAGER_HPP
#define PERIODIC_STATE_MANAGER_HPP

#include <cstring>

#include "TimeWarpStateManager.hpp"
#include "serialization.hpp"

/* Subclass of TimeWarpStateManager which implements a periodic state saving. In this
 * implementation, the period is fixed and is a number of events. The save state method
 * should be called for each event processed so that the period count can be decremented. When
 * period count reaches 0, the state is saved.
 */

namespace warped {

class TimeWarpPeriodicStateManager : public TimeWarpStateManager {
public:

    TimeWarpPeriodicStateManager(unsigned int period) :
        period_(period) {}

    void initialize(unsigned int num_local_lps) override;

    // Saves the state of the specified lp if the count is equal to 0.
    virtual void saveState(std::shared_ptr<Event> current_event, unsigned int local_lp_id,
        LogicalProcess *lp);

private:
    // Period is the number of events that must be processed before saving state
    unsigned int period_;

    /** for loading an object of this class from archive.
     * @author O'HARA Mamoru
     * @date 2016 Mar 16
     */
    unsigned int num_local_lps_;

    // Count keeps a running count of the number of event that we must wait before saving
    // the state again (per lp)
    std::unique_ptr<unsigned int []> count_;

    friend class cereal::access;
    template <typename Archive>
    void save(Archive& ar) const {
      ar(period_);
      ar(cereal::base_class<TimeWarpStateManager>(this));

      ar(num_local_lps_);
      for (unsigned int i=0; i<num_local_lps_; ++i)
	ar(count_[i]);
    }
    template <typename Archive>
    void load(Archive& ar) {
      ar(period_);
      ar(cereal::base_class<TimeWarpStateManager>(this));

      ar(num_local_lps_);
      initialize(num_local_lps_);

      for (unsigned int i=0; i<num_local_lps_; ++i)
	ar(count_[i]);
    }
    template <typename Archive>
    static void load_and_construct(Archive& ar, cereal::construct<TimeWarpPeriodicStateManager>& construct) {
      unsigned int period;
      ar(period);

      construct(period);

      ar(cereal::base_class<TimeWarpStateManager>(construct.ptr()));

      unsigned int num_local_lps;
      ar(num_local_lps);
      construct->initialize(num_local_lps);

      for (unsigned int i=0; i<num_local_lps; ++i)
	ar(construct->count_[i]);
    }
};

} // namespace warped

#endif
