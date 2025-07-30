#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

namespace proto {

struct BaseCodec {
  struct Error {
    std::string err_msg;
  };

  auto buffer() -> std::stringstream& { return _ss; }

  auto buffer() const -> const std::stringstream& { return _ss; }

  template <typename T>
  auto encode(const T& data) -> std::expected<void, Error> {
    return {};
  }

  template <typename T>
  auto decode(T& data) -> std::expected<void, Error> {
    return {};
  }

 protected:
  std::stringstream _ss;
};

template <typename Model>
class BaseModel;

template <template <typename> typename Model, typename Codec>
  requires std::is_base_of_v<BaseCodec, Codec>
class BaseModel<Model<Codec>> {
 public:
  struct BaseField {
    virtual const char* name() const = 0;

    virtual auto dump(const void* model, Codec& codec) const -> std::expected<void, typename Codec::Error> = 0;

    virtual auto load(void* model, Codec& codec) const -> std::expected<void, typename Codec::Error> = 0;
  };

  template <typename T>
  class TypeField : public BaseField {
   public:
    TypeField(const char* name, T Model<Codec>::* offset) : _name(name), _offset(offset) {};

    const char* name() const { return _name; }

    auto dump(const void* model, Codec& codec) const -> std::expected<void, typename Codec::Error> override {
      return codec.encode(static_cast<const Model<Codec>*>(model)->*_offset);
    }

    auto load(void* model, Codec& codec) const -> std::expected<void, typename Codec::Error> override {
      return codec.decode(static_cast<Model<Codec>*>(model)->*_offset);
    }

   private:
    const char* _name;
    T Model<Codec>::* _offset;
  };

  static auto fields() -> const std::vector<std::unique_ptr<BaseField>>& { return BaseModel::_fields; }

 protected:
  template <typename T>
  static void _regist(const char* name, T Model<Codec>::* offset) {
    BaseModel::_fields.emplace_back(std::make_unique<TypeField<T>>(name, offset));
  }

 private:
  inline static std::vector<std::unique_ptr<BaseField>> _fields;
};

}  // namespace proto

#define FIELD(type, name, ...) \
 public:                       \
  type name = __VA_ARGS__;     \
                               \
 private:                      \
  inline static auto _dumpy_##name = (Model::template _regist<type>(#name, &Model::name), 0);
