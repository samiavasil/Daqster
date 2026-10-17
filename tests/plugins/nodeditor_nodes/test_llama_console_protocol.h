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
    void consoleEmitsRequestEnvelopeWithModelSchema();
    void llamaModelAcceptsConsoleEnvelope();
    void llamaModelReportsNoConnectionInsteadOfSilence();
    void consoleExtractsReplyFromLlamaModelEnvelope();
    void consoleAlsoAcceptsMessageAndContentShapes();
    void llamaModelIgnoresEmptyAndMalformedInput();
};
