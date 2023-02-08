#ifndef HISTORY_H
#define HISTORY_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <QJsonObject>
#include <QDir>

class FileDiff
{
public:
    enum class EntryType {
        Insertion,
        Deletion
    };

    struct Entry {
        EntryType kind;
        qsizetype line;
        QString text;

        Entry() {}

        Entry(EntryType _kind, qsizetype _line, const QString& _text):
            kind(_kind),
            line(_line),
            text(_text) {}
    };

    FileDiff(const QString& origText, const QString& newText);

    QVector<Entry> diff();

    static QString apply(const QString& source, const QVector<Entry>& diff);

private:
    QStringList _orig;
    QStringList _new;

    qsizetype _offset;
    qsizetype _origLength;
    qsizetype _newLength;
};

class History : public QObject {
    Q_OBJECT

public:
    ///
    /// \brief Create a new history tracker
    /// \param doc The JSON history data
    /// \param parent The parent object
    ///
    explicit History(const QJsonObject& obj, const QDir& relativeTo, QObject* parent = nullptr);

    explicit History(const QString& uuid, QObject* parent = nullptr);

    ///
    /// \brief Output history as JSON
    /// \return The JSON document
    ///
    QJsonObject toJSON() const;

    void addFile(const QString& file, const QString& contents);
signals:
    ///
    /// \brief Emitted when the history has changed
    ///
    void historyChanged();

public slots:
    ///
    /// \brief Update the history of the given file if it is currently being tracked
    /// \param file The file path
    /// \param contents The new file contents
    ///
    void updateFileContents(const QString& file, const QString& contents);

    ///
    /// \brief Remove stored changes and create new history node
    ///
    void commit();

private:
    struct Change {
        double timestamp;
        QVector<FileDiff::Entry> edits;
    };

    struct FileHistory {
        QString snapshot;
        QVector<Change> changes;
    };

    QString _uuid;
    QString _parent;
    QMap<QString, FileHistory> _history;
    QDir _rootDir;
};

#endif // HISTORY_H
