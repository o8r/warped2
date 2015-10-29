#include <cstring> // for memcpy
#include <algorithm> // for std::remove_if
#include <cstdint>  // for uint8_t type
#include <cassert>

#include "TimeWarpMPICommunicationManager.hpp"
#include "TimeWarpEventDispatcher.hpp"          // for EventMessage
#include "TimeWarpMatternGVTManager.hpp"
#include "utility/memory.hpp"
#include "utility/warnings.hpp"

TicketLock serialization_lock;

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

    send_queue_ = std::make_shared<MPISendQueue>(max_buffer_size_);
    recv_queue_ = std::make_shared<MPIRecvQueue>(max_buffer_size_);

    recv_queue_->msg_list_ = make_unique<std::deque<std::unique_ptr<TimeWarpKernelMessage>>[]>(num_worker_threads_+1);

    send_queue_->pending_request_list_.reserve(10000);
    recv_queue_->pending_request_list_.reserve(10000);

    return getNumProcesses();
}

void TimeWarpMPICommunicationManager::finalize() {
    MPI_Finalize();
}

unsigned int TimeWarpMPICommunicationManager::getNumProcesses() {
    int size = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    return size;
}

unsigned int TimeWarpMPICommunicationManager::getID() {
    int id = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    return id;
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

    send_queue_->testRequests(thread_id);

    send_queue_->mpi_lock_.lock();

    unsigned int receiver_id = msg->receiver_id;

    serialization_lock.lock();
    std::ostringstream oss;
    cereal::PortableBinaryOutputArchive oarchive(oss);
    oarchive(std::move(msg));
    serialization_lock.unlock();

    auto new_request = make_unique<PendingRequest>(make_unique<uint8_t[]>(max_buffer_size_));
    std::memcpy(new_request->buffer_.get(), oss.str().c_str(), oss.str().length()+1);

    if(MPI_Isend(
            new_request->buffer_.get(),
            oss.str().length()+1,
            MPI_BYTE,
            receiver_id,
            thread_id,
            MPI_COMM_WORLD,
            &new_request->request_) != MPI_SUCCESS) {
        throw std::runtime_error("MPI_Isend failed");
    }

    send_queue_->pending_request_list_.push_back(std::move(new_request));

    send_queue_->mpi_lock_.unlock();
}

void TimeWarpMPICommunicationManager::sendMessage(std::unique_ptr<TimeWarpKernelMessage> msg) {
    sendMessage(std::move(msg), num_worker_threads_);
}

std::unique_ptr<TimeWarpKernelMessage> TimeWarpMPICommunicationManager::getMessage(unsigned int thread_id) {

    assert(thread_id <= num_worker_threads_);

    if (recv_queue_->msg_list_[thread_id].empty()) {
        return nullptr;
    }

    auto msg = std::move(recv_queue_->msg_list_[thread_id].front());
    recv_queue_->msg_list_[thread_id].pop_front();

    return std::move(msg);
}

unsigned int TimeWarpMPICommunicationManager::startReceiveRequests(unsigned int thread_id) {
    int flag = 0;
    MPI_Status status;
    unsigned int requests = 0;

    assert(thread_id <= num_worker_threads_);

    recv_queue_->testRequests(thread_id);

    recv_queue_->mpi_lock_.lock();

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

            recv_queue_->pending_request_list_.push_back(std::move(new_request));

        } else {
            break;
        }

        requests++;
    }

    recv_queue_->mpi_lock_.unlock();

    return requests;
}

unsigned int MPISendQueue::testRequests(unsigned int thread_id) {
    unsigned int requests_completed = 0;
    unused(thread_id);

    mpi_lock_.lock();

    for (auto& pr : pending_request_list_) {
        MPI_Test(&pr->request_, &pr->flag_, &pr->status_);
        if (pr->flag_ != 0) {
            requests_completed++;
        }
    }

    pending_request_list_.erase(
        std::remove_if(
            pending_request_list_.begin(),
            pending_request_list_.end(),
            [&](const std::unique_ptr<PendingRequest>& pr) { return  pr->flag_ != 0; }),
        pending_request_list_.end()
    );

    mpi_lock_.unlock();

    return requests_completed;
}

unsigned int MPIRecvQueue::testRequests(unsigned int thread_id) {
    unsigned int requests_completed = 0;

    mpi_lock_.lock();

    for (auto& pr : pending_request_list_) {
        MPI_Test(&pr->request_, &pr->flag_, &pr->status_);
        if (pr->flag_ != 0) {
            requests_completed++;
        }
    }

    for (auto& pr : pending_request_list_) {
        std::unique_ptr<TimeWarpKernelMessage> msg = nullptr;

        int count;
        MPI_Get_count(&pr->status_, MPI_BYTE, &count);

        serialization_lock.lock();
        std::istringstream iss(std::string(reinterpret_cast<char*>(pr->buffer_.get()), count));
        cereal::PortableBinaryInputArchive iarchive(iss);
        iarchive(msg);
        serialization_lock.unlock();

        msg_list_[thread_id].push_back(std::move(msg));
    }

    pending_request_list_.erase(
        std::remove_if(
            pending_request_list_.begin(),
            pending_request_list_.end(),
            [&](const std::unique_ptr<PendingRequest>& pr) { return  pr->flag_ != 0; }),
        pending_request_list_.end()
    );

    mpi_lock_.unlock();

    return requests_completed;
}

} // namespace warped

