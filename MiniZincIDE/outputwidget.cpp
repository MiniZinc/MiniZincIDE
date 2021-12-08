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

    _headerTableFormat.setWidth(QTextLength(QTextLength::PercentageLength, 100));
    _headerTableFormat.setBorder(0);
    _headerTableFormat.setCellSpacing(0);
    _headerTableFormat.setProperty(Property::Expanded, true);

    QFontMetrics metrics(ui->textBrowser->font());
    auto spaceNeeded = metrics.height() + 4;
    auto cwc = { QTextLength(QTextLength::FixedLength, spaceNeeded) };
    _headerTableFormat.setColumnWidthConstraints(cwc);

    _arrowFormat.setObjectType(TextObject::Arrow);
    _arrowFormat.setProperty(Property::Expanded, true);
    _arrowFormat.setForeground(Qt::gray);

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

void OutputWidget::setDarkMode(bool darkMode)
{
    _noticeCharFormat.setForeground(Themes::currentTheme.functionColor.get(darkMode));
    _errorCharFormat.setForeground(Themes::currentTheme.errorColor.get(darkMode));
    _errorCharFormat.setForeground(Themes::currentTheme.errorColor.get(darkMode));
    _infoCharFormat.setForeground(Qt::gray);
    _commentCharFormat.setForeground(Themes::currentTheme.commentColor.get(darkMode));

    ui->textBrowser->viewport()->setStyleSheet(Themes::currentTheme.styleSheet(darkMode));
}

void OutputWidget::scrollToBottom()
{
    auto* scrollbar = ui->textBrowser->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void OutputWidget::startExecution(const QString& label)
{
    TextLayoutLock lock(this);
    auto c = ui->textBrowser->textCursor();
    c.movePosition(QTextCursor::End);

    // Create table containing heading above frame
    _rootTable = c.insertTable(2, 3, _headerTableFormat);
    _rootTable->mergeCells(1, 1, 1, 2);
    _rootTable->cellAt(1, 0).firstCursorPosition().block().setVisible(false);

    auto leftSideCursor = _rootTable->cellAt(0, 0).firstCursorPosition();
    leftSideCursor.insertText(QString(QChar::ObjectReplacementCharacter), _arrowFormat);

    auto headerCursor = _rootTable->cellAt(0, 1).firstCursorPosition();
    headerCursor.insertText(label, noticeCharFormat());

    auto rightSideCursor = _rootTable->cellAt(0, 2).firstCursorPosition();
    rightSideCursor.setBlockFormat(_rightAlignBlockFormat);
    _statusCursor = rightSideCursor;

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
                addTextToSection(section, "% " + it.second.toString(), _commentCharFormat);
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
                auto cursor = nextInsertAt();
                auto* t = cursor.insertTable(2, 2, _headerTableFormat);
                t->cellAt(0, 0).firstCursorPosition().insertText(QString(QChar::ObjectReplacementCharacter), _arrowFormat);
                auto label = QString("[ %1 more solutions ]").arg(_solutionCount - _firstCompressedSolution);
                t->cellAt(0, 1).firstCursorPosition().insertText(label, noticeCharFormat());
                t->cellAt(1, 0).firstCursorPosition().block().setVisible(false);
                t->cellAt(1, 1).firstCursorPosition().insertFragment(contents);
                toggleTableVisibility(t);
            }
            delete doc;
        }
    }
}

void OutputWidget::addCheckerOutput(const QVariantMap& output, const QStringList& order)
{
    // Print when we get solution
    _checkerOutput.clear();
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
            auto* t = nextInsertAt().insertTable(2, 2, _headerTableFormat);
            t->cellAt(0, 0).firstCursorPosition().insertText(QString(QChar::ObjectReplacementCharacter), _arrowFormat);
            auto label = QString("[ %1 more solutions ]").arg(_solutionCount - _firstCompressedSolution - 1);
            t->cellAt(0, 1).firstCursorPosition().insertText(label, noticeCharFormat());
            t->cellAt(1, 0).firstCursorPosition().block().setVisible(false);
            t->cellAt(1, 1).firstCursorPosition().insertFragment(QTextDocumentFragment(cursor));
            toggleTableVisibility(t);
        }

        // Print last solution
        cursor.setPosition(_lastSolutions[0].position());
        cursor.setPosition(_lastSolutions[1].position(), QTextCursor::KeepAnchor);
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
    _rootTable = nullptr;
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
    if (_rootTable != nullptr) {
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
    _rootTable = nullptr;
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
}

void OutputWidget::onAnchorClicked(const QUrl& link)
{
    emit anchorClicked(link);
}

void OutputWidget::toggleTableVisibility(QTextTable* table)
{
    auto format = table->format();
    if (!format.hasProperty(Property::Expanded)) {
        return;
    }

    TextLayoutLock lock(this, false);
    auto visible = !format.property(Property::Expanded).toBool();
    format.setProperty(Property::Expanded, visible);
    table->setFormat(format);

    auto arrowCell = table->cellAt(0, 0);
    auto arrowCursor = arrowCell.firstCursorPosition();
    arrowCursor.setPosition(arrowCell.lastPosition(), QTextCursor::KeepAnchor);
    auto arrowFormat = arrowCursor.charFormat();
    arrowFormat.setProperty(Property::Expanded, visible);
    arrowCursor.setCharFormat(arrowFormat);

    auto parentVisible = isFrameVisible(table->parentFrame());
    if (!parentVisible) {
        return;
    }

    // Set visibility of TextBlocks
    auto cell = table->cellAt(1, 1);
    for (auto c = cell.firstCursorPosition();
         c <= cell.lastCursorPosition();
         c.movePosition(QTextCursor::NextBlock)) {
        auto block = c.block();
        auto v = visible;
        auto* table = c.currentTable();
        if (v && table != nullptr) {
            QTextFrame* f = table;
            if (table->cellAt(c).row() == 0 && table->format().hasProperty(Property::Expanded)) {
                // Header is part of parent
                f = table->parentFrame();
            }
            while (v && f != nullptr) {
                auto format = f->format();
                if (format.hasProperty(Property::Expanded)) {
                    v &= format.property(Property::Expanded).toBool();
                }
                f = f->parentFrame();
            }
        }
        block.setVisible(v);
    }

    auto start = cell.firstPosition();
    auto length = cell.lastPosition() - start;
    ui->textBrowser->document()->markContentsDirty(start, length);
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
                            auto cell = table->cellAt(cursor);
                            if (cell.isValid() && cell.row() == 0) {
                                toggleTableVisibility(table);
                            }
                        }
                    }
                } else if (!cursor.charFormat().isAnchor()) {
                    auto* table = cursor.currentTable();
                    if (table != nullptr) {
                        auto cell = table->cellAt(cursor);
                        if (cell.isValid() && cell.row() == 0) {
                            toggleTableVisibility(table);
                        }
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
    if (_o->_rootTable != nullptr) {
        auto frameVisible = _o->isFrameVisible(_o->_rootTable);
        if (!frameVisible) {
            _o->toggleTableVisibility(_o->_rootTable);
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

    QTextCursor cursor(doc);
    cursor.setPosition(posInDocument);
    auto t = cursor.currentTable();
    if (t != nullptr) {
        auto f = t->format();
        auto requiredFormat = static_cast<OutputWidget*>(parent())->headerTableFormat();
        if (f.columnWidthConstraints()[0].rawValue() !=
                requiredFormat.columnWidthConstraints()[0].rawValue()) {
            t->setFormat(requiredFormat);
        }
    }

    auto h2 = metrics.ascent() / 2;
    auto w2 = h2 / 2;

    QPainterPath p;
    p.moveTo(-w2, -h2);
    p.lineTo(w2, 0);
    p.lineTo(-w2, h2);
    p.closeSubpath();
    painter->translate(rect.center());
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

    if (_rootTable != nullptr) {
        auto cursor = _rootTable->cellAt(1, 1).lastCursorPosition();
        return cursor;
    }

    QTextCursor cursor(ui->textBrowser->document());
    cursor.movePosition(QTextCursor::End);
    return cursor;
}
