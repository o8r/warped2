//-*- C++ -*-
/** Implementation of GVTSynchronizedCheckpointManager
 * @author O'HARA Mamoru
 * @date 2016 Mar 8
 */
#include "GVTSynchronizedCheckpointManager.hpp"

#include <cassert>

#include "Configuration.hpp"

//////////////////////////////////////////////////////////////////////
// GVTSynchronizationCheckpointManager::Impl
struct warped::GVTSynchronizedCheckpointManager::Impl
{
  unsigned int interval, remaining;
  pthread_barrier_t gvt_coordinated_barrier;

  ~Impl() {
    pthread_barrier_destroy(&gvt_coordinated_barrier);
  }
};

//////////////////////////////////////////////////////////////////////
// ctors
warped::GVTSynchronizedCheckpointManager::GVTSynchronizedCheckpointManager(Configuration const& config) :
  TimeWarpCheckpointManager{ config },
  pimpl_{ new Impl }
{}

//////////////////////////////////////////////////////////////////////
// Overrides
void
warped::GVTSynchronizedCheckpointManager::doInitialize
(unsigned int num_worker_threads,
 std::vector<LogicalProcess*> const& /*lps*/,
 TimeWarpCommunicationManager const& /*comm_manager*/,
 TimeWarpEventSet const& /*event_set*/,
 TimeWarpGVTManager const& /*gvt_manager*/,
 TimeWarpStateManager const& /*state_manager*/,
 TimeWarpOutputManager const& /*output_manager*/,
 TimeWarpFileStreamManager const& /*twfs_manager*/,
 TimeWarpTerminationManager& /*termination_manager*/,
 TimeWarpStatistics& /*tw_stats*/)
{
  assert(pimpl_);

  pimpl_->interval = configuration().root()["checkpointing"]["interval"].asUInt();
  pimpl_->remaining = pimpl_->interval;

  pthread_barrier_init(&pimpl_->gvt_coordinated_barrier, NULL, num_worker_threads+1);
}

bool
warped::GVTSynchronizedCheckpointManager::onGVT(unsigned int /*gvt*/)
{
  // All threads in all processes will block here after coordination of GVT
  pthread_barrier_wait(&pimpl_->gvt_coordinated_barrier);

  return !(--pimpl_->remaining > 0);
}

void
warped::GVTSynchronizedCheckpointManager::generateCheckpoint()
{
  pimpl_->remaining = pimpl_->interval;
}

