#include "uactor_sdk.hpp"
#include "uactor_sdk/include/data_structures.hpp"
#include "uactor_sdk/include/imports.hpp"
#include "uactor_sdk/include/publication.hpp"
#include "uactor_sdk/include/subscription.hpp"
#include "uactor_sdk/include/uactor.hpp"
#include "uactor_sdk/include/uactor_sdk.hpp"
#include "utils.hpp"

void func2(uactor::Publication& pub) {
  print("func2");
  uactor::Publication::Entry* entry = pub.find("body");
  if (entry == nullptr) {
    print("no body");
    return;
  }
  print(static_cast<const char*>(entry->_elem));
}

WASM_EXPORT int receive(size_t _pub) {
  uactor::Subscription subscription{};
  subscription.insert("test", "val");
  subscribe(&subscription);
  uactor::Publication pub{_pub};
  uactor::Publication::Entry* entry = pub.find("type");
  if (entry == nullptr) {
    return -1;
  }
  if (entry->_t == uactor::Publication::Type::STRING &&
      string_compare(static_cast<const char*>(entry->_elem), "init")) {
    uactor::Publication ret_pub{};
    ret_pub.insert("type", uactor::Publication::Entry{
                               uactor::Publication::Type::STRING, "type",
                               static_cast<const void*>("http_request")});

    ret_pub.insert("http_method",
                   uactor::Publication::Entry{uactor::Publication::Type::STRING,
                                              "http_method",
                                              static_cast<const void*>("GET")});

    ret_pub.insert("request_url",
                   uactor::Publication::Entry{
                       uactor::Publication::Type::STRING, "request_url",
                       static_cast<const void*>("http://webhookinbox.com/")});

    int* req_id = malloc<int>();
    ret_pub.insert("request_id",
                   uactor::Publication::Entry{
                       uactor::Publication::Type::INT32, "request_id",
                       static_cast<const void*>(req_id)});
    publish(&ret_pub);
    print("Published http request");
    free(req_id);
    return 1;
  } else {
    switch (entry->_t) {
      case uactor::Publication::Type::STRING:
        print(static_cast<const char*>(entry->_elem));
        func2(pub);
        break;
      case uactor::Publication::Type::INT32:
        print("int");
        break;
      case uactor::Publication::Type::FLOAT:
        print("float");
        break;
      case uactor::Publication::Type::BIN:
        print("bin");
        break;
      default:
        print("should not happen");
    }
    return 3;
  }
  return 0;
}

// having functions linked that are not used lead to issues with wasm3
// since I was unable to find the correct compiler flag,
// every linked function is called once in the ignore_me function
// and will therefore not be omitted by the compiler
WASM_EXPORT void ignore_me() {
  void* a = malloc(1);
  free(a);
  print(nullptr);
  publish(nullptr);
  subscribe(nullptr);
  pub_get_val(0, nullptr, nullptr);
}
