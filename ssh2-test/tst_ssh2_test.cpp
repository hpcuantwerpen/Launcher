#include <QString>
#include <QtTest>
#include "ssh2.h"

class Ssh2_test : public QObject
{
    Q_OBJECT

public:
    Ssh2_test();

private Q_SLOTS:
    void testCase1();
};

Ssh2_test::Ssh2_test()
{
}

void Ssh2_test::testCase1()
{
    ssh2::Session s;
    s.open();
    QVERIFY2(true, "Failure");
}

QTEST_APPLESS_MAIN(Ssh2_test)

#include "tst_ssh2_test.moc"
