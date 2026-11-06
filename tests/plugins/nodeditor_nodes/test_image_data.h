#pragma once

#include <QtTest>

// Test class for ImageData (REQ-SW-PL-080). Declared in a header instead of
// relying on QTEST_GUILESS_MAIN inside the .cpp, so the class shares the
// nodeditor_nodes test binary with the other value-type suites — see
// test_main.cpp.
class ImageDataTest : public QObject
{
    Q_OBJECT

private slots:
    void typeIdIsImage();
    void sharedPtrTransferKeepsTheSameBuffer();
    void metadataSurvivesTransfer();
    void defaultConstructedIsEmpty();
    void imageFromNullImageIsEmpty();
    void repeatedReadsDoNotDetach();
    void widthHeightComeFromTheImage();
};
