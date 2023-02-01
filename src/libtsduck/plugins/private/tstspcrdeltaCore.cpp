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

#include "tstspcrdeltaCore.h"
#include "tsGuardMutex.h"
#include "tsGuardCondition.h"
#include "tsAlgorithm.h"
#include "tsFatal.h"


//----------------------------------------------------------------------------
// Constructor and destructor.
//----------------------------------------------------------------------------

ts::tspcrdelta::Core::Core(const PcrComparatorArgs& opt, const PluginEventHandlerRegistry& handlers, Report& log) :
    _log(log),
    _opt(opt),
    _inputs(_opt.inputs.size(), nullptr),
    _output(_opt, handlers, *this, _log), // load output plugin and analyze options
    _mutex(),
    _gotInput(),
    _curPlugin(0),
    _curCycle(0),
    _terminate(false),
    _actions(),
    _events(),
    _pcrs(),
    _pcr_delta_threshold_in_ms(1),
    _output_stream(),
    _output_file(nullptr)
{
    // Load all input plugins, analyze their options.
    for (size_t i = 0; i < _inputs.size(); ++i) {
        _inputs[i] = new InputExecutor(opt, handlers, i, *this, log);
        CheckNonNull(_inputs[i]);
        // Set the asynchronous logger as report method for all executors.
        _inputs[i]->setReport(&_log);
        _inputs[i]->setMaxSeverity(_log.maxSeverity());
        // Initialize the PCR vector
        _pcrs.push_back({});
    }

    // Set the asynchronous logger as report method for output as well.
    _output.setReport(&_log);
    _output.setMaxSeverity(_log.maxSeverity());
}

ts::tspcrdelta::Core::~Core()
{
    // Deallocate all input plugins.
    // The destructor of each plugin waits for its termination.
    for (size_t i = 0; i < _inputs.size(); ++i) {
        delete _inputs[i];
    }
    _inputs.clear();
    _pcrs.clear();
}


//----------------------------------------------------------------------------
// Start the tspcrdelta processing.
//----------------------------------------------------------------------------

bool ts::tspcrdelta::Core::start()
{
    // Get all input plugin options.
    for (size_t i = 0; i < _inputs.size(); ++i) {
        if (!_inputs[i]->plugin()->getOptions()) {
            return false;
        }
    }

    // Start output plugin.
    if (!_output.plugin()->getOptions() ||  // Let plugin fetch its command line options.
        !_output.plugin()->start() ||       // Open the output "device", whatever it means.
        !_output.start())                   // Start the output thread.
    {
        return false;
    }

    // Create the output file if there is one
    if (_opt.output_name.empty()) {
        _output_file = &std::cerr;
    }
    else {
        _output_file = &_output_stream;
        _output_stream.open(_opt.output_name.toUTF8().c_str());
        if (!_output_stream) {
            return false;
        }
    }

    // Output header
    csvHeader();

    // Always start with the first input plugin.
    _curPlugin = 0;

    // Start all input threads (but do not open the input "devices").
    bool success = true;
    for (size_t i = 0; success && i < _inputs.size(); ++i) {
        // Here, start() means start the thread, not start input plugin.
        success = _inputs[i]->start();
    }

    if (!success) {
        // If one input thread could not start, abort all started threads.
        stop(false);
    }
    else {
        // Always start all input plugins, they continue to receive in parallel
        for (size_t i = 0; i < _inputs.size(); ++i) {
            _inputs[i]->startInput();
        }
    }

    return success;
}


//----------------------------------------------------------------------------
// Stop the tspcrdelta processing.
//----------------------------------------------------------------------------

void ts::tspcrdelta::Core::stop(bool success)
{
    // Wake up all threads waiting for something on the compare object.
    {
        GuardCondition lock(_mutex, _gotInput);
        _terminate = true;
        lock.signal();
    }

    // Tell the output plugin to terminate.
    _output.terminateOutput();

    // Tell all input plugins to terminate.
    for (size_t i = 0; success && i < _inputs.size(); ++i) {
        _inputs[i]->terminateInput();
    }
}

//----------------------------------------------------------------------------
// Invoked when the receive timeout expires.
// Implementation of WatchDogHandlerInterface.
//----------------------------------------------------------------------------

void ts::tspcrdelta::Core::handleWatchDogTimeout(WatchDog& watchdog)
{
    return;
}


//----------------------------------------------------------------------------
// Names of actions for debug messages.
//----------------------------------------------------------------------------

const ts::Enumeration ts::tspcrdelta::Core::_actionNames({
    {u"NONE",            NONE},
    {u"START",           START},
    {u"WAIT_STARTED",    WAIT_STARTED},
    {u"WAIT_INPUT",      WAIT_INPUT},
    {u"STOP",            STOP},
    {u"WAIT_STOPPED",    WAIT_STOPPED},
    {u"ABORT_INPUT",     ABORT_INPUT}
});


//----------------------------------------------------------------------------
// Stringify an Action object.
//----------------------------------------------------------------------------

ts::UString ts::tspcrdelta::Core::Action::toString() const
{
    return UString::Format(u"%s, %d, %s", {_actionNames.name(type), index, flag});
}


//----------------------------------------------------------------------------
// Operator "less" for containers of Action objects.
//----------------------------------------------------------------------------

bool ts::tspcrdelta::Core::Action::operator<(const Action& a) const
{
    if (type != a.type) {
        return type < a.type;
    }
    else if (index != a.index) {
        return index < a.index;
    }
    else {
        return int(flag) < int(a.flag);
    }
}


//----------------------------------------------------------------------------
// Enqueue an action.
//----------------------------------------------------------------------------

void ts::tspcrdelta::Core::enqueue(const Action& action, bool highPriority)
{
    _log.debug(u"enqueue action %s", {action});
    if (highPriority) {
        _actions.push_front(action);
    }
    else {
        _actions.push_back(action);
    }
}


//----------------------------------------------------------------------------
// Remove all instructions with type in bitmask.
//----------------------------------------------------------------------------

void ts::tspcrdelta::Core::cancelActions(int typeMask)
{
    for (auto it = _actions.begin(); it != _actions.end(); ) {
        // Check if the current action is one that must be removed.
        if ((int(it->type) & typeMask) != 0) {
            // Yes, remove instruction.
            _log.debug(u"cancel action %s", {*it});
            it = _actions.erase(it);
        }
        else {
            // No, keep it and move to next action.
            ++it;
        }
    }
}


//----------------------------------------------------------------------------
// Execute all commands until one needs to wait.
//----------------------------------------------------------------------------

void ts::tspcrdelta::Core::execute(const Action& event)
{
    // Set current event. Ignore flag in event.
    const Action eventNoFlag(event, false);
    if (event.type != NONE && !Contains(_events, eventNoFlag)) {
        // The event was not present.
        _events.insert(eventNoFlag);
        _log.debug(u"setting event: %s", {event});
    }

    // Loop on all enqueued commands.
    while (!_actions.empty()) {

        // Inspect front command. Will be dequeued if executed.
        const Action& action(_actions.front());
        _log.debug(u"executing action %s", {action});
        assert(action.index < _inputs.size());

        // Try to execute the front command. Return if wait is required.
        switch (action.type) {
            case NONE: {
                break;
            }
            case START: {
                _inputs[action.index]->startInput();
                break;
            }
            case STOP: {
                _inputs[action.index]->stopInput();
                break;
            }
            case ABORT_INPUT: {
                // Abort only if flag is set in action.
                if (action.flag && !_inputs[action.index]->abortInput()) {
                    _log.warning(u"input plugin %s does not support interruption, blocking may occur", {_inputs[action.index]->pluginName()});
                }
                break;
            }
            case WAIT_STARTED:
            case WAIT_INPUT:
            case WAIT_STOPPED: {
                // Wait commands, check if an event of this type is pending.
                const auto it = _events.find(Action(action, false));
                if (it == _events.end()) {
                    // Event not found, cannot execute further, keep the action in queue and retry later.
                    _log.debug(u"not ready, waiting: %s", {action});
                    return;
                }
                // Clear the event.
                _log.debug(u"clearing event: %s", {*it});
                _events.erase(it);
                break;
            }
            default: {
                // Unknown action.
                assert(false);
            }
        }

        // Command executed, dequeue it.
        _actions.pop_front();
    }
}


//----------------------------------------------------------------------------
// Get some packets to output (called by output plugin).
//----------------------------------------------------------------------------

bool ts::tspcrdelta::Core::getOutputArea(size_t& pluginIndex, TSPacket*& first, TSPacketMetadata*& data, size_t& count)
{
    assert(pluginIndex < _inputs.size());

    // Loop on _gotInput condition until the current input plugin has something to output.
    GuardCondition lock(_mutex, _gotInput);
    for (;;) {
        if (_terminate) {
            first = nullptr;
            count = 0;
        }
        else {
            _inputs[_curPlugin]->getOutputArea(first, data, count);
        }
        // Return when there is something to output in current plugin or the application terminates.
        if (count > 0 || _terminate) {
            // Tell the output plugin which input plugin is used.
            pluginIndex = _curPlugin;
            // Return false when the application terminates.
            return !_terminate;
        }
        // Otherwise, sleep on _gotInput condition.
        lock.waitCondition();
    }
}


//----------------------------------------------------------------------------
// Report output packets (called by output plugin).
//----------------------------------------------------------------------------

bool ts::tspcrdelta::Core::outputSent(size_t pluginIndex, size_t count)
{
    assert(pluginIndex < _inputs.size());

    // Inform the input plugin that the packets can be reused for input.
    // We notify the original input plugin from which the packets came.
    // The "current" input plugin may have changed in the meantime.
    _inputs[pluginIndex]->freeOutput(count);

    // Return false when the application terminates.
    return !_terminate;
}


//----------------------------------------------------------------------------
// Report completion of input start (called by input plugins).
//----------------------------------------------------------------------------

bool ts::tspcrdelta::Core::inputStarted(size_t pluginIndex, bool success)
{
    GuardMutex lock(_mutex);

    // Execute all commands if waiting on this event.
    execute(Action(WAIT_STARTED, pluginIndex, success));

    // Return false when the application terminates.
    return !_terminate;
}


//----------------------------------------------------------------------------
// Report input reception of packets (called by input plugins).
//----------------------------------------------------------------------------

bool ts::tspcrdelta::Core::inputReceived(size_t pluginIndex)
{
    GuardCondition lock(_mutex, _gotInput);

    // Execute all commands if waiting on this event. This may change the current input.
    execute(Action(WAIT_INPUT, pluginIndex));

    if (pluginIndex == _curPlugin) {
        // Wake up output plugin if it is sleeping, waiting for packets to output.
        lock.signal();
    }

    // Return false when the application terminates.
    return !_terminate;
}


//----------------------------------------------------------------------------
// Pass incoming TS packets for analyzing (called by input plugins).
//----------------------------------------------------------------------------
void ts::tspcrdelta::Core::analyzePacket(TSPacket*& pkt, size_t count, size_t pluginIndex)
{
    PCRs& pcrList = _pcrs.at(pluginIndex);
    for (size_t i = 0; i < count; i++)
    {
        GuardMutex lock(_mutex);
        uint64_t pcr = pkt[i].getPCR();
        const bool has_pcr = pcr != INVALID_PCR;
        if (has_pcr) {
            pcrList.push_back(pcr);
            comparePCRs(_pcrs);
        }
    }
}


//----------------------------------------------------------------------------
// Generate csv header
//----------------------------------------------------------------------------
void ts::tspcrdelta::Core::csvHeader()
{
    if (_opt.csv_format) {
        *_output_file << "PCR1" << _opt.separator
                      << "PCR2" << _opt.separator
                      << "PCR Delta" << _opt.separator
                      << "PCR Delta (ms)" << _opt.separator
                      << "Sync" << std::endl;
    }
}


//----------------------------------------------------------------------------
// Compare different between two PCRs
//----------------------------------------------------------------------------
void ts::tspcrdelta::Core::comparePCRs(PCRsVector& pcrs)
{
    if (pcrs.size() == 2) {
        PCRs& pcrList1 = pcrs.at(0);
        PCRs& pcrList2 = pcrs.at(1);

        if (pcrList1.size() > 0 && pcrList2.size() > 0) {
            int64_t pcrList1Front = pcrList1.front();
            int64_t pcrList2Front = pcrList2.front();
            int64_t pcrDelta = abs(pcrList1Front - pcrList2Front);
            double pcrDeltaInMs = (double) pcrDelta / (90000 * 300) * 1000;
            bool reachPcrDeltaThreshold = pcrDelta >= 0 && pcrDeltaInMs <= _pcr_delta_threshold_in_ms;

            *_output_file << pcrList1Front << _opt.separator
                          << pcrList2Front << _opt.separator
                          << pcrDelta << _opt.separator
                          << pcrDeltaInMs << _opt.separator
                          << reachPcrDeltaThreshold << std::endl;

            pcrList1.pop_front();
            pcrList2.pop_front();
        }
    }
}


//----------------------------------------------------------------------------
// Report completion of input session (called by input plugins).
//----------------------------------------------------------------------------

bool ts::tspcrdelta::Core::inputStopped(size_t pluginIndex, bool success)
{
    _log.debug(u"input %d completed, success: %s", {pluginIndex, success});
    bool stopRequest = false;

    // Locked sequence.
    {
        GuardMutex lock(_mutex);

        // Count end of cycle when the last plugin terminates.
        if (pluginIndex == _inputs.size() - 1) {
            _curCycle++;
        }

        // Check if the complete processing is terminated.
        stopRequest = _opt.terminate || (_opt.cycleCount > 0 && _curCycle >= _opt.cycleCount);

        if (stopRequest) {
            // Need to stop now. Remove any further action, except waiting for termination.
            cancelActions(~WAIT_STOPPED);
        }

        // Execute all commands if waiting on this event.
        execute(Action(WAIT_STOPPED, pluginIndex));
    }

    // Stop everything when we reach the end of the tspcrdelta processing.
    // This must be done outside the locked sequence to avoid deadlocks.
    if (stopRequest) {
        stop(true);
    }

    // Return false when the application terminates.
    return !_terminate;
}


//----------------------------------------------------------------------------
// Wait for completion of all plugins.
//----------------------------------------------------------------------------

void ts::tspcrdelta::Core::waitForTermination()
{
    // Wait for output termination.
    _output.waitForTermination();

    // Wait for all input termination.
    for (size_t i = 0; i < _inputs.size(); ++i) {
        _inputs[i]->waitForTermination();
    }
}
