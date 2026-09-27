#include "core.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace oli {

QString findTool(const QString &name) {
    return QStandardPaths::findExecutable(name);
}

QString canonicalQuant(const QString &label) {
    const QString l = label.trimmed().toLower();
    if (l == QLatin1String("q8_0")) return QStringLiteral("q8_0");
    if (l == QLatin1String("q5_k_m")) return QStringLiteral("q5_k_m");
    if (l == QLatin1String("f16")) return QStringLiteral("f16");
    return QStringLiteral("q4_k_m");
}

int pipelineStepCount(const ImportPlan &plan) {
    return plan.method == Method::FullModel ? 3 : 1;
}

QStringList buildStepArgv(const ImportPlan &plan, int stepIndex) {
    QStringList out;
    if (plan.method == Method::FullModel) {
        if (stepIndex == 0) {
            out = {QStringLiteral("python3"),
                   plan.llmCppCheckout + QStringLiteral("/convert_hf_to_gguf.py"),
                   plan.sourceDir,
                   QStringLiteral("--outtype"), QStringLiteral("f16"),
                   QStringLiteral("--outfile"),
                   plan.stagingDir + QStringLiteral("/temp_f16.gguf")};
        } else if (stepIndex == 1) {
            out = {QStringLiteral("llama-quantize"),
                   plan.stagingDir + QStringLiteral("/temp_f16.gguf"),
                   plan.stagingDir + QStringLiteral("/final.gguf"),
                   canonicalQuant(plan.quantLabel)};
        } else {
            out = {QStringLiteral("ollama"), QStringLiteral("create"),
                   plan.modelName, QStringLiteral("-f"),
                   plan.stagingDir + QStringLiteral("/Modelfile")};
        }
    } else {
        out = {QStringLiteral("ollama"), QStringLiteral("create"),
               plan.modelName, QStringLiteral("-f"),
               plan.stagingDir + QStringLiteral("/Modelfile")};
    }
    return out;
}

QString buildModelfileContent(const ImportPlan &plan) {
    switch (plan.method) {
    case Method::FullModel:
        return QStringLiteral("FROM %1/final.gguf\n").arg(plan.stagingDir);
    case Method::LoraAdapter:
        return QStringLiteral("FROM %1\nADAPTER %2\n").arg(plan.baseModel, plan.sourceDir);
    case Method::NativeFolder:
        return QStringLiteral("FROM %1\n").arg(plan.sourceDir);
    }
    return QString();
}

QStringList validatePlan(const ImportPlan &plan) {
    QStringList errors;
    if (plan.sourceDir.trimmed().isEmpty()) {
        errors << QStringLiteral("Source Safetensors directory is required.");
    } else if (!QFileInfo::exists(plan.sourceDir)) {
        errors << QStringLiteral("Source directory does not exist: %1").arg(plan.sourceDir);
    } else if (!QFileInfo(plan.sourceDir).isDir()) {
        errors << QStringLiteral("Source path is not a directory: %1").arg(plan.sourceDir);
    }
    if (plan.modelName.trimmed().isEmpty()) {
        errors << QStringLiteral("Target Ollama model name is required.");
    } else if (plan.modelName.contains(QLatin1Char(' ')) || plan.modelName.contains(QLatin1Char('/'))) {
        errors << QStringLiteral("Model name may not contain spaces or slashes.");
    }
    if (plan.method == Method::LoraAdapter && plan.baseModel.trimmed().isEmpty()) {
        errors << QStringLiteral("Base model is required for LoRA adapter imports.");
    }
    if (plan.method == Method::FullModel) {
        if (plan.llmCppCheckout.trimmed().isEmpty()) {
            errors << QStringLiteral("llama.cpp checkout path is required for full-model imports.");
        } else if (!QFileInfo::exists(plan.llmCppCheckout + QStringLiteral("/convert_hf_to_gguf.py"))) {
            errors << QStringLiteral("convert_hf_to_gguf.py not found in: %1").arg(plan.llmCppCheckout);
        }
    }
    return errors;
}

std::vector<PreflightItem> preflightChecks(const ImportPlan &plan) {
    std::vector<PreflightItem> checks;
    checks.push_back({PreflightItem::Kind::Executable, QStringLiteral("ollama"), {},
                      QStringLiteral("'ollama' was not found on PATH — is Ollama installed?")});
    checks.push_back({PreflightItem::Kind::Command, {},
                      {QStringLiteral("ollama"), QStringLiteral("list")},
                      QStringLiteral("Ollama is not reachable — is the ollama daemon running?")});
    if (plan.method == Method::FullModel) {
        checks.push_back({PreflightItem::Kind::Executable, QStringLiteral("python3"), {},
                          QStringLiteral("'python3' was not found on PATH.")});
        checks.push_back({PreflightItem::Kind::PathExists,
                          plan.llmCppCheckout + QStringLiteral("/convert_hf_to_gguf.py"), {},
                          QStringLiteral("convert_hf_to_gguf.py not found in the configured llama.cpp checkout.")});
        checks.push_back({PreflightItem::Kind::Executable, QStringLiteral("llama-quantize"), {},
                          QStringLiteral("'llama-quantize' was not found on PATH — build/install llama.cpp first.")});
    }
    return checks;
}

std::optional<QString> modelExistsError(const QString &ollamaListOutput, const QString &modelName) {
    const QString target = modelName.trimmed();
    const QString tagged = target + QStringLiteral(":latest");
    const QStringList lines = ollamaListOutput.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QStringList fields = line.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.isEmpty())
            continue;
        const QString name = fields.first();
        if (name == target || name == tagged) {
            return QStringLiteral("Model '%1' already exists in Ollama. Choose a different name, or delete it first.").arg(target);
        }
    }
    return std::nullopt;
}

QString safeModelComponent(const QString &modelName) {
    QString safe;
    for (const QChar &c : modelName) {
        safe += (c.isLetterOrNumber() || c == QLatin1Char('.') || c == QLatin1Char('-') || c == QLatin1Char('_'))
                    ? c
                    : QLatin1Char('_');
    }
    return safe.isEmpty() ? QStringLiteral("model") : safe;
}

QString defaultStagingDir(const QString &modelName) {
    QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cache.isEmpty())
        cache = QDir::homePath() + QStringLiteral("/.cache");
    return cache + QStringLiteral("/ollama-kde-importer/") + safeModelComponent(modelName);
}

bool removeStaging(const QString &stagingDir) {
    const QString path = QDir::cleanPath(stagingDir);
    if (path.isEmpty() || !QFileInfo(path).isAbsolute())
        return false;
    if (!path.contains(QStringLiteral("/ollama-kde-importer")))
        return false;
    return QDir(path).removeRecursively();
}

} // namespace oli