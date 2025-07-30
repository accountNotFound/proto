#pragma once

#include <format>

#include "proto.h"

namespace proto::_impl {

template <typename DerivedCodec>
struct PlaintextCodec : public BaseCodec {
  template <typename T>
    requires std::is_arithmetic_v<T>
  auto encode(T value) -> std::expected<void, Error> {
    _ss << value;
    return {};
  }

  auto encode(const std::string& value) -> std::expected<void, Error> {
    _ss << '"' << value << '"';
    return {};
  }

  template <typename V>
  auto encode(const std::vector<V>& arr) -> std::expected<void, Error> {
    _ss << '[';
    for (size_t i = 0; auto& v : arr) {
      if (i > 0) {
        _ss << ',';
      }
      if (auto r = static_cast<DerivedCodec*>(this)->encode(v); !r) {
        return r;
      }
      ++i;
    }
    _ss << ']';
    return {};
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
      if (auto r = static_cast<DerivedCodec*>(this)->decode(val); !r) {
        return r;
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

/**
 * @note
 *
 * 1. Use fixed size number (.e.g `std::int16_t`, `std::uint32_t`) instead of `int` or other things to garuantee the
 * correctness.
 *
 * 2. Max supported length of string is `4GB`, or it will be truncated
 *
 */
template <typename DerivedCodec>
struct BinaryCodec : public BaseCodec {
  struct TypeTag {
    inline static const char Number = 0xf1;
    inline static const char String = 0xf2;
    inline static const char Array = 0xf3;
    inline static const char Model = 0xf4;
  };

  BinaryCodec() {
    uint16_t test = 0x0001;
    _is_little_endian = (*reinterpret_cast<char*>(&test) == 0x01);
  }

  template <typename T>
    requires std::is_arithmetic_v<T>
  auto encode(T value) -> std::expected<void, Error> {
    _ss.write(&TypeTag::Number, 1);
    if (_is_little_endian) {
      // to big endian order
      char* start = reinterpret_cast<char*>(&value);
      char* end = start + sizeof(T) - 1;
      while (start < end) {
        std::swap(*start, *end);
        ++start;
        --end;
      }
    }
    _ss.write(reinterpret_cast<char*>(&value), sizeof(T));
    return {};
  }

  auto encode(const std::string& value) -> std::expected<void, Error> {
    _ss.write(&TypeTag::String, 1);
    uint32_t len = value.size();
    if (len != value.size()) {
      return std::unexpected(Error("string length should be <= 4G bytes"));
    }
    encode(len);
    _ss.write(value.data(), len);
    return {};
  }

  template <typename V>
  auto encode(const std::vector<V>& arr) -> std::expected<void, Error> {
    _ss.write(&TypeTag::Array, 1);
    uint32_t len = arr.size();
    if (len != arr.size()) {
      return std::unexpected(Error(std::format("vector<{}> length should be <= 4G", typeid(V).name())));
    }
    encode(len);
    for (auto& v : arr) {
      if (auto r = static_cast<DerivedCodec*>(this)->encode(v); !r) {
        return r;
      }
    }
    return {};
  }

  template <typename T>
    requires std::is_arithmetic_v<T>
  auto decode(T& value) -> std::expected<void, Error> {
    char tag = 0;
    _ss.read(&tag, 1);
    if (_ss.gcount() == 0 || tag != TypeTag::Number) {
      return std::unexpected(Error(std::format("invalid tag while parse {}", typeid(T).name())));
    }
    _ss.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (_ss.gcount() != sizeof(T)) {
      return std::unexpected(Error(std::format("insufficent bytes for {}", typeid(T).name())));
    }
    if (_is_little_endian) {
      char* start = reinterpret_cast<char*>(&value);
      char* end = start + sizeof(T) - 1;
      while (start < end) {
        std::swap(*start, *end);
        ++start;
        --end;
      }
    }
    return {};
  }

  auto decode(std::string& value) -> std::expected<void, Error> {
    char tag = 0;
    _ss.read(&tag, 1);
    if (_ss.gcount() == 0 || tag != TypeTag::String) {
      return std::unexpected(Error("invalid tag while parse string"));
    }
    uint32_t len;
    if (auto r = decode(len); !r) {
      return r;
    }
    value.resize(len);
    _ss.read(value.data(), len);
    if (_ss.gcount() != len) {
      return std::unexpected(Error(std::format("insufficent bytes for string")));
    }
    return {};
  }

  template <typename V>
  auto decode(std::vector<V>& arr) -> std::expected<void, Error> {
    char tag = 0;
    _ss.read(&tag, 1);
    if (_ss.gcount() == 0 || tag != TypeTag::Array) {
      return std::unexpected(Error(std::format("invalid tag while parse vector<{}>", typeid(V).name())));
    }
    uint32_t len;
    if (auto r = decode(len); !r) {
      return r;
    }
    for (uint32_t i = 0; i < len; ++i) {
      V v;
      if (auto r = static_cast<DerivedCodec*>(this)->decode(v); !r) {
        return r;
      }
      arr.emplace_back(std::move(v));
    }
    return {};
  }

 private:
  bool _is_little_endian;
};

}  // namespace proto::_impl

namespace proto {

/**
 * @note Decode destination object may come into invalid status if decode failed
 */
class ReprCodec : public _impl::PlaintextCodec<ReprCodec> {
 public:
  using PlaintextCodec::decode;
  using PlaintextCodec::encode;

  template <template <typename> typename Model>
    requires std::is_base_of_v<BaseModel<Model<BaseCodec>>, Model<BaseCodec>>
  auto encode(const Model<BaseCodec>& model) -> std::expected<void, Error> {
    static Model<ReprCodec> _;
    _ss << '(';
    for (size_t i = 0; auto& field : Model<ReprCodec>::fields()) {
      if (i > 0) {
        _ss << ',';
      }
      if (auto r = field->dump(&model, *this); !r) {
        return r;
      }
      ++i;
    }
    _ss << ')';
    return {};
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
      if (auto r = field->load(&model, *this); !r) {
        return r;
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

/**
 * @note Decode destination object may come into invalid status if decode failed
 */
class JsonCodec : public _impl::PlaintextCodec<JsonCodec> {
 public:
  using PlaintextCodec::decode;
  using PlaintextCodec::encode;

  template <template <typename> typename Model>
    requires std::is_base_of_v<BaseModel<Model<BaseCodec>>, Model<BaseCodec>>
  auto encode(const Model<BaseCodec>& model) -> std::expected<void, Error> {
    static Model<JsonCodec> _;
    _ss << '{';
    for (size_t i = 0; auto& field : Model<JsonCodec>::fields()) {
      if (i > 0) {
        _ss << ',';
      }
      _ss << '"' << field->name() << '"' << ':';
      if (auto r = field->dump(&model, *this); !r) {
        return r;
      }
      ++i;
    }
    _ss << '}';
    return {};
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
      if (auto r = field->load(&model, *this); !r) {
        return r;
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

/**
 * @brief A binary codec
 *
 * @note Decode destination object may come into invalid status if decode failed
 */
class FastCodec : public _impl::BinaryCodec<FastCodec> {
 public:
  using BinaryCodec::decode;
  using BinaryCodec::encode;

  template <template <typename> typename Model>
    requires std::is_base_of_v<BaseModel<Model<BaseCodec>>, Model<BaseCodec>>
  auto encode(const Model<BaseCodec>& model) -> std::expected<void, Error> {
    static Model<FastCodec> _;
    _ss.write(&TypeTag::Model, 1);
    for (size_t i = 0; auto& field : Model<FastCodec>::fields()) {
      if (auto r = field->dump(&model, *this); !r) {
        return r;
      }
      ++i;
    }
    return {};
  }

  template <template <typename> typename Model>
    requires std::is_base_of_v<BaseModel<Model<BaseCodec>>, Model<BaseCodec>>
  auto decode(Model<BaseCodec>& model) -> std::expected<void, Error> {
    static Model<FastCodec> _;
    char tag = 0;
    _ss.read(&tag, 1);
    if (_ss.gcount() == 0 || tag != TypeTag::Model) {
      return std::unexpected(Error(std::format("invalid tag while parse {}", typeid(Model<BaseCodec>).name())));
    }
    for (auto& field : Model<FastCodec>::fields()) {
      if (auto r = field->load(&model, *this); !r) {
        return r;
      }
    }
    return {};
  }
};

}  // namespace proto
