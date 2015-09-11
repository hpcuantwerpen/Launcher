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
    s=nullptr;

    s = pbs::parse("blabla#huhhuh");
    QVERIFY2(s->type()==pbs::types::ShellCommand, "Failure");
    delete s;
    s=nullptr;

    s = pbs::parse("blabla #huhhuh");
    QVERIFY2(s->type()==pbs::types::ShellCommand, "Failure");
    delete s;
    s=nullptr;

    s = pbs::parse("#blabla");
    QVERIFY2(s->type()==pbs::types::UserComment, "Failure");
    delete s;
    s=nullptr;

    s = pbs::parse("#!blabla");
    QVERIFY2(s->type()==pbs::types::Shebang, "Failure");
    delete s;
    s=nullptr;

    try {
        s = pbs::parse("#LAU blabla");
        QVERIFY2(false, "Failure");
    } catch(std::runtime_error &e) {
        std::cout << "(Intentional) Error: " << e.what() <<std::endl;
    }

    s = pbs::parse("#LAU bla = bla");
    QVERIFY2(s->type()==pbs::types::LauncherComment, "Failure");
    QVERIFY2(s->parameters()["bla"]=="bla","failure");
    delete s;
    s=nullptr;

    try {
        s = pbs::parse("#PBS blabla");
        QVERIFY2(false, "Failure");
    } catch(std::runtime_error &e) {
        std::cout << "(Intentional) Error: " << e.what() <<std::endl;
    }

    s = pbs::parse("#PBS -W x=nmatchpolicy:y=soep:z=ballekes:exactnode #huh");
    QVERIFY2(s->type()==pbs::types::PbsDirective, "Failure");
    delete s;
    s=nullptr;
}

void Jobscript_test::testCase2()
{
    try {
        pbs::Script script;
        script["-M"] = "engelbert.tijskens@uantwerpen.be";
        script["nodes"]= "2";
        QString s = script.text();
        std::cout << s.toStdString();
        script.write("testCase2.sh",false);
        std::cout<<"writing script to file" << std::endl;
    } catch(std::exception &e) {
        std::cout << "Error: " << e.what() <<std::endl;
        QVERIFY2(false, "Failure");
    }
    try {
        std::cout<<"reading script from file" << std::endl;
        pbs::Script script("testCase2.sh");
        QString s = script.text();
        std::cout << s.toStdString();
    } catch(std::exception &e) {
        std::cout << "Error: " << e.what() <<std::endl;
        QVERIFY2(false, "Failure");
    }
}
QTEST_APPLESS_MAIN(Jobscript_test)

#include "jobscript_test.moc"
