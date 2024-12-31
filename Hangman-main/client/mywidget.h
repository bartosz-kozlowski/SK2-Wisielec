#ifndef MYWIDGET_H
#define MYWIDGET_H

#include <QWidget>
#include <QTcpSocket>
#include <QTimer>
#include <QGraphicsScene>

namespace Ui {
class MyWidget;
}

class MyWidget : public QWidget {
    Q_OBJECT

public:
    explicit MyWidget(QWidget *parent = nullptr);
    ~MyWidget();

private slots:
    void connectBtnHit();
    void sendBtnHit();
    void exitBtnHit();
    void nickBtnHit();
    void rulesBtnHit();
    void joinBtnHit();
    void socketConnected();
    void socketDisconnected();
    void socketError(QTcpSocket::SocketError err);
    void socketReadable();

private:
    Ui::MyWidget *ui;
    QTcpSocket *sock = nullptr;
    QTimer *connTimeoutTimer = nullptr;

    QString nickname;
    bool isLeader=false;

    void setCommunicate(QString currentText);
    void setRanking(QString currentText);
    void setHaslo(QString currentText);
    void setImage(QString currentText);
    void setTime(QString currentText);
    void setNickname(QString currentText);
    void setWaitingRoom(QString currentText);
    // Dodanie sceny jako członka klasy
    QGraphicsScene *scene = nullptr;
};

#endif // MYWIDGET_H
