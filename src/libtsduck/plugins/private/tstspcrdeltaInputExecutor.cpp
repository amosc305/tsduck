//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2022, Thierry Lelegard
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------

#include "tstspcrdeltaInputExecutor.h"
#include "tsPcrComparator.h"

#if defined(TS_NEED_STATIC_CONST_DEFINITIONS)
constexpr size_t ts::InputExecutor::MAX_INPUT_PACKETS;
constexpr size_t ts::InputExecutor::BUFFERED_PACKETS;
#endif

//----------------------------------------------------------------------------
// Constructor and destructor.
//----------------------------------------------------------------------------

ts::InputExecutor::InputExecutor(const PcrComparatorArgs& opt,
                                    size_t index,
                                    PcrComparator& comparator,
                                    Report& log) :

    // Input threads have a high priority to be always ready to load incoming packets in the buffer.
    PluginThread(&log, opt.appName, PluginType::INPUT, opt.inputs[index], ThreadAttributes().setPriority(ThreadAttributes::GetHighPriority())),
    _opt(opt),
    _comparator(comparator),
    _input(dynamic_cast<InputPlugin*>(PluginThread::plugin())),
    _pluginIndex(index),
    _buffer(BUFFERED_PACKETS),
    _metadata(BUFFERED_PACKETS),
    _terminate(false)
{
    // Make sure that the input plugins display their index.
    setLogName(UString::Format(u"%s[%d]", {pluginName(), _pluginIndex}));
}

ts::InputExecutor::~InputExecutor()
{
    waitForTermination();
}


//----------------------------------------------------------------------------
// Implementation of TSP. We do not use "joint termination" in tspcrdelta.
//----------------------------------------------------------------------------

void ts::InputExecutor::useJointTermination(bool)
{
}

void ts::InputExecutor::jointTerminate()
{
}

bool ts::InputExecutor::useJointTermination() const
{
    return false;
}

bool ts::InputExecutor::thisJointTerminated() const
{
    return false;
}

size_t ts::InputExecutor::pluginCount() const
{
    return _opt.inputs.size();
}

void ts::InputExecutor::signalPluginEvent(uint32_t event_code, Object* plugin_data) const
{
}


//----------------------------------------------------------------------------
// Implementation of TSP.
//----------------------------------------------------------------------------

size_t ts::InputExecutor::pluginIndex() const
{
    return _pluginIndex;
}


//----------------------------------------------------------------------------
// Terminate input.
//----------------------------------------------------------------------------

void ts::InputExecutor::terminateInput()
{
    debug(u"received terminate request");
    _terminate = true;
}


//----------------------------------------------------------------------------
// Invoked in the context of the plugin thread.
//----------------------------------------------------------------------------

void ts::InputExecutor::main()
{
    debug(u"input thread started");

    // Main loop. Each iteration is a complete input session.
    for (;;) {
        // Here, we need to start an input session.
        debug(u"starting input plugin");
        const bool started = _input->start();
        debug(u"input plugin started, status: %s", {started});

        // Exit main loop when termination is requested.
        if (_terminate) {
            break;
        }

        // Loop on incoming packets.
        for (;;) {

            // Input area (first packet index and packet count).
            size_t inFirst = 0;
            size_t inCount = MAX_INPUT_PACKETS;

            // Assertion failure if inFirst or inFirst + inCount larger than buffer size
            assert(inFirst < _buffer.size());
            assert(inFirst + inCount <= _buffer.size());

            // Reset packet metadata.
            for (size_t n = inFirst; n < inFirst + inCount; ++n) {
                _metadata[n].reset();
            }

            // Receive packets.
            if ((inCount = _input->receive(&_buffer[inFirst], &_metadata[inFirst], inCount)) == 0) {
                // End of input.
                debug(u"received end of input from plugin");
                break;
            }

            // Pass packet to comparator for analyzing
            TSPacket* pkt = &_buffer[inFirst];
            TSPacketMetadata* metadata = &_metadata[inFirst];
            _comparator.analyzePacket(pkt, metadata, inCount, _pluginIndex);
        }
    }
}
