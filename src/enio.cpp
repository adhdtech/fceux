/* FCE Ultra - NES/Famicom Emulator
*
* Copyright notice for this file:
*  Copyright (C) 2002 Xodnizel
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
//#include "x6502.h"
#include "fceu.h"
#include "enio.h"
//#include "sound.h"
//#include "file.h"
//#include "utils/md5.h"
//#include "utils/memory.h"
//#include "state.h"
//#include "file.h"
//#include "cart.h"
//#include "netplay.h"
//#include "driver.h"
//#include "movie.h"

//  TODO:  BLAH

//static DECLFR(ENIONESGetByte)
//{
	// ENIO NES Read ISR
//	return MyENIO.NESGetByte(&MyENIO);
//}

//static DECLFW(ENIONESPutByte)
//{
	// ENIO NES Write ISR
//	MyENIO.NESPutByte(&MyENIO, V);
//}

CircularBuffer* CircularBufferInit(CircularBuffer** pQue, int size)
{
        int sz = size+sizeof(CircularBuffer);
        *pQue = (CircularBuffer*) malloc(sz);
        if(*pQue)
        {
            (*pQue)->size=size;
            (*pQue)->writePointer = 0;
            (*pQue)->readPointer  = 0;
            (*pQue)->pendingCmds  = 0;
            (*pQue)->lastCmdToGo  = 0;
            (*pQue)->curCmdToGo   = 0;
            (*pQue)->pendingBytes = 0;
			(*pQue)->keys = (uint8*)(*pQue)+sizeof(CircularBuffer);
        }

        return *pQue;
}

int CircularBufferIsFull(CircularBuffer* que)
{
     return ((que->writePointer + 1) % que->size == que->readPointer);
}

int CircularBufferIsEmpty(CircularBuffer* que)
{
     return ((que->readPointer) % que->size == que->writePointer);
}

int CircularBufferEnque(CircularBuffer* que, uint8 k)
{
//		printf("\n\t\t\tCBE    Que 0x%08x",que);
//		printf("\n\t\t\t\tKeys Start: 0x%08x, Write Ptr: 0x%08x",que->keys,que->writePointer);
        int isFull = CircularBufferIsFull(que);
        que->keys[que->writePointer] = k;
        que->writePointer++;
        que->writePointer %= que->size;
        que->pendingBytes++;
        return isFull;
}

int CircularBufferDeque(CircularBuffer* que, uint8* pK)
{
        int isEmpty =  CircularBufferIsEmpty(que);
        *pK = que->keys[que->readPointer];
        que->readPointer++;
        que->readPointer %= que->size;
        que->pendingBytes--;
        return(isEmpty);
}

// The NESGetByte function is analogous to the send ISR executed by the PIC32
uint8 ENIOModule::NESGetByte (ENIOModule* ENIO)
{
    uint8 rtnVal, nextVal;
    //printf("Called function ENIO::NESGetByte, declaring PutChan = &ENIO->Channels[%d]\n", ENIO->ChanOut);

    CGPChannel *PutChan = &ENIO->Channels[ENIO->ChanOut];

    rtnVal = ENIO->CPLDOutByte;

//	printf("\t[O] x%02x\n", rtnVal);

    if (PutChan->queOut->pendingBytes) {
        //printf("Dequeing byte from Channel %d\n", ENIO->ChanOut);
        CircularBufferDeque(PutChan->queOut, &nextVal);
        PutChan->LastByteOutEmpty = 0;
        //printf("Next byte - %d\r\n", ENIO->CPLDOutByte);
    } else {
        //printf("No data ready!\n");
        nextVal = 0;
        PutChan->LastByteOutEmpty = 1;
    }
    ENIO->CPLDOutByte = nextVal;
    PutChan->LastByteOut = nextVal;
//	if ((PutChan->LastByteOutEmpty == 0) && (ENIO->ChanOut == 7)) {
//		printf(" 0x%02x", nextVal);
//		printf("[%d] queOut %d/%d\n", ENIO->ChanOut, PutChan->queOut->pendingBytes, PutChan->queOut->size);
//	}
    //printf("'%c' 0x%02x\n", rtnVal, rtnVal);
    //printf("\t\t<ENIO::NESGetByte> -> 0x%02x, 0x%02x, 0x%02x\n", rtnVal, ENIO->CPLDOutByte, PutChan->LastByteOut);
    return rtnVal;
}

// The ENIO::NESPutByte function is analogous to the receive ISR executed by the PIC32
uint8 ENIOModule::NESPutByte (ENIOModule* ENIO, uint8 ByteIn)
{
    CGPChannel *GetChan;

    ENIO->CPLDInByte = ByteIn;
//	printf("[I] x%02x\n", ByteIn);

    // First byte should be the Channel ID
    if (! ENIO->RecvChannelID) {
        ENIO->RecvChannelID = ENIO->CPLDInByte;
		if (ENIO->RecvChannelID) {
//			printf("\n[C] 0x%02x ", ByteIn);
		}
        return 0;
    }

    GetChan = &ENIO->Channels[ENIO->RecvChannelID];

//    printf("\n\t\t\tNPB    Que 0x%08x",GetChan->queOut);

    if (! ENIO->RecvLenGotByte1) {
        if (GetChan->cgpAware) {
            CircularBufferEnque(GetChan->queIn, ENIO->CPLDInByte);
        }
        ENIO->RecvLen = ENIO->CPLDInByte<<8;
        ENIO->RecvLenGotByte1 = 1;
//		printf("0x%02x", ENIO->CPLDInByte);
        return 0;
    }

    if (! ENIO->RecvLenGotByte2) {
        if (GetChan->cgpAware) {
            CircularBufferEnque(GetChan->queIn, ENIO->CPLDInByte);
        }
        ENIO->RecvLen|= ENIO->CPLDInByte;
        ENIO->RecvLenGotByte2 = 1;
//		printf("%02x\n", ENIO->CPLDInByte);
        return 0;
    }

    ENIO->RecvLen--;

//	printf("0x%02x ", ENIO->CPLDInByte);

    // Are we putting to the Command channel?
    if (ENIO->RecvChannelID == 1) {
//        printf("\tChan 1, Length 0x%04x - Begin\n", ENIO->RecvLen);

        // Treat as a CGP cmd
        if (! GetChan->chanCmd) {
            GetChan->chanCmd = ENIO->CPLDInByte;
        } else {
            GetChan->chanCmdData[GetChan->chanCmdDataCount] = ENIO->CPLDInByte;
            GetChan->chanCmdDataCount++;
        }


        // Is the command done?
        if (! ENIO->RecvLen) {
			//printf("\tChan 1, Length 0x%04x - Processing command 0x%02x...\n", GetChan->chanCmdDataCount+1, GetChan->chanCmd);
            ENIO->ProcessCGPCmd(ENIO, ENIO->RecvChannelID);
            GetChan->chanCmd = 0;
            GetChan->chanCmdDataLen = 0;
            GetChan->chanCmdDataCount = 0;
        }
//		printf("\tChan 1, Length 0x%04x - Processed\n", ENIO->RecvLen);
        
    } else {

        // Treat as a raw queue
        uint8 isFull = 0;
        isFull = CircularBufferEnque(GetChan->queIn, ENIO->CPLDInByte);
		//printf("0x%02x ", ENIO->CPLDInByte);
        //printf("'%c' 0x%02x 0x%02x\n", ENIO->CPLDInByte, ENIO->CPLDInByte, isFull);
    }

    if (! ENIO->RecvLen) {
//        if (ENIO->RecvChannelID == 6) printf("\n*%d*\n", GetChan->queIn->writePointer - GetChan->queIn->readPointer);
        if (GetChan->cgpAware && ENIO->RecvLenGotByte1 && ENIO->RecvLenGotByte2) {
            GetChan->queIn->pendingCmds++;
        }
        ENIO->RecvChannelID = 0;
        ENIO->RecvLenGotByte1 = 0;
        ENIO->RecvLenGotByte2 = 0;
//		printf("\n");
    }

    return 0;
}

void ENIOModule::PutCGPCmd (ENIOModule* ENIO, uint8 ChannelID, uint16 cgpCmdLen, uint8 cgpCmdType, char *cgpCmdData) {
    uint8 nextVal;
    uint16 tmpCounter;

    CGPChannel *PutCGPChan;
    PutCGPChan = &ENIO->Channels[ChannelID];
//	printf("\t\t+");
//	printf("\n\t\t\tPutCGP Que 0x%08x",PutCGPChan->queOut);
    CircularBufferEnque(PutCGPChan->queOut, cgpCmdLen>>8);
    if (PutCGPChan->LastByteOutEmpty) {
        CircularBufferDeque(PutCGPChan->queOut, &nextVal);
        PutCGPChan->LastByteOut = nextVal;
        if (ENIO->ChanOut == ChannelID) {
            ENIO->CPLDOutByte = nextVal;
        }
        PutCGPChan->LastByteOutEmpty = 0;
    }
//	printf("+");
    CircularBufferEnque(PutCGPChan->queOut, cgpCmdLen & 0x00FF);
//	printf("+");
    CircularBufferEnque(PutCGPChan->queOut, cgpCmdType);

//    printf("cgpCmdLen = %d\n", cgpCmdLen);

//    printf("+\n");

    for (tmpCounter=1; tmpCounter < cgpCmdLen; tmpCounter++) {
        printf(".");
        CircularBufferEnque(PutCGPChan->queOut, cgpCmdData[tmpCounter-1]);
    }

//    printf("-\n");
}

uint16 ENIOModule::GetCGPCmd (ENIOModule* ENIO, uint8 ChannelID, uint8 *cgpCmdType, char *cgpCmdData) {

    uint8 tmpChar;
    uint16 cgpCmdLen = 0;
    uint16 tmpCounter;

    CGPChannel *GetCGPChan;
    GetCGPChan = &ENIO->Channels[ChannelID];

//printf("Getting inbound command size high byte...\n");
    CircularBufferDeque(GetCGPChan->queIn, &tmpChar);
    cgpCmdLen = tmpChar<<8;
//printf("Getting inbound command size low byte...\n");
    CircularBufferDeque(GetCGPChan->queIn, &tmpChar);
    cgpCmdLen|= tmpChar;

    *cgpCmdType = 0;

    for (tmpCounter=0; tmpCounter < cgpCmdLen; tmpCounter++) {
        CircularBufferDeque(GetCGPChan->queIn, &tmpChar);
        if (!(*cgpCmdType)) {
            *cgpCmdType = tmpChar;
        } else {
//			printf(" 0x%02x", tmpChar);
            cgpCmdData[tmpCounter-1] = tmpChar;
        }
    }
//	printf("\n\t\tGetCGPCmd DONE\n");
    GetCGPChan->queIn->pendingCmds--;
    return cgpCmdLen;
}

void ENIOModule::ProcessCGPCmd (ENIOModule* ENIO, uint8 ChannelID) {

    uint8 Command = 0;
    uint8 TargetChan = 0;
    unsigned int i, j;

    CGPChannel *CmdChan;
    CGPChannel *TmpChan;

    char *CmdDataOut = (char*) malloc(4096);

    CmdChan = &ENIO->Channels[ChannelID];

    Command = CmdChan->chanCmd;
    TargetChan = CmdChan->chanCmdData[0];

    //CMDChanCommands swCommand = Command;

//printf("Processing CMDChan cmd ID 0x%02x...\n", Command);

    // What command did we receive?
    //switch (swCommand) {
	switch(Command) {

        case Hello:
			printf("\t\tReplying to HELLO command...\n");
            ENIO->PutCGPCmd (ENIO, ChannelID, 0x0001, 0x01, CmdDataOut);
			printf("\t\tReply sent.\n");
            break;

        case Status:
            j=0;
            for (i = 1; i <= 8; i++) {
                TmpChan = &ENIO->Channels[i];
                //CmdDataOut[j] = TmpChan->chanID;
                CmdDataOut[j] = i;
                j++;
                CmdDataOut[j] = TmpChan->chanStatus;
                j++;
                if (TmpChan->queOutInit) {
                    CmdDataOut[j] = TmpChan->queOut->size >> 8;
                    j++;
                    CmdDataOut[j] = TmpChan->queOut->size & 0x00FF;
                    j++;
                    CmdDataOut[j] = (TmpChan->queOut->pendingBytes + !TmpChan->LastByteOutEmpty) >> 8;
                    j++;
                    CmdDataOut[j] = (TmpChan->queOut->pendingBytes + !TmpChan->LastByteOutEmpty) & 0x00FF;
                    j++;
                } else {
                    CmdDataOut[j] = 0;
                    j++;
                    CmdDataOut[j] = 0;
                    j++;
                    CmdDataOut[j] = 0;
                    j++;
                    CmdDataOut[j] = 0;
                    j++;
                }
                CmdDataOut[j] = 0;
                j++;
                CmdDataOut[j] = 0;
                j++;
            }

            //for (i = 0; i < j; i++) {
            //    printf("0x%02x ", CmdDataOut[i]);
            //}
            //printf("\n");
            ENIO->PutCGPCmd (ENIO, ChannelID, j+1, 0x02, CmdDataOut);
//printf("\t\t<ENIO> Channel 0x%02x QueOut pending bytes -> %05d\n", ChannelID, (&ENIO->Channels[ChannelID])->queOut->pendingBytes);
            break;

        case SelectOut:
            ENIO->ChanOut = TargetChan;
            ENIO->CPLDOutByte = ENIO->Channels[TargetChan].LastByteOut;
//            printf("Switched output queue to %d, loaded value 0x%02x to ENIO->CPLDOutByte...\n", ENIO->ChanOut);
            break;

        case Control:
	    ENIO->Channels[TargetChan].chanCmd = CmdChan->chanCmdData[1];
            ENIO->Channels[TargetChan].chanCmdDataCount = CmdChan->chanCmdDataCount - 2;
            for (i=2; i<(CmdChan->chanCmdDataCount); i++) {
                ENIO->Channels[TargetChan].chanCmdData[i-2] = CmdChan->chanCmdData[i];
            }
            break;

        default:
            break;
    }

    free (CmdDataOut);
}

void ENIOModule::ProcessKBInput (ENIOModule* ENIO, uint8 LastKBInput) {
    // Do we have input from the keyboard?  If so, queue the data
}

//void ENIO::ProcessDynamicChannels (ENIOModule* ENIO) {

//    uint8 i;

    // Loop through the Dynamic Channels
//    for(i=4; i<8; i++) {

//      if (ENIO->Channels[i].chanCmdData[0]) {
//        ENIO->Channels[i].chanType = ENIO->Channels[i].chanCmdData[0];
//      }

//      ChannelType swChanType = ENIO->Channels[i].chanType;

//      switch (swChanType) {
//        case FileChannel:

            // Pass the ENIO channel to the file processing function
//            ENIO::ProcessFileChannel(ENIO, i);
//            break;

//        case NetworkChannel:

            // Pass the ENIO channel to the network processing function
//            ENIO::ProcessNetworkChannel(ENIO, i);
//            break;
//      }
//    }
//}

void ENIOModule::ProcessFileChannels (ENIOModule* ENIO) {

//    DIR           *d;
//    struct dirent *dir;

//    dir = malloc(sizeof (struct dirent));

    char *CmdDataOut;
    CmdDataOut = (char*) malloc(4096);

    uint16 CmdSize = 1;
    uint8 CmdType = 0;
    uint16 LoopVar;
    uint8 nextVal;
    int BytesAvail = 0;
    int BytesRead = 0;
    uint16 cgpInSize;
    uint8 FileCtrlChanID = 5;

    CGPChannel *FileCtrlChan = &ENIO->Channels[FileCtrlChanID];

    CGPChannel *FileDataChan;

    //printf("\n\t\t<ENIO> Starting ENIO::ProcessFileChannels, %d commands waiting\n", FileCtrlChan->queIn->pendingCmds);

    while (FileCtrlChan->queIn->pendingCmds) {

        cgpInSize = ENIO->GetCGPCmd (ENIO, FileCtrlChanID, &FileCtrlChan->chanCmd, (char*) FileCtrlChan->chanCmdData);

//		printf("\tFILE_CTRL Command: 0x%02x\n\t\t", FileCtrlChan->chanCmd);
		for (LoopVar=0;LoopVar<cgpInSize-1;LoopVar++) {
//			printf(" 0x%02x", FileCtrlChan->chanCmdData[LoopVar]);
		}
		FileCtrlChan->chanCmdData[LoopVar] = 0;
//		printf("\n");

//        if (FileCtrlChan->chanCmd) {
//            printf("\t\t<ENIO> Processing command [%d] for FileChannel [%d], queOut Size %d\n", FileCtrlChan->chanCmd, FileDataChanID, (&ENIO->Channels[FileDataChanID])->queOut->size);
//        }

        // First, let's see if there are any control commands to process

        //FileChanCommand swChanCmd = FileCtrlChan->chanCmd;

        //switch (swChanCmd) {
		switch(FileCtrlChan->chanCmd) {

			/*
			------------------------
			 0 - Channel to control
			 1 - R/W
			 2+ - Filename
			------------------------
			*/

            case CmdOpenFile:
                CmdType = 1;
                FileDataChan = &ENIO->Channels[FileCtrlChan->chanCmdData[0]];
				FileDataChan->chanType = FileChannel;
				strcpy ((char *)FileDataChan->pFileName, (char *)&FileCtrlChan->chanCmdData[2]);
                //FileDataChan->pFileName = &FileCtrlChan->chanCmdData[2];
                if (FileCtrlChan->chanCmdData[1]) {
                    FileDataChan->pFileHandle = fopen((char*)FileDataChan->pFileName,"rb");
                    FileDataChan->FileOpenMode = 1;
                } else {
                    FileDataChan->pFileHandle = fopen((char*)FileDataChan->pFileName,"wb");
                    FileDataChan->FileOpenMode = 0;
                }
                if(FileDataChan->pFileHandle == NULL) {
                    // Could not open file
                    printf("\t\t<ENIO> COULD NOT OPEN FILE '%s'!\n", (char*)FileDataChan->pFileName);
                    CmdDataOut[0] = 0;
                } else {
                    // Opened Successfully
                    printf("\t\t<ENIO> FILE OPENED - [%d] %s\n", FileCtrlChan->chanCmdData[0], (char*)FileDataChan->pFileName);
                    CmdDataOut[0] = 1;
                    FileDataChan->chanStatus = 1;
                    FileDataChan->cgpAware = 1;

                }
                CmdSize++;
                break;

            case CmdCloseFile:

                CmdType = 2;
                FileDataChan = &ENIO->Channels[FileCtrlChan->chanCmdData[0]];
                printf("\n\t\t<ENIO> Closing file [%d] %s...\n", FileCtrlChan->chanCmdData[0], (char*)FileDataChan->pFileName);
                fclose(FileDataChan->pFileHandle);
                printf("\n\t\t<ENIO> FILE CLOSED\n");
                break;

            case CmdListFiles:
//                CmdType = 3;
    //            printf("\t\t<ENIO> Opening directory for listing...\n");
//                d = opendir(".");
//                if (d)
//                {
                    // Need to add Queue Full checking

//                    while ((dir = readdir(d)) != NULL)
//                    {
                        // Need to add Queue Full checking
                        //printf("%s\n", dir->d_name);
                        //CmdDataOut[CmdSize-1] = dir->d_type;
                        //CmdSize++;
//                        if (dir->d_type == 0x08) {
                            //printf("%s -> 0x%02x, %d\n", dir->d_name, dir->d_type, strlen(dir->d_name));
//                            for (LoopVar = 0; LoopVar < strlen(dir->d_name); LoopVar++) {
//                                CmdDataOut[CmdSize-1] = dir->d_name[LoopVar];
//                                CmdSize++;
//                            }
//                            CmdDataOut[CmdSize-1] = 0x0A;
//                            CmdSize++;
//                        }
//                    }
//                    closedir(d);
//                }
//            printf("\t\t<ENIO> Done listing directory.\n");

                break;

            default:
                break;
        }

        if (CmdType) {
//printf("\t\t<ENIO> Enqueueing %d bytes to Channel %d, CmdType = 0x%02x, CmdDataOut[0] = 0x%02x...\n", CmdSize, 5, CmdType, CmdDataOut[0]);
            ENIO->PutCGPCmd (ENIO, FileCtrlChanID, CmdSize, CmdType, CmdDataOut);
            FileCtrlChan->chanCmd = 0;
        }
    }

    for (LoopVar=7; LoopVar<=8; LoopVar++) {

      FileDataChan = &ENIO->Channels[LoopVar];

	  if (FileDataChan->chanType != FileChannel) {
		  continue;
	  }

      if ((FileDataChan->chanStatus) && (FileDataChan->FileOpenMode)) {
//		printf("[%d] queOut %d/%d\n", LoopVar, FileDataChan->queOut->pendingBytes, FileDataChan->queOut->size);
        // READ FILE DATA
//        BytesAvail = FileDataChan->queOut->size - FileDataChan->queOut->pendingBytes;
//        printf("Queue %d pending bytes: %d\n", LoopVar, FileDataChan->queOut->pendingBytes);

//		if ((FileDataChan->queOut->size - FileDataChan->queOut->pendingBytes) <= 256) {
//			printf("[%d] queOut %d/%d - FULL!\n", LoopVar, FileDataChan->queOut->pendingBytes, FileDataChan->queOut->size);
//		}

		if (((FileDataChan->queOut->size - FileDataChan->queOut->pendingBytes) > 256) && (FileDataChan->chanStatus)) {

//		  printf("Reading file...\n");

          while (((FileDataChan->queOut->size - FileDataChan->queOut->pendingBytes) > 256) && (FileDataChan->chanStatus)) {
//            printf("Reading file...\n");

            if(fread(&CmdDataOut[0],1,1,FileDataChan->pFileHandle)){
                //printf(" 0x%02x", CmdDataOut[0]);
                CircularBufferEnque(FileDataChan->queOut, CmdDataOut[0]);
                if (FileDataChan->LastByteOutEmpty) {
                    CircularBufferDeque(FileDataChan->queOut, &nextVal);
                    FileDataChan->LastByteOut = nextVal;
                    if (ENIO->ChanOut == LoopVar) {
                        ENIO->CPLDOutByte = nextVal;
                    }
                   FileDataChan->LastByteOutEmpty = 0;
                }
                BytesRead++;
            } else {
                fclose(FileDataChan->pFileHandle);
                FileDataChan->chanStatus = 0;

            }

			if ((BytesRead % 256 == 0) || (FileDataChan->chanStatus == 0)) {
				//printf("\n\t\tRead %d bytes from file to Chan[%d]->queOut... (%d/%d)", BytesRead, LoopVar, FileDataChan->queOut->pendingBytes, FileDataChan->queOut->size);
			}
          }
		}
      }

      if ((FileDataChan->chanStatus) && (! (FileDataChan->FileOpenMode)) && (FileDataChan->queIn->pendingBytes)) {
        // WRITE FILE DATA
		//printf("\n\t\tWriting %d bytes to File from Chan %02d, que start @ %d...\n", FileDataChan->queIn->pendingBytes, LoopVar, FileDataChan->queIn->readPointer);
		
        while (FileDataChan->queIn->pendingBytes) {
            uint16 tmpWriteCount;
            uint8 tmpWriteBuffer[64];
			//printf("\n\t\tWriting %d to File Chan %02d...\n", LoopVar);
            for (tmpWriteCount=0; tmpWriteCount < 64 && FileDataChan->queIn->pendingBytes; tmpWriteCount++) {
                CircularBufferDeque(FileDataChan->queIn, &tmpWriteBuffer[tmpWriteCount]);
				//printf(" 0x%02x",tmpWriteBuffer[tmpWriteCount]);
            }
			//printf("\n");
            fwrite(tmpWriteBuffer, 1, tmpWriteCount, FileDataChan->pFileHandle);
        }
      }

      BytesRead = 0;
    
	}

    free(CmdDataOut);
}

void ENIOModule::ProcessNetworkChannels (ENIOModule* ENIO) {

	char *CmdDataOut;
    CmdDataOut = (char*) malloc(4096);
	char *CmdDataIn;
    CmdDataIn = (char*) malloc(4096);

    uint16 CmdSize = 1;
    uint8 CmdType = 0;
    uint16 LoopVar;
    uint8 nextVal;
    int BytesAvail = 0;
    int BytesRead = 0;
	int recvCount = 0;
	int tmpCount = 0;
    uint16 cgpInSize = 0;
    uint16 cgpOutSize = 0;
    uint8 NetCtrlChanID = 4;
	uint16 bufferCount = 0;

    CGPChannel *NetCtrlChan = &ENIO->Channels[NetCtrlChanID];

    CGPChannel *NetDataChan;

	// First, let's see if there are any control commands to process

    while (NetCtrlChan->queIn->pendingCmds) {

      cgpInSize = ENIO->GetCGPCmd (ENIO, NetCtrlChanID, &NetCtrlChan->chanCmd, (char*) NetCtrlChan->chanCmdData);
	  for (LoopVar=0;LoopVar<cgpInSize-1;LoopVar++) {
//			printf(" 0x%02x", FileCtrlChan->chanCmdData[LoopVar]);
      }
	  NetCtrlChan->chanCmdData[LoopVar] = 0;

	  switch(NetCtrlChan->chanCmd) {

		  	/*
			------------------------
			 0 - Channel to control
			 1 - TCP/UDP
			 2 - PortH
			 3 - PortL
			 4+ - Hostname
			------------------------
			*/

        case CmdOpenSocket:
		  NetDataChan = &ENIO->Channels[NetCtrlChan->chanCmdData[0]];
		  NetDataChan->chanType = NetworkChannel;
          if (NetCtrlChan->chanCmdData[1]) {
                // Set open socket parameters - TCP
                NetDataChan->serverProtocol = IPPROTO_TCP;
                NetDataChan->serverType = SOCK_STREAM;
            } else {
                // Set open socket parameters - UDP
                NetDataChan->serverProtocol = IPPROTO_UDP;
                NetDataChan->serverType = SOCK_DGRAM;
            }

            NetDataChan->chanStatus = 1;
            NetDataChan->cgpAware = 1;
		
            NetDataChan->SocketState = SCK_START;
            NetDataChan->serverPort = NetCtrlChan->chanCmdData[3] | NetCtrlChan->chanCmdData[2]<<8;

			strcpy ((char *)NetDataChan->pServerName, (char *)&NetCtrlChan->chanCmdData[4]);

            break;

        case CmdCloseSocket:
			NetDataChan = &ENIO->Channels[NetCtrlChan->chanCmdData[0]];
            //if (TCP) {
                // Set close socket parameters - TCP
            //} else if (UDP) {
                // Set close socket parameters - UDP
            //}

            NetDataChan->SocketState = SCK_DISCONNECT;
            break;

        default:
            break;

	  }

	  if (CmdType) {
//printf("\t\t<ENIO> Enqueueing %d bytes to Channel %d, CmdType = 0x%02x, CmdDataOut[0] = 0x%02x...\n", CmdSize, 5, CmdType, CmdDataOut[0]);
            ENIO->PutCGPCmd (ENIO, NetCtrlChanID, CmdSize, CmdType, CmdDataOut);
            NetCtrlChan->chanCmd = 0;
      }

	}


	// Done processing any commands.  Now loop through the Dynamic Channels

    for (LoopVar=7; LoopVar<=8; LoopVar++) {

		NetDataChan = &ENIO->Channels[LoopVar];

		if (NetDataChan->chanType != NetworkChannel) {
		  continue;
	    }

        if ((NetDataChan->chanStatus) && (NetDataChan->SocketState)) {

		  switch (NetDataChan->SocketState) {
			case SCK_INACTIVE:
				return;

			case SCK_START:
				NetDataChan->SocketState = SCK_RESOLVE;

			case SCK_RESOLVE:
	            NetDataChan->host = gethostbyname((char*)NetDataChan->pServerName);
				if (NetDataChan->host) {  // If host is resolved... needs to be modified for PIC32 & emulators
	                NetDataChan->SocketState = SCK_CREATE;
					printf("\t\tResolved '%s' to %s\n", NetDataChan->pServerName, inet_ntoa(*((struct in_addr *)NetDataChan->host->h_addr)));
				} else {
					printf("\t\tCould not resolve '%s'\n", NetDataChan->pServerName);

			        DWORD dwError;
					dwError = WSAGetLastError();
					if (dwError != 0) {
						if (dwError == WSAHOST_NOT_FOUND) {
							printf("Host not found\n");
						} else if (dwError == WSANO_DATA) {
							printf("No data record found\n");
						} else {
						    printf("Function failed with error: %ld\n", dwError);
						}
					}
					break;
				}

			case SCK_CREATE:
				NetDataChan->bsdSocket = socket(AF_INET, NetDataChan->serverType, NetDataChan->serverProtocol);

				if (NetDataChan->bsdSocket == -1)
	            //if((NetDataChan->bsdSocket = socket(AF_INET, NetDataChan->serverType, NetDataChan->serverProtocol)) == -1 )
				{
					printf("Could not create socket, NetDataChan->bsdSocket = %d\n", NetDataChan->bsdSocket);
	                break;
				}
				printf("CREATED SOCKET\n");
				NetDataChan->SocketState = SCK_CONNECT;

				/* Set the socket to nonblocking */
//				if (ioctlsocket(NetDataChan->bsdSocket, FIONBIO, &nonblocking) != 0)
//					printf("ioctlsocket() failed");
            
			if (NetDataChan->serverProtocol == IPPROTO_UDP) {
				u_long nonblocking_enabled = TRUE;
				ioctlsocket(NetDataChan->bsdSocket , FIONBIO, &nonblocking_enabled );

                //flags = fcntl(NetDataChan->bsdSocket, F_GETFL);
                //flags |= O_NONBLOCK;
                //fcntl(NetDataChan->bsdSocket, F_SETFL, flags);
            }

				NetDataChan->server_addr.sin_family = AF_INET;
				NetDataChan->server_addr.sin_addr = *((struct in_addr *)NetDataChan->host->h_addr);
				NetDataChan->server_addr.sin_port = NetDataChan->serverPort;
				//bzero(&(ENIO->server_addr.sin_zero),8);
				memset(&(NetDataChan->server_addr.sin_zero), '\0', 8);

			case SCK_CONNECT:
	            if (NetDataChan->serverProtocol == IPPROTO_TCP) {
//					if(connect( NetDataChan->bsdSocket, (struct sockaddr*)&ENIO->server_addr, addrlen) < 0)
//					{
//	                    printf("Could not connect to server %s:%d, error - %d\n",inet_ntoa(ENIO->server_addr.sin_addr), ntohs(ENIO->server_addr.sin_port), connect( NetDataChan->bsdSocket, (struct sockaddr*)&ENIO->server_addr, addrlen));
//						return;
//					}
//                flags = fcntl(NetDataChan->bsdSocket, F_GETFL);
//                flags |= O_NONBLOCK;
//                fcntl(NetDataChan->bsdSocket, F_SETFL, flags);
				} else {
					// No connect logic needed for UDP
				}
				NetDataChan->SocketState = SCK_ACTIVE;

				printf("Activated socket to server %s:%d\n",inet_ntoa(NetDataChan->server_addr.sin_addr), ntohs(NetDataChan->server_addr.sin_port));

			case SCK_ACTIVE:
//            printf("Checking for data to send...\n");
				bufferCount = 0;
				recvCount = 0;
                cgpOutSize = 0;

                if (NetDataChan->queIn->pendingCmds) {
					CircularBufferDeque(NetDataChan->queIn, (uint8 *)&ENIO->outBuffer[bufferCount]);
                    cgpOutSize = ENIO->outBuffer[bufferCount]<<8;
                    bufferCount++;
                    CircularBufferDeque(NetDataChan->queIn, (uint8 *)&ENIO->outBuffer[bufferCount]);
                    cgpOutSize|= ENIO->outBuffer[bufferCount];
                    bufferCount++;
					
					for (tmpCount=0;tmpCount<cgpOutSize;tmpCount++) {
                        CircularBufferDeque(NetDataChan->queIn, (uint8 *)&ENIO->outBuffer[bufferCount]);
                        bufferCount++;
                    }
                                    
                    NetDataChan->queIn->pendingCmds--;
                }

				if (bufferCount) {
	                if (NetDataChan->serverProtocol == IPPROTO_TCP) {
						send(NetDataChan->bsdSocket, (char*) ENIO->outBuffer, bufferCount, 0);
					} else {
	                    //sendto(NetDataChan->bsdSocket, (char*) ENIO->outBuffer, bufferCount, 0, (struct sockaddr*)NetDataChan->server_addr.sin_addr, addrlen);
						sendto(NetDataChan->bsdSocket, (char*) ENIO->outBuffer, bufferCount, 0, (struct sockaddr*)&NetDataChan->server_addr, sizeof(NetDataChan->server_addr));
	                    printf("Sent %d bytes to UDP server\n\r", bufferCount);
					}
				}

				// First, see if there is any data to receive.
//		           printf("Waiting to receive data 1...\n");
				if (NetDataChan->serverProtocol == IPPROTO_TCP) {
//	                recvCount = recv(NetDataChan->bsdSocket, (char*) ENIO->inBuffer, sizeof(ENIO->inBuffer)-1, 0);
				} else {
//					  printf("---Receive UDP---\t%d inBuffer Size\n", sizeof(ENIO->inBuffer)-1);

//					recvCount = recvfrom(NetDataChan->bsdSocket, ENIO->inBuffer, sizeof(ENIO->inBuffer)-1, 0, (struct sockaddr*)&ENIO->server_addr, &addrlen);
					recvCount = recvfrom(NetDataChan->bsdSocket, (char *)&ENIO->inBuffer, sizeof(ENIO->inBuffer)-1, 0, NULL, NULL);
//	                recvCount = recvfrom(NetDataChan->bsdSocket, CmdDataIn, sizeof(CmdDataIn)-1, 0, NULL, NULL);
				}

//	   printf("Waiting to receive data 2...\n");
		        if (recvCount > 0 && recvCount != 0xFFFF) {
				   printf("Receiving %d bytes from server...\n", recvCount);
	                for (tmpCount=0; tmpCount < recvCount; tmpCount++) {
//                    printf("0x%02x ", ENIO->inBuffer[tmpCount]);
						CircularBufferEnque(NetDataChan->queOut, ENIO->inBuffer[tmpCount]);
//						if (NetDataChan->LastByteOutEmpty) {
//							CircularBufferDeque(NetDataChan->queOut, &nextVal);
//							NetDataChan->LastByteOut = nextVal;
//							if (ENIO->ChanOut == ChannelID) {
//	                            ENIO->CPLDOutByte = nextVal;
//							}
//							NetDataChan->LastByteOutEmpty = 0;
//						}
					}
//				} else {
//		               printf("No data received\n");
					recvCount = 0;
				}
		
				break;

			case SCK_DISCONNECT:
				close(NetDataChan->bsdSocket);
				NetDataChan->SocketState = SCK_INACTIVE;

				break;
					
			default:
				break;
		  }

		}
	}

	free(CmdDataOut);
	free(CmdDataIn);
}

void ENIOModule::Init(ENIOModule* ENIO)
{
	// Set Memory Handlers
//	SetReadHandler(0x4700,0x47FF,ENIONESGetByte);
//	SetWriteHandler(0x4700,0x47FF,ENIONESPutByte);

	// Perform startup tasks
	CGPChannel *CurChan;
	
	uint8 i;
	uint8 CmdChan = 1;
    uint8 NetCtrlChan = 4;
    uint8 NetDataChan = 8;
    uint8 FileCtrlChan = 5;
    uint8 DynChan1 = 7;
    uint8 DynChan2 = 8;
    uint8 CmdType = 0;
    uint16 CmdLen = 0;
    uint16 LoopVar = 0;
    uint16 BytesPending = 0;
    uint8 ByteIn = 0;

    uint8 CmdInType;
    uint16 CmdInLen;
    char *CmdInData;
    CmdInData = (char*) malloc(4096);

//	inBuffer = (char*) malloc(4096);
//	outBuffer = (char*) malloc(4096);

	memset(inBuffer, '\0', 4096);
	memset(outBuffer, '\0', 4096);
	memset(&Garbage, '\0', 8192);

    for (i = 1; i <= 8; i++) {
        CurChan = &Channels[i];
        CurChan->queInInit = 0;
        CurChan->queOutInit = 0;
        CurChan->chanStatus = 0;
        CurChan->cgpAware = 0;
        CurChan->LastByteOutEmpty = 1;
    }

    CurChan = &Channels[CmdChan];

    printf("Allocating queue space for Cmd channel %d...\n", CmdChan);
	printf("\n\t\t\tInit1  Que 0x%08x",CurChan->queOut);
    CircularBufferInit(&CurChan->queOut, 1024);
	printf("\n\t\t\tInit2  Que 0x%08x",CurChan->queOut);
    CurChan->queOutInit = 1;

	CurChan = &Channels[NetCtrlChan];

    printf("Allocating queue space for Net Control channel %d...\n", NetCtrlChan);
    CircularBufferInit(&CurChan->queIn, 1024);
    CurChan->queInInit = 1;
    CircularBufferInit(&CurChan->queOut, 9000);
    CurChan->queOutInit = 1;
    CurChan->cgpAware = 1;

    CurChan = &Channels[FileCtrlChan];

    printf("Allocating queue space for File Control channel %d...\n", FileCtrlChan);
    CircularBufferInit(&CurChan->queIn, 1024);
    CurChan->queInInit = 1;
    CircularBufferInit(&CurChan->queOut, 9000);
    CurChan->queOutInit = 1;
    CurChan->cgpAware = 1;

    CurChan = &Channels[DynChan1];

    printf("Allocating queue space for File Data channel %d...\n", DynChan1);
    CircularBufferInit(&CurChan->queIn, 1024);
    CurChan->queInInit = 1;
    CircularBufferInit(&CurChan->queOut, 9000);
    CurChan->queOutInit = 1;

    CurChan = &Channels[DynChan2];

    printf("Allocating queue space for Net Data channel %d...\n", DynChan2);
    CircularBufferInit(&CurChan->queIn, 9000);
    CurChan->queInInit = 1;
    CircularBufferInit(&CurChan->queOut, 9000);
    CurChan->queOutInit = 1;

	CurChan = &Channels[DynChan1];
	printf("Allocating queue space for DynChan1 Filename...\n");
	CurChan->pFileName = (uint8 *) malloc(255);
	printf("Allocating queue space for DynChan1 Servername...\n");
	CurChan->pServerName = (uint8 *) malloc(255);

	CurChan = &Channels[DynChan2];
	printf("Allocating queue space for DynChan2 Filename...\n");
	CurChan->pFileName = (uint8 *) malloc(255);
	printf("Allocating queue space for DynChan2 Servername...\n");
	CurChan->pServerName = (uint8 *) malloc(255);

    ChanOut = 1;

	WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void ENIOModule::MainLoop(ENIOModule* ENIO)
{
	// ENIO Main Program Loop

	// Process queues for input and output
	ProcessFileChannels(ENIO);
	ProcessNetworkChannels(ENIO);
}

//static DECLFW(MyENIO.NESPutByt);
//static DECLFR(MyENIO.NESGetByte);

//static void ENIOUninit(void)
//{
	// Whack the ENIO module
//}

