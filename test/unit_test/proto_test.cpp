#include "proto.h"

#include <iostream>
#include <vector>

#include "codec.h"
#include "mtest.h"

template <typename T = proto::BaseCodec>
struct UserDetail : public proto::BaseModel<UserDetail<T>> {
  using Model = UserDetail;

  FIELD(float, height, -1);
  FIELD(double, weight, -1);
  FIELD(std::string, address, "unknown");
};

template <typename T = proto::BaseCodec>
struct User : public proto::BaseModel<User<T>> {
  using Model = User;

  FIELD(int, id, -1);
  FIELD(std::string, name, "unkown");
  FIELD(UserDetail<>, detail, {});
};

template <typename T = proto::BaseCodec>
struct Message : public proto::BaseModel<Message<T>> {
  using Model = Message;

  FIELD(int, code, 0);
  FIELD(std::string, msg, "");
  FIELD(std::vector<User<>>, data, {});
};

TEST(proto, plaintext_codec) {
  proto::ReprCodec repr;
  proto::JsonCodec json;
  {
    User<> u1 = {.id = 123, .name = "Alice", .detail = {.height = 1.6, .weight = 50.5, .address = "Beijing"}};
    User<> u2 = {.id = 456, .name = "Bob", .detail = {.height = 1.8, .weight = 72.3, .address = "Shenzhen"}};
    Message<> msg = {.data = {u1, u2}};

    repr.encode(msg);
    std::cout << repr.buffer().str() << '\n';

    json.encode(msg);
    std::cout << json.buffer().str() << '\n';
  }
  {
    Message<> msg2;
    ASSERT(repr.decode(msg2), "");
    ASSERT(msg2.data.size() == 2, "");

    ASSERT(json.decode(msg2), "");
    ASSERT(msg2.data.size() == 2, "");

    json.buffer().clear();
    json.buffer() << repr.buffer().str();
    ASSERT(!json.decode(msg2), "");
  }
}

TEST(proto, plaintext_decode) {
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
    proto::ReprCodec repr;
    repr.buffer() << repr_str;
    Message<> msg;
    ASSERT(repr.decode(msg), "");
    ASSERT(msg.data.size() == 2, "");

    repr.buffer().clear();
    repr.buffer() << json_str;
    ASSERT(!repr.decode(msg), "");
  }

  {
    proto::JsonCodec json;
    json.buffer() << json_str;
    Message<> msg;
    ASSERT(json.decode(msg), "");
    ASSERT(msg.data.size() == 2, "");

    json.buffer().clear();
    json.buffer() << repr_str;
    ASSERT(!json.decode(msg), "");
  }
}
