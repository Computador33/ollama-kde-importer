#include "core.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static oli::ImportPlan planFor(oli::Method method) {
    oli::ImportPlan p;
    p.method = method;
    p.sourceDir = QStringLiteral("/home/user/My Models");
    p.modelName = QStringLiteral("my-model");
    p.baseModel = QStringLiteral("llama3.2");
    p.quantLabel = QStringLiteral("Q4_K_M");
    p.llmCppCheckout = QStringLiteral("/home/user/llama.cpp");
    p.stagingDir = QStringLiteral("/home/user/.cache/ollama-kde-importer/import-test");
    return p;
}

static void testCanonicalQuant() {
    CHECK(oli::canonicalQuant(QStringLiteral("Q4_K_M")) == QStringLiteral("q4_k_m"));
    CHECK(oli::canonicalQuant(QStringLiteral("Q8_0")) == QStringLiteral("q8_0"));
    CHECK(oli::canonicalQuant(QStringLiteral("Q5_K_M")) == QStringLiteral("q5_k_m"));
    CHECK(oli::canonicalQuant(QStringLiteral("F16")) == QStringLiteral("f16"));
    CHECK(oli::canonicalQuant(QStringLiteral("bogus")) == QStringLiteral("q4_k_m"));
}

static void testPipelineStepCounts() {
    CHECK(oli::pipelineStepCount(planFor(oli::Method::FullModel)) == 3);
    CHECK(oli::pipelineStepCount(planFor(oli::Method::LoraAdapter)) == 1);
    CHECK(oli::pipelineStepCount(planFor(oli::Method::NativeFolder)) == 1);
}

static void testStepArgvKeepsPathsWithSpacesAsSingleArgument() {
    const oli::ImportPlan p = planFor(oli::Method::FullModel);
    const QStringList convert = oli::buildStepArgv(p, 0);
    CHECK(convert.size() == 7);
    CHECK(convert.at(0) == QStringLiteral("python3"));
    CHECK(convert.at(1) == QStringLiteral("/home/user/llama.cpp/convert_hf_to_gguf.py"));
    CHECK(convert.at(2) == QStringLiteral("/home/user/My Models"));       // single element, spaces intact
    CHECK(convert.at(0).contains(QLatin1Char(' ')) == false);             // no element is a full shell line
    const QStringList quantize = oli::buildStepArgv(p, 1);
    CHECK(quantize == (QStringList() << QStringLiteral("llama-quantize")
                       << p.stagingDir + QStringLiteral("/temp_f16.gguf")
                       << p.stagingDir + QStringLiteral("/final.gguf")
                       << QStringLiteral("q4_k_m")));
    const QStringList create = oli::buildStepArgv(p, 2);
    CHECK(create.at(0) == QStringLiteral("ollama"));
    CHECK(create.at(1) == QStringLiteral("create"));
    CHECK(create.at(2) == p.modelName);
    CHECK(create.at(4) == p.stagingDir + QStringLiteral("/Modelfile"));
}

static void testStepArgvForLoraAndNative() {
    const oli::ImportPlan lora = planFor(oli::Method::LoraAdapter);
    const QStringList loraStep = oli::buildStepArgv(lora, 0);
    CHECK(loraStep == (QStringList() << QStringLiteral("ollama") << QStringLiteral("create")
                       << lora.modelName << QStringLiteral("-f")
                       << lora.stagingDir + QStringLiteral("/Modelfile")));

    const oli::ImportPlan native = planFor(oli::Method::NativeFolder);
    const QStringList nativeStep = oli::buildStepArgv(native, 0);
    CHECK(nativeStep == (QStringList() << QStringLiteral("ollama") << QStringLiteral("create")
                         << native.modelName << QStringLiteral("-f")
                         << native.stagingDir + QStringLiteral("/Modelfile")));
}

static void testModelfileContentPerMethod() {
    const oli::ImportPlan full = planFor(oli::Method::FullModel);
    CHECK(oli::buildModelfileContent(full) ==
          QStringLiteral("FROM %1/final.gguf\n").arg(full.stagingDir));
    const oli::ImportPlan lora = planFor(oli::Method::LoraAdapter);
    CHECK(oli::buildModelfileContent(lora) ==
          QStringLiteral("FROM %1\nADAPTER %2\n").arg(lora.baseModel, lora.sourceDir));
    const oli::ImportPlan native = planFor(oli::Method::NativeFolder);
    CHECK(oli::buildModelfileContent(native) ==
          QStringLiteral("FROM %1\n").arg(native.sourceDir));
}

static void touchSafetensors(const QString &dir) {
    QFile w(dir + QStringLiteral("/model.safetensors"));
    CHECK(w.open(QIODevice::WriteOnly));
    w.write("dummy");
    w.close();
}

static void testValidatePlanRules() {
    QTemporaryDir source;                               // a *real* existing dir
    CHECK(source.isValid());
    touchSafetensors(source.path());                    // real weights like the HF cache

    oli::ImportPlan empty;
    CHECK(oli::validatePlan(empty).size() >= 2);                       // source dir + model name

    oli::ImportPlan badName = planFor(oli::Method::NativeFolder);
    badName.sourceDir = source.path();
    badName.modelName = QStringLiteral("my model name");
    CHECK(oli::validatePlan(badName).size() == 1);

    oli::ImportPlan noBase = planFor(oli::Method::LoraAdapter);
    noBase.sourceDir = source.path();
    noBase.baseModel.clear();
    CHECK(oli::validatePlan(noBase).size() == 1);

    oli::ImportPlan noCheckout = planFor(oli::Method::FullModel);
    noCheckout.sourceDir = source.path();
    noCheckout.llmCppCheckout.clear();
    CHECK(oli::validatePlan(noCheckout).size() == 1);

    oli::ImportPlan good = planFor(oli::Method::NativeFolder);
    good.sourceDir = source.path();
    CHECK(oli::validatePlan(good).isEmpty());
}

static void testValidatePlanRejectsDirWithoutSafetensors() {
    QTemporaryDir empty;                                // dir exists but holds no weights
    CHECK(empty.isValid());

    oli::ImportPlan full = planFor(oli::Method::FullModel);
    full.sourceDir = empty.path();
    const QStringList fullErrors = oli::validatePlan(full);
    CHECK(!fullErrors.isEmpty());
    CHECK(fullErrors.join(QLatin1Char('\n'))
              .contains(QStringLiteral(".safetensors")));   // the new rule fires

    oli::ImportPlan native = planFor(oli::Method::NativeFolder);
    native.sourceDir = empty.path();
    CHECK(!oli::validatePlan(native).isEmpty());
}

static void testValidatePlanAcceptsDirContainingSafetensors() {
    QTemporaryDir dir;
    QTemporaryDir checkout;
    CHECK(dir.isValid() && checkout.isValid());
    touchSafetensors(dir.path());

    QFile script(checkout.path() + QStringLiteral("/convert_hf_to_gguf.py"));
    CHECK(script.open(QIODevice::WriteOnly));
    script.write("#!/usr/bin/env python3\n");
    script.close();

    oli::ImportPlan full = planFor(oli::Method::FullModel);
    full.sourceDir = dir.path();
    full.llmCppCheckout = checkout.path();
    CHECK(oli::validatePlan(full).isEmpty());
}

static void testPreflightChecksCoverOllama() {
    const oli::ImportPlan native = planFor(oli::Method::NativeFolder);
    const auto checks = oli::preflightChecks(native);
    bool hasOllamaExec = false, hasListCommand = false;
    for (const auto &item : checks) {
        if (item.kind == oli::PreflightItem::Kind::Executable && item.target == QStringLiteral("ollama"))
            hasOllamaExec = true;
        if (item.kind == oli::PreflightItem::Kind::Command &&
            item.argv == (QStringList() << QStringLiteral("ollama") << QStringLiteral("list")))
            hasListCommand = true;
    }
    CHECK(hasOllamaExec);
    CHECK(hasListCommand);
}

static void testPreflightChecksIncludeMethod1Tools() {
    const oli::ImportPlan full = planFor(oli::Method::FullModel);
    const auto checks = oli::preflightChecks(full);
    bool hasPython = false, hasConvertScript = false, hasQuantize = false;
    for (const auto &item : checks) {
        if (item.kind == oli::PreflightItem::Kind::Executable && item.target == QStringLiteral("python3"))
            hasPython = true;
        if (item.kind == oli::PreflightItem::Kind::PathExists &&
            item.target == QStringLiteral("/home/user/llama.cpp/convert_hf_to_gguf.py"))
            hasConvertScript = true;
        if (item.kind == oli::PreflightItem::Kind::Executable && item.target == QStringLiteral("llama-quantize"))
            hasQuantize = true;
    }
    CHECK(hasPython);
    CHECK(hasConvertScript);
    CHECK(hasQuantize);
}

static void testModelExistsErrorBlocksExistingName() {
    const QString output =
        QStringLiteral("NAME          ID              SIZE      MODIFIED\n"
                       "llama3.2       a1b2c3          6.2 GB    2 hours ago\n"
                       "my-model:latest  d4e5f6        1.1 GB    5 minutes ago\n");
    CHECK(oli::modelExistsError(output, QStringLiteral("my-model")).has_value());
    CHECK(oli::modelExistsError(output, QStringLiteral("other")).has_value() == false);
}

static void testSafeModelComponentAndStagingDir() {
    CHECK(oli::safeModelComponent(QStringLiteral("my model/name")) == QStringLiteral("my_model_name"));
    CHECK(oli::safeModelComponent(QStringLiteral("ok-1.2")) == QStringLiteral("ok-1.2"));
    const QString dir = oli::defaultStagingDir(QStringLiteral("my-model"));
    CHECK(dir.contains(QStringLiteral("/ollama-kde-importer/my-model")));
}

static void testWizardSourceHasRequiredSafetySurface() {
    QFile wiz(QStringLiteral("importwizard.cpp"));
    CHECK(wiz.exists() && wiz.open(QIODevice::ReadOnly));
    const QString src = QString::fromUtf8(wiz.readAll());
    CHECK(src.contains(QStringLiteral("errorOccurred")));      // FailedToStart handled
    CHECK(src.contains(QStringLiteral("validateCurrentPage"))); // per-page validation
    CHECK(src.contains(QStringLiteral("terminate()")));         // graceful cancel
    CHECK(src.contains(QStringLiteral("removeStaging")));       // cleanup on cancel/success
    CHECK(src.contains(QStringLiteral("QDialog::accept")));     // wizard closes on completion
}

static void testRemoveStagingRemovesOnlyItsOwnDir() {
    QTemporaryDir outer;
    CHECK(outer.isValid());
    const QString staging = outer.path() + QStringLiteral("/ollama-kde-importer/import-test");
    QDir().mkpath(staging);
    QFile f(staging + QStringLiteral("/temp_f16.gguf"));
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("junk");
    f.close();
    // Refuses an absolute path that is outside an ollama-kde-importer tree:
    CHECK(oli::removeStaging(outer.path()) == false);
    CHECK(QDir(staging).exists());
    // Removes its own staging tree:
    CHECK(oli::removeStaging(staging) == true);
    CHECK(QDir(staging).exists() == false);
}

int main() {
    testCanonicalQuant();
    testPipelineStepCounts();
    testStepArgvKeepsPathsWithSpacesAsSingleArgument();
    testStepArgvForLoraAndNative();
    testModelfileContentPerMethod();
    testValidatePlanRules();
    testValidatePlanRejectsDirWithoutSafetensors();
    testValidatePlanAcceptsDirContainingSafetensors();
    testPreflightChecksCoverOllama();
    testPreflightChecksIncludeMethod1Tools();
    testModelExistsErrorBlocksExistingName();
    testSafeModelComponentAndStagingDir();
    testRemoveStagingRemovesOnlyItsOwnDir();
    testWizardSourceHasRequiredSafetySurface();
    if (g_failures) {
        std::printf("%d core test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("core tests PASS\n");
    return 0;
}