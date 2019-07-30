#include "kmalloc.h"
#include "vesavga.h"
#include "sym6432.h"

extern "C"
{
	constexpr uint64_t ALIGN_MASK = 0xFFFFFFFFFFFFF000ull;
	constexpr uint64_t BITS_MASK = 0x0000000000000FFFull;
	extern uint64_t placement_address;
	static void *kmalloc_generic(size_t sz, bool align, void **phys);

	void *kmalloc(size_t sz)
	{
		return kmalloc_generic(sz,false,nullptr);
	}

	void *kmalloc_aligned(size_t sz)
	{
		return kmalloc_generic(sz,true,nullptr);
	}

	void *kmalloc_phys(size_t sz, void **phys)
	{
		return kmalloc_generic(sz,false,phys);
	}

	void *kmalloc_aligned_phys(size_t sz, void **phys)
	{
		return kmalloc_generic(sz, true, phys);
	}
	
	static void page_align_placement()
	{
		if( 0 != (placement_address & BITS_MASK) )
		{
			// Align it.
			placement_address &= ALIGN_MASK;
			placement_address += 0x1000;		
		}
	}

	static void *kmalloc_generic(size_t sz, bool align, void **phys)
	{
		if( align )
		{
			page_align_placement();
		}

		auto *tmp = reinterpret_cast<void *>(placement_address);
		if (phys)
		{
			*phys = tmp;
		}
		
		placement_address += sz;
		return tmp;
	}
}
