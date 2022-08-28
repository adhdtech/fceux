#include "../src/drivers/win/common.h"

//typedef unsigned char uint8;
//typedef unsigned short uint16;
//typedef unsigned char KeyType;
//typedef unsigned short DWORD;
//typedef unsigned int WORD;
//typedef unsigned char uint8;
//typedef unsigned int SOCKET;

typedef enum {
	SCK_INACTIVE = 0,
	SCK_START,
	SCK_RESOLVE,
	SCK_CREATE,
	SCK_CONNECT,
	SCK_ACTIVE,
	SCK_DISCONNECT
} CGPSocketState;

typedef enum {
	Hello		= 0x01,
	Status		= 0x02,
	SelectOut	= 0x03,
	Control		= 0x04
} CMDChanCommands;

typedef enum {
	CMDChan     = 0x01,
	KBInputChan = 0x02,
	UARTChan    = 0x03,
	Ethernet    = 0x04,
	FileSystem  = 0x05,
	DynChan0    = 0x11,
	DynChan1    = 0x12,
	DynChan2    = 0x13,
	DynChan3    = 0x14
} ChannelList;

typedef enum {
	FileChannel		= 0x01,
	NetworkChannel	= 0x02
} ChannelType;

typedef enum {
	CmdOpenFile		= 0x01,
	CmdCloseFile	= 0x02,
	CmdListFiles	= 0x03
} FileChanCommand;

typedef enum {
	CmdOpenSocket	= 0x01,
	CmdCloseSocket  = 0x02
} NetChanCommand;

typedef struct
{
	int writePointer; /**< write pointer */
	int readPointer;  /**< read pointer */
	uint8 pendingCmds;
	uint8 lastCmdToGo;
	uint8 curCmdToGo;
	int size;         /**< size of circular buffer */
	int pendingBytes;
	//KeyType keys[];    /**< Element of circular buffer */
	uint8* keys;
} CircularBuffer;

typedef struct
{
	uint8		chanID;
	uint8		queInInit;
	CircularBuffer*	queIn;
	uint8		queOutInit;
	CircularBuffer*	queOut;
	FILE*		pFileHandle;
	DWORD		serverIP;
	WORD		serverPort;
	uint8		serverProtocol;
	uint8		serverType;
	SOCKET		bsdSocket;
	CGPSocketState	SocketState;
	uint8		chanStatus;
	uint8		chanCmd;
	ChannelType	chanType;
        uint8           cgpAware;
	uint8		LastByteOut;
	uint8		LastByteOutEmpty;
	uint8		chanCmdData[64];
	uint8		chanCmdDataLen;
	uint8    	chanCmdDataCount;
	uint8		FileOpenMode;
	uint8*		pFileName;
	uint8*		pServerName;
	int			totalOut;
	sockaddr_in	server_addr;
    struct hostent  *host;
} CGPChannel;


// Function Prototypes

CircularBuffer* CircularBufferInit(CircularBuffer** pQue, int size);
int CircularBufferEnque(CircularBuffer* que, uint8 pK);
int CircularBufferIsFull(CircularBuffer* que);
int CircularBufferIsEmpty(CircularBuffer* que);
int CircularBufferDeque(CircularBuffer* que, uint8 *pK);

class ENIOModule {
  public:
	    
//        char*           inBuffer;
//        char*           outBuffer;
		uint8           inBuffer[4096];
		uint8           outBuffer[4096];
        uint8           CPLDOutByte;
        uint8           CPLDInByte;
        uint8           ChanOut;
        uint8           RecvChannelID;
        uint8           RecvLenGotByte1;
        uint8           RecvLenGotByte2;
        uint16          RecvLen;
        uint8           Activated;
		CGPChannel      Channels[9];
		uint8			Garbage[8192];

		uint8 NESGetByte (ENIOModule* ENIO);
		uint8 NESPutByte (ENIOModule* ENIO, uint8 putVal);

		void ProcessCGPCmd (ENIOModule* ENIO, uint8 ChannelID);
		void ProcessKBInput (ENIOModule* ENIO, uint8 LastKBInput);
		void ProcessDynamicChannels (ENIOModule* ENIO);
		void ProcessFileChannels (ENIOModule* ENIO);
		void ProcessNetworkChannels (ENIOModule* ENIO);
		void PutCGPCmd (ENIOModule* ENIO, uint8 ChannelID, uint16 cgpCmdLen, uint8 cgpCmdType, char *cgpCmdData);
		uint16 GetCGPCmd (ENIOModule* ENIO, uint8 ChannelID, uint8 *cgpCmdType, char *cgpCmdData);
		void Init (ENIOModule* ENIO);
		void MainLoop (ENIOModule* ENIO);
  private:
};

//#if defined(GOTENIO)
//#else
// #define GOTENIO "Yes"
// static ENIOModule MyENIO;
//#endif
