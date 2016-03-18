#include "Simulation.hpp"

#include <memory>
#include <vector>
#include <fstream>
#include <sstream>
#include <tuple>
#include <algorithm>

#include "Configuration.hpp"
#include "EventDispatcher.hpp"
#include "Partitioner.hpp"
#include "LogicalProcess.hpp"
#include "TimeWarpMPICommunicationManager.hpp"
#include "serialization.hpp"
#include "json/json.h"

extern "C" {
    void warped_is_present(void) {
        return;
    }
}

namespace warped {

std::unique_ptr<EventDispatcher> Simulation::event_dispatcher_;

Simulation::Simulation(const std::string& model_description, int argc, const char* const* argv,
                       const std::vector<TCLAP::Arg*>& cmd_line_args)
    : config_(model_description, argc, argv, cmd_line_args) {}

Simulation::Simulation(const std::string& model_description, int argc, const char* const* argv)
    : config_(model_description, argc, argv) {}

Simulation::Simulation(const std::string& config_file_name, unsigned int max_sim_time)
    : config_(config_file_name, max_sim_time) {}

TerminationStatus Simulation::simulate(const std::vector<LogicalProcess*>& lps) {
    check(lps);

    auto comm_manager = config_.makeCommunicationManager();
    unsigned int num_partitions = comm_manager->initialize();
    TerminationStatus status;

    if (config_.isRestarting()) {
      restart(lps, comm_manager);
    } else {
      auto partitioned_lps = config_.makePartitioner()->partition(lps, num_partitions);
      comm_manager->initializeLPMap(partitioned_lps);
      
      comm_manager->waitForAllProcesses();
      
      unsigned int num_schedulers = num_partitions;
      auto local_partitioner = config_.makeLocalPartitioner(comm_manager->getID(), num_schedulers);
      auto local_partitions =
        local_partitioner->partition(partitioned_lps[comm_manager->getID()], num_schedulers);
      
      event_dispatcher_ = config_.makeDispatcher(comm_manager);
      
      status = event_dispatcher_->startSimulation(local_partitions);
    }

    comm_manager->finalize();

    return status;
}

TerminationStatus Simulation::simulate(const std::vector<LogicalProcess*>& lps,
    std::unique_ptr<Partitioner> partitioner) {
    check(lps);

    auto comm_manager = config_.makeCommunicationManager();
    unsigned int num_partitions = comm_manager->initialize();
    TerminationStatus status;

    if (config_.isRestarting()) {
      restart(lps, comm_manager);
    } else {
      auto partitioned_lps =
        config_.makePartitioner(std::move(partitioner))->partition(lps, num_partitions);
      comm_manager->initializeLPMap(partitioned_lps);
      
      comm_manager->waitForAllProcesses();
      
      unsigned int num_schedulers = num_partitions;
      auto local_partitioner = config_.makeLocalPartitioner(comm_manager->getID(), num_schedulers);
      auto local_partitions =
        local_partitioner->partition(partitioned_lps[comm_manager->getID()], num_schedulers);
      
      event_dispatcher_ = config_.makeDispatcher(comm_manager);

      status = event_dispatcher_->startSimulation(local_partitions);
    }

    comm_manager->finalize();

    return status;
}

TerminationStatus Simulation::restart(const std::vector<LogicalProcess*>& lps, std::shared_ptr<TimeWarpCommunicationManager> comm_manager)
{
  auto file = config_.root()["checkpointing"]["file"];
  if (file.empty()) file = "checkpoint";

  auto id = comm_manager->getID();
  std::ostringstream filename;
  filename << file << "." << id;

  // Open the checkpoint file
  std::ifstream ifs { filename.str() };
  cereal::PortableBinaryInputArchive ar { ifs };

  // Restore the time from which rejuvenation started
  std::chrono::time_point<std::chrono::steady_clock> started;
  ar(started);

  // Restore Configuration
  ar(config_);

  // Restore CommunicationManager
  ar(*comm_manager);

  // make EventDispacher
  event_dispatcher_ = config_.makeDispatcher(comm_manager);
  auto status = event_dispatcher_->restart(lps, ar);

  comm_manager->finalize();

  return status;
}

void Simulation::check(const std::vector<LogicalProcess*>& lps) {
   if(std::adjacent_find(lps.begin(), lps.end(), equalNames) != lps.end())
      throw std::runtime_error(std::string("Two LogicalProcess with the same name."));
}

FileStream& Simulation::getFileStream(LogicalProcess* lp, const std::string& filename,
    std::ios_base::openmode mode, std::shared_ptr<Event> this_event) {

    return event_dispatcher_->getFileStream(lp, filename, mode, this_event);
}

} // namepsace warped
