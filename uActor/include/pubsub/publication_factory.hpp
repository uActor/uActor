#pragma once

#include <memory>

#include "publication.hpp"
#include "support/logger.hpp"

namespace uActor::PubSub {

class PublicationFactory {
  struct Storage;

 public:
  PublicationFactory();
  PublicationFactory(PublicationFactory&&);

  PublicationFactory& operator=(PublicationFactory&&);

  ~PublicationFactory();

  void write(const char* data, size_t size);

  std::optional<Publication> build();

 private:
  std::unique_ptr<Storage> storage;
};

}  // namespace uActor::PubSub
