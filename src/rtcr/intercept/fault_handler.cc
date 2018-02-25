/*
 * fault_handler.cpp
 *
 *  Created on: Jan 25, 2018
 *      Author: josef
 */
#include "fault_handler.h"
#include "../intercept/cpu_session.h"
#include "../arm_v7a/instruction.h"

using namespace Rtcr;


Managed_region_map_info *Fault_handler::_find_faulting_mrm_info()
{
	Genode::Region_map::State state;
	Managed_region_map_info *result_info = nullptr;

	Ram_dataspace_info *ramds_info = _ramds_infos.first();
	while(ramds_info && !result_info)
	{
		if(!ramds_info->mrm_info)
			continue;

		Genode::Region_map_client rm_client(ramds_info->mrm_info->region_map_cap);

		if(rm_client.state().type != Genode::Region_map::State::READY)
		{
			result_info = ramds_info->mrm_info;
			break;
		}

		ramds_info = ramds_info->next();
	}

	return result_info;
}


void Fault_handler::_handle_fault()
{
	// Find faulting Managed_region_info
	Managed_region_map_info *faulting_mrm_info = _find_faulting_mrm_info();


	// Get state of faulting Region_map
	Genode::Region_map::State state = Genode::Region_map_client{faulting_mrm_info->region_map_cap}.state();

	if(verbose_debug)
	{
	Genode::log("Handle fault: Region map ",
			faulting_mrm_info->region_map_cap, " state is ",
			state.type == Genode::Region_map::State::READ_FAULT  ? "READ_FAULT"  :
			state.type == Genode::Region_map::State::WRITE_FAULT ? "WRITE_FAULT" :
			state.type == Genode::Region_map::State::EXEC_FAULT  ? "EXEC_FAULT"  : "READY",
			" pf_addr=", Genode::Hex(state.addr), " ip: ", Genode::Hex(state.pf_ip));
	}

	// Find dataspace which contains the faulting address

	Designated_dataspace_info *dd_info = faulting_mrm_info->dd_infos.first();
	if(dd_info) dd_info = dd_info->find_by_addr(state.addr);


	// Check if a dataspace was found
	if(!dd_info)
	{
		Genode::warning("No designated dataspace for addr = ", state.addr,
				" in Region_map ", faulting_mrm_info->region_map_cap);
		return;
	}


	// Find thread which caused the fault

	Cpu_thread_component * cpu_thread = nullptr;
	Cpu_session_component* c = Cpu_session_component::current_session;
	while (c) {
		// Iterate through every CPU thread
		cpu_thread =
				c->parent_state().cpu_threads.first();
		int j = 0;
		while (cpu_thread) {
			// Pause the CPU thread
			//if(cpu_thread->state().unresolved_page_fault)
			{
				if(state.pf_ip == cpu_thread->state().ip)
				{
					PINF("Page-faulting Thread: %i, Pagefault: %i, IP: %lx", j,
						cpu_thread->state().unresolved_page_fault,
						cpu_thread->state().ip);
					goto found_thread;
				}
				j++;
			}
			cpu_thread = cpu_thread->next();
		}

		c = c->next();
	}

	found_thread:

	//TODO: Find memory region/dataspace that the ip points into




	//example instruction
	unsigned int instr = 0b00000000010001010101001110110100;
	//unsigned int instr = 0xe5832004;
	//TODO: get real instruction

	/* decode the instruction and update states accordingly */
	bool writes = false;
	bool ldst = Instruction::load_store(instr, writes, state.format, state.reg);


	PINF("instruction: %x, %s, %s, %s, reg: %x", instr, ldst ? "LOADSTORE" : "OTHER",
			writes ? "store" : "load",
				state.format == Region_map::LSB8 ? "LSB8 " : (state.format == Region_map::LSB16 ? "LSB16" : "LSB32"), state.reg);


	// Don't attach found dataspace to its designated address,
	// because we need to continue receiving pagefaults in order
	// to simulate them
	//dd_info->attach();
	//dd_info->detach();

	//TODO: simulate instruction
	char* addr = _env.rm().attach(dd_info->cap);
	long long unsigned int value;
	memcpy(&value,addr + state.addr,sizeof(value));
	PINF("Value at %lx: %llx", state.addr, value);



    static Rom_connection rom("sheepcount");
	Dataspace_capability elf_ds = rom.dataspace();

	/* attach ELF locally */
	addr_t elf_addr;
	try {
		elf_addr = _env.rm().attach(elf_ds);
	} catch (Region_map::Attach_failed) {
		error("local attach of ELF executable failed");
		throw;
	}

	/* setup ELF object and read program entry pointer */
	Elf_binary elf(elf_addr);
	if (!elf.valid())
		PINF("Invalid binary");

	PINF("First 4 Bytes: %x", *(uint8_t* )elf_addr);

	addr_t entry = elf.entry();
	Elf_segment seg;
	for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {
		if (seg.flags().skip)
			continue;

		/* same values for r/o and r/w segments */
		addr_t const addr = (addr_t) seg.start();
		size_t const size = seg.mem_size();

		bool parent_info = false;

		bool const write = seg.flags().w;
		bool const exec = seg.flags().x;
		if (!write) {
			/* read-only segment */

			if (seg.file_size() != seg.mem_size())
				warning("filesz and memsz for read-only segment differ");

			off_t const offset = seg.file_offset();
			if (exec)
			//remote_rm.attach_executable(elf_ds, addr, size, offset);
			{
				addr_t inst_addr = 0x1001bdc;
				//TODO: EINHÄNGEN, und an anderer Stelle damit keine Überlappung
				//env.rm().attach(elf_ds, size, addr, true, offset, true);
				PINF("Address: %lx, Offset: %lx, value: %x, target inst: %x",
						addr, offset, *((uint32_t* ) (elf_addr + offset)),
						*((uint32_t* ) (elf_addr + offset + inst_addr - addr)));
			}
			//	else
			//	remote_rm.attach_at(elf_ds, addr, size, offset);

		}
	}

	_env.rm().detach(elf_addr);







	//value+=10;
	//memcpy(addr + state.addr,&value,sizeof(value));
	_env.rm().detach(addr);
	//dd_info->attach();



	// Increase instruction pointer (ip) by one word
	Genode::Thread_state st = cpu_thread->state();
	st.ip += Instruction::size();
	cpu_thread->state(st);
	PINF("IP: %lx", st.ip);

	//continue execution since we resolved the pagefault
	Genode::Region_map_client{faulting_mrm_info->region_map_cap}.processed(state);
}


Fault_handler::Fault_handler(Genode::Env &env, Genode::Signal_receiver &receiver,
		Genode::List<Ram_dataspace_info> &ramds_infos)
:
	Thread(env, "managed dataspace pager", 16*1024),
	_env(env),
	_receiver(receiver), _ramds_infos(ramds_infos)
{ }


void Fault_handler::entry()
{
	while(true)
	{
		Genode::Signal signal = _receiver.wait_for_signal();
		for(unsigned int i = 0; i < signal.num(); ++i)
			_handle_fault();
	}
}
