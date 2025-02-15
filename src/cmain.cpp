/*  kernel.c - the C part of the kernel */
/*  Copyright (C) 1999, 2010  Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "multiboot2.h"
#include "vesavga.h"
#include "TextFrameBuffer.h"
#include "MultiBootInfoHeader.h"
#include "BootInformation.h"
#include "elf/elf.h"
#include "VMManager.h"
/*  Macros. */

/*  Some screen stuff. */
/*  The number of columns. */
#define COLUMNS                 80
/*  The number of lines. */
#define LINES                   24
/*  The attribute of an character. */
#define ATTRIBUTE               7
/*  The video memory address. */
#define VIDEO                   0xB8000

// #define HIDWORD(a) ((unsigned)(a >> 32))
// #define LODWORD(a) ((unsigned)(a & 0xFFFFFFFF))

/*  Variables. */
/*  Save the X position. */
//static int xpos;
/*  Save the Y position. */
//static int ypos;
/*  Point to the video memory. */
static volatile unsigned char *video;

/*  Forward declarations. */
void cmain (unsigned long magic, unsigned long addr);
static void cls (void);
//static void itoa (char *buf, int base, int d);
//static void putchar (int c);
int printf (const char *format, ...);

#define MMAP_RAM 1
#define MMAP_ACPI 3
#define MMAP_DEFECTIVE 5


#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_NVS                    4
#define MULTIBOOT_MEMORY_BADRAM                 5

static
const char *mmap_type(int n)
{
	switch(n)
	{
		case MULTIBOOT_MEMORY_AVAILABLE:
			return "available";
		case MULTIBOOT_MEMORY_RESERVED:
			return "reserved";
		case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
			return "acpi";
		case MULTIBOOT_MEMORY_NVS:
			return "nvs";
		case MULTIBOOT_MEMORY_BADRAM:
			return "bad";
	}
	return "unknown";
}

void
print_mem_size(uint64_t size, const char *type)
{
	if( size < 10*1024 )
	{
		printf("%u %s", (uint32_t)size, type);
	}
	else if( size < 10*1024*1024 )
	{
		printf("%uK %s", (uint32_t)(size/1024), type);
	}
	else if( size < 10ull*1024ull*1024ull*1024ull )
	{
		printf("%uM %s", (uint32_t)(size/(1024*1024)), type);
	}
	else if( size < 10ull*1024ull*1024ull*1024ull*1024ull )
	{
		printf("%uG %s", (uint32_t)(size/(1024ull*1024ull*1024ull)), type);
	}
	else
	{
		printf("%uT %s", (uint32_t)(size/(1024ull*1024ull*1024ull*1024ull)), type);
	}
}

static void print_elf_sections(const multiboot_tag_elf_sections *elf)
{
	printf("elf sections, num %d, entsize %d, shndx: %d\n", elf->num, elf->entsize, elf->shndx);	
}


static void print_apm(const multiboot_tag_apm *apm)
{
	printf("apm\n");
	printf("Version: %d, cseg 0x%04.4x, offset 0x%08.8x\n", apm->version, apm->cseg, apm->offset);
	printf("cseg16: 0x%04.4x, dseg: 0x%04.4x, flags: 0x%04.4x\n", apm->cseg_16, apm->dseg, apm->flags );
	printf("cseg_len: %d, cseg_16_len: %d, dseg_len: %d\n", apm->cseg_len, apm->cseg_16_len, apm->dseg_len);
}

static void print_load_base_addr(const multiboot_tag_load_base_addr *info)
{
	printf("load base addr: 0x%08.8x\n", info->load_base_addr);
}

static void print_acpi_old(const multiboot_tag_old_acpi *acpi)
{
	const unsigned char *ptr = acpi->rsdp;
	printf("acpi old\n");
	printf("Signature: %.8s, ", (char *)ptr );
	ptr += 8;
	printf("checksum: %d, ", (unsigned)(*ptr));
	ptr++;
	printf("OEMID: %.6s,\n", (char *)ptr );
	ptr += 6;
	printf("revision: %d, ", (unsigned)(*ptr));
	++ptr;
	printf("RstAddress: 0x%08.8x\n", *((multiboot_uint32_t *)ptr) );
}

static void print_acpi_new(const multiboot_tag_new_acpi *acpi)
{
	const unsigned char *ptr = acpi->rsdp;
	printf("acpi new\n");
	printf("Signature: %.8s, ", (char *)ptr );
	ptr += 8;
	printf("checksum: %d, ", (unsigned)(*ptr));
	ptr++;
	printf("OEMID: %.6s,\n", (char *)ptr );
	ptr += 6;
	printf("revision: %d, ", (unsigned)(*ptr));
	++ptr;
	printf("RstAddress: 0x%08.8x\n", *((multiboot_uint32_t *)ptr) );
	ptr += 4;
	printf("Length: %d\t", *((multiboot_uint32_t *)ptr) );
	ptr += 4;
	printf("XsdtAddress 0x08.8%x08.8%x\t", *((multiboot_uint32_t *)ptr+4), *((multiboot_uint32_t *)ptr) );
	ptr += 8;
	printf("reserved: {%d, %d, %d}\n", (unsigned)ptr[0], (unsigned)ptr[1], (unsigned)ptr[2]);
}

static void print_cmdline(const multiboot_tag_string *cmd)
{
	printf ("Command line = %s\n", cmd->string);
}

static void print_bootloader_name(const multiboot_tag_string *name)
{
	printf ("Boot loader name = %s\n", name->string);

}

extern bool allocPages(uint64_t startAddress, size_t numPages, bool isKernel, bool isWritable);

static errval_t ElfAllocatorFn(void * /* state */ , genvaddr_t base, size_t size, uint32_t flags, void **ret)
{
	size_t numPages = (size + 4095) / 4096;
//	auto success = allocPages(base, numPages, true, true);
	auto success = true;
	if (success && ret)
	{
		*ret = reinterpret_cast<void *>(base);
	}
	return success;
}



static void print_module(const multiboot_tag_module *module)
{
	printf ("Module at 0x%08.8x-0x%08.8x. Command line %s\n",
		  module->mod_start, module->mod_end, module->cmdline);
	genvaddr_t entry = 0;
	auto success = elf_load(EM_X86_64, ElfAllocatorFn, nullptr, (module->mod_start | 0xFFFF800000000000), module->mod_end - module->mod_start, &entry);
	if (success == 0)
	{
		printf("elf_load() loaded module. Entry Address is : 0x%016.16lx\n", entry);
			typedef int (*PrintfT)( const char* format, ... );
			typedef void (*HelloT)(PrintfT);
			auto fn = reinterpret_cast<HelloT>(entry);
			(*fn)(printf);

	}

}

static void print_basic_meminfo(const multiboot_tag_basic_meminfo *info)
{
	printf ("mem_lower = %uKB, (0x%08.8xKB) mem_upper = %uKB (0x%08.8xKB)\n",
		info->mem_lower, info->mem_lower, info->mem_upper, info->mem_upper);
}


static void print_boot_device(const multiboot_tag_bootdev *bootdev)
{
	printf ("Boot device 0x%08.8x, %u (0x%08.8x), %u (0x%08.8x)\n",
		bootdev->biosdev, bootdev->slice, bootdev->slice, bootdev->part, bootdev->part);
}

static void print_mmap(const  multiboot_tag_mmap *mmapStart)
{
	uint64_t	ram_size = 0;
	uint64_t	acpi_size = 0;
	uint64_t	reserved_size = 0;
	uint64_t	defective_size = 0;

	printf ("mmap\n");

	for (auto *mmap = mmapStart->entries;
		 reinterpret_cast<const multiboot_uint8_t *>(mmap ) < reinterpret_cast<const multiboot_uint8_t *>(mmapStart) + mmapStart->size;
		 mmap = reinterpret_cast<const multiboot_memory_map_t *> (reinterpret_cast<const uint8_t *>(mmap) + mmapStart->entry_size)) 
	{
		printf (" base_addr = 0x%08.8x%08.8x, length = 0x%08.8x%08.8x, type = %s\n",
		HIDWORD(mmap->addr), LODWORD(mmap->addr) , 
		HIDWORD(mmap->len), LODWORD(mmap->len),
		mmap_type( mmap->type) );
		
	  /* printf (" base_addr = 0x%08.8x%08.8x,"
		  " length = 0x%08.8x%08.8x, type = %s\n",
		  HIDWORD(mmap->addr ),
		  LODWORD(mmap->addr),
		  HIDWORD (mmap->len),
		  LODWORD (mmap->len),
		  mmap_type( mmap->type) ) ; */
		  
		  if( MMAP_RAM == mmap->type)
		  {
			  ram_size += mmap->len;
		  }
		  else if( MMAP_ACPI == mmap->type)
		  {
			  acpi_size += mmap->len;
		  }
		  else if( MMAP_DEFECTIVE == mmap->type)
		  {
			  defective_size += mmap->len;
		  }
		  else
		  {
			  reserved_size += mmap->len;
		  }
	}
	printf("Totals: ");
	print_mem_size(ram_size, "RAM, ");
	print_mem_size(acpi_size, "ACPI, ");
	print_mem_size(defective_size, "Defective, ");
	print_mem_size(reserved_size, "reserved\n");
	// printf("Totals: %uK RAM, %uK ACPI, %uK Defective, %uK reserved\n", ram_size/1024, acpi_size/1024, defective_size/1024, reserved_size/1024 );

}

static uint32_t get_indexed_color(const multiboot_tag_framebuffer *tagfb)
{
	uint32_t distance;

	const multiboot_color *palette = tagfb->framebuffer_palette;

	uint32_t color = 0;
	uint32_t best_distance = 4*256*256;

	for (auto i = 0; i < tagfb->framebuffer_palette_num_colors; i++)
	{
		distance = (0xff - palette[i].blue) * (0xff - palette[i].blue) + 
					palette[i].red * palette[i].red + 
					palette[i].green * palette[i].green;
		
		if (distance < best_distance)
		{
			if (distance < best_distance)
			{
				color = i;
				best_distance = distance;
			}
		}
	}
	return color;
}

template<typename Color, const int bpp>
void set_pixel(multiboot_tag_framebuffer *tagfb, multiboot_uint32_t x, multiboot_uint32_t y, multiboot_uint32_t c)
{
	void *fb = (void *) (unsigned long) tagfb->common.framebuffer_addr;
	multiboot_uint8_t *ptr = reinterpret_cast<multiboot_uint8_t *>(fb) + tagfb->common.framebuffer_pitch * x + ((tagfb->common.framebuffer_bpp+1) / 8) * y;
	*reinterpret_cast<Color *>(ptr) = static_cast<Color>(c);
}

template<>
void set_pixel<multiboot_uint32_t, 24>(multiboot_tag_framebuffer *tagfb, multiboot_uint32_t x, multiboot_uint32_t y, multiboot_uint32_t c)
{
	void *fb = (void *) (unsigned long) tagfb->common.framebuffer_addr;
	multiboot_uint8_t *ptr = reinterpret_cast<multiboot_uint8_t *>(fb) + tagfb->common.framebuffer_pitch * x + ((tagfb->common.framebuffer_bpp+1) / 8) * y;
	uint32_t *pixel = reinterpret_cast<uint32_t *>(ptr);
	*pixel = ((c & 0x00FFFFFF) | (*pixel & 0xFF000000));
}

static void draw_line(multiboot_tag_framebuffer *tagfb, multiboot_uint32_t color, void (*set)(multiboot_tag_framebuffer *tagfb, multiboot_uint32_t x, multiboot_uint32_t y, multiboot_uint32_t c))
{
	for (auto i = 0; i < tagfb->common.framebuffer_width && i < tagfb->common.framebuffer_height; i++)
	{
		set(tagfb, i, i, color);
	}
}

static void set_pixel_color(const multiboot_tag_framebuffer *tagfb, multiboot_uint32_t color)
{
	void *fb = reinterpret_cast<void *>(static_cast<uint64_t>(tagfb->common.framebuffer_addr) + VM_BASE);
	
	for (auto i = 0; i < tagfb->common.framebuffer_width && i < tagfb->common.framebuffer_height; i++)
	{
		multiboot_uint8_t *ptr = reinterpret_cast<multiboot_uint8_t *>(fb) + tagfb->common.framebuffer_pitch * i + ((tagfb->common.framebuffer_bpp+1) / 8) * i;
		switch (tagfb->common.framebuffer_bpp)
		{
			case 8:
				*ptr = color;
				break;
			case 15:
			case 16:
				*reinterpret_cast<multiboot_uint16_t *>(ptr) = color;
				break;
			case 24:
				{
					multiboot_uint32_t *pixel = reinterpret_cast<multiboot_uint32_t *>(ptr);
					*pixel = ((color & 0x00FFFFFF) | (*pixel & 0xFF000000));
				}
				break;
			case 32:
				*reinterpret_cast<multiboot_uint32_t *>(ptr) = color;
				break;
		}
	}
}

static void print_frame_buffer(const multiboot_tag_framebuffer *tagfb)
{
	multiboot_uint32_t color;
	unsigned i;
	//~ struct multiboot_tag_framebuffer *tagfb
	  //~ = (struct multiboot_tag_framebuffer *) tag;
	void *fb = (void *) (unsigned long) tagfb->common.framebuffer_addr;

	printf("framebuffer: type: %d\n", tagfb->common.framebuffer_type);
	printf("addr: 0x%08.8x%08.8x, ", HIDWORD(tagfb->common.framebuffer_addr), LODWORD(tagfb->common.framebuffer_addr));
	printf("pitch: %d, width: %d, height: %d, bpp: %d\n", tagfb->common.framebuffer_pitch,
	tagfb->common.framebuffer_width, tagfb->common.framebuffer_height, tagfb->common.framebuffer_bpp);

	switch (tagfb->common.framebuffer_type)
	  {
	  case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED:
		color = get_indexed_color(tagfb);
		break;

	  case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
		color = ((1 << tagfb->framebuffer_blue_mask_size) - 1)  << tagfb->framebuffer_blue_field_position;
		break;

	  case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
		color = '\\' | 0x0100;
		break;

	  default:
		color = 0xffffffff;
		break;
	  }
	
	  set_pixel_color(tagfb, color);
}

#define STR(a) #a
#define CONCAT(a,b) a##b

#define CASE(a) case a: return #a;

const char *
tagType2String(multiboot_uint16_t type)
{
	switch(type)
	{
		CASE(MULTIBOOT_TAG_TYPE_END);
		CASE(MULTIBOOT_TAG_TYPE_CMDLINE);
		CASE(MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME);
		CASE(MULTIBOOT_TAG_TYPE_MODULE);
		CASE(MULTIBOOT_TAG_TYPE_BASIC_MEMINFO);
		CASE(MULTIBOOT_TAG_TYPE_BOOTDEV);
		CASE(MULTIBOOT_TAG_TYPE_MMAP);
		CASE(MULTIBOOT_TAG_TYPE_VBE);
		CASE(MULTIBOOT_TAG_TYPE_FRAMEBUFFER);
		CASE(MULTIBOOT_TAG_TYPE_ELF_SECTIONS);
		CASE(MULTIBOOT_TAG_TYPE_APM);
		CASE(MULTIBOOT_TAG_TYPE_EFI32);
		CASE(MULTIBOOT_TAG_TYPE_EFI64);
		CASE(MULTIBOOT_TAG_TYPE_SMBIOS);
		CASE(MULTIBOOT_TAG_TYPE_ACPI_OLD);
		CASE(MULTIBOOT_TAG_TYPE_ACPI_NEW);
		CASE(MULTIBOOT_TAG_TYPE_NETWORK);
		CASE(MULTIBOOT_TAG_TYPE_EFI_MMAP);
		CASE(MULTIBOOT_TAG_TYPE_EFI_BS);
		CASE(MULTIBOOT_TAG_TYPE_EFI32_IH);
		CASE(MULTIBOOT_TAG_TYPE_EFI64_IH);
		CASE(MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR);
		default:
			break;
	}
	return "<unknown>";
}

const multiboot_tag *findMultiBootInfoHeaderTag(const MultiBootInfoHeader *addr, multiboot_uint32_t type)
{
	for( auto *tag = addr->tags;
		 tag->type != MULTIBOOT_TAG_TYPE_END;
		 tag = (struct multiboot_tag *)((multiboot_uint8_t *)tag
		+ ((tag->size + 7) & ~7)) )
	{
		if( tag->type == type )
		{
			return tag;
		}
	}
	return nullptr;
}

/*  Check if MAGIC is valid and print the Multiboot information structure
   pointed by ADDR. */
void
cmain (BootInformation &bootInfo, const MultiBootInfoHeader *addr)
{  
	const multiboot_tag *tag;
	unsigned size;

	/*  Clear the screen. */
//	cls (); 

	if(!addr)
	{
		printf("<NULL> MultiBootInfoHeader\n");
		return;
	}
	
	if ( reinterpret_cast<uintptr_t>(addr) & 7)
	{
	  printf ("Unaligned mbi: 0x%lx\n", reinterpret_cast<uintptr_t>(addr));
	  return;
	}

	  size = addr->size;
	  printf ("Announced mbi size %d (%08.8x)\n", size, size);
	  for (tag = addr->tags;
		   tag->type != MULTIBOOT_TAG_TYPE_END ;
		   tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag 
										   + ((tag->size + 7) & ~7)))
	{
		uint8_t oldColor = set_foreground_color((uint8_t)TextColors::RED);
		// printf ("Tag 0x%08.8x, Size %d\n", tag->type, tag->size);
		printf("Tag: %s, Size %d\n",tagType2String(tag->type), tag->size);
		if( oldColor != 0xFF )
		{
			set_foreground_color(oldColor);
		}
		switch (tag->type)
		{
			case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
				bootInfo.elf_sections = reinterpret_cast<const multiboot_tag_elf_sections *>(tag);
				print_elf_sections(bootInfo.elf_sections);
				break;
			case MULTIBOOT_TAG_TYPE_APM:
				bootInfo.apm = reinterpret_cast<const multiboot_tag_apm *>(tag);
				print_apm(bootInfo.apm);
				break;
			case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR:
				bootInfo.load_base_addr = reinterpret_cast<const multiboot_tag_load_base_addr *>(tag);
				print_load_base_addr(bootInfo.load_base_addr );
				break;
			case MULTIBOOT_TAG_TYPE_ACPI_OLD:
				bootInfo.acpi_old = reinterpret_cast<const multiboot_tag_old_acpi *>(tag);
				print_acpi_old(bootInfo.acpi_old);
				break;
			case MULTIBOOT_TAG_TYPE_ACPI_NEW:
				bootInfo.acpi_new = reinterpret_cast<const multiboot_tag_new_acpi *>(tag);
				print_acpi_new(bootInfo.acpi_new);
				break;
			case MULTIBOOT_TAG_TYPE_CMDLINE:
				bootInfo.command_line = reinterpret_cast<const multiboot_tag_string *>(tag);
				print_cmdline(bootInfo.command_line);
				break;
			case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
				bootInfo.bootloader_name = reinterpret_cast<const multiboot_tag_string *>(tag);
				print_bootloader_name(bootInfo.bootloader_name);
				break;
			case MULTIBOOT_TAG_TYPE_MODULE:
				bootInfo.module_info = reinterpret_cast<const multiboot_tag_module *>(tag);
				print_module(bootInfo.module_info);
				break;
			case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
				bootInfo.basic_meminfo = reinterpret_cast<const multiboot_tag_basic_meminfo *>(tag);
				print_basic_meminfo(bootInfo.basic_meminfo);
				break;
			case MULTIBOOT_TAG_TYPE_BOOTDEV:
				bootInfo.boot_device = reinterpret_cast<const multiboot_tag_bootdev *>(tag);
				print_boot_device(bootInfo.boot_device);
				break;
			case MULTIBOOT_TAG_TYPE_MMAP:
				bootInfo.mmap = reinterpret_cast<const multiboot_tag_mmap *>(tag);
				print_mmap(bootInfo.mmap);
				break;
			case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
				bootInfo.frame_buffer = reinterpret_cast<const multiboot_tag_framebuffer *>(tag);
				print_frame_buffer(bootInfo.frame_buffer);
				break;
		}
	}
	tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag 
								  + ((tag->size + 7) & ~7));
	printf ("Total mbi size %u (0x%08.8x)\n", (unsigned) (unsigned long long)((uint8_t *)tag - (uint8_t *)addr), (unsigned)(unsigned long long) ((uint8_t *)tag - (uint8_t *)addr));
}    

/*  Clear the screen and initialize VIDEO, XPOS and YPOS. */
static void
cls (void)
{
	extern void monitor_clear(void);
	monitor_clear();
}

#if 0

/*  Convert the integer D to a string and save the string in BUF. If
   BASE is equal to 'd', interpret that D is decimal, and if BASE is
   equal to 'x', interpret that D is hexadecimal. */
static void
itoa (char *buf, int base, int d)
{
  char *p = buf;
  char *p1, *p2;
  unsigned long ud = d;
  int divisor = 10;
  
  /*  If %d is specified and D is minus, put '-' in the head. */
  if (base == 'd' && d < 0)
	{
	  *p++ = '-';
	  buf++;
	  ud = -d;
	}
  else if (base == 'x')
	divisor = 16;

  /*  Divide UD by DIVISOR until UD == 0. */
  do
	{
	  int remainder = ud % divisor;
	  
	  *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
	}
  while (ud /= divisor);

  /*  Terminate BUF. */
  *p = 0;
  
  /*  Reverse BUF. */
  p1 = buf;
  p2 = p - 1;
  while (p1 < p2)
	{
	  char tmp = *p1;
	  *p1 = *p2;
	  *p2 = tmp;
	  p1++;
	  p2--;
	}
}

/*  Put the character C on the screen. */
static void
putchar (int c)
{
  if (c == '\n' || c == '\r')
	{
	newline:
	  xpos = 0;
	  ypos++;
	  if (ypos >= LINES)
		ypos = 0;
	  return;
	}

  *(video + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
  *(video + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;

  xpos++;
  if (xpos >= COLUMNS)
	goto newline;
}
#endif // 0

/*  Format a string and print it on the screen, just like the libc
   function printf. 
void
printf (const char *format, ...)
{
  char **arg = (char **) &format;
  int c;
  char buf[20];

  arg++;
  
  while ((c = *format++) != 0)
	{
	  if (c != '%')
		putchar (c);
	  else
		{
		  char *p, *p2;
		  int pad0 = 0, pad = 0;
		  
		  c = *format++;
		  if (c == '0')
			{
			  pad0 = 1;
			  c = *format++;
			}

		  if (c >= '0' && c <= '9')
			{
			  pad = c - '0';
			  c = *format++;
			}

		  switch (c)
			{
			case 'd':
			case 'u':
			case 'x':
			  itoa (buf, c, *((int *) arg++));
			  p = buf;
			  goto string;
			  break;

			case 's':
			  p = *arg++;
			  if (! p)
				p = "(null)";

			string:
			  for (p2 = p; *p2; p2++);
			  for (; p2 < p + pad; p2++)
				putchar (pad0 ? '0' : ' ');
			  while (*p)
				putchar (*p++);
			  break;

			default:
			  putchar (*((int *) arg++));
			  break;
			}
		}
	}
}
*/
