#pragma once

#include "proto.h"

namespace proto::_impl {

struct ValueLiteralCodec : public BaseCodec {
  template <typename T>
    requires std::is_arithmetic_v<T>
  void encode(T& value) {
    _ss << value;
  }

  void encode(std::string& value) { _ss << '"' << value << '"'; }

  template <typename T>
  auto decode() -> std::expected<T, Error> {
    // TODO
    return {};
  }
};

}  // namespace proto::_impl

namespace proto {

class ReprCodec : public _impl::ValueLiteralCodec {
 public:
  using ValueLiteralCodec::decode;
  using ValueLiteralCodec::encode;

  template <template <typename> typename Model, typename Codec>
    requires std::is_base_of_v<BaseModel<Model<Codec>>, Model<Codec>>
  void encode(Model<Codec>& model) {
    static Model<ReprCodec> _;
    _ss << '(';
    for (size_t i = 0; auto& field : Model<ReprCodec>::fields()) {
      if (i > 0) {
        _ss << ',';
      }
      field->dump(&model, *this);
      ++i;
    }
    _ss << ')';
  }
};

class JsonCodec : public _impl::ValueLiteralCodec {
 public:
  using ValueLiteralCodec::decode;
  using ValueLiteralCodec::encode;

  template <template <typename> typename Model, typename Codec>
    requires std::is_base_of_v<BaseModel<Model<Codec>>, Model<Codec>>
  void encode(Model<Codec>& model) {
    static Model<JsonCodec> _;
    _ss << '{';
    for (size_t i = 0; auto& field : Model<JsonCodec>::fields()) {
      if (i > 0) {
        _ss << ',';
      }
      _ss << '"' << field->name() << '"' << ':';
      field->dump(&model, *this);
      ++i;
    }
    _ss << '}';
  }
};

}  // namespace proto
