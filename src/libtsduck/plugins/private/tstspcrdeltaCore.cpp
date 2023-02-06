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
    _mutex(),
    _gotInput(),
    _curCycle(0),
    _terminate(false),
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

    // Start all input threads (but do not open the input "devices").
    bool success = true;
    for (size_t i = 0; success && i < _inputs.size(); ++i) {
        // Here, start() means start the thread, and start input plugin.
        success = _inputs[i]->start();
    }

    if (!success) {
        // If one input thread could not start, abort all started threads.
        stop(false);
    }

    return success;
}


//----------------------------------------------------------------------------
// Stop the tspcrdelta processing.
//----------------------------------------------------------------------------

void ts::tspcrdelta::Core::stop(bool success)
{
    return;
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
// Wait for completion of all plugins.
//----------------------------------------------------------------------------

void ts::tspcrdelta::Core::waitForTermination()
{
    // Wait for all input termination.
    for (size_t i = 0; i < _inputs.size(); ++i) {
        _inputs[i]->waitForTermination();
    }
}
