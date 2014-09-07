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
#include "pmt_helper.h"
#include <Poco/Format.h>
#include <cmath>
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

    for (size_t i = 0; i < Pothos::Block::inputs().size(); i++)
    {
        this->input(i)->setReserve(d_history+1);
    }

    reinterpret_cast<gr::block_executor *>(_executor.get())->run_one_iteration();
}

/***********************************************************************
 * propagateLabels
 **********************************************************************/
void gr::block::propagateLabels(const Pothos::InputPort *inputPort)
{
    switch (tag_propagation_policy())
    {
    case block::TPP_DONT: return;
    case block::TPP_ONE_TO_ONE:
    {
        if (inputPort->index() == -1) return;
        const auto portIndex = size_t(inputPort->index());
        if (portIndex >= Pothos::Block::outputs().size()) return;
        auto outputPort = Pothos::Block::output(portIndex);
        for (const auto &label : inputPort->labels())
        {
            auto newLabel = label;
            newLabel.index += d_attr_delay;
            newLabel.index = std::llround(newLabel.index*this->relative_rate());
            outputPort->postLabel(label);
        }
    }
    case block::TPP_ALL_TO_ALL:
    {
        for (const auto &label : inputPort->labels())
        {
            auto newLabel = label;
            newLabel.index += d_attr_delay;
            newLabel.index = std::llround(newLabel.index*this->relative_rate());
            for (auto outputPort : Pothos::Block::outputs()) outputPort->postLabel(label);
        }
    }
    default: return;
    }
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
 * customized hooks - produce/consume
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

/***********************************************************************
 * customized hooks - tags
 **********************************************************************/
namespace gr
{
  void
  block::add_item_tag(unsigned int which_output,
                         const tag_t &tag)
  {
    auto outputPort = Pothos::Block::output(which_output);
    Pothos::Label label;
    label.id = pmt::symbol_to_string(tag.key);
    label.data = pmt_to_obj(tag.value);
    assert(tag.offset >= outputPort->totalElements());
    label.index = tag.offset - outputPort->totalElements();
    outputPort->postLabel(label);
  }

  void
  block::remove_item_tag(unsigned int which_input,
                         const tag_t &tag)
  {
    auto inputPort = Pothos::Block::input(which_input);
    for (const auto &label : inputPort->labels())
    {
        if (label.index + inputPort->totalElements() != tag.offset - d_attr_delay) continue;
        if (label.id != pmt::symbol_to_string(tag.key)) continue;
        return inputPort->removeLabel(label);
    }
  }

  void
  block::get_tags_in_range(std::vector<tag_t> &v,
                           unsigned int which_input,
                           uint64_t start, uint64_t end)
  {
    return get_tags_in_range(v, which_input, start, end, pmt::pmt_t());
  }

  void
  block::get_tags_in_range(std::vector<tag_t> &v,
                           unsigned int which_input,
                           uint64_t start, uint64_t end,
                           const pmt::pmt_t &key)
  {
    v.clear();
    auto inputPort = Pothos::Block::input(which_input);
    for (const auto &label : inputPort->labels())
    {
        auto offset = label.index + inputPort->totalElements() + d_attr_delay;
        if (offset < start or offset >= end) continue;
        tag_t tag;
        tag.key = pmt::string_to_symbol(label.id);
        if (pmt::is_symbol(key) and key != tag.key) continue;
        tag.value = obj_to_pmt(label.data);
        tag.offset = offset;
        v.push_back(tag);
    }
  }
}
