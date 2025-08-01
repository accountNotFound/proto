#include "codec.h"

namespace proto::_impl {

auto TextCodec::encode(const std::string& str) -> std::expected<void, Error> {
  _ss << '"' << str << '"';
  return {};
}

auto TextCodec::decode(std::string& str) -> std::expected<void, Error> {
  try {
    _try_eat('"', "string start");
    std::stringstream buffer;
    while (!_see('"')) {
      char c;
      _get(c);
      buffer << c;
    }
    _try_eat('"', "string end");
    str = buffer.str();
  } catch (Error e) {
    return std::unexpected(std::move(e));
  }
  return {};
}

bool TextCodec::_see(char c) {
  _drop_blanks();
  return _ss.peek() == c;
}

auto TextCodec::_catch(std::function<void()>&& func) -> std::expected<void, Error> {
  try {
    func();
  } catch (Error e) {
    return std::unexpected(std::move(e));
  }
  return {};
}

void TextCodec::_try(std::expected<void, Error>&& ex, std::string&& msg) {
  if (!ex) {
    throw Error(msg + ": " + std::move(ex.error().err_msg));
  }
}

void TextCodec::_try_eat(char c, std::string&& msg) {
  _drop_blanks();
  if (_ss.peek() != c) {
    throw Error(std::format("expect char '{}'", c));
  }
  _ss >> c;
}

void TextCodec::_drop_blanks() {
  while (!_ss.eof()) {
    char c = _ss.peek();
    if (c != ' ' && c != '\n' && c != '\t') {
      return;
    }
    _ss.get();
  }
}

auto BytesCodec::encode(const std::string& value) -> std::expected<void, Error> {
  auto len = _encode_varible_len(value.size());
  if (!len) {
    return std::unexpected(len.error());
  }
  _ss.write(value.data(), *len);
  return {};
}

auto BytesCodec::decode(std::string& value) -> std::expected<void, Error> {
  char c;
  if (_ss.read(&c, 1); c != _variable_length_tag) {
    return std::unexpected(Error("string start: expect variable length tag"));
  }
  VariableLength len;
  if (auto r = decode(len); !r) {
    return r;
  }
  value.resize(len);
  _ss.read(value.data(), len);
  if (_ss.gcount() != len) {
    return std::unexpected(Error("string parse: insufficent bytes for string"));
  }
  return {};
}

auto BytesCodec::_catch(std::function<void()>&& func) -> std::expected<void, Error> {
  try {
    func();
  } catch (Error e) {
    return std::unexpected(std::move(e));
  }
  return {};
}

auto BytesCodec::_decode_variable_len() -> std::expected<VariableLength, Error> {
  if (_ss.eof()) {
    return std::unexpected(Error("no sufficient bytes"));
  }
  char c;
  _ss >> c;
  if (c != _variable_length_tag) {
    return std::unexpected(Error("expect variable length tag"));
  }
  VariableLength len;
  if (auto r = decode(len); !r) {
    return std::unexpected(Error(std::format("decode variable length {}", std::move(r.error().err_msg))));
  }
  return len;
}

void BytesCodec::_reverse_byte_order(char* start, size_t size) {
  char* end = start + size - 1;
  while (start < end) {
    std::swap(*start, *end);
    ++start;
    --end;
  }
}

}  // namespace proto::_impl
