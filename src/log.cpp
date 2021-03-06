//
// Created by gubin on 17-11-22.
//

#include "log.h"

#include <sys/stat.h>

#include "spdlog/spdlog.h"
#include "spdlog/logger.h"

#include <iostream>
#include <memory>
#include <cstdarg>

#include "operation_utils.h"

#include "json/json.hpp"

// for convenience
using nlohmann::json;
using std::string;
namespace spd = spdlog;


static void parse_time_offset(char* dst){
    int _offset = atoi(dst);
    int _hour = _offset / 100;
    int _minutes = _offset % 100;
    sprintf(dst, "%02d:%02d",_hour,_minutes);
}


class _iotseed_log_msg{
    public:
        string IOTSeedLogLevel;
        string IOTSeedClientID;
        string IOTSeedLogType;
        string IOTSeedMessage;
        string IOTSeedTS;
    public:
        _iotseed_log_msg(string clientID, string type, string msg){
            this->IOTSeedClientID = clientID;
            this->IOTSeedLogType = type;
            this->IOTSeedMessage = msg;
            auto now = time(nullptr);
            auto tm_time = spdlog::details::os::localtime(now);
            char date_buf[100];
            std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%dT%H:%M:%S%z", &tm_time);
            size_t _offset = strlen(date_buf) - 4;
            parse_time_offset(date_buf + _offset);
            this->IOTSeedTS = date_buf;
        }

};

static void to_json(json& j, const _iotseed_log_msg& p) {
    j = json{{"IOTSeedLogLevel", p.IOTSeedLogLevel},
                {"IOTSeedClientID", p.IOTSeedClientID},
                {"IOTSeedLogType", p.IOTSeedLogType},
                {"IOTSeedMessage", p.IOTSeedMessage},
                {"IOTSeedTS", p.IOTSeedTS}
    };
}

static inline ST_BOOLEAN iConfigIsNull(const IOTSEED_LOG_CONFIG* config ){
    if (nullptr == config){
        return SD_TRUE;
    }

    return SD_FALSE;
}

static spd::level::level_enum iotseed_loglevel_to_spd(const IOTSEED_LOG_LEVEL level){
    switch (level){
//        case Trace:
//            return spd::level::trace;
//        case Debug:
//            return spd::level::debug;
        case Info:
            return spd::level::info;
        case Warn:
            return spd::level::warn;
        case Err:
            return spd::level::err;
        case Critical:
            return spd::level::critical;
//        case Off:
//            return spd::level::off;
        default:
            return spd::level::info;
    }
}

static IOTSEED_LOG_CONFIG* iotseed_create_log_config(const char *name, const char *clientID){

    if(nullptr == name || strlen(name) > IOTSEED_LOG_NAME_MAX){
        fprintf(stderr, "log name is longer than max length:%d\n", IOTSEED_LOG_NAME_MAX);
        return nullptr;
    }
    auto pCnf = new IOTSEED_LOG_CONFIG();
    strcpy(pCnf->pattern,IOTSEED_LOG_PATTERN_DEFAULT);
    strcpy(pCnf->clientID, clientID);
    pCnf->log_level = Info;
    strcpy(pCnf->name , name);
    pCnf->logger_type = Normal;
    return pCnf;
}

static ST_VOID destroy_log_config(IOTSEED_LOG_CONFIG** config){
    delete(*config);
    *config = nullptr;
}



static ST_RET iotseed_create_spd_logger(IOTSEED_LOGGER *pLogger, const char *name, const char *path, const IOTSEED_LOGGER_TYPE eLogType, ...) {
    va_list args;
    va_start(args, eLogType);
    std::shared_ptr<spd::logger> logger;
    switch (eLogType) {
        case Normal:
            logger = spd::basic_logger_mt(name, path, true);
            break;
        case Rotate:
        {
            size_t file_size_bit = va_arg(args, size_t);
            size_t file_num = va_arg(args, size_t);
            logger = spd::rotating_logger_mt(name, path, file_size_bit, file_num);
        }
            break;
        case Daily:
        {
            int hour = va_arg(args, int);
            int minute = va_arg(args, int);
            logger = spd::daily_logger_mt(name,path, hour, minute);
            logger->flush_on(spd::level::warn);
        }
            break;
        default:
            logger = spd::stdout_color_mt(name);
            break;
    }

    va_end(args);

    strncpy(pLogger->config->name, name, strlen(name) + 1);

    logger->set_pattern(IOTSEED_LOG_PATTERN_DEFAULT);

    pLogger->spd_logger = logger.get();

    return SD_SUCCESS;

}



IOTSEED_LOGGER* iotseed_create_rotated_log(const char *name, const char *clientID, const char *path, const size_t file_size_bit, const size_t file_num) {
    IOTSEED_LOGGER* pLogger = new IOTSEED_LOGGER();
    if((pLogger->config = iotseed_create_log_config(name, clientID)) == nullptr){
        fprintf(stderr, "create log config failed\n");
        return nullptr;
    }

    iotseed_create_spd_logger(pLogger, name, path, Rotate, file_size_bit, file_num);
    return pLogger;

}


IOTSEED_LOGGER* iotseed_create_console_log(const char* name, const char *clientID) {
    IOTSEED_LOGGER* pLogger = new IOTSEED_LOGGER();
    if((pLogger->config = iotseed_create_log_config(name, clientID)) == nullptr){
        fprintf(stderr, "create log config failed\n");
        return nullptr;
    };

    iotseed_create_spd_logger(pLogger, name, nullptr, Console);
    return pLogger;
}


IOTSEED_LOGGER* iotseed_create_daily_log(const char *name, const char *clientID, const char *path, const int hour, const int minute) {
    IOTSEED_LOGGER* pLogger = (IOTSEED_LOGGER*) calloc(1, sizeof(IOTSEED_LOGGER));
    if((pLogger->config = iotseed_create_log_config(name, clientID)) == nullptr){
        fprintf(stderr, "create log config failed\n");
        return nullptr;
    };

    iotseed_create_spd_logger(pLogger, name, path, Daily, hour, minute);
    return pLogger;
}


ST_VOID iotseed_destroy_logger(IOTSEED_LOGGER **log){
    destroy_log_config(&((*log)->config));
    delete(*log);
    *log = nullptr;
}



ST_RET iotseed_log_sync_mode(ST_VOID){

    spd::set_sync_mode();

    return SD_SUCCESS;
}


ST_RET iotseed_log_async_mode(const size_t buf_size, const ST_UINT32 seconds){
    if(!judge_is_log2((int)buf_size)){
        return SD_FAILURE;
    }
    spd::set_async_mode(buf_size, spdlog::async_overflow_policy::block_retry, nullptr, std::chrono::seconds(seconds));

    return SD_SUCCESS;
}



ST_RET iotseed_set_log_level(IOTSEED_LOGGER* pLogger, const IOTSEED_LOG_LEVEL level){
    if (iConfigIsNull(pLogger->config)){
        return SD_FAILURE;
    }

    spd::get(pLogger->config->name)->set_level(iotseed_loglevel_to_spd(level));

    pLogger->config->log_level = level;

    return SD_SUCCESS;
}




ST_RET iotseed_set_log_format(IOTSEED_LOGGER* pLogger, const char* format){
    if (iConfigIsNull(pLogger->config)){
        return SD_FAILURE;
    }
    if(strlen(format) > IOTSEED_LOG_PATTERN_MAX){
        fprintf(stderr, "log lenght is greater than:%d\n", IOTSEED_LOG_PATTERN_MAX);
        return SD_FAILURE;
    }

    spd::get(pLogger->config->name)->set_pattern(format);

    strncpy(pLogger->config->pattern, format, strlen(format) + 1);

    return SD_SUCCESS;
}


ST_VOID iotseed_write_log(const IOTSEED_LOGGER* logger, IOTSEED_LOG_LEVEL level, const char* msg, IOTSEED_LOG_TYPE type){
    auto local_log = spd::get(logger->config->name);
    if (nullptr == msg || nullptr == local_log){
        return;
    }
    string _type;
    switch (type){
        case Log:
            _type = "log";
            break;
        case Recipe:
            _type = "recipe";
            break;
        default:
            fprintf(stderr,"type not defined\n");
            return;
    }
    auto message = _iotseed_log_msg(logger->config->clientID, _type, msg);
    json j;
    switch (level){
        case Info:
            message.IOTSeedLogLevel = "INFO";
            j = message;
            (local_log)->info(j.dump());
            break;
        case Warn:
            message.IOTSeedLogLevel = "WARNING";
            j = message;
            (local_log)->warn(j.dump());
            break;
        case Critical:
            message.IOTSeedLogLevel = "CRITICAL";
            j = message;
            (local_log)->critical(j.dump());
            break;
        case Err:
            message.IOTSeedLogLevel = "ERROR";
            j = message;
            (local_log)->error(j.dump());
            break;
        default:
            message.IOTSeedLogLevel = "INFO";
            j = message;
            (local_log)->info(j.dump());
            break;
    }

}

