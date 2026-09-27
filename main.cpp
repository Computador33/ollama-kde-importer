#include "importwizard.h"
#include <QApplication>
#include <QTimer>
#include <KAboutData>
#include <KLocalizedString>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    KAboutData aboutData(
        QStringLiteral("ollama-kde-importer"),
        i18n("Ollama Safetensors Import Helper"),
        QStringLiteral("1.0.0"),
        i18n("KDE utility to convert and import Safetensors into Ollama on CachyOS."),
        KAboutLicense::GPL,
        i18n("(C) 2026 Developer"));
    KAboutData::setApplicationData(aboutData);

    if (app.arguments().contains(QStringLiteral("--ui-selfcheck"))) {
        ImportWizard wizard;
        wizard.show();
        QTimer::singleShot(600, &wizard, &QDialog::accept);
        return app.exec();
    }

    ImportWizard wizard;
    wizard.show();
    return app.exec();
}