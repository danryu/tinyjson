#include <vector>

#include "json.hpp"
#include "lexer.hpp"
#include "macros/optional-return.hpp"

namespace json {
namespace {
struct Parser {
    std::vector<Token> tokens;
    size_t             cursor                = 0;
    bool               allow_trailing_commas = false;

    template <class T>
    auto create_value() -> Value {
        return Value::create<T>();
    }

    template <class T, class... Args>
    auto create_value(Args... args) -> Value {
        return Value::create<T>(std::forward<Args...>(args...));
    }

    auto peek() -> const Token* {
        if(!(cursor < tokens.size())) return nullptr;
        return &tokens[cursor];
    }

    template <class T>
    auto peek_type() -> const T* {
        const auto next_o = peek();
        if(!next_o) return nullptr;
        const auto& next = *next_o;
        return next.get<T>();
    }

    auto read() -> const Token* {
        const auto next_o = peek();
        if(!next_o) return nullptr;
        const auto& next = *next_o;
        cursor += 1;
        return &next;
    }

    template <class T>
    auto read_type() -> const T* {
        const auto next_o = read();
        if(!next_o) return nullptr;
        const auto& next = *next_o;
        return next.get<T>();
    }

    auto parse_value() -> std::optional<Value> {
        unwrap(token, peek());
        switch(token.get_index()) {
        case Token::index_of<token::LeftBrace>:
            return parse_object();
        case Token::index_of<token::LeftBracket>:
            return parse_array();
        case Token::index_of<token::String>:
            read();
            return create_value<String>(token.as<token::String>().value);
        case Token::index_of<token::Number>:
            read();
            return create_value<Number>(token.as<token::Number>().value);
        case Token::index_of<token::Boolean>:
            read();
            return create_value<Boolean>(token.as<token::Boolean>().value);
        case Token::index_of<token::Null>:
            read();
            return create_value<Null>();
        default:
            return std::nullopt;
        }
    }

    auto parse_object() -> std::optional<Value> {
        ensure(read_type<token::LeftBrace>());
        auto children = std::vector<Object::KeyValue>();
        if(peek_type<token::RightBrace>()) {
            read();
            return create_value<Object>(std::move(children));
        }
    loop:
        unwrap(key, read_type<token::String>());
        ensure(read_type<token::Colon>());
        unwrap_mut(value, parse_value());
        children.emplace_back(key.value, std::move(value));

        unwrap(next, read());
        switch(next.get_index()) {
        case Token::index_of<token::RightBrace>:
            return create_value<Object>(std::move(children));
        case Token::index_of<token::Comma>:
            if(allow_trailing_commas && peek_type<token::RightBrace>()) {
                read();
                return create_value<Object>(std::move(children));
            } else {
                goto loop;
            }
        default:
            return std::nullopt;
        }
    }

    auto parse_array() -> std::optional<Value> {
        ensure(read_type<token::LeftBracket>());
        auto array = Array();
        if(peek_type<token::RightBracket>()) {
            read();
            return create_value<Array>(std::move(array));
        }
    loop:
        unwrap_mut(value, parse_value());
        array.value.push_back(std::move(value));

        unwrap(next, read());
        switch(next.get_index()) {
        case Token::index_of<token::RightBracket>:
            return create_value<Array>(std::move(array));
        case Token::index_of<token::Comma>:
            if(allow_trailing_commas && peek_type<token::RightBracket>()) {
                read();
                return create_value<Array>(std::move(array));
            } else {
                goto loop;
            }
        default:
            return std::nullopt;
        }
    }

    auto parse() -> std::optional<Object> {
        unwrap_mut(value, parse_object());
        return std::move(value.as<Object>());
    }

    auto get_error() -> std::string {
        return std::format("parser error at token {} of {}", cursor, tokens.size());
    }
};
} // namespace

auto parse(std::vector<Token> tokens, bool allow_trailing_commas) -> std::optional<Object> {
    auto parser = Parser{
        .tokens                = std::move(tokens),
        .allow_trailing_commas = allow_trailing_commas,
    };
    auto ret_o = parser.parse();
    if(!ret_o) {
        bail("{}", parser.get_error());
    } else {
        return std::move(ret_o.value());
    }
}

auto parse(const std::string_view str, ParseOpts opts) -> std::optional<Object> {
    unwrap_mut(token, tokenize(str, opts.allow_comments));
    unwrap_mut(object, parse(std::move(token), opts.allow_trailing_commas));
    return std::move(object);
}
} // namespace json
