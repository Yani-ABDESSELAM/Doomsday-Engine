#include "errorlogdialog.h"
#include "preferences.h"
//#include <QLabel>
//#include <QDialogButtonBox>
//#include <QTextEdit>
//#include <QVBoxLayout>

using namespace de;

DE_PIMPL(ErrorLogDialog)
{
//    QLabel *msg;
//    QTextEdit *text;

    Impl(Public *i) : Base(i)
    {
//        QVBoxLayout *layout = new QVBoxLayout;

//        msg = new QLabel;
//        layout->addWidget(msg, 0);

//        text = new QTextEdit;
//        text->setReadOnly(true);
//        text->setFont(Preferences::consoleFont());
//        QFontMetrics metrics(text->font());
//        text->setMinimumWidth(metrics.width("A") * 120);
//        text->setMinimumHeight(metrics.height() * 15);
//        layout->addWidget(text, 1);

//        QDialogButtonBox *buttons = new QDialogButtonBox;
//        buttons->addButton(QDialogButtonBox::Ok);
//        QObject::connect(buttons, SIGNAL(accepted()), thisPublic, SLOT(accept()));
//        layout->addWidget(buttons, 0);

//        self().setLayout(layout);
    }
};

ErrorLogDialog::ErrorLogDialog()
    : d(new Impl(this))
{
    title().setText("Error Log");
}

void ErrorLogDialog::setMessage(const String &messageText)
{
    message().setText(messageText);
}

void ErrorLogDialog::setLogContent(const String &text)
{
    //d->text->setText(text);
}
