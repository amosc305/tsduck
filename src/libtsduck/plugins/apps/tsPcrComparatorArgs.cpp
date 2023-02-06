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
constexpr size_t ts::PcrComparatorArgs::DEFAULT_MAX_INPUT_PACKETS;
constexpr size_t ts::PcrComparatorArgs::MIN_INPUT_PACKETS;
constexpr size_t ts::PcrComparatorArgs::DEFAULT_BUFFERED_PACKETS;
constexpr size_t ts::PcrComparatorArgs::MIN_BUFFERED_PACKETS;
#endif


//----------------------------------------------------------------------------
// Constructors.
//----------------------------------------------------------------------------

ts::PcrComparatorArgs::PcrComparatorArgs() :
    appName(),
    terminate(false),
    cycleCount(1),
    bufferedPackets(0),
    maxInputPackets(0),
    inputs(),
    separator(),
    csv_format(false),
    log_format(false),
    output_name()
{
}


//----------------------------------------------------------------------------
// Enforce default or minimum values.
//----------------------------------------------------------------------------

void ts::PcrComparatorArgs::enforceDefaults()
{
    if (inputs.empty()) {
        // If no input plugin is used, used only standard input.
        inputs.push_back(PluginOptions(u"file"));
    }

    bufferedPackets = std::max(bufferedPackets, MIN_BUFFERED_PACKETS);
    maxInputPackets = std::max(maxInputPackets, MIN_INPUT_PACKETS);
}


//----------------------------------------------------------------------------
// Define command line options in an Args.
//----------------------------------------------------------------------------

void ts::PcrComparatorArgs::defineArgs(Args& args)
{
    args.option(u"buffer-packets", 'b', Args::POSITIVE);
    args.help(u"buffer-packets",
              u"Specify the size in TS packets of each input plugin buffer. "
              u"The default is " + UString::Decimal(DEFAULT_BUFFERED_PACKETS) + u" packets.");

    args.option(u"cycle", 'c', Args::POSITIVE);
    args.help(u"cycle",
              u"Specify how many times to repeat the cycle through all input plugins in sequence. "
              u"By default, all input plugins are executed in sequence only once (--cycle 1). "
              u"The options --cycle, --infinite and --terminate are mutually exclusive.");

    args.option(u"infinite", 'i');
    args.help(u"infinite", u"Infinitely repeat the cycle through all input plugins in sequence.");

    args.option(u"max-input-packets", 0, Args::POSITIVE);
    args.help(u"max-input-packets",
              u"Specify the maximum number of TS packets to read at a time. "
              u"This value may impact the switch response time. "
              u"The default is " + UString::Decimal(DEFAULT_MAX_INPUT_PACKETS) + u" packets. "
              u"The actual value is never more than half the --buffer-packets value.");

    args.option(u"terminate", 't');
    args.help(u"terminate", u"Terminate execution when the current input plugin terminates.");

    args.option(u"output-file", 'o', Args::FILENAME);
    args.help(u"output-file", u"filename",
              u"Output file name for CSV reporting (standard error by default).");
    
    args.option(u"separator", 's', Args::STRING);
    args.help(u"separator", u"string",
              u"Field separator string in CSV output (default: '" TS_DEFAULT_CSV_SEPARATOR u"').");

    args.option(u"csv", 'c');
    args.help(u"csv",
              u"Report data in CSV (comma-separated values) format. All values are reported "
              u"in decimal. This is the default output format. It is suitable for later "
              u"analysis using tools such as Microsoft Excel.");

    args.option(u"log");
    args.help(u"log",
              u"Report data in \"log\" format through the standard tsp logging system. "
              u"All values are reported in hexadecimal.");
}


//----------------------------------------------------------------------------
// Load arguments from command line.
//----------------------------------------------------------------------------

bool ts::PcrComparatorArgs::loadArgs(DuckContext& duck, Args& args)
{
    appName = args.appName();
    terminate = args.present(u"terminate");
    args.getIntValue(cycleCount, u"cycle", args.present(u"infinite") ? 0 : 1);
    args.getIntValue(bufferedPackets, u"buffer-packets", DEFAULT_BUFFERED_PACKETS);
    maxInputPackets = std::min(args.intValue<size_t>(u"max-input-packets", DEFAULT_MAX_INPUT_PACKETS), bufferedPackets / 2);
    separator = args.value(u"separator", TS_DEFAULT_CSV_SEPARATOR);
    output_name = args.value(u"output-file");
    csv_format = args.present(u"csv");
    log_format = args.present(u"log");

    // Check conflicting modes.
    if (args.present(u"cycle") + args.present(u"infinite") + args.present(u"terminate") > 1) {
        args.error(u"options --cycle, --infinite and --terminate are mutually exclusive");
    }

    // Use CSV format by default.
    if (!csv_format && !log_format) {
        csv_format = true;
    }

    // Load all plugin descriptions. Default output is the standard output file.
    ArgsWithPlugins* pargs = dynamic_cast<ArgsWithPlugins*>(&args);
    if (pargs != nullptr) {
        pargs->getPlugins(inputs, PluginType::INPUT);
    }
    else {
        inputs.clear();
    }
    if (inputs.empty()) {
        // If no input plugin is used, used only standard input.
        inputs.push_back(PluginOptions(u"file"));
    }

    // Check number of input plugins (Must be 2)
    if (inputs.size() != 2) {
        args.error(u"Number of input plugins must be 2");
    }

    return args.valid();
}
