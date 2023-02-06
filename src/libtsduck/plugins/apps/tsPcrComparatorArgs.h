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
//!  Transport stream PCR comparator command-line options
//!
//----------------------------------------------------------------------------

#pragma once
#include "tsPluginOptions.h"
#include "tsIPv4SocketAddress.h"

namespace ts {

    class Args;
    class DuckContext;

    //!
    //! Transport stream PCR comparator command-line options
    //! @ingroup plugin
    //!
    class TSDUCKDLL PcrComparatorArgs
    {
    public:
        UString             appName;           //!< Application name, for help messages.
        size_t              bufferedPackets;   //!< Input buffer size in packets.
        size_t              maxInputPackets;   //!< Maximum input packets to read at a time.
        PluginOptionsVector inputs;            //!< Input plugins descriptions.
        UString             separator;         //!< Field separator
        bool                csv_format;        //!< Output in CSV format.
        bool                log_format;        //!< Output in log format.
        UString             output_name;       //!< Output file name (empty means stderr).

        static constexpr size_t      DEFAULT_MAX_INPUT_PACKETS = 128;  //!< Default maximum input packets to read at a time.
        static constexpr size_t      MIN_INPUT_PACKETS = 1;            //!< Minimum input packets to read at a time.
        static constexpr size_t      DEFAULT_BUFFERED_PACKETS = 512;   //!< Default input size buffer in packets.
        static constexpr size_t      MIN_BUFFERED_PACKETS = 16;        //!< Minimum input size buffer in packets.
        static constexpr int         DESIGNATED_INPUT_PLUGIN_NUMBER = 2; //!< Designated input plugin allowed.

        //!
        //! Constructor.
        //!
        PcrComparatorArgs();

        //!
        //! Enforce default or minimum values.
        //!
        void enforceDefaults();

        //!
        //! Add command line option definitions in an Args.
        //! @param [in,out] args Command line arguments to update.
        //!
        void defineArgs(Args& args);

        //!
        //! Load arguments from command line.
        //! Args error indicator is set in case of incorrect arguments.
        //! @param [in,out] duck TSDuck execution context.
        //! @param [in,out] args Command line arguments.
        //! @return True on success, false on error in argument line.
        //!
        bool loadArgs(DuckContext& duck, Args& args);
    };
}
