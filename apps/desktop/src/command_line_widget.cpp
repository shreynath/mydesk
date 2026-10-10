#include "command_line_widget.hpp"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace mydesk::desktop {

CommandLineWidget::CommandLineWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    history_ = new QPlainTextEdit(this);
    history_->setReadOnly(true);
    history_->setMaximumBlockCount(500);
    history_->setPlaceholderText("Command history");
    history_->setStyleSheet("QPlainTextEdit { background: #121820; color: #d7e2ec; "
                            "font-family: 'IBM Plex Mono', 'SF Mono', Menlo, monospace; "
                            "font-size: 12px; border: 1px solid #2a3644; }");

    input_ = new QLineEdit(this);
    input_->setPlaceholderText("Type a command (L, C, UNDO, …)");
    input_->setStyleSheet("QLineEdit { background: #0e141c; color: #f0f4f8; "
                          "font-family: 'IBM Plex Mono', 'SF Mono', Menlo, monospace; "
                          "font-size: 13px; padding: 6px; border: 1px solid #3a4a5c; }");

    layout->addWidget(history_, 1);
    layout->addWidget(input_, 0);

    connect(input_, &QLineEdit::returnPressed, this, [this]() {
        const QString text = input_->text();
        input_->clear();
        emit commandSubmitted(text);
    });
}

void CommandLineWidget::appendHistory(const QString& line) {
    history_->appendPlainText(line);
}

void CommandLineWidget::setPrompt(const QString& prompt) {
    input_->setPlaceholderText(prompt.isEmpty() ? QStringLiteral("Type a command") : prompt);
}

void CommandLineWidget::focusInput() { input_->setFocus(Qt::OtherFocusReason); }

} // namespace mydesk::desktop
