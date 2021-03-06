#pragma once

#include <string>

namespace uActor::ActorRuntime {
struct CodeIdentifier {
  CodeIdentifier(std::string_view type, std::string_view version,
                 std::string_view runtime_type)
      : type(type), version(version), runtime_type(runtime_type) {}

  bool operator==(const CodeIdentifier& other) const {
    return type == other.type && version == other.version &&
           runtime_type == other.runtime_type;
  }

  bool operator<(const CodeIdentifier& other) const {
    return type < other.type ||
           (type == other.type && version < other.version) ||
           (version == other.version && runtime_type < other.runtime_type);
  }

  std::string type;
  std::string version;
  std::string runtime_type;
};

struct CodeIdentifierHasher {
  size_t operator()(const CodeIdentifier& code_identifier) const {
    return std::hash<std::string>{}(code_identifier.runtime_type +
                                    code_identifier.type +
                                    code_identifier.version);
  }
};
}  // namespace uActor::ActorRuntime
