#include "common.h"
#include "vesavga.h"
#include "serial.h"
#include "ata.h"
#include "multiboot2.h"
#include "PageDirectory.h"
#include "Frames.h"
#include "TextFrameBuffer.h"
#include "MultiBootInfoHeader.h"
#include "BootInformation.h"
#include "vfs.h"
#include "timer.h"
#include "cpuinfo.h"
#include "fat.h"

void init_idt64_table();

// uint32_t mboot_header=0;

volatile int foo___ = 0;
static void test_page_fault();

uint32_t indent=0;

void cmain (BootInformation &bootInfo, const MultiBootInfoHeader *addr);

extern Frames<uint64_t> *initHeap();
void mapMemory(Frames<uint64_t> *frames, const multiboot_tag_mmap *mmap);
// uint64_t RTC_currentTime();
void init_rct_interrupts();
// void init_timer(uint32_t frequency);
void dump_fat_table(const BootBlock &boot);
void dump_root_dir(const BootBlock &boot);
void print_clusters(const BootBlock &boot, uint16_t startCluster, uint32_t fileSize);
void print_file(const BootBlock &boot, const char *fileName, const char *ext);

static void SetGlobalPageBits();

static const char *yn(uint32_t n)
{
	return n ? "YES" : "no";
}

extern "C"
{
	void initTextFrameBuffer();
	extern uint64_t placement_address;
	void __libc_init_array (void);
	extern void startup_data_start();
	extern void startup_data_end();
	extern void report_idt_info();

	extern uint64_t p3_gb_mapped_table[512];
	extern uint64_t p4_table[512];

	extern void invalidate_all_tlbs();

	void kmain64(uint32_t magic, const MultiBootInfoHeader *mboot_header)
	{
		// Entry - at this point, we're in 64-bit long mode with a basic
		// page table that identity maps the first 2 MB or RAM
		// install our interrupt handlers
//		init_idt64_table();

//		__libc_init_array();

		/*  Am I booted by a Multiboot-compliant boot loader? */
		if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
		{
			char buffer[40];
			sprintf(buffer,"Invalid magic number: 0x%x\n", (unsigned) magic);
			PANIC (buffer);
		}


		if(mboot_header && placement_address < reinterpret_cast<uint64_t>(mboot_header))
		{
			// if the mboot header is higher up in memory that placement_address
			// then copy the mboot header down to the bottom of our memory
			// so we can allow placement_address to run across the header's memory.
			auto size = mboot_header->size;
			memcpy(reinterpret_cast<void *>(placement_address), reinterpret_cast<const void *>(mboot_header), size);
			mboot_header = reinterpret_cast<const MultiBootInfoHeader *>(placement_address);
			placement_address += size;
		}
		initTextFrameBuffer();
		set_foreground_color((uint8_t)TextColors::GREEN);
		set_background_color((uint8_t)TextColors::BLACK);

		printf("Hello World from 64-bit long mode!!!!!\n");

		auto success = init_serial(2, BAUD_115200, BITS_8, PARITY_NONE, NO_STOP_BITS) ;
		if(success == SUCCESS)
		{
			printf("Initialized COM2\n");
		}

		debug_out("Startup Data Block: start 0x%016.16lx, end: 0x%016.16lx\n",(uint64_t)&startup_data_start, (uint64_t)&startup_data_end);
		report_idt_info();


		CpuInfo info{};
		getCpuInfo(info);

		if(info.pge)
		{
			SetGlobalPageBits();
		}

		if( info.page1gb )
		{
			auto *pte = reinterpret_cast<PTE_64_4K *>(0xfffffffffffff000);
			auto * pdpte= reinterpret_cast<PDPTE_64_1G *>(p3_gb_mapped_table);
			auto phys = (reinterpret_cast<uint64_t>(pdpte) | 0x03);
			pte->physical[256] = phys;
			invalidate_all_tlbs();
		}


		auto *frames = initHeap();
		printf("Heap Initialized...\n");

		success = init_serial(1, BAUD_115200, BITS_8, PARITY_NONE, NO_STOP_BITS) ;
		if( success == SUCCESS)
		{
			printf("Initialized COM1 port\n");
		}

		printf("COM1: %s,\tCOM2: %s\n",identify_uart(1), identify_uart(2));
		printf("COM3: %s,\tCOM4: %s\n",identify_uart(3), identify_uart(4));

		// detect ata disks & controllers.
		detectControllers();

		BootInformation bootInfo{};

		// process the mboot header.
		if( mboot_header != 0)
		{
			printf("process mboot_header\n");
			cmain(bootInfo, mboot_header);
			mapMemory(frames, bootInfo.mmap);
		}
		else
		{
			PANIC("mboot_header is NULL\n");
		}

		// printf("RTC Current Time:\n");
		// RTC_currentTime();
		init_timer(1);
		init_rct_interrupts();
		asm("sti");

		printf("Vendor Id String: %s\n", info.vendorId);
		printf("Stepping: %d, Model: %d, Family: %d, Type: %d, ExtendedModel: %d, ExtendedFamily: %d\n",
		info.stepping, info.model, info.family, info.processor_type, info.extendedModelId, info.extendedFamilyId);
		printf("Global Page Support: %s, SYSCALL support: %s, 1GB Page Support %s\n", yn(info.pge), yn(info.syscall), yn(info.page1gb));
		printf("4MB Page Support: %s, PAE support: %s, IA-32e Support: %s\n", yn(info.pse), yn(info.pae), yn(info.intel64));

		const char *str = getBrandString(info.brand_index);
		if( str )
		{
			printf("Brand String: %s\n", str);
		}

		if( info.apic )
		{
			printf("APIC detected...\n");
		}

//		asm("cli");
		BootBlock boot{};
		read(&boot, 0, sizeof(BootBlock));
		printf("BootBlock: BytesPerBlock %d, ReservedBlocks %d, NumRootDirEntries %d, TotalNumBlocks: %d\n",
		boot.BytesPerBlock(), boot.ReservedBlocks(), boot.NumRootDirEntries(), boot.TotalNumBlocks());
		printf("NumBlocksFat1: %d, NumBlocksPerTrack %d, NumHeads: %d, NumHiddenBlocks: %d\n",
		boot.NumBlocksFat1(), boot.NumBlocksPerTrack(), boot.NumHeads(), boot.NumHiddenBlocks());
		printf("TotalBlocks: %d, PhysDriveNo: %d, VolumeSerialNumber: %04.4x-%04.4x, FsId: %8.8s, BlockSig: %x\n",
		boot.TotalBlocks(), boot.PhysDriveNo(), (boot.VolumeSerialNumber() & 0xFFFF0000)>>16, boot.VolumeSerialNumber() & 0x0000FFFF, boot.FsId(), boot.BlockSig());
		printf("Volume Label: %11.11s\n", boot.volume_label);

		// dump_fat_table(boot);
		dump_root_dir(boot);
		// printf("Printing README.TXT\n");
		// print_clusters(boot, 578);
		// print_file(boot, "README  ","TXT");
		// printf("================== DONE =====================\n");
//		asm("sti");


		while(true)
		{
			// asm("sti");
			asm("hlt");
		}

		// test_page_fault();


	}

}

extern Page4K *getPage(void *vaddr);

void mapMemory(Frames<uint64_t> *frames, const multiboot_tag_mmap *mmap)
{
	const uint8_t *end = reinterpret_cast<const uint8_t *>(mmap) + mmap->size;
	for (auto *entry = mmap->entries;
		 reinterpret_cast<const uint8_t *>(entry) < end;
		 entry = reinterpret_cast<const multiboot_memory_map_t *> (reinterpret_cast<const uint8_t *>(entry) + mmap->entry_size))
	{
		// mark anything that's not available as in use
		// so we don't try to use these addresses.
		if( entry->type != MULTIBOOT_MEMORY_AVAILABLE )
		{
			uint64_t nPages = (entry->len>>12);
			frames->markFrames(entry->addr, nPages);
			auto addr = entry->addr;
			for( auto i=0ull; i<nPages; ++i, addr += 0x1000 )
			{
				auto *page = getPage(reinterpret_cast<void *>(addr));
				if( page )
				{
					page->rw = 0; 	// mark the page as read-only
					page->user = 0;	// only kernel access
				}
			}
		}
	}
}

static void test_page_fault()
{
	printf("Testing Page Fault\n");
	uint32_t *ptr = (uint32_t*)0xA0000000;
	uint32_t do_page_fault = *ptr;
	monitor_write_dec(do_page_fault);
	monitor_write("After page fault! SHOULDN'T GET HERE \n");
}

static void SetGlobalPageBits()
{
	PTE_64_4K *pte = reinterpret_cast<PTE_64_4K *>(0xfffffffffffff000);
	printf("pte->physical[511] == 0x%016.16lx\n", pte->physical[511]);
	pte->physical[511] |= (1<<8);
	// make the recursive entry in plme4 global
}