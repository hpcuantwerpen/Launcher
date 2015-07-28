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
    std::shared_ptr<pbs::ShellCommand> s;
    s = pbs::parse("blabla");
    QVERIFY2(s->type()==pbs::types::ShellCommand, "Failure");

    s = pbs::parse("blabla#huhhuh");
    QVERIFY2(s->type()==pbs::types::ShellCommand, "Failure");

    s = pbs::parse("blabla #huhhuh");
    QVERIFY2(s->type()==pbs::types::ShellCommand, "Failure");

    s = pbs::parse("#blabla");
    QVERIFY2(s->type()==pbs::types::UserComment, "Failure");

    s = pbs::parse("#!blabla");
    QVERIFY2(s->type()==pbs::types::Shebang, "Failure");

    try {
        s = pbs::parse("#LAU blabla");
        QVERIFY2(false, "Failure");
    } catch(std::runtime_error &e) {
        std::cout << "(Intentional) Error: " << e.what() <<std::endl;
    }

    s = pbs::parse("#LAU bla = bla");
    QVERIFY2(s->type()==pbs::types::LauncherComment, "Failure");
    QVERIFY2(s->parameters()["bla"]=="bla","failure");

    try {
        s = pbs::parse("#PBS blabla");
        QVERIFY2(false, "Failure");
    } catch(std::runtime_error &e) {
        std::cout << "(Intentional) Error: " << e.what() <<std::endl;
    }

    s = pbs::parse("#PBS -W x=nmatchpolicy:y=soep:z=ballekes:exactnode #huh");
    QVERIFY2(s->type()==pbs::types::PbsDirective, "Failure");
}

void Jobscript_test::testCase2()
{
    try {
        pbs::Script script;
        QString s = script.text();
        std::cout << s.toStdString();
    } catch(std::exception &e) {
        std::cout << "Error: " << e.what() <<std::endl;
        QVERIFY2(false, "Failure");
    }
}
QTEST_APPLESS_MAIN(Jobscript_test)

#include "jobscript_test.moc"
