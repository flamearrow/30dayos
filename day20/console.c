#include "bootpack.h"
#include <string.h>
#include <stdio.h>


/* 
	start a task that runs a console 
	a task needs a timer for flashing cursor
	a fifo for transferring int data
	a sheet to display info
*/
void console_task(struct SHEET *sheet, unsigned int memtotal) {
	/* need to sleep the task when it's not active, so we need to get the reference of the task */
	struct TASK *task = task_now();
	struct TIMER *timer;

	int i, fifobuf[128];
	/* fifo belongs to a task, this allows task_a to put data to task_cons's fifo */
	fifo32_init(&task->fifo, 128, fifobuf, task);
	timer = timer_alloc();
	timer_init(timer, &task->fifo, 1);
	timer_settime(timer, 50);
	/* cmdline is used to buffer the info we input in command */
	char cmdline[30];
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;

	/* up to 2880 clusters in a floppy */
	int *fat = (int *)memman_alloc_4k(memman, 4 * 2880);

	/* use a struct to buffer console sheetj and coordinates */
	struct CONSOLE cons;
	cons.sht = sheet;
	cons.cur_x = 8;
	cons.cur_y = 28;
	cons.cur_c = -1;

	/* 
		we assign the address value of cons to address 0x0fec(of the segment), 
		so that naskfunc will know the address of cons and provide it for api
	*/
	*((int *)0x0fec) = (int)&cons;


	/* PS1 */
	cons_putstr0(&cons, "MLGB:>");

	/* fat stores at 0x000200 of floppy, read its info to have fat[] in place */
	file_readfat(fat, (unsigned char*)(ADR_DISKIMG + 0x000200));


	for(;;) {
		io_cli();
		if(fifo32_status(&task->fifo) == 0) {
			/* sleep as much as possible */
			task_sleep(task);
			io_sti();
		} else  {
			i = fifo32_get(&task->fifo);
			io_sti();
			if(i <= 1) {
				/* siwtching data/color */
				if(i != 0) {
					timer_init(timer, &task->fifo, 0);
					if(cons.cur_c >= 0) {
						cons.cur_c = COL8_FFFFFF;
					}
				} else {
					timer_init(timer, &task->fifo, 1);
					if(cons.cur_c >= 0) {
						cons.cur_c = COL8_000000;
					}
				}
				timer_settime(timer, 50);
			}
			/* tab is pressed, now console_task is on focus, should begin flashing its cursor */
			if (i == 2) {
				cons.cur_c = COL8_FFFFFF;
			}
			/* console_task lost focus, erase cursor and stop flashing */
			if (i == 3) {
				boxfill8(sheet->buf, sheet->bxsize, COL8_000000, cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
				cons.cur_c = -1;
			}
			/* keyboard data sent from task_a */
			if(256<=i && i <= 511) {
				/* note i is already transferred to char, (char)i-256 will be the char */
				/* backspace */
				if(i == 256 + 8) {
					/* keep console head (6+1)*8=56 */
					if(cons.cur_x > 56) {
						cons_putchar(&cons, ' ', 0);
						cons.cur_x -= 8;
					}
				} 
				/* enter key, do line break */
				else if( i == 256 + 10 )  {
					/* clear current cursor */
					cons_putchar(&cons, ' ', 0);
					/* 
						finish collecting a command input, add 0 to terminate c style string 
						we add this in case string like 'memmlgb' is also parsed as valid command
					*/
					cmdline[cons.cur_x/8-7] = 0;

					/* do line break */
					cons_newline(&cons);
					/* call different func according to input */
					cons_runcmd(cmdline, &cons, fat, memtotal);

					/* PS1 */
					cons_putstr0(&cons, "MLGB:>");
				} 
				else {
					/* regular chars, append it */
					if(cons.cur_x < 240) {
						/* buffer this char to cmdline, when enter is pressed, check buffered command */
						cmdline[cons.cur_x/8-7] = i-256;
						cons_putchar(&cons, i-256, 1);
					}
				}
			}
			/* redisplay the cursor */
			if(cons.cur_c >= 0) {
				boxfill8(sheet->buf, sheet->bxsize, cons.cur_c, cons.cur_x, cons.cur_y, cons.cur_x+7, cons.cur_y+15);
			}
			sheet_refresh(sheet, cons.cur_x, cons.cur_y, cons.cur_x+8, cons.cur_y+16);
		}
	}
}


void cons_putchar(struct CONSOLE *cons, int chr, char move)
{
	char s[2];
	s[0] = chr;
	s[1] = 0;
	/* tab, will pad spaces until it's multiple of 4 */
	if(s[0] == '\t') {
		/* 31 pixels is 4 letters */
		do {
			putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
			cons->cur_x += 8;
			if(cons->cur_x == 248) {
				cons_newline(cons);
			}
		} while(((cons->cur_x-8) % 32) != 0);
		/* 
			note we can also do the following:
			while(((cursor_x-8) & 31) != 0);
			because all 32's multiplier would have the trailing bits as 0,
			we just apply a trailing number of all 1 can do the trick
		*/
	}
	else if(s[0] == '\n') /* line break, we only break a line for 0x0a, like linux */
	{
		cons_newline(cons);
	} else if(s[0] == 0x0d) /* carrirage return */
	{
		/* nothing */
	} 
	/* regular chars, just print it */
	else {
		putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
		/* when move==0, we don't update cursor_x, this is used to wipe out cursor or backspace */
		if(move!=0) {
			/* linebreak */
			cons->cur_x += 8;
			if(cons->cur_x == 248) {
				cons_newline(cons);
			}	
		}
		
	}
	return;
}

void cons_putstr0(struct CONSOLE *cons, char *s) {
	for(; *s != 0; s++) {
		cons_putchar(cons, *s, 1);
	}
	return;
}

void cons_putstr1(struct CONSOLE *cons, char *s, int l) {
	int i;
	for(i = 0; i < l; i++) {
		cons_putchar(cons, s[i], 1);
	}
	return;
}

/* 
	start a new line, append/scroll when necessary 
	eventually each command is also appending line by line 
	use this method whenever a line break is required
*/
void cons_newline(struct CONSOLE *cons) {
	int x, y;
	struct SHEET *sheet = cons->sht;
	/* if we're note at bottom line, add a new line */
	if( cons->cur_y < 28 + 112 ) {
		cons->cur_y += 16;
	} 
	/* otherwise offset line (2:n) to line (1:n-1) and draw new line at bottom */
	else {
		for(y = 28; y < 28 + 112; y++) {
			for(x = 8; x < 248; x++) {
				sheet->buf[x + y*sheet->bxsize] = sheet->buf[x + (y+16) * sheet->bxsize];
			}
		}
		/* clear the last line */
		for(y = 28 + 112; y < 28 + 128; y++) {
			for(x = 8; x < 248; x++) {
				sheet->buf[x + y*sheet->bxsize] = COL8_000000;
			}
		}
		sheet_refresh(sheet, 8, 28, 8+240, 28+128);
	}
	cons->cur_x = 8;
	return;
}

void cons_runcmd(char *cmdline, struct CONSOLE *cons, int *fat, unsigned int memtotal)
{
	/* if input is 'mem' then execute the command */
	if(strcmp(cmdline, "mem") == 0) {
		cmd_mem(cons, memtotal);
	}
	/* clean entire screen! */
	else if(strcmp(cmdline, "cls") == 0) {
		cmd_cls(cons);
	}
	/* display info of file that crunched with the img */
	else if(strcmp(cmdline, "dir") == 0) {
		cmd_dir(cons);
	}
	/* 
		`type blah` == `cat blah` 
		strncmp(ml, gb, num): only compare first number letters of ml and gb
	*/
	else if(strncmp(cmdline, "type ", 5) == 0) {
		cmd_type(cons, fat, cmdline);
	}
	/* otherwise if it's not a blank line, check if it's a file name and try to execute*/
	else if(cmdline[0] != 0) {
		/* it's not a file to execute */
		if(cmd_app(cons, fat, cmdline) == 0) {
			cons_putstr0(cons, "MLGB! Invalid Command!\n\n");
		}
	}
	return;
}

void cmd_mem(struct CONSOLE *cons, unsigned int memtotal) {
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	char s[30];
	/* memtotal is in byte */
	sprintf(s, "total %dMB\n", memtotal/1024/1024);
	cons_putstr0(cons, s);
	sprintf(s, "free %dKB\n", memman_total(memman)/1024);
	cons_putstr0(cons, s);
	/* add an additional line break*/
	cons_newline(cons);
	return;
}

void cmd_cls(struct CONSOLE *cons) {
	int x, y;
	struct SHEET *sheet = cons->sht;
	for(y = 28; y < 28 + 128; y++) {
		for(x = 8; x < 240; x++) {
			sheet->buf[x + y*sheet->bxsize] = COL8_000000;
		}
	}
	sheet_refresh(sheet, 8, 28, 8+240, 28+128);
	cons->cur_y = 28;
	return;
}

void cmd_dir(struct CONSOLE *cons) {
	/* from 0x002600 of the img addr, file info are stored, when copied to disk, we need to offset disk starting address */
	struct FILEINFO *finfo = (struct FILEINFO *)(ADR_DISKIMG + 0x002600);
	char s[30];
	int x, y;
	/* can crunch up to 224 files with img */
	for(x = 0; x < 224; x++) {
		/* 0x00 is empty file */
		if(finfo[x].name[0] == 0x00) {
			break;
		}
		/* 
			if it's not deleted, then it's a valid file/dir 
			note if it's not empty then it will be parsed as a valid character
		*/
		if(finfo[x].name[0] != 0xe5) {
			/* if it's not dir or non-file info( if it's a file) */
			if((finfo[x].type & 0x18) == 0) {
				sprintf(s, "filename.ext  %7d\n", finfo[x].size);
				/* since file name and ext length are fixed, we can replace them in s afterwards */
				for(y = 0; y < 8; y++) {
					s[y] = finfo[x].name[y];
				}
				for(y = 9; y < 12; y++) {
					s[y] = finfo[x].ext[y-9];
				}
				cons_putstr0(cons, s);
			}
		}
	}
	cons_newline(cons);
	return;
}

void cmd_type(struct CONSOLE *cons, int *fat, char *cmdline) {
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	/* note we only pass the file name to be searched, which would be cmdline+5 */
	struct FILEINFO *finfo = file_search(cmdline + 5, (struct FILEINFO *)(ADR_DISKIMG + 0x002600), 224);
	char *p;
	int i;
	/* found the file */
	if(finfo != 0) {
		/* 
			since file is not consequtively sotred, we first alloc a chunk of memory, copy the file content there
			then read from that chunk of memory byte by byte
			finally we free that chunk of data
		*/							
		p = (char *)memman_alloc_4k(memman, finfo->size);
		/* add ADR_DISKIMG to convert the address of flopy to the address of disk */
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *)0x003e00 + ADR_DISKIMG);
		for(i = 0; i < finfo->size; i++) {
			cons_putchar(cons, p[i], 1);
		}
		/* finish reading the file, free the memory */
		memman_free(memman, (int) p, finfo->size);
	}
	/* file not found */
	else  {
		putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
		cons_newline(cons);
	}
	cons_newline(cons);
	return;
}

/* try to find a file(application) with given name and execute it, if not found return 0 */
int cmd_app(struct CONSOLE *cons, int *fat, char *cmdline) {
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;	
	/* we need to use gdt to start an application */
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
	char name[18], *p;
	int i;
	int dot = 0;
	for(i = 0; i < 13; i++) {
		if(cmdline[i] <= ' ') {
			break;
		}
		if(cmdline[i] == '.') {
			dot = 1;
		}
		name[i] = cmdline[i];
	}
	/* replace all command into the form xxx.hrb */
	if(!dot) {
		name[i] = '.';
		name[i+1] = 'H';
		name[i+2] = 'R';
		name[i+3] = 'B';
		name[i+4] = 0;
	} else {
		name[i] = 0;
	}

	struct FILEINFO *finfo = file_search(name, (struct FILEINFO *)(ADR_DISKIMG + 0x002600), 224);


	/* found the file */
	if(finfo != 0) {
		/* 
			since file is not consequtively sotred, we first alloc a chunk of memory, copy the file content there
			then read from that chunk of memory byte by byte
			finally we free that chunk of data
		*/							
		p = (char *)memman_alloc_4k(memman, finfo->size);
		/* add ADR_DISKIMG to convert the address of flopy to the address of disk */
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *)0x003e00 + ADR_DISKIMG);
		
		/* 
			note: the new application shouldn't occupy any task(1-1000), 
			we assign it 1003 to bypass taskmgr
			this application still belongs to console_task
			we jump to 0 of segment 1003 here and execute instructions there
			*p would point to the memory where the byte code of hlt.nas is stored,
				therefore we will execute hlt.nas by calling farcall
			since when doing farcall current address is pushed, 
				we can use RETF to return from hlt.nas to here
		*/

		/*
			invocation sequences:
				console.c::cmd_app()(segment 2) 
					--> (far call)
				hlt.nas(segment 1003)
					-*> (system interruption)
				naskfunc.nas::_asm_cons_putchar()(segment 2) 
					-> (call)
				console.c::cons_putchar()(segment 2)
					<- (ret)
				naskfunc.nas::_asm_cons_putchar()(segment 2)
					<*- (IRETD)
				hlt.nas(segment 1003)
					<-- (retf)
				console.c::cmd_app()(segment 2) 
		*/
		set_segmdesc(gdt + 1003, finfo->size - 1, (int)p, AR_CODE32_ER);
		farcall(0, 1003 * 8);
		memman_free(memman, (int) p, finfo->size);
		cons_newline(cons);
		return 1;
	}
	/* file not found */
	return 0;
}

/* 
	system api
	hanlding INT 0x40 call, when an application call INT 0x40, will swich handling according to edx value 
*/
void hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax) {
	/* we will assign the address of cons to 0x0fec(arbitrarily) */
	struct CONSOLE *cons = (struct CONSOLE *) *((int*)0x0fec);
	if (edx == 1) {
		/* no.1: putchar, need to MOV the char to EAX */
		cons_putchar(cons, eax & 0xff, 1);
	} else if (edx == 2) {
		/* no.2 cons_putstr0(struct CONSOLE *cons, char *s);
			 need to MOV the address of str to EBX */
		cons_putstr0(cons, (char *)ebx);
	} else if (edx == 3) {
		/* no.3 cons_putstr1(struct CONSOLE *cons, char *s, int l);
			 need to MOV the address of str to EBX
			 and length to ECX
		*/
		cons_putstr1(cons, (char *)ebx, ecx);
	}
}
