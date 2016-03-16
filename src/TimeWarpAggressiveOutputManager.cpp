#include "TimeWarpAggressiveOutputManager.hpp"

WARPED_REGISTER_POLYMORPHIC_SERIALIZABLE_CLASS(warped::TimeWarpAggressiveOutputManager)

namespace warped {

// For aggressive cancellation, just simply remove events after straggler event
//  and return them.
std::unique_ptr<std::vector<std::shared_ptr<Event>>>
TimeWarpAggressiveOutputManager::rollback(std::shared_ptr<Event> straggler_event, unsigned int local_lp_id) {
    return std::move(removeEventsSentAfter(straggler_event, local_lp_id));
}

} // namespace warped

