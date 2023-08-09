#include "conf.hpp"
#include "io.hpp"
#include "string.hpp"

namespace fs = std::filesystem;
using namespace std::literals::string_literals;

namespace conf {

namespace {

struct Token {
    enum Type {
        Ident, Int, Float, True, False, String, EqualSign, Newline,
        LeftSquare, RightSquare, Comma,
        Unterminated, InvalidChar, End,
    } type;
    std::string_view text;
    std::size_t pos;
};

bool is_ident_char(char c) { return string::is_alpha(c) || c == '_' || c == '-'; }

struct Lexer {
    std::string_view text;
    std::size_t cur = 0, start = 0;

    explicit Lexer(std::string_view s) : text{s} {}

    char peek() const   { return text[cur]; }
    char advance()      { return text[cur++]; }
    bool at_end() const { return text.size() == cur; }

    auto position_of(Token t) const
    {
        auto tmp = text.substr(0, t.pos);
        auto line = std::count(tmp.begin(), tmp.end(), '\n') + 1;
        auto column = t.pos - tmp.find_last_of('\n');
        return std::make_pair(line, column);
    }

    Token make(Token::Type type)
    {
        return Token {
            .type = type,
            .text = text.substr(start, cur - start),
            .pos  = start,
        };
    }

    void skip()
    {
        for (;;) {
            switch (peek()) {
            case ' ': case '\r': case '\t': cur++; break;
            case '#':
                while (peek() != '\n')
                    advance();
                break;
            default:
                return;
            }
        }
    }

    Token number()
    {
        while (string::is_digit(text[cur]))
            advance();
        if (peek() != '.')
            return make(Token::Int);
        advance();
        while (string::is_digit(text[cur]))
            advance();
        return make(Token::Float);
    }

    Token ident()
    {
        while (is_ident_char(peek()) || string::is_digit(peek()))
            advance();
        auto word = text.substr(start, cur - start);
        return make(word == "true"  ? Token::True
                  : word == "false" ? Token::False
                  :                   Token::Ident);
    }

    Token string_token()
    {
        while (peek() != '"' && !at_end())
            advance();
        if (at_end())
            return make(Token::Unterminated);
        advance();
        return make(Token::String);
    }

    Token lex()
    {
        skip();
        start = cur;
        if (at_end())
            return make(Token::End);
        char c = advance();
        return c == '='    ? make(Token::EqualSign)
             : c == '\n'   ? make(Token::Newline)
             : c == '['    ? make(Token::LeftSquare)
             : c == ']'    ? make(Token::RightSquare)
             : c == ','    ? make(Token::Comma)
             : c == '"'    ? string_token()
             : string::is_digit(c) ? number()
             : is_ident_char(c) ? ident()
             : make(Token::InvalidChar);
    }
};

struct Parser {
    Lexer lexer;
    const Data &defaults;
    Token cur, prev;
    std::vector<Error> errors;
    Flags<ParseFlags> flags;

    explicit Parser(std::string_view s, const Data &defaults, Flags<ParseFlags> flags)
        : lexer{s}, defaults{defaults}, flags{flags}
    {}

    void error(Token t, Error::Type type, std::string key = {},
        conf::Value defvalue = {}, conf::Value value = {})
    {
        auto [line, col] = lexer.position_of(t);
        throw Error {
            .type = type,
            .prev = std::string(prev.text),
            .cur  = cur.type == Token::End ? "end" : std::string(cur.text),
            .line = line, .col = std::ptrdiff_t(col),
            .key = key,
            .value = value,
            .def = defvalue,
        };
    }

    void advance()
    {
        prev = cur, cur = lexer.lex();
        if (cur.type == Token::Unterminated) error(cur, Error::UnterminatedString);
        if (cur.type == Token::InvalidChar)  error(cur, Error::UnexpectedCharacter);
    }

    void consume(Token::Type type, Error::Type err, std::string key = {},
        conf::Value defvalue = {})
    {
        cur.type == type ? advance() : error(cur, err, key, defvalue);
    }

    bool match(Token::Type type)
    {
        if (cur.type != type)
            return false;
        advance();
        return true;
    }

    std::optional<conf::Value> parse_value()
    {
             if (match(Token::Int))    return conf::Value(string::to_number<  int>(prev.text).value());
        else if (match(Token::Float))  return conf::Value(string::to_number<float>(prev.text).value());
        else if (match(Token::String)) return conf::Value(std::string(prev.text.substr(1, prev.text.size() - 2)));
        else if (match(Token::True)
              || match(Token::False))  return conf::Value(prev.type == Token::True);
        else if (match(Token::LeftSquare)) return parse_list();
        return std::nullopt;
    }

    conf::Value parse_list()
    {
        std::vector<conf::Value> values;
        do
            if (auto v = parse_value(); v)
                values.push_back(v.value());
        while (match(Token::Comma));
        consume(Token::RightSquare, Error::ExpectedRightSquare);
        return conf::Value(std::move(values));
    }

    ParseResult parse()
    {
        conf::Data data;
        advance();
        while (!lexer.at_end()) {
            try {
                if (match(Token::Newline))
                    continue;
                consume(Token::Ident, Error::NoIdent);
                auto ident = std::string(prev.text);
                auto it = defaults.find(ident);
                if (it == defaults.end() && !flags.contains(AcceptAnyKey))
                    error(prev, Error::InvalidKey);
                auto defval = it != defaults.end() ? it->second : Value{};
                auto &pos = data[ident];
                pos = defval;
                consume(Token::EqualSign, Error::NoEqualAfterIdent, ident, defval);
                auto v = parse_value();
                if (!v)
                    error(prev, Error::NoValueAfterEqual, ident, defval);
                else if (it != defaults.end() && v.value().type() != it->second.type())
                    error(prev, Error::MismatchedTypes, ident, it->second, v.value());
                else
                    pos = v.value();
                consume(Token::Newline, Error::NoNewlineAfterValue);
            } catch (const Error &error) {
                errors.push_back(error);
                while (cur.type != Token::End && cur.type != Token::Newline)
                    advance();
                advance();
            }
        }
        for (auto [k, v] : defaults) {
            if (auto r = data.find(k); r == data.end()) {
                data[k] = v;
                errors.push_back({ .type = Error::MissingKey, .key = k, .def = v });
            }
        }
        return std::make_pair(data, errors);
    }
};

} // namespace

std::string Error::message()
{
    if (external_error != std::error_code{})
        return "error: " + external_error.message();
    auto l = [&] { return std::to_string(line) + ":" + std::to_string(col) + ": error: "; };
    auto n = [&] { return "\n   note: using default value '" + def.to_string() + "' for key '" + key + "'"; };
    switch (type) {
    case Error::NoIdent:             return l() + "expected identifier";
    case Error::NoEqualAfterIdent:   return l() + "expected '=' after '" + prev + "'" + n();
    case Error::NoValueAfterEqual:   return l() + "expected value after '='" + n();
    case Error::NoNewlineAfterValue: return l() + "expected newline after '" + prev + "'";
    case Error::UnterminatedString:  return l() + "unterminated string";
    case Error::UnexpectedCharacter: return l() + "unexpected character '" + cur + "'";
    case Error::ExpectedRightSquare: return l() + "expected ']' after '" + prev + "'";
    case Error::InvalidKey:          return l() + "invalid key '" + prev + "'";
    case Error::MissingKey:          return "error: missing key '" + key + "'" + n();
    case Error::MismatchedTypes:     return l() + "mismatched types for key '" + key + "': expected type '"
                                                 + type_to_string(def.type()) + "', got value '" + value.to_string()
                                                 + "' of type '" + type_to_string(value.type()) + "'" + n();
    case Error::External:            return "external error";
    case Error::Custom:              return custom_error_string;
    default:                         return "unknown error";
    }
}

ParseResult parse(std::string_view text, const Data &defaults, Flags<ParseFlags> flags)
{
    Parser parser{text, defaults, flags};
    return parser.parse();
}

std::error_code write_to(std::filesystem::path path, const Data &data)
{
    auto file = io::File::open(path, io::Access::Write);
    if (!file)
        return file.error();
    auto width = std::max_element(data.begin(), data.end(), [](const auto &a, const auto &b) {
        return a.first.size() < b.first.size();
    })->first.size();
    for (auto [k, v] : data)
        fprintf(file.value().data(), "%.*s = %s\n", int(width), k.c_str(), v.to_string().c_str());
    return std::error_code{};
}

std::error_code write(std::string_view appname, const Data &data)
{
    auto dir = getdir(appname);
    if (!dir)
        return dir.error();
    return write_to(dir.value() / (std::string(appname) + ".conf"), data);
}

tl::expected<std::filesystem::path, std::error_code> getdir(std::string_view appname)
{
    auto config = io::directory::config();
    auto appdir = fs::exists(config) ? config / appname
                                     : io::directory::home() / ("." + std::string(appname));
    if (!fs::exists(appdir)) {
        std::error_code ec;
        if (!fs::create_directory(appdir), ec)
            return tl::unexpected(ec);
    }
    return appdir;
}

ParseResult parse_or_create(std::string_view appname, const Data &defaults, Flags<ParseFlags> flags)
{
    auto dir = getdir(appname);
    if (!dir)
        return std::make_pair(defaults, std::vector{ Error { .type = Error::External, .external_error = dir.error() } });
    auto file_path = dir.value() / (std::string(appname) + ".conf");
    if (auto text = io::read_file(file_path); text)
        return parse(text.value(), defaults, flags);
    auto err = write_to(file_path, defaults);
    return std::make_pair(
        defaults,
        err == std::error_code{} ? std::vector<Error>{}
                                 : std::vector{ Error { .type = Error::External, .external_error = err } }
    );
}

} // namespace conf
