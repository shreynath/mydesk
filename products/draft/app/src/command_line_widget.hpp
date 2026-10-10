#pragma once

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

namespace mydesk::desktop {

class CommandLineWidget : public QWidget {
    Q_OBJECT
public:
    explicit CommandLineWidget(QWidget* parent = nullptr);

    void appendHistory(const QString& line);
    void setPrompt(const QString& prompt);
    void focusInput();

signals:
    void commandSubmitted(const QString& text);

private:
    QPlainTextEdit* history_{nullptr};
    QLineEdit* input_{nullptr};
};

} // namespace mydesk::desktop
