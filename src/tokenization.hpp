#pragma once

// Tokens available in language
enum class TokenType
{
    exit,
    int_lit,
    semi,
    open_paren,
    close_paren,
    ident,
    let,
    eq,
    plus,
    star,
    minus,
    fslash,
    percent,
    open_curly,
    close_curly,
    _if,
    _else,
    elif,
    print,
    function,
    char_lit,
    float_lit,
    comma,
};

// converting tokens to strings to indicate errors
inline std::string to_string(const TokenType type)
{
    switch (type)
    {
    case TokenType::exit:
        return "`exit`";
    case TokenType::int_lit:
        return "int literal";
    case TokenType::semi:
        return "`;`";
    case TokenType::open_paren:
        return "`(`";
    case TokenType::close_paren:
        return "`)`";
    case TokenType::ident:
        return "identifier";
    case TokenType::let:
        return "`let`";
    case TokenType::eq:
        return "`=`";
    case TokenType::plus:
        return "`+`";
    case TokenType::minus:
        return "`-`";
    case TokenType::star:
        return "`*`";
    case TokenType::fslash:
        return "`/`";
    case TokenType::percent:
        return "`%`";
    case TokenType::open_curly:
        return "`{`";
    case TokenType::close_curly:
        return "`}`";
    case TokenType::_if:
        return "`if`";
    case TokenType::_else:
        return "`else`";
    case TokenType::elif:
        return "`elif`";
    case TokenType::print:
        return "`print`";
    case TokenType::function:
        return "`function`";
    case TokenType::char_lit:
        return "char literal";
    case TokenType::float_lit:
        return "float literal";
    case TokenType::comma:
        return "`,`";
    default:
        assert(false);
    }
}

// precedence of operators
inline std::optional<int> bin_prec(const TokenType type)
{
    switch (type)
    {
    case TokenType::plus:
    case TokenType::minus:
        return 0;
    case TokenType::star:
    case TokenType::fslash:
    case TokenType::percent:
        return 1;
    default:
        return {};
    }
}

// sequence of meaningful characters of the language
struct Token
{
    TokenType type;
    int line;
    std::optional<std::string> value{};
};

class Tokenizer
{

public:
    // takes input .blu file contents as string
    explicit Tokenizer(std::string src)
        : m_src(std::move(src))
    {
    }

    std::vector<Token> tokenize()
    {
        std::vector<Token> tokens;
        std::string buf;
        int line_count = 1;
        while (peek().has_value())
        {
            // keywords, identifiers cannot start with a number
            if (std::isalpha(peek().value()))
            {
                buf.push_back(consume());
                // consecutive characters can be alphabet or number
                while (peek().has_value() && std::isalnum(peek().value()))
                {
                    buf.push_back(consume());
                }
                if (buf == "exit")
                {
                    tokens.push_back({TokenType::exit, line_count});
                }
                else if (buf == "let")
                {
                    tokens.push_back({TokenType::let, line_count});
                }
                else if (buf == "if")
                {
                    tokens.push_back({TokenType::_if, line_count});
                }
                else if (buf == "else")
                {
                    tokens.push_back({TokenType::_else, line_count});
                }
                else if (buf == "elif")
                {
                    tokens.push_back({TokenType::elif, line_count});
                }
                else if (buf == "print")
                {
                    tokens.push_back({TokenType::print, line_count});
                }
                else if (buf == "function")
                {
                    tokens.push_back({TokenType::function, line_count});
                }
                else
                {
                    tokens.push_back({TokenType::ident, line_count, buf});
                }
                buf.clear();
            }
            // integer literals
            else if (std::isdigit(peek().value()) || peek().value() == '.')
            {
                if(std::isdigit(peek().value()))
                {
                    buf.push_back(consume());
                    while (peek().has_value() && std::isdigit(peek().value()))
                    {
                        buf.push_back(consume());
                    }
                }
                else
                {
                    buf.push_back('0');
                }
                if(peek().has_value() && peek().value() == '.')
                {
                    buf.push_back(consume());
                    while (peek().has_value() && std::isdigit(peek().value()))
                    {
                        buf.push_back(consume());
                    }
                    tokens.push_back({TokenType::float_lit, line_count, buf});
                }
                else
                {
                    tokens.push_back({TokenType::int_lit, line_count, buf});
                }
                buf.clear();
            }
            // no tokens for space
            else if (std::isspace(peek().value()))
            {
                if (peek().value() == '\n')
                {
                    line_count++;
                }
                consume();
            }
            else
            {
                // tokenising single symbols
                char c = peek().value();
                switch (c)
                {
                case '=':
                    consume();
                    tokens.push_back({TokenType::eq, line_count});
                    break;
                case '(':
                    consume();
                    tokens.push_back({TokenType::open_paren, line_count});
                    break;
                case ')':
                    consume();
                    tokens.push_back({TokenType::close_paren, line_count});
                    break;
                case ';':
                    consume();
                    tokens.push_back({TokenType::semi, line_count});
                    break;
                case '+':
                    consume();
                    tokens.push_back({TokenType::plus, line_count});
                    break;
                case '*':
                    consume();
                    tokens.push_back({TokenType::star, line_count});
                    break;
                case '-':
                    consume();
                    tokens.push_back({TokenType::minus, line_count});
                    break;
                case '/':
                    consume();
                    // single line comment
                    if (peek().has_value() && peek().value() == '/')
                    {
                        consume();
                        while (peek().has_value() && peek().value() != '\n')
                        {
                            consume();
                        }
                        line_count++;
                    }
                    // multi line comment
                    else if (peek().has_value() && peek().value() == '*')
                    {
                        consume();
                        while (peek().has_value())
                        {
                            if (peek().value() == '*' && peek(1).has_value() && peek(1).value() == '/')
                            {
                                break;
                            }
                            if (consume() == '\n')
                            {
                                line_count++;
                            }
                        }
                        if (peek().has_value() && peek().value() == '*')
                            consume();
                        if (peek().has_value() && peek().value() == '/')
                            consume();
                    }
                    else
                    {
                        tokens.push_back({TokenType::fslash, line_count});
                    }
                    break;
                case '%':
                    consume();
                    tokens.push_back({TokenType::percent, line_count});
                    break;
                case '{':
                    consume();
                    tokens.push_back({TokenType::open_curly, line_count});
                    break;
                case '}':
                    consume();
                    tokens.push_back({TokenType::close_curly, line_count});
                    break;
                case '\'':
                    consume();
                    if(peek().has_value() && peek().value() == '\'')
                    {
                        tokens.push_back({TokenType::char_lit, line_count, ""});                    
                    }
                    else if(peek(1).has_value() && peek(1).value() == '\'')
                    {
                        tokens.push_back({TokenType::char_lit, line_count, std::to_string(consume())});
                    }
                    consume();
                    break;
                case ',':
                    consume();
                    tokens.push_back({TokenType::comma, line_count});
                    break;
                case '\n':
                    consume();
                    line_count++;
                    break;
                default:
                    std::cerr << "Invalid token" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
        }
        m_index = 0;
        return tokens;
    }

private:
    // for peeking next character
    [[nodiscard]] std::optional<char> peek(const size_t offset = 0) const
    {
        if (m_index + offset >= m_src.length())
        {
            return {};
        }
        return m_src.at(m_index + offset);
    }

    // consuming character
    char consume()
    {
        return m_src.at(m_index++);
    }

    size_t m_index = 0;
    const std::string m_src;
};