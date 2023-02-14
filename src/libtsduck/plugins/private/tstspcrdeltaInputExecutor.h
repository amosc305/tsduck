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
//!  PCR comparator (tspcrdelta) input plugin executor thread.
//!
//----------------------------------------------------------------------------

#pragma once
#include "tsPluginThread.h"
#include "tsPcrComparator.h"
#include "tsPcrComparatorArgs.h"
#include "tsInputPlugin.h"

namespace ts {
    //!
    //! Execution context of a tspcrdelta input plugin.
    //! @ingroup plugin
    //!
    class InputExecutor : public PluginThread
    {
        TS_NOBUILD_NOCOPY(InputExecutor);

    public:
        //!
        //! Constructor.
        //! @param [in] opt Command line options.
        //! @param [in] index Input plugin index.
        //! @param [in,out] comparator Comparator instance
        //! @param [in,out] log Log report.
        //!
        InputExecutor(const PcrComparatorArgs &opt,
                      size_t index,
                      PcrComparator &comparator,
                      Report &log);

        //!
        //! Virtual destructor.
        //!
        virtual ~InputExecutor() override;

        // Implementation of TSP. We do not use "joint termination" in tspcrdelta.
        virtual void useJointTermination(bool) override;
        virtual void jointTerminate() override;
        virtual bool useJointTermination() const override;
        virtual bool thisJointTerminated() const override;
        virtual size_t pluginCount() const override;
        virtual void signalPluginEvent(uint32_t event_code, Object *plugin_data = nullptr) const override;

        //!
        //! Terminate the input executor thread.
        //!
        void terminateInput();

        // Implementation of TSP.
        virtual size_t pluginIndex() const override;

    private:
        const PcrComparatorArgs& _opt;         //!< Command line options.
        PcrComparator&           _comparator;  // Comparator instance
        InputPlugin*             _input;       // Plugin API.
        const size_t             _pluginIndex; // Index of this input plugin.
        TSPacketVector           _buffer;      // Packet buffer.
        TSPacketMetadataVector   _metadata;    // Packet metadata.

        static constexpr size_t      MAX_INPUT_PACKETS = 128;  // Maximum input packets to read at a time.
        static constexpr size_t      BUFFERED_PACKETS = 512;   // Input size buffer in packets.

        // Implementation of Thread.
        virtual void main() override;
    };
}
