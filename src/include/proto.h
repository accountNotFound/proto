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
  void encode(const T& data) {}

  template <typename T>
  auto decode() -> std::expected<T, Error> {
    return {};
  }

 protected:
  std::stringstream _ss;
};

template <typename Codec>
struct BaseField {
  virtual const char* name() const = 0;

  virtual void dump(void* model, Codec& codec) const = 0;

  virtual auto load(void* model, Codec& codec) const -> std::optional<typename Codec::Error> = 0;
};

template <typename Model>
class BaseModel;

template <template <typename> typename Model, typename Codec>
  requires std::is_base_of_v<BaseCodec, Codec>
class BaseModel<Model<Codec>> {
 public:
  template <typename T>
  class TypeField : public BaseField<Codec> {
   public:
    TypeField(const char* name, T Model<Codec>::* offset) : _name(name), _offset(offset) {};

    const char* name() const { return _name; }

    void dump(void* model, Codec& codec) const override { codec.encode(static_cast<Model<Codec>*>(model)->*_offset); }

    auto load(void* model, Codec& codec) const -> std::optional<typename Codec::Error> override {
      std::expected<T, typename Codec::Error> v = codec.template decode<T>();
      if (v) {
        static_cast<Model<Codec>*>(model)->*_offset = std::move(*v);
        return {};
      }
      return v.error();
    }

   private:
    const char* _name;
    T Model<Codec>::* _offset;
  };

  static auto fields() -> const std::vector<std::unique_ptr<BaseField<Codec>>>& { return BaseModel::_fields; }

 protected:
  template <typename T>
  static void _regist(const char* name, T Model<Codec>::* offset) {
    BaseModel::_fields.emplace_back(std::make_unique<TypeField<T>>(name, offset));
  }

 private:
  inline static std::vector<std::unique_ptr<BaseField<Codec>>> _fields;
};

}  // namespace proto

#define FIELD(type, name, ...) \
 public:                       \
  type name = __VA_ARGS__;     \
                               \
 private:                      \
  inline static auto _dumpy_##name = (Model::template _regist<type>(#name, &Model::name), 0);
