#include "webpage.h"
 
#include <QDebug>
#include <QTextStream>
 
WebPage::WebPage(QObject* parent): QWebPage(parent){
}
 
void WebPage::javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID){
   QString logEntry = message +" on line:"+ QString::number(lineNumber) +" Source:"+ sourceID;
   qDebug()<<logEntry;
}