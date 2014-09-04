/*
 * Copyright 2014 Free Software Foundation, Inc.
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

#include <Pothos/Framework.hpp>
#include <gnuradio/block.h>
#include "block_executor.h"
#include <Poco/Format.h>
#include <cassert>
#include <iostream>

/***********************************************************************
 * init the name and ports -- called by the block constructor
 **********************************************************************/
void gr::block::initialize(void)
{
    Pothos::Block::setName(d_name);
    for (size_t i = 0; i < d_input_signature->sizeof_stream_items().size(); i++)
    {
        if (d_input_signature->max_streams() != io_signature::IO_INFINITE and int(i) >= d_input_signature->max_streams()) break;
        auto bytes = d_input_signature->sizeof_stream_items()[i];
        Pothos::Block::setupInput(i, Pothos::DType(Poco::format("GrIoSig%d", bytes), bytes));
    }
    for (size_t i = 0; i < d_output_signature->sizeof_stream_items().size(); i++)
    {
        if (d_output_signature->max_streams() != io_signature::IO_INFINITE and int(i) >= d_output_signature->max_streams()) break;
        auto bytes = d_output_signature->sizeof_stream_items()[i];
        Pothos::Block::setupOutput(i, Pothos::DType(Poco::format("GrIoSig%d", bytes), bytes));
    }
    Pothos::Block::registerCall(this, POTHOS_FCN_TUPLE(gr::block, __setNumInputs));
    Pothos::Block::registerCall(this, POTHOS_FCN_TUPLE(gr::block, __setNumOutputs));
}

void gr::block::__setNumInputs(const size_t num)
{
    for (size_t i = Pothos::Block::inputs().size(); i < num; i++)
    {
        assert(not Pothos::Block::inputs().empty());
        Pothos::Block::setupInput(i, Pothos::Block::inputs().back()->dtype());
    }
}

void gr::block::__setNumOutputs(const size_t num)
{
    for (size_t i = Pothos::Block::outputs().size(); i < num; i++)
    {
        assert(not Pothos::Block::outputs().empty());
        Pothos::Block::setupInput(i, Pothos::Block::outputs().back()->dtype());
    }
}

/***********************************************************************
 * activation/deactivate notification events
 **********************************************************************/
void gr::block::activate(void)
{
    auto block = gr::cast_to_block_sptr(this->shared_from_this());
    _executor.reset(new gr::block_executor(block));
}

void gr::block::deactivate(void)
{
    _executor.reset();
}

/***********************************************************************
 * stimulus even occured - handle inputs and call general work
 **********************************************************************/
void gr::block::work(void)
{
    const auto &workInfo = Pothos::Block::workInfo();

    if (workInfo.minElements == 0) return;

    //grab all of the messages

    //grab all of the labels

    for (size_t i = 0; i < Pothos::Block::inputs().size(); i++)
    {
        this->input(i)->setReserve(d_history+1);
    }

    reinterpret_cast<gr::block_executor *>(_executor.get())->run_one_iteration();
}

/***********************************************************************
 * TODO
 **********************************************************************/
void gr::block::propagateLabels(const Pothos::InputPort *input)
{
    
}

/***********************************************************************
 * custom buffer managers - circular buffers for blocks with history
 **********************************************************************/
std::shared_ptr<Pothos::BufferManager> gr::block::getInputBufferManager(const std::string &name, const std::string &domain)
{
    //install circular buffer when history is enabled
    if (d_history > 1)
    {
        Pothos::BufferManagerArgs args;
        const auto require = (d_history+1)*Pothos::Block::input(name)->dtype().size();
        args.bufferSize = std::max(args.bufferSize, require*8/*factor*/);
        return Pothos::BufferManager::make("circular", args);
    }
    return Pothos::Block::getInputBufferManager(name, domain);
}

std::shared_ptr<Pothos::BufferManager> gr::block::getOutputBufferManager(const std::string &name, const std::string &domain)
{
    return Pothos::Block::getOutputBufferManager(name, domain);
}

/***********************************************************************
 * customized hooks into the Pothos framework
 **********************************************************************/
namespace gr
{
  void
  block::consume(int which_input, int how_many_items)
  {
    if (how_many_items < 0) return;
    Pothos::Block::input(which_input)->consume(how_many_items);
  }

  void
  block::consume_each(int how_many_items)
  {
    if (how_many_items < 0) return;
    for (auto input : Pothos::Block::inputs()) input->consume(how_many_items);
  }

  void
  block::produce(int which_output, int how_many_items)
  {
    if (how_many_items < 0) return;
    Pothos::Block::output(which_output)->produce(how_many_items);
  }
}
