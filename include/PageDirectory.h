#ifndef PAGEDIRECTORY_H_INCLUDED
#define PAGEDIRECTORY_H_INCLUDED

#include "kmalloc.h"
#include "PageT.h"
#include "memcpy.h"
#include "NewObj.h"

template<const int SHIFT>
const char *DirectoryName();


template<typename T, const int SHIFT, const int BITS>
struct PageDirectory
{
	using PageType=typename T::PageType;

	static constexpr int bits=BITS;
	static constexpr uintptr_t MASK = ((1<<BITS)-1);
	static constexpr decltype(sizeof(int)) NUM_ENTRIES = 4096 / sizeof(uintptr_t);
	
	static constexpr  size_t OFFSET_BITS=T::OFFSET_BITS;
	
	static constexpr bool is_huge=false;
	
	static constexpr size_t shift=T::shift+BITS;
	
	static constexpr uintptr_t ATTRS = ((OFFSET_BITS >12 ) ? 0x03 : 0x03);

	static constexpr bool IsDirectory = true;
	
	uintptr_t physical[NUM_ENTRIES];

	PageDirectory()
	{
		memset(physical, '\0', sizeof(physical));
	}

	void setPhys(uintptr_t p)
	{
	}

	static uintptr_t index(uintptr_t vaddr)
	{
		return ((vaddr>>(SHIFT)) & MASK);
	}

	T *operator[](int n)const
	{
		constexpr uintptr_t MASK = (~((uintptr_t)(0xFFF)));
		const auto *ptr  = T::IsDirectory ? reinterpret_cast<const T *>(MASK & physical[n]) : reinterpret_cast<const T *>(&physical[n]);
		return const_cast <T *>(ptr);
	}

	PageType *getPage(uintptr_t vaddr)
	{
		auto i = index(vaddr);
		T *table = (*this)[i];
		if( table == nullptr )
		{
			ASSERT(T::IsDirectory==true);
			void *phys = 0;
			table = AlignedNewPhys<T>(&phys );
			physical[i] = static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(phys)|ATTRS); // PRESENT, RW, US.
		}
		return table->getPage(vaddr);
	}

	PageType *findPage(uintptr_t vaddr)
	{
		auto i = index(vaddr);
		T *table = (*this)[i];
		return table ? table->findPage(vaddr) : nullptr;
	}

	void dump(uint64_t vaddr)const
	{
#if 1 // PageDirectory<T,%d,%d>
		if( 0 != (vaddr & 0x0000800000000000) )
		{
			vaddr |= 0xFFFF000000000000;
		}

		INDENT();debug_out("%s: this 0x%016.16lx\n", DirectoryName<SHIFT>(), (uint64_t)this);
		++indent;
		for (auto i = 0ul; i < NUM_ENTRIES; ++i)
		{
			auto vaddr2 = (vaddr | (i<<SHIFT));
			uint64_t entry = (uint64_t)physical[i];
			if( entry != 0)
			{
				INDENT();debug_out("0x%016.16lx, %d == 0x%016.16lx \n", vaddr2, i, entry);
				if constexpr(sizeof(T) != sizeof(Page4K))
				{
					if ( (SHIFT == 30 || SHIFT == 21) && (entry & (1<<7)))	// check for a 1GB page entry in the level 3 table
					{														// or a 2MB page entry in the level 2 table
						;
					}
					else
					{
					
						auto *ptr = (*this)[i];
						if (ptr)
						{
							ptr->dump(vaddr2);
						}
					}
				}
			}
		}
		--indent;
		INDENT();debug_out("===========================================\n");
#endif // 0		
	}
};

template<const int BITS, const int PAGE_BITS=12>
class PageTableT
{
public:

	static constexpr decltype(sizeof(int)) NUM_PAGES = 4096 / sizeof(uintptr_t);
	static constexpr uintptr_t MASK = (~(static_cast<uintptr_t>(-1)<<BITS));

	PageTableT()
	{
		uintptr_t *p = reinterpret_cast<uintptr_t *>(pages);
		for( auto i=0; i<NUM_PAGES; ++i )
		{
			p[i] = 0u;
		}
	}

	using PageType=PageT<PAGE_BITS>;
	static constexpr  size_t OFFSET_BITS=PageType::OFFBITS;

	PageType *getPage(uintptr_t vaddr)
	{
		return &(pages[( (vaddr >> (PAGE_BITS)) & MASK)]);
	}

	void setPhys(uint32_t){}

	uintptr_t operator[](int n)const
	{
		return *(uintptr_t *)(&pages[n]);
	}

	void dump()
	{
		for (auto i = 0u; i < NUM_PAGES; ++i)
		{
			printf("\tpage: %d == 0x%08.8x\n", i, *(uint32_t *)&(pages[i]) );
		}
		printf("\t+++++++++++++++++++++++\n");
	}


private:
	PageType pages[NUM_PAGES];

};

template<const int BITS=9>
using PageTable64 = PageTableT<BITS>;

template<typename T, const int SHIFT>
using PageDirectory64 = PageDirectory<T, SHIFT, 9>;

template<const int BITS=9>
using PTE_=PageTable64<BITS>;

template<typename T>
using PDE_ = PageDirectory64<T, 21>;

template<const int BITS=10>
using PageTable32 = PageTableT<BITS>;

template<typename T, const int SHIFT>
using PageDirectory32 = PageDirectory<T, SHIFT, 10>;

template<typename T>
using PDPTE_ = PageDirectory64<T, 30>;

template<typename T>
using PML4E_=PageDirectory64<T,39>;




using PageTable32_4M = PageDirectory32<Page4M, 22>;




using PTE32 = PageTable32<10>;

template<typename T>
using PDE32 = PageDirectory32<T, 22>;

using PDE32_4K = PDE32<PTE32>;
using PDE32_2M = PageTable32<22>;

// 32-bit PAE paging ( 4K or 2MB pages )
using PDPTE_PAE = PageDirectory32<PDE32<PTE32>, 30>;
using PDPTE_PAE_ = PageDirectory<PageDirectory<Page4K, 12,10>,30,10>;


// 32-bit 4K page:
using Page32_4K=Page4K;
// 32-bit 2M page
using Page32_2M=Page2M;
// 32-bit 4M page: - defined in PageT.h
// Page4M=PageT<uint32_t, 22>;

// 64 bit 4K page
using Page64_4K=Page4K;
// 64-bit 2M page
using Page64_2M=Page2M;
// 64-bit 1G page
using Page64_1G=Page1G;

// 32-bit 4k page table
using PTE_32_4K=PageDirectory<Page32_4K,Page32_4K::shift,10>;
using PDE_32_4K=PageDirectory<PTE_32_4K,PTE_32_4K::shift,10>;

//32-bit 4M page table
using PDE_32_4M=PageDirectory<Page4M,Page4M::shift,10>;

// 32-bit 4k PAE table
using PTE_PAE_4K=PageDirectory<Page32_4K,Page32_4K::shift, 9>;
using PDE_PAE_4K=PageDirectory<PTE_PAE_4K,PTE_PAE_4K::shift,9>;
using PDPTE_PAE_4K=PageDirectory<PDE_PAE_4K,PDE_PAE_4K::shift,2>;

// 32-bit 2M PAE Table
using PDE_PAE_2M=PageDirectory<Page32_2M,Page32_2M::shift, 9>;
using PDPTE_PAE_2M=PageDirectory<PDE_PAE_2M,PDE_PAE_2M::shift,2>;

// 64-bit 4k page table
using PTE_64_4K=PageDirectory<Page64_4K,Page64_4K::shift,9>;
using PDE_64_4K=PageDirectory<PTE_64_4K,PTE_64_4K::shift,9>;
using PDPTE_64_4K=PageDirectory<PDE_64_4K,PDE_64_4K::shift,9>;
using PML4E_4K=PageDirectory<PDPTE_64_4K,PDPTE_64_4K::shift,9>;

// 64-bit 2M page table
using PDE_64_2M=PageDirectory<Page64_2M,Page64_2M::shift,9>;
using PDPTE_64_2M=PageDirectory<PDE_64_2M,PDE_64_2M::shift,9>;
using PML4E_2M=PageDirectory<PDPTE_64_2M,PDPTE_64_2M::shift,9>;

// 64-bit 1G page table
using PDPTE_64_1G=PageDirectory<Page64_1G,Page64_1G::shift,9>;
using PML4E_1G=PageDirectory<PDPTE_64_1G,PDPTE_64_1G::shift,9>;

template<>
inline
const char *DirectoryName<39>()
{
	return "PML4E";
}

template<>
inline
const char *DirectoryName<30>()
{
	return "PDPTE";
}

template<>
inline
const char *DirectoryName<21>()
{
	return "PDE";
}

template<>
inline
const char *DirectoryName<12>()
{
	return "PTE";
}



#endif // PAGEDIRECTORY_H_INCLUDED
