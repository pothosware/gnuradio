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
#include <Poco/Format.h>
#include <iostream>

/***********************************************************************
 * init the name and ports -- called by the block constructor
 **********************************************************************/
void gr::block::initialize(void)
{
    Pothos::Block::setName(d_name);
    for (size_t i = 0; i < d_input_signature->sizeof_stream_items().size(); i++)
    {
        auto bytes = d_input_signature->sizeof_stream_items()[i];
        Pothos::Block::setupInput(i, Pothos::DType(Poco::format("GrIoSig%d", bytes), bytes));
    }
    for (size_t i = 0; i < d_output_signature->sizeof_stream_items().size(); i++)
    {
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
        Pothos::Block::setupInput(i, Pothos::Block::inputs().back()->dtype());
    }
}

void gr::block::__setNumOutputs(const size_t num)
{
    for (size_t i = Pothos::Block::outputs().size(); i < num; i++)
    {
        Pothos::Block::setupInput(i, Pothos::Block::outputs().back()->dtype());
    }
}

/***********************************************************************
 * activation/deactivate notification events
 **********************************************************************/
void gr::block::activate(void)
{
    this->start();
}

void gr::block::deactivate(void)
{
    this->stop();
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

    //create a count of the number of input items per indexed ports
    std::vector<int> ninput_items(this->inputs().size());
    for (size_t i = 0; i < ninput_items.size(); i++)
    {
        this->input(i)->setReserve(d_history+1);
        ninput_items[i] = this->input(i)->elements();
    }

    //perform the forecast input loop
    int noutput_items = workInfo.minOutElements;
    std::vector<int> forecast_items(this->inputs().size());
    while (noutput_items > 0)
    {
        this->forecast(noutput_items, forecast_items);
        for (size_t i = 0; i < ninput_items.size(); i++)
        {
            if (forecast_items[i] > ninput_items[i])
            {
                noutput_items -= 1;
                goto forecast_again;
            }
        }
        break;
        forecast_again: continue;
    }

    //call into general work with the available buffers
    int ret = this->general_work(noutput_items, ninput_items,
        const_cast<gr_vector_const_void_star &>(workInfo.inputPointers),
        const_cast<gr_vector_void_star &>(workInfo.outputPointers));

    //when ret is positive it means produce this much on all indexed output ports
    if (ret > 0) for (auto output : Pothos::Block::outputs()) output->produce(size_t(ret));
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
