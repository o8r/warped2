#include <cstring> // for memcpy
#include <algorithm> // for std::remove_if
#include <cstdint>  // for uint8_t type
#include <cassert>

#include "TimeWarpMPICommunicationManager.hpp"
#include "TimeWarpEventDispatcher.hpp"          // for EventMessage
#include "TimeWarpMatternGVTManager.hpp"
#include "utility/memory.hpp"
#include "utility/warnings.hpp"

namespace warped {

unsigned int TimeWarpMPICommunicationManager::initialize() {

    // MPI_Init requires command line arguments, but doesn't use them. Just give
    // it an empty vector.
    int argc = 0;
    char** argv = new char*[1];
    argv[0] = NULL;
    int provided;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    delete [] argv;

    if (provided < MPI_THREAD_MULTIPLE) {
        throw std::runtime_error("MPI_THREAD_MULTIPLE is not supported by the MPI \
implementation you are currently using.");
    }

    send_queue_ = std::make_shared<MessageQueue>(max_buffer_size_);
    recv_queue_ = std::make_shared<MessageQueue>(max_buffer_size_);

    send_queue_->pending_request_list_ = make_unique<std::vector<std::unique_ptr<PendingRequest>>[]>(num_worker_threads_+1);
    recv_queue_->pending_request_list_ = make_unique<std::vector<std::unique_ptr<PendingRequest>>[]>(num_worker_threads_+1);

    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank_);
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes_);

    return getNumProcesses();
}

void TimeWarpMPICommunicationManager::finalize() {
    MPI_Finalize();
}

unsigned int TimeWarpMPICommunicationManager::getNumProcesses() {
    return num_processes_;
}

unsigned int TimeWarpMPICommunicationManager::getID() {
    return my_rank_;
}

int TimeWarpMPICommunicationManager::waitForAllProcesses() {
    assert(isInitiatingThread());
    return MPI_Barrier(MPI_COMM_WORLD);
}

bool TimeWarpMPICommunicationManager::isInitiatingThread() {
    int flag = 0;
    MPI_Is_thread_main(&flag);
    return flag;
}

int
TimeWarpMPICommunicationManager::sumReduceUint64(uint64_t *send_local, uint64_t *recv_global) {
    return MPI_Reduce(send_local, recv_global, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
}

int
TimeWarpMPICommunicationManager::gatherUint64(uint64_t *send_local, uint64_t *recv_root) {
    return MPI_Gather(send_local, 1, MPI_UINT64_T, recv_root, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
}

void TimeWarpMPICommunicationManager::sendMessage(std::unique_ptr<TimeWarpKernelMessage> msg, unsigned int thread_id) {

    assert(thread_id <= num_worker_threads_);

    unsigned int receiver_id = msg->receiver_id;

    testSendRequests(thread_id);

    std::ostringstream oss;
    cereal::PortableBinaryOutputArchive oarchive(oss);
    oarchive(std::move(msg));

    auto new_request = make_unique<PendingRequest>(make_unique<uint8_t[]>(max_buffer_size_));
    std::memcpy(new_request->buffer_.get(), oss.str().c_str(), oss.str().length()+1);

    if(MPI_Isend(
            new_request->buffer_.get(),
            max_buffer_size_,
            MPI_BYTE,
            receiver_id,
            thread_id,
            MPI_COMM_WORLD,
            &new_request->request_) != MPI_SUCCESS) {
        throw std::runtime_error("MPI_Isend failed");
    }

    send_queue_->pending_request_list_[thread_id].push_back(std::move(new_request));
}

void TimeWarpMPICommunicationManager::sendMessage(std::unique_ptr<TimeWarpKernelMessage> msg) {
    sendMessage(std::move(msg), num_worker_threads_);
}

bool TimeWarpMPICommunicationManager::handleReceives(unsigned int thread_id) {
    testReceiveRequests(thread_id);
    startReceiveRequests(thread_id);
    return true;
}

unsigned int TimeWarpMPICommunicationManager::startReceiveRequests(unsigned int thread_id) {
    int flag = 0;
    MPI_Status status;
    unsigned int requests = 0;

    while (true) {

        MPI_Iprobe(
            MPI_ANY_SOURCE,
            thread_id,
            MPI_COMM_WORLD,
            &flag,
            &status);

        if (flag) {
             auto new_request = make_unique<PendingRequest>(make_unique<uint8_t[]>(max_buffer_size_));

            if (MPI_Irecv(
                    new_request->buffer_.get(),
                    max_buffer_size_,
                    MPI_BYTE,
                    MPI_ANY_SOURCE,
                    thread_id,
                    MPI_COMM_WORLD,
                    &new_request->request_) != MPI_SUCCESS) {
                throw std::runtime_error("MPI_Irecv failed");
            }

            recv_queue_->pending_request_list_[thread_id].push_back(std::move(new_request));

        } else {
            break;
        }

        requests++;
    }

    return requests;
}

unsigned int TimeWarpMPICommunicationManager::testSendRequests(unsigned int thread_id) {
    unsigned int requests_completed = 0;

    for (auto& pr : send_queue_->pending_request_list_[thread_id]) {
        MPI_Test(&pr->request_, &pr->flag_, &pr->status_);
        if (pr->flag_ != 0) {
            requests_completed++;
        }
    }

    send_queue_->pending_request_list_[thread_id].erase(
        std::remove_if(
            send_queue_->pending_request_list_[thread_id].begin(),
            send_queue_->pending_request_list_[thread_id].end(),
            [&](const std::unique_ptr<PendingRequest>& pr) { return  pr->flag_ != 0; }),
        send_queue_->pending_request_list_[thread_id].end()
    );

    return requests_completed;
}

unsigned int TimeWarpMPICommunicationManager::testReceiveRequests(unsigned int thread_id) {
    unsigned int requests_completed = 0;

    for (auto& pr : recv_queue_->pending_request_list_[thread_id]) {
        MPI_Test(&pr->request_, &pr->flag_, &pr->status_);
    }

    for (auto& pr : recv_queue_->pending_request_list_[thread_id]) {
        if (pr->flag_ != 0) {
            requests_completed++;

            std::unique_ptr<TimeWarpKernelMessage> msg = nullptr;
            std::istringstream iss(std::string(reinterpret_cast<char*>(pr->buffer_.get()), max_buffer_size_));
            cereal::PortableBinaryInputArchive iarchive(iss);
            iarchive(msg);

            MessageType msg_type = msg->get_type();
            int msg_type_int = static_cast<int>(msg_type);
            msg_handler_by_msg_type_[msg_type_int](std::move(msg));
        }
    }

    recv_queue_->pending_request_list_[thread_id].erase(
        std::remove_if(
            recv_queue_->pending_request_list_[thread_id].begin(),
            recv_queue_->pending_request_list_[thread_id].end(),
            [&](const std::unique_ptr<PendingRequest>& pr) { return  pr->flag_ != 0; }),
        recv_queue_->pending_request_list_[thread_id].end()
    );

    return requests_completed;
}

} // namespace warped

