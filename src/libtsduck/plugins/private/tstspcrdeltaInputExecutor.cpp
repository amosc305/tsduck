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
#include "tstspcrdeltaCore.h"
#include "tsGuardMutex.h"
#include "tsGuardCondition.h"


//----------------------------------------------------------------------------
// Constructor and destructor.
//----------------------------------------------------------------------------

ts::tspcrdelta::InputExecutor::InputExecutor(const PcrComparatorArgs& opt,
                                           const PluginEventHandlerRegistry& handlers,
                                           size_t index,
                                           Core& core,
                                           Report& log) :

    // Input threads have a high priority to be always ready to load incoming packets in the buffer.
    PluginExecutor(opt, handlers, PluginType::INPUT, opt.inputs[index], ThreadAttributes().setPriority(ThreadAttributes::GetHighPriority()), core, log),
    _input(dynamic_cast<InputPlugin*>(PluginThread::plugin())),
    _pluginIndex(index),
    _buffer(opt.bufferedPackets),
    _metadata(opt.bufferedPackets),
    _mutex(),
    _todo(),
    _outFirst(0),
    _outCount(0),
    _start_time(true) // initialized with current system time
{
    // Make sure that the input plugins display their index.
    setLogName(UString::Format(u"%s[%d]", {pluginName(), _pluginIndex}));
}

ts::tspcrdelta::InputExecutor::~InputExecutor()
{
    waitForTermination();
}


//----------------------------------------------------------------------------
// Implementation of TSP.
//----------------------------------------------------------------------------

size_t ts::tspcrdelta::InputExecutor::pluginIndex() const
{
    return _pluginIndex;
}

//----------------------------------------------------------------------------
// Invoked in the context of the plugin thread.
//----------------------------------------------------------------------------

void ts::tspcrdelta::InputExecutor::main()
{
    debug(u"input thread started");

    // Main loop. Each iteration is a complete input session.
    for (;;) {
        // Here, we need to start an input session.
        debug(u"starting input plugin");
        const bool started = _input->start();
        debug(u"input plugin started, status: %s", {started});

        // Loop on incoming packets.
        for (;;) {

            // Input area (first packet index and packet count).
            size_t inFirst = 0;
            size_t inCount = 0;

            // Initial sequence under mutex protection.
            {
                // Wait for free buffer or stop.
                GuardCondition lock(_mutex, _todo);
                while (_outCount >= _buffer.size()) {
                    // Not the current input plugin in --fast-switch mode.
                    // Drop older packets, free at most --max-input-packets.
                    assert(_outFirst < _buffer.size());
                    const size_t freeCount = std::min(_opt.maxInputPackets, _buffer.size() - _outFirst);
                    assert(freeCount <= _outCount);
                    _outFirst = (_outFirst + freeCount) % _buffer.size();
                    _outCount -= freeCount;
                }

                // There is some free buffer, compute first index and size of receive area.
                // The receive area is limited by end of buffer and max input size.
                inFirst = (_outFirst + _outCount) % _buffer.size();
                inCount = std::min(_opt.maxInputPackets, std::min(_buffer.size() - _outCount, _buffer.size() - inFirst));
            }

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

            // Fill input time stamps with monotonic clock if none was provided by the input plugin.
            // Only check the first returned packet. Assume that the input plugin generates time stamps for all or none.
            if (!_metadata[inFirst].hasInputTimeStamp()) {
                const NanoSecond current = Monotonic(true) - _start_time;
                for (size_t n = 0; n < inCount; ++n) {
                    _metadata[inFirst + n].setInputTimeStamp(current, NanoSecPerSec, TimeSource::TSP);
                }
            }

            // Pass packet to tspcrdelta core for analyzing
            TSPacket* pkt = &_buffer[inFirst];
            _core.analyzePacket(pkt, inCount, _pluginIndex);
        }
    }
}
