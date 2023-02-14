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
//!
//!  @file
//!  Implementation of the PCR comparator  (command tspcrdelta).
//!
//----------------------------------------------------------------------------

#pragma once
#include "tsPcrComparatorArgs.h"
#include "tsMutex.h"

namespace ts {

    class InputExecutor;

    //!
    //! Implementation of the PCR comparator
    //! This class is used by the @a tspcrdelta utility.
    //! @ingroup plugin
    //!
    class TSDUCKDLL PcrComparator
    {
        TS_NOBUILD_NOCOPY(PcrComparator);
    public:
        //!
        //! Constructor.
        //! The complete input comparing session is performed in this constructor.
        //! The constructor returns only when the PCR comparator session terminates or fails tp start.
        //! @param [in] args Arguments and options.
        //! @param [in,out] report Where to report errors, logs, etc.
        //! This object will be used concurrently by all plugin execution threads.
        //! Consequently, it must be thread-safe. For performance reasons, it should
        //! be asynchronous (see for instance class AsyncReport).
        //!
        PcrComparator(const PcrComparatorArgs& args, Report& report);

        //!
        //! Destructor.
        //! It waits for termination of the session if it is running.
        //!
        ~PcrComparator();

        //!
        //! Get a reference to the report object for the PCR comparator.
        //! @return A reference to the report object for the PCR comparator.
        //!
        Report& report() const { return _report; }

        //!
        //! Start the PCR comparator session.
        //! @param [in] args Arguments and options.
        //! @return True on success, false on failure to start.
        //!
        bool start(const PcrComparatorArgs& args);

        //!
        //! Suspend the calling thread until PCR comparator is completed.
        //!
        void waitForTermination();

        //!
        //! Called by an input plugin when it received input packets.
        //! @param [in] pkt Income TS packet.
        //! @param [in] count TS packet count.
        //! @param [in] pluginIndex Index of the input plugin.
        //!
        void analyzePacket(TSPacket*& pkt, TSPacketMetadata*& metadata, size_t count, size_t pluginIndex);

        //!
        //! Check if the session, when completely run in the constructor, was successful.
        //! @return True on success, false on failure to start.
        //!
        bool success() const { return _success; }

    private:
        typedef std::vector<InputExecutor*> InputExecutorVector;
        typedef std::vector<uint64_t> pcrData;
        typedef std::list<pcrData> pcrDataList;
        typedef std::vector<pcrDataList> pcrDataListVector;

        Report&                    _report;
        PcrComparatorArgs          _args;
        volatile bool              _success;
        InputExecutorVector        _inputs;           // Input plugins threads.
        Mutex                      _mutex;            // Global mutex, protect access to all subsequent fields.
        pcrDataListVector          _pcrs;             // A vector of lists of PCR data, where each list of PCR data is associated with a particular input plugin.
        int64_t                    _pcr_delta_threshold_in_ms; // Limit for difference between two PCRs in millisecond (1 ms = 0.001s).
        std::ofstream              _output_stream;    // Output stream file
        std::ostream*              _output_file;      // Reference to actual output stream file

        // Generate csv header
        void csvHeader();

        // Compare the different between two PCR data list
        void comparePCR(pcrDataListVector& pcrs);

        // Verify PCR data input timestamp
        bool verifyPCRDataInputTimestamp(pcrData& data1, pcrData& data2);

        // Reset all PCR data list
        void resetPCRDataList();
    };
}
