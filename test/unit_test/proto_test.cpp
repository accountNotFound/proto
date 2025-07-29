#include "proto.h"

#include <iostream>
#include <vector>

#include "codec.h"
#include "mtest.h"

template <typename T = proto::BaseCodec>
struct User : public proto::BaseModel<User<T>> {
  using Model = User;

  FIELD(int, id, -1);
  FIELD(std::string, name, "unkown");
};

template <typename T = proto::BaseCodec>
struct Message : public proto::BaseModel<Message<T>> {
  using Model = Message;

  FIELD(int, code, 0);
  FIELD(std::string, msg, "");
  FIELD(std::vector<User<>>, data, {});
};

TEST(proto, repr_codec) {
  User<> u1 = {.id = 123, .name = "Alice"};
  User<> u2 = {.id = 456, .name = "Bob"};
  Message<> msg = {.data = {u1, u2}};

  proto::ReprCodec repr;
  repr.encode(msg);
  std::cout << repr.buffer().str() << '\n';

  proto::JsonCodec json;
  json.encode(msg);
  std::cout << json.buffer().str() << '\n';
}
