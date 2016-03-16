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
    void notifyEvent(std::shared_ptr<Event> event) {
      if (onEvent(event))
	generateCheckpoint();
    }
    void notifyPassive() {
      if (onPassive())
	generateCheckpoint();
    }
    void notifyGVTUpdate(unsigned int gvt) {
      if (onGVT(gvt))
	generateCheckpoint();
    }

    virtual void generateCheckpoint();
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
    virtual bool onEvent(std::shared_ptr<Event> /*event*/) { return false; }
    virtual bool onPassive() { return false; }
    virtual bool onGVT(unsigned int /*gvt*/) { return false; }
    virtual void doGenerateCheckpoint(cereal::PortableBinaryOutputArchive& /*ar*/) {}
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

