#include "server.h"

#include <QWebSocket>
#include <QJsonDocument>
#include <QDesktopServices>

#include "ideutils.h"
#include "exception.h"

VisConnector::~VisConnector()
{
    for (auto* c : _clients) {
        delete c;
    }
}

void VisConnector::newWebSocketClient(QWebSocket* socket)
{
    _clients << socket;
    connect(socket, &QWebSocket::disconnected, this, &VisConnector::webSocketClientDisconnected);
    connect(socket, &QWebSocket::textMessageReceived, this, &VisConnector::webSocketMessageReceived);
    QJsonObject obj({{"event", "init"}, {"windows", _windows}});
    auto json = QString::fromUtf8(QJsonDocument(obj).toJson());
    socket->sendTextMessage(json);
}

void VisConnector::webSocketClientDisconnected()
{
    auto* client = qobject_cast<QWebSocket*>(sender());
    if (client) {
        _clients.removeAll(client);
        client->deleteLater();
    }
}

void VisConnector::addWindow(const QString& url, const QJsonValue& userData)
{
    _windows << QJsonObject({{"url", url}, {"userData", userData}});
    _solutions.append(QJsonArray());
    QJsonObject obj({{"event", "window"},
                     {"url", url},
                     {"userData", userData}});
    broadcastMessage(QJsonDocument(obj));
}

void VisConnector::addSolution(const QJsonArray& items, qint64 time)
{
    if (_solutions.isEmpty() || items.size() != _solutions.size()) {
        assert(false);
        return;
    }
    auto solIndex = _solutions.first().size();
    for (auto i = 0; i < items.size(); i++) {
        QJsonObject it({{"time", time},
                        {"data", items[i]},
                        {"index", solIndex}});
        _solutions[i].append(it);
    }
    QJsonObject obj({{"event", "solution"},
                     {"time", time},
                     {"items", items},
                     {"index", solIndex}});
    broadcastMessage(QJsonDocument(obj));
}

void VisConnector::setFinalStatus(const QString& status, qint64 time)
{
    _finalStatus = QJsonObject({{"time", time}, {"status", status}});
    QJsonObject obj({{"event", "status"},
                     {"time", time},
                     {"status", status}});
    broadcastMessage(QJsonDocument(obj));
}

void VisConnector::setFinished(qint64 time)
{
    _finishTime = time;
    QJsonObject obj({{"event", "finish"},
                     {"time", time}});
    broadcastMessage(QJsonDocument(obj));
}

void VisConnector::broadcastMessage(const QJsonDocument& message)
{
    auto json = QString::fromUtf8(message.toJson());
    for (auto* client : _clients) {
        client->sendTextMessage(json);
    }
}

void VisConnector::webSocketMessageReceived(const QString& message)
{
    auto* socket = qobject_cast<QWebSocket*>(sender());

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(message.toUtf8(), &error);
    auto msg = json.object();
    auto event = msg["event"].toString();
    if (event == "solve") {
        auto mf = msg["modelFile"].toString();
        QStringList dfs;
        for (auto v : msg["dataFiles"].toArray()) {
            dfs << v.toString();
        }
        auto opts = msg["options"].toObject().toVariantMap();
        emit solveRequested(mf, dfs, opts);
    } else if (event == "getNumSolutions") {
        assert(!_solutions.isEmpty());
        QJsonObject obj({{"event", "response"},
                         {"id", msg["id"]},
                         {"window", msg["window"]},
                         {"payload", _solutions.isEmpty() ? 0 : _solutions.first().size()}});
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson()));
    } else if (event == "getSolution") {
        auto windowIndex = msg["window"].toInt();
        if (windowIndex < 0 || windowIndex >= _solutions.size()) {
            QJsonObject obj({{"event", "error"},
                             {"id", msg["id"]},
                             {"window", msg["window"]},
                             {"message", "Invalid window."}});
            socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson()));
            return;
        }
        auto idx = msg["index"].toInt();
        if (idx < 0) {
            idx += _solutions[windowIndex].size();
        }
        if (idx < 0 || idx >= _solutions[windowIndex].size()) {
            QJsonObject obj({{"event", "error"},
                             {"id", msg["id"]},
                             {"window", msg["window"]},
                             {"message", "Index out of range."}});
            socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson()));
            return;
        }
        QJsonObject obj({{"event", "response"},
                         {"id", msg["id"]},
                         {"window", msg["window"]},
                         {"payload", _solutions[windowIndex][idx]}});
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson()));
    } else if (event == "getAllSolutions") {
        auto windowIndex = msg["window"].toInt();
        if (windowIndex < 0 || windowIndex >= _solutions.size()) {
            QJsonObject obj({{"event", "error"},
                             {"id", msg["id"]},
                             {"window", msg["window"]},
                             {"message", "Invalid window."}});
            socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson()));
            return;
        }
        QJsonObject obj({{"event", "response"},
                         {"id", msg["id"]},
                         {"window", windowIndex},
                         {"payload", _solutions[windowIndex]}});
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson()));
    } else if (event == "getFinalStatus") {
        QJsonObject obj({{"event", "response"},
                         {"id", msg["id"]},
                         {"window", msg["window"]},
                         {"payload", _finalStatus}});
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson()));
    } else if (event == "getFinishTime") {
        QJsonObject obj({{"event", "response"},
                         {"id", msg["id"]},
                         {"window", msg["window"]},
                         {"payload", _finishTime == -1 ? QJsonValue() : _finishTime}});
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson()));
    } else {
        QJsonObject obj({{"event", "error"},
                         {"id", msg["id"]},
                         {"window", msg["window"]},
                         {"message", QString("Unknown event '%1'.").arg(event)}});
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson()));
    }
}

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

Server::~Server()
{
    for (auto* c : connectors) {
        delete c;
    }
}

VisConnector* Server::addConnector(const QString& label, const QStringList& roots)
{
    auto* c = new VisConnector(this);
    c->_label = label;
    c->_roots = roots;
    c->_url.setScheme("http");
    c->_url.setHost(address());
    c->_url.setPort(port());
    c->_url.setPath(QString("/%1").arg(connectors.size()));
    connectors << c;
    return c;
}

void Server::clear()
{
    for (auto* c : connectors) {
        delete c;
    }
    connectors.clear();
}

bool Server::sendToLastClient(const QJsonDocument& message)
{
    auto json = QString::fromUtf8(message.toJson());
    for (auto it = clients.rbegin(); it != clients.rend(); it++) {
        if ((*it)->isValid()) {
            (*it)->sendTextMessage(json);
            return true;
        }
    }
    return false;
}

void Server::newHttpClient()
{
    auto socket = http->nextPendingConnection();
    QTextStream ts(socket);
    socket->waitForReadyRead();
    auto parts = ts.readAll().split(QRegularExpression("\\s+"));
    if (parts.length() < 3 || parts[0] != "GET") {
        return;
    }

    // Connector script
    if (parts[1] == "/minizinc-ide.js") {
        QFile f(":/server/server/connector.js");
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream c(&f);
        ts << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: text/javascript\r\n"
           << "\r\n"
           << c.readAll();
        socket->close();
        return;
    }

    QRegularExpression re("^/?(\\d+)(/.*)?$");
    auto p = QUrl::fromPercentEncoding(parts[1].toUtf8());
    auto match = re.match(p);
    if (!match.hasMatch()) {
        ts << "HTTP/1.1 404 OK\r\n"
           << "\r\n"
           << "File not found.";
        socket->close();
        return;
    }

    auto index = match.captured(1).toInt();
    if (index >= connectors.size()) {
        ts << "HTTP/1.1 404 OK\r\n"
           << "\r\n"
           << "File not found.";
        socket->close();
        return;
    }

    auto path = match.captured(2);
    // Window management page
    if (path.isEmpty() || path == "index.html") {
        QString title = connectors[index]->_label.toHtmlEscaped();
        QFile f(":/server/server/index.html");
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream c(&f);
        ts << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: text/html\r\n"
           << "\r\n"
           << c.readAll().arg(ws->serverUrl().toString()).arg(index).arg(title);
        socket->close();
        return;
    }

    // Static file server
    QStringList base_dirs({":/server/server"});
    base_dirs << connectors[index]->_roots;
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
    auto path = socket->requestUrl().path();
    bool ok = false;
    auto index = path.right(path.size() - 1).toInt(&ok);
    if (!ok || index >= connectors.size()) {
        socket->close();
        return;
    }
    auto* c = connectors[index];
    c->newWebSocketClient(socket);
    clients << socket;
    connect(socket, &QWebSocket::disconnected, this, &Server::webSocketClientDisconnected);
}

void Server::webSocketClientDisconnected()
{
    auto* client = qobject_cast<QWebSocket*>(sender());
    if (client) {
        clients.removeAll(client);
    }
}
