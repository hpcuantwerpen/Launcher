#include <QString>
#include <QtTest>
#include "ssh2tools.h"

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
    s.exec("uptime");
    s.close();
    QVERIFY2(true, "Failure");
}

QTEST_APPLESS_MAIN(Ssh2_test)

#include "ssh2tools_test.moc"
