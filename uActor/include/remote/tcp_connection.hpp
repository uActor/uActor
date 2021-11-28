#pragma once

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "remote_connection.hpp"
#include "stream_publication_framer.hpp"

namespace uActor::Remote {

class TCPConnection : public RemoteConnection {
 public:
  TCPConnection(uint32_t local_id, int32_t socket_id, std::string remote_addr,
                uint16_t remote_port, ConnectionRole connection_role,
                ForwarderSubscriptionAPI* handle)
      : RemoteConnection(local_id, handle),
        sock(socket_id),
        partner_ip(remote_addr),
        partner_port(remote_port),
        connection_role(connection_role) {}

  void process_data(uint32_t len, char* data) {
    framer.process_data(
        len, data,
        [this](PubSub::Publication&& publication, size_t encoded_length) {
          process_remote_publication(std::move(publication), encoded_length);
        });
  }

 private:
  int sock = 0;

  StreamPublicationFramer framer;

  std::string partner_ip;
  uint16_t partner_port;
  ConnectionRole connection_role;

  uint32_t last_read_contact = 0;
  uint32_t last_write_contact = 0;

  int len = 0;
  std::vector<char> rx_buffer = std::vector<char>(512);

  std::queue<std::shared_ptr<std::vector<char>>> write_buffer;
  size_t write_offset = 0;

  friend uActor::Remote::TCPForwarder;
};

}  //  namespace uActor::Remote
