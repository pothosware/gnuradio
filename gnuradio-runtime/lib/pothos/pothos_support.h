/*
 * Copyright 2014-2015 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#pragma once
#include <Pothos/Object.hpp>
#include <Pothos/Framework.hpp>
#include <gnuradio/basic_block.h>
#include <gnuradio/block.h>
#include <pmt/pmt.h>

/*!
 * The extended_io_signature is cast-able to gr::io_signature,
 * and contains extra pointer information for pothos support.
 */
struct extended_io_signature
{
    extended_io_signature(gr::io_signature::sptr sig):
        d_min_streams(sig->min_streams()),
        d_max_streams(sig->max_streams()),
        d_sizeof_stream_item(sig->sizeof_stream_items()){}

    //io_signature ABI
    int			d_min_streams;
    int			d_max_streams;
    std::vector<int>	d_sizeof_stream_item;

    //extended info
    gr::basic_block *basic_block;
    Pothos::Block *pothos_block;
};

/*!
 * Extract the Pothos block given a pointer to the basic block.
 */
static inline Pothos::Block *extractPothosBlock(gr::basic_block *b)
{
    //when constructed through GrPothosBlock, the input_signature contains extra block pointers
    //we can verify this by checking the basic block pointer to determine pothos enabled block
    auto ptrs = reinterpret_cast<const extended_io_signature *>(b->input_signature().get());
    if (ptrs->basic_block != b) return nullptr;
    return ptrs->pothos_block;
}

/*!
 * Conversions between Object and pmt_t types.
 * This is a simple type-based conversion.
 * In the future, we can make this more efficient
 * by directly typedefing pmt_t as Object
 * and reimplementing pmt calls on top of STL.
 */

pmt::pmt_t obj_to_pmt(const Pothos::Object &obj);

Pothos::Object pmt_to_obj(const pmt::pmt_t &pmt);
