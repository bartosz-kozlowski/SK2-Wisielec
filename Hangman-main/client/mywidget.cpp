#include "mywidget.h"
#include "ui_mywidget.h"
#include <iostream>
#include <QMessageBox>
#include <QDir>

#include <QGraphicsPixmapItem>

using namespace std;

MyWidget::MyWidget(QWidget *parent) : QWidget(parent), ui(new Ui::MyWidget), scene(new QGraphicsScene(this)) {
    ui->setupUi(this);

    // Ustawienia pól jako nieedytowalne
    ui->Komunikaty->setReadOnly(true);
    ui->KomunikatyGeneral->setReadOnly(true);
    ui->Haslo->setReadOnly(true);
    ui->Time->setReadOnly(true);

    this->setStyleSheet(
        "QPushButton {"
        "   background-color: #f39c12;"
        "   color: white;"
        "   font-weight: bold;"
        "   border: 2px solid #d35400;"
        "   border-radius: 8px;"
        "   padding: 2px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #f1c40f;"
        "   border-color: #e67e22;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #e67e22;"
        "   border-color: #d35400;"
        "}"
        );

    // Ustawienie sceny dla QGraphicsView
    ui->Image->setScene(scene);

    // Połączenie sygnałów z przyciskami i polami tekstowymi
    connect(ui->conectBtn, &QPushButton::clicked, this, &MyWidget::connectBtnHit);
    connect(ui->sendBtn, &QPushButton::clicked, this, &MyWidget::sendBtnHit);
    connect(ui->exitBtn, &QPushButton::clicked, this, &MyWidget::exitBtnHit);
    connect(ui->nickBtn, &QPushButton::clicked, this, &MyWidget::nickBtnHit);
    connect(ui->disconnectBtn, &QPushButton::clicked, this, &MyWidget::socketDisconnected);
    connect(ui->joinBtn, &QPushButton::clicked, this, &MyWidget::joinBtnHit);
    connect(ui->rulesBtn, &QPushButton::clicked, this, &MyWidget::rulesBtnHit);
    connect(ui->Wiadomosc, &QLineEdit::returnPressed, ui->sendBtn, &QPushButton::click);
    connect(ui->Nick, &QLineEdit::returnPressed, ui->nickBtn, &QPushButton::click);

    ui->Wiadomosc->setMaxLength(1);

    //ograniczenie pol tekstowych tak zeby przyjmowaly tylko litery
    QRegularExpression regExpMess("[a-zA-Z]");
    QRegularExpressionValidator *validatorMess = new QRegularExpressionValidator(regExpMess, this);
    ui->Wiadomosc->setValidator(validatorMess);

    QRegularExpression regExpNick("[a-zA-Z]+"); // Akceptuje tylko litery
    QRegularExpressionValidator *validatorNick = new QRegularExpressionValidator(regExpNick, this);
    ui->Nick->setValidator(validatorNick);

    ui->connectGroup->setEnabled(true);
    ui->talkGroup->setEnabled(false);
    ui->NickGroup->setEnabled(false);
    ui->Menu->setEnabled(false);
}

MyWidget::~MyWidget() {
    if (sock)
        sock->close();
    delete scene; // Usuwanie sceny
    delete ui;
}

void MyWidget::connectBtnHit() {
    ui->connectGroup->setEnabled(false);
    ui->KomunikatyGeneral->append("<b>Łączenie się z wybranym adresem</b>");
 //   ui->talkGroup->setEnabled(false);
    ui->NickGroup->setEnabled(true);
//    ui->Menu->setEnabled(false);
    if (sock)
        delete sock;

    sock = new QTcpSocket(this);
    connTimeoutTimer = new QTimer(this);
    connTimeoutTimer->setSingleShot(true);

    connect(connTimeoutTimer, &QTimer::timeout, [&]() {
        sock->abort();
        sock->deleteLater();
        sock = nullptr;
        connTimeoutTimer->deleteLater();
        connTimeoutTimer = nullptr;
        ui->connectGroup->setEnabled(true);
        ui->NickGroup->setEnabled(false);
        ui->KomunikatyGeneral->append("<b>Nie udało się połączyć z serwerem</b>");
        QMessageBox::critical(this, "Błąd", "Przekroczono czas oczekiwania na połączenie");
    });
    //set size unlimitted
    ui->Wiadomosc->setMaxLength(1);
    connect(sock, &QTcpSocket::connected, this, &MyWidget::socketConnected);
    connect(sock, &QTcpSocket::disconnected, this, &MyWidget::socketDisconnected);
    connect(sock, &QTcpSocket::errorOccurred, this, &MyWidget::socketError);
    connect(sock, &QTcpSocket::readyRead, this, &MyWidget::socketReadable);

    sock->connectToHost(ui->hostLineEdit->text(),1234);
    connTimeoutTimer->start(3000);
}

void MyWidget::socketConnected() {
    connTimeoutTimer->stop();
    connTimeoutTimer->deleteLater();
    connTimeoutTimer = nullptr;
    ui->NickGroup->setEnabled(true);
}

void MyWidget::socketDisconnected() {
    ui->talkGroup->setEnabled(false);
    ui->Menu->setEnabled(false);
    ui->NickGroup->setEnabled(false);
    ui->connectGroup->setEnabled(true);
    ui->Wiadomosc->clear();
    ui->Ranking->clear();
    ui->Nick->clear();
    ui->Komunikaty->clear();
    ui->Image->scene()->clear();
    ui->KomunikatyGeneral->clear();
    ui->Haslo->clear();
    ui->Time->clear();
    if (sock)
        sock->close();
}

void MyWidget::socketError(QTcpSocket::SocketError err) {
    if (err == QTcpSocket::RemoteHostClosedError) {
        QMessageBox::critical(this, "Błąd", "Połączenie zostało przerwane.");
        return;
    }

    if (connTimeoutTimer) {
        connTimeoutTimer->stop();
        connTimeoutTimer->deleteLater();
        connTimeoutTimer = nullptr;
    }

    QMessageBox::critical(this, "Błąd", sock->errorString());
    ui->KomunikatyGeneral->append("<b>Wystąpił błąd połączenia: " + sock->errorString() + "</b>");
    //ui->KomunikatyGeneral->append("<b>Socket error: " + sock->errorString() + "</b>");
    ui->talkGroup->setEnabled(false);
    ui->NickGroup->setEnabled(false);
    ui->Menu->setEnabled(false);
    ui->connectGroup->setEnabled(true);
}
void MyWidget::setCommunicate(QString currentText){
    ui->Komunikaty->append(currentText);
}
void MyWidget::setRanking(QString currentText){
    // Rozdzielenie currentText na linie
    QStringList lines = currentText.split("\n");

    // Aktualizacja Ranking2
    ui->Ranking->clear();
    // Rysowanie wisielca dla każdej linii, pomijając puste linie
    for (const QString &line : lines) {
        if (!line.isEmpty()) { // Pomijanie pustych linii
            QStringList parts = line.split(":");
          //  cout << parts[0].toStdString() << endl;
            if (parts.size() > 2) {
                QString imagePath = "images/Wisielec" + parts[2] + ".png";
                QPixmap pixmap(imagePath);

                if (!pixmap.isNull()) {
                    QPixmap scaledPixmap = pixmap.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    QIcon icon(scaledPixmap);
                    QListWidgetItem *item = new QListWidgetItem(icon, QString("%1 : %2").arg(parts[0], parts[1]));
                        // Ustawienie pogrubionej czcionki
                        QFont boldFont;
                        boldFont.setBold(true);
                        item->setFont(boldFont);

                        // Ustawienie koloru w zależności od wartości parts[3]
                        if (parts[3].trimmed() == "1") {
                            QBrush greenBrush(Qt::magenta);
                            item->setForeground(greenBrush); // Zielony dla aktywnych graczy
                        } else {
                            QBrush blackBrush(Qt::black);
                            item->setForeground(blackBrush); // Czarny dla nieaktywnych graczy
                        } 
                    ui->Ranking->addItem(item);
                } else {
                    ui->Ranking->addItem("Nie udało się załadować obrazu: " + imagePath);
                }
            }
        }
    }
}
void MyWidget::setHaslo(QString currentText){
    ui->Haslo->clear();
    if (!currentText.contains("-")) {
        ui->Wiadomosc->setReadOnly(true);
    }
    ui->Haslo->append(currentText);
}
void MyWidget::setImage(QString currentText){
    if (currentText == "7") {
        // Blokowanie wysyłania wiadomości, gdy gracz zostaje powieszony
        ui->Wiadomosc->setReadOnly(true);
    }else if(currentText == "0"){
        ui->Wiadomosc->setReadOnly(false);
    }
    QString imagePath = "images/Wisielec" + currentText + ".png";
    QPixmap pixmap(imagePath);

    if (!pixmap.isNull()) {
        // Wyczyszczenie sceny i dodanie nowego obrazu
        scene->clear();
        pixmap = pixmap.scaled(351, 192, Qt::KeepAspectRatio); // Skalowanie do 351x192 pikseli
        scene->addPixmap(pixmap);

        // Ustawienie sceny na rozmiar obrazka
        scene->setSceneRect(pixmap.rect());
    } else {
        ui->Komunikaty->append("<b>Nie udało się załadować obrazu:</b> " + imagePath);
    }

    // Skalowanie obrazu w widoku
    ui->Image->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}
void MyWidget::setTime(QString currentText){
    ui->Time->clear();
    ui->Time->append(currentText);
}
void MyWidget::setWaitingRoom(QString currentText){
    //poczekalnia
    //qDebug() << "Katalog roboczy aplikacji:" << QDir::currentPath();
    QString imagePath = "images/waitingCat.jpg";
    QPixmap pixmap(imagePath);
    if (!pixmap.isNull()) {
        scene->clear();
        pixmap = pixmap.scaled(351, 192, Qt::KeepAspectRatio);
        scene->addPixmap(pixmap);
        scene->setSceneRect(pixmap.rect());
    } else {
        ui->Komunikaty->append("<b>Nie udało się załadować obrazu:</b> " + imagePath);
    }
    ui->Time->clear();
    // Skalowanie obrazu w widoku
    ui->Image->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    ui->Komunikaty->append(currentText);
    ui->Wiadomosc->setReadOnly(false);
}
void MyWidget::setNickname(QString currentText){
    //ustawienie nicku klienta na taki zakceptowany przez serwer
    //qDebug() << "Ustawiam nick na:" << currentText;
    nickname = currentText;
    ui->Nick->setText(nickname);
    ui->NickGroup->setEnabled(false);
    ui->Menu->setEnabled(true);
}
void MyWidget::socketReadable() {
    static QByteArray buffer; // Bufor do przechowywania nieprzetworzonych danych
    buffer.append(sock->readAll()); // Dodanie nowych danych do bufora

    while (true) {
        int delimiterIndex = buffer.indexOf('&'); // Znalezienie pierwszego separatora
        if (delimiterIndex == -1) {
            // Jeśli brak pełnego komunikatu, przerwij pętlę
            break;
        }

        // Wyodrębnienie pełnego komunikatu
        QByteArray singleMessage = buffer.left(delimiterIndex);
        buffer.remove(0, delimiterIndex + 1); // Usuń przetworzony komunikat z bufora

        QString message = QString::fromUtf8(singleMessage).trimmed();

        // Podział na typ i treść komunikatu
        int colonIndex = message.indexOf(';');
        if (colonIndex == -1) {
            //qWarning() << "Niepoprawny format komunikatu: " << message;
            continue; // Pomijamy niepoprawne komunikaty
        }

        QString currentType = message.left(colonIndex).trimmed(); // Typ komunikatu
        QString currentText = message.mid(colonIndex + 1).trimmed(); // Treść komunikatu

        // Obsługa komunikatu na podstawie jego typu
        if (ui->talkGroup->isEnabled()) {
            if (currentType == "0") {
                setCommunicate(currentText);
            } else if (currentType == "1") {
                setRanking(currentText);
            } else if (currentType == "2") {
                setHaslo(currentText);
            } else if (currentType == "3") {
                setImage(currentText);
            } else if (currentType == "4") {
                setTime(currentText);
            } else if (currentType == "6") {
                setWaitingRoom(currentText);
            }
        } else if (currentType == "01") {
            ui->KomunikatyGeneral->append(currentText);
        } else if (currentType == "02") {
            setNickname(currentText);
        } else {
            //qWarning() << "Nieobsługiwany typ komunikatu: " << currentType << "  " << currentText;
        }
    }
}



void MyWidget::sendBtnHit() {
    auto txt = ui->Wiadomosc->text().trimmed();
    if (txt.isEmpty()) // Jeśli pole tekstowe jest puste, wyjdź
        return;

    // Wysyłanie wiadomości przez socket
    sock->write((txt + ";").toUtf8());

    // Czyszczenie pola tekstowego i ustawienie focusa
    ui->Wiadomosc->clear();
    ui->Wiadomosc->setFocus();
}
void MyWidget::nickBtnHit() {
    auto txt = ui->Nick->text().trimmed();
    if (txt.isEmpty()) // Jeśli pole tekstowe jest puste, wyjdź
        return;

    // Wysyłanie wiadomości przez socket
    sock->write((txt + ";").toUtf8());
    ui->Nick->clear();
    ui->Nick->setFocus();
}
void MyWidget::joinBtnHit() {
    ui->Menu->setEnabled(false);
    // Wyślij do serwera, aby dołączył użytkownika
    sock->write("USER JOIN;");
    ui->talkGroup->setEnabled(true);
}

void MyWidget::exitBtnHit() {
    ui->Menu->setEnabled(true);
    sock->write("USER LEFT;");
    ui->talkGroup->setEnabled(false);
    ui->Wiadomosc->clear();
    ui->Ranking->clear();
    ui->Time->clear();
    //ui->Nick->clear();
    ui->Komunikaty->clear();
    ui->Image->scene()->clear();
    ui->KomunikatyGeneral->clear();
    ui->Haslo->clear();
}
void MyWidget::rulesBtnHit(){
    QMessageBox::about(this, "ZASADY",
                      "===== WISIELEC =====\n\n"
                      "1. Łączysz się z serwerem i wybierasz unikalny nick.\n"
                      "2. Dołączasz do gry – jeden pokój, który można dołączyć w dowolnym momencie.\n"
                      "3. Za każdą poprawną literę zdobywasz punkt, a gra trwa do zakończenia wisielca.\n"
                      "4. Gra kończy się, gdy pozostanie jeden aktywny gracz.\n"
                      "5. Każda tura trwa 1,5 minuty – jeśli odgadniesz hasło, pozostali mają dodatkowy czas.\n"
                      "6. W trakcie tury wyświetlana jest kategoria hasła.\n"
                      "7. W przypadku rozłączenia graczy, tura trwa do końca.\n");
}

