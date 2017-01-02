/*
 * Copyright (c) 2015-2016, Luca Fulchir<luca@fulchir.it>, All rights reserved.
 *
 * This file is part of "libRaptorQ".
 *
 * libRaptorQ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * libRaptorQ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * and a copy of the GNU Lesser General Public License
 * along with libRaptorQ.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "RaptorQ/v1/block_sizes.hpp"
#include "RaptorQ/v1/common.hpp"
#include "RaptorQ/v1/RaptorQ.hpp"
#include "RaptorQ/v1/wrapper/C_common.h"
#if __cplusplus >= 201103L || _MSC_VER > 1900
#include <future>
#include <utility>
#define RQ_EXPLICIT explicit
#else
#define RQ_EXPLICIT
#endif


// There are a lot of NON-C++11 things here.
// there is also a lot of void-casting.
// However,  I could not find a nicer way of hiding the implementation
// and make this impervious to things like linking against things which
// have different stl, or different C++ version.
// This is ugly. I know. no other way?

// For added safety, please use the header-only library


namespace RaptorQ__v1 {
namespace Impl {

class RAPTORQ_API Encoder_void final
{
public:
    Encoder_void (const RaptorQ_type type, const RaptorQ_Block_Size symbols,
                                                    const size_t symbol_size);
    ~Encoder_void();
    Encoder_void() = delete;
    Encoder_void (const Encoder_void&) = delete;
    Encoder_void& operator= (const Encoder_void&) = delete;
    Encoder_void (Encoder_void&&) = default;
    Encoder_void& operator= (Encoder_void&&) = default;
    RQ_EXPLICIT operator bool() const;

    uint16_t symbols() const;
    size_t symbol_size() const;
    uint32_t max_repair() const;

    bool has_data() const;
    size_t set_data (const void* from, const void* to);
    void clear_data();
    void stop();

    bool precompute_sync();
    bool compute_sync();
    #if __cplusplus >= 201103L || _MSC_VER > 1900
    // not even going to try and make this C++98
    std::future<Error> precompute();
    std::future<Error> compute();
    #endif

    size_t encode (void* &output, const void* end, const uint32_t id);

private:
    RaptorQ_type _type;
    void *_encoder;
};

class RAPTORQ_API Decoder_void
{
public:
    Decoder_void (const RaptorQ_type type, const RaptorQ_Block_Size symbols,
                const size_t symbol_size, const RaptorQ_Compute computation);
    ~Decoder_void();
    Decoder_void() = delete;
    Decoder_void (const Decoder_void&) = delete;
    Decoder_void& operator= (const Decoder_void&) = delete;
    Decoder_void (Decoder_void&&) = default;
    Decoder_void& operator= (Decoder_void&&) = default;
    RQ_EXPLICIT operator bool() const;

    uint16_t symbols() const;
    size_t symbol_size() const;

    Error add_symbol (void* &from, const void* to, const uint32_t esi);
    void end_of_input();

    bool can_decode() const;
    void stop();
    uint16_t needed_symbols() const;

    void set_max_concurrency (const uint16_t max_threads);
    RaptorQ_Decoder_Result decode_once();
    struct wait_res
    {
        const Error err;
        const uint16_t symbol;
    };

    wait_res poll();
    wait_res wait_sync();
    #if __cplusplus >= 201103L || _MSC_VER > 1900
    // not even going to try and make this C++98
    std::future<std::pair<Error, uint16_t>> wait();
    #endif

    Error decode_symbol (void* &start, const void* end, const uint16_t esi);
    // returns number of bytes written, offset of data in last iterator
    struct decode_pair {
        const size_t written;
        const size_t offset;
    };
    decode_pair decode_bytes (void* &start, const void* end,
                                    const size_t from_byte, const size_t skip);
private:
    RaptorQ_type _type;
    void *_decoder;
};


} // namespace Impl
} // namespace RaptorQ__v1
