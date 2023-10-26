#ifndef _ENTRY_H_
#define _ENTRY_H_

#include <stdbool.h>
#include <stdint.h>


//entrypoint
void start(void);



//provided
uint32_t getMemSz(void);	//in MB
void consoleWrite(char ch);
uint32_t getStoreSz(void);							//in 512-byte blocks
bool readblock(uint32_t blkNo, uint32_t *dst);
bool writeblock(uint32_t blkNo, const uint32_t *src);



#endif
