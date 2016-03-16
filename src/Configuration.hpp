#ifndef WARPED_CONFIGURATION_HPP
#define WARPED_CONFIGURATION_HPP

#include <memory>
#include <string>
#include <sstream>
#include <vector>

namespace TCLAP { class Arg; }
namespace Json { class Value; }
namespace cereal { class access; }

namespace warped {

class EventDispatcher;
class Partitioner;
class TimeWarpCommunicationManager;

// This class is responsible for creating and configuring the EventDispatcher
// to be used in the simulation. There are three tiers of configuration data
// that can be used. There are default values for all necessary configuration
// options. The user can also specify a configuration file that will override
// any specified values. Finally, any configuration value can also be
// specified from the command line, which will override values from the first
// two sources.
class Configuration {
public:
    Configuration(const std::string& model_description, int argc, const char* const* argv);
    Configuration(const std::string& model_description, int argc, const char* const* argv,
                  const std::vector<TCLAP::Arg*>& cmd_line_args);
    Configuration(const std::string& config_file_name, unsigned int max_time);
    ~Configuration();

    /** Copy ctor and assignment.
     * @author O'HARA Mamoru
     * @date 2016 Mar 11
     */
    Configuration(Configuration const& other);
    Configuration& operator=(Configuration const& other);

    // Create a fully configured EventDispatcher
    std::unique_ptr<EventDispatcher>
    makeDispatcher(std::shared_ptr<TimeWarpCommunicationManager> comm_manager);

    // Create a partitioner based on the chosen configuration.
    std::unique_ptr<Partitioner> makePartitioner();

    // Create a partitioner if configured, or return the given user provided
    // partitioner.
    std::unique_ptr<Partitioner> makePartitioner(std::unique_ptr<Partitioner> user_partitioner);

    std::unique_ptr<Partitioner> makeLocalPartitioner(unsigned int node_id,
        unsigned int& num_schedulers);

    // Create a communcation manager based on configurations
    std::shared_ptr<TimeWarpCommunicationManager> makeCommunicationManager();

    bool checkTimeWarpConfigs(uint64_t local_config_id, uint64_t *all_config_ids,
        std::shared_ptr<TimeWarpCommunicationManager> comm_manager);

    bool isRestarting() const;
private:
    void init(const std::string& model_description, int argc, const char* const* argv,
              const std::vector<TCLAP::Arg*>& cmd_line_args);
    void readUserConfig();

    std::string config_file_name_;
    unsigned int max_sim_time_;
    std::unique_ptr<Json::Value> root_;

    friend class cereal::access;
    template <typename Archive> friend void save(Archive&, Configuration const&);
    template <typename Archive> friend void load(Archive&, Configuration&);
public:
    /** returns reference to root of configuration tree.
     * @author O'HARA Mamoru
     * @date 2016 Mar 9
     */
    Json::Value const& root() const { return *root_; }
};

} // namespace warped

#endif

