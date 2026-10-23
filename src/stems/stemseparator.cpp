#include "stems/stemseparator.h"

#include <sys/resource.h>
#include <unistd.h>

#include <QFile>
#include <thread>

#include "stems/htdemucsseparator.h"
#include "stems/spleeterseparator.h"

namespace mixxx {
namespace stems {

namespace {

// Nice value of the model threads: below the UI and far below the audio
constexpr int kModelThreadNiceness = 10;

struct ModelThread {
    std::thread thread;
};

OrtCustomThreadHandle createModelThread(
        void*, OrtThreadWorkerFn workerFn, void* pWorkerParam) {
    auto* pModelThread = new ModelThread;
    pModelThread->thread = std::thread([workerFn, pWorkerParam]() {
        setpriority(PRIO_PROCESS, static_cast<id_t>(gettid()), kModelThreadNiceness);
        workerFn(pWorkerParam);
    });
    return reinterpret_cast<OrtCustomThreadHandle>(pModelThread);
}

void joinModelThread(OrtCustomThreadHandle handle) {
    auto* pModelThread = reinterpret_cast<ModelThread*>(const_cast<OrtCustomHandleType*>(handle));
    pModelThread->thread.join();
    delete pModelThread;
}

int readMemInfoMb(const QByteArray& key) {
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly)) {
        return -1;
    }
    for (const QByteArray& line : file.readAll().split('\n')) {
        if (line.startsWith(key)) {
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 2) {
                return static_cast<int>(parts[1].toLongLong() / 1024);
            }
        }
    }
    return -1;
}

} // namespace

// static
std::unique_ptr<StemSeparator> StemSeparator::create(Model model) {
    switch (model) {
    case Model::Spleeter:
        return std::make_unique<SpleeterSeparator>();
    case Model::HTDemucs:
        return std::make_unique<HTDemucsSeparator>();
    }
    return nullptr;
}

// static
Ort::SessionOptions StemSeparator::sessionOptions(const Options& options) {
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(options.threads);
    sessionOptions.SetInterOpNumThreads(1);
    sessionOptions.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    // The arena keeps the peak memory of the largest run and made the runs
    // stall on Android
    sessionOptions.DisableCpuMemArena();
    sessionOptions.DisableMemPattern();
    sessionOptions.AddConfigEntry("session.use_device_allocator_for_initializers", "1");
    // Waiting threads sleep instead of spinning, leaving the CPU to the audio
    sessionOptions.AddConfigEntry("session.intra_op.allow_spinning", "0");
    sessionOptions.SetCustomCreateThreadFn(createModelThread);
    sessionOptions.SetCustomJoinThreadFn(joinModelThread);
    return sessionOptions;
}

int availableMemoryMb() {
    return readMemInfoMb(QByteArrayLiteral("MemAvailable:"));
}

int totalMemoryMb() {
    return readMemInfoMb(QByteArrayLiteral("MemTotal:"));
}

} // namespace stems
} // namespace mixxx
