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

/*!
 * Allow access to has_msg_handler and dispatch_msg
 * without header modifications or friend accessors.
 */
#define protected public
#include <gnuradio/basic_block.h>
#undef protected

#include "pothos_block_executor.h"
#include "pothos_support.h"
#include <cmath>
#include <cassert>
#include <cctype>
#include <iostream>

/***********************************************************************
 * try our best to infer the data type given the info at hand
 **********************************************************************/
static Pothos::DType inferDType(const size_t ioSize, const std::string &name, const bool isInput)
{
    Pothos::DType dtype("custom", ioSize); //default if we cant figure it out
    const auto lastUnder = name.find_last_of("_");
    if (lastUnder == std::string::npos) return dtype;

    //grab data type suffix
    auto suffix = name.substr(lastUnder+1);
    if (suffix.empty()) return dtype;

    //strip leading v (for vector)
    if (std::tolower(suffix.front()) == 'v') suffix = suffix.substr(1);
    if (suffix.empty()) return dtype;

    //extract signature character
    char sig = suffix.front();
    if (not isInput and suffix.size() >= 2) sig = suffix.at(1);
    sig = std::tolower(sig);

    //inspect signature and io type for size-multiple
    #define inspectSig(sigName, sigType) \
        if (sig == sigName and ioSize%sizeof(sigType) == 0) \
            return Pothos::DType(typeid(sigType), ioSize/sizeof(sigType))
    inspectSig('b', char);
    inspectSig('s', short);
    inspectSig('i', int);
    inspectSig('f', float);
    inspectSig('c', gr_complex);
    return dtype;
}

/***********************************************************************
 * GrPothosBlock interfaces a gr::basic_block to the Pothos framework 
 **********************************************************************/
class GrPothosBlock : public Pothos::Block
{
public:
    Pothos::Block *make(gr::block *block)
    {
        return new GrPothosBlock(block);
    }

    GrPothosBlock(gr::block *block);
    ~GrPothosBlock(void);
    void __setNumInputs(size_t);
    void __setNumOutputs(size_t);
    void activate(void);
    void deactivate(void);
    void work(void);
    void propagateLabels(const Pothos::InputPort *input);
    Pothos::BufferManager::Sptr getInputBufferManager(const std::string &name, const std::string &domain);
    Pothos::BufferManager::Sptr getOutputBufferManager(const std::string &name, const std::string &domain);

private:
    gr::block *d_block;
    gr::pothos_block_executor *d_exec;
};

/***********************************************************************
 * init the name and ports -- called by the block constructor
 **********************************************************************/
GrPothosBlock::GrPothosBlock(gr::block *block):
    d_block(block)
{
    Pothos::Block::setName(d_block->name());

    auto d_input_signature = d_block->input_signature();
    for (size_t i = 0; i < d_input_signature->sizeof_stream_items().size(); i++)
    {
        if (d_input_signature->max_streams() != gr::io_signature::IO_INFINITE and int(i) >= d_input_signature->max_streams()) break;
        auto bytes = d_input_signature->sizeof_stream_items()[i];
        Pothos::Block::setupInput(i, inferDType(bytes, d_block->name(), true));
    }

    auto d_output_signature = d_block->output_signature();
    for (size_t i = 0; i < d_output_signature->sizeof_stream_items().size(); i++)
    {
        if (d_output_signature->max_streams() != gr::io_signature::IO_INFINITE and int(i) >= d_output_signature->max_streams()) break;
        auto bytes = d_output_signature->sizeof_stream_items()[i];
        Pothos::Block::setupOutput(i, inferDType(bytes, d_block->name(), false));
    }

    Pothos::Block::registerCall(this, POTHOS_FCN_TUPLE(GrPothosBlock, __setNumInputs));
    Pothos::Block::registerCall(this, POTHOS_FCN_TUPLE(GrPothosBlock, __setNumOutputs));
}

GrPothosBlock::~GrPothosBlock(void)
{
    return;
}

void GrPothosBlock::__setNumInputs(const size_t num)
{
    for (size_t i = Pothos::Block::inputs().size(); i < num; i++)
    {
        assert(not Pothos::Block::inputs().empty());
        Pothos::Block::setupInput(i, Pothos::Block::inputs().back()->dtype());
    }
}

void GrPothosBlock::__setNumOutputs(const size_t num)
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
void GrPothosBlock::activate(void)
{
    auto block = gr::cast_to_block_sptr(d_block->shared_from_this());
    d_exec = new gr::pothos_block_executor(block);
}

void GrPothosBlock::deactivate(void)
{
    delete d_exec;
}

/***********************************************************************
 * stimulus even occured - handle inputs and call general work
 **********************************************************************/
void GrPothosBlock::work(void)
{
    //forward messages into the queues
    for (const auto &pair : Pothos::Block::allInputs())
    {
        auto inputPort = pair.second;
        while (inputPort->hasMessage()) d_block->_post(
            pmt::string_to_symbol(inputPort->name()),
            obj_to_pmt(inputPort->popMessage()));
    }

    // handle any queued up messages
    pmt::pmt_t msg;
    for (const auto &i : d_block->get_msg_map())
    {
        // Check if we have a message handler attached before getting
        // any messages. This is mostly a protection for the unknown
        // startup sequence of the threads.
        if(d_block->has_msg_handler(i.first)) {
          while((msg = d_block->delete_head_nowait(i.first))) {
            d_block->dispatch_msg(i.first,msg);
          }
        }
    }

    //re-apply reserve in-case it changed (low cost setter)
    size_t reserve = d_block->history();
    if (d_block->fixed_rate())
    {
        const size_t noutput_items = d_block->output_multiple_set()?d_block->output_multiple():1;
        reserve = d_block->fixed_rate_noutput_to_ninput(noutput_items);
    }
    for (auto input : Pothos::Block::inputs()) input->setReserve(reserve);

    //check that input and output items meets the reserve req
    const auto &workInfo = Pothos::Block::workInfo();
    if (workInfo.minInElements < reserve) return;
    if (workInfo.minOutElements == 0) return;
    if (d_block->fixed_rate() and int(workInfo.minOutElements) < d_block->fixed_rate_ninput_to_noutput(reserve)) return;

    //run the executor for one iteration to call into derived class's work()
    d_exec->run_one_iteration();
}

/***********************************************************************
 * propagateLabels
 **********************************************************************/
void GrPothosBlock::propagateLabels(const Pothos::InputPort *inputPort)
{
    switch (d_block->tag_propagation_policy())
    {
    case gr::block::TPP_DONT: return;
    case gr::block::TPP_ONE_TO_ONE:
    {
        if (inputPort->index() == -1) return;
        const auto portIndex = size_t(inputPort->index());
        if (portIndex >= Pothos::Block::outputs().size()) return;
        auto outputPort = Pothos::Block::output(portIndex);
        for (const auto &label : inputPort->labels())
        {
            auto newLabel = label;
            newLabel.index += d_block->sample_delay(inputPort->index());
            newLabel.index = std::llround(newLabel.index*d_block->relative_rate());
            outputPort->postLabel(label);
        }
    }
    case gr::block::TPP_ALL_TO_ALL:
    {
        for (const auto &label : inputPort->labels())
        {
            auto newLabel = label;
            newLabel.index += d_block->sample_delay(inputPort->index());
            newLabel.index = std::llround(newLabel.index*d_block->relative_rate());
            for (auto outputPort : Pothos::Block::outputs()) outputPort->postLabel(label);
        }
    }
    default: return;
    }
}

/***********************************************************************
 * custom buffer managers - circular buffers for blocks with history
 **********************************************************************/
Pothos::BufferManager::Sptr GrPothosBlock::getInputBufferManager(const std::string &name, const std::string &domain)
{
    //install circular buffer when history is enabled
    const size_t d_history = d_block->history();
    if (d_history > 1)
    {
        Pothos::BufferManagerArgs args;
        const auto require = (d_history+1)*Pothos::Block::input(name)->dtype().size();
        args.bufferSize = std::max(args.bufferSize, require*8/*factor*/);
        return Pothos::BufferManager::make("circular", args);
    }
    return Pothos::Block::getInputBufferManager(name, domain);
}

Pothos::BufferManager::Sptr GrPothosBlock::getOutputBufferManager(const std::string &name, const std::string &domain)
{
    return Pothos::Block::getOutputBufferManager(name, domain);
}

/***********************************************************************
 * registration
 **********************************************************************/
static Pothos::BlockRegistry registerGrPothosBlock(
    "/gnuradio/block", &GrPothosBlock::make);
