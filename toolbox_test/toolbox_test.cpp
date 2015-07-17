#include <QString>
#include <QtTest>

#include <stdexcept>
#include <toolbox.h>

class ToolboxTest : public QObject
{
    Q_OBJECT

public:
    ToolboxTest();

private Q_SLOTS:
    void testCase1();
};

ToolboxTest::ToolboxTest()
{
}

void ToolboxTest::testCase1()
{
    toolbox::OrderedMap om;
    om["one"]="1";
    QVERIFY2(om["one"]=="1", "Failure");
}

QTEST_APPLESS_MAIN(ToolboxTest)

#include "toolbox_test.moc"
