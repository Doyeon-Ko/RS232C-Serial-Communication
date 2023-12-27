/*** includes ***/
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h> //header file needed for serial communication on Windows
#include <tchar.h> //header file needed for serial communication on Windows
#include <stdbool.h> //to define boolean variable

/*** defines ***/
#define MaxFileNameLen 100
#define MaxLineNum 1000
#define MaxLenofLine 10000

/*** data ***/
typedef struct fileInfo
{
	char* fileName;
	int totalLine;
	char* content[MaxLineNum]; //At first, we are going to handle the text file that has 1~1000 lines.
	DWORD lineSize[MaxLineNum]; //store the size of 'one line' in the array
}TEXTFILE;

typedef struct {
	char* fileName[MaxFileNameLen];
	int totalLines;
}FILE_METADATA;

/*** functions ***/
int myFgets(char* buffer, int max, FILE* fp); //save text file line by line
int loadFile(const char* filePath, TEXTFILE* filePtr);
void configureSerialPort(HANDLE hSerial);
bool isWriteSuccessful(DWORD expectedSize, DWORD actualSize);
int sendMetadataOverSerial(HANDLE hSerial, const char* filename, int totalLines);
int sendDataOverSerial(HANDLE hSerial, const char* data, size_t dataSize);

/*** definition ***/
int myFgets(char* buffer, int max, FILE* fp)
{
	size_t bytesRead;
	int i = 0;
	char dat = 0;

	while (i < max - 1)
	{
		bytesRead = fread(&dat, sizeof(dat), 1, fp); //read text character by character until meet the linefeed character

		if (bytesRead > 0)
		{
			if (dat == '\n') 
			{
				buffer[i] = dat;
				buffer[i + 1] = 0;
				return i + 1;
			}

			buffer[i++] = dat;
		}

		else if (bytesRead == 0 && !feof(fp)) //file is empty
		{
			buffer[i] = 0;
			printf("File is empty.");
			return -1;
		}

		else if (ferror(fp))
		{
			perror("Error from reading the file.");
			return -1;
		}
	}
}

int loadFile(const char* filePath, TEXTFILE* filePtr)
{
	FILE* file = fopen(filePath, "rb");

	if (file == NULL)
	{
		perror("Error opening the file. Check the file exists.");
		return -1;
	}

	char* buffer = NULL;
	char* secondBuffer = NULL;
	size_t textLen;
	int lineNum = 0;

	buffer = (char*)malloc(MaxLenofLine * sizeof(char));

	if (!buffer)
	{
		printf("Memory allocation for loading text file to buffer failed.");
		return -1;
	}

	//file reading. txt file => buffer
	while (1) 
	{
		if (myFgets(buffer, MaxLenofLine, file) > 0) //fread ftn read one line normally and met the linefeed character
		{
			textLen = strlen(buffer);
			filePtr->lineSize[lineNum] = textLen;

			secondBuffer = (char*)realloc(buffer, (sizeof(char) * textLen) + 1);

			if (secondBuffer == NULL)
			{
				printf("Memory allocation for loading text file to second buffer failed.");
				exit(1);
			}

			filePtr->content[lineNum++] = secondBuffer;

			if (feof(file)) //reach the end of the file
			{
				filePtr->totalLine = lineNum - 1;
				fclose(file);
				break;
			}
		} 

		else
		{
			perror("Failed to load file appropriately.");
			fclose(file);
			exit(1);
		}
	}
}

void configureSerialPort(HANDLE hSerial) //configuring the parameter of the serial port
{
	DCB dcbSerialParams = { 0 }; //DCB structure is used to store the configuration parameters for the serial port
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	if (!GetCommState(hSerial, &dcbSerialParams)) //GetCommState ftn retrieves the current configuration of the serial port and stores it in the 'dcbSerialParams' structure
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

bool isWriteSuccessful(DWORD expectedSize, DWORD actualSize)
{
	return expectedSize == actualSize;
}

int sendMetadataOverSerial(HANDLE hSerial, const char* fileName, int totalLines)
{
	FILE_METADATA metadata;

	strcpy(metadata.fileName, fileName);
	metadata.totalLines = totalLines;

	return sendDataOverSerial(hSerial, (const char*)&metadata, sizeof(FILE_METADATA));
}

int sendDataOverSerial(HANDLE hSerial, const char* data, size_t dataSize)
{
	//Send file content
	DWORD bytesWritten;

	//buffer => serial port, copy file from buffer to serial port line by line using hSerial
	if (WriteFile(hSerial, data, dataSize, &bytesWritten, NULL) == 0) //Error handling
	{
		DWORD error = GetLastError();
		printf("Error writing to serial port. Error code: %lu\n", error);
		return -1;
	}

	//check whether the content of file is correctly transmitted.
	return bytesWritten;
}

int sendTextFileContentOverSerial(HANDLE hSerial, TEXTFILE* filePtr)
{
	//Send metadata
	if (sendMetadataOverSerial(hSerial, filePtr->fileName, filePtr->totalLine) == -1)
	{
		//Handle error
		return -1;
	}

	//Send each line to the serial port
	for (int i = 0; i < filePtr->totalLine; i++)
	{
		DWORD lineSize = strlen(filePtr->content[i]);
		DWORD bytesSent = sendDataOverSerial(hSerial, filePtr, filePtr->content[i], lineSize);

		if (!isWriteSuccessful(lineSize, bytesSent))
		{
			if (bytesSent < 0)
			{
				//Handle error : The file is not appropriately transmitted from buffer to serial port
				CloseHandle(hSerial);

				printf("Text file is not correctly transmitted to the serial port. Please try again to resend it.");

				//Free allocated memory before returning
				for (int k = 0; k < filePtr->totalLine; k++)
				{
					free(filePtr->content[k]);
				}

				free(filePtr->content);

				return -1;
			}

			else
			{
				printf("Error: Unexpected number of bytes sent. Expected: %lu, Actual: %lu\n", lineSize, bytesSent);
				return -1;
			}
		}
	}

	return 0;
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		printf("The number of arguments is insufficient.\n Please enter as below.\n");
		printf("> RS232C Serial Communication.exe <SantaTellMe>");
		return 0;
	}

	const char* serialPort = "COM1"; //Adjust the port as needed
	const char* filePath = "path/to/your/Santa Tell Me.txt"; //Adjust the file path

	TEXTFILE dyTextFile;
	TEXTFILE* filePtr = (TEXTFILE*)malloc(sizeof(dyTextFile));

	if (!filePtr)
	{
		printf("Memory allocation failed.");
		return 1;
	}

	filePtr->fileName = (char*)malloc(strlen(argv[1] + 1));

	if (filePtr->fileName == NULL)
	{
		printf("Memory allocation for loading file name failed.");
		return 1;
	}

	strcpy(filePtr->fileName, argv[1]); //Suppose that we are going to handle the file named "SantaTellMe.txt"

	HANDLE hSerial = CreateFile(_T("COM1"), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (hSerial == INVALID_HANDLE_VALUE) {
		//Handle error

		return 1;
	}

	configureSerialPort(hSerial);

	loadFile(filePath, filePtr);

	sendTextFileContentOverSerial(hSerial, filePtr);

	CloseHandle(hSerial);

	return 0;
}


