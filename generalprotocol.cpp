#include "QObject"
#include "QVector"
#include "QDebug"
#include "generalprotocol.h"
#include "stdint.h"
#include "string.h"

#define BUFF_SIZE 64
#define CHANNEL_CNT 0x7
#define CHANNEL_ALL 0xFF

typedef enum {
    COMMAND_DATA_FLAG_8_BIT_SIZE  = 0,
    COMMAND_DATA_FLAG_16_BIT_SIZE = 1,
    COMMAND_DATA_FLAG_32_BIT_SIZE = 2,
    COMMAND_DATA_FLAG_SIZE_MASK   = 3,
}COMMAND_DATA_FLAG;

typedef enum {
    EXTERNAL_MODULE_COLDPALE = 0,
    EXTERNAL_MODULE_RGB_CAP  = 1,
} ExternalModule;

typedef enum{
    GP_DATA                   = 0,
    GP_STOP                   = 1,
    GP_START_CLOCK_WISE       = 2,
    GP_START_CONTR_CLOCK_WISE = 3,
    GP_START_AUTO_SWITCHER    = 4,
    GP_SET_MID                = 6,
    GP_GET_MID                = 7,
} GP_COMMAND;

#pragma pack(push, 1)
#define PROTOCOL_BUFF_SIZE (BUFF_SIZE - 1)
typedef struct {
    uint8_t  channel;
    uint8_t  flags;
    uint16_t cnt;
    uint8_t  data[];
}GpDataSubcommand;

typedef struct {
    uint8_t  channel;
}GpStartClockWiseSubcommand;

typedef struct {
    uint8_t  channel;
}GpStartContrClockWiseSubcommand;

typedef struct {
    uint8_t  channel;
    uint16_t offTime;
    uint16_t onTime;
    uint32_t cnt;
}GpStartAutoSwitcher;

typedef struct {
    uint8_t  channel;
}GpStoptSubcommand;

#define MAX_MID_DATA_LEN 20
typedef struct {
    uint8_t extModuleId;
    uint8_t midDataLen;
    uint8_t midData[MAX_MID_DATA_LEN];
}GpSetMidSubCommand;

typedef struct {
    uint8_t extModuleId;
    uint8_t midDataLen;
    uint8_t midData[MAX_MID_DATA_LEN];
}GpGetMidSubCommand;

typedef struct GpCommand{
    uint8_t headr;
    union {
        GpDataSubcommand                dataSubComand;
        GpStartClockWiseSubcommand      startClockWiseSubcommand;
        GpStartContrClockWiseSubcommand startContrClockWiseSubcommand;
        GpStoptSubcommand               stoptSubcommand;
        GpStartAutoSwitcher             startAutoSwitcher;
        GpSetMidSubCommand              setMidSubCommand;
        GpGetMidSubCommand              getMidSubCommand;
        uint8_t                         buffSubComand[PROTOCOL_BUFF_SIZE];
    };
} GpCommand;

typedef union {
    GpCommand command;
    uint8_t buff[sizeof(GpCommand)];
} GpCommandBuff ;
#pragma pack(pop)

generalProtocol::generalProtocol()
{
    //
}

generalProtocol::~generalProtocol()
{
    //
}

bool generalProtocol::gpDecode(uint8_t data[], uint32_t size)
{
    GpCommandBuff gpCommandBuff;
    memcpy((gpCommandBuff.buff), static_cast<uint8_t*>(data), size);
    switch(gpCommandBuff.command.headr) {

        case GP_GET_MID: {
            if (gpCommandBuff.command.getMidSubCommand.midDataLen > MAX_MID_DATA_LEN) {
                return false;
            }
            uint8_t dataSize = gpCommandBuff.command.getMidSubCommand.midDataLen;
            QVector<uint8_t> midDataRx(dataSize);
            for (uint8_t i = 0; i < dataSize; i++) {
                midDataRx[i] = gpCommandBuff.command.getMidSubCommand.midData[i];
            }
            switch (gpCommandBuff.command.getMidSubCommand.extModuleId) {
               case EXTERNAL_MODULE_COLDPALE:
                   emit gpColdPalteMidRx(midDataRx);
                   break;
                case EXTERNAL_MODULE_RGB_CAP:
                    emit gpRgbCapMidRx(midDataRx);
                   break;
                default: break;
            }
            break;
        }
        case GP_DATA:
            uint8_t sizeFlag = gpCommandBuff.command.dataSubComand.flags & COMMAND_DATA_FLAG_SIZE_MASK;
            QVector<uint8_t>  dataSize8;
            switch(sizeFlag) {
            case COMMAND_DATA_FLAG_8_BIT_SIZE: {
                    QVector<uint8_t>  dataSize8(gpCommandBuff.command.dataSubComand.cnt / sizeof(uint8_t));
                    uint8_t *rxType = (uint8_t*)(gpCommandBuff.command.dataSubComand.data);
                    for(uint16_t k = 0; k < gpCommandBuff.command.dataSubComand.cnt; k += sizeof(int8_t)) {
                        dataSize8[k / sizeof(uint8_t)] = (*rxType++);
                    }
                    emit gpDataRxSize8(dataSize8);
                }
                break;
            case COMMAND_DATA_FLAG_16_BIT_SIZE: {
                QVector<uint16_t> dataSize16(gpCommandBuff.command.dataSubComand.cnt / sizeof(uint16_t));
                uint16_t *rxType = (uint16_t*)gpCommandBuff.command.dataSubComand.data;
                for(uint16_t k = 0; k < gpCommandBuff.command.dataSubComand.cnt; k += sizeof(int16_t)) {
                    dataSize16[k / sizeof(uint16_t)] = (*rxType++);
                }
                emit gpDataRxSize16(dataSize16);
            }
                break;
/*
            case COMMAND_DATA_FLAG_32_BIT_SIZE:{
                QVector<double> dataSizeDouble(gpCommandBuff.command.dataSubComand.cnt / sizeof(uint32_t));
                uint32_t *rxType = (uint32_t*)(gpCommandBuff.command.dataSubComand.data);
                for(uint16_t k = 0; k < gpCommandBuff.command.dataSubComand.cnt; k += sizeof(int32_t)) {
                    dataSize32[k / sizeof(uint32_t)] = (*rxType++);
                }
                emit gpDataRxSize32(dataSize32);
            }
                break;
            }
*/
            case COMMAND_DATA_FLAG_32_BIT_SIZE:{
                QVector<double> dataSizeDouble(gpCommandBuff.command.dataSubComand.cnt / sizeof(uint32_t));
                uint32_t *rxType = (uint32_t*)(gpCommandBuff.command.dataSubComand.data);
                for(uint16_t k = 0; k < gpCommandBuff.command.dataSubComand.cnt; k += sizeof(int32_t)) {
                    dataSizeDouble[k / sizeof(uint32_t)] = static_cast<double>(*rxType++);
                }
                emit gpDataRxSizeDouble(dataSizeDouble);
            }
                break;
            }
            break;
    };
    return true;
}

void generalProtocol::gpStopCommandTx(uint8_t channel)
{
    QVector<uint8_t> gpCommandV;
    gpCommandV.resize(sizeof(GpCommand));
    GpCommand *gpCommand = (GpCommand*)(gpCommandV.data());
    gpCommand->headr = GP_STOP;
    gpCommand->stoptSubcommand.channel = static_cast<uint8_t>(channel);
    emit gpSend(gpCommandV);
}

void generalProtocol::gpStartClockWiseCommandTx(uint8_t channel)
{
    QVector<uint8_t> gpCommandV;
    gpCommandV.resize(sizeof(GpCommand));
    GpCommand *gpCommand = (GpCommand*)(gpCommandV.data());
    gpCommand->headr = GP_START_CLOCK_WISE;
    gpCommand->stoptSubcommand.channel = static_cast<uint8_t>(channel);
    emit gpSend(gpCommandV);
}

void generalProtocol::gpStartContrClockWiseCommandTx(uint8_t channel)
{
    QVector<uint8_t> gpCommandV;
    gpCommandV.resize(sizeof(GpCommand));
    GpCommand *gpCommand = (GpCommand*)(gpCommandV.data());
    gpCommand->headr = GP_START_CONTR_CLOCK_WISE;
    gpCommand->stoptSubcommand.channel = static_cast<uint8_t>(channel);
    emit gpSend(gpCommandV);
}

void generalProtocol::gpStartAutoSwitcherCommandTx(uint8_t channel, uint16_t offTime, uint16_t onTime)
{
    QVector<uint8_t> gpCommandV;
    gpCommandV.resize(sizeof(GpCommand));
    GpCommand *gpCommand = (GpCommand*)(gpCommandV.data());
    gpCommand->headr = GP_START_AUTO_SWITCHER;
    gpCommand->startAutoSwitcher.channel = static_cast<uint8_t>(channel);
    gpCommand->startAutoSwitcher.onTime  = onTime;
    gpCommand->startAutoSwitcher.offTime = offTime;
    emit gpSend(gpCommandV);
}

void generalProtocol::gpSetMidCommandTx(uint8_t module, uint8_t data[], uint8_t size)
{
    QVector<uint8_t> gpCommandV;
    gpCommandV.resize(sizeof(GpCommand));
    GpCommand *gpCommand = (GpCommand*)(gpCommandV.data());
    gpCommand->headr = GP_SET_MID;
    gpCommand->setMidSubCommand.extModuleId = module;
    gpCommand->setMidSubCommand.midDataLen = size;
    memcpy(gpCommand->setMidSubCommand.midData, data, size);
    emit gpSend(gpCommandV);
}

void generalProtocol::gpGetMidCommandTx(uint8_t module, uint8_t size)
{
    QVector<uint8_t> gpCommandV;
    gpCommandV.resize(sizeof(GpCommand));
    GpCommand *gpCommand = (GpCommand*)(gpCommandV.data());
    gpCommand->headr = GP_GET_MID;
    gpCommand->setMidSubCommand.extModuleId = module;
    gpCommand->setMidSubCommand.midDataLen = size;
    emit gpSend(gpCommandV);
}
