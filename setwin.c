
#include <cmoc.h>
#define MODULE_NAME 	"SetWin"
#define VERSION_STRING 	"0.9"

#define 	F$Fork 			0x03  	// Syscall to create/run a child process
#define 	F$Wait  			0x04  	// Syscall that temporarily turns off a calling process
#define 	F$Exit 			0x06  	// Syscall that terminates the calling process
#define 	F$ID 			0x0C  	// Syscall returns the Process ID and User ID of calling program
#define	F$GPrDsc			0x18  	// Syscall retrieves 512-byte Process Descriptor of a process
#define 	F$Sleep 			0x0A  	// Syscall sleeps X amount of "ticks"
#define 	I$Open  			0x84  	// Syscall opens a path to existing file or device
#define 	I$Write 			0x8A  	// Syscall for writing bytes to a Path
#define 	I$GetStt 			0x8D  	// Syscall for performing a GetStat which returns status info of a file or device

#define 	P$SelP 			0xAC   	// Offset for byte within a Process Descriptor that contains Path Number for currently select CoCo window

#define 	SS_DevNm  		0x0E 	// GetStat for returning a device's name
#define 	SS_ScSiz 			0x26  	// GetStat for returning Screen Size of a window
#define 	SS_ScTyp  		0x93 	// GetStat for returning Screen Type of a window
#define 	SS_FBRgs 			0x96  	// GetStat for returning current Foreground, Background, and Border colors of a window

#define 	MODE_READ 		0x01
#define 	MODE_WRITE  		0x02

#define 	TYPE_OFFSET 		0x04
#define 	WIDTH_OFFSET 		0x07
#define 	HEIGHT_OFFSET		0x08
#define 	FORECOLOR_OFFSET	0x09
#define 	BACKCOLOR_OFFSET	0x0A
#define 	BORDER_OFFSET  	0x0B

int getParamNumber(char*);
unsigned char getCurWindowPath(void);
unsigned char sendToWindowPath(unsigned char, unsigned char*, unsigned int);
unsigned char sleepTicks(unsigned int);
unsigned char getStat(unsigned char, unsigned char);
unsigned char getNewWindowPath(void);
unsigned char forkShell(unsigned char*);

struct 
{
	unsigned char Reg_A;
	unsigned char Reg_B;
	unsigned int  Reg_X;
	unsigned int  Reg_Y;
	unsigned int  Reg_U;

} cpuReturnRegs;

unsigned char winDevPathName[32];

int main(int argc, char *argv[])
{
	char winTypeFlag = ''; 
	char newWindowFlag = '';		// New Window flag defaults to empty, meaning "do NOT create a new window for this"
	int windowWidth = -1, windowHeight = -1, colorDepth = -1, windowType = -1;
	int foregroundColor = -1, backgroundColor = -1, borderColor = -1;  	// Colors all default to -1 which represents "undefined"
	int curWinColorDepth, curWinTypeFlag;

	unsigned char paramChar, curWinPath, targetWinPath;
	int index = 1;
	int paramValue, paramIndex = 0;
	unsigned char setCurWinCodes[] = { 0x1B, 0x24, 0x1B, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	unsigned char selectWinCodes[] = { 0x1B, 0x21 };

	if (argv[1] == NULL)
		{
			printf("%s v%s Written by Todd Wallace\n", MODULE_NAME, VERSION_STRING);
			printf("\nUsage:\n  %s [-n] [-t | -g] [-cX] [Width] [Height] [FGColor] [BGColor] [BColor]\n\n", MODULE_NAME);
			printf("Flag Definitions:\n");
			printf("  -n = Creates a new window instead of modifying the existing one.\n");
			printf("  -t = Specifies a text-only window type\n");
			printf("  -g = Specifies a graphics text window type\n");
			printf("  -c = Specifies X number of colors for a graphics text window (2, 4, or 16)\n\n");

			printf("  FGColor = Text foreground color\n");
			printf("  BGColor = Screen background color\n");
			printf("  BColor  = Screen border color\n\n");

			printf("Note: If any of the above parameters are omitted in the command,\n");
			printf("%s will attempt to extract the missing parameters from the CURRENT\n", MODULE_NAME);
			printf("window instead. This feature should make the process easier for when you only\n");
			printf("want to change 1 or 2 characteristics of an already defined window.\n");
			return 0;
		}

	// Parse all the command-line arguments before attempt to make syscalls
	while (index < argc)
	{
		if (argv[index][0] == '-')
		{
			paramChar = toupper(argv[index][1]);
			if ((paramChar == 'T') || (paramChar == 'G'))
				winTypeFlag = paramChar;
			else if (paramChar == 'C')
			{
				paramValue = getParamNumber(argv[index] + 2);
				if (paramValue == -1)
				{
					printf("%s: Invalid parameter (%04X). Expected a number. Aborting.\n", MODULE_NAME, paramValue);
					return 0;
				}
				else if ((paramValue == 2) || (paramValue == 4) || (paramValue == 16))
					colorDepth = paramValue;
				else
				{
					printf("%s: Invalid number of colors. (Must be 2, 4, or 16). Aborted.\n", MODULE_NAME);
					return 0;
				}	
			}
			else if ((paramChar == 'N') && (argv[index][2] == ''))
				newWindowFlag = paramChar;
			else
			{
				printf("%s: Invalid flag 2. Aborted.\n", MODULE_NAME);
				return 0;
			}		
		}
		else
		{
			paramValue = getParamNumber(argv[index]);
			if (paramValue == -1)
			{
				printf("Invalid parameter. Aborting.\n");
				return 0;
			}
			else
			{
				switch (paramIndex)
				{
				case 0:
					windowWidth = paramValue;
					break;
				case 1:
					windowHeight = paramValue;
					break;
				case 2:
					foregroundColor = paramValue;
					break;
				case 3:
					backgroundColor = paramValue;
					break;
				case 4:
					borderColor = paramValue;
					break;
				}
				paramIndex++;
			}
		}

		/*
		// Are we parsing the 2nd to last parameter? This SHOULD be the specified window width.
		else if (index == (argc - 2))
		{
			paramValue = getParamNumber(argv[index]);
			if ((paramValue == -1) || (paramValue > 80))
			{
				printf("setwin: Invalid window width (Valid widths are 40 or 80). Aborted.\n");
				return 0;
			}
			windowWidth = paramValue;
		}
		else if (index == (argc - 1))
		{
			paramValue = getParamNumber(argv[index]);
			if ((paramValue == -1) || (paramValue > 25))
			{
				printf("setwin: Invalid window height. Aborted.\n");
				return 0;
			}
			windowHeight = paramValue;
		}
		else
		{
			printf("Arg1 = %s\n", argv[(argc - 2)]);
			printf("Arg2 = %s\n", argv[(argc - 1)]);
			printf("setwin: Invalid command syntax.\n");
			return 0;
		}
		*/
		index++;
	}

	if ((windowWidth == -1) || (windowHeight == -1))
	{
		printf("Missing parameter. Aborting.\n");
		return 0;
	}

	/*
	printf("Mode = %c, Colors = %u, Width = %u, Height = %u", winTypeFlag, colorDepth, windowWidth, windowHeight);
	if (foregroundColor != -1)
		printf(", Fcolor = %u", foregroundColor);
	if (backgroundColor != -1)
		printf(", Bcolor = %u", backgroundColor);
	if (borderColor != -1)
		printf(", Border = %u", borderColor);
	printf("\n");
	*/

	// Ok, if here, we SHOULD have all the required (and optional) parameters parsed and set
	curWinPath = getCurWindowPath();

	getStat(curWinPath, SS_ScSiz);
	// If user hasnt specified a parameter, use current window's parameters
	if (windowWidth == -1)
		windowWidth = cpuReturnRegs.Reg_X;
	if (windowHeight == -1)
		windowHeight = cpuReturnRegs.Reg_Y;

	getStat(curWinPath, SS_FBRgs);
	if (foregroundColor == -1)
		foregroundColor = cpuReturnRegs.Reg_A;
	if (backgroundColor == -1)
		backgroundColor = cpuReturnRegs.Reg_B;
	if (borderColor == -1)
		borderColor = cpuReturnRegs.Reg_X;

	getStat(curWinPath, SS_ScTyp);
	switch (cpuReturnRegs.Reg_A)
	{
	case 1:
	case 2:
		curWinTypeFlag = 'T';
		break;
	case 5:
		curWinTypeFlag = 'G';
		curWinColorDepth = 2;
		break;
	case 6:
		curWinTypeFlag = 'G';
		curWinColorDepth = 4;
		break;
	case 7:
		curWinTypeFlag = 'G';
		curWinColorDepth = 4;
		break;
	case 8:
		curWinTypeFlag = 'G';
		curWinColorDepth = 16;
		break;
	}
	if (winTypeFlag == '')
		winTypeFlag = curWinTypeFlag;
	if ((winTypeFlag == 'G') && (colorDepth == -1))
		colorDepth = curWinColorDepth;

	if (newWindowFlag == 'N')
	{
		targetWinPath = getNewWindowPath();
		if (targetWinPath >= 0x80)
		{
			printf("Could not open a new window. Aborting\n");
			return 0;
		}
	}
	else
		targetWinPath = curWinPath;

	// Finally, use all the parameters that we either parsed from the user OR we extracted from the current window's params
	// to build our DWSet code sequence
	if (winTypeFlag == 'T')
	{
		if (windowWidth > 40)
			setCurWinCodes[TYPE_OFFSET] = 0x02;
		else
			setCurWinCodes[TYPE_OFFSET] = 0x01;
	}
	else
	{
		if ((windowWidth > 40) && (colorDepth == 2))
			setCurWinCodes[TYPE_OFFSET] = 0x05;
		else if ((windowWidth > 40) && (colorDepth == 4))
			setCurWinCodes[TYPE_OFFSET] = 0x07;
		else if ((windowWidth <= 40) && (colorDepth == 4))
			setCurWinCodes[TYPE_OFFSET] = 0x06;
		else if ((windowWidth <= 40) && (colorDepth == 16))
			setCurWinCodes[TYPE_OFFSET] = 0x08;
		else
		{
			printf("Unsupported parameter combination.\n");
			return 0;
		}
	}
	setCurWinCodes[WIDTH_OFFSET] = windowWidth;
	setCurWinCodes[HEIGHT_OFFSET] = windowHeight;
	setCurWinCodes[FORECOLOR_OFFSET] = foregroundColor;
	setCurWinCodes[BACKCOLOR_OFFSET] = backgroundColor;
	setCurWinCodes[BORDER_OFFSET] = borderColor;

	if (newWindowFlag == 'N')
	{
		sendToWindowPath(targetWinPath, setCurWinCodes + 2, sizeof(setCurWinCodes) - 2); 	// Skip the two-byte DWEnd command
		sendToWindowPath(targetWinPath, selectWinCodes, sizeof(selectWinCodes));
		forkShell(winDevPathName);
		sleepTicks(30);
	}
	else
	{
		sendToWindowPath(1, selectWinCodes, sizeof(selectWinCodes));
		sendToWindowPath(targetWinPath, setCurWinCodes, sizeof(setCurWinCodes));
		sendToWindowPath(targetWinPath, selectWinCodes, sizeof(selectWinCodes));
		forkShell(winDevPathName);
	}

	return 0;
}

int getParamNumber(char* inputString)
{
	int tempValue;
	tempValue = (*inputString) - 0x30;		// First, try to validate that the next argument is a number (not a 100% reliable method tho)
	// If it's not a number, return error code. Otherwise, convert ascii number to integer and return it
	if ((tempValue < 0) || (tempValue > 9))
		return -1;
	else
		return atoi(inputString);
}

unsigned char getCurWindowPath(void)
{
	unsigned char pathNum, errorCode
;	unsigned char processDesc[512];
	unsigned int tempWord = 0;

	asm
	{
		tfr 	PC,D
		std 	:tempWord
		std  :processDesc
		os9	F$ID 	// Get the current Process's ID
		leax	:processDesc
		os9 	F$GPrDsc 	// Use Process ID to get the current window's Process Descriptor
		bcs 	error
		// Now extract the current window's path number from the Process Descriptor
		lda 	P$SelP,X
		sta 	:pathNum

		clrb
	error:
		stb 	:errorCode
	}

	if (errorCode != 0)
		return errorCode;
	else
		return pathNum;
}

unsigned char sendToWindowPath(unsigned char pathNum, unsigned char* buffer, unsigned int bufferSize)
{
	unsigned char errorCode;
	unsigned int bytesSent;

	asm
	{
		lda	:pathNum
		ldx 	:buffer
		pshs Y
		ldy 	:bufferSize
		os9 	I$Write
		tfr 	Y,X
		puls	Y
		bcs  writeError

		stx 	:bytesSent

		clrb
	writeError:
		stb 	:errorCode
	}
/*
	printf("Sent %u bytes to path %02X = ", bytesSent, pathNum);
	for (unsigned int i = 0; i < bytesSent; i++)
		printf("%02X ", *(buffer + i));
	printf("\n");
*/
	if (errorCode != 0)
		printf("Error sending to window path.\n");
	return errorCode;
}

unsigned char sleepTicks(unsigned int ticksToSleep)
{
	unsigned char errorCode;

	asm
	{
		ldx 	:ticksToSleep
		os9 	F$Sleep
		bcs 	SLEEP_ERROR

		clrb
	SLEEP_ERROR:
		stb 	:errorCode
	}
	return errorCode;
}

unsigned char getStat(unsigned char pathNum, unsigned char functionCode)
{
	unsigned char errorCode;

	asm
	{
		pshs	U,Y
		lda 	:pathNum
		ldb 	:functionCode
		os9 	I$GetStt

		pshs	U,Y  	// Temporarily store the results in these register on stack since CMOC needs their original values as pointers
		ldy 	4,S  	// Retrieve CMOC's pointer we saved at the beginning from the stack (skipping passed our saved results)
		ldu 	6,S  	// Retrieve CMOC's other pointer we saved at the beginning from the stack (skipping passed our saved results)
		
		sta 	:cpuReturnRegs.Reg_A
		stb 	:cpuReturnRegs.Reg_B
		stx 	:cpuReturnRegs.Reg_X
		ldd 	,S  		// Grab our Y register result from GetStat from stack via D so we don't clobber CMOC pointers in Y and U
		std 	:cpuReturnRegs.Reg_Y
		ldd 	2,S 		// Grab our U register result from GetStat from stack via D so we don't clobber CMOC pointers in Y and U
		std 	:cpuReturnRegs.Reg_U

		leas	8,S  	// Skip over the pushed bytes we needed for our Register-"Musical Chairs" shenanigans since they are no longer needed
		bcs 	GETSTAT_ERROR
		clrb
		bra 	GETSTAT_EXIT

	GETSTAT_ERROR:
		ldb 	:cpuReturnRegs.Reg_B
	GETSTAT_EXIT:
		stb 	:errorCode
	}

	if (errorCode != 0)
		printf("Error performing GetStat.\n");
	return errorCode;
}

unsigned char getNewWindowPath(void)
{
	unsigned char errorCode, newPathNum;
	unsigned char accessMode = (MODE_READ | MODE_WRITE);
	const char *newPathName = "/W\r";

	asm
	{
		lda	:accessMode
		ldx 	:newPathName
		os9 	I$Open
		bcs  newWinPathError
		sta 	:newPathNum

		ldb 	#SS_DevNm
		leax	:winDevPathName
		os9 	I$GetStt
		bcs 	newWinPathError

		clrb
	newWinPathError:
		stb 	:errorCode
	}

	if (errorCode != 0)
	{
		printf("Error getting new window path.\n");
		return errorCode;
	}
	else
	{
		// Search return buffer for "high bit"-terminated character, mask off the high bit and add CR + NULL
		for (int i = 0; i < 32; i++)
			if (winDevPathName[i] >= 0x80)
			{
				winDevPathName[i] = winDevPathName[i] & 0x7F;
				winDevPathName[i+1] = 0x00;
				break;
			}
		//printf("dev name = %s\n", winDevPathName);
		return newPathNum;
	}
}

unsigned char forkShell(unsigned char* destDevPathName)
{
	unsigned char errorCode;
	const char *commandShell = "shell\r";
	char commandParam[10];
	sprintf(commandParam, "i=/%s&\r", destDevPathName);
	unsigned int paramLength = strlen((const char*)commandParam);

	asm
	{
		pshs 	U,Y	
		ldx 		:commandShell
		leay  	:commandParam
		tfr  	Y,D
		ldy 		:paramLength
		tfr  	D,U
		ldd  	#0
		os9  	F$Fork
		bcs  	forkError

		clrb
	forkError:
		puls  	Y,U
		stb  	:errorCode
	}

	if (errorCode != 0)
		printf("Error forking shell command.\n");
	/*
	else
	{
		printf("Fork string sent = ");
		for (unsigned int i = 0; i < paramLength; i++)
			printf("%02X ", commandParam[i]);
		printf("\n");
	}
	*/
	return errorCode;
}