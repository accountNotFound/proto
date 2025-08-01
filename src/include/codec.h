#pragma once

#include <expected>
#include <format>
#include <functional>
#include <sstream>

namespace proto {

template <typename Derived>
class BaseModel;

template <typename Codec>
concept Codeable = requires(Codec c, std::string s) {
  { c.template encode(s) } -> std::convertible_to<std::expected<void, typename Codec::Error>>;
  { c.template decode(s) } -> std::convertible_to<std::expected<void, typename Codec::Error>>;
  { c.buffer() } -> std::convertible_to<std::stringstream&>;
};

namespace _impl {

class TextCodec {
 public:
  struct Error {
    std::string err_msg;
  };

  auto buffer() -> std::stringstream& { return _ss; }

  auto encode(const std::string& str) -> std::expected<void, Error>;

  auto decode(std::string& str) -> std::expected<void, Error>;

  template <typename T>
    requires std::is_arithmetic_v<T>
  auto encode(T num) -> std::expected<void, Error> {
    return (_put(num), std::expected<void, Error>{});
  }

  template <typename T>
    requires std::is_arithmetic_v<T>
  auto decode(T& num) -> std::expected<void, Error> {
    return _get(num) ? std::expected<void, Error>{}
                     : std::unexpected(Error(std::format("invalid bytes for {}", typeid(T).name())));
  }

 protected:
  std::stringstream _ss;

  template <typename T>
  void _put(const T& v) {
    _ss << v;
  }

  template <typename T>
  bool _get(T& v) {
    return (_ss >> v, _ss.good());
  }

  // skip blanks and see next valid character
  bool _see(char c);

  auto _catch(std::function<void()>&& stats) -> std::expected<void, Error>;

  void _try(std::expected<void, Error>&& ex, std::string&& msg);

  // skip blanks and try to eat next valid character
  void _try_eat(char c, std::string&& msg);

  void _drop_blanks();
};

template <typename Codec>
class ArrayTextCodec : public TextCodec {
 public:
  using TextCodec::decode;
  using TextCodec::encode;

  template <typename T>
  auto encode(const std::vector<T>& arr) -> std::expected<void, Error> {
    return _catch([this, &arr]() {
      _put('[');
      for (size_t i = 0; i < arr.size(); ++i) {
        i > 0 ? _put(',') : void();
        _try(static_cast<Codec*>(this)->encode(arr[i]), std::format("array[{}] encode", i));
      }
      _put(']');
    });
  }

  template <typename T>
  auto decode(std::vector<T>& arr) -> std::expected<void, Error> {
    return _catch([this, &arr]() mutable {
      _try_eat('[', "array start");
      arr.clear();
      for (size_t i = 0; !_see(']'); ++i) {
        !arr.empty() ? _try_eat(',', "array seperate") : void();
        T val;
        _try(static_cast<Codec*>(this)->decode(val), std::format("array[{}] decode", i));
        arr.emplace_back(std::move(val));
      }
      _try_eat(']', "array end");
    });
  }
};

class BytesCodec {
 public:
  using VariableLength = u_int32_t;

  struct Error {
    std::string err_msg;
  };

  auto buffer() -> std::stringstream& { return _ss; }

  auto encode(const std::string& value) -> std::expected<void, Error>;

  auto decode(std::string& value) -> std::expected<void, Error>;

  template <typename T>
    requires std::is_arithmetic_v<T>
  auto encode(T num) -> std::expected<void, Error> {
    std::endian::native == std::endian::little ? _reverse_byte_order(reinterpret_cast<char*>(&num), sizeof(T)) : void();
    _ss.write(reinterpret_cast<char*>(&num), sizeof(T));
    return {};
  }

  template <typename T>
    requires std::is_arithmetic_v<T>
  auto decode(T& num) -> std::expected<void, Error> {
    _ss.read(reinterpret_cast<char*>(&num), sizeof(T));
    if (_ss.gcount() != sizeof(T)) {
      return std::unexpected(Error(std::format("invalid bytes for {}", typeid(T).name())));
    }
    std::endian::native == std::endian::little ? _reverse_byte_order(reinterpret_cast<char*>(&num), sizeof(T)) : void();
    return {};
  }

 protected:
  std::stringstream _ss;

  auto _catch(std::function<void()>&& stats) -> std::expected<void, Error>;

  template <typename T>
  T _try(std::expected<T, Error>&& ex, std::string&& msg) {
    if (!ex) {
      throw Error(std::move(msg));
    }
    if constexpr (!std::is_same_v<T, void>) {
      return std::move(ex.value());
    }
  }

  template <typename T>
  auto _encode_varible_len(T len) -> std::expected<VariableLength, Error> {
    VariableLength u32 = len;
    if (u32 < len) {
      return std::unexpected(Error("variable length object (string and array) only support a maximum 4G elements"));
    }
    _ss.write(&_variable_length_tag, 1);
    encode(u32);  // always success
    return u32;
  }

  auto _decode_variable_len() -> std::expected<VariableLength, Error>;

 private:
  inline static constexpr char _variable_length_tag = 0xf1;

  void _reverse_byte_order(char* start, size_t size);
};

}  // namespace _impl

/**
 * @brief A repr text codec. Suitable for shallow nesting object
 *
 * @note Decode destination object may come into invalid status if decode failed
 */
class ReprCodec : public _impl::ArrayTextCodec<ReprCodec> {
 public:
  using ArrayTextCodec<ReprCodec>::decode;
  using ArrayTextCodec<ReprCodec>::encode;

  template <template <typename> typename Model, typename Codec>
    requires std::is_base_of_v<BaseModel<Model<Codec>>, Model<Codec>>
  auto encode(const Model<Codec>& model) -> std::expected<void, Error> {
    return _catch([this, &model]() {
      auto& fields = Model<ReprCodec>::fields();
      _put('(');
      for (size_t i = 0; i < fields.size(); ++i) {
        i > 0 ? _put(',') : void();
        _try(fields[i]->dump(&model, *this),
             std::format("{}::{} encode", typeid(Model<Codec>).name(), fields[i]->name()));
      }
      _put(')');
    });
  }

  template <template <typename> typename Model, typename Codec>
    requires std::is_base_of_v<BaseModel<Model<Codec>>, Model<Codec>>
  auto decode(Model<Codec>& model) -> std::expected<void, Error> {
    return _catch([this, &model]() mutable {
      auto& fields = Model<ReprCodec>::fields();
      _try_eat('(', "model start");
      for (size_t i = 0; i < fields.size(); ++i) {
        i > 0 ? _try_eat(',', "model seperate") : void();
        _try(fields[i]->load(&model, *this),
             std::format("{}::{} decode", typeid(Model<Codec>).name(), fields[i]->name()));
      }
      _try_eat(')', "model end");
    });
  }
};

class JsonCodec : public _impl::ArrayTextCodec<JsonCodec> {
 public:
  using ArrayTextCodec<JsonCodec>::decode;
  using ArrayTextCodec<JsonCodec>::encode;

  template <template <typename> typename Model, typename Codec>
    requires std::is_base_of_v<BaseModel<Model<Codec>>, Model<Codec>>
  auto encode(const Model<Codec>& model) -> std::expected<void, Error> {
    return _catch([this, &model]() {
      auto& fields = Model<JsonCodec>::fields();
      _put('{');
      for (size_t i = 0; i < fields.size(); ++i) {
        i > 0 ? _put(',') : void();
        _try(encode(fields[i]->name()),
             std::format("{}::{} encode key", typeid(Model<Codec>).name(), fields[i]->name()));
        _put(':');
        _try(fields[i]->dump(&model, *this),
             std::format("{}::{} encode value", typeid(Model<Codec>).name(), fields[i]->name()));
      }
      _put('}');
    });
  }

  template <template <typename> typename Model, typename Codec>
    requires std::is_base_of_v<BaseModel<Model<Codec>>, Model<Codec>>
  auto decode(Model<Codec>& model) -> std::expected<void, Error> {
    return _catch([this, &model]() mutable {
      auto& fields = Model<JsonCodec>::fields();
      _try_eat('{', "model start");
      for (size_t i = 0; i < fields.size(); ++i) {
        i > 0 ? _try_eat(',', "model separate") : void();
        std::string name;
        _try(decode(name), std::format("{}::{} decode key", typeid(Model<Codec>).name(), fields[i]->name()));
        _try_eat(':', "model colon");
        _try(fields[i]->load(&model, *this),
             std::format("{}::{} decode value", typeid(Model<Codec>).name(), fields[i]->name()));
      }
      _try_eat('}', "model end");
    });
  }
};

/**
 * @brief A binary codec. Suitable for large numeric object
 *
 * @note Decode destination object may come into invalid status if decode failed
 */
class BinaryCodec : public _impl::BytesCodec {
 public:
  using BytesCodec::decode;
  using BytesCodec::encode;

  template <typename T>
  auto encode(const std::vector<T>& arr) -> std::expected<void, Error> {
    return _catch([this, &arr]() {
      VariableLength len = _try(_encode_varible_len(arr.size()), "array encode");
      for (VariableLength i = 0; i < len; ++i) {
        _try(encode(arr[i]), std::format("array[{}] encode", i));
      }
    });
  }

  template <typename T>
  auto decode(std::vector<T>& arr) -> std::expected<void, Error> {
    return _catch([this, &arr]() mutable {
      VariableLength len = _try(_decode_variable_len(), "array decode");
      arr.clear();
      for (VariableLength i = 0; i < len; ++i) {
        T val;
        _try(decode(val), std::format("array[{}] decode", i));
        arr.emplace_back(std::move(val));
      }
    });
  }

  template <template <typename> typename Model, typename Codec>
    requires std::is_base_of_v<BaseModel<Model<Codec>>, Model<Codec>>
  auto encode(const Model<Codec>& model) -> std::expected<void, Error> {
    return _catch([this, &model]() {
      for (auto& field : Model<BinaryCodec>::fields()) {
        _try(field->dump(&model, *this), std::format("{}::{} encode", typeid(Model<Codec>).name(), field->name()));
      }
    });
  }

  template <template <typename> typename Model, typename Codec>
    requires std::is_base_of_v<BaseModel<Model<Codec>>, Model<Codec>>
  auto decode(Model<Codec>& model) -> std::expected<void, Error> {
    return _catch([this, &model]() mutable {
      for (auto& field : Model<BinaryCodec>::fields()) {
        _try(field->load(&model, *this), std::format("{}::{} decode", typeid(Model<Codec>).name(), field->name()));
      }
    });
  }
};

}  // namespace proto