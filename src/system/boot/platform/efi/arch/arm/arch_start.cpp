/*
 * Copyright 2019-2020 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */


#include <boot/platform.h>
#include <boot/stage2.h>
#include <boot/stdio.h>

#include "efi_platform.h"


extern "C" void arch_enter_kernel(struct kernel_args *kernelArgs,
	addr_t kernelEntry, addr_t kernelStackTop);

extern void arch_mmu_post_efi_setup(size_t memory_map_size,
    efi_memory_descriptor *memory_map, size_t descriptor_size,
    uint32_t descriptor_version);

void
arch_start_kernel(addr_t kernelEntry)
{
	// Prepare to exit EFI boot services.
	// Read the memory map.
	// First call is to determine the buffer size.
	size_t mmap_size = 0;
	size_t map_key;
	size_t descriptor_size;
	uint32_t descriptor_version;
	if (kBootServices->GetMemoryMap(&mmap_size, NULL, &map_key,
			&descriptor_size, &descriptor_version) != EFI_BUFFER_TOO_SMALL)
		panic("Unable to determine size of system memory map");


	// Allocate a buffer twice as large as needed just in case it gets bigger
	// between calls to ExitBootServices.
	size_t allocated_mmap_size = mmap_size * 2;
	efi_memory_descriptor *mmap =
		(efi_memory_descriptor *)kernel_args_malloc(allocated_mmap_size);

	if (mmap == NULL)
		panic("Unable to allocate memory map.");

	dprintf("Calling ExitBootServices. So long, EFI!\n");
	do {
		mmap_size = allocated_mmap_size;
		if (kBootServices->GetMemoryMap(&mmap_size, mmap, &map_key,
				&descriptor_size, &descriptor_version) != EFI_SUCCESS)
			panic("Unable to fetch system memory map.");

	} while (kBootServices->ExitBootServices(kImage, map_key) != EFI_SUCCESS);
	// The console was provided by boot services, disable it.
	stdout = NULL;
	stderr = NULL;

	dprintf("Memory map after exit:\n");
	addr_t addr = (addr_t)mmap;
	for (size_t offset = 0; offset < mmap_size; offset += descriptor_size) {
		efi_memory_descriptor *entry = (efi_memory_descriptor *)(addr + offset);
		dprintf("  %#lx-%#lx  %#lx %#x %#lx\n", entry->PhysicalStart,
			entry->PhysicalStart + entry->NumberOfPages * B_PAGE_SIZE,
			entry->VirtualStart, entry->Type, entry->Attribute);
	}

	// Update EFI, generate final kernel physical memory map, etc.
	arch_mmu_post_efi_setup(mmap_size, mmap, descriptor_size,
		descriptor_version);

	//smp_boot_other_cpus(final_pml4, kernelEntry);

	// Enter the kernel!
	arch_enter_kernel(&gKernelArgs, kernelEntry,
		gKernelArgs.cpu_kstack[0].start + gKernelArgs.cpu_kstack[0].size);
}
