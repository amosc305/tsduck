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
#include "tsSystemMonitor.h"
#include "tstspcrdeltaCore.h"

//----------------------------------------------------------------------------
// Constructors and destructor.
//----------------------------------------------------------------------------

ts::PcrComparator::PcrComparator(Report& report) :
    PluginEventHandlerRegistry(),
    _report(report),
    _args(),
    _core(nullptr),
    _success(false)
{
}

ts::PcrComparator::PcrComparator(const PcrComparatorArgs& args, Report& report) :
    PcrComparator(report)
{
    _success = start(args);
    waitForTermination();
}

ts::PcrComparator::~PcrComparator()
{
    // Wait for processing termination to avoid other threads accessing a destroyed object.
    waitForTermination();
}


//----------------------------------------------------------------------------
// Start the PCR comparator session.
//----------------------------------------------------------------------------

bool ts::PcrComparator::start(const PcrComparatorArgs& args)
{
    // Filter already started.
    if (_core != nullptr) {
        _report.error(u"PCR comparator already started");
        return false;
    }

    // Keep command line options for further use.
    _args = args;
    _args.enforceDefaults();

    // Debug message.
    if (_report.debug()) {
        UString cmd(args.appName);
        cmd.append(u" ");
        for (const auto& it : args.inputs) {
            cmd.append(u" ");
            cmd.append(it.toString(PluginType::INPUT));
        }
        cmd.append(u" ");
        cmd.append(args.output.toString(PluginType::OUTPUT));
        _report.debug(u"starting: %s", {cmd});
    }

    // Clear errors on the report, used to check further initialisation errors.
    _report.resetErrors();

    // Create the tspcrdelta core instance.
    _core = new tspcrdelta::Core(_args, *this, _report);
    CheckNonNull(_core);
    _success = !_report.gotErrors();

    // Start the processing.
    _success = _success && _core->start();

    if (!_success) {
        internalCleanup();
    }

    return _success;
}


//----------------------------------------------------------------------------
// Delegations to core object.
//----------------------------------------------------------------------------

void ts::PcrComparator::stop()
{
    if (_core != nullptr) {
        _core->stop(true);
    }
}


//----------------------------------------------------------------------------
// Internal and unconditional cleanup of resources.
//----------------------------------------------------------------------------

void ts::PcrComparator::internalCleanup()
{
    // Then, terminate the core.
    if (_core != nullptr) {
        delete _core;
        _core = nullptr;
    }
}


//----------------------------------------------------------------------------
// Suspend the calling thread until PCR comparator is completed.
//----------------------------------------------------------------------------

void ts::PcrComparator::waitForTermination()
{
    if (_core != nullptr) {
        _core->waitForTermination();
    }
    internalCleanup();
}
