#ifndef	_ORANGES_MEMMAN_H_
#define	_ORANGES_MEMMAN_H_
#define MEMSTART	0x00400000
#define MEMEND		0x02000000

#define TEST		0x11223344
#define KSIZE		512u
#define USIZE 	(1024u * 2)
#define CSIZE   (1024u * 128)
#define KB		1024u 
#define MB		(1024u * KB)
#define GB		(1024u * MB)
#define ZONE_SIZE (8 * MB)
#define PAGE_SIZE 4096u
#define PAGE_SHIFT 12
#define MAX_ORDER 12 // 1024 * 8 PAGES
#define ALL_PAGES ((MEMEND)/PAGE_SIZE)
#define size_to_num(x) ((x)*2)
#define phy_to_pfn(x) ((x) / PAGE_SIZE)
#define pfn_to_phy(x) ((x) * PAGE_SIZE)
#define size_to_pages(x) ((x-1)>>PAGE_SHIFT)
#define offset_to_phy(x) (pfn_to_phy(x))
#define pfn_to_page(x)    (mem_map + (x))
#define page_to_pfn(x)   ((x) - mem_map)
#define phy_to_page(x)  (pfn_to_page(phy_to_pfn(x)))
struct MEM{
  u32 pages;
  u32 longest[size_to_num(ALL_PAGES)];
};

struct page{
  u32 order;
  u32 inbuddy;
  u32 count;
  struct page * next;
};

struct free_area {
	struct page * free_list;
	u32		nr_free;
};

struct allocator{
  struct free_area   free_area[MAX_ORDER];
};


void free_pages(struct page *page, u32 order);
void init();
struct page * alloc_pages( u32 order );
u32 do_malloc(u32 size);
u32 do_kmalloc(u32 size);
u32 do_malloc_4k();
u32 do_kmalloc_4k();
void do_free(u32 paddr);

u32 kmalloc(u32 size);
void kfree(u32 laddr);
void sign_block_proc(u32 phy_addr,PROCESS * proc);
void recycle_pages(PHY_BLOCK *page_list);
u32 usign_block_proc(u32 phy_addr,PROCESS * proc);

void infobuddy();
void infoinit();
void set_map_count(struct page * page,u32 num);

extern struct page mem_map[];
#endif 
