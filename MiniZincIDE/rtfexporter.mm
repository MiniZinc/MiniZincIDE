#import <Cocoa/Cocoa.h>
#include "rtfexporter.h"

NSData* ba_toNSData(const QByteArray& data) {
  return [NSData dataWithBytes:data.constData() length:data.size()];
}

QByteArray ba_fromNSData(const NSData *data)
{
    QByteArray ba;
    ba.resize([data length]);
    [data getBytes:ba.data() length:ba.size()];
    return ba;
}

QVariant MyRtfMime::convertToMime(const QString &mimeType, QList<QByteArray> data, QString flavor) {
    if (!canConvert(mimeType, flavor))
        return QVariant();
    if (data.count() > 1)
        qWarning("QMacPasteboardMimeHTMLText: Cannot handle multiple member data");

    // Read RTF into to NSAttributedString, then convert the string to HTML
    NSAttributedString *string = [[NSAttributedString alloc] initWithData:ba_toNSData(data.at(0))
      options:[NSDictionary dictionaryWithObject:NSRTFTextDocumentType forKey:NSDocumentTypeDocumentAttribute]
      documentAttributes:nil
      error:nil];
    NSError *error;
    NSRange range = NSMakeRange(0, [string length]);
    NSDictionary *dict = [NSDictionary dictionaryWithObject:NSHTMLTextDocumentType forKey:NSDocumentTypeDocumentAttribute];
    NSData *htmlData = [string dataFromRange:range documentAttributes:dict error:&error];
    return ba_fromNSData(htmlData);
}

QList<QByteArray> MyRtfMime::convertFromMime(const QString &mime, QVariant data, QString flavor) {
    QList<QByteArray> ret;
    if (!canConvert(mime, flavor))
        return ret;
    NSAttributedString *string = [[NSAttributedString alloc] initWithData:ba_toNSData(data.toByteArray())
      options:[NSDictionary dictionaryWithObject:NSHTMLTextDocumentType forKey:NSDocumentTypeDocumentAttribute]
      documentAttributes:nil
      error:nil];
    NSError *error;
    NSRange range = NSMakeRange(0, [string length]);
    NSDictionary *dict = [NSDictionary dictionaryWithObject:NSRTFTextDocumentType forKey:NSDocumentTypeDocumentAttribute];
    NSData *rtfData = [string dataFromRange:range documentAttributes:dict error:&error];
    ret << ba_fromNSData(rtfData);
    return ret;
}
