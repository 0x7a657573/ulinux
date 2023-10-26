/*
	(c) 2021 Dmitry Grinberg   https://dmitry.gr
	Non-commercial use only OR licensing@dmitry.gr
*/

#include <stdio.h>
#include "mem.h"

typedef struct {

	uint32_t pa;
	uint32_t sz;
	MemAccessF aF;

} MemRegion;

struct {

	MemRegion regions[MAX_MEM_REGIONS];

} gMem = { };


bool memRegionAdd(uint32_t pa, uint32_t sz, MemAccessF aF){

	uint8_t i;
	
	//check for intersection with another region
	
	for(i = 0; i < MAX_MEM_REGIONS; i++){
		
		if(!gMem.regions[i].sz) continue;
		if((gMem.regions[i].pa <= pa && gMem.regions[i].pa + gMem.regions[i].sz > pa) || (pa <= gMem.regions[i].pa && pa + sz > gMem.regions[i].pa)){
		
			return false;		//intersection -> fail
		}
	}
	
	
	//find a free region and put it there
	
	for(i = 0; i < MAX_MEM_REGIONS; i++){
		if(gMem.regions[i].sz == 0){
		
			gMem.regions[i].pa = pa;
			gMem.regions[i].sz = sz;
			gMem.regions[i].aF = aF;
		
			return true;
		}
	}
	
	
	//fail miserably
	
	return false;	
}

bool memRegionDel(uint32_t pa, uint32_t sz){

	uint8_t i;
	
	for(i = 0; i < MAX_MEM_REGIONS; i++){
		if(gMem.regions[i].pa == pa && gMem.regions[i].sz ==sz){
		
			gMem.regions[i].sz = 0;
			return true;
		}
	}
	
	return false;
}

bool memAccess(uint32_t addr, uint_fast8_t size, bool write, void* buf){
	
	uint_fast8_t i;

	//pr("mem %c of %ub @ 0x%08X\r\n", write ? 'W' : 'R', size, addr);

	for(i = 0; i < MAX_MEM_REGIONS; i++){
		if(gMem.regions[i].pa <= addr && gMem.regions[i].pa + gMem.regions[i].sz > addr){
		
			return gMem.regions[i].aF(addr, size, write & 0x7F, buf);
		}
	}
	
	err_str("\nMemory %s of %u bytes at physical addr 0x%08x fails\r\n", (write & 0x7F) ? "write" : "read", size, (unsigned)addr);
	
	return false;
}

