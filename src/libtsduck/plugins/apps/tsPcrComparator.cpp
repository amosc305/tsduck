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

#include "tsPcrComparator.h"
#include "tstspcrdeltaInputExecutor.h"

//----------------------------------------------------------------------------
// Constructors and destructor.
//----------------------------------------------------------------------------

ts::PcrComparator::PcrComparator(const PcrComparatorArgs& args, Report& report) :
    _report(report),
    _args(args),
    _success(false),
    _inputs(),
    _mutex(),
    _latency_threshold(args.latencyThreshold),
    _output_stream(),
    _output_file(nullptr)
{
    // Debug message.
    if (_report.debug()) {
        UString cmd(args.appName);
        cmd.append(u" ");
        for (const auto& input : args.inputs) {
            cmd.append(u" ");
            cmd.append(input.toString(PluginType::INPUT));
        }
        _report.debug(u"starting: %s", {cmd});
    }

    // Clear errors on the report, used to check further initialisation errors.
    _report.resetErrors();

    // Get all input plugin options.
    for (size_t i = 0; i < _args.inputs.size(); ++i) {
        auto inputExecutor = std::make_shared<InputExecutor>(_args, i, *this, _report);
        _inputs.push_back(InputData{inputExecutor, {}});
    }

    _success = start(args);
    waitForTermination();
}


//----------------------------------------------------------------------------
// Start the PCR comparator session.
//----------------------------------------------------------------------------

bool ts::PcrComparator::start(const PcrComparatorArgs& args)
{
    // Get all input plugin options.
    for (size_t i = 0; i < _inputs.size(); ++i) {
        if (!_inputs[i].inputExecutor -> plugin()->getOptions()) {
            return false;
        }
    }

    // Create the output file if there is one
    if (_args.outputName.empty()) {
        _output_file = &std::cerr;
    }
    else {
        _output_file = &_output_stream;
        _output_stream.open(_args.outputName.toUTF8().c_str());
        if (!_output_stream) {
            return false;
        }
    }

    // Output header
    csvHeader();

    // Start all input threads
    for (size_t i = 0; i < _inputs.size(); ++i) {
        // Here, start() means start the thread, and start input plugin.
        _success = _inputs[i].inputExecutor->start();
    }

    return _success;
}


//----------------------------------------------------------------------------
// Pass incoming TS packets for analyzing (called by input plugins).
//----------------------------------------------------------------------------
void ts::PcrComparator::analyzePacket(TSPacket*& pkt, TSPacketMetadata*& metadata, size_t count, size_t pluginIndex)
{
    InputData::TimingDataList& timingDataList = _inputs[pluginIndex].timingDataList;
    for (size_t i = 0; i < count; i++)
    {
        GuardMutex lock(_mutex);
        uint64_t pcr = pkt[i].getPCR();
        const bool has_pcr = pcr != INVALID_PCR;
        if (has_pcr) {
            uint64_t timestamp = metadata[i].getInputTimeStamp();
            timingDataList.push_back(InputData::TimingData{pcr, timestamp});
            comparePCR(_inputs);
        }
    }
}


//----------------------------------------------------------------------------
// Generate csv header
//----------------------------------------------------------------------------
void ts::PcrComparator::csvHeader()
{
    *_output_file << "PCR1" << TS_DEFAULT_CSV_SEPARATOR
                    << "PCR2" << TS_DEFAULT_CSV_SEPARATOR
                    << "PCR Delta" << TS_DEFAULT_CSV_SEPARATOR
                    << "Latency (ms)" << TS_DEFAULT_CSV_SEPARATOR
                    << "Sync" << std::endl;
}


//----------------------------------------------------------------------------
// Compare different between two PCRs
//----------------------------------------------------------------------------
void ts::PcrComparator::comparePCR(InputDataVector& inputs)
{
    if (inputs.size() == 2) {
        InputData::TimingDataList& timingDataList1 = inputs[0].timingDataList;
        InputData::TimingDataList& timingDataList2 = inputs[1].timingDataList;

        if (timingDataList1.size() > 0 && timingDataList2.size() > 0) {
            InputData::TimingData timingData1 = timingDataList1.front();
            InputData::TimingData timingData2 = timingDataList2.front();

            // Make sure two PCR data are from the same time interval
            bool pcrDataOutOfSync = verifyPCRDataInputTimestamp(timingData1.timestamp, timingData2.timestamp);

            if (pcrDataOutOfSync) {
                resetPCRDataList();
            } else {
                int64_t pcr1 = timingData1.pcr;
                int64_t pcr2 = timingData2.pcr;
                int64_t pcrDelta = abs(pcr1 - pcr2);
                double latency = (double) pcrDelta/(90000*300)*1000;
                bool reachLatencyThreshold = pcrDelta >= 0 && latency <= _latency_threshold;

                *_output_file << pcr1 << TS_DEFAULT_CSV_SEPARATOR
                            << pcr2 << TS_DEFAULT_CSV_SEPARATOR
                            << pcrDelta << TS_DEFAULT_CSV_SEPARATOR
                            << latency << TS_DEFAULT_CSV_SEPARATOR
                            << std::boolalpha << reachLatencyThreshold 
                            << std::noboolalpha << std::endl;

                timingDataList1.pop_front();
                timingDataList2.pop_front();
            }
        } else if (timingDataList1.size() > 10 || timingDataList2.size() > 10) {
            // Avoid one of the list become too large during input lost
            resetPCRDataList();
        }
    }
}


//----------------------------------------------------------------------------
// Compare the times of two PCR data and check that they were retrieved at the same time interval
//----------------------------------------------------------------------------
bool ts::PcrComparator::verifyPCRDataInputTimestamp(int64_t timestamp1, int64_t timestamp2)
{
    double timestampThreshold = 10; // Threshold of the different between two timestamp (in millisecond)
    double timestampDiffInMs = (double) abs(timestamp1-timestamp2)/(90000*300)*1000;
    return timestampDiffInMs > timestampThreshold;
}


//----------------------------------------------------------------------------
// Reset all PCR data list
//----------------------------------------------------------------------------
void ts::PcrComparator::resetPCRDataList()
{
    for (size_t i = 0; i < _inputs.size(); i++) {
        _inputs[i].timingDataList.clear();
    }
}


//----------------------------------------------------------------------------
// Suspend the calling thread until PCR comparator is completed.
//----------------------------------------------------------------------------
void ts::PcrComparator::waitForTermination()
{
    // Wait for all input termination.
    for (size_t i = 0; i < _inputs.size(); ++i) {
        _inputs[i].inputExecutor->waitForTermination();
    }
}
