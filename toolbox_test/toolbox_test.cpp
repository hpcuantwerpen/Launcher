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
    void testCase2();
};

ToolboxTest::ToolboxTest()
{
}

void ToolboxTest::testCase1()
{
    toolbox::OrderedMap om;
    om["one"]="1";
    QVERIFY2(om["one"]=="1", "Failure");
    QString now = toolbox::now();
}

void ToolboxTest::testCase2()
{
    Log l1(1);
    l1 << "blabla" << std::endl;
    Log(2) << std::string("silence")<< std::endl;
    Log(0) <<INFO<< std::string("really?")<< std::endl;
}

QTEST_APPLESS_MAIN(ToolboxTest)

#include "toolbox_test.moc"
