#include "menu.h"
#include "ui_menu.h"
#include "mywidget.h"
#include "ui_mywidget.h"

menu::menu(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::menu)
    , nextWidget(nullptr)  // Initialize nextWidget pointer
    , sock(nullptr)
    , connTimeoutTimerMenu(nullptr)
{
    ui->setupUi(this);
    ui->Komunikaty->setReadOnly(true);

    // Connect buttons to their respective slots
    connect(ui->conectBtn, &QPushButton::clicked, this, &menu::connectBtnHit);
    connect(ui->joinBtn, &QPushButton::clicked, this, &menu::joinBtnHit);
    connect(ui->rulesBtn, &QPushButton::clicked, this, &menu::rulesBtnHit);
    connect(ui->nickBtn, &QPushButton::clicked, this, &menu::nickBtnHit);
    connect(ui->Wiadomosc, &QLineEdit::returnPressed, ui->nickBtn, &QPushButton::click);
}

menu::~menu()
{
    if (sock) {
        sock->abort();
        sock->deleteLater();
    }
    if (connTimeoutTimerMenu) {
        connTimeoutTimerMenu->stop();
        connTimeoutTimerMenu->deleteLater();
    }
    delete ui;
}

void menu::connectBtnHit()
{
    ui->Komunikaty->append("<b>Connecting to localhost:1234</b>");

    if (sock) {
        sock->abort();
        sock->deleteLater();
        sock = nullptr;
    }

    sock = new QTcpSocket(this);
    connTimeoutTimerMenu = new QTimer(this);
    connTimeoutTimerMenu->setSingleShot(true);

    connect(connTimeoutTimerMenu, &QTimer::timeout, this, [this]() {
        if (sock) {
            sock->abort();
            sock->deleteLater();
            sock = nullptr;
        }
        connTimeoutTimerMenu->deleteLater();
        connTimeoutTimerMenu = nullptr;
        ui->Komunikaty->append("<b>Connection timed out</b>");
        QMessageBox::critical(this, "Error", "Connection timed out");
    });

    connect(sock, &QTcpSocket::connected, this, &menu::socketConnected);
    connect(sock, &QTcpSocket::disconnected, this, &menu::socketDisconnected);
    connect(sock, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &menu::socketError);
    connect(sock, &QTcpSocket::readyRead, this, &menu::socketReadable);

    sock->connectToHost("localhost", 1234);
    connTimeoutTimerMenu->start(300000);  // 5-minute timeout
}

void menu::nickBtnHit()
{
    auto txt = ui->Wiadomosc->text().trimmed();
    if (!txt.isEmpty() && sock && sock->state() == QTcpSocket::ConnectedState) {
        sock->write((txt + '\n').toUtf8());
        ui->Wiadomosc->clear();
        ui->Wiadomosc->setFocus();
    } else {
        ui->Komunikaty->append("<b>Error:</b> Not connected to the server");
    }
}

void menu::socketConnected()
{
    ui->Komunikaty->append("<b>Connected to server</b>");
    updateJoinButtonState();
}

void menu::joinBtnHit()
{
    QString message = "0;" + nickname + ";";
    qDebug() << message;
    sock->write(message.toUtf8());

    if (!nextWidget) {
        nextWidget = new MyWidget(sock, &nickname, this);
    }
    this->hide();   // Hide current view
    nextWidget->show();  // Show new view
}

void menu::rulesBtnHit()
{
    QMessageBox::information(this, "Rules", "Here you can show the game rules or any specific information.");
}

void menu::socketError(QTcpSocket::SocketError err)
{
    if (err == QTcpSocket::RemoteHostClosedError) {
        return;
    }
    if (connTimeoutTimerMenu) {
        connTimeoutTimerMenu->stop();
        connTimeoutTimerMenu->deleteLater();
        connTimeoutTimerMenu = nullptr;
    }
    QMessageBox::critical(this, "Socket Error", sock ? sock->errorString() : "Unknown error");
    ui->Komunikaty->append("<b>Socket error: " + (sock ? sock->errorString() : "Unknown error") + "</b>");
}

void menu::socketReadable()
{
    if (!sock) return;

    QByteArray ba = sock->readAll();
    QString message = QString::fromUtf8(ba).trimmed();
    QStringList messages = message.split(';', Qt::SkipEmptyParts);
    ui->Komunikaty->append(message);
    if (messages.size() >= 2) {
        QString currentType = messages[0];
        QString currentText = messages[1];
        if (currentType == "0") {
            ui->Komunikaty->append(currentText);
        } else if (currentType == "5") {
            nickname = currentText;
        }
    } else {
        ui->Komunikaty->append("<b>Received:</b> " + message);
    }
}

void menu::socketDisconnected()
{
    ui->joinBtn->setEnabled(false);  // Disable join button
    if (sock) {
        sock->abort();
        sock->deleteLater();
        sock = nullptr;
    }
    ui->Komunikaty->append("<b>Disconnected from server</b>");
}

void menu::updateJoinButtonState()
{
    bool enableButton = sock && sock->state() == QTcpSocket::ConnectedState;
    ui->joinBtn->setEnabled(enableButton);
}
