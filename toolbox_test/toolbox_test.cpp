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
    void testCase3();
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

void ToolboxTest::testCase3()
{// test verify.h
    QDir("/Users/etijskens/Launcher/oops").removeRecursively();
    QDir("/Users/etijskens/oops").removeRecursively();

    bool ok;
    QStringList folders;
    int verify_error;
 // create_path==false, stay_inside==true
    {// existing and inside
        QDir dir("/Users/etijskens/Launcher");
        ok = verify_directory(&dir);
        QVERIFY2( ok, "Failure");
    }{//inside but non existing
        QDir dir("/Users/etijskens/Launcher/oops");
        ok = verify_directory(&dir,false,&verify_error);
        QVERIFY2( !ok, "Failure");
        QVERIFY2( verify_error==Inexisting, "Failure");
    }{//outside
        QDir dir("/Users/etijskens/Launcher");
        ok = verify_directory(&dir);
        QVERIFY2( ok, "Failure");
        ok = verify_directory(&dir,"/oops",false,true,&folders,&verify_error);
        QVERIFY2( !ok, "Failure");
    }{//outside
        QDir dir("/Users/etijskens/Launcher");
        ok = verify_directory(&dir);
        QVERIFY2( ok, "Failure");
        ok = verify_directory(&dir,"../oops",false,true,&folders,&verify_error);
        QVERIFY2( !ok, "Failure");
        QVERIFY2( verify_error==Outside, "Failure");
    }
 // create_path==true, stay_inside==true
    {// not inside
        QDir dir("/Users/etijskens/Launcher");
        ok = verify_directory(&dir);
        QVERIFY2( ok, "Failure");
        ok = verify_directory(&dir,"/oops",true,true,&folders,&verify_error);
        QVERIFY2( !ok, "Failure");
        QVERIFY2( verify_error==Outside, "Failure");
    }{//not inside
        QDir dir("/Users/etijskens/Launcher");
        ok = verify_directory(&dir);
        QVERIFY2( ok, "Failure");
        ok = verify_directory(&dir,"../oops",true,true,&folders,&verify_error);
        QVERIFY2( !ok, "Failure");
        QVERIFY2( verify_error==Outside, "Failure");
    }{//inside
        QDir dir("/Users/etijskens/Launcher");
        ok = verify_directory(&dir);
        QVERIFY2( ok, "Failure");
        ok = verify_directory(&dir,"oops",true,true,&folders,&verify_error);
        QVERIFY2( ok, "Failure");
        QVERIFY2( folders.at(0)=="/Users/etijskens/Launcher", "Failure");
        QVERIFY2( folders.at(1)=="", "Failure");
        QVERIFY2( folders.at(2)=="oops", "Failure");
        QVERIFY2( verify_error==Ok, "Failure");
    }
 // create_path==true, stay_inside==false
    {// not inside
        QDir dir("/Users/etijskens/Launcher");
        ok = verify_directory(&dir);
        QVERIFY2( ok, "Failure");
        ok = verify_directory(&dir,"../oops",true,false,&folders,&verify_error);
        QVERIFY2( ok, "Failure");
        QVERIFY2( folders.at(0)=="/Users/etijskens/Launcher", "Failure");
        QVERIFY2( folders.at(1)=="..", "Failure");
        QVERIFY2( folders.at(2)=="oops", "Failure");
        QVERIFY2( verify_error==Ok, "Failure");
    }{// not inside
        QDir dir("/Users/etijskens/Launcher");
        ok = verify_directory(&dir);
        QVERIFY2( ok, "Failure");
        ok = verify_directory(&dir,"/Users/etijskens/oops/oops",true,false,&folders,&verify_error);
        QVERIFY2( ok, "Failure");
        QVERIFY2( folders.at(0)=="/Users/etijskens/Launcher", "Failure");
        QVERIFY2( folders.at(1)=="../oops", "Failure");
        QVERIFY2( folders.at(2)=="oops", "Failure");
        QVERIFY2( verify_error==Ok, "Failure");
    }{// not inside, but no write access
        QDir dir("/Users/etijskens/Launcher");
        ok = verify_directory(&dir);
        QVERIFY2( ok, "Failure");
        ok = verify_directory(&dir,"/oops",true,false,&folders,&verify_error);
        QVERIFY2( !ok, "Failure");
        QVERIFY2( verify_error==Ok, "Failure");
    }


    {
        QDir dir("/Users/etijskens/Launcher");
        ok = verify_directory(&dir,"jobs/hello_world",false,true,&folders,&verify_error);
        QVERIFY2( ok, "Failure");
        QVERIFY2( folders.at(0)=="/Users/etijskens/Launcher", "Failure");
        QVERIFY2( folders.at(1)=="jobs", "Failure");
        QVERIFY2( folders.at(2)=="hello_world", "Failure");
        ok = verify_file(&dir,"pbs.sh");
        QVERIFY2( ok, "Failure");
        QVERIFY2( verify_error==Ok, "Failure");
    }

    QDir("/Users/etijskens/Launcher/oops").removeRecursively();
    QDir("/Users/etijskens/oops").removeRecursively();
}

QTEST_APPLESS_MAIN(ToolboxTest)

#include "toolbox_test.moc"
