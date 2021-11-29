#include "outputwidget.h"
#include "ui_outputwidget.h"

#include "highlighter.h"

#include <cmath>

#include <QTextDocumentFragment>
#include <QJsonObject>
#include <QTextTable>
#include <QTextFrame>
#include <QPainter>
#include <QScrollBar>
#include <QClipboard>
#include <QTextStream>
#include <QDebug>
#include <QMenu>
#include <QPainterPath>
#include "ideutils.h"

OutputWidget::OutputWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OutputWidget)
{
    ui->setupUi(this);

    ui->textBrowser->installEventFilter(this);
    ui->textBrowser->viewport()->installEventFilter(this);
    ui->textBrowser->setOpenLinks(false);
    connect(ui->textBrowser, &QTextBrowser::anchorClicked, this, &OutputWidget::onAnchorClicked);

    auto* arrow = new OutputWidgetArrow;
    arrow->setParent(this);
    ui->textBrowser->document()->documentLayout()->registerHandler(TextObject::Arrow, arrow);
    _frame = ui->textBrowser->document()->rootFrame();

    _headerTableFormat.setWidth(QTextLength(QTextLength::PercentageLength, 100));
    _headerTableFormat.setBorder(0);
    _headerTableFormat.setCellSpacing(0);
    _headerTableFormat.setProperty(Property::ToggleFrame, true);

    _arrowFormat.setObjectType(TextObject::Arrow);
    _arrowFormat.setProperty(Property::Expanded, true);
    _arrowFormat.setForeground(Qt::gray);

    _frameFormat.setLeftMargin(10);
    _frameFormat.setProperty(Property::Expanded, true);

    _rightAlignBlockFormat.setAlignment(Qt::AlignRight);
    _rightAlignBlockFormat.setLeftMargin(10);
}

OutputWidget::~OutputWidget()
{
    delete ui;
}

void OutputWidget::setDarkMode(bool darkMode)
{
    _noticeCharFormat.setForeground(Themes::currentTheme.functionColor.get(darkMode));
    _errorCharFormat.setForeground(Themes::currentTheme.errorColor.get(darkMode));
    _infoCharFormat.setForeground(Qt::gray);

    ui->textBrowser->setStyleSheet(Themes::currentTheme.styleSheet(darkMode));
}

void OutputWidget::scrollToBottom()
{
    auto* scrollbar = ui->textBrowser->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void OutputWidget::startExecution(const QString& type, const QStringList& files)
{
    TextLayoutLock lock(this);
    auto c = ui->textBrowser->textCursor();
    c.movePosition(QTextCursor::End);

    // Create frame which will contain children
    _frame = c.insertFrame(_frameFormat);
    c = _frame->firstCursorPosition();
    // Move back to create headings above frame (Qt inserts extra line breaks if the table is created first)
    c.movePosition(QTextCursor::PreviousBlock);

    // Create table containing heading above frame
    auto* table = c.insertTable(1, 2, _headerTableFormat);

    auto leftSideCursor = table->cellAt(0, 0).firstCursorPosition();
    leftSideCursor.insertText(QString(QChar::ObjectReplacementCharacter), _arrowFormat);
    leftSideCursor.insertText(type, noticeCharFormat());
    leftSideCursor.insertText(" ", noticeCharFormat());
    leftSideCursor.insertText(files.join(", "), noticeCharFormat());

    auto rightSideCursor = table->cellAt(0, 1).firstCursorPosition();
    rightSideCursor.setBlockFormat(_rightAlignBlockFormat);
    _statusCursor = rightSideCursor;

    QTextCursor frameCursor = _frame->firstCursorPosition();
    frameCursor.setKeepPositionOnInsert(true);
    _hierarchy.clear();
    _hierarchy.append({frameCursor, 0});

    _hadServerUrl = false;
}

void OutputWidget::associateProfilerExecution(int executionId)
{
    _statusCursor.insertHtml(QString("<a href=\"cpprofiler://execution?%1\">search profiler</a> ")
                             .arg(executionId));
}

void OutputWidget::associateServerUrl(const QString& url)
{
    if (!_hadServerUrl){
        _statusCursor.insertHtml(QString("<a href=\"%1\">visualisation</a> ").arg(url));
        _hadServerUrl = true;
    }
}


void OutputWidget::addSolution(const QVariantMap& output, qint64 time)
{
    TextLayoutLock lock(this);
    auto limit = solutionLimit();
    if (limit > 0) {
        for (auto i = 0; i < _hierarchy.size() && _hierarchy[i].second >= limit; i++) {
            auto start = 1;
            for (auto j = i + 1; j < _hierarchy.size(); j++){
                start += _hierarchy[j].second * solutionLimit() * j;
            }
            auto end = start + static_cast<int>(pow(solutionLimit(), i + 1)) - 1;

            auto cursor = _hierarchy[i].first;
            cursor.setKeepPositionOnInsert(false);
            cursor.clearSelection();
            cursor.setPosition(_frame->lastPosition(), QTextCursor::KeepAnchor);

            // Remove trailing newline from fragment
            auto fragmentCursor = cursor;
            fragmentCursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
            QTextDocumentFragment fragment(fragmentCursor);

            cursor.removeSelectedText();

            if (i == _hierarchy.size() - 1) {
                _hierarchy.append({_hierarchy[i].first, 1});
            } else {
                _hierarchy[i + 1].second++;
            }

            // Create frame which will contain children
            auto* frame = cursor.insertFrame(_frameFormat);
            auto c = frame->firstCursorPosition();
            c.movePosition(QTextCursor::PreviousBlock);

            // Create table containing heading above frame
            auto* table = c.insertTable(1, 1, _headerTableFormat);

            auto cellCursor = table->cellAt(0, 0).firstCursorPosition();
            cellCursor.insertText(QString(QChar::ObjectReplacementCharacter), _arrowFormat);
            cellCursor.insertText(QString("Solutions %1..%2").arg(start).arg(end), _defaultCharFormat);

            frame->firstCursorPosition().insertFragment(fragment);

            toggleFrameVisibility(frame);

            auto next = _frame->lastCursorPosition();
            next.setKeepPositionOnInsert(true);
            for (auto j = 0; j <= i; j++) {
                _hierarchy[j].first = next;
            }
            _hierarchy[i].second = 0;
        }
    }

    QTextCursor cursor = _frame->lastCursorPosition();

    for (auto it = output.begin(); it != output.end(); it++) {
        auto section = it.key();
        if (section == "raw" || it.value().toString().isEmpty()) {
            continue;
        }
        addSection(section);
        QTextBlockFormat format;
        format.setProperty(Property::Section, section);
        cursor.setBlockFormat(format);
        cursor.block().setVisible(_sections[section]);
        if (section == "html" || section.endsWith("_html")) {
            addHtmlToSection(section, it.value().toString());
        } else {
            addTextToSection(section, it.value().toString());
        }
        if (!cursor.block().text().isEmpty()) {
            cursor.insertBlock();
        }
    }
    if (time != -1) {
        QTextBlockFormat f;
        f.setProperty(Property::MessageType, "Timing");
        cursor.setBlockFormat(f);
        addMessageType("Timing");
        if (!isMessageTypeVisible("Timing")) {
            cursor.block().setVisible(false);
        }
        cursor.insertText(QString("% time elapsed: "), _noticeCharFormat);
        cursor.insertText(IDEUtils::formatTime(time), _defaultCharFormat);
        cursor.insertText("\n", _defaultCharFormat);
    }
    QTextBlockFormat format;
    cursor.setBlockFormat(format);
    cursor.insertText("----------\n", _defaultCharFormat);
    _hierarchy.first().second++;
}

void OutputWidget::addText(const QString& text, const QString& messageType) {
    addText(text, QTextCharFormat());
}

void OutputWidget::addText(const QString& text, const QTextCharFormat& format, const QString& messageType) {
    TextLayoutLock lock(this);
    auto cursor = _frame->lastCursorPosition();
    if (messageType.isEmpty()) {
        cursor.insertText(text, format);
    } else {
        addMessageType(messageType);
        auto start = cursor.position();
        QTextBlockFormat f;
        f.setProperty(Property::MessageType, messageType);
        cursor.setBlockFormat(f);
        cursor.insertText(text, format);
        auto end = cursor.position();
        if (!isMessageTypeVisible(messageType)) {
            cursor.setPosition(start);
            while (cursor.position() < end) {
                cursor.block().setVisible(false);
                cursor.movePosition(QTextCursor::NextBlock);
            }
        }
    }
}

void OutputWidget::addHtml(const QString& html, const QString& messageType) {
    TextLayoutLock lock(this);
    auto cursor = _frame->lastCursorPosition();
    if (messageType.isEmpty()) {
        cursor.insertHtml(html);
    } else {
        addMessageType(messageType);
        auto start = cursor.position();
        QTextBlockFormat f;
        f.setProperty(Property::MessageType, messageType);
        cursor.insertHtml(html);
        auto end = cursor.position();
        cursor.setPosition(start);
        while (cursor.position() < end) {
            cursor.mergeBlockFormat(f);
            cursor.block().setVisible(isMessageTypeVisible(messageType));
            cursor.movePosition(QTextCursor::NextBlock);
        }
    }
}

void OutputWidget::addTextToSection(const QString& section, const QString& text)
{
    addTextToSection(section, text, QTextCharFormat());
}

void OutputWidget::addTextToSection(const QString& section, const QString& text, const QTextCharFormat& format)
{
    TextLayoutLock lock(this);
    auto cursor = _frame->lastCursorPosition();
    auto start = cursor.position();
    addSection(section);
    QTextBlockFormat f;
    f.setProperty(Property::Section, section);
    cursor.setBlockFormat(f);
    cursor.insertText(text, format);
    auto end = cursor.position();
    if (!_sections[section]) {
        cursor.setPosition(start);
        while (cursor.position() < end) {
            cursor.block().setVisible(false);
            cursor.movePosition(QTextCursor::NextBlock);
        }
    }
}

void OutputWidget::addHtmlToSection(const QString& section, const QString& html)
{
    TextLayoutLock lock(this);
    auto cursor = _frame->lastCursorPosition();
    auto start = cursor.position();
    addSection(section);
    QTextBlockFormat f;
    f.setProperty(Property::Section, section);
    cursor.insertHtml(html);
    auto end = cursor.position();
    cursor.setPosition(start);
    while (cursor.position() < end) {
        cursor.mergeBlockFormat(f);
        cursor.block().setVisible(_sections[section]);
        cursor.movePosition(QTextCursor::NextBlock);
    }
}

void OutputWidget::addStatistics(const QVariantMap& statistics)
{
    TextLayoutLock lock(this);
    auto cursor = _frame->lastCursorPosition();
    QTextBlockFormat f;
    f.setProperty(Property::MessageType, "Statistics");
    cursor.setBlockFormat(f);
    addMessageType("Statistics");
    for (auto it = statistics.begin(); it != statistics.end(); it++) {
        if (!isMessageTypeVisible("Statistics")) {
            cursor.block().setVisible(false);
        }
        cursor.insertText("%%%mzn-stat: ", _noticeCharFormat);
        cursor.insertText(it.key(), _defaultCharFormat);
        cursor.insertText("=", _defaultCharFormat);
        cursor.insertText(it.value().toString(), _defaultCharFormat);
        cursor.insertText("\n", _noticeCharFormat);
    }
    if (!isMessageTypeVisible("Statistics")) {
        cursor.block().setVisible(false);
    }
    cursor.insertText("%%%mzn-stat-end\n", _noticeCharFormat);
}

void OutputWidget::addStatus(const QString& status, qint64 time)
{
    TextLayoutLock lock(this);
    QMap<QString, QString> status_map = {
        {"ALL_SOLUTIONS", "=========="},
        {"OPTIMAL_SOLUTION", "=========="},
        {"UNSATISFIABLE", "=====UNSATISFIABLE====="},
        {"UNSAT_OR_UNBOUNDED", "=====UNSATorUNBOUNDED====="},
        {"UNBOUNDED", "=====UNBOUNDED====="},
        {"UNKNOWN", "=====UNKNOWN====="},
        {"ERROR", "=====ERROR====="}
    };
    auto it = status_map.find(status);
    if (it != status_map.end()) {
        _frame->lastCursorPosition().insertText(*it + "\n", _defaultCharFormat);
    }
}

void OutputWidget::endExecution(int exitCode, qint64 time)
{
    TextLayoutLock lock(this);
    if (exitCode != 0) {
        QString msg = "Process finished with non-zero exit code %1.\n";
        _frame->lastCursorPosition().insertText(msg.arg(exitCode), errorCharFormat());
    }
    auto t = IDEUtils::formatTime(time);
    _frame->lastCursorPosition().insertText(QString("Finished in %1.").arg(t), noticeCharFormat());
    _statusCursor.insertText(t, infoCharFormat());
    _frame = ui->textBrowser->document()->rootFrame();
}


void OutputWidget::addMessageType(const QString &messageType)
{
    if (_messageTypeVisible.contains(messageType)) {
        return;
    }
    _messageTypeVisible[messageType] = true;

    auto* button = new QPushButton(messageType);
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setCheckable(true);
    button->setChecked(true);

    connect(button, &QPushButton::toggled, this, [=](bool checked) { setMessageTypeVisibility(messageType, checked); });
    connect(this, &OutputWidget::messageTypeToggled, button, [=] (const QString& mt, bool visible) {
        if (messageType == mt) {
            button->setChecked(visible);
        }
    });

    ui->messageTypeButtons_horizontalLayout->addWidget(button);
}

void OutputWidget::setMessageTypeVisibility(const QString& messageType, bool visible)
{
    if (_messageTypeVisible[messageType] == visible) {
        return;
    }
    TextLayoutLock lock(this, false);
    auto cursor = ui->textBrowser->textCursor();
    cursor.movePosition(QTextCursor::Start);
    auto* rootFrame = cursor.currentFrame();
    auto rootFrameFormat = rootFrame->frameFormat();
    while (!cursor.atEnd()) {
        auto frameVisible = isFrameVisible(cursor.currentFrame());
        if (!frameVisible) {
            cursor = cursor.currentFrame()->lastCursorPosition();
            cursor.movePosition(QTextCursor::NextBlock);
            continue;
        }
        auto block = cursor.block();
        auto blockFormat = block.blockFormat();
        if (blockFormat.hasProperty(Property::MessageType)
                && blockFormat.property(Property::MessageType).toString() == messageType) {
            block.setVisible(visible);
        }
        cursor.movePosition(QTextCursor::NextBlock);
    }
    rootFrame->setFrameFormat(rootFrameFormat); // Force relayout
    _messageTypeVisible[messageType] = visible;

    emit messageTypeToggled(messageType, visible);
}

void OutputWidget::clear()
{
    auto* l1 = ui->sectionButtons_horizontalLayout;
    while (l1->count() > 0) {
        auto* child = l1->takeAt(0);
        delete child->widget();
        delete child;
    }
    auto* l2 = ui->messageTypeButtons_horizontalLayout;
    while (l2->count() > 0) {
        auto* child = l2->takeAt(0);
        delete child->widget();
        delete child;
    }
    _sections.clear();
    _messageTypeVisible.clear();
    ui->textBrowser->clear();
    ui->toggleAll_pushButton->setEnabled(false);
    _frame = ui->textBrowser->document()->rootFrame();
}

void OutputWidget::setBrowserFont(const QFont& font)
{
    ui->textBrowser->setFont(font);
    QTextCharFormat format;
    format.setFont(font);
    auto cursor = ui->textBrowser->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.mergeCharFormat(format);
}

void OutputWidget::onAnchorClicked(const QUrl& link)
{
    emit anchorClicked(link);
}

void OutputWidget::onClickTable(QTextTable* table) {
    TextLayoutLock lock(this, false);
    auto tableFormat = table->format();
    if (tableFormat.hasProperty(Property::ToggleFrame)) {
        auto tableCursor = table->lastCursorPosition();
        tableCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, 2);
        auto* frame = tableCursor.currentFrame();
        toggleFrameVisibility(frame);
        auto cursor = table->cellAt(0, 0).firstCursorPosition();
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        auto arrowFormat = cursor.charFormat();
        arrowFormat.setProperty(Property::Expanded, frame->frameFormat().property(Property::Expanded));
        cursor.setCharFormat(arrowFormat);
    }
}

void OutputWidget::toggleFrameVisibility(QTextFrame* frame)
{
    TextLayoutLock lock(this, false);
    auto frameFormat = frame->frameFormat();
    auto visible = !frameFormat.property(Property::Expanded).toBool();
    frameFormat.setProperty(Property::Expanded, visible);
    frame->setFrameFormat(frameFormat);

    auto parentVisible = isFrameVisible(frame->parentFrame());
    if (!parentVisible) {
        return;
    }

    // Set visibility of TextBlocks
    auto cursor = frame->firstCursorPosition();
    auto end = frame->lastCursorPosition();
    while (cursor < end) {
        auto* childFrame = cursor.currentFrame();
        if (childFrame != nullptr && childFrame != frame) {
            auto childFormat = frame->frameFormat();
            auto childVisible = !frameFormat.property(Property::Expanded).toBool();
            if (childVisible == visible) {
                cursor = childFrame->lastCursorPosition();
                cursor.movePosition(QTextCursor::NextBlock);
                continue;
            }
        }

        auto block = cursor.block();
        auto blockFormat = block.blockFormat();
        auto blockVisible = true;
        if (blockFormat.hasProperty(Property::Section)) {
            blockVisible = _sections[blockFormat.property(Property::Section).toString()];
        } else if (blockFormat.hasProperty(Property::MessageType)) {
            blockVisible = _messageTypeVisible[blockFormat.property(Property::MessageType).toString()];
        }
        block.setVisible(isFrameVisible(cursor.currentFrame()) && blockVisible);
        cursor.movePosition(QTextCursor::NextBlock);
    }
}

void OutputWidget::addSection(const QString& section)
{
    if (_sections.contains(section)) {
        return;
    }
    _sections[section] = true;

    auto* button = new QPushButton(section);
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setCheckable(true);
    button->setChecked(true);

    connect(button, &QPushButton::toggled, this, [=](bool checked) { setSectionVisibility(section, checked); });
    connect(this, &OutputWidget::sectionToggled, button, [=] (const QString& s, bool visible) {
        if (section == s) {
            button->setChecked(visible);
        }
    });

    auto* menu = new QMenu(button);
    auto* toggleSection = menu->addAction("Show section", this, [=] () { toggleSectionVisibility(section); });
    menu->addSeparator();
    menu->addAction("Show only this section", this, [=] () {
        setAllSectionsVisibility(false);
        setSectionVisibility(section, true);
    });
    menu->addAction("Show all but this section", this, [=] () {
        setAllSectionsVisibility(true);
        setSectionVisibility(section, false);
    });

    button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(button, &QPushButton::customContextMenuRequested, this, [=] (const QPoint &p) {
        toggleSection->setText(_sections[section] ? "Hide section" : "Show section");
        menu->exec(button->mapToGlobal(p));
    });

    ui->toggleAll_pushButton->setEnabled(true);
    ui->sectionButtons_horizontalLayout->addWidget(button);
}

void OutputWidget::setSectionVisibility(const QString& section, bool visible)
{
    if (_sections[section] == visible) {
        return;
    }
    TextLayoutLock lock(this, false);
    auto cursor = ui->textBrowser->textCursor();
    cursor.movePosition(QTextCursor::Start);
    auto* rootFrame = cursor.currentFrame();
    auto rootFrameFormat = rootFrame->frameFormat();
    while (!cursor.atEnd()) {
        auto frameVisible = isFrameVisible(cursor.currentFrame());
        if (!frameVisible) {
            cursor = cursor.currentFrame()->lastCursorPosition();
            cursor.movePosition(QTextCursor::NextBlock);
            continue;
        }
        auto block = cursor.block();
        auto blockFormat = block.blockFormat();
        if (blockFormat.hasProperty(Property::Section)
                && blockFormat.property(Property::Section).toString() == section) {
            block.setVisible(visible);
        }
        cursor.movePosition(QTextCursor::NextBlock);
    }
    _sections[section] = visible;
    rootFrame->setFrameFormat(rootFrameFormat); // Force relayout

    emit sectionToggled(section, visible);

    for (auto v : _sections) {
        if (!v) {
            ui->toggleAll_pushButton->setText("Show all");
            return;
        }
    }
    ui->toggleAll_pushButton->setText("Hide all");
}

void OutputWidget::setAllSectionsVisibility(bool visible)
{
    TextLayoutLock lock(this, false);
    auto cursor = ui->textBrowser->textCursor();
    cursor.movePosition(QTextCursor::Start);
    auto* rootFrame = cursor.currentFrame();
    auto rootFrameFormat = rootFrame->frameFormat();
    while (!cursor.atEnd()) {
        auto frameVisible = isFrameVisible(cursor.currentFrame());
        if (!frameVisible) {
            cursor = cursor.currentFrame()->lastCursorPosition();
            cursor.movePosition(QTextCursor::NextBlock);
            continue;
        }
        auto block = cursor.block();
        auto blockFormat = block.blockFormat();
        if (blockFormat.hasProperty(Property::Section)) {
            block.setVisible(visible);
        }
        cursor.movePosition(QTextCursor::NextBlock);
    }

    for (auto it = _sections.begin(); it != _sections.end(); it++) {
        bool toggled = it.value() != visible;
        _sections[it.key()] = visible;
        if (toggled) {
            emit sectionToggled(it.key(), visible);
        }
    }
    rootFrame->setFrameFormat(rootFrameFormat); // Force relayout

    ui->toggleAll_pushButton->setText(visible ? "Hide all" : "Show all");
}



int OutputWidget::mouseToPosition(const QPoint& mousePos, Qt::HitTestAccuracy accuracy)
{
    auto* hbar = ui->textBrowser->horizontalScrollBar();
    auto hOffset = isRightToLeft() ? (hbar->maximum() - hbar->value()) : hbar->value();
    auto vOffset = ui->textBrowser->verticalScrollBar()->value();
    auto* layout = ui->textBrowser->document()->documentLayout();
    return layout->hitTest(QPointF(mousePos.x() + hOffset, mousePos.y() + vOffset), accuracy);
}

QTextCursor OutputWidget::fragmentCursor(int position)
{
    if (position != -1) {
        auto cursor = ui->textBrowser->textCursor();
        cursor.setPosition(position);
        auto block = cursor.block();
        for (auto it = block.begin(); it != block.end(); it++) {
            auto fragment = it.fragment();
            if (fragment.contains(position)) {
                cursor.setPosition(fragment.position());
                cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, fragment.length());
                return cursor;
            }
        }
    }
    return QTextCursor();
}

bool OutputWidget::eventFilter(QObject* object, QEvent* event)
{
    if (object == ui->textBrowser->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton) {
            if (_dragging) {
                _dragging = false;
            } else {
                auto position = mouseToPosition(e->pos());
                auto cursor = fragmentCursor(position);
                if (cursor.isNull()) {
                    // See if we're clicking an entire table
                    position = mouseToPosition(e->pos(), Qt::FuzzyHit);
                    if (position != -1) {
                        cursor = ui->textBrowser->textCursor();
                        cursor.setPosition(position);
                        auto* table = cursor.currentTable();
                        if (table != nullptr) {
                            onClickTable(table);
                        }
                    }
                } else if (!cursor.charFormat().isAnchor()) {
                    auto* table = cursor.currentTable();
                    if (table != nullptr) {
                        onClickTable(table);
                    }
                }
            }
        }
    } else if (object == ui->textBrowser && event->type() == QEvent::KeyPress) {
        auto* e = static_cast<QKeyEvent*>(event);
        if (e == QKeySequence::Copy || e == QKeySequence::Cut) {
            auto* clipboard = QApplication::clipboard();
            QString contents;
            QTextStream ts(&contents);
            auto cursor = ui->textBrowser->textCursor();

            auto block = ui->textBrowser->document()->begin();
            while (block.isValid()) {
                if (block.isVisible()) {
                    ts << block.text().remove(QChar::ObjectReplacementCharacter);
                }
                block = block.next();
            }
            clipboard->setText(contents);
            return true;
        }
    }
    return false;
}

bool OutputWidget::isFrameVisible(QTextFrame* frame)
{
    while (frame != nullptr) {
        auto format = frame->frameFormat();
        if (format.hasProperty(Property::Expanded)
                && !format.property(Property::Expanded).toBool()) {
            return false;
        }
        frame = frame->parentFrame();
    }
    return true;
}

OutputWidget::TextLayoutLock::TextLayoutLock(OutputWidget* o, bool scroll) : _o(o), _scroll(scroll) {
    if (_o->_frame != _o->ui->textBrowser->document()->rootFrame()) {
        auto frameVisible = _o->isFrameVisible(_o->_frame);
        if (!frameVisible) {
            _o->toggleFrameVisibility(_o->_frame);
        }
    }
    _o->ui->textBrowser->textCursor().beginEditBlock();
}

OutputWidget::TextLayoutLock::~TextLayoutLock()
{
    _o->ui->textBrowser->textCursor().endEditBlock();
    if (_scroll) {
        _o->scrollToBottom();
    }
}

QSizeF OutputWidgetArrow::intrinsicSize(QTextDocument* doc, int posInDocument, const QTextFormat& format)
{
    QFontMetrics metrics(format.toCharFormat().font());
    return QSizeF(metrics.height(), metrics.height());
}

void OutputWidgetArrow::drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc, int posInDocument, const QTextFormat& format)
{
    QFontMetrics metrics(format.toCharFormat().font());
    qreal hh = metrics.ascent() / 2.0;
    qreal hw = hh / 2.0;
    QPainterPath p;
    p.moveTo(-hw, -hh);
    p.lineTo(hw, 0);
    p.lineTo(-hw, hh);
    p.closeSubpath();
    painter->translate(rect.center());
    painter->translate(-metrics.descent(), metrics.descent());
    if (format.property(OutputWidget::Property::Expanded).toBool()) {
        painter->rotate(90);
    }
    painter->fillPath(p, format.foreground());
}

void OutputWidget::on_toggleAll_pushButton_clicked()
{
    for (auto it = _sections.begin(); it != _sections.end(); it++) {
        if (!it.value()) {
            // Some invisible, so show all
            setAllSectionsVisibility(true);
            return;
        }
    }
    // Hide all sections since they were all previously visible
    setAllSectionsVisibility(false);
}
