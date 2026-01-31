#include "ExtractionClient.h"
#include "CurlHttpClient.h"
#include "JsonParser.h"
#include <deque>
#include <fstream>

namespace musicgpt {

namespace {
    // Debug logging - writes to ~/Documents/MusicGPTExtractor.log
    void debugLog(const juce::String& message) {
        static juce::File logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("MusicGPTExtractor.log");
        static bool initialized = false;

        if (!initialized) {
            logFile.deleteFile();
            initialized = true;
        }

        juce::String timestamp = juce::Time::getCurrentTime().toString(true, true, true, true);
        juce::String line = "[" + timestamp + "] " + message + "\n";
        logFile.appendText(line);
    }
    juce::String stemTypeToString(StemType type) {
        juce::StringArray stems;
        if (hasStem(type, StemType::Vocals)) stems.add("vocals");
        if (hasStem(type, StemType::Drums)) stems.add("drums");
        if (hasStem(type, StemType::Bass)) stems.add("bass");
        if (hasStem(type, StemType::Other)) stems.add("other");
        if (hasStem(type, StemType::Instrumental)) stems.add("instrumental");
        return stems.joinIntoString(",");
    }

    juce::String stemTypeToFileSuffix(StemType type) {
        switch (type) {
            case StemType::Vocals: return "_vocals";
            case StemType::Drums: return "_drums";
            case StemType::Bass: return "_bass";
            case StemType::Other: return "_other";
            case StemType::Instrumental: return "_instrumental";
            default: return "_stem";
        }
    }
}

struct ExtractionJob {
    juce::String jobId;
    juce::String remoteJobId;
    juce::File audioFile;
    StemType requestedStems;
    ProgressCallback onProgress;
    CompletionCallback onComplete;
    std::atomic<bool> cancelled{false};
};

struct PendingUpdate {
    enum class Type { Progress, Completion };
    Type type;
    juce::String jobId;
    ProgressInfo progress;
    ExtractionResult result;
};

class ExtractionClient::Impl : public juce::Thread {
public:
    Impl(ExtractionClient* owner, const ExtractionConfig& config)
        : juce::Thread("MusicGPT Extraction Worker")
        , owner_(owner)
        , config_(config)
    {
        httpClient_.setApiKey(config.apiKey);
        httpClient_.setBaseUrl(config.apiEndpoint);
        httpClient_.setConnectionTimeout(config.connectionTimeoutMs);
        httpClient_.setTransferTimeout(config.transferTimeoutMs);
        httpClient_.setValidateCertificates(config.validateCertificates);
    }

    ~Impl() override {
        cancelAll();
        stopThread(5000);
    }

    juce::String queueJob(
        const juce::File& audioFile,
        StemType stems,
        ProgressCallback onProgress,
        CompletionCallback onComplete
    ) {
        auto job = std::make_shared<ExtractionJob>();
        job->jobId = juce::Uuid().toString();
        job->audioFile = audioFile;
        job->requestedStems = stems;
        job->onProgress = std::move(onProgress);
        job->onComplete = std::move(onComplete);

        {
            juce::ScopedLock lock(queueLock_);
            jobQueue_.push_back(job);
            jobMap_[job->jobId] = job;
        }

        if (!isThreadRunning())
            startThread();

        return job->jobId;
    }

    void cancelJob(const juce::String& jobId) {
        juce::ScopedLock lock(queueLock_);
        auto it = jobMap_.find(jobId);
        if (it != jobMap_.end()) {
            it->second->cancelled.store(true);
        }
        httpClient_.cancel();
    }

    void cancelAll() {
        juce::ScopedLock lock(queueLock_);
        for (auto& pair : jobMap_) {
            pair.second->cancelled.store(true);
        }
        httpClient_.cancel();
    }

    bool isBusy() const {
        juce::ScopedLock lock(queueLock_);
        return !jobQueue_.empty() || currentJob_ != nullptr;
    }

    int getActiveJobCount() const {
        juce::ScopedLock lock(queueLock_);
        return static_cast<int>(jobQueue_.size()) + (currentJob_ != nullptr ? 1 : 0);
    }

    void deliverPendingUpdates() {
        std::vector<PendingUpdate> updates;
        {
            juce::ScopedLock lock(updateLock_);
            updates.swap(pendingUpdates_);
        }

        for (const auto& update : updates) {
            std::shared_ptr<ExtractionJob> job;
            {
                juce::ScopedLock lock(queueLock_);
                auto it = jobMap_.find(update.jobId);
                if (it != jobMap_.end())
                    job = it->second;
            }

            if (!job) continue;

            if (update.type == PendingUpdate::Type::Progress) {
                if (job->onProgress)
                    job->onProgress(update.progress);
            } else {
                if (job->onComplete)
                    job->onComplete(update.result);

                // Remove completed job from map
                juce::ScopedLock lock(queueLock_);
                jobMap_.erase(update.jobId);
            }
        }
    }

private:
    void run() override {
        while (!threadShouldExit()) {
            std::shared_ptr<ExtractionJob> job;

            {
                juce::ScopedLock lock(queueLock_);
                if (jobQueue_.empty()) {
                    currentJob_ = nullptr;
                    break;
                }
                job = jobQueue_.front();
                jobQueue_.pop_front();
                currentJob_ = job;
            }

            if (job->cancelled.load()) {
                completeJob(job, makeResult(job->jobId, JobStatus::Cancelled, ErrorType::Cancelled, "Job cancelled"));
                continue;
            }

            processJob(job);
        }
    }

    void processJob(std::shared_ptr<ExtractionJob> job) {
        ExtractionResult result;
        result.jobId = job->jobId;

        debugLog("=== Starting extraction job: " + job->jobId);
        debugLog("Audio file: " + job->audioFile.getFullPathName());
        debugLog("Requested stems: " + stemTypeToString(job->requestedStems));
        debugLog("API endpoint: " + config_.apiEndpoint);

        // Phase 1: Upload
        reportProgress(job, ProgressInfo::Phase::Uploading, 0.0f, "Uploading audio file...");
        debugLog("Phase 1: Starting upload to /Extraction");

        std::vector<std::pair<juce::String, juce::String>> formFields;
        formFields.push_back({"stems", stemTypeToString(job->requestedStems)});

        auto uploadResponse = httpClient_.postMultipart(
            "/Extraction",
            job->audioFile,
            "audio",
            formFields,
            [this, job](float p) {
                if (!job->cancelled.load())
                    reportProgress(job, ProgressInfo::Phase::Uploading, p, "Uploading...");
            }
        );

        debugLog("Upload response - success: " + juce::String(uploadResponse.success ? "true" : "false"));
        debugLog("Upload response - status code: " + juce::String(uploadResponse.statusCode));
        debugLog("Upload response - body: " + uploadResponse.body.substring(0, 2000));
        if (uploadResponse.errorMessage.isNotEmpty())
            debugLog("Upload response - error: " + uploadResponse.errorMessage);

        if (job->cancelled.load()) {
            debugLog("Job cancelled during upload");
            completeJob(job, makeResult(job->jobId, JobStatus::Cancelled, ErrorType::Cancelled, "Job cancelled"));
            return;
        }

        if (!uploadResponse.success) {
            auto errorType = uploadResponse.errorMessage.isNotEmpty()
                ? ErrorType::NetworkError
                : JsonParser::classifyError(static_cast<int>(uploadResponse.statusCode), uploadResponse.body);
            debugLog("Upload failed - errorType: " + juce::String(static_cast<int>(errorType)));
            completeJob(job, makeResult(job->jobId, JobStatus::Failed, errorType,
                uploadResponse.errorMessage.isNotEmpty() ? uploadResponse.errorMessage : "Upload failed"));
            return;
        }

        auto uploadResult = JsonParser::parseUploadResponse(uploadResponse.body);
        debugLog("Parse upload response - success: " + juce::String(uploadResult.success ? "true" : "false"));
        debugLog("Parse upload response - jobId: " + uploadResult.jobId);
        if (uploadResult.errorMessage.isNotEmpty())
            debugLog("Parse upload response - error: " + uploadResult.errorMessage);

        if (!uploadResult.success) {
            debugLog("Failed to parse upload response");
            completeJob(job, makeResult(job->jobId, JobStatus::Failed, ErrorType::ParseError, uploadResult.errorMessage));
            return;
        }

        job->remoteJobId = uploadResult.jobId;
        debugLog("Remote job ID assigned: " + job->remoteJobId);

        // Phase 2: Poll for status
        debugLog("Phase 2: Starting status polling");
        reportProgress(job, ProgressInfo::Phase::Processing, 0.0f, "Processing...");

        JsonParser::StatusResponse statusResult;
        int retryCount = 0;
        int pollCount = 0;

        while (!job->cancelled.load() && !threadShouldExit()) {
            Thread::sleep(config_.pollIntervalMs);
            pollCount++;

            juce::String statusUrl = "/byId?conversionType=EXTRACTION&task_id=" + job->remoteJobId;
            debugLog("Poll #" + juce::String(pollCount) + " - GET " + statusUrl);

            auto statusResponse = httpClient_.get(statusUrl);

            debugLog("Status response - success: " + juce::String(statusResponse.success ? "true" : "false"));
            debugLog("Status response - status code: " + juce::String(statusResponse.statusCode));
            debugLog("Status response - body: " + statusResponse.body.substring(0, 2000));
            if (statusResponse.errorMessage.isNotEmpty())
                debugLog("Status response - error: " + statusResponse.errorMessage);

            if (job->cancelled.load()) {
                debugLog("Job cancelled during polling");
                break;
            }

            if (!statusResponse.success) {
                retryCount++;
                debugLog("Status request failed, retry count: " + juce::String(retryCount));
                if (retryCount >= config_.maxRetries) {
                    debugLog("Max retries exceeded, failing job");
                    completeJob(job, makeResult(job->jobId, JobStatus::Failed, ErrorType::NetworkError, "Failed to get job status"));
                    return;
                }
                Thread::sleep(config_.retryDelayMs * retryCount);
                continue;
            }

            retryCount = 0;
            statusResult = JsonParser::parseStatusResponse(statusResponse.body);

            debugLog("Parsed status - success: " + juce::String(statusResult.success ? "true" : "false"));
            debugLog("Parsed status - status: " + juce::String(static_cast<int>(statusResult.status)));
            debugLog("Parsed status - progress: " + juce::String(statusResult.progress));
            debugLog("Parsed status - stems count: " + juce::String(static_cast<int>(statusResult.stems.size())));
            if (statusResult.errorMessage.isNotEmpty())
                debugLog("Parsed status - error: " + statusResult.errorMessage);

            if (!statusResult.success) {
                debugLog("Failed to parse status response");
                completeJob(job, makeResult(job->jobId, JobStatus::Failed, statusResult.errorType, statusResult.errorMessage));
                return;
            }

            reportProgress(job, ProgressInfo::Phase::Processing, statusResult.progress, "Processing...");

            if (statusResult.status == JobStatus::Succeeded) {
                debugLog("Job succeeded, moving to download phase");
                break;
            }

            if (statusResult.status == JobStatus::Failed) {
                debugLog("Job failed on server");
                completeJob(job, makeResult(job->jobId, JobStatus::Failed, statusResult.errorType, statusResult.errorMessage));
                return;
            }
        }

        if (job->cancelled.load()) {
            debugLog("Job cancelled after polling loop");
            completeJob(job, makeResult(job->jobId, JobStatus::Cancelled, ErrorType::Cancelled, "Job cancelled"));
            return;
        }

        // Phase 3: Download stems
        debugLog("Phase 3: Starting stem downloads");
        debugLog("Number of stems to download: " + juce::String(static_cast<int>(statusResult.stems.size())));
        debugLog("Output directory: " + config_.outputDirectory.getFullPathName());
        reportProgress(job, ProgressInfo::Phase::Downloading, 0.0f, "Downloading stems...");

        result.status = JobStatus::Succeeded;
        juce::String baseName = job->audioFile.getFileNameWithoutExtension();

        for (size_t i = 0; i < statusResult.stems.size(); ++i) {
            if (job->cancelled.load()) {
                debugLog("Job cancelled during download");
                completeJob(job, makeResult(job->jobId, JobStatus::Cancelled, ErrorType::Cancelled, "Job cancelled"));
                return;
            }

            auto& stem = statusResult.stems[i];
            juce::String fileName = baseName + stemTypeToFileSuffix(stem.type) + ".wav";
            juce::File destFile = config_.outputDirectory.getChildFile(fileName);

            debugLog("Downloading stem " + juce::String(static_cast<int>(i + 1)) + "/" + juce::String(static_cast<int>(statusResult.stems.size())));
            debugLog("  URL: " + stem.url);
            debugLog("  Destination: " + destFile.getFullPathName());

            float baseProgress = static_cast<float>(i) / static_cast<float>(statusResult.stems.size());
            float progressRange = 1.0f / static_cast<float>(statusResult.stems.size());

            bool downloadSuccess = httpClient_.downloadFile(
                stem.url,
                destFile,
                [this, job, baseProgress, progressRange](float p) {
                    if (!job->cancelled.load())
                        reportProgress(job, ProgressInfo::Phase::Downloading, baseProgress + p * progressRange, "Downloading stems...");
                }
            );

            debugLog("  Download result: " + juce::String(downloadSuccess ? "success" : "failed"));

            if (!downloadSuccess) {
                debugLog("Failed to download stem: " + fileName);
                completeJob(job, makeResult(job->jobId, JobStatus::Failed, ErrorType::FileIOError, "Failed to download stem: " + fileName));
                return;
            }

            StemResult resultStem;
            resultStem.type = stem.type;
            resultStem.file = destFile;
            resultStem.url = stem.url;
            result.stems.push_back(resultStem);
        }

        result.error = ErrorType::None;
        debugLog("=== Extraction completed successfully");
        completeJob(job, result);
    }

    void reportProgress(std::shared_ptr<ExtractionJob> job, ProgressInfo::Phase phase, float progress, const juce::String& message) {
        PendingUpdate update;
        update.type = PendingUpdate::Type::Progress;
        update.jobId = job->jobId;
        update.progress.phase = phase;
        update.progress.progress = progress;
        update.progress.message = message;

        {
            juce::ScopedLock lock(updateLock_);
            pendingUpdates_.push_back(update);
        }

        owner_->triggerAsyncUpdate();
    }

    void completeJob(std::shared_ptr<ExtractionJob> job, const ExtractionResult& result) {
        PendingUpdate update;
        update.type = PendingUpdate::Type::Completion;
        update.jobId = job->jobId;
        update.result = result;

        {
            juce::ScopedLock lock(updateLock_);
            pendingUpdates_.push_back(update);
        }

        {
            juce::ScopedLock lock(queueLock_);
            if (currentJob_ == job)
                currentJob_ = nullptr;
        }

        owner_->triggerAsyncUpdate();
    }

    ExtractionResult makeResult(const juce::String& jobId, JobStatus status, ErrorType error, const juce::String& message) {
        ExtractionResult result;
        result.jobId = jobId;
        result.status = status;
        result.error = error;
        result.errorMessage = message;
        return result;
    }

    ExtractionClient* owner_;
    ExtractionConfig config_;
    CurlHttpClient httpClient_;

    mutable juce::CriticalSection queueLock_;
    std::deque<std::shared_ptr<ExtractionJob>> jobQueue_;
    std::map<juce::String, std::shared_ptr<ExtractionJob>> jobMap_;
    std::shared_ptr<ExtractionJob> currentJob_;

    juce::CriticalSection updateLock_;
    std::vector<PendingUpdate> pendingUpdates_;
};

// ExtractionClient implementation

ExtractionClient::ExtractionClient(const ExtractionConfig& config)
    : impl_(std::make_unique<Impl>(this, config))
{
}

ExtractionClient::~ExtractionClient() {
    juce::AsyncUpdater::cancelPendingUpdate();
}

juce::String ExtractionClient::extractStems(
    const juce::File& audioFile,
    StemType stems,
    ProgressCallback onProgress,
    CompletionCallback onComplete
) {
    return impl_->queueJob(audioFile, stems, std::move(onProgress), std::move(onComplete));
}

void ExtractionClient::cancelJob(const juce::String& jobId) {
    impl_->cancelJob(jobId);
}

void ExtractionClient::cancelAll() {
    impl_->cancelAll();
}

bool ExtractionClient::isBusy() const {
    return impl_->isBusy();
}

int ExtractionClient::getActiveJobCount() const {
    return impl_->getActiveJobCount();
}

void ExtractionClient::handleAsyncUpdate() {
    impl_->deliverPendingUpdates();
}

} // namespace musicgpt
