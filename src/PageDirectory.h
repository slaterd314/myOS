#ifndef PAGEDIRECTORY_H_INCLUDED
#define PAGEDIRECTORY_H_INCLUDED

#include "kmalloc.h"
#include "PageT.h"

template<typename T, const int SHIFT, const int BITS>
struct PageDirectory
{
	using PageType=typename T::PageType;
	using  Pointer=typename PageType::Pointer;

	static constexpr Pointer MASK = ((1<<BITS)-1);
	static constexpr decltype(sizeof(int)) NUM_ENTRIES = 4096 / sizeof(Pointer);

	Pointer physical[NUM_ENTRIES];
//	Pointer tables[NUM_ENTRIES];
//	Pointer physicalAddr;


	PageDirectory()
	{
//		memset(tables, '\0', sizeof(tables));
		memset(physical, '\0', sizeof(physical));
//		physicalAddr = 0;
		// printf("PageDirectory<,%d,%d> ctor, this == 0x%08.8x\n", SHIFT,BITS, (uint32_t)this);
	}

	void setPhys(Pointer p)
	{
		// printf("setPhys: this: 0x%08.8x, phys: 0x%08.8x\n", (uint32_t)this, (uint32_t)p);
//		physicalAddr = p;
	}

	Pointer index(Pointer vaddr)const
	{
		return ((vaddr>>(SHIFT)) & MASK);
	}

	T *operator[](int n)const
	{
		const Pointer MASK = (~((Pointer)(0xFFF)));
		return reinterpret_cast<T *>(  MASK & physical[n] );
	}

	PageType *getPage(Pointer vaddr)
	{
		auto i = index(vaddr);
		auto *table = (*this)[i];
		if( table == nullptr)
		{
			uint32_t phys = 0;

			table = new(reinterpret_cast<void *>(kmalloc_aligned_phys(sizeof(T), &phys))) T{};
//			table->setPhys(phys);
//			tables[i] = reinterpret_cast<Pointer>(table);
			physical[i] = static_cast<Pointer>(phys|0x03); // PRESENT, RW, US.
		}
		return table->getPage(vaddr);
	}

	void dump()const
	{
#if 0		
		printf32("PageDirectory: this 0x%08.8x, phys 0x%08.8x\n", (uint32_t)this ,(uint32_t)physicalAddr);
		for (auto i = 0u; i < NUM_ENTRIES; ++i)
		{
			printf32("entry: %d == 0x%08.8x (0x%08.8x)\n", i, (uint32_t)tables[i], (uint32_t)physical[i]);
			if (tables[i])
			{
				reinterpret_cast<T *>(tables[i])->dump();
			}
		}
		printf32("===========================================\n");
#endif // 0		
	}
};

template<class UINT>
class PageTableT
{
public:

	static constexpr decltype(sizeof(int)) NUM_PAGES = 4096 / sizeof(UINT);

	PageTableT()
	{
		memset(pages, '\0', sizeof(pages));
	}

	using PageType=PageT<UINT>;

	PageType *getPage(UINT vaddr)
	{
		return &(pages[( (vaddr >> 12) & 0xFFF)]);
	}

	void setPhys(uint32_t){}

	UINT operator[](int n)
	{
		return *(UINT *)(&pages[n]);
	}

	void dump()
	{
		for (auto i = 0u; i < NUM_PAGES; ++i)
		{
			printf32("\tpage: %d == 0x%08.8x\n", i, *(uint32_t *)&(pages[i]) );
		}
		printf32("\t+++++++++++++++++++++++\n");
	}


private:
	PageType pages[NUM_PAGES];

};

#endif // PAGEDIRECTORY_H_INCLUDED
