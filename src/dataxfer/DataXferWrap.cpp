#include "DataXferWrap.h"
#include "dataXfer.h"
#include <iostream>

std::string DataXferWrap::outCharBuffer;
std::mutex DataXferWrap::outCharBufferMutex;

DataXferWrap::DataXferWrap(IDataXferWrapCallbacks *callbacks)
{
    this->iDataXferWrapCallbacks = callbacks;
    initDataXfer();
}

DataXferWrap::~DataXferWrap()
{
    // This frees memory associated with any variables during the
    // initialization process.
    initDataXfer();
}

void DataXferWrap::onDataIn(const std::string &bytes, const unsigned int currentMilliseconds)
{
    for (auto c: bytes) {
        char c_in = c;
        char c_out;
        uint u_index;
        const char* sz_error;

        // If an item was received, pass it on.
        if (receiveVar(c_in, &c_out, &u_index, currentMilliseconds, &sz_error)) {
            if (u_index == CHAR_RECEIVED_INDEX) {
                std::string s(1, c_out);
                this->iDataXferWrapCallbacks->displayRawData(s);
            } else {
                ASSERT(u_index < NUM_XFER_VARS);
                // We received a variable. Obtains its contents from xferVar.
                std::string name(xferVar[u_index].psz_name);
                std::string description(xferVar[u_index].psz_desc);
                bool isModifiable = isVarWriteable(u_index);

                // Format the value per the specified format string.
                std::string value;
                char s_value[1024];
                int i_ret = formatVar(u_index, s_value, sizeof(s_value));
                if (i_ret < 0) {
                    value = "Error: cannot format value.";
                } else {
                    value = s_value;
                }

                // Pass it to the GUI.
#if 0
                // Debug info, if needed.
                std::cout << "u_index = " << u_index << ", name = " << name << ", description = " << description << ", value = " << value << ", isModifiable = " << isModifiable << "\n";
#endif
                this->iDataXferWrapCallbacks->variableUpdated(u_index, name, description,
                                                              value, isModifiable);
            }
        }
    }
}

extern "C" void outChar(uint8_t c)
{
    DataXferWrap::outCharBuffer.append(1, c);
}

const std::string& DataXferWrap::escapeDataOut(const std::string &typed)
{
    // Clear the output buffer, fill it with output charactesrs (outCharXfer will
    // call outChar above), then return that for transmission.

    std::lock_guard<std::mutex> guard(outCharBufferMutex);

    outCharBuffer.clear();
    for (auto c: typed) {
        outCharXfer(c);
    }
    return outCharBuffer;
}

void DataXferWrap::variableEdited(const unsigned int u_index, const std::string &newValue)
{
    ASSERT(u_index < NUM_XFER_VARS);
    ASSERT(isVarWriteable(u_index));

    // Convert the data.
    sscanf(newValue.data(), xferVar[u_index].psz_format, xferVar[u_index].pu8_data);

    // Send it. sendVar merely places characters in a buffer to be sent.
    // So, clear the buffer, fill it via sendVar, then send it.
    std::lock_guard<std::mutex> guard(outCharBufferMutex);
    outCharBuffer.clear();
    sendVar(u_index);
    this->iDataXferWrapCallbacks->sendRawData(outCharBuffer);
}
