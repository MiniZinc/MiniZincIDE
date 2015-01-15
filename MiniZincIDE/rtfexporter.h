#ifndef RTFEXPORTER_H
#define RTFEXPORTER_H

#include <QMacPasteboardMime>

class MyRtfMime : QMacPasteboardMime {
public:
    MyRtfMime() : QMacPasteboardMime(MIME_ALL) { }
    QString convertorName() { return QString("MyRtfMime"); }
    bool canConvert(const QString &mime, QString flav) {
        return mimeFor(flav)==mime;
    }
    QString mimeFor(QString flav) {
        if (flav==QString("public.rtf"))
            return QString("text/html");
        return QString();
    }
    QString flavorFor(const QString &mime) {
        if (mime==QString("text/html"))
            return QString("public.rtf");
        return QString();
    }
    QVariant convertToMime(const QString &mimeType, QList<QByteArray> data, QString flavor);

    QList<QByteArray> convertFromMime(const QString &mime, QVariant data, QString flav);
};


#endif // RTFEXPORTER_H
