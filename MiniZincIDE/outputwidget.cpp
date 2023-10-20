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
#include <QCheckBox>
#include "ideutils.h"
#include "highlighter.h"

OutputWidget::OutputWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OutputWidget)
{
    ui->setupUi(this);

    auto* sectionMenu = new QMenu(this);
    ui->sectionMenu_pushButton->hide();
    ui->sectionMenu_pushButton->setMenu(sectionMenu);
    connect(sectionMenu, &QMenu::aboutToShow, this, [=] () {
        for (auto* action : sectionMenu->actions()) {
            action->setChecked(isSectionVisible(action->text()));
        }
    });

    auto* messageTypeMenu = new QMenu(this);
    ui->messageTypeMenu_pushButton->hide();
    ui->messageTypeMenu_pushButton->setMenu(messageTypeMenu);
    connect(messageTypeMenu, &QMenu::aboutToShow, this, [=] () {
        for (auto* action : messageTypeMenu->actions()) {
            action->setChecked(isMessageTypeVisible(action->text()));
        }
    });

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

    QFontMetrics metrics(ui->textBrowser->font());
    auto spaceNeeded = metrics.height() + 4;
    auto cwc = { QTextLength(QTextLength::FixedLength, spaceNeeded) };
    _headerTableFormat.setColumnWidthConstraints(cwc);

    _arrowFormat.setObjectType(TextObject::Arrow);
    _arrowFormat.setForeground(Qt::gray);

    _frameFormat.setLeftMargin(spaceNeeded);
    _frameFormat.setProperty(Property::Expanded, true);

    _rightAlignBlockFormat.setAlignment(Qt::AlignRight);
    _rightAlignBlockFormat.setLeftMargin(10);
#ifdef Q_OS_MAC
    ui->toggleAll_pushButton->setMinimumWidth(85);
    layout()->setSpacing(8);

    ui->buttons_widget->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ui->sectionButtons_widget->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ui->messageTypeButtons_widget->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ui->sectionMenu_pushButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ui->messageTypeMenu_pushButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ui->toggleAll_pushButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
#else
    setStyleSheet("QPushButton { padding: 2px 6px; }");
#endif
    _contextMenu = new QMenu(this);
    _contextMenu->addAction("Copy selected", this, [=] () { copySelectionToClipboard(false); });
    _contextMenu->addAction("Copy selected including hidden", this, [=] () { copySelectionToClipboard(true); });
    _contextMenu->addSeparator();
    _contextMenu->addAction("Select All", this, [=] () { ui->textBrowser->selectAll(); });

    connect(ui->textBrowser, &QTextBrowser::customContextMenuRequested, this, &OutputWidget::onBrowserContextMenu);
    ui->textBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
}

OutputWidget::~OutputWidget()
{
    delete ui;
}

QString OutputWidget::lastTraceLoc(const QString &newTraceLoc)
{
    QString ltl = _lastTraceLoc;
    _lastTraceLoc = newTraceLoc;
    return ltl;
}

void OutputWidget::setTheme(const Theme& theme, bool darkMode)
{
    _defaultCharFormat.setForeground(theme.textColor.get(darkMode));
    _noticeCharFormat.setForeground(theme.functionColor.get(darkMode));
    _errorCharFormat.setForeground(theme.errorColor.get(darkMode));
    _errorCharFormat.setForeground(theme.errorColor.get(darkMode));
    _infoCharFormat.setForeground(Qt::gray);
    _commentCharFormat.setForeground(theme.commentColor.get(darkMode));

    ui->textBrowser->viewport()->setStyleSheet(theme.styleSheet(darkMode));
}

void OutputWidget::scrollToBottom()
{
    auto* scrollbar = ui->textBrowser->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void OutputWidget::startExecution(const QString& label)
{
    _checkerOutput.clear();

    TextLayoutLock lock(this);
    auto c = ui->textBrowser->textCursor();
    c.movePosition(QTextCursor::End);

    // Create table containing heading
    auto* table = c.insertTable(1, 3, _headerTableFormat);

    auto leftSideCursor = table->cellAt(0, 0).firstCursorPosition();
    leftSideCursor.insertText(QString(QChar::ObjectReplacementCharacter), _arrowFormat);

    auto headerCursor = table->cellAt(0, 1).firstCursorPosition();
    headerCursor.insertText(label, noticeCharFormat());

    auto rightSideCursor = table->cellAt(0, 2).firstCursorPosition();
    rightSideCursor.setBlockFormat(_rightAlignBlockFormat);
    _statusCursor = rightSideCursor;

    // Create frame which will contain children
    c.movePosition(QTextCursor::End);
    _frame = c.insertFrame(_frameFormat);

    _hadServerUrl = false;

    _solutionCount = 0;
    _effectiveSolutionLimit = _solutionLimit;
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

void OutputWidget::addSolution(const QVariantMap& output, const QStringList& order, qint64 time)
{
    TextLayoutLock lock(this);
    QTextCursor cursor = nextInsertAt();
    if (!_checkerOutput.isEmpty()) {
        cursor.insertText("% Solution checker report:\n", _commentCharFormat);
        for (auto& it : _checkerOutput) {
            auto section = it.first;
            if (section == "raw" || it.second.toString().isEmpty()) {
                continue;
            }
            addSection(section);
            QTextBlockFormat format;
            format.setProperty(Property::Section, section);
            cursor.setBlockFormat(format);
            cursor.block().setVisible(_sections[section]);
            if (section == "html" || section.endsWith("_html")) {
                addHtmlToSection(section, it.second.toString());
            } else {
                auto lines = it.second.toString().split("\n");
                if (lines.last().isEmpty()) {
                    lines.pop_back();
                }
                bool first = true;
                for (auto& line : lines) {
                    addTextToSection(section, (first ? "% " : "\n% ") + line, _commentCharFormat);
                    first = false;
                }
            }
            if (!cursor.block().text().isEmpty()) {
                cursor.insertBlock();
            }
        }
        _checkerOutput.clear();
    }

    for (auto& section : order) {
        if (section == "raw" || output[section].toString().isEmpty()) {
            continue;
        }
        addSection(section);
        QTextBlockFormat format;
        format.setProperty(Property::Section, section);
        cursor.setBlockFormat(format);
        cursor.block().setVisible(_sections[section]);
        if (section == "html" || section.endsWith("_html")) {
            addHtmlToSection(section, output[section].toString());
        } else {
            addTextToSection(section, output[section].toString());
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

    _solutionCount++;

    _lastSolutions[0] = _lastSolutions[1];
    _lastSolutions[1] = cursor;
    _lastSolutions[1].setKeepPositionOnInsert(true);

    if (_solutionLimit > 0 && _solutionCount >= _solutionLimit) {
        if (_solutionBuffer == nullptr) {
            _solutionBuffer = new QTextDocument(this);
            _firstCompressedSolution = _solutionCount;
            _effectiveSolutionLimit *= 2;
        } else if (_solutionCount >= _effectiveSolutionLimit - 1) {
            auto* doc = _solutionBuffer;
            _solutionBuffer = nullptr;
            if (!doc->isEmpty()) {
                QTextDocumentFragment contents(doc);

                // Create table containing heading
                auto* t = nextInsertAt().insertTable(1, 2, _headerTableFormat);
                t->cellAt(0, 0).firstCursorPosition().insertText(QString(QChar::ObjectReplacementCharacter), _arrowFormat);
                auto label = QString("[ %1 more solutions ]").arg(_solutionCount - _firstCompressedSolution);
                t->cellAt(0, 1).firstCursorPosition().insertText(label, noticeCharFormat());
                
                // Create frame which will contain children
                auto* frame = nextInsertAt().insertFrame(_frameFormat);
                frame->firstCursorPosition().insertFragment(contents);
                if (frame->lastCursorPosition().block().text().isEmpty()) {
                    // Remove trailing newline
                    frame->lastCursorPosition().deletePreviousChar();
                }
                toggleFrameVisibility(frame);
            }
            delete doc;
        }
    }
}

void OutputWidget::addCheckerOutput(const QVariantMap& output, const QStringList& order)
{
    for (auto& it : order) {
        _checkerOutput.append({it, output[it]});
    }
}

void OutputWidget::addText(const QString& text, const QString& messageType) {
    addText(text, QTextCharFormat(), messageType);
}

void OutputWidget::addText(const QString& text, const QTextCharFormat& format, const QString& messageType) {
    TextLayoutLock lock(this);
    auto cursor = nextInsertAt();
    if (messageType != "trace") {
        _lastTraceLoc = "";
    }
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
    auto cursor = nextInsertAt();
    if (messageType != "trace") {
        _lastTraceLoc = "";
    }
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
    auto cursor = nextInsertAt();
    _lastTraceLoc = "";
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
    auto cursor = nextInsertAt();
    _lastTraceLoc = "";
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
    auto cursor = nextInsertAt();
    _lastTraceLoc = "";
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
    _lastTraceLoc = "";
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
        nextInsertAt().insertText(*it + "\n", _defaultCharFormat);
    }
}

void OutputWidget::endExecution(int exitCode, qint64 time)
{
    TextLayoutLock lock(this);

    if (_solutionBuffer != nullptr) {
        auto* doc = _solutionBuffer;
        _solutionBuffer = nullptr;

        // Print compressed solutions
        QTextCursor cursor(doc);
        cursor.setPosition(_lastSolutions[0].position(), QTextCursor::KeepAnchor);
        if (cursor.hasSelection()) {
            auto* t = nextInsertAt().insertTable(1, 2, _headerTableFormat);
            t->cellAt(0, 0).firstCursorPosition().insertText(QString(QChar::ObjectReplacementCharacter), _arrowFormat);
            auto label = QString("[ %1 more solutions ]").arg(_solutionCount - _firstCompressedSolution - 1);
            t->cellAt(0, 1).firstCursorPosition().insertText(label, noticeCharFormat());
            
            auto* f = nextInsertAt().insertFrame(_frameFormat);
            f->firstCursorPosition().insertFragment(QTextDocumentFragment(cursor));
            if (f->lastCursorPosition().block().text().isEmpty()) {
                // Remove trailing newline
                f->lastCursorPosition().deletePreviousChar();
            }
            toggleFrameVisibility(f);
        }

        // Print last solution and final status/stats
        cursor.setPosition(_lastSolutions[0].position());
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        nextInsertAt().insertFragment(QTextDocumentFragment(cursor));

        delete doc;
    }

    _lastTraceLoc = "";
    if (exitCode != 0) {
        QString msg = "Process finished with non-zero exit code %1.\n";
        nextInsertAt().insertText(msg.arg(exitCode), errorCharFormat());
    }
    auto t = IDEUtils::formatTime(time);
    nextInsertAt().insertText(QString("Finished in %1.").arg(t), noticeCharFormat());
    _statusCursor.insertText(t, infoCharFormat());
    _frame = ui->textBrowser->document()->rootFrame();
}


void OutputWidget::addMessageType(const QString &messageType)
{
    if (_messageTypeVisible.contains(messageType)) {
        return;
    }
    _messageTypeVisible[messageType] = true;

#ifdef Q_OS_MAC
    auto* button = new QCheckBox(messageType);
#else
    auto* button = new QPushButton(messageType);
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setCheckable(true);
#endif
    button->setChecked(true);

    auto* menuItem = ui->messageTypeMenu_pushButton->menu()->addAction(messageType, this, [=] () {
        toggleMessageTypeVisibility(messageType);
    });
    menuItem->setCheckable(true);
    connect(button, &QAbstractButton::toggled, this, [=](bool checked) { setMessageTypeVisibility(messageType, checked); });
    connect(this, &OutputWidget::messageTypeToggled, button, [=] (const QString& mt, bool visible) {
        if (messageType == mt) {
            button->setChecked(visible);
            menuItem->setChecked(visible);
        }
    });

    button->setMinimumWidth(1);
    ui->messageTypeButtons_horizontalLayout->addWidget(button);
    layoutButtons();
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
    if (_frame != ui->textBrowser->document()->rootFrame()) {
        // Can't clear in middle of run
        return;
    }

    _sections.clear();
    _messageTypeVisible.clear();
    ui->textBrowser->clear();
    ui->toggleAll_pushButton->setEnabled(false);
    ui->sectionMenu_pushButton->hide();
    ui->sectionMenu_pushButton->menu()->clear();
    ui->messageTypeMenu_pushButton->hide();
    ui->messageTypeMenu_pushButton->menu()->clear();
    _frame = ui->textBrowser->document()->rootFrame();
    qDeleteAll(ui->sectionButtons_widget->findChildren<QWidget*>("", Qt::FindDirectChildrenOnly));
    qDeleteAll(ui->messageTypeButtons_widget->findChildren<QWidget*>("", Qt::FindDirectChildrenOnly));
    ui->sectionButtons_widget->show();
    ui->messageTypeButtons_widget->show();
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

    QFontMetrics metrics(font);
    auto spaceNeeded = metrics.height() + 4;
    auto cwc = { QTextLength(QTextLength::FixedLength, spaceNeeded) };
    _headerTableFormat.setColumnWidthConstraints(cwc);

    _frameFormat.setLeftMargin(spaceNeeded);
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
    while (cursor <= end) {
        auto* childFrame = cursor.currentFrame();
        if (childFrame != nullptr && childFrame != frame) {
            auto childFormat = frame->frameFormat();
            auto childVisible = !childFormat.property(Property::Expanded).toBool();
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

#ifdef Q_OS_MAC
    auto* button = new QCheckBox(section);
#else
    auto* button = new QPushButton(section);
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setCheckable(true);
#endif
    button->setChecked(true);

    connect(button, &QAbstractButton::toggled, this, [=](bool checked) { setSectionVisibility(section, checked); });

    auto* menu = new QMenu(button);
    auto handler = [=] () { toggleSectionVisibility(section); };
    auto* toggleSection = menu->addAction("Show section", this, handler);
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
    connect(button, &QWidget::customContextMenuRequested, this, [=] (const QPoint &p) {
        toggleSection->setText(_sections[section] ? "Hide section" : "Show section");
        menu->exec(button->mapToGlobal(p));
    });

    auto* menuItem = ui->sectionMenu_pushButton->menu()->addAction(section, this, handler);
    menuItem->setCheckable(true);

    connect(this, &OutputWidget::sectionToggled, button, [=] (const QString& s, bool visible) {
        if (section == s) {
            button->setChecked(visible);
            menuItem->setChecked(visible);
        }
    });

    ui->toggleAll_pushButton->setEnabled(true);

    button->setMinimumWidth(1);

    ui->sectionButtons_horizontalLayout->addWidget(button);

    layoutButtons();
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
    if (object == ui->textBrowser->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton) {
            _pressed = true;
        }
    } else if (object == ui->textBrowser->viewport() && event->type() == QEvent::MouseMove) {
        auto* e = static_cast<QMouseEvent*>(event);
        if (_pressed) {
            _dragging = true;
        }
    } else if (object == ui->textBrowser->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton) {
            _pressed = false;
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
            copySelectionToClipboard(false);
            return true;
        }
    }
    return false;
}

void OutputWidget::copySelectionToClipboard(bool includeHidden)
{
    auto cursor = ui->textBrowser->textCursor();
    if (!cursor.hasSelection()) {
        return;
    }

    // Copy text to new document and remove hidden/object replacement characters
    auto* doc = new QTextDocument(this);
    QTextCursor c(doc);
    c.insertFragment(ui->textBrowser->textCursor().selection());
    c.setPosition(0);

    while (!c.atEnd()) {
        auto* table = c.currentTable();
        if (table != nullptr) {
            auto f = table->format();
            if (f.hasProperty(Property::Expanded)) {
                auto cell = table->cellAt(0, 0);
                auto cursor = cell.firstCursorPosition();
                cursor.setPosition(cell.lastPosition(), QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                if (!f.property(Property::Expanded).toBool()) {
                    cursor.setPosition(table->lastPosition() + 2);
                    cursor.setPosition(cursor.currentFrame()->lastPosition(), QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                }
            }
        }
        if (!includeHidden) {
            auto format = c.block().blockFormat();
            bool remove = format.hasProperty(Property::Section)
                    && !isSectionVisible(format.property(Property::Section).toString());
            remove |= format.hasProperty(Property::MessageType)
                    && !isMessageTypeVisible(format.property(Property::MessageType).toString());
            if (remove) {
                c.setPosition(c.block().position());
                if (!c.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor)) {
                    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                }
                c.removeSelectedText();
                continue;
            }
        }
        if (!c.movePosition(QTextCursor::NextBlock)) {
            break;
        }
    }

    IDEUtils::MimeDataExporter te;
    te.setDocument(doc);
    te.selectAll();
    QMimeData* mimeData = te.md();
    QApplication::clipboard()->setMimeData(mimeData);

    delete doc;
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

TextLayoutLock::TextLayoutLock(OutputWidget* o, bool scroll) : _o(o), _scroll(scroll) {
    if (_o->_frame != _o->ui->textBrowser->document()->rootFrame()) {
        auto frameVisible = _o->isFrameVisible(_o->_frame);
        if (!frameVisible) {
            _o->toggleFrameVisibility(_o->_frame);
        }
    }
    _o->ui->textBrowser->textCursor().beginEditBlock();
}

TextLayoutLock::~TextLayoutLock()
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

    QTextCursor cursor(doc);
    cursor.setPosition(posInDocument);
    auto t = cursor.currentTable();
    assert(t != nullptr);
    cursor.setPosition(t->lastPosition() + 2);
    auto* f = cursor.currentFrame();
    assert(f != nullptr);

    auto ff = f->frameFormat();
    bool expanded = ff.property(OutputWidget::Property::Expanded).toBool();
    
    auto tf = t->format();
    auto requiredFormat = static_cast<OutputWidget*>(parent())->headerTableFormat();
    if (tf.columnWidthConstraints()[0].rawValue() !=
            requiredFormat.columnWidthConstraints()[0].rawValue()) {
        // Re-align
        t->setFormat(requiredFormat);
        f->setFrameFormat(static_cast<OutputWidget*>(parent())->frameFormat());
    }

    auto h2 = metrics.ascent() / 2;
    auto w2 = h2 / 2;

    QPainterPath p;
    p.moveTo(-w2, -h2);
    p.lineTo(w2, 0);
    p.lineTo(-w2, h2);
    p.closeSubpath();
    painter->translate(rect.center());
    if (expanded) {
        painter->rotate(90);
    }
    auto infoFormat = static_cast<OutputWidget*>(parent())->infoCharFormat();
    painter->fillPath(p, infoFormat.foreground());
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

void OutputWidget::resizeEvent(QResizeEvent *e)
{
    layoutButtons();
}

void OutputWidget::layoutButtons()
{
    auto neededWidth = 0;
    auto spacing = ui->sectionButtons_horizontalLayout->spacing();
    for (auto* b : ui->sectionButtons_widget->findChildren<QAbstractButton*>()) {
        neededWidth += b->sizeHint().width() + spacing;
    }
    for (auto* b : ui->messageTypeButtons_widget->findChildren<QAbstractButton*>()) {
        neededWidth += b->sizeHint().width() + spacing;
    }
    auto availableWidth = ui->buttons_widget->width();

    if (availableWidth > neededWidth) {
        // Normal layout
        if (ui->sectionMenu_pushButton->isVisible()) {
            ui->sectionMenu_pushButton->hide();
            ui->messageTypeMenu_pushButton->hide();
            ui->sectionButtons_widget->show();
            ui->messageTypeButtons_widget->show();
        }
    } else {
        // Compact layout
        ui->sectionMenu_pushButton->setVisible(!_sections.empty());
        ui->messageTypeMenu_pushButton->setVisible(!_messageTypeVisible.empty());
        ui->sectionButtons_widget->hide();
        ui->messageTypeButtons_widget->hide();
    }
}

QTextCursor OutputWidget::nextInsertAt() const
{
    if (_solutionBuffer != nullptr) {
        QTextCursor cursor(_solutionBuffer);
        cursor.movePosition(QTextCursor::End);
        return cursor;
    }

    return _frame->lastCursorPosition();
}

void OutputWidget::onBrowserContextMenu(const QPoint& pos)
{
    _contextMenu->popup(ui->textBrowser->viewport()->mapToGlobal(pos));
}
