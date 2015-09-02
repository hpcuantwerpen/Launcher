#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <launcher.h>
#include <cfg.h>

#include <iostream>


namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void setIgnoreSignals( bool ignore=true );
    NodesetInfo const& nodesetInfo() const;

private:
    void print_( std::string const& signature ) const;
    template <class T>
    void print_( std::string const& signature, T arg1 ) const {
        this->print_(signature);
        std::cout << "    arg1 = " << arg1 << std::endl;
    }

private slots:
    void on_wCluster_currentIndexChanged(const QString &arg1);

    void on_wNodeset_currentTextChanged(const QString &arg1);

    void on_wRequestNodes_clicked();

    void on_wRequestCores_clicked();

    void on_wAutomaticRequests_toggled(bool checked);

    void on_wNNodes_valueChanged(int arg1);

    void on_wNCoresPerNode_valueChanged(int arg1);

    void on_wPages_currentChanged(int index);

private:
    Ui::MainWindow *ui;
    Launcher launcher_;
    bool ignoreSignals_;
    int previousPage_;
};

#define PRINT0_AND_CHECK_IGNORESIGNAL( signature ) \
    this->print_( signature ); \
    if( this->ignoreSignals_ ) return;

#define PRINT1_AND_CHECK_IGNORESIGNAL( signature, arg1 ) \
    this->print_( signature, (arg1) ); \
    if( this->ignoreSignals_ ) return;

#define FORWARDING \
    std::cout << "    forwarding" << std::flush;


#endif // MAINWINDOW_H
