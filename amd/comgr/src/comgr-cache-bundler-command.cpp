/*******************************************************************************
 *
 * University of Illinois/NCSA
 * Open Source License
 *
 * Copyright (c) 2003-2017 University of Illinois at Urbana-Champaign.
 * Modifications (c) 2018 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimers.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimers in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the names of the LLVM Team, University of Illinois at
 *       Urbana-Champaign, nor the names of its contributors may be used to
 *       endorse or promote products derived from this Software without specific
 *       prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
 * THE SOFTWARE.
 *
 ******************************************************************************/

#include "llvm/Support/MemoryBuffer.h"
#include <comgr-cache-bundler-command.h>

#include <clang/Driver/OffloadBundler.h>
#include <llvm/BinaryFormat/Magic.h>

namespace COMGR {
using namespace llvm;
using namespace clang;

namespace {
}

bool UnbundleCommand::canCache() const {
    // Check the input file magic. Handle only compressed bundles
    if(Config.InputFileNames.size() != 1)
        return false;

    StringRef InputFilename = Config.InputFileNames.front();
    file_magic Magic;
    if(identify_magic(InputFilename, Magic))
        return false;

    // it's not worth to cache other types of bundles
    return Magic == file_magic::offload_bundle_compressed;
}

Error UnbundleCommand::writeExecuteOutput(StringRef CachedBuffer) {
}

Expected<StringRef> UnbundleCommand::readExecuteOutput() {
}

amd_comgr_status_t UnbundleCommand::execute(raw_ostream &LogS) {
}

CachedCommandAdaptor::ActionClass UnbundleCommand::getClass() const {
  return clang::driver::Action::OffloadUnbundlingJobClass;
}

void UnbundleCommand::addOptionsIdentifier(HashAlgorithm &H) const {
    H.update(Config.TargetNames.size());
    for(StringRef Target : Config.TargetNames) {
        CachedCommandAdaptor::addString(H, Target);
    }
}

Error UnbundleCommand::addInputIdentifier(HashAlgorithm &H) const {
    StringRef InputFilename = Config.InputFileNames.front();

    ErrorOr<std::unique_ptr<MemoryBuffer>> MaybeInputBuffer = MemoryBuffer::getFileSlice(InputFilename, sizeof(CompressedOffloadBundle::Header), 0);
    if(!MaybeInputBuffer) {
        std::error_code EC = MaybeInputBuffer.getError();
        return createStringError(EC, Twine("Failed to open ") + InputFilename +
                                        " : " + EC.message() + "\n");
    }

    auto MaybeHeader = CompressedOffloadBundle::readHeader(MaybeInputBuffer->get()->getBuffer());
    if(!MaybeHeader) 
        return MaybeHeader.takeError();

    // only hash the input file, not the whole header. Colissions are unlikely since the header includes a hash (weak) of the contents
    H.update(ArrayRef<uint8_t>(reinterpret_cast<uint8_t*>(&MaybeHeader), sizeof(MaybeHeader))); 
    return Error::success();
}

}
