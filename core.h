#pragma once
#include <QString>
#include <QStringList>
#include <optional>
#include <vector>

namespace oli {

enum class Method : int { FullModel = 1, LoraAdapter = 2, NativeFolder = 3 };

struct ImportPlan {
    Method method = Method::FullModel;
    QString sourceDir;      // Safetensors dir (all methods)
    QString modelName;      // target Ollama model name
    QString baseModel;      // Method 2 only
    QString quantLabel;     // friendly label: "Q4_K_M", "Q8_0", "Q5_K_M", "F16"
    QString llmCppCheckout; // Method 1 only: dir containing convert_hf_to_gguf.py
    QString stagingDir;     // per-run temp dir under ~/.cache/ollama-kde-importer/
};

struct PreflightItem {
    enum class Kind { Executable, PathExists, Command };
    Kind kind = Kind::Command;
    QString target;      // Executable / PathExists target
    QStringList argv;    // Command
    QString failMessage;
};

QString findTool(const QString &name);
QString canonicalQuant(const QString &label);
int pipelineStepCount(const ImportPlan &plan);
QStringList buildStepArgv(const ImportPlan &plan, int stepIndex);
QString buildModelfileContent(const ImportPlan &plan);
QStringList validatePlan(const ImportPlan &plan);
std::vector<PreflightItem> preflightChecks(const ImportPlan &plan);
std::optional<QString> modelExistsError(const QString &ollamaListOutput, const QString &modelName);
QString safeModelComponent(const QString &modelName);
QString stagingRoot();
QString defaultStagingDir(const QString &modelName);
bool removeStaging(const QString &stagingDir);

} // namespace oli