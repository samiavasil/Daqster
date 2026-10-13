#include "ExprParser.h"
#include <QRegularExpression>

ExprParser::ExprParser()
    : m_pos(0)
{
}

void ExprParser::tokenize(const QString& expression)
{
    m_tokens.clear();
    int i = 0;
    int len = expression.length();

    while (i < len) {
        QChar c = expression[i];

        if (c.isSpace()) {
            i++;
            continue;
        }

        // Numbers
        if (c.isDigit() || c == '.') {
            int start = i;
            while (i < len && (expression[i].isDigit() || expression[i] == '.')) {
                i++;
            }
            Token tok;
            tok.type = TokenType::NUMBER;
            tok.numValue = expression.mid(start, i - start).toDouble();
            m_tokens.append(tok);
            continue;
        }

        // Variables (single letter a-h)
        if (c.isLetter() && c.unicode() >= 'a' && c.unicode() <= 'h') {
            Token tok;
            tok.type = TokenType::VARIABLE;
            tok.varName = QString(c);
            m_tokens.append(tok);
            i++;
            continue;
        }

        // Operators
        Token tok;
        tok.type = TokenType::END;

        switch (c.unicode()) {
        case '+': tok.type = TokenType::PLUS; break;
        case '-': tok.type = TokenType::MINUS; break;
        case '*': tok.type = TokenType::STAR; break;
        case '/': tok.type = TokenType::SLASH; break;
        case '%': tok.type = TokenType::PERCENT; break;
        case '&':
            if (i + 1 < len && expression[i + 1] == '&') {
                tok.type = TokenType::AMP_AMP;
                i++;
            } else {
                tok.type = TokenType::AMP;
            }
            break;
        case '|':
            if (i + 1 < len && expression[i + 1] == '|') {
                tok.type = TokenType::PIPE_PIPE;
                i++;
            } else {
                tok.type = TokenType::PIPE;
            }
            break;
        case '^': tok.type = TokenType::CARET; break;
        case '~': tok.type = TokenType::TILDE; break;
        case '<':
            if (i + 1 < len && expression[i + 1] == '<') {
                tok.type = TokenType::LSHIFT;
                i++;
            } else if (i + 1 < len && expression[i + 1] == '=') {
                tok.type = TokenType::LE;
                i++;
            } else {
                tok.type = TokenType::LT;
            }
            break;
        case '>':
            if (i + 1 < len && expression[i + 1] == '>') {
                tok.type = TokenType::RSHIFT;
                i++;
            } else if (i + 1 < len && expression[i + 1] == '=') {
                tok.type = TokenType::GE;
                i++;
            } else {
                tok.type = TokenType::GT;
            }
            break;
        case '=':
            if (i + 1 < len && expression[i + 1] == '=') {
                tok.type = TokenType::EQ;
                i++;
            }
            break;
        case '!':
            if (i + 1 < len && expression[i + 1] == '=') {
                tok.type = TokenType::NE;
                i++;
            } else {
                tok.type = TokenType::BANG;
            }
            break;
        case '?': tok.type = TokenType::QUESTION; break;
        case ':': tok.type = TokenType::COLON; break;
        case '(': tok.type = TokenType::LPAREN; break;
        case ')': tok.type = TokenType::RPAREN; break;
        default:
            m_error = QString("Unexpected character: '%1' at position %2").arg(c).arg(i);
            return;
        }

        m_tokens.append(tok);
        i++;
    }

    Token endTok;
    endTok.type = TokenType::END;
    m_tokens.append(endTok);
}

ExprParser::Token ExprParser::next()
{
    if (m_pos < m_tokens.size()) {
        return m_tokens[m_pos++];
    }
    Token endTok;
    endTok.type = TokenType::END;
    return endTok;
}

ExprParser::Token ExprParser::peek()
{
    if (m_pos < m_tokens.size()) {
        return m_tokens[m_pos];
    }
    Token endTok;
    endTok.type = TokenType::END;
    return endTok;
}

void ExprParser::expect(TokenType type)
{
    Token tok = next();
    if (tok.type != type) {
        m_error = QString("Unexpected token, expected type %1").arg(static_cast<int>(type));
    }
}

bool ExprParser::parse(const QString& expression)
{
    m_error.clear();
    m_tokens.clear();
    m_pos = 0;
    m_vars.clear();

    tokenize(expression);
    if (!m_error.isEmpty()) return false;

    // Extract variable names
    for (const Token& tok : m_tokens) {
        if (tok.type == TokenType::VARIABLE) {
            m_vars[tok.varName] = 0;
        }
    }

    return true;
}

QStringList ExprParser::variables() const
{
    return m_vars.keys();
}

QString ExprParser::errorString() const
{
    return m_error;
}

double ExprParser::evaluate()
{
    m_pos = 0;
    double result = parseExpression();
    if (!m_error.isEmpty()) return 0;
    return result;
}

void ExprParser::setVariable(const QString& name, double value)
{
    m_vars[name] = value;
}

double ExprParser::parseExpression()
{
    return parseTernary();
}

double ExprParser::parseTernary()
{
    double cond = parseLogicalOr();

    if (peek().type == TokenType::QUESTION) {
        next(); // consume '?'
        double trueVal = parseExpression();
        expect(TokenType::COLON);
        double falseVal = parseExpression();
        return cond != 0 ? trueVal : falseVal;
    }

    return cond;
}

double ExprParser::parseLogicalOr()
{
    double left = parseLogicalAnd();

    while (peek().type == TokenType::PIPE_PIPE) {
        next();
        double right = parseLogicalAnd();
        left = (left != 0 || right != 0) ? 1.0 : 0.0;
    }

    return left;
}

double ExprParser::parseLogicalAnd()
{
    double left = parseBitwiseOr();

    while (peek().type == TokenType::AMP_AMP) {
        next();
        double right = parseBitwiseOr();
        left = (left != 0 && right != 0) ? 1.0 : 0.0;
    }

    return left;
}

double ExprParser::parseBitwiseOr()
{
    double left = parseBitwiseXor();

    while (peek().type == TokenType::PIPE) {
        next();
        double right = parseBitwiseXor();
        left = static_cast<double>(static_cast<int64_t>(left) | static_cast<int64_t>(right));
    }

    return left;
}

double ExprParser::parseBitwiseXor()
{
    double left = parseBitwiseAnd();

    while (peek().type == TokenType::CARET) {
        next();
        double right = parseBitwiseAnd();
        left = static_cast<double>(static_cast<int64_t>(left) ^ static_cast<int64_t>(right));
    }

    return left;
}

double ExprParser::parseBitwiseAnd()
{
    double left = parseEquality();

    while (peek().type == TokenType::AMP) {
        next();
        double right = parseEquality();
        left = static_cast<double>(static_cast<int64_t>(left) & static_cast<int64_t>(right));
    }

    return left;
}

double ExprParser::parseEquality()
{
    double left = parseComparison();

    while (peek().type == TokenType::EQ || peek().type == TokenType::NE) {
        Token op = next();
        double right = parseComparison();
        if (op.type == TokenType::EQ) {
            left = (left == right) ? 1.0 : 0.0;
        } else {
            left = (left != right) ? 1.0 : 0.0;
        }
    }

    return left;
}

double ExprParser::parseComparison()
{
    double left = parseShift();

    while (peek().type == TokenType::LT || peek().type == TokenType::GT ||
           peek().type == TokenType::LE || peek().type == TokenType::GE) {
        Token op = next();
        double right = parseShift();
        switch (op.type) {
        case TokenType::LT: left = (left < right) ? 1.0 : 0.0; break;
        case TokenType::GT: left = (left > right) ? 1.0 : 0.0; break;
        case TokenType::LE: left = (left <= right) ? 1.0 : 0.0; break;
        case TokenType::GE: left = (left >= right) ? 1.0 : 0.0; break;
        default: break;
        }
    }

    return left;
}

double ExprParser::parseShift()
{
    double left = parseAdditive();

    while (peek().type == TokenType::LSHIFT || peek().type == TokenType::RSHIFT) {
        Token op = next();
        double right = parseAdditive();
        int64_t iLeft = static_cast<int64_t>(left);
        int64_t iRight = static_cast<int64_t>(right);
        if (op.type == TokenType::LSHIFT) {
            left = static_cast<double>(iLeft << iRight);
        } else {
            left = static_cast<double>(iLeft >> iRight);
        }
    }

    return left;
}

double ExprParser::parseAdditive()
{
    double left = parseMultiplicative();

    while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
        Token op = next();
        double right = parseMultiplicative();
        if (op.type == TokenType::PLUS) {
            left = left + right;
        } else {
            left = left - right;
        }
    }

    return left;
}

double ExprParser::parseMultiplicative()
{
    double left = parseUnary();

    while (peek().type == TokenType::STAR || peek().type == TokenType::SLASH ||
           peek().type == TokenType::PERCENT) {
        Token op = next();
        double right = parseUnary();
        switch (op.type) {
        case TokenType::STAR:
            left = left * right;
            break;
        case TokenType::SLASH:
            if (right == 0.0) {
                m_error = "Division by zero";
                return 0;
            }
            left = left / right;
            break;
        case TokenType::PERCENT:
            if (right == 0.0) {
                m_error = "Modulo by zero";
                return 0;
            }
            left = static_cast<double>(static_cast<int64_t>(left) % static_cast<int64_t>(right));
            break;
        default: break;
        }
    }

    return left;
}

double ExprParser::parseUnary()
{
    Token tok = peek();

    if (tok.type == TokenType::MINUS) {
        next();
        return -parseUnary();
    }
    if (tok.type == TokenType::BANG) {
        next();
        return (parseUnary() == 0.0) ? 1.0 : 0.0;
    }
    if (tok.type == TokenType::TILDE) {
        next();
        return static_cast<double>(~static_cast<int64_t>(parseUnary()));
    }

    return parsePrimary();
}

double ExprParser::parsePrimary()
{
    Token tok = next();

    if (tok.type == TokenType::NUMBER) {
        return tok.numValue;
    }

    if (tok.type == TokenType::VARIABLE) {
        if (m_vars.contains(tok.varName)) {
            return m_vars[tok.varName];
        }
        m_error = QString("Undefined variable: %1").arg(tok.varName);
        return 0;
    }

    if (tok.type == TokenType::LPAREN) {
        double result = parseExpression();
        expect(TokenType::RPAREN);
        return result;
    }

    m_error = "Unexpected token in expression";
    return 0;
}
