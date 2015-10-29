#pragma once

#include <functional>
#include "system_tick_hal.h"

namespace particle { namespace protocol {

#ifndef PRODUCT_ID
#define PRODUCT_ID (0xffff)
#endif

#ifndef PRODUCT_FIRMWARE_VERSION
#define PRODUCT_FIRMWARE_VERSION (0xffff)
#endif

enum ProtocolError
{
	NO_ERROR,
	PING_TIMEOUT,
	TRANSPORT_FAILURE,
	INVALID_STATE,
	INSUFFICIENT_STORAGE,
	MALFORMED_MESSAGE,
};

typedef uint16_t chunk_index_t;

const chunk_index_t NO_CHUNKS_MISSING = 65535;
const chunk_index_t MAX_CHUNKS = 65535;
const size_t MISSED_CHUNKS_TO_SEND = 50;
const size_t MAX_FUNCTION_ARG_LENGTH = 64;
const size_t MAX_FUNCTION_KEY_LENGTH = 12;
const size_t MAX_VARIABLE_KEY_LENGTH = 12;
const size_t MAX_EVENT_NAME_LENGTH = 64;
const size_t MAX_EVENT_DATA_LENGTH = 64;
const size_t MAX_EVENT_TTL_SECONDS = 16777215;

#ifndef PROTOCOL_BUFFER_SIZE
    #if PLATFORM_ID<2
        #define PROTOCOL_BUFFER_SIZE 640
    #else
        #define PROTOCOL_BUFFER_SIZE 800
    #endif
#endif


namespace ChunkReceivedCode {
  enum Enum {
    OK = 0x44,
    BAD = 0x80
  };
}

enum DescriptionType {
    DESCRIBE_SYSTEM = 1<<1,            // modules
    DESCRIBE_APPLICATION = 1<<2,       // functions and variables
};



typedef std::function<system_tick_t()> millis_callback;
typedef std::function<int()> callback;


}}