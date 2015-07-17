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

    s = pbs::parse("#La# blabla");
    QVERIFY2(s->type()==pbs::types::LauncherComment, "Failure");
    delete s;

    s = pbs::parse("#PBS blabla");
    QVERIFY2(s->type()==pbs::types::PbsDirective, "Failure");
    delete s;
}

QTEST_APPLESS_MAIN(Jobscript_test)

#include "jobscript_test.moc"
