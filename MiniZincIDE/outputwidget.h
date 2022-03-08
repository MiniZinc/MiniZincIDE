#ifndef OUTPUTWIDGET_H
#define OUTPUTWIDGET_H

#include <QWidget>
#include <QTextCursor>
#include <QTextObjectInterface>
#include <QTimer>
#include <QElapsedTimer>
#include <QMenu>

namespace Ui {
class OutputWidget;
}

class OutputWidget : public QWidget
{
    Q_OBJECT
    friend class TestIDE;

public:
    explicit OutputWidget(QWidget *parent = nullptr);
    ~OutputWidget();

    enum TextObject {
        Arrow = QTextFormat::UserObject + 1
    };

    enum Property {
        Expanded = QTextFormat::UserProperty + 1,
        ToggleFrame,
        Section,
        MessageType
    };

    bool isSectionVisible(const QString& section) { return _sections.value(section, false); }
    bool isMessageTypeVisible(const QString& messageType) { return _messageTypeVisible.value(messageType, false); }

    const QTextCharFormat& defaultCharFormat() const { return _defaultCharFormat; }
    const QTextCharFormat& noticeCharFormat() const { return _noticeCharFormat; }
    const QTextCharFormat& errorCharFormat() const { return _errorCharFormat; }
    const QTextCharFormat& infoCharFormat() const { return _infoCharFormat; }
    const QTextCharFormat& commentCharFormat() const { return _commentCharFormat; }

    const QTextTableFormat headerTableFormat() const { return _headerTableFormat; }
    const QTextFrameFormat frameFormat() const { return _frameFormat; }

    int solutionLimit() { return _solutionLimit; }

    QString lastTraceLoc(const QString& newTraceLoc);

public slots:
    void setDarkMode(bool darkMode);

    void copySelectionToClipboard(bool includeHidden = false);

    void startExecution(const QString& label);
    void associateProfilerExecution(int executionId);
    void associateServerUrl(const QString& url);
    void addSolution(const QVariantMap& output, const QStringList& order, qint64 time = -1);
    void addCheckerOutput(const QVariantMap& output, const QStringList& order);
    void addText(const QString& text, const QString& messageType = QString());
    void addText(const QString& text, const QTextCharFormat& format, const QString& messageType = QString());
    void addHtml(const QString& html, const QString& messageType = QString());
    void addTextToSection(const QString& section, const QString& text);
    void addTextToSection(const QString& section, const QString& text, const QTextCharFormat& format);
    void addHtmlToSection(const QString& section, const QString& html);

    void addStatistics(const QVariantMap& statistics);
    void addStatus(const QString& status, qint64 time = -1);
    void endExecution(int exitCode, qint64 time = -1);

    void clear();

    void setSolutionLimit(int solutionLimit) { _solutionLimit = solutionLimit; }
    void setBrowserFont(const QFont& font);

    void setSectionVisibility(const QString& section, bool visible);
    void toggleSectionVisibility(const QString& section) { setSectionVisibility(section, !isSectionVisible(section)); }
    void setAllSectionsVisibility(bool visible);

    void setMessageTypeVisibility(const QString& messageType, bool visible);
    void toggleMessageTypeVisibility(const QString& messageType) { setMessageTypeVisibility(messageType, !isMessageTypeVisible(messageType)); }

    void scrollToBottom();
signals:
    void sectionToggled(const QString& section, bool visible);
    void messageTypeToggled(const QString& messageType, bool visible);
    void anchorClicked(const QUrl& url);

private:
    Ui::OutputWidget *ui;

    friend class TextLayoutLock;

    QMenu* _contextMenu = nullptr;

    bool _dragging = false;
    bool _pressed = false;
    int _solutionLimit = 100;
    int _firstCompressedSolution = 0;

    QTextCursor _statusCursor;
    QTextFrame* _frame = nullptr;
    QTextDocument* _solutionBuffer = nullptr;
    QTextCursor _lastSolutions[2];

    QMap<QString, bool> _sections;
    QMap<QString, bool> _messageTypeVisible;

    bool _hadServerUrl;

    QTextCharFormat _defaultCharFormat;
    QTextCharFormat _noticeCharFormat;
    QTextCharFormat _errorCharFormat;
    QTextCharFormat _infoCharFormat;
    QTextCharFormat _commentCharFormat;
    QTextCharFormat _arrowFormat;
    QTextBlockFormat _rightAlignBlockFormat;
    QTextTableFormat _headerTableFormat;
    QTextFrameFormat _frameFormat;

    QVector<QPair<QString, QVariant>> _checkerOutput;

    int _solutionCount = 0;
    int _effectiveSolutionLimit = 0;

    QString _lastTraceLoc;

    void addSection(const QString& section);
    void addMessageType(const QString& messageType);
    void onClickTable(QTextTable* table);
    void toggleFrameVisibility(QTextFrame* frame);
    bool eventFilter(QObject* object, QEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;
    bool isFrameVisible(QTextFrame* frame);
    void layoutButtons();

    QTextCursor nextInsertAt() const;

    int mouseToPosition(const QPoint& mousePos, Qt::HitTestAccuracy accuracy = Qt::ExactHit);
    QTextCursor fragmentCursor(int position);

private slots:
    void onAnchorClicked(const QUrl& link);
    void on_toggleAll_pushButton_clicked();
    void onBrowserContextMenu(const QPoint& pos);
};

class OutputWidgetArrow : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    QSizeF intrinsicSize(QTextDocument* doc, int posInDocument, const QTextFormat& format) override;
    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc, int posInDocument, const QTextFormat& format) override;
};

class TextLayoutLock {
public:
    explicit TextLayoutLock(OutputWidget* o, bool scroll = true);
    ~TextLayoutLock();
private:
    bool _scroll;
    OutputWidget* _o;
};

#endif // OUTPUTWIDGET_H
