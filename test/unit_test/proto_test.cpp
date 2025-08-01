#include "proto.h"

#include <iostream>
#include <vector>

#include "mtest.h"
#include "proto.h"

template <typename C = proto::ReprCodec>
struct UserDetail : public proto::BaseModel<UserDetail<C>> {
  using Model = UserDetail;

  PROTO_FIELD(float, height, -1);
  PROTO_FIELD(double, weight, -1);
  PROTO_FIELD(std::string, address, "unknown");
};

template <typename C = proto::ReprCodec>
struct User : public proto::BaseModel<User<C>> {
  using Model = User;

  PROTO_FIELD(uint32_t, id, 0);
  PROTO_FIELD(std::string, name, "unkown");
  PROTO_FIELD(UserDetail<>, detail, {});
};

template <typename T = proto::ReprCodec>
struct Message : public proto::BaseModel<Message<T>> {
  using Model = Message;

  PROTO_FIELD(uint32_t, code, 0);
  PROTO_FIELD(std::string, msg, "");
  PROTO_FIELD(std::vector<User<>>, data, {});
};

User<> user1 = {.id = 123, .name = "Alice", .detail = {.height = 1.6, .weight = 50.5, .address = "Beijing"}};
User<> user2 = {.id = 456, .name = "Bob", .detail = {.height = 1.8, .weight = 72.3, .address = "Shenzhen"}};
Message<> message = {.data = {user1, user2}};

TEST(proto, plaintext_codec) {
  std::string json_str;
  std::string repr_str;
  {
    repr_str = message.encode().value();
    std::cout << repr_str << '\n';

    json_str = message.encode_by<proto::JsonCodec>().value();
    std::cout << json_str << '\n';
  }
  {
    auto msg = Message<>::decode(repr_str);
    ASSERT(msg && msg->data.size() == 2, "");

    auto msg2 = Message<>::decode_by<proto::JsonCodec>(json_str);
    ASSERT(msg2 && msg2->data.size() == 2, "");
  }
  {
    ASSERT(!Message<>::decode(json_str), "");
    ASSERT(!Message<>::decode_by<proto::JsonCodec>(repr_str), "");
  }
}

TEST(proto, pretty_decode) {
  auto repr_str = R"(
(0, "", [
  (123, "Alice", (
    1.6, 50.5, "Beijing"
  )), 
  (456, "Bob", (
    1.8, 72.3, "Shenzhen"
  ))
])
)";

  auto json_str = R"(
{
  "code": 0,
  "msg": "",
  "data": [
    {
      "id": 123,
      "name": "Alice",
      "detail": {
        "height": 1.6,
        "weight": 50.5,
        "address": "Beijing"
      }
    },
    {
      "id": 456,
      "name": "Bob",
      "detail": {
        "height": 1.8,
        "weight": 72.3,
        "address": "Shenzhen"
      }
    }
  ]
}
)";

  {
    auto msg1 = Message<>::decode(repr_str);
    ASSERT(msg1 && msg1->data.size() == 2, "");

    auto msg2 = Message<>::decode_by<proto::JsonCodec>(json_str);
    ASSERT(msg2 && msg2->data.size() == 2, "");

    ASSERT(!Message<>::decode(json_str), "");
    ASSERT(!Message<>::decode_by<proto::JsonCodec>(repr_str), "");
  }
}

TEST(proto, binary_codec) {
  auto repr_str = message.encode();
  auto json_str = message.encode_by<proto::JsonCodec>();
  auto bin_str = message.encode_by<proto::BinaryCodec>();

  ASSERT(repr_str && json_str && bin_str, "");

  std::cout << std::format("repr len={} Bytes, json len={} Bytes, bin len={} Bytes\n", repr_str->size(),
                           json_str->size(), bin_str->size());

  {
    ASSERT(!Message<>::decode(*json_str), "");
    ASSERT(!Message<>::decode(*bin_str), "");
  }
  {
    auto msg = Message<>::decode_by<proto::BinaryCodec>(*bin_str);
    ASSERT(msg && msg->data.size() == 2, "");
  }
}