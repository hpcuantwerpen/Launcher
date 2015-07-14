#include <QString>
#include <QtTest>

class Jobscript_test : public QObject
{
    Q_OBJECT

public:
    Jobscript_test();

private Q_SLOTS:
    void testCase1();
};

Jobscript_test::Jobscript_test()
{
}

void Jobscript_test::testCase1()
{
    QVERIFY2(true, "Failure");
}

QTEST_APPLESS_MAIN(Jobscript_test)

#include "tst_jobscript_test.moc"
