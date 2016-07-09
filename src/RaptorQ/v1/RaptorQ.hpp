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

#include "RaptorQ/v1/common.hpp"
#include "RaptorQ/v1/API_Iterators.hpp"
#include "RaptorQ/v1/Encoder.hpp"
#include "RaptorQ/v1/Decoder.hpp"
#include <future>


namespace RaptorQ__v1 {
namespace Impl {

// nope. Decoder needs new iterator!
template <typename Rnd_It, typename Fwd_It>
using Symbol_Iterator = RFC6330__v1::Symbol_Iterator<Rnd_It, Fwd_It>;

template <typename Rnd_It, typename Fwd_It>
class RAPTORQ_LOCAL Encoder
{
public:
	~Encoder();
    // used for precomputation
    Encoder (const uint16_t symbols, const uint16_t symbol_size);
    // with data at the beginning. Less work.
    Encoder (const Rnd_It data_from, const Rnd_It data_to,
                                            const uint16_t symbol_size);
    Symbol_Iterator<Rnd_It, Fwd_It> begin ();
    Symbol_Iterator<Rnd_It, Fwd_It> end ();


	uint64_t add_data (Rnd_It from, const Rnd_It to);
	bool compute_sync();

    std::future<Error> compute();
    uint64_t encode (Fwd_It &output, const Fwd_It end, const uint32_t &id);

private:
	enum class Data_State : uint8_t {
		NEED_DATA = 1,	// first constructor used. no interleaver until FULL
		FULL = 2,
		INIT = 3	// second constructor used: we already have the interleaver
	};

    std::unique_ptr<RFC6330__v1::Impl::Interleaver<Rnd_It>> interleaver;
	Raw_Encoder<Rnd_It, Fwd_It> encoder;
	DenseMtx precomputed;
	std::vector<typename std::iterator_traits<Rnd_It>::value_type> data;
	const uint16_t _symbol_size, _symbols;
	const RaptorQ__v1::Work_State work = RaptorQ__v1::Work_State::KEEP_WORKING;
    Data_State state;
};

template <typename In_It, typename Fwd_It>
class RAPTORQ_LOCAL Decoder
{
public:

	enum class RAPTORQ_LOCAL Report : uint8_t {
		PARTIAL_FROM_BEGINNING = 1,
		PARTIAL_ANY = 2,
		COMPLETE = 3
	};

    Decoder (const uint64_t bytes, const uint16_t symbol_size,
															const Report type);


	Error add_symbol (In_It from, const In_It to, const uint32_t esi);
	using Decoder_Result = typename Raw_Decoder<In_It>::Decoder_Result;

	bool can_decode() const;
	Decoder_Result decode();
	void stop();


	std::future<std::pair<Error, uint16_t>> ready();

	uint64_t decode_symbol (Fwd_It &start, const Fwd_It end,const uint16_t esi,
															const size_t skip);
	uint64_t decode_bytes (Fwd_It &start, const Fwd_It end,
								const uint64_t from_byte, const size_t skip);
	std::pair<size_t, size_t> decode_aligned (Fwd_It &start, const Fwd_It end,
									const size_t from_byte, const size_t skip);
private:
	Raw_Decoder<In_It> dec;
	const Report _type;
	RaptorQ__v1::Work_State work = RaptorQ__v1::Work_State::KEEP_WORKING;
};


///////////////////
//// Encoder
///////////////////

template <typename Rnd_It, typename Fwd_It>
Encoder<Rnd_It, Fwd_It>::~Encoder()
{
	encoder->stop();
}

template <typename Rnd_It, typename Fwd_It>
Encoder<Rnd_It, Fwd_It>::Encoder (const uint16_t symbols,
													const uint16_t symbol_size)
	: interleaver (nullptr), encoder (symbols),
										_symbol_size (symbol_size),
										_symbols (symbols)
{
	IS_RANDOM(Rnd_It, "RaptorQ__v1::Encoder");
	IS_FORWARD(Fwd_It, "RaptorQ__v1::Encoder");
    state = Data_State::INIT;
}

template <typename Rnd_It, typename Fwd_It>
Encoder<Rnd_It, Fwd_It>::Encoder (const Rnd_It data_from, const Rnd_It data_to,
                                            const uint16_t symbol_size)
    : interleaver (new RFC6330__v1::Impl::Interleaver<Rnd_It> (data_from,
													data_to, _symbol_size,
														SIZE_MAX, symbol_size)),
	  encoder (interleaver.get(), 0), _symbol_size (0),	// unused
									_symbols (0)		// unused
{
	IS_RANDOM(Rnd_It, "RaptorQ__v1::Encoder");
	IS_FORWARD(Fwd_It, "RaptorQ__v1::Encoder");
    state = Data_State::NEED_DATA;
}

template <typename Rnd_It, typename Fwd_It>
uint64_t Encoder<Rnd_It, Fwd_It>::add_data (Rnd_It from, const Rnd_It to)
{
	uint64_t written = 0;
	using T = typename std::iterator_traits<Rnd_It>::value_type;

	if (state != Data_State::NEED_DATA)
		return written;
	while (from != to) {
		if ((data.size() * sizeof (T) >= _symbols * _symbol_size)) {
			state = Data_State::FULL;
			break;
		}
		data.push_back (from);
		++from;
		++written;
	}
	return written;
}

template <typename Rnd_It, typename Fwd_It>
bool Encoder<Rnd_It, Fwd_It>::compute_sync()
{
	if (state == Data_State::INIT) {
		return encoder.generate_symbols (&work);
	} else {
		precomputed = encoder.get_precomputed (&work);
		return precomputed.rows() != 0;
	}
	return false;
}

template <typename Rnd_It, typename Fwd_It>
uint64_t Encoder<Rnd_It, Fwd_It>::encode (Fwd_It &output, const Fwd_It end,
															const uint32_t &id)
{
	switch (state) {
	case Data_State::INIT:
		if (!encoder->ready())
			return 0;
		return encoder.Enc (id, output, end);
	case Data_State::NEED_DATA:
		return 0;
	case Data_State::FULL:
		if (!encoder->ready()) {
			if (precomputed.rows() == 0) {
				return 0;
			} else {
				interleaver = std::unique_ptr<
							RFC6330__v1::Impl::Interleaver<Rnd_It>> (
									new RFC6330__v1::Impl::Interleaver<Rnd_It> (
													data.begin(), data.end(),
													_symbol_size, SIZE_MAX,
																_symbol_size));
				encoder.generate_symbols (precomputed, interleaver);
				precomputed = DenseMtx();	// free mem
			}
		}
		return encoder.Enc (id, output, end);
	}
}


///////////////////
//// Decoder
///////////////////

template <typename In_It, typename Fwd_It>
Decoder<In_It, Fwd_It>::Decoder (const uint64_t bytes,
													const uint16_t symbol_size,
															const Report type)
	:_type (type)
{
	uint16_t symbols = static_cast<uint16_t> (bytes / symbol_size);
	if (bytes % symbol_size != 0)
		++symbols;
	dec = Raw_Decoder<In_It> (symbols, symbol_size);
}

template <typename In_It, typename Fwd_It>
Error Decoder<In_It, Fwd_It>::add_symbol (In_It from, const In_It to,
															const uint32_t esi)
{
	return dec.add_symbol (from, to, esi);
}

template <typename In_It, typename Fwd_It>
bool Decoder<In_It, Fwd_It>::can_decode() const
{
	return dec.can_decode();
}

template <typename In_It, typename Fwd_It>
typename Decoder<In_It, Fwd_It>::Decoder_Result Decoder<In_It, Fwd_It>::decode()
{
	return dec.decode (&work);
}

template <typename In_It, typename Fwd_It>
void Decoder<In_It, Fwd_It>::stop()
{
	dec.stop();
}

template <typename In_It, typename Fwd_It>
uint64_t Decoder<In_It, Fwd_It>::decode_bytes(Fwd_It &start, const Fwd_It end,
								const uint64_t from_byte, const size_t skip)
{
	if (!dec.ready())
		return 0;
	auto decoded = dec->get_symbols();
	uint16_t esi = from_byte / decoded.cols();
	uint16_t byte = from_byte % decoded.cols();

	using T = typename std::iterator_traits<Fwd_It>::value_type;
	size_t offset_al = skip;
	T element = *start;
	uint64_t written = 0;
	while (start != end && esi < decoded.rows()) {
		element += static_cast<T> (static_cast<uint8_t>((*decoded)(esi, byte)))
															<< offset_al * 8;
		++offset_al;
		if (offset_al == sizeof(T)) {
			*start = element;
			written += offset_al;
			offset_al = 0;
			element = static_cast<T> (0);
		}
		++byte;
		if (byte == decoded->cols()) {
			byte = 0;
			++esi;
		}
	}
	if (start != end && offset_al != 0) {
		// we have more stuff in "element", but not enough to fill
		// the iterator.
		*start = element;
		written += offset_al;
	}
	return written;
}

template <typename In_It, typename Fwd_It>
std::pair<size_t, size_t> Decoder<In_It, Fwd_It>::decode_aligned (Fwd_It &start,
													const Fwd_It end,
													const uint64_t from_byte,
													const size_t skip)
{
	using T = typename std::iterator_traits<Fwd_It>::value_type;
	uint64_t written = decode_bytes (start, end, from_byte, skip);
	size_t it_written = (written + skip) / sizeof(T);
	size_t last_iterator = (written + skip) % sizeof(T);
	return {it_written, last_iterator};
}

template <typename In_It, typename Fwd_It>
uint64_t Decoder<In_It, Fwd_It>::decode_symbol (Fwd_It &start, const Fwd_It end,
															const uint16_t esi,
															const size_t skip)
{
	if (!dec.ready())
		return 0;
	size_t esi_byte = esi * dec->cols();
	return decode_bytes (start, end, esi_byte, skip);
}

}   // namespace Impl
}   // namespace RaptorQ__v1
