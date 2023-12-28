#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <tchar.h>

/*** defines ***/
#define MaxFileNameLen 100
#define MaxLineNum 1000
#define MaxLenofLine 10000

/*** data ***/
typedef struct {
	char* fileName[MaxFileNameLen];
	int totalLines;
}FILE_METADATA;

typedef struct fileInfo
{
	char* content[MaxLineNum]; //At first, we are going to handle the text file that has 1~1000 lines.
	DWORD lineSize[MaxLineNum]; //store the size of 'one line' in the array
}TEXTFILE;

/*** functions ***/
void configureSerialPort(HANDLE hSerial);
int receiveMetadataOverSerial(HANDLE hSerial, FILE_METADATA* metadata);
DWORD receiveDataOverSerial(HANDLE hSerial, char* buffer, DWORD bufferSize);
void receiveTextFileContentOverSerial(HANDLE hSerial, FILE_METADATA* metadata, TEXTFILE* filePtr);
int saveFile(const char* filePath, FILE_METADATA* metadata, TEXTFILE* filePtr);

void configureSerialPort(HANDLE hSerial)
{
	DCB dcbSerialParams = { 0 }; //DCB structure is used to store the configuration parameters for the serial port
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	if (!GetCommState(hSerial, &dcbSerialParams)) //GetCommState ftn retrieves the current configuration of the serial port and stores it iin the 'dcbSerialParams' structure
	{
		//Handle error

		return;
	}

	dcbSerialParams.BaudRate = CBR_9600; //Set your baud rate : the speed of the data transmission
	dcbSerialParams.ByteSize = 8; //8 data bits : specifies the number of data bits in each character
	dcbSerialParams.StopBits = ONESTOPBIT; //1 stop bit : specifies the number of stop bits used to indicate the end of a character
	dcbSerialParams.Parity = NOPARITY; //No parity : specifies whether and how parity checking is performed. in this case, no parity checking is performed.

	if (!SetCommState(hSerial, &dcbSerialParams)) //SetCommState ftn applies the configured parameters to the serial port.
	{                                             //if successful, the serial port is now configured with the desired settings.
		//Handle error
		return;
	}
}

DWORD receiveDataOverSerial(HANDLE hSerial, char* buffer, DWORD bufferSize) //receive txt file from the serial port
{
	//Receive file content
	DWORD bytesRead;

	//serial port => buffer, copy file from serial port to buffer line by line using hSerial
	if (!ReadFile(hSerial, buffer, bufferSize - 1, &bytesRead, NULL))
	{
		//Handle error
		DWORD error = GetLastError();
		printf("Error reading from serial port. Error code: %lu\n", error);
		return -1;
	}

	buffer[bytesRead] = '\0';

	return bytesRead;
}

int receiveMetadataOverSerial(HANDLE hSerial, FILE_METADATA* metadata)
{
	int bytesRead;

	//Receive file name
	bytesRead = receiveDataOverSerial(hSerial, metadata->fileName, MaxFileNameLen);
	
	if (bytesRead == -1)
	{
		//Handle error

		return -1;
	}

	//Receive total lines
	int receivedTotalLines;
	bytesRead = receiveDataOverSerial(hSerial, &receivedTotalLines, sizeof(int));

	if (bytesRead == -1)
	{
		//Handle error
		return -1;
	}

	metadata->totalLines = receivedTotalLines;

	return 0; //Success
}

void receiveTextFileContentOverSerial(HANDLE hSerial, FILE_METADATA* metadata, TEXTFILE* filePtr)
{
	for (int i = 0; i < metadata->totalLines; i++)
	{
		receiveDataOverSerial(hSerial, filePtr->content[i], MaxLenofLine);
	}
}

int saveFile(const char* filePath, FILE_METADATA* metadata, TEXTFILE* filePtr) //save text file line by line to the memory
{
	FILE* newFile;

	if (fopen_s(&newFile, filePath, "w") != 0)
	{
		printf("Failed to write a new file.");
		return -1;
	}

	//file writing. TEXTFILE => a new file
	for (int lineNum = 0; lineNum < metadata->totalLines; lineNum++)
	{
		fputs(filePtr->content[lineNum], newFile);
	}

	fclose(newFile);

	return 0;
}

int main()
{
	const char* serialPort = "COM1"; //Adjust the port as needed
	const char* filePath = "path/to/your/Santa Tell Me.txt"; //Adjust the file path

	HANDLE hSerial = CreateFile(_T("COM1"), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	TEXTFILE dyTextFile;
	TEXTFILE* filePtr = (TEXTFILE*)malloc(sizeof(dyTextFile));

	FILE_METADATA metadata;
	FILE_METADATA* metadataPtr = (FILE_METADATA*)malloc(sizeof(metadata));

	if (hSerial == INVALID_HANDLE_VALUE) {
		//Handle error

		return 1;
	}

	configureSerialPort(hSerial);

	receiveMetadataOverSerial(hSerial, metadataPtr);

	receiveTextFileContentOverSerial(hSerial, metadataPtr, filePtr);

	saveFile(filePath, metadataPtr, filePtr);

	CloseHandle(hSerial);

	free(filePtr);
	free(metadataPtr);

	return 0;
}
