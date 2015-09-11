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
    void testCase2();
};

Ssh2_test::Ssh2_test()
{
}

void Ssh2_test::testCase1()
{
    QString qout;
    QString qerr;

    ssh2::Session s;
    s.setUsername("vsc20170");
    s.setPrivatePublicKeyPair("/Users/etijskens/.ssh/id_rsa_npw");
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

    s.exec("ls", &qout, &qerr );
    std::cout << "****\n"
              << qout.toStdString()
              << "****" << std::endl;
    std::cout << "****\n"
              << qerr.toStdString()
              << "****" << std::endl;

    s.close();
    QVERIFY2(true, "Failure");
}

void Ssh2_test::testCase2()
{
    QString qout;
    QString qerr;

    ssh2::Session s;
    s.setUsername("vsc20170");
    s.setPrivatePublicKeyPair("/Users/etijskens/.ssh/id_rsa");
    try {
        s.open();
    } catch( ssh2::PassphraseNeeded& e ) {
        std::cout << "\n(Intentional) Error: " << e.what() << std::endl;
        s.setPassphrase("Thezoo12");
        s.open();
        s.close();
    }
    try {
        s.setPassphrase("wrong passphrase");
        s.open();
    } catch( ssh2::WrongPassphrase& e ) {
        std::cout << "\n(Intentional) Error: " << e.what() << std::endl;
    }

    QVERIFY2(true, "Failure");
}

QTEST_APPLESS_MAIN(Ssh2_test)

#include "ssh2tools_test.moc"
