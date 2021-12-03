// extern long int create_new_publication();

#include "uactor_sdk.hpp"
#include "uactor_sdk/include/imports.hpp"
#include "uactor_sdk/include/uactor.hpp"
#include "uactor_sdk/include/uactor_sdk.hpp"
#include "utils.hpp"

void func2(uactor::Publication* pub) {
  print("func2");
  uactor::Publication::Entry* entry = pub->find("body");
  if(entry == nullptr) {
    print("no body");
    return;
  }
  print(reinterpret_cast<const char*>(entry->_elem));
}

WASM_EXPORT int recieve(int _pub) {
  uactor::Publication* pub = reinterpret_cast<uactor::Publication*>(_pub);
  uactor::Publication::Entry* entry = pub->find("type");
  if(entry == nullptr) {
    print("no type");
    return -1;
  }
  if (string_compare(reinterpret_cast<const char*>(entry->_elem), "init")) {
    print("init");
    uactor::Publication ret_pub{7};
    ret_pub.acc(0)->_t = uactor::Publication::Type::STRING;
    ret_pub.acc(0)->key = "type";
    ret_pub.acc(0)->_elem = reinterpret_cast<const void*>("http_request");

    ret_pub.acc(1)->_t = uactor::Publication::Type::STRING;
    ret_pub.acc(1)->key = "http_method";
    ret_pub.acc(1)->_elem = reinterpret_cast<const void*>("GET");

    ret_pub.acc(2)->_t = uactor::Publication::Type::STRING;
    ret_pub.acc(2)->key = "request_url";
    ret_pub.acc(2)->_elem =
        reinterpret_cast<const void*>("http://webhookinbox.com/");

    ret_pub.acc(3)->_t = uactor::Publication::Type::INT32;
    ret_pub.acc(3)->key = "request_id";

    const int req_id = 0;
    ret_pub.acc(3)->_elem = reinterpret_cast<const void*>(&req_id);
    publish(&ret_pub);
    print("Published");
    return 1;
  } else {
      switch (entry->_t) {
        case uactor::Publication::Type::STRING:
          print(reinterpret_cast<const char*>(entry->_elem));
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
  const auto a = malloc(1);
  free(a);
  print(nullptr);
  publish(nullptr);
}
