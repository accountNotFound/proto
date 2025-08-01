#pragma once

#include <memory>
#include <vector>

#include "codec.h"

namespace proto {

namespace _impl {

template <typename T>
struct MemberTrait {};

template <typename T, typename Model>
struct MemberTrait<T Model::*> {
  using Field = T;
  using Class = Model;
};

}  // namespace _impl

template <typename Model>
class BaseModel;

/**
 * @param Codec Default codec for this model class
 */
template <template <typename> typename Model, Codeable Codec>
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

  constexpr static auto fields() -> const std::vector<std::unique_ptr<BaseField>>& {
    static Model<Codec> _;
    return Model<Codec>::_fields;
  }

  /**
   * @brief Regist the field with given default value
   */
  template <auto offset>
  constexpr static auto codable_field(const char* name, typename _impl::MemberTrait<decltype(offset)>::Field value) {
    static auto _ = (_regist<decltype(value)>(name, offset), 0);
    return value;
  }

  /**
   * @param data Input string is expected to be valid `Codec` format
   */
  static auto decode(const std::string& data) -> std::expected<Model<Codec>, typename Codec::Error> {
    return decode_by<Codec>(data);
  }

  template <Codeable CustomCodec>
  static auto decode_by(const std::string& data) -> std::expected<Model<Codec>, typename CustomCodec::Error> {
    Model<Codec> model;
    CustomCodec codec;
    codec.buffer() << data;
    if (auto r = codec.decode(model); r) {
      return model;
    } else {
      return std::unexpected(r.error());
    }
  }

  /**
   * @return A valid `codec` format string, if success
   */
  auto encode() const -> std::expected<std::string, typename Codec::Error> { return encode_by<Codec>(); }

  template <Codeable CustomCodec>
  auto encode_by() const -> std::expected<std::string, typename CustomCodec::Error> {
    CustomCodec codec;
    if (auto r = codec.encode(*static_cast<const Model<Codec>*>(this)); r) {
      return codec.buffer().str();
    } else {
      return std::unexpected(r.error());
    }
  }

 protected:
  template <typename T>
  static void _regist(const char* name, T Model<Codec>::* offset) {
    BaseModel::_fields.emplace_back(std::make_unique<TypeField<T>>(name, offset));
  }

 private:
  inline static std::vector<std::unique_ptr<BaseField>> _fields;
};

}  // namespace proto

// #define PROTO_FIELD(type, name, ...) \
//  public:                             \
//   type name = __VA_ARGS__;           \
//                                      \
//  private:                            \
//   inline static auto _dumpy_##name = (Model::template _regist<type>(#name, &Model::name), 0);

#define PROTO_FIELD(type, name, ...) type name = Model::template codable_field<&Model::name>(#name, __VA_ARGS__);
