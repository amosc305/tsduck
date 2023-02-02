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
#include "tstspcrdeltaPluginExecutor.h"
#include "tsPcrComparatorArgs.h"
#include "tsInputPlugin.h"
#include "tsMutex.h"
#include "tsCondition.h"
#include "tsMonotonic.h"

namespace ts {
    namespace tspcrdelta {
        //!
        //! Execution context of a tspcrdelta input plugin.
        //! @ingroup plugin
        //!
        class InputExecutor : public PluginExecutor
        {
            TS_NOBUILD_NOCOPY(InputExecutor);
        public:
            //!
            //! Constructor.
            //! @param [in] opt Command line options.
            //! @param [in] handlers Registry of event handlers.
            //! @param [in] index Input plugin index.
            //! @param [in,out] core Command core instance.
            //! @param [in,out] log Log report.
            //!
            InputExecutor(const PcrComparatorArgs& opt,
                          const PluginEventHandlerRegistry& handlers,
                          size_t index,
                          Core& core,
                          Report& log);

            //!
            //! Virtual destructor.
            //!
            virtual ~InputExecutor() override;

            // Implementation of TSP.
            virtual size_t pluginIndex() const override;

        private:
            InputPlugin*             _input;         // Plugin API.
            const size_t             _pluginIndex;   // Index of this input plugin.
            TSPacketVector           _buffer;        // Packet buffer.
            TSPacketMetadataVector   _metadata;      // Packet metadata.
            Mutex                    _mutex;         // Mutex to protect all subsequent fields.
            Condition                _todo;          // Condition to signal something to do.
            size_t                   _outFirst;      // Index of first packet to output in _buffer.
            size_t                   _outCount;      // Number of packets to output, not always contiguous, may wrap up.
            Monotonic                _start_time;    // Creation time in a monotonic clock.

            // Implementation of Thread.
            virtual void main() override;
        };

        //!
        //! Vector of pointers to InputExecutor.
        //!
        typedef std::vector<InputExecutor*> InputExecutorVector;
    }
}
