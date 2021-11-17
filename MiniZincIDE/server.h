#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QtWebSockets/QtWebSockets>
#include <QJsonValue>
#include <QElapsedTimer>

///
/// \brief HTTP and WebSocket server for web visualisation
///
class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    QString address() const { return http->serverAddress().toString(); }
    quint16 port() const { return http->serverPort(); }
    QUrl url() const;
    int clientCount() const { return clients.size(); }
    const QStringList& webroots() const { return roots; }
    void setWebroots(const QStringList& webroots) { roots = webroots; }
    void clearHistory() { history.clear(); }
    void openUrl(bool force = false);
signals:
    void receiveMessage(const QJsonDocument& message);
public slots:
    void broadcastMessage(const QJsonDocument& message);

private slots:
    void newHttpClient();
    void newWebSocketClient();
    void webSocketClientDisconnected();
    void webSocketMessageReceived(const QString& message);

private:
    QTcpServer* http;
    QWebSocketServer* ws;
    QList<QWebSocket*> clients;
    QStringList history;
    QStringList roots;
    QElapsedTimer lastOpened;
};

#endif // SERVER_H
