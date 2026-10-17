#pragma once

#include <QObject>

/**
 * Protocol tests for the Конзола (ConsoleDataModel) ↔ LLamaModel
 * (LLamaModelDataModel) pair (REQ-SW-PL-074, AC 2-4).
 *
 * The pair talks over the graph: Console.out carries
 *   {"messages":[...],"temperature":<d>,"n_predict":<i>}
 * and LlamaModel.out carries
 *   {"request_body":{...},"response":{...OpenAI chat completion...}}
 *
 * These are the wire contracts that must survive the core/gui split. No HTTP
 * server is started here: the assertions cover parsing/formatting only, which
 * is where a silent regression would hide.
 */
class TestLlamaConsoleProtocol : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void consoleEmitsRequestEnvelopeWithModelSchema();
    void llamaModelAcceptsConsoleEnvelope();
    void llamaModelReportsNoConnectionInsteadOfSilence();
    void consoleExtractsReplyFromLlamaModelEnvelope();
    void consoleAlsoAcceptsMessageAndContentShapes();
    void consoleExtractsReplyFromThinkingModelEnvelope();
    void llamaModelIgnoresEmptyAndMalformedInput();

    // REQ-SW-PL-074 (R5/R6): the end-to-end local-server path. These drive the
    // real QProcess + QNetworkAccessManager code against an in-process HTTP
    // stand-in, because the two defects they guard against (an unset base URL
    // and a dropped thinking-model reply) are invisible to pure parsing tests.
    void localServerStartEstablishesBaseUrlAndDeliversReply();
    void consoleEnvelopeReachesLocalServerAndReturnsExchange();
    // Opt-in (env LLAMA_SERVER_BIN + LLAMA_SERVER_MODEL): same path, real
    // llama.cpp server. Skips by default so the suite stays hermetic.
    void liveRealServerEndToEnd();
};
