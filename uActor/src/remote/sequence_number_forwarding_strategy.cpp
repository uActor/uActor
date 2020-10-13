#include "remote/sequence_number_forwarding_strategy.hpp"

namespace uActor::Remote {
std::unordered_map<std::string, Remote::SequenceInfo>
    SequenceNumberForwardingStrategy::incomming_sequence_infos;
std::mutex SequenceNumberForwardingStrategy::incomming_mtx;
std::atomic<uint32_t> SequenceNumberForwardingStrategy::sequence_number{1};

bool SequenceNumberForwardingStrategy::should_accept(
    const PubSub::Publication& p) {
  auto publisher_node_id = std::string(*p.get_str_attr("publisher_node_id"));
  auto sequence_number = p.get_int_attr("_internal_sequence_number");
  auto epoch_number = p.get_int_attr("_internal_epoch");

  if (!sequence_number || !epoch_number) {
    return false;
  }

  uActor::Support::Logger::trace("FORWARDING-STRATEGY", "MESSAGE FROM",
                                 publisher_node_id.c_str());

  std::unique_lock lck(incomming_mtx);
  auto sequence_info_it = incomming_sequence_infos.find(publisher_node_id);

  if (sequence_info_it == incomming_sequence_infos.end()) {
    incomming_sequence_infos.try_emplace(publisher_node_id, *sequence_number,
                                         *epoch_number,
                                         BoardFunctions::timestamp());
    return true;
  } else {
    auto& sequence_info = sequence_info_it->second;
    if (sequence_info.is_older_than(*sequence_number, *epoch_number)) {
      if (*epoch_number - sequence_info.epoch >= 0 &&
          *sequence_number - sequence_info.sequence_number > 1) {
        Support::Logger::debug(
            "REMOTE-CONNECTION", "RECEIVE", "Potentially lost %d message(s)",
            *sequence_number - sequence_info.sequence_number);
      }
      sequence_info = Remote::SequenceInfo(*sequence_number, *epoch_number,
                                           BoardFunctions::timestamp());
      return true;
    }
  }
  return false;
}

void SequenceNumberForwardingStrategy::add_incomming_routing_fields(
    PubSub::Publication* p) {
  if (p->has_attr("_internal_forwarded_by")) {
    p->set_attr("_internal_forwarded_by",
                std::string(*p->get_str_attr("_internal_forwarded_by")) + "," +
                    partner_node_id());
  } else {
    p->set_attr("_internal_forwarded_by", partner_node_id());
  }
}

bool SequenceNumberForwardingStrategy::should_forward(
    const PubSub::Publication& p) {
  if (p.has_attr("_internal_forwarded_by")) {
    size_t start = 0;
    std::string_view forwarders = *p.get_str_attr("_internal_forwarded_by");
    while (start < forwarders.length()) {
      start = forwarders.find(partner_node_id(), start);
      if (start != std::string_view::npos) {
        size_t end_pos = start + partner_node_id().length();
        if ((start == 0 || forwarders.at(start - 1) == ',') &&
            (end_pos == forwarders.length() || forwarders.at(end_pos) == ',')) {
          return false;
        } else {
          start++;
        }
      } else {
        break;
      }
    }
  }

  // The might be multiple subscriptions for the same message.
  // They are indistinguishable on the remote node. Hence, filter them
  // and prevent forwarding. This is e.g. important usefull for external
  // deployments if the deployment manager use label filters. As this
  // has to be per connection, this unfortunately causes a potentially
  // large memory overhead.
  // TODO(raphaehetzel) Add garbage collector to remove outdated
  // sequence infos. As this is an optimization, this is safe here.

  {
    auto publisher_node_id = p.get_str_attr("publisher_node_id");
    auto sequence_number = p.get_int_attr("_internal_sequence_number");
    auto epoch_number = p.get_int_attr("_internal_epoch");

    if (publisher_node_id && sequence_number && epoch_number) {
      auto new_sequence_info = uActor::Remote::SequenceInfo(
          *sequence_number, *epoch_number, BoardFunctions::timestamp());
      auto sequence_info_it =
          outgoing_sequence_infos.find(std::string(*publisher_node_id));
      if (sequence_info_it == outgoing_sequence_infos.end()) {
        outgoing_sequence_infos.try_emplace(std::string(*publisher_node_id),
                                            std::move(new_sequence_info));
      } else if (sequence_info_it->second.is_older_than(*sequence_number,
                                                        *epoch_number)) {
        sequence_info_it->second = std::move(new_sequence_info);
      } else {
        return false;
      }
    }
  }

  return true;
}
}  // namespace uActor::Remote
