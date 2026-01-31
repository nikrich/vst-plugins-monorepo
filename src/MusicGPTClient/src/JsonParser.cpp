#include "JsonParser.h"

namespace musicgpt {

JsonParser::UploadResponse JsonParser::parseUploadResponse(const juce::String& json) {
    UploadResponse result;

    auto parsed = juce::JSON::parse(json);
    if (parsed.isVoid()) {
        result.errorMessage = "Failed to parse JSON response";
        return result;
    }

    auto* obj = parsed.getDynamicObject();
    if (!obj) {
        result.errorMessage = "Invalid JSON structure";
        return result;
    }

    // Check for error response
    if (obj->hasProperty("error")) {
        result.errorMessage = obj->getProperty("error").toString();
        if (obj->hasProperty("message"))
            result.errorMessage = obj->getProperty("message").toString();
        return result;
    }

    // Extract job ID
    if (obj->hasProperty("job_id")) {
        result.jobId = obj->getProperty("job_id").toString();
    } else if (obj->hasProperty("jobId")) {
        result.jobId = obj->getProperty("jobId").toString();
    } else if (obj->hasProperty("id")) {
        result.jobId = obj->getProperty("id").toString();
    }

    if (result.jobId.isEmpty()) {
        result.errorMessage = "No job ID in response";
        return result;
    }

    result.success = true;
    return result;
}

JsonParser::StatusResponse JsonParser::parseStatusResponse(const juce::String& json) {
    StatusResponse result;

    auto parsed = juce::JSON::parse(json);
    if (parsed.isVoid()) {
        result.errorMessage = "Failed to parse JSON response";
        result.errorType = ErrorType::ParseError;
        return result;
    }

    auto* obj = parsed.getDynamicObject();
    if (!obj) {
        result.errorMessage = "Invalid JSON structure";
        result.errorType = ErrorType::ParseError;
        return result;
    }

    // Check for error response
    if (obj->hasProperty("error")) {
        result.errorMessage = obj->getProperty("error").toString();
        if (obj->hasProperty("message"))
            result.errorMessage = obj->getProperty("message").toString();
        result.errorType = ErrorType::ServerError;
        return result;
    }

    // Extract job ID
    if (obj->hasProperty("job_id"))
        result.jobId = obj->getProperty("job_id").toString();
    else if (obj->hasProperty("jobId"))
        result.jobId = obj->getProperty("jobId").toString();
    else if (obj->hasProperty("id"))
        result.jobId = obj->getProperty("id").toString();

    // Extract status
    juce::String statusStr;
    if (obj->hasProperty("status"))
        statusStr = obj->getProperty("status").toString();
    result.status = parseJobStatus(statusStr);

    // Extract progress
    if (obj->hasProperty("progress"))
        result.progress = static_cast<float>(obj->getProperty("progress"));

    // Extract stems/results
    juce::var stemsVar;
    if (obj->hasProperty("stems"))
        stemsVar = obj->getProperty("stems");
    else if (obj->hasProperty("results"))
        stemsVar = obj->getProperty("results");
    else if (obj->hasProperty("outputs"))
        stemsVar = obj->getProperty("outputs");

    if (auto* stemsArray = stemsVar.getArray()) {
        for (const auto& stemVar : *stemsArray) {
            if (auto* stemObj = stemVar.getDynamicObject()) {
                StemResult stem;

                juce::String typeStr;
                if (stemObj->hasProperty("type"))
                    typeStr = stemObj->getProperty("type").toString();
                else if (stemObj->hasProperty("name"))
                    typeStr = stemObj->getProperty("name").toString();
                stem.type = parseStemType(typeStr);

                if (stemObj->hasProperty("url"))
                    stem.url = stemObj->getProperty("url").toString();
                else if (stemObj->hasProperty("download_url"))
                    stem.url = stemObj->getProperty("download_url").toString();

                result.stems.push_back(stem);
            }
        }
    }

    // Extract ETA if available
    if (obj->hasProperty("eta"))
        result.eta = obj->getProperty("eta").toString();

    // Parse conversion_path_wav (WAV URLs) - this is a JSON string that needs secondary parsing
    // Falls back to conversion_path (MP3 URLs) if WAV not available
    juce::String conversionPathStr;
    if (obj->hasProperty("conversion_path_wav")) {
        conversionPathStr = obj->getProperty("conversion_path_wav").toString();
    } else if (obj->hasProperty("conversion_path")) {
        conversionPathStr = obj->getProperty("conversion_path").toString();
    }

    if (conversionPathStr.isNotEmpty()) {
        auto conversionParsed = juce::JSON::parse(conversionPathStr);
        if (auto* conversionObj = conversionParsed.getDynamicObject()) {
            for (auto& prop : conversionObj->getProperties()) {
                juce::String stemName = prop.name.toString();
                juce::String stemUrl = prop.value.toString();
                StemType stemType = parseStemType(stemName);

                // Find existing stem or create new one
                bool found = false;
                for (auto& existingStem : result.stems) {
                    if (existingStem.type == stemType) {
                        existingStem.url = stemUrl;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    StemResult newStem;
                    newStem.type = stemType;
                    newStem.url = stemUrl;
                    result.stems.push_back(newStem);
                }
            }
        }
    }

    result.success = true;
    return result;
}

ErrorType JsonParser::classifyError(int httpStatus, const juce::String& responseBody) {
    if (httpStatus == 401 || httpStatus == 403)
        return ErrorType::AuthError;

    if (httpStatus == 400 || httpStatus == 422)
        return ErrorType::ValidationError;

    if (httpStatus == 429)
        return ErrorType::QuotaExceeded;

    if (httpStatus >= 500)
        return ErrorType::ServerError;

    if (httpStatus == 0)
        return ErrorType::NetworkError;

    // Try to parse error from body
    auto parsed = juce::JSON::parse(responseBody);
    if (auto* obj = parsed.getDynamicObject()) {
        juce::String errorCode;
        if (obj->hasProperty("code"))
            errorCode = obj->getProperty("code").toString().toLowerCase();
        else if (obj->hasProperty("error_code"))
            errorCode = obj->getProperty("error_code").toString().toLowerCase();

        if (errorCode.contains("auth") || errorCode.contains("token") || errorCode.contains("key"))
            return ErrorType::AuthError;
        if (errorCode.contains("quota") || errorCode.contains("limit") || errorCode.contains("rate"))
            return ErrorType::QuotaExceeded;
        if (errorCode.contains("valid") || errorCode.contains("format") || errorCode.contains("invalid"))
            return ErrorType::ValidationError;
    }

    return ErrorType::ServerError;
}

JobStatus JsonParser::parseJobStatus(const juce::String& statusStr) {
    juce::String lower = statusStr.toLowerCase();

    if (lower == "pending" || lower == "queued" || lower == "waiting")
        return JobStatus::Pending;
    if (lower == "processing" || lower == "running" || lower == "in_progress")
        return JobStatus::Processing;
    if (lower == "succeeded" || lower == "success" || lower == "completed" || lower == "done")
        return JobStatus::Succeeded;
    if (lower == "failed" || lower == "error")
        return JobStatus::Failed;
    if (lower == "cancelled" || lower == "canceled")
        return JobStatus::Cancelled;

    return JobStatus::Pending;
}

StemType JsonParser::parseStemType(const juce::String& stemStr) {
    juce::String lower = stemStr.toLowerCase();

    if (lower == "vocals" || lower == "voice" || lower == "vocal")
        return StemType::Vocals;
    if (lower == "drums" || lower == "drum" || lower == "percussion")
        return StemType::Drums;
    if (lower == "bass")
        return StemType::Bass;
    if (lower == "other" || lower == "others")
        return StemType::Other;
    if (lower == "instrumental" || lower == "accompaniment" || lower == "music")
        return StemType::Instrumental;

    return StemType::Other;
}

} // namespace musicgpt
