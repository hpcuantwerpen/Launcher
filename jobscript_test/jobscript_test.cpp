#include <QString>
#include <QtTest>

#include <jobscript.h>

class Jobscript_test : public QObject
{
    Q_OBJECT

public:
    Jobscript_test();

private Q_SLOTS:
    void testCase1();
    void testCase2();
};

Jobscript_test::Jobscript_test()
{
}

void Jobscript_test::testCase1()
{
    pbs::ShellCommand* s = nullptr;
    s = pbs::parse("blabla");
    QVERIFY2(s->type()==pbs::types::ShellCommand, "Failure");
    delete s;

    s = pbs::parse("#blabla");
    QVERIFY2(s->type()==pbs::types::UserComment, "Failure");
    delete s;

    s = pbs::parse("#!blabla");
    QVERIFY2(s->type()==pbs::types::Shebang, "Failure");
    delete s;

    try {
        s = pbs::parse("#La# blabla");
        delete s;
        QVERIFY2(false, "Failure");
    } catch(std::runtime_error &e) {
        std::cout << "(Intentional) Error: " << e.what() <<std::endl;
    }

    s = pbs::parse("#La# bla = bla");
    QVERIFY2(s->type()==pbs::types::LauncherComment, "Failure");
    QVERIFY2(s->parms()["bla"]=="bla","failure");
    delete s;

    try {
        s = pbs::parse("#PBS blabla");
        delete s;
        QVERIFY2(false, "Failure");
    } catch(std::runtime_error &e) {
        std::cout << "(Intentional) Error: " << e.what() <<std::endl;
    }

    s = pbs::parse("#PBS -W x=nmatchpolicy:y=soep:z=ballekes:exactnode #huh");
    QVERIFY2(s->type()==pbs::types::PbsDirective, "Failure");
    delete s;
}
void Jobscript_test::testCase2()
{
    pbs::Script script();
}
QTEST_APPLESS_MAIN(Jobscript_test)

#include "jobscript_test.moc"
