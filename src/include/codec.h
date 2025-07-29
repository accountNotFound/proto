#pragma once

#include <format>

#include "proto.h"

namespace proto::_impl {

template <typename DerivedCodec>
struct PlaintextCodec : public BaseCodec {
  template <typename T>
    requires std::is_arithmetic_v<T>
  void encode(T& value) {
    _ss << value;
  }

  void encode(std::string& value) { _ss << '"' << value << '"'; }

  template <typename V>
  void encode(std::vector<V>& arr) {
    _ss << '[';
    for (size_t i = 0; auto& value : arr) {
      if (i > 0) {
        _ss << ',';
      }
      static_cast<DerivedCodec*>(this)->encode(value);
      ++i;
    }
    _ss << ']';
  }

  template <typename T>
    requires std::is_arithmetic_v<T>
  auto decode(T& value) -> std::expected<void, Error> {
    _remove_blanks();
    if (_ss >> value) {
      return {};
    }
    return std::unexpected(Error(std::format("invalid parse for {}", typeid(T).name())));
  }

  auto decode(std::string& value) -> std::expected<void, Error> {
    _remove_blanks();
    if (_ss.get() != '"') {
      return std::unexpected(Error(std::format("expect '\"' to start string")));
    }
    std::stringstream buffer;
    while (true) {
      char c = _ss.get();
      if (c == '"') {
        value = buffer.str();
        return {};
      }
      if (c == _ss.eof()) {
        return std::unexpected(Error(std::format("expect '\"' to end string")));
      }
      buffer.put(c);
    }
  }

  template <typename V>
  auto decode(std::vector<V>& array) -> std::expected<void, Error> {
    _remove_blanks();
    if (_ss.get() != '[') {
      return std::unexpected(Error(std::format("expect '[' to start vector<{}>", typeid(V).name())));
    }
    std::vector<V> arr;
    for (size_t i = 0; true; ++i) {
      _remove_blanks();
      char c = _ss.peek();
      if (c == ']') {
        break;
      }
      if (c == _ss.eof()) {
        return std::unexpected(Error(std::format("expect ']' to end vector<{}>", typeid(V).name())));
      }
      if (i > 0 && _ss.get() != ',') {
        return std::unexpected(Error(std::format("expect ',' to seperate vector<{}>", typeid(V).name())));
      }
      V val;
      if (auto status = static_cast<DerivedCodec*>(this)->decode(val); !status) {
        return status;
      }
      arr.emplace_back(std::move(val));
    }
    if (_ss.get() != ']') {
      return std::unexpected(Error(std::format("expect ']' to end vector<{}>", typeid(V).name())));
    }
    array = std::move(arr);
    return {};
  }

 protected:
  void _remove_blanks() {
    while (true) {
      char c = _ss.peek();
      if (c == ' ' || c == '\n' || c == '\t') {
        _ss.get();
      } else {
        return;
      }
    }
  }
};

}  // namespace proto::_impl

namespace proto {

class ReprCodec : public _impl::PlaintextCodec<ReprCodec> {
 public:
  using PlaintextCodec::decode;
  using PlaintextCodec::encode;

  template <template <typename> typename Model>
    requires std::is_base_of_v<BaseModel<Model<BaseCodec>>, Model<BaseCodec>>
  void encode(Model<BaseCodec>& model) {
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

  template <template <typename> typename Model>
    requires std::is_base_of_v<BaseModel<Model<BaseCodec>>, Model<BaseCodec>>
  auto decode(Model<BaseCodec>& model) -> std::expected<void, Error> {
    static Model<ReprCodec> _;
    _remove_blanks();
    if (_ss.get() != '(') {
      return std::unexpected(Error(std::format("expect '(' to start {}", typeid(Model<BaseCodec>).name())));
    }
    for (size_t i = 0; auto& field : Model<ReprCodec>::fields()) {
      _remove_blanks();
      char c = _ss.peek();
      if (c == _ss.eof()) {
        return std::unexpected(Error(std::format("expect ')' to end {}", typeid(Model<BaseCodec>).name())));
      }
      if (i > 0 && _ss.get() != ',') {
        return std::unexpected(Error(std::format("expect ',' to separate {}", typeid(Model<BaseCodec>).name())));
      }
      if (auto status = field->load(&model, *this); !status) {
        return status;
      }
      ++i;
    }
    _remove_blanks();
    if (_ss.get() != ')') {
      return std::unexpected(Error(std::format("expect ')' to end {}", typeid(Model<BaseCodec>).name())));
    }
    return {};
  }
};

class JsonCodec : public _impl::PlaintextCodec<JsonCodec> {
 public:
  using PlaintextCodec::decode;
  using PlaintextCodec::encode;

  template <template <typename> typename Model>
    requires std::is_base_of_v<BaseModel<Model<BaseCodec>>, Model<BaseCodec>>
  void encode(Model<BaseCodec>& model) {
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

  template <template <typename> typename Model>
    requires std::is_base_of_v<BaseModel<Model<BaseCodec>>, Model<BaseCodec>>
  auto decode(Model<BaseCodec>& model) -> std::expected<void, Error> {
    static Model<JsonCodec> _;
    _remove_blanks();
    if (_ss.get() != '{') {
      return std::unexpected(Error(std::format("expect '{{' to start {}", typeid(Model<BaseCodec>).name())));
    }
    for (size_t i = 0; auto& field : Model<JsonCodec>::fields()) {
      _remove_blanks();
      char c = _ss.peek();
      if (c == _ss.eof()) {
        return std::unexpected(Error(std::format("expect '}}' to end {}", typeid(Model<BaseCodec>).name())));
      }
      if (i > 0 && _ss.get() != ',') {
        return std::unexpected(Error(std::format("expect ',' to separate {}", typeid(Model<BaseCodec>).name())));
      }
      std::string name;
      decode(name);
      _remove_blanks();
      if (_ss.get() != ':') {
        return std::unexpected(Error(std::format("expect ':' to separate {}", typeid(Model<BaseCodec>).name())));
      }
      if (auto status = field->load(&model, *this); !status) {
        return status;
      }
      ++i;
    }
    _remove_blanks();
    if (_ss.get() != '}') {
      return std::unexpected(Error(std::format("expect '}}' to end {}", typeid(Model<BaseCodec>).name())));
    }
    return {};
  }
};

}  // namespace proto
