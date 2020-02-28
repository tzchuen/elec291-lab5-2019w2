SHELL=cmd
CC=c51
COMPORT = $(shell type COMPORT.inc)
OBJS=Load_EFM8LB1.obj

Load_EFM8LB1.hex: $(OBJS)
	$(CC) $(OBJS)
	@echo Done!
	
Load_EFM8LB1.obj: Load_EFM8LB1.c
	$(CC) -c Load_EFM8LB1.c

clean:
	@del $(OBJS) *.asm *.lkr *.lst *.map *.hex *.map 2> nul

LoadFlash:
	@Taskkill /IM putty.exe /F 2>NUL | wait 500
	..\Pro89lp\Pro89lp -p -v Load_EFM8LB1.hex
	cmd /c start c:\PUTTY\putty -serial $(COMPORT) -sercfg 115200,8,n,1,N -v

putty:
	@Taskkill /IM putty.exe /F 2>NUL | wait 500
	cmd /c start c:\PUTTY\putty -serial $(COMPORT) -sercfg 115200,8,n,1,N -v

Dummy: Load_EFM8LB1.hex Load_EFM8LB1.Map
	@echo Nothing to see here!
	
explorer:
	cmd /c start explorer .
		