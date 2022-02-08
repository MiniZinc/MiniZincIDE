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

    QJsonArray _windows;
    QVector<QJsonArray> _solutions;
    QJsonValue _finalStatus;
    qint64 _finishTime = -1;

    QUrl _url;

    friend class Server;
public:
    explicit VisConnector(QObject *parent = nullptr) : QObject(parent) {}
    ~VisConnector();

    QUrl url() const { return _url; }

signals:
    void solveRequested(const QString& modelFile, const QStringList& dataFiles, const QVariantMap& options);

public slots:
    void addWindow(const QString& url, const QJsonValue& userData);
    void addSolution(const QJsonArray& items, qint64 time);
    void setFinalStatus(const QString& status, qint64 time);
    void setFinished(qint64 time);

private slots:
    void newWebSocketClient(QWebSocket* s);
    void webSocketClientDisconnected();
    void webSocketMessageReceived(const QString& message);
    void broadcastMessage(const QJsonDocument& message);
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
    void clear();

    bool sendToLastClient(const QJsonDocument& doc);

signals:
    void solve(const QString& model, const QStringList& data, const QVariantMap& options);

private slots:
    void newHttpClient();
    void newWebSocketClient();
    void webSocketClientDisconnected();
    void handleHttpRequest();

private:
    QTcpServer* http;
    QWebSocketServer* ws;
    QList<VisConnector*> connectors;
    QList<QWebSocket*> clients;
};

#endif // SERVER_H
