#include "history.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>

FileDiff::FileDiff(const QString& origText, const QString& newText):
    _orig(origText.split("\n")),
    _new(newText.split("\n"))
{
    _offset = 0;
    _origLength = _orig.size();
    _newLength = _new.size();

    // Trim start lines if same
    while (_offset < _orig.size() - 1 && _offset < _new.size() - 1 && _orig[_offset] == _new[_offset]) {
        _offset++;
        _origLength--;
        _newLength--;
    }

    // Trim end lines if same
    while (_origLength > 0 && _newLength > 0 && _orig[_offset + _origLength - 1] == _new[_offset + _newLength - 1]) {
        _origLength--;
        _newLength--;
    }
}

QVector<FileDiff::Entry> FileDiff::diff()
{
    if (_origLength == 0 && _newLength == 0) {
        return {};
    }

    auto m = _origLength + 1;
    auto n = _newLength + 1;
    QVector<int> c;
    c.resize(m * n);
    for (auto i = 0; i < _origLength; i++) {
        for (auto j = 0; j < _newLength; j++) {
            if (_orig[i + _offset] == _new[j + _offset]) {
                c[(j + 1) * m + i + 1] = c[j * m + i] + 1;
            } else {
                auto a = c[j * m + i + 1];
                auto b = c[(j + 1) * m + i];
                c[(j + 1) * m + i + 1] = qMax(a, b);
            }
        }
    }
    QVector<FileDiff::Entry> result;
    auto i = _origLength;
    auto j = _newLength;
    while (true) {
        if (i > 0 && j > 0
                && c[j * m + i] == c[(j - 1) * m + i - 1] + 1
                && c[(j - 1) * m + i] == c[j * m + i - 1]) {
            i--;
            j--;
        } else if (j > 0 && (i == 0 || c[(j - 1) * m + i] >= c[j * m + i - 1])) {
            result.prepend(FileDiff::Entry(
                               FileDiff::EntryType::Insertion,
                               i + 1 + _offset,
                               _new[j - 1 + _offset]
                           ));
            j--;
        } else if (i > 0 && (j == 0 || c[(j - 1) * m + i] < c[j * m + i - 1])) {
            result.prepend(FileDiff::Entry(
                               FileDiff::EntryType::Deletion,
                               i + _offset,
                               _orig[i - 1 + _offset]
                           ));
            i--;
        } else {
            break;
        }
    }
    return result;
}

QString FileDiff::apply(const QString& source, const QVector<FileDiff::Entry>& diff)
{
    auto split = source.split("\n");
    QStringList text;
    auto it = diff.begin();
    for (auto i = 0; i < split.size(); i++) {
        bool deleteOrig = false;
        while (it != diff.end() && it->line == i + 1) {
            if (it->kind == FileDiff::EntryType::Insertion) {
                text.append(it->text);
            } else {
                assert(!deleteOrig);
                deleteOrig = true;
            }
            it++;
        }
        if (!deleteOrig) {
            text.append(split[i]);
        }
    }
    while (it != diff.end()) {
        assert(it->kind == FileDiff::EntryType::Insertion);
        assert(it->line == split.size() + 1);
        text.append(it->text);
        it++;
    }
    return text.join('\n');
}

History::History(const QJsonObject& obj, const QDir& relativeTo, QObject* parent): QObject(parent), _rootDir(relativeTo)
{
    _uuid = obj["uuid"].toString();
    _parent = obj["parent"].toString();
    auto files = obj["files"].toObject();
    for (auto it = files.begin(); it != files.end(); it++) {
        auto file = _rootDir.absoluteFilePath(it.key());
        auto entry = it.value().toObject();
        FileHistory h;
        h.snapshot = entry["snapshot"].toString();
        for (auto change : entry["changes"].toArray()) {
            auto changeObj = change.toObject();
            History::Change c;
            c.timestamp = changeObj["timestamp"].toDouble();
            for (auto edit : changeObj["edits"].toArray()) {
                auto editObj = edit.toObject();
                auto type = editObj["type"].toString() == "insertion" ? FileDiff::EntryType::Insertion : FileDiff::EntryType::Deletion;
                auto line = editObj["line"].toInt();
                auto text = editObj["text"].toString();
                c.edits.append(FileDiff::Entry(type, line, text));
            }
            h.changes.append(c);
        }
        _history.insert(file, h);
    }
}

History::History(const QString& uuid, QObject* parent): QObject(parent), _uuid(uuid)
{
}

void History::addFile(const QString& file, const QString& contents)
{
    FileHistory h;
    h.snapshot = contents;
    _history.insert(file, h);
}

QJsonObject History::toJSON() const
{
    QJsonObject root;
    root["uuid"] = _uuid;
    root["parent"] = _parent.isEmpty() ? QJsonValue(QJsonValue::Type::Null) : QJsonValue(_parent);
    QJsonObject files;
    for (auto it = _history.begin(); it != _history.end(); it++) {
        auto& val = it.value();
        auto file = _rootDir.relativeFilePath(it.key());
        QJsonObject entry;
        entry["snapshot"] = val.snapshot;
        QJsonArray changes;
        for (auto& change : val.changes) {
            QJsonObject c;
            c["timestamp"] = change.timestamp;
            QJsonArray edits;
            for (auto& edit : change.edits) {
                QJsonObject e;
                e["type"] = edit.kind == FileDiff::EntryType::Insertion ? "insertion" : "deletion";
                e["line"] = edit.line;
                e["text"] = edit.text;
                edits.append(e);
            }
            c["edits"] = edits;
            changes.append(c);
        }
        entry["changes"] = changes;
        files.insert(file, entry);
    }
    root["files"] = files;
    return root;
}

void History::updateFileContents(const QString& file, const QString& contents)
{
    auto it = _history.find(file);
    if (it == _history.end()) {
        return;
    }
    FileDiff d(it.value().snapshot, contents);
    auto diff = d.diff();
    if (diff.empty()) {
        return;
    }
    History::Change c;
    c.edits = diff;
    c.timestamp = static_cast<double>(QDateTime::currentMSecsSinceEpoch()) / 1000.0;
    it.value().changes.append(std::move(c));
    it.value().snapshot = contents;
    emit historyChanged();
}

void History::commit() {
    for (auto it = _history.begin(); it != _history.end(); it++) {
        it.value().changes.clear();
    }
    _parent = _uuid;
    _uuid = QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
    emit historyChanged();
}
