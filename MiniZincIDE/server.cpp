#include "server.h"

#include <QWebSocket>
#include <QJsonDocument>
#include <QDesktopServices>

#include "ideutils.h"
#include "exception.h"

Server::Server(QObject *parent) :
    QObject(parent),
    http(new QTcpServer(this)),
    ws(new QWebSocketServer("MiniZincIDE", QWebSocketServer::NonSecureMode, this))
{
    if (!http->listen(QHostAddress::LocalHost)) {
        throw new InternalError("Failed to start HTTP visualisation server");
    }
    if (!ws->listen(QHostAddress::LocalHost)) {
        throw new InternalError("Failed to start WebSocket visualisation server");
    }

    connect(http, &QTcpServer::newConnection, this, &Server::newHttpClient);
    connect(ws, &QWebSocketServer::newConnection, this, &Server::newWebSocketClient);
}

QUrl Server::url() const {
    QUrl url;
    url.setScheme("http");
    url.setHost(address());
    url.setPort(port());
    return url;
}

void Server::newHttpClient()
{
    auto socket = http->nextPendingConnection();
    QTextStream ts(socket);
    socket->waitForReadyRead();
    auto parts = ts.readAll().split(QRegExp("\\s+"));
    if (parts.length() < 3 || parts[0] != "GET") {
        return;
    }

    // Window management page
    if (parts[1] == "/" || parts[1] == "/index.html") {
        QFile f(":/server/server/index.html");
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream c(&f);
        ts << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: text/html\r\n"
           << "\r\n"
           << c.readAll().arg(ws->serverUrl().toString());
        socket->close();
        return;
    }

    // Static file server
    QStringList base_dirs({":/server/server"});
    base_dirs << roots;
    auto path = QUrl::fromPercentEncoding(parts[1].toUtf8());
    for (auto& base_dir : base_dirs) {
        auto full_path = base_dir + "/" + path;
        if (IDEUtils::isChildPath(base_dir, full_path)) {
            QFile file(full_path);
            if (!file.exists()) {
                continue;
            }
            if (!file.open(QFile::ReadOnly)) {
                ts << "HTTP/1.1 500 OK\r\n"
                   << "\r\n"
                   << "Failed to retrieve file";
                socket->close();
                return;
            }
            QMimeDatabase db;
            QMimeType mime = db.mimeTypeForFile(QFileInfo(file));
            ts << "HTTP/1.1 200 OK\r\n"
               << "Content-Type: " << mime.name() << "\r\n"
               << "\r\n";
            ts.flush();
            socket->write(file.readAll());
            socket->close();
            return;
        }
    }
    ts << "HTTP/1.1 404 OK\r\n"
       << "\r\n"
       << "File not found.";
    socket->close();
    return;
}

void Server::newWebSocketClient()
{
    auto* socket = ws->nextPendingConnection();
    connect(socket, &QWebSocket::disconnected, this, &Server::webSocketClientDisconnected);
    clients << socket;
    connect(socket, &QWebSocket::textMessageReceived, this, &Server::webSocketMessageReceived);
    for (auto& item: history) {
        // Send previous messages so client is caught up
        socket->sendTextMessage(item);
    }
}

void Server::webSocketClientDisconnected()
{
    auto* client = qobject_cast<QWebSocket*>(sender());
    if (client) {
        clients.removeAll(client);
        client->deleteLater();
    }
}

void Server::broadcastMessage(const QJsonDocument& message)
{
    auto json = QString::fromUtf8(message.toJson());
    for (auto* client : clients) {
        client->sendTextMessage(json);
    }
    history << json; // Save data so we can send to new clients
}

void Server::webSocketMessageReceived(const QString& message)
{
    QJsonParseError error;
    auto json = QJsonDocument::fromJson(message.toUtf8(), &error);
    emit receiveMessage(json);
}

void Server::openUrl(bool force)
{
    if (force || (clientCount() == 0 && (!lastOpened.isValid() || lastOpened.hasExpired(1000)))) {
        // Open if no one is connected (and don't open multiple times in rapid succession)
        lastOpened.restart();
        QDesktopServices::openUrl(url());
    }
}
