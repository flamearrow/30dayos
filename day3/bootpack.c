/* declare there's another function defined in other place (naskfunc.obj) */

void io_hlt(void);


void HariMain(void)
{

fin:
	io_hlt(); /* これでnaskfunc.nasの_io_hltが実行されます */
	goto fin;

}
