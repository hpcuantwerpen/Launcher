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
    QString qout;
    QString qerr;

    ssh2::Session s;
    s.open();
    //s.exec("uptime");
    s.exec("uptime", &qout );
    std::cout << "****\n"
              << qout.toStdString()
              << "****" << std::endl;
    s.exec("uptime", &qout, &qerr );
    std::cout << "****\n"
              << qout.toStdString()
              << "****" << std::endl;
    std::cout << "****\n"
              << qerr.toStdString()
              << "****" << std::endl;
    s.close();
    QVERIFY2(true, "Failure");
}

QTEST_APPLESS_MAIN(Ssh2_test)

#include "ssh2tools_test.moc"
