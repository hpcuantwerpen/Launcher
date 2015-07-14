#include <QtTest>

#include "cfg.h"


#include <iostream>
using std::logic_error;

class CfgTest : public QObject
{
    Q_OBJECT

public:
    CfgTest();

private Q_SLOTS:
    void testCase0();
    void testCase1();
};

CfgTest::CfgTest()
{
}

void CfgTest::testCase0()
{
    QVariant qi = 2;
    QVERIFY2(qi.canConvert(QMetaType::Int), "Failure");
    QVERIFY2(qi.canConvert(QMetaType::Double), "Failure");
    std::cout << qi.value<int>() << std::endl;
    std::cout << qi.value<double>() << std::endl;
    qDebug() << qi.value<int>()<< "=="<<qi  .value<double>();
    qDebug()<< qi.toInt();
    QVariant qd = 2.5;
    QVERIFY2(qd.canConvert(QMetaType::Int), "Failure");
    QVERIFY2(qd.canConvert(QMetaType::Double), "Failure");
    qDebug() << qd.value<int>()<< "=="<<qd.value<double>();
    std::cout << qd.value<double>() << std::endl;
}

void CfgTest::testCase1()
{
    cfg::Item item("item0");
    QVERIFY2(item.name()=="item0", "Failure");
    QVERIFY2(item.choices()==QList<QVariant>(), "Failure");
    QVERIFY2(item.default_value()==QVariant (), "Failure");
    QVERIFY2(item.        value()==QVariant (), "Failure");
    item.set_default_value(1);
    QVERIFY2(item.default_value()==1, "Failure");
    QVERIFY2(item.        value()==1, "Failure");
    QList<QVariant> choices;
    choices.append(1);
    choices.append(3);
    std::cout << choices.first().typeName() << choices.first().type() << std::endl;
    item.set_choices(choices);
    choices.append(4);
    item.set_choices(choices);
    QVERIFY2(item.default_value()==1, "Failure");
    item.set_value(3);
    QVERIFY2(item.        value()==3, "Failure");
    try {
        item.set_value(2);
        QVERIFY2(false, "Failure");
    } catch(logic_error &e) {
        std::cout << "(Intentional) Error: " << e.what();
    }
    try {
        item.set_choices(choices,true);
        QVERIFY2(false, "Failure");
    } catch(logic_error &e) {
        std::cout << "(Intentional) Error: " << e.what();
    }
    choices.removeAt(2);
    item.set_choices(choices,true);
    item.set_value(2);
    QVERIFY2(item.value()==2, "Failure");
    try {
        item.set_value(2.5);
        QVERIFY2(false, "Failure");
    } catch(logic_error &e) {
        std::cout << "(Intentional) Error: " << e.what();
    }
    try {
        choices.first()=4;
        choices.last() =3;
        item.set_choices(choices,true);
        QVERIFY2(false, "Failure");
    } catch(logic_error &e) {
        std::cout << "(Intentional) Error: " << e.what();
    }
}

QTEST_APPLESS_MAIN(CfgTest)

#include "tst_cfg_test.moc"
