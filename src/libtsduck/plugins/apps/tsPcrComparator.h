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
#include "tsPluginEventHandlerRegistry.h"
#include "tsPcrComparatorArgs.h"

namespace ts {

    // Used in private part.
    namespace tspcrdelta {
        class Core;
    }

    //!
    //! Implementation of the input plugin switcher.
    //! This class is used by the @a tspcrdelta utility.
    //! @ingroup plugin
    //!
    class TSDUCKDLL PcrComparator: public PluginEventHandlerRegistry
    {
        TS_NOBUILD_NOCOPY(PcrComparator);
    public:
        //!
        //! Constructor.
        //! This constructor does not start the session.
        //! @param [in,out] report Where to report errors, logs, etc.
        //! This object will be used concurrently by all plugin execution threads.
        //! Consequently, it must be thread-safe. For performance reasons, it should
        //! be asynchronous (see for instance class AsyncReport).
        //!
        PcrComparator(Report& report);

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
        //! Check if the PCR comparator is started.
        //! @return True if the PCR comparator is in progress, false otherwise.
        //!
        bool isStarted() const { return _core != nullptr; }

        //!
        //! Stop the PCR comparator.
        //!
        void stop();

        //!
        //! Suspend the calling thread until PCR comparator is completed.
        //!
        void waitForTermination();

        //!
        //! Full session constructor.
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
        //! Check if the session, when completely run in the constructor, was successful.
        //! @return True on success, false on failure to start.
        //!
        bool success() const { return _success; }

    private:
        Report&                    _report;
        PcrComparatorArgs          _args;
        tspcrdelta::Core*            _core;
        volatile bool              _success;

        // Internal and unconditional cleanup of resources.
        void internalCleanup();
    };
}
