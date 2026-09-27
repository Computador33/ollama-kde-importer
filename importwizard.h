#pragma once
#include "core.h"
#include <QProcess>
#include <QWizard>
#include <vector>

class QComboBox;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTextEdit;

class ImportWizard : public QWizard {
    Q_OBJECT
public:
    explicit ImportWizard(QWidget *parent = nullptr);

    void accept() override;
    void reject() override;
    bool validateCurrentPage() override;

private:
    void createPage1();
    void createPage2();
    void createPage3();
    void updateMethodFields();
    oli::ImportPlan collectPlan() const;
    void beginImport();
    void runNext();
    void startProcess(const QStringList &argv);
    void writeModelfile();
    void onProcessOutput();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode);
    void fail(const QString &message);
    void finishSuccess();
    void setRunning(bool running);
    void setLog(const QString &text);

    QComboBox *m_method = nullptr;
    QLineEdit *m_sourceDir = nullptr;
    QPushButton *m_browse = nullptr;
    QLineEdit *m_modelName = nullptr;
    QLineEdit *m_baseModel = nullptr;
    QComboBox *m_quant = nullptr;
    QLineEdit *m_checkout = nullptr;
    QWizardPage *m_configPage = nullptr;

    QProgressBar *m_progress = nullptr;
    QTextEdit *m_log = nullptr;

    QProcess *m_proc = nullptr;
    QStringList m_currentArgv;
    QString m_listCapture;
    QString m_ollamaListOutput;

    oli::ImportPlan m_plan;
    std::vector<oli::PreflightItem> m_checks;
    int m_checkIndex = 0;
    std::vector<QStringList> m_pipeline;
    int m_stepIndex = 0;
    bool m_running = false;
};