#pragma once

#include <map>
#include <string>
#include <utility>

namespace uActor::Remote {
struct ClusterLabelValuePair {
  ClusterLabelValuePair(std::string local_label, std::string remote_label)
      : local_label(std::move(local_label)),
        remote_label(std::move(remote_label)) {}
  std::string local_label;
  std::string remote_label;
};

using ClusterLabels = std::map<std::string, ClusterLabelValuePair>;
}  // namespace uActor::Remote
