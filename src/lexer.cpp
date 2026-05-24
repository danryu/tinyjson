#include <string_view>
#include <vector>

#include "lexer.hpp"
#include "macros/unwrap.hpp"
#include "macros/optional-return.hpp"
#include "string-reader/string-reader.hpp"

#include "util/charconv.hpp"

namespace json {
namespace {
struct Lexer {
    StringReader reader;
    bool         allow_comments = false;

    auto skip_comment() -> bool {
        ensure(reader.read()); // skip '/'
        unwrap(c, reader.read());
        if(c == '/') { // line comment
            ensure(reader.read_until('\n', '\r'));
        } else if(c == '*') { // block comment
            ensure(reader.read_until("*/"));
            ensure(reader.read(2)); // skip "*/"
        } else {
            bail("unknown comment type {}", c);
        }
        return true;
    }

    auto parse_string_token() -> std::optional<Token> {
        ensure(reader.read()); // skip '"'
        auto str = std::string();
        while(true) {
            unwrap(c, reader.read());
            if(c == '\\') {
                unwrap(d, reader.read());
                str.push_back(d);
                continue;
            }
            if(c == '"') {
                break;
            }
            str.push_back(c);
        }
        return Token::create<token::String>(std::move(str));
    }

    auto expect_string(const std::string_view expect) -> bool {
        const auto str_o = reader.read(expect.size());
        if(!str_o) return false;
        const auto& str = *str_o;
        return str == expect;
    }

    auto parse_boolean_token() -> std::optional<Token> {
        unwrap(next, reader.peek());
        if(next == 't') {
            return expect_string("true") ? std::optional(Token::create<token::Boolean>(true)) : std::nullopt;
        } else if(next == 'f') {
            return expect_string("false") ? std::optional(Token::create<token::Boolean>(false)) : std::nullopt;
        } else {
            return std::nullopt;
        }
    }

    auto parse_null_token() -> std::optional<Token> {
        return expect_string("null") ? std::optional(Token::create<token::Null>()) : std::nullopt;
    }

    auto parse_number_token() -> std::optional<Token> {
        auto len = 0;
        while(true) {
            unwrap(next, reader.peek());
            switch(next) {
            case '+':
            case '-':
            case '.':
            case 'e':
            case 'E':
            case 'x':
                goto ok;
            }
            if(next >= '0' && next <= '9') {
                goto ok;
            }
            break;
        ok:
            reader.read();
            len += 1;
            continue;
        }
        reader.cursor -= len;
        unwrap(buf, reader.read(len));
        unwrap(num, from_chars<double>(buf));
        return Token::create<token::Number>(num);
    }

    auto parse_next_token() -> std::optional<Token> {
        unwrap(next, reader.peek());
        switch(next) {
        case ' ':
        case '\n':
        case '\t':
            reader.read();
            return Token::create<token::WhiteSpace>();
        case '\r': {
            reader.read();
            unwrap(next, reader.peek());
            if(next == '\n') {
                reader.read();
                return Token::create<token::WhiteSpace>();
            }
        } break;
        case '{':
            reader.read();
            return Token::create<token::LeftBrace>();
        case '}':
            reader.read();
            return Token::create<token::RightBrace>();
        case '[':
            reader.read();
            return Token::create<token::LeftBracket>();
        case ']':
            reader.read();
            return Token::create<token::RightBracket>();
        case ',':
            reader.read();
            return Token::create<token::Comma>();
        case ':':
            reader.read();
            return Token::create<token::Colon>();
        case '"':
            return parse_string_token();
        case 't':
        case 'f':
            return parse_boolean_token();
        case 'n':
            return parse_null_token();
        case '+':
        case '-':
        case '.':
            return parse_number_token();
        }
        if(next >= '0' && next <= '9') {
            return parse_number_token();
        }
        bail("unexpected character: '{}'", next);
    }

    auto tokenize() -> std::optional<std::vector<Token>> {
        auto tokens = std::vector<Token>();
        while(!reader.is_eof()) {
            if(allow_comments && reader.peek() == '/') {
                ensure(skip_comment());
                continue;
            }
            unwrap_mut(token, parse_next_token());
            // ignore white space
            if(token.get<token::WhiteSpace>()) {
                continue;
            }
            tokens.push_back(std::move(token));
        }
        return tokens;
    }

    auto get_current_pos() const -> std::pair<int, int> {
        auto line  = 1;
        auto chara = 1;

        const auto limit = std::min(reader.cursor, reader.str.size());
        for(auto i = 0u; i < limit; i += 1) {
            if(reader.str[i] == '\n') {
                line += 1;
                chara = 1;
            } else {
                chara += 1;
            }
        }
        return {line, chara};
    }
};
} // namespace

auto tokenize(const std::string_view str, const bool allow_comments) -> std::optional<std::vector<Token>> {
    auto lexer = Lexer{
        .reader         = StringReader{str},
        .allow_comments = allow_comments,
    };
    auto ret_o = lexer.tokenize();
    if(!ret_o) {
        const auto [l, c] = lexer.get_current_pos();
        bail("lexer error at line {}, character {}", l, c);
    } else {
        return std::move(ret_o.value());
    }
}
} // namespace json
