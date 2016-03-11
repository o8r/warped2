// -*- C++ -*-
/** This CheckpointManager takes checkpoints synchronously with GVT coordination.
 * @author O'HARA Mamoru
 * @date 2016 Mar 8
 */
#ifndef WARPED_GVTSYNCHRONIZEDCHECKPOINTMANAGER_HPP_
#define WARPED_GVTSYNCHRONIZEDCHECKPOINTMANAGER_HPP_

#include <memory>
#include <atomic>

#include <pthread.h>

#include "TimeWarpCheckpointManager.hpp"

namespace warped {

  class GVTSynchronizedCheckpointManager
    : public TimeWarpCheckpointManager
  {
  public:
    // ctors
    GVTSynchronizedCheckpointManager(Configuration const& config);

  protected:
    void doInitialize(unsigned int num_worker_threads,
		      std::vector<LogicalProcess*> const& lps,
		      TimeWarpCommunicationManager const& comm_manager,
		      TimeWarpEventSet const& event_set,
		      TimeWarpGVTManager const& gvt_manager,
		      TimeWarpStateManager const& state_manager,
		      TimeWarpOutputManager const& output_manager,
		      TimeWarpFileStreamManager const& twfs_manager,
		      TimeWarpTerminationManager& termination_manager,
		      TimeWarpStatistics& tw_stats
		      ) override;
    bool onGVT(unsigned int gvt) override;
    void generateCheckpoint() override;

  private:
    struct Impl;
    std::shared_ptr<Impl> pimpl_;
  };

}  // namespace warped
#endif
