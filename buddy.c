

#define LEFT_LEAF(index) ((index) * 2 + 1)
#define RIGHT_LEAF(index) ((index) * 2 + 2)
#define PARENT(index) ( ((index) + 1) / 2 - 1)

#define IS_POWER_OF_2(x) (!((x)&((x)-1)))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static u32 fixsize(u32 size) {
  size |= size >> 1;
  size |= size >> 2;
  size |= size >> 4;
  size |= size >> 8;
  size |= size >> 16;
  return size+1;
}

struct page mem_map[ALL_PAGES];
struct allocator s_buddy;
struct allocator * buddy_allocator = &s_buddy;
struct init_mem_info init_info;


static u32
__find_buddy_pfn(u32 page_pfn, u32 order)
{
	return page_pfn ^ (1 << order);
}

static bool 
page_is_buddy(struct page *page, struct page *buddy,u32 order)
{
	if (!buddy->inbuddy)
		return false;

	if (buddy->order != order)
		return false;

	return true;
}

static void 
del_page_from_free_list(struct page *page, struct allocator * allocator,u32 order )
{      
    // list operation
    allocator->free_area[order].free_list = page->next ;
    // page -> next != NULL
    page->next = NULL ;
	page -> inbuddy = false;
	allocator->free_area[order].nr_free--;
}

void
set_map_count(struct page * page,u32 num)
{
    page->count = num;
}

static void 
add_to_free_list(struct page *page, struct allocator *allocator,u32 order )
{   
    
	struct free_area *area = &allocator->free_area[order];
	page->next = NULL;
	if(area->free_list==NULL){
		area->free_list = page;
	}else{
		struct page * node;
		node = area->free_list;
		while (node->next)
		{
			node = node->next;
		}
		node->next = page;
	}
	area->nr_free++;
    set_map_count(page,0);
    
}

static void
set_page_order(struct page *page, u32 order)
{
	page->order = order;
}

static void 
set_buddy_order(struct page *page, u32 order)
{
	set_page_order(page, order);
	page->inbuddy = true;
}

static  void 
buddy_free(struct page *page,u32 pfn,struct allocator * buddy_allocator, u32 order)
{
	u32 buddy_pfn;
	u32 combined_pfn;
	u32 max_order;
	struct page *buddy;

    max_order =  MAX_ORDER - 1;

    if(page->count>1){
        return;
    }


continue_merging:
	while (order < max_order) {

		buddy_pfn = __find_buddy_pfn(pfn, order);
		buddy = page + (buddy_pfn - pfn);

        // until not buddy
		if (!page_is_buddy(page, buddy, order))
			goto done_merging;
		
		del_page_from_free_list(buddy, buddy_allocator, order);

		combined_pfn = buddy_pfn & pfn;     // find head of block by & 
		page = page + (combined_pfn - pfn);
		pfn = combined_pfn;
		order++;
	}
done_merging:
	set_buddy_order(page, order);
	add_to_free_list(page, buddy_allocator, order);
}

// free core
void 
free_pages(struct page *page, u32 order)
{
	u32 pfn = page_to_pfn(page);
	buddy_free( page, pfn, buddy_allocator ,order);
}

static void 
__free_pages_memory(u32 start, u32 end)
{
	int order = MAX_ORDER - 1UL;
	while (start < end) {
        while (start + (1UL << order) > end)
			order--;
        set_map_count(pfn_to_page(start),0);
		free_pages(pfn_to_page(start), order);
		start += (1UL << order);
	}
}
  
static u32  
__free_memory_core(u32 start,u32 end)
{
	u32 start_pfn = phy_to_pfn(start);
	u32 end_pfn = phy_to_pfn(end);

    __free_pages_memory(start_pfn,end_pfn);

	return end_pfn - start_pfn;
}

static void
get_range_mem(u32 *start ,u32 *end)
{
    *start = MEMSTART ;
    *end = MEMEND;
}

static u32 
free_memory(void)
{
	u32 count = 0;
	u32 start, end;
    get_range_mem(&start,&end);
   
	count = __free_memory_core(start, end);
	return count;
}

static u32 
free_all()
{
    u32 pages;
    
    
    pages = free_memory();

    for(int i=0;i<MAX_ORDER;i++){
        init_info.init_order[i] = buddy_allocator->free_area[i].nr_free;
    }
    init_info.total_mem = ((ALL_PAGES/1024)*4); // x MB

    return pages;
}

// init buddy v0.2
void 
init()
{   
    // init allocator
    u32 order;
	for (order = 0; order < MAX_ORDER; order++){
		buddy_allocator->free_area[order].free_list = NULL;
		buddy_allocator->free_area[order].nr_free = 0;
	}
    // free 
    free_all();
}

// alloc
static  void 
expand(struct allocator * allocator, struct page *page, u32 low, u32 high)
{
	u32 size = 1 << high;

	while (high > low) {
		high--;
		size >>= 1;
		add_to_free_list(&page[size], allocator, high);
		set_buddy_order(&page[size], high);
	}
}

static  struct page *
get_page_from_free_area(struct free_area *area)
{   
    struct page * node ;
    //返回该链表第一个块或者返回NULL
    node = area->free_list; // *page
    return node;
}

static 
struct page *
buddy_alloc(struct allocator * allocator, u32 order)
{
	u32 current_order;
	struct free_area *area;
	struct page *page;

	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
		area = &(allocator->free_area[current_order]);
		page = get_page_from_free_area(area);
		if (!page)
			continue;
		del_page_from_free_list(page, allocator, current_order );
		expand(allocator, page, order, current_order);
		return page;
	}

	return NULL;
}

struct page *
alloc_pages( u32 order )
{
	struct page *page;
	page = buddy_alloc(buddy_allocator, order);
	if (page) {
		page->order = order;
        set_map_count(page,1);
		return page;
	}

	return NULL;
}

static 
u32 get_order(u32 size)
{
	u32 order;

	size = (size - 1) >> (PAGE_SHIFT - 1);
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);
	return order;
}

// in kernel can use : 
u32 do_malloc(u32 size)
{ 
    u32 order,phy_addr;
    struct page * page;
    order = get_order(size);
    page = alloc_pages(order);
    phy_addr = page_to_pfn(page);
    phy_addr = pfn_to_phy(phy_addr);
    return phy_addr;
}
	
u32 do_kmalloc(u32 size)
{
  return do_malloc(size);
}

u32 do_malloc_4k()
{
	return do_malloc(4096u);
}

u32 do_kmalloc_4k()
{
	return do_kmalloc(4096u);
}

void do_free(u32 paddr)
{   
    struct page * page;
    paddr = phy_to_pfn(paddr);
    page = pfn_to_page(paddr);
    free_pages(page,page->order);
}

// can use in kernel
u32 kmalloc(u32 size)
{   
    u32 phy_addr = do_malloc(size);
    return K_PHY2LIN(phy_addr);
  
}

void kfree(u32 laddr)
{   
    u32 paddr = K_LIN2PHY(laddr);
    do_free(paddr);
}





