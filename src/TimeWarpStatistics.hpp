#ifndef TIME_WARP_STATISTICS_HPP
#define TIME_WARP_STATISTICS_HPP

#include <memory>   // for unique_ptr
#include <cstdint>  // uint64_t
#include <tuple>
#include <type_traits>  /* for remove_reference */

#include "TimeWarpCommunicationManager.hpp"
#include "serialization.hpp"

namespace warped {

template <unsigned I>
struct stats_index{
    stats_index(){}
    static unsigned const value = I;
};

struct Stats {

    std::tuple<
        uint64_t,                   // Local positive events sent   0
        uint64_t,                   // Remote positive events sent  1
        uint64_t,                   // Local negative events sent   2
        uint64_t,                   // Remote positive events sent  3
        uint64_t,                   // Total events sent            4
        double,                     // Percent remote events        5
        uint64_t,                   // Primary rollbacks            6
        uint64_t,                   // Secondary rollbacks          7
        uint64_t,                   // Total rollbacks              8
        uint64_t,                   // Coast forward events         9
        uint64_t,                   // Events processed             10
        uint64_t,                   // Events committed             11
        uint64_t,                   // Total negative events sent   12
        uint64_t,                   // Cancelled events             13
        uint64_t,                   // Total Events Received        14
        uint64_t,                   // Average Maximum Memory       15
        uint64_t,                   // GVT cycles                   16
        uint64_t,                   // Number of objects            17
        uint64_t,                   // Rolled back events           18
        double,                     // Average Rollback Length      19
        double,                     // Efficiency                   20
        double,                     // Time for positive events     21
        double,                     // Time for rollback            22
        double,                     // Time for recovery            23
        uint64_t,                   // Total checkpoints            24
        double,                     // Checkpoint size              25
        double,                     // Time for save checkpoint     26
        double,                     // Time for load checkpoint     27
        double,                     // Time for rejuvenation        28
        uint64_t,                   // Number of rejuvenation       29
        uint64_t                    // dummy/number of elements     30
    > stats_;

    template<unsigned I>
    auto operator[](stats_index<I>) -> decltype(std::get<I>(stats_)) {
        return std::get<I>(stats_);
    }

    template <typename Archive>
    void serialize(Archive& ar) {
      ar(stats_);
    }
};

const stats_index<0> LOCAL_POSITIVE_EVENTS_SENT;
const stats_index<1> REMOTE_POSITIVE_EVENTS_SENT;
const stats_index<2> LOCAL_NEGATIVE_EVENTS_SENT;
const stats_index<3> REMOTE_NEGATIVE_EVENTS_SENT;
const stats_index<4> TOTAL_EVENTS_SENT;
const stats_index<5> PERCENT_REMOTE;
const stats_index<6> PRIMARY_ROLLBACKS;
const stats_index<7> SECONDARY_ROLLBACKS;
const stats_index<8> TOTAL_ROLLBACKS;
const stats_index<9> COAST_FORWARDED_EVENTS;
const stats_index<10> EVENTS_PROCESSED;
const stats_index<11> EVENTS_COMMITTED;
const stats_index<12> TOTAL_NEGATIVE_EVENTS;
const stats_index<13> CANCELLED_EVENTS;
const stats_index<14> TOTAL_EVENTS_RECEIVED;
const stats_index<15> AVERAGE_MAX_MEMORY;
const stats_index<16> GVT_CYCLES;
const stats_index<17> NUM_OBJECTS;
const stats_index<18> EVENTS_ROLLEDBACK;
const stats_index<19> AVERAGE_RBLENGTH;
const stats_index<20> EFFICIENCY;
const stats_index<21> POSITIVE_TIME;
const stats_index<22> ROLLBACK_TIME;
const stats_index<23> RECOVERY_TIME;
const stats_index<24> TOTAL_CHECKPOINTS;
const stats_index<25> CHECKPOINT_SIZE;
const stats_index<26> CHECKPOINT_SAVE_TIME;
const stats_index<27> CHECKPOINT_LOAD_TIME;
const stats_index<28> REJUVENATION_TIME;
  const stats_index<29> TOTAL_REJUVENATION;
const stats_index<30> NUM_STATISTICS;

class TimeWarpStatistics {
public:
    TimeWarpStatistics(std::shared_ptr<TimeWarpCommunicationManager> comm_manager,
        std::string stats_file) :
        comm_manager_(comm_manager), stats_file_(stats_file) {}

    void initialize(unsigned int num_worker_threads, unsigned int num_objects);

    /** returns statistics counter value.
     * @author O'HARA Mamoru
     * @date 2016 Mar 3
     */
    template <unsigned I>
    auto getLocal(stats_index<I> i, unsigned int thread_id) const -> decltype(Stats()[i]) {
      return local_stats_[thread_id][i];
    }

    template <unsigned I>
    void updateAverage(stats_index<I> i, typename std::remove_reference<decltype(Stats()[i])>::type new_val, unsigned int count) {
        global_stats_[i] = (new_val + (count - 1) * global_stats_[i]) / (count);
    }

    /** update local average.
     * @author O'HARA Mamoru
     * @date 2016 Mar 3
     */
    template <unsigned I>
    void updateLocalAverage(stats_index<I> i, unsigned int thread_id, typename std::remove_reference<decltype(Stats()[i])>::type new_val, unsigned int count) {
        local_stats_[thread_id][i] = (new_val + (count-1) * local_stats_[thread_id][i]) / (count);
    }

    template <unsigned I>
    uint64_t upCount(stats_index<I> i, unsigned int thread_id, unsigned int num = 1) {
        local_stats_[thread_id][i] += num;
        return local_stats_[thread_id][i];
    }
    /** count up a global counter.
     * @author O'HARA Mamoru
     * @date 2016 Mar 18
     */
    template <unsigned I>
    uint64_t upGlobalCount(stats_index<I> i, unsigned int num =1) {
        global_stats_[i] += num;
        return global_stats_[i];
    }

    template <unsigned I>
    void sumReduceLocal(stats_index<I> j, typename std::remove_reference<decltype(Stats()[j])>::type *& recv_array) {
        using value_type = typename std::remove_const<typename std::remove_reference<decltype(Stats()[j])>::type>::type;
        value_type local_count = 0;

        for (unsigned int i = 0; i < num_worker_threads_+1; i++) {
            local_count += local_stats_[i][j];
        }

        recv_array = new value_type[comm_manager_->getNumProcesses()];
        gather(&local_count, recv_array);

        for (unsigned int i = 0; i < comm_manager_->getNumProcesses(); i++) {
            global_stats_[j] += recv_array[i];
        }
    }

    void calculateStats();

    void writeToFile(double num_seconds);

    void printStats();

private:
    uint64_t gather(uint64_t* send_local, uint64_t* recv_root) { return comm_manager_->gatherUint64(send_local, recv_root); }
    double gather(double* send_local, double* recv_root) { return comm_manager_->gatherDouble(send_local, recv_root); }

    std::unique_ptr<Stats []> local_stats_;
    Stats global_stats_;

    uint64_t *local_pos_sent_by_node_;
    uint64_t *local_neg_sent_by_node_;
    uint64_t *remote_pos_sent_by_node_;
    uint64_t *remote_neg_sent_by_node_;
    uint64_t *primary_rollbacks_by_node_;
    uint64_t *secondary_rollbacks_by_node_;
    uint64_t *coast_forward_events_by_node_;
    uint64_t *cancelled_events_by_node_;
    uint64_t *processed_events_by_node_;
    uint64_t *committed_events_by_node_;
    uint64_t *total_events_received_by_node_;
    uint64_t *num_objects_by_node_;
    double* rollback_time_by_node_;
    double* recovery_time_by_node_;
    uint64_t* rejuvenation_count_by_node_;

    std::shared_ptr<TimeWarpCommunicationManager> comm_manager_;

    std::string stats_file_;

    unsigned int num_worker_threads_;

    friend class cereal::access;
    template <typename Archive>
    void save(Archive& ar) const {
      ar(stats_file_);
      ar(num_worker_threads_);
      ar(global_stats_);

      for (unsigned int i=0; i<num_worker_threads_+1; ++i)
        ar(local_stats_[i]);
    }
    template <typename Archive>
    void load(Archive& ar) {
      ar(stats_file_);
      ar(num_worker_threads_);
      ar(global_stats_);

      initialize(num_worker_threads_, 0);  // 0 is dummy.

      for (unsigned int i=0; i<num_worker_threads_+1; ++i)
        ar(local_stats_[i]);
    }
    template <typename Archive>
    static void load_and_construct(Archive& ar, cereal::construct<TimeWarpStatistics>& construct) {
      std::string stats_file;
      ar(stats_file);

      construct(nullptr, stats_file);
      ar(construct->num_worker_threads_);
      ar(construct->global_stats_);

      for (unsigned int i=0; i<construct->num_worker_threads_+1; ++i)
        ar(construct->local_stats_[i]);
    }
};

} // namespace warped

#endif

