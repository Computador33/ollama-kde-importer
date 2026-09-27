#include "importwizard.h"
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

ImportWizard::ImportWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("Ollama Safetensors Import Helper (Qt6/KF6)"));
    setWizardStyle(QWizard::ClassicStyle);
    createPage1();
    createPage2();
    createPage3();
}

void ImportWizard::createPage1() {
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Select Import Architecture Type"));
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("Choose how you want to bring your Safetensors into Ollama:")));
    m_method = new QComboBox(page);
    m_method->addItem(tr("Method 1: Full Model (conversion & quantization via llama.cpp)"),
                      static_cast<int>(oli::Method::FullModel));
    m_method->addItem(tr("Method 2: LoRA Fine-Tuning Adapter (overlay onto a base model)"),
                      static_cast<int>(oli::Method::LoraAdapter));
    m_method->addItem(tr("Method 3: Direct Native Folder Import (Ollama Safetensors import)"),
                      static_cast<int>(oli::Method::NativeFolder));
    connect(m_method, &QComboBox::currentIndexChanged, this, &ImportWizard::updateMethodFields);
    layout->addWidget(m_method);
    addPage(page);
}

void ImportWizard::createPage2() {
    m_configPage = new QWizardPage(this);
    m_configPage->setTitle(tr("Configure Import Paths"));
    auto *layout = new QVBoxLayout(m_configPage);

    layout->addWidget(new QLabel(tr("Source Safetensors directory:")));
    auto *srcRow = new QHBoxLayout();
    m_sourceDir = new QLineEdit(m_configPage);
    m_browse = new QPushButton(tr("Browse…"), m_configPage);
    srcRow->addWidget(m_sourceDir);
    srcRow->addWidget(m_browse);
    layout->addLayout(srcRow);
    connect(m_browse, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Safetensors Directory"), QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!dir.isEmpty())
            m_sourceDir->setText(dir);
    });

    layout->addWidget(new QLabel(tr("Target Ollama model name:")));
    m_modelName = new QLineEdit(m_configPage);
    layout->addWidget(m_modelName);

    layout->addWidget(new QLabel(tr("Base model (required for Method 2 only):")));
    m_baseModel = new QLineEdit(m_configPage);
    layout->addWidget(m_baseModel);

    layout->addWidget(new QLabel(tr("Quantization level (Method 1 only):")));
    m_quant = new QComboBox(m_configPage);
    m_quant->addItems({QStringLiteral("Q4_K_M"), QStringLiteral("Q8_0"),
                       QStringLiteral("Q5_K_M"), QStringLiteral("F16")});
    layout->addWidget(m_quant);

    layout->addWidget(new QLabel(tr("llama.cpp checkout path (Method 1 only):")));
    m_checkout = new QLineEdit(m_configPage);
    layout->addWidget(m_checkout);

    updateMethodFields();
    addPage(m_configPage);
}

void ImportWizard::createPage3() {
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Processing Pipeline"));
    auto *layout = new QVBoxLayout(page);
    m_progress = new QProgressBar(page);
    m_progress->setRange(0, 100);
    layout->addWidget(m_progress);
    m_log = new QTextEdit(page);
    m_log->setReadOnly(true);
    layout->addWidget(m_log);
    addPage(page);
}

void ImportWizard::updateMethodFields() {
    const oli::Method method = static_cast<oli::Method>(m_method->currentData().toInt());
    m_baseModel->setEnabled(method == oli::Method::LoraAdapter);
    m_quant->setEnabled(method == oli::Method::FullModel);
    m_checkout->setEnabled(method == oli::Method::FullModel);
}

oli::ImportPlan ImportWizard::collectPlan() const {
    oli::ImportPlan plan;
    plan.method = static_cast<oli::Method>(m_method->currentData().toInt());
    plan.sourceDir = m_sourceDir->text().trimmed();
    plan.modelName = m_modelName->text().trimmed();
    plan.baseModel = m_baseModel->text().trimmed();
    plan.quantLabel = m_quant->currentText();
    plan.llmCppCheckout = m_checkout->text().trimmed();
    return plan;
}

bool ImportWizard::validateCurrentPage() {
    if (currentPage() != m_configPage)
        return true;
    const QStringList errors = oli::validatePlan(collectPlan());
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Import configuration"),
                             errors.join(QStringLiteral("\n")));
        return false;
    }
    return true;
}

void ImportWizard::beginImport() {
    m_plan = collectPlan();
    const QStringList errors = oli::validatePlan(m_plan);
    if (!errors.isEmpty()) {
        setLog(tr("Configuration errors:\n  • ") + errors.join(QStringLiteral("\n  • ")));
        return;
    }
    m_plan.stagingDir = oli::defaultStagingDir(m_plan.modelName);
    QDir().mkpath(m_plan.stagingDir);
    m_checks = oli::preflightChecks(m_plan);
    m_checkIndex = 0;
    m_pipeline.clear();
    m_stepIndex = 0;
    m_listCapture.clear();
    m_ollamaListOutput.clear();
    setLog(tr("Starting import…"));
    setRunning(true);
    runNext();
}

void ImportWizard::runNext() {
    // Phase 1: preflight checks
    if (m_checkIndex < static_cast<int>(m_checks.size())) {
        const oli::PreflightItem &item = m_checks[static_cast<size_t>(m_checkIndex)];
        if (item.kind == oli::PreflightItem::Kind::Executable) {
            if (oli::findTool(item.target).isEmpty())
                fail(item.failMessage);
            else {
                ++m_checkIndex;
                runNext();
            }
            return;
        }
        if (item.kind == oli::PreflightItem::Kind::PathExists) {
            if (!QFileInfo::exists(item.target))
                fail(item.failMessage);
            else {
                ++m_checkIndex;
                runNext();
            }
            return;
        }
        setLog(tr("Preflight: %1").arg(item.argv.join(QLatin1Char(' '))));
        startProcess(item.argv);
        return;
    }
    // Phase 2: model-exists guard (after ollama list output)
    if (const auto existing = oli::modelExistsError(m_ollamaListOutput, m_plan.modelName)) {
        fail(*existing);
        return;
    }
    // Phase 3: pipeline steps
    if (m_pipeline.empty()) {
        const int count = oli::pipelineStepCount(m_plan);
        for (int i = 0; i < count; ++i)
            m_pipeline.push_back(oli::buildStepArgv(m_plan, i));
    }
    if (m_stepIndex >= static_cast<int>(m_pipeline.size())) {
        finishSuccess();
        return;
    }
    const QStringList &argv = m_pipeline[static_cast<size_t>(m_stepIndex)];
    if (argv.contains(QLatin1String("create")))
        writeModelfile();
    if (m_plan.method == oli::Method::FullModel)
        m_progress->setValue(25 * (m_stepIndex + 1));   // 25/50/75
    else
        m_progress->setValue(50);                       // preflight done → 50
    setLog(tr("Step %1/%2: %3").arg(m_stepIndex + 1).arg(m_pipeline.size()).arg(argv.join(QLatin1Char(' '))));
    startProcess(argv);
}

void ImportWizard::startProcess(const QStringList &argv) {
    if (m_proc)
        m_proc->deleteLater();
    m_proc = new QProcess(this);
    m_currentArgv = argv;
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &ImportWizard::onProcessOutput);
    connect(m_proc, &QProcess::readyReadStandardError, this, &ImportWizard::onProcessOutput);
    connect(m_proc, &QProcess::errorOccurred, this, &ImportWizard::onProcessError);
    connect(m_proc, &QProcess::finished, this, &ImportWizard::onProcessFinished);
    m_proc->start(argv.value(0), argv.mid(1));
}

void ImportWizard::writeModelfile() {
    const QString path = m_plan.stagingDir + QStringLiteral("/Modelfile");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        fail(tr("Cannot write Modelfile: %1").arg(path));
        return;
    }
    file.write(oli::buildModelfileContent(m_plan).toUtf8());
    file.close();
    setLog(tr("Wrote %1").arg(path));
}

void ImportWizard::onProcessOutput() {
    const QByteArray out = m_proc->readAllStandardOutput();
    const QByteArray err = m_proc->readAllStandardError();
    if (!out.isEmpty()) {
        setLog(QString::fromUtf8(out).trimmed());
        if (m_currentArgv.value(0) == QLatin1String("ollama") && m_currentArgv.contains(QLatin1String("list")))
            m_listCapture += QString::fromUtf8(out);
    }
    if (!err.isEmpty())
        setLog(QString::fromUtf8(err).trimmed());
}

void ImportWizard::onProcessError(QProcess::ProcessError error) {
    const QString detail = m_proc ? m_proc->errorString() : QString();
    fail(tr("Failed to start '%1': %2").arg(m_currentArgv.value(0), detail));
    Q_UNUSED(error);
}

void ImportWizard::onProcessFinished(int exitCode) {
    if (exitCode != 0) {
        fail(tr("Step failed — exit code %1").arg(exitCode));
        return;
    }
    if (m_currentArgv.value(0) == QLatin1String("ollama") && m_currentArgv.contains(QLatin1String("list")))
        m_ollamaListOutput = m_listCapture;
    if (m_checkIndex < static_cast<int>(m_checks.size())) {
        ++m_checkIndex;
        runNext();
    } else {
        ++m_stepIndex;
        runNext();
    }
}

void ImportWizard::fail(const QString &message) {
    setLog(tr("<font color='red'>%1</font>").arg(message.toHtmlEscaped()));
    setRunning(false);
    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}

void ImportWizard::finishSuccess() {
    oli::removeStaging(m_plan.stagingDir);
    m_progress->setValue(100);
    setRunning(false);
    setLog(tr("Import complete: %1").arg(m_plan.modelName));
    QDialog::accept();
}

void ImportWizard::setRunning(bool running) {
    m_running = running;
    button(QWizard::BackButton)->setEnabled(!running);
    button(QWizard::NextButton)->setEnabled(!running);
    button(QWizard::FinishButton)->setEnabled(!running);
}

void ImportWizard::setLog(const QString &text) {
    m_log->append(text);
}

void ImportWizard::accept() {
    if (m_running)
        return; // Finish is disabled while running; belt-and-suspenders
    beginImport();
}

void ImportWizard::reject() {
    if (m_running && m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        if (!m_proc->waitForFinished(3000))
            m_proc->kill();
    }
    if (!m_plan.stagingDir.isEmpty())
        oli::removeStaging(m_plan.stagingDir);
    QDialog::reject();
}