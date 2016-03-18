#include "TimeWarpCheckpointManager.hpp"

#include <chrono>
#include <cassert>

#include "Configuration.hpp"
// for serialization
#include "TimeWarpCommunicationManager.hpp" 
#include "TimeWarpEventSet.hpp"
#include "TimeWarpGVTManager.hpp"
#include "TimeWarpStateManager.hpp"
#include "TimeWarpOutputManager.hpp"
#include "TimeWarpFileStreamManager.hpp"
#include "TimeWarpTerminationManager.hpp"
#include "TimeWarpStatistics.hpp"
#include "serialization.hpp"

//////////////////////////////////////////////////////////////////////
// TimeWarpCheckpointManager::Impl
struct warped::TimeWarpCheckpointManager::Impl
{
  Configuration configuration;

  unsigned int num_worker_threads;
  std::vector<LogicalProcess*> lps;

  TimeWarpCommunicationManager const* comm_manager;
  TimeWarpEventSet const* event_set;
  TimeWarpGVTManager const* gvt_manager;
  TimeWarpStateManager const* state_manager;
  TimeWarpOutputManager const* output_manager;
  TimeWarpFileStreamManager const* twfs_manager;
  TimeWarpTerminationManager * termination_manager;
  TimeWarpStatistics* tw_stats;

  Impl(Configuration const& config) : 
    configuration { config },
    comm_manager { nullptr }, event_set { nullptr }, gvt_manager { nullptr }, 
    state_manager { nullptr }, output_manager { nullptr }, twfs_manager { nullptr },
    termination_manager { nullptr }, tw_stats { nullptr }
  {}
};


//////////////////////////////////////////////////////////////////////
// ctors
warped::TimeWarpCheckpointManager::TimeWarpCheckpointManager(Configuration const& config)
  : pimpl_{ new Impl(config) }
{}

//////////////////////////////////////////////////////////////////////
// dtor
warped::TimeWarpCheckpointManager::~TimeWarpCheckpointManager()
{}


//////////////////////////////////////////////////////////////////////
/** load a checkpoint.
 */
void
warped::TimeWarpCheckpointManager::load()
{
}


warped::Configuration const&
warped::TimeWarpCheckpointManager::configuration() const
{
  return pimpl_->configuration;
}

//////////////////////////////////////////////////////////////////////
/** initialize.
 * @param lps LogicalProcesses
 */
void
warped::TimeWarpCheckpointManager::initialize
(unsigned int num_worker_threads,
 std::vector<LogicalProcess*> const& lps,
 TimeWarpCommunicationManager const& comm_manager,
 TimeWarpEventSet const& event_set,
 TimeWarpGVTManager const& gvt_manager,
 TimeWarpStateManager const& state_manager,
 TimeWarpOutputManager const& output_manager,
 TimeWarpFileStreamManager const& twfs_manager,
 TimeWarpTerminationManager& termination_manager,
 TimeWarpStatistics& tw_stats)
{
  assert(pimpl_);

  pimpl_->lps.insert(end(pimpl_->lps), begin(lps), end(lps));

  pimpl_->num_worker_threads = num_worker_threads;
  pimpl_->lps.clear(); pimpl_->lps.insert(begin(pimpl_->lps), begin(lps), end(lps));
  pimpl_->comm_manager = &comm_manager;
  pimpl_->event_set = &event_set;
  pimpl_->gvt_manager = &gvt_manager;
  pimpl_->state_manager = &state_manager;
  pimpl_->output_manager = &output_manager;
  pimpl_->twfs_manager = &twfs_manager;
  pimpl_->termination_manager = &termination_manager;
  pimpl_->tw_stats = &tw_stats;

  doInitialize(num_worker_threads, lps, comm_manager, event_set, gvt_manager, state_manager, output_manager,
	       twfs_manager, termination_manager, tw_stats);
}


//////////////////////////////////////////////////////////////////////
/** make a checkpoint if necessary.
 */
void
warped::TimeWarpCheckpointManager::checkpointIfNecessary()
{
  if (checkpointRequired())
    generateCheckpoint();
}

void
warped::TimeWarpCheckpointManager::blockIfNecessary()
{
  if (checkpointRequired())
    doBlock();
}

//////////////////////////////////////////////////////////////////////
/** generateCheckpoint
 */
void
warped::TimeWarpCheckpointManager::generateCheckpoint()
{
  assert(pimpl_);

  auto now = std::chrono::steady_clock::now();

  auto filepath = pimpl_->configuration.root()["checkpointing"]["file"].asString();
  std::ofstream ofs { filepath, std::ios_base::out | std::ios_base::trunc };
  cereal::PortableBinaryOutputArchive ar { ofs };

  ar(now);

  ar(pimpl_->configuration, *pimpl_->comm_manager, *pimpl_->event_set, *pimpl_->gvt_manager,
     *pimpl_->state_manager, *pimpl_->output_manager, *pimpl_->twfs_manager, *pimpl_->termination_manager,
     *pimpl_->tw_stats);

  // archive Logical processes
  for (LogicalProcess* lp: pimpl_->lps)
    ar(*lp);

  doGenerateCheckpoint(ar);

  auto stop = std::chrono::steady_clock::now();
  double num_seconds = double((stop - now).count()) * 
    std::chrono::steady_clock::period::num / std::chrono::steady_clock::period::den;
  auto c = pimpl_->tw_stats->upGlobalCount(TOTAL_CHECKPOINTS);
  pimpl_->tw_stats->updateAverage(CHECKPOINT_SAVE_TIME, num_seconds, c);

  if (terminateAfterCheckpoint()) {
    // Force termination of all worker threads
#if 0
    for (unsigned int i=0; i<pimpl_->num_worker_threads; ++i)
      pimpl_->termination_manager->setThreadPassive(i);
#else
    pimpl_->termination_manager->pause();
#endif
  }
}
