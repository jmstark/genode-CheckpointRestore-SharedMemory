#ifndef _RTCR_FAULT_HANDLER_H_
#define _RTCR_FAULT_HANDLER_H_

#include <base/thread.h>
#include <rom_session/connection.h>

#include <../src/include/base/internal/elf.h>

#include "../online_storage/ram_dataspace_info.h"
#include "../online_storage/redundant_memory_ds_info.h"

namespace Rtcr {
	class Fault_handler;

	constexpr bool fh_verbose_debug = true;
}

/**
 * \brief Page fault handler designated to handle the page faults caused for the
 * incremental checkpoint mechanism of the custom Ram_session_component
 *
 * Page fault handler which has a list of region maps and their associated dataspaces.
 * Each page fault signal is handled by finding the faulting region map and attaching
 * the designated dataspace to the faulting address.
 */
class Rtcr::Fault_handler : public Genode::Thread
{
private:

	Genode::Env& _env;
	/**
	 * Enable log output for debugging
	 */
	static constexpr bool verbose_debug = fh_verbose_debug;
	/**
	 * Signal_receiver on which the page fault handler waits
	 */
	Genode::Signal_receiver          &_receiver;
	/**
	 * List of region maps and their associated dataspaces
	 * It must contain Managed_region_map_info
	 */
	Genode::List<Ram_dataspace_info> &_ramds_infos;
	/**
	 * ROM name of ELF binary for redundant memory -> access instruction for emulation.
	 * If "", do not use redundant memory.
	 */
	Genode::String<32>  _name;
	/**
	 * Address where the ELF binary is attached
	 */
	Genode::addr_t elf_addr;
	/**
	 * Offset of the code segment of the binary
	 */
	Genode::off_t elf_seg_offset;
	/**
	 * Size of the code segment of the binary
	 */
	Genode::size_t elf_seg_size;
	/**
	 * Virtual memory address of the code segment
	 */
	Genode::addr_t elf_seg_addr;
	/**
	 * Find the first faulting Region_map in the list of Ram_dataspaces
	 *
	 * \return Pointer to Managed_region_map_info which contains the faulting Region_map
	 */
	Managed_region_map_info *_find_faulting_mrm_info();
	/**
	 * Handles the page fault by attaching a designated dataspace into its region map
	 */
	void _handle_fault();
	/**
	 * Handles the page fault by emulating the instruction with redundant writing
	 */
	void _handle_fault_redundant_memory();

public:
	Fault_handler(Genode::Env &env, Genode::Signal_receiver &receiver,
			Genode::List<Ram_dataspace_info> &ramds_infos, const char* name = "");
	/**
	 * Entrypoint of the thread
	 * The thread waits for a signal and calls the handler function if it receives any signal
	 */
	void entry();
};






#endif /* _RTCR_FAULT_HANDLER_H_ */
