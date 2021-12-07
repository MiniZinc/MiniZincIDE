#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QtWebSockets/QtWebSockets>
#include <QJsonValue>
#include <QElapsedTimer>

class VisConnector : public QObject {
    Q_OBJECT

private:
    QString _label;
    QStringList _roots;
    QList<QWebSocket*> _clients;
    QJsonArray _history;
    QUrl _url;

    friend class Server;
public:
    explicit VisConnector(QObject *parent = nullptr) : QObject(parent) {}
    ~VisConnector();

    QUrl url() const { return _url; }

signals:
    void receiveMessage(const QJsonDocument& message);
public slots:
    void broadcastMessage(const QJsonDocument& message);

private slots:
    void newWebSocketClient(QWebSocket* s);
    void webSocketClientDisconnected();
    void webSocketMessageReceived(const QString& message);
};

///
/// \brief HTTP and WebSocket server for web visualisation
///
class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    ~Server();
    QString address() const { return http->serverAddress().toString(); }
    quint16 port() const { return http->serverPort(); }

    VisConnector* addConnector(const QString& label, const QStringList& roots);

    bool sendToLastClient(const QJsonDocument& doc);

signals:
    void solve(const QString& model, const QStringList& data, const QVariantMap& options);

private slots:
    void newHttpClient();
    void newWebSocketClient();
    void webSocketClientDisconnected();

private:
    QTcpServer* http;
    QWebSocketServer* ws;
    QList<VisConnector*> connectors;
    QList<QWebSocket*> clients;
};

#endif // SERVER_H
