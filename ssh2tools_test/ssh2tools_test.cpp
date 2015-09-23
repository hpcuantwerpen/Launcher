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
    void testCase3();
    void testCase4();
};

Ssh2_test::Ssh2_test()
{
}

void Ssh2_test::testCase1()
{
    ssh2::Session s;
    s.setLoginNode("login.hpc.uantwerpen.be");
    s.setUsername("vsc20170");
    s.setPrivatePublicKeyPair("/Users/etijskens/.ssh/id_rsa_npw");
    s.open();
    s.execute("uptime" );
    s.execute("ls");
    s.execute("hostname -f");
    s.execute("ls -l python");
//    s.execute("mkdir -p /scratch/antwerpen/201/vsc20170/a/b/c");
//    s.execute("ll /scratch/antwerpen/201/vsc20170/a/b");


    s.close();
    QVERIFY2(true, "Failure");
}

void Ssh2_test::testCase2()
{
    ssh2::Session s;
    s.setLoginNode("login.hpc.uantwerpen.be");
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

void Ssh2_test::testCase3()
{
    ssh2::Session s;
    s.setLoginNode("login.hpc.uantwerpen.be");
    s.setUsername("vsc20170");
    s.setPrivatePublicKeyPair("/Users/etijskens/.ssh/id_rsa_npw");
    //s.open();
    ssh2::Session::autoOpen=true;
    ssh2::Session::autoClose=true;
    QString cmd("echo $VSC_SCRATCH");
    s.execute(cmd);
    //s.execute("/bin/touch /scratch/antwerpen/201/vsc20170/franky");
    QString remote = s.qout().trimmed();
            remote.append("/a/b/c");
    cmd = QString("mkdir -p ").append(remote);
    s.execute(cmd);
    s.execute("tree /scratch/antwerpen/201/vsc20170/a");
    s.execute("ls -l /scratch/antwerpen/201/vsc20170/a/b/c");
    QString local ("/Users/etijskens/Launcher_pp/jobs/subfolder/job0/pbs.sh");
//    s.scp_file_put("/Users/etijskens/Launcher_pp/jobs/subfolder/job0/pbs.sh"
//                  ,"/scratch/antwerpen/201/vsc20170/a/b/c/pbs.sh" );
    s.sftp_put_file("/Users/etijskens/Launcher_pp/jobs/subfolder/job0/pbs.sh"
                   ,"/scratch/antwerpen/201/vsc20170/a/b/c/pbs.sh");
    s.sftp_get_file("/Users/etijskens/Launcher_pp/jobs/subfolder/job0/pbs1.sh"
                   ,"/scratch/antwerpen/201/vsc20170/a/b/c/pbs.sh");

    QVERIFY2(true, "Failure");
}

void Ssh2_test::testCase4()
{
    ssh2::Session s;
    s.setLoginNode("login.hpc.uantwerpen.be");
    s.setUsername("vsc20170");
    s.setPrivatePublicKeyPair("/Users/etijskens/.ssh/id_rsa_npw");
    ssh2::Session::autoOpen=true;

    QDir("/Users/etijskens/Launcher_pp/jobs/subfolder/test2").removeRecursively();
    s.execute("rm -rf /scratch/antwerpen/201/vsc20170/test");

    s.sftp_put_dir("/Users/etijskens/Launcher_pp/jobs/subfolder/test"
                  ,            "/scratch/antwerpen/201/vsc20170/test");
    s.sftp_get_dir("/Users/etijskens/Launcher_pp/jobs/subfolder/test2"
                  ,            "/scratch/antwerpen/201/vsc20170/test");
    QVERIFY2( QFileInfo("/Users/etijskens/Launcher_pp/jobs/subfolder/test2"        ).exists(), "Failure");
    QVERIFY2( QFileInfo("/Users/etijskens/Launcher_pp/jobs/subfolder/test2"        ).isDir (), "Failure");
    QVERIFY2( QFileInfo("/Users/etijskens/Launcher_pp/jobs/subfolder/test2/a.txt"  ).exists(), "Failure");
    QVERIFY2( QFileInfo("/Users/etijskens/Launcher_pp/jobs/subfolder/test2/a.txt"  ).isFile(), "Failure");
    QVERIFY2( QFileInfo("/Users/etijskens/Launcher_pp/jobs/subfolder/test2/b"      ).exists(), "Failure");
    QVERIFY2( QFileInfo("/Users/etijskens/Launcher_pp/jobs/subfolder/test2/b"      ).isDir (), "Failure");
    QVERIFY2( QFileInfo("/Users/etijskens/Launcher_pp/jobs/subfolder/test2/b/b.txt").exists(), "Failure");
    QVERIFY2( QFileInfo("/Users/etijskens/Launcher_pp/jobs/subfolder/test2/b/b.txt").isFile(), "Failure");
    QVERIFY2(true, "Failure");
}



QTEST_APPLESS_MAIN(Ssh2_test)

#include "ssh2tools_test.moc"
