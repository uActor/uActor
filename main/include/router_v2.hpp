#ifndef MAIN_INCLUDE_ROUTER_V2_HPP_
#define MAIN_INCLUDE_ROUTER_V2_HPP_

#include <list>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stack>
#include <string>
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

  bool operator<(const RouterNode& other) const { return path < other.path; }

  bool operator==(const RouterNode& other) const { return path == other.path; }

  void _send_message(const Message& message);

  void send_message(const Message& message) {
    if (is_alias()) {
      for (RouterNode* master : *std::get<std::list<RouterNode*>*>(content)) {
        master->send_message(message);
      }
    } else if (is_master()) {
      _send_message(message);
    }
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

  std::optional<std::list<RouterNode*>> prepare_remove() {
    if (is_master()) {
      std::list<RouterNode*> aliases =
          std::move(std::get<MasterStructure>(content).aliases);
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
  std::set<RouterNode> children;
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
                   false);
  }

  void send_recursive(RouterNode* node, std::string path_tail,
                      const Message& message, bool wildcard,
                      std::string head_path = "") {
    node->send_message(message);
    std::istringstream stream(path_tail);
    std::string next_segment;
    std::string new_tail;
    if (!std::getline(stream, next_segment, '/')) {
      next_segment = "";
    }
    if (!std::getline(stream, new_tail)) {
      new_tail = "";
    }

    if (next_segment == "+" || next_segment == "#" || wildcard) {
      for (auto it = node->children.begin(); it != node->children.end(); ++it) {
        RouterNode* child = const_cast<RouterNode*>(&(*it));
        std::stringstream np;
        np << head_path << child->path;
        if (new_tail != "") {
          np << "/" << new_tail;
        }
        std::stringstream new_head;
        new_head << head_path << child->path << "/";
        Message out = Message(message.sender(), np.str().c_str(), message.tag(),
                              message.buffer());
        send_recursive(child, new_tail, out, (next_segment == "#" || wildcard),
                       new_head.str());
      }
    } else {
      auto it = node->children.find(RouterNode(next_segment));
      if (it != node->children.end()) {
        RouterNode* child = const_cast<RouterNode*>(&(*it));
        std::stringstream new_head;
        new_head << head_path << child->path << "/";
        send_recursive(child, new_tail, message, false, new_head.str());
      }
    }
  }

  void register_actor(const char* actor_id) {
    std::unique_lock<std::mutex> lock(mutex);
    RouterNode* current = &root;
    std::istringstream path(actor_id);
    std::string next_segment;
    while (std::getline(path, next_segment, '/')) {
      std::set<RouterNode>::iterator it =
          current->children.find(RouterNode(next_segment));
      if (it != current->children.end()) {
        current = const_cast<RouterNode*>(&((*it)));
      } else {
        current = const_cast<RouterNode*>(
            &(*(current->children.emplace(RouterNode(current, next_segment))
                    .first)));
      }
    }
    current->add_master();
  }

  void deregister_actor(const char* actor_id) {
    std::unique_lock<std::mutex> lock(mutex);
    std::stack<std::pair<std::string, RouterNode*>> previous;

    RouterNode* current = &root;
    std::istringstream path(actor_id);
    std::string next_segment = "";

    previous.push(std::make_pair(next_segment, current));

    while (std::getline(path, next_segment, '/')) {
      auto it = current->children.find(RouterNode(next_segment));
      if (it != current->children.end()) {
        current = const_cast<RouterNode*>(&((*it)));
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
        current->children.erase(RouterNode(previous->path));
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
      auto it = current->children.find(RouterNode(next_segment));
      if (it != current->children.end()) {
        current = const_cast<RouterNode*>(&((*it)));
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
      auto it = current->children.find(RouterNode(next_segment));
      if (it != current->children.end()) {
        current = const_cast<RouterNode*>(&((*it)));
      } else {
        auto inserted = current->children.emplace(current, next_segment);
        current = const_cast<RouterNode*>(&(*inserted.first));
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
      auto it = current->children.find(RouterNode(next_segment));
      if (it != current->children.end()) {
        current = const_cast<RouterNode*>(&((*it)));
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

#endif  // MAIN_INCLUDE_ROUTER_V2_HPP_
