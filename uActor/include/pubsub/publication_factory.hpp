#ifndef UACTOR_INCLUDE_PUBSUB_PUBLICATION_FACTORY_HPP_
#define UACTOR_INCLUDE_PUBSUB_PUBLICATION_FACTORY_HPP_

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

  void write(const char* input, size_t size);

  std::optional<Publication> build();

 private:
  std::unique_ptr<Storage> storage;
};

}  // namespace uActor::PubSub

#endif  // UACTOR_INCLUDE_PUBSUB_PUBLICATION_FACTORY_HPP_
