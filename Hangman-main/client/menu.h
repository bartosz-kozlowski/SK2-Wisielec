#ifndef MENU_H
#define MENU_H

#include <QDialog>
#include <QTcpSocket>
#include <QTimer>
#include <QMessageBox>

class MyWidget; // Forward declaration for MyWidget

namespace Ui {
class menu;
}

class menu : public QDialog
{
    Q_OBJECT

public:
    explicit menu(QWidget *parent = nullptr);
    ~menu();

private slots:
    void nickBtnHit();
    void joinBtnHit();
    void rulesBtnHit();
    void socketError(QTcpSocket::SocketError err);
    void socketReadable();
    void socketConnected();   // Ensure this line exists
    void socketDisconnected();
    void connectBtnHit();


private:
    Ui::menu *ui;
    QTcpSocket *sock = nullptr;           // Wskaźnik na socket TCP
    QTimer *connTimeoutTimerMenu = nullptr;   // Timer do obsługi timeoutu połączenia
    MyWidget *nextWidget = nullptr;       // Wskaźnik na kolejny widok
    QString nickname;                     // Przechowywanie nicku użytkownika

    void updateJoinButtonState();        // Funkcja pomocnicza do zarządzania stanem przycisków
};

#endif // MENU_H
