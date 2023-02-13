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

#include "tsPcrComparatorArgs.h"
#include "tsArgsWithPlugins.h"

#if defined(TS_NEED_STATIC_CONST_DEFINITIONS)
constexpr int ts::PcrComparatorArgs::DESIGNATED_INPUT_PLUGIN_NUMBER;
#endif


//----------------------------------------------------------------------------
// Constructors.
//----------------------------------------------------------------------------

ts::PcrComparatorArgs::PcrComparatorArgs() :
    appName(),
    inputs(),
    output_name()
{
}


//----------------------------------------------------------------------------
// Define command line options in an Args.
//----------------------------------------------------------------------------

void ts::PcrComparatorArgs::defineArgs(Args& args)
{
    args.option(u"output-file", 'o', Args::FILENAME);
    args.help(u"output-file", u"filename",
              u"Output file name for CSV reporting (standard error by default).");
}


//----------------------------------------------------------------------------
// Load arguments from command line.
//----------------------------------------------------------------------------

bool ts::PcrComparatorArgs::loadArgs(Args& args)
{
    appName = args.appName();
    output_name = args.value(u"output-file");

    // Load all plugin descriptions. Default output is the standard output file.
    ArgsWithPlugins* pargs = dynamic_cast<ArgsWithPlugins*>(&args);
    if (pargs != nullptr) {
        pargs->getPlugins(inputs, PluginType::INPUT);
    }

    // Check number of input plugins
    if (inputs.size() != DESIGNATED_INPUT_PLUGIN_NUMBER) {
        args.error(u"Number of input plugins must be %d", {DESIGNATED_INPUT_PLUGIN_NUMBER});
    }

    return args.valid();
}
