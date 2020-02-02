#include "remote_connection.hpp"

std::unordered_map<std::string, RemoteConnection::SequenceInfo>
    RemoteConnection::sequence_infos;
std::atomic<uint32_t> RemoteConnection::sequence_number{1};
std::mutex RemoteConnection::mtx;
