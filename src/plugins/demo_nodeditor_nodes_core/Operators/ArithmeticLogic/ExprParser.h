#ifndef EXPRPARSER_H
#define EXPRPARSER_H

#pragma once

#include <QString>
#include <QVector>
#include <QMap>
#include <QStringList>

class ExprParser
{
public:
    enum class TokenType {
        NUMBER, VARIABLE,
        PLUS, MINUS, STAR, SLASH, PERCENT,
        AMP, PIPE, CARET, TILDE,
        LT, GT, LE, GE, EQ, NE,
        AMP_AMP, PIPE_PIPE, BANG,
        LSHIFT, RSHIFT,
        QUESTION, COLON,
        LPAREN, RPAREN,
        END
    };

    struct Token {
        TokenType type;
        double numValue = 0;
        QString varName;
    };

    ExprParser();

    bool parse(const QString& expression);
    double evaluate();
    void setVariable(const QString& name, double value);
    QString errorString() const;
    QStringList variables() const;

private:
    void tokenize(const QString& expression);
    Token next();
    Token peek();
    void expect(TokenType type);

    double parseExpression();
    double parseTernary();
    double parseLogicalOr();
    double parseLogicalAnd();
    double parseBitwiseOr();
    double parseBitwiseXor();
    double parseBitwiseAnd();
    double parseEquality();
    double parseComparison();
    double parseShift();
    double parseAdditive();
    double parseMultiplicative();
    double parseUnary();
    double parsePrimary();

    QVector<Token> m_tokens;
    int m_pos;
    QMap<QString, double> m_vars;
    QString m_error;
};

#endif // EXPRPARSER_H
