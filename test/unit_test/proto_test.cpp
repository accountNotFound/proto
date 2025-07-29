#include "proto.h"

#include <format>
#include <iostream>

#include "codec.h"
#include "mtest.h"

template <typename Codec = proto::BaseCodec>
struct User : public proto::BaseModel<User<Codec>> {
  using Model = User;

  FIELD(int, id, -1);
  FIELD(std::string, name, "unkown");
};

TEST(proto, repr_codec) {
  User<> u = {.id = 123, .name = "Alice"};

  proto::ReprCodec repr;
  repr.encode(u);
  std::cout << repr.buffer().str() << '\n';

  proto::JsonCodec json;
  json.encode(u);
  std::cout << json.buffer().str() << '\n';
}
