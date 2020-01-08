#ifndef MAIN_LEGACY_INCLUDE_ROUTER_V2_HPP_
#define MAIN_LEGACY_INCLUDE_ROUTER_V2_HPP_

#include <list>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>

#include "message.hpp"

class RouterNode {
 public:
  class Queue;

  RouterNode() : parent(nullptr), path("/") {}

  RouterNode(RouterNode* parent, std::string path_fragment)
      : parent(parent), path(path_fragment) {}

  explicit RouterNode(std::string path_fragment)
      : parent(nullptr), path(path_fragment) {}

  ~RouterNode();

  struct MasterStructure {
    explicit MasterStructure(Queue* queue) : queue(queue) {}
    Queue* queue;
    std::list<RouterNode*> aliases;
  };

  void _send_message(Message&& message);

  void send_message(Message&& message) {
    if (is_alias()) {
      for (RouterNode* master : *std::get<std::list<RouterNode*>*>(content)) {
        master->send_message(std::move(message));
      }
    } else if (is_master()) {
      _send_message(std::move(message));
    }
  }

  bool has_listeners() {
    return (is_alias() &&
            !std::get<std::list<RouterNode*>*>(content)->empty()) ||
           is_master();
  }

  std::optional<Message> _receive_message(uint32_t timeout);

  std::optional<Message> receive_message(uint32_t timeout) {
    if (!is_master()) {
      printf("one can only call receive on a master node\n");
      return std::nullopt;
    }
    return _receive_message(timeout);
  }

  void _add_master();

  bool is_master() { return std::holds_alternative<MasterStructure>(content); }

  bool is_alias() {
    return std::holds_alternative<std::list<RouterNode*>*>(content);
  }

  void was_aliased(RouterNode* alias_node) {
    if (is_master()) {
      std::get<MasterStructure>(content).aliases.emplace_back(alias_node);
    } else {
      printf("can only alias a master node\n");
    }
  }

  void add_master() {
    if (is_alias() || is_master()) {
      printf("node already used \n");
    }
    _add_master();
  }

  void remove_alias(RouterNode* master) {
    if (!is_alias()) {
      printf("wrong remove function \n");
    } else {
      std::list<RouterNode*>* aliases =
          std::get<std::list<RouterNode*>*>(content);
      aliases->remove(master);
      if (aliases->begin() == aliases->end()) {
        content.emplace<std::monostate>();
      }
    }
  }

  void _remove_queue();

  struct PointerHash {
    bool operator()(RouterNode* node) const {
      // if(!lhs || !rhs) return true;
      return std::hash<std::string>()(node->path);
    }
  };

  struct PointerEqual {
    bool operator()(RouterNode* lhs, RouterNode* rhs) const {
      // if(!lhs || !rhs) return true;
      return lhs->path == rhs->path;
    }
  };

  struct PointerCompare {
    bool operator()(RouterNode* lhs, RouterNode* rhs) const {
      // if(!lhs || !rhs) return true;
      return lhs->path < rhs->path;
    }
  };

  std::optional<std::list<RouterNode*>> prepare_remove() {
    if (is_master()) {
      std::list<RouterNode*> aliases =
          std::move(std::get<MasterStructure>(content).aliases);
      _remove_queue();
      content.emplace<std::monostate>();
      return std::move(aliases);
    }
    return std::nullopt;
  }

  void add_alias(RouterNode* master) {
    if (is_master()) {
      printf("node already used \n");
    } else if (is_alias()) {
      std::get<std::list<RouterNode*>*>(content)->emplace_back(master);
    } else {
      content.emplace<std::list<RouterNode*>*>(new std::list<RouterNode*>);
      std::get<std::list<RouterNode*>*>(content)->emplace_back(master);
    }
  }

  std::variant<std::monostate, MasterStructure, std::list<RouterNode*>*>
      content;
  RouterNode* parent;
  std::unordered_set<RouterNode*, PointerHash, PointerEqual> children;
  // std::set<RouterNode*, PointerCompare> children;
  std::string path;
};

class RouterV2 {
 public:
  static RouterV2& getInstance() {
    static RouterV2 instance;
    return instance;
  }

  void send(Message&& message) {
    send_recursive(&root, std::string(message.receiver()), std::move(message),
                   false, false);
  }

  void send_recursive(RouterNode* node, std::string path_tail,
                      Message&& message, bool wildcard_in_path, bool wildcard) {
    if (wildcard_in_path && node->has_listeners()) {
      std::stringstream new_head;
      RouterNode* walk = node;
      std::stack<RouterNode*> path;
      while (walk) {
        if (walk->parent) {
          path.push(walk);
        }
        walk = walk->parent;
      }

      while (!path.empty()) {
        new_head << path.top()->path;
        path.pop();
        if (!path.empty()) {
          new_head << "/";
        }
      }
      Message out = Message(message.sender(), new_head.str().c_str(),
                            message.tag(), message.buffer());
      node->send_message(std::move(out));
    } else if (node->has_listeners()) {
      if (!wildcard && path_tail.empty()) {
        node->send_message(std::move(message));
        return;
      } else {
        node->send_message(Message(message));
      }
    }
    std::istringstream stream(path_tail);
    std::string next_segment;
    std::string new_tail;
    std::getline(stream, next_segment, '/');
    if (!std::getline(stream, new_tail)) {
      new_tail = "";
    }

    if ((!next_segment.empty() &&
         (next_segment == "+" || next_segment == "#")) ||
        wildcard) {
      for (auto it = node->children.begin(); it != node->children.end(); ++it) {
        RouterNode* child = const_cast<RouterNode*>((*it));
        send_recursive(child, new_tail, Message(message), true,
                       (next_segment == "#" || wildcard));
      }
    } else {
      RouterNode tmp = RouterNode(next_segment);
      auto it = node->children.find(&tmp);
      if (it != node->children.end()) {
        RouterNode* child = const_cast<RouterNode*>((*it));
        send_recursive(child, new_tail, std::move(message), wildcard_in_path,
                       false);
      }
    }
  }

  void register_actor(const char* actor_id) {
    std::unique_lock<std::mutex> lock(mutex);
    RouterNode* current = &root;
    std::istringstream path(actor_id);
    std::string next_segment;
    while (std::getline(path, next_segment, '/')) {
      RouterNode tmp = RouterNode(next_segment);
      auto it = current->children.find(&tmp);
      if (it != current->children.end()) {
        current = const_cast<RouterNode*>(((*it)));
      } else {
        current = const_cast<RouterNode*>(
            (*(current->children.emplace(new RouterNode(current, next_segment))
                   .first)));
      }
    }
    current->add_master();
  }

  void deregister_actor(const char* actor_id) {
    std::unique_lock<std::mutex> lock(mutex);

    RouterNode* current = &root;
    std::istringstream path(actor_id);
    std::string next_segment = "";
    while (std::getline(path, next_segment, '/')) {
      RouterNode tmp = RouterNode(next_segment);
      auto it = current->children.find(&tmp);
      if (it != current->children.end()) {
        current = const_cast<RouterNode*>(((*it)));
      } else {
        printf("error, path does not exist\n");
        return;
      }
    }

    auto aliases = current->prepare_remove();

    if (aliases) {
      for (RouterNode* alias : *aliases) {
        alias->remove_alias(current);
        cleanup_path(alias);
      }
    }

    if (current->children.begin() == current->children.end()) {
      cleanup_path(current);
    }
  }

  void cleanup_path(RouterNode* tail) {
    RouterNode* current = tail;
    RouterNode* previous = nullptr;
    while (current) {
      if (previous) {
        delete previous;
        RouterNode tmp = RouterNode(previous->path);
        current->children.erase(&tmp);
      }
      if (!current->is_master() && !current->is_alias() &&
          current->children.begin() == current->children.end() &&
          current->parent != nullptr) {
        previous = current;
        current = current->parent;
      } else {
        break;
      }
    }
  }

  void register_alias(const char* actor_id, const char* alias_id) {
    std::unique_lock<std::mutex> lock(mutex);
    RouterNode* current = &root;
    std::istringstream path(actor_id);
    std::string next_segment;

    while (std::getline(path, next_segment, '/')) {
      RouterNode tmp = RouterNode(next_segment);
      auto it = current->children.find(&tmp);
      if (it != current->children.end()) {
        current = const_cast<RouterNode*>(((*it)));
      } else {
        printf("error, path does not exist\n");
        return;
      }
    }

    RouterNode* master_node = current;

    path = std::istringstream(alias_id);
    next_segment.clear();
    current = &root;

    while (std::getline(path, next_segment, '/')) {
      if (next_segment == "#") {
        break;
      }
      RouterNode tmp = RouterNode(next_segment);
      auto it = current->children.find(&tmp);
      if (it != current->children.end()) {
        current = const_cast<RouterNode*>(((*it)));
      } else {
        auto inserted =
            current->children.emplace(new RouterNode(current, next_segment));
        current = const_cast<RouterNode*>((*inserted.first));
      }
    }

    current->add_alias(master_node);
    master_node->was_aliased(current);
  }

  std::optional<Message> receive(const char* receiver, size_t wait_time = 0) {
    RouterNode* current = &root;
    std::istringstream path(receiver);
    std::string next_segment;

    while (std::getline(path, next_segment, '/')) {
      RouterNode tmp = RouterNode(next_segment);
      auto it = current->children.find(&tmp);
      if (it != current->children.end()) {
        current = const_cast<RouterNode*>(((*it)));
      } else {
        printf("error, path does not exist\n");
        return std::nullopt;
      }
    }

    return current->receive_message(wait_time);
  }

 private:
  RouterNode root;
  std::mutex mutex;
};

#endif  // MAIN_LEGACY_INCLUDE_ROUTER_V2_HPP_
