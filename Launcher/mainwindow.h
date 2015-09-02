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
    template <class T>
    void print_( std::string const& signature, T arg1) const
    {
        if( this->ignoreSignals_ ) {
            std::cout << "\nIgnoring signal:\n";
        } else {
            std::cout << '\n';
        }
        std::cout << signature << "\n    arg1 = " << arg1 << std::endl;
    }
    void print_( std::string const& signature ) const;

private slots:
    void on_wCluster_currentIndexChanged(const QString &arg1);

    void on_wNodeset_currentTextChanged(const QString &arg1);

    void on_wRequestNodes_clicked();

    void on_wRequestCores_clicked();

private:
    Ui::MainWindow *ui;
    Launcher launcher_;
    bool ignoreSignals_;
};

#endif // MAINWINDOW_H
