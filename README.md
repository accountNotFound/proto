# brief

A simple struct serialize & deserialize library based on C++23

### Usage

- model definition
```c++
template <typename C = proto::ReprCodec> // specify default codec class
struct User : public proto::BaseModel<User<C>> {
  using Model = User;

  // declare a field which can be serialized and deserialized
  uint32_t id = Model::template codable_field<&Model::id>("id", 0);

  // or use this macro, which simplily expand to the `id` definition above
  // PROTO_FIELD(uint32_t, id, 0);

  PROTO_FIELD(std::string, name, "unkown");
  PROTO_FIELD(UserDetail<>, detail, {});

  // a simple field can not be serialized or deserialized
  double weight = 70.5;
};

template <typename C = proto::ReprCodec>
struct UserResponse : public proto::BaseModel<UserResponse<C>> {
  using Model = UserResponse;

  PROTO_FIELD(User<>, user, {});
  PROTO_FIELD(std::vector<User<>>, followers, {});
};

template <typename C = proto::ReprCodec>
struct Message : public proto::BaseModel<Message<C>> {
  using Model = Message;

  PROTO_FIELD(uint32_t, code, 0);
  PROTO_FIELD(std::string, msg, "");
  PROTO_FIELD(UserResponse<>, data, {});
};
```

- serialize & deserialize
```c++
{
  auto repr_str = R"(
(0,"",(
  (123,"Alice"),[
    (456,"Bob"),(789,"Cathy")]))
)";

  // deserialize from repr string
  std::expected<Message<>, proto::ReprCodec::Error> msg = Message<>::decode(repr_str);

  // serialize to json string
  std::expected<std::string, proto::_impl::TextCodec::Error> json_str = msg->encode_by<proto::JsonCodec>();

  std::cout << json_str << '\n';
  // output: 
  // {"code":0,"msg":"","data":{"user":{"id":123,"name":"Alice"},"followers":[{"id":456,"name":"Bob"},{"id":789,"name":"Cathy"}]}}

  // serialize & deserialize to binary byte sequence
  auto binary_str = msg->encode_by<proto::BinaryCodec>();
  auto msg_from_bytes = Message<>::decode_by<proto::BinaryCodec>(binary_str);
}
```