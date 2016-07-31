#include "htmlpage.h"
#include "mainwindow.h"
#include "QDebug"

#ifdef MINIZINC_IDE_HAVE_WEBENGINE

HTMLPage::HTMLPage(MainWindow* mw, QWidget *parent) :
    QWebEnginePage(parent), _mw(mw), _webChannel(new QWebChannel(this)), _mznide(new MiniZincIDEJS(this)),
    loadFinished(false)
{
    connect(this, SIGNAL(loadFinished(bool)), this, SLOT(pageLoadFinished(bool)));
    setWebChannel(_webChannel);
    _webChannel->registerObject("mznide", _mznide);
}

void
HTMLPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel, const QString &message, int lineNumber, const QString &sourceID)
{
    _mw->addOutput("<div style='color:red;'>JavaScript message: source " +sourceID + ", line no. " + QString().number(lineNumber) + ": " + message + "</div><br>\n");
}

void HTMLPage::runJs(QString js)
{
    runJavaScript(js);
}

MiniZincIDEJS::MiniZincIDEJS(HTMLPage *p)
    : QObject(p), _htmlPage(p)
{

}

void MiniZincIDEJS::selectSolution(int n)
{
    _htmlPage->selectSolution(n);
}

#else

#include <QWebFrame>

HTMLPage::HTMLPage(MainWindow* mw, QWidget *parent) :
    QWebPage(parent), _mw(mw),
    loadFinished(false)
{
    connect(this, SIGNAL(loadFinished(bool)), this, SLOT(pageLoadFinished(bool)));
    connect(mainFrame(), SIGNAL(javaScriptWindowObjectCleared()), this, SLOT(jsCleared()));
}

void
HTMLPage::javaScriptConsoleMessage(const QString &message, int lineNumber, const QString &sourceID)
{
    _mw->addOutput("<div style='color:red;'>JavaScript message: source " +sourceID + ", line no. " + QString().number(lineNumber) + ": " + message + "</div><br>\n");
}

void
HTMLPage::jsCleared()
{
    mainFrame()->addToJavaScriptWindowObject("mznide", this);
}

void HTMLPage::runJs(QString js)
{
    mainFrame()->evaluateJavaScript(js);
}

#endif

void
HTMLPage::selectSolution(int n)
{
    qDebug() << "select " << n;
    _mw->selectJSONSolution(this,n);
}

void
HTMLPage::pageLoadFinished(bool ok)
{
    if (ok) {
        loadFinished = true;

#ifdef MINIZINC_IDE_HAVE_WEBENGINE
        // Load qwebchannel javascript
        QFile qwebchanneljs(":/qtwebchannel/qwebchannel.js");
        if (!qwebchanneljs.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "can't open qrc:///qtwebchannel/qwebchannel.js";
            return;
        }
        QTextStream qwebchanneljs_in(&qwebchanneljs);
        QString qwebchanneljs_text = qwebchanneljs_in.readAll();
        runJs(qwebchanneljs_text);

        QString setup_object("new QWebChannel(qt.webChannelTransport, function (channel) {"
                             "window.mznide = channel.objects.mznide;"
                             "});"
                             );
        runJs(setup_object);
#endif
        for (int i=0; i<json.size(); i++) {
            runJs(json[i]);
        }
        json.clear();
    }
}

void
HTMLPage::addSolution(const QString &json0)
{
    QString j = json0;
    j.replace("'","\\'");
    j.replace("\"","\\\"");
    j.replace("\n"," ");
    if (loadFinished) {
        runJs("addSolution('"+j+"')");
    } else {
        json.push_back("addSolution('"+j+"')");
    }
}

void
HTMLPage::finish(qint64 runtime)
{
    QString jscall = "if (typeof finish == 'function') { finish("+QString().number(runtime)+"); }";
    if (loadFinished) {
        runJs(jscall);
    } else {
        json.push_back(jscall);
    }
}

void
HTMLPage::showSolution(int n)
{
    if (loadFinished) {
        runJs("gotoSolution('"+QString().number(n)+"')");
    }
}
