//-*- C++ -*-
/** Implementation of GVTSynchronizedCheckpointManager
 * @author O'HARA Mamoru
 * @date 2016 Mar 8
 */
#include "GVTSynchronizedCheckpointManager.hpp"

#include <atomic>
#include <cassert>

#include "Configuration.hpp"
#include "json/json.h"  /* for Json::Value */

//////////////////////////////////////////////////////////////////////
// GVTSynchronizationCheckpointManager::Impl
struct warped::GVTSynchronizedCheckpointManager::Impl
{
  unsigned int interval, remaining;
  int time_to_live;
  std::atomic<bool> checkpointRequired;
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
  pimpl_->time_to_live = configuration().root()["checkpointing"]["count-till-termination"].asInt();
  pimpl_->checkpointRequired.store(false);
  pthread_barrier_init(&pimpl_->gvt_coordinated_barrier, NULL, num_worker_threads+1);
}

void
warped::GVTSynchronizedCheckpointManager::onGVT(unsigned int /*gvt*/)
{
  if (--pimpl_->remaining == 0)
    pimpl_->checkpointRequired.store(true);
}

bool
warped::GVTSynchronizedCheckpointManager::checkpointRequired() const
{
  return pimpl_->checkpointRequired.load();
}

void
warped::GVTSynchronizedCheckpointManager::doGenerateCheckpoint(cereal::PortableBinaryOutputArchive&)
{
  pimpl_->remaining = pimpl_->interval;
  pimpl_->checkpointRequired.store(false);
  if (pimpl_->time_to_live > 0)  --pimpl_->time_to_live;

  pthread_barrier_wait(&pimpl_->gvt_coordinated_barrier);
}

void
warped::GVTSynchronizedCheckpointManager::doBlock()
{
  pthread_barrier_wait(&pimpl_->gvt_coordinated_barrier);
}

bool
warped::GVTSynchronizedCheckpointManager::terminateAfterCheckpoint() const
{
  return pimpl_->time_to_live == 0;
}
