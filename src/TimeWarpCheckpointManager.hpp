// -*- C++ -*-
/** Base class of Checkpoint managers
 * @author O'HARA Mamoru
 * @date 2016 Mar 8
 */
#ifndef WARPED_TIMEWARPCHECKPOINTMANAGER_HPP_
#define WARPED_TIMEWARPCHECKPOINTMANAGER_HPP_

#include <vector>
#include <fstream>
#include <string>
#include <memory>

#include "serialization.hpp"

namespace warped {

  class Event;
  class LogicalProcess;
  class Configuration;
  class TimeWarpCommunicationManager;
  class TimeWarpEventSet;
  class TimeWarpGVTManager;
  class TimeWarpStateManager;
  class TimeWarpOutputManager;
  class TimeWarpFileStreamManager;
  class TimeWarpTerminationManager;
  class TimeWarpStatistics;

  class TimeWarpCheckpointManager
  {
  public:
    // ctors
    explicit TimeWarpCheckpointManager(Configuration const& config);
    // dtor
    virtual ~TimeWarpCheckpointManager();
    // assignments
    TimeWarpCheckpointManager& operator=(TimeWarpCheckpointManager const&) =delete;

    void initialize(unsigned int num_worker_threads,
		    std::vector<LogicalProcess*> const& lps,
		    TimeWarpCommunicationManager const& comm_manager,
		    TimeWarpEventSet const& event_set,
		    TimeWarpGVTManager const& gvt_manager,
		    TimeWarpStateManager const& state_manager,
		    TimeWarpOutputManager const& output_manager,
		    TimeWarpFileStreamManager const& twfs_manager,
		    TimeWarpTerminationManager & termination_manager,
		    TimeWarpStatistics& tw_stats
		    );

    /** load a checkpoint data.
     * @remark call load() before calling initialize().
     */
    void load();

    // Event handlers
    virtual void onEvent(std::shared_ptr<Event> /*event*/) { }
    virtual void onPassive() { }
    virtual void onGVT(unsigned int /*gvt*/) { }

    //* Called from TimeWarpEventDispatcher::startSimulation() (main thread) and generate a checkpoint if necessary.
    void checkpointIfNecessary();
    //* Called from TimeWarpEventDispatcher::processEvent() (worker threads) and block during checkpointing if necessary.
    void blockIfNecessary();

    void generateCheckpoint();
  protected:
    Configuration const& configuration() const;

    virtual void doInitialize(unsigned int /*num_worker_threads*/,
			      std::vector<LogicalProcess*> const& /*lps*/,
			      TimeWarpCommunicationManager const& /*comm_manager*/,
			      TimeWarpEventSet const& /*event_set*/,
			      TimeWarpGVTManager const& /*gvt_manager*/,
			      TimeWarpStateManager const& /*state_manager*/,
			      TimeWarpOutputManager const& /*output_manager*/,
			      TimeWarpFileStreamManager const& /*twfs_manager*/,
			      TimeWarpTerminationManager& /*termination_manager*/,
			      TimeWarpStatistics& /*tw_stats*/
			      ) 
    {}
    virtual bool checkpointRequired() const { return false; }
    virtual void doGenerateCheckpoint(cereal::PortableBinaryOutputArchive& /*ar*/) {}
    virtual void doBlock() {}
    virtual bool terminateAfterCheckpoint() const { return false; }

  private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
  };


  class NullCheckpointManager :
    public TimeWarpCheckpointManager
  {
  public:
    NullCheckpointManager(Configuration const& config) :
      TimeWarpCheckpointManager{ config }
    {}
  };

  
}  // namespace warped

#endif

