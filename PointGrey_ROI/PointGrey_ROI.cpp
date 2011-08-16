// PointGrey_ROI.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <FlyCapture2.h>

using namespace FlyCapture2;

#define NUM_ARGS 6 // program name + 5 positional parameters

void PrintBuildInfo()
{
    FC2Version fc2Version;
    Utilities::GetLibraryVersion( &fc2Version );
    char version[128];
    sprintf( 
        version, 
        "FlyCapture2 library version: %d.%d.%d.%d\n", 
        fc2Version.major, fc2Version.minor, fc2Version.type, fc2Version.build );

    printf( "%s", version );

    char timeStamp[512];
    sprintf( timeStamp, "Application build date: %s %s\n\n", __DATE__, __TIME__ );

    printf( "%s", timeStamp );
}

void PrintCameraInfo( CameraInfo* pCamInfo )
{
    printf(
        "\n*** CAMERA INFORMATION ***\n"
        "Serial number - %u\n"
        "Camera model - %s\n"
        "Camera vendor - %s\n"
        "Sensor - %s\n"
        "Resolution - %s\n"
        "Firmware version - %s\n"
        "Firmware build time - %s\n\n",
        pCamInfo->serialNumber,
        pCamInfo->modelName,
        pCamInfo->vendorName,
        pCamInfo->sensorInfo,
        pCamInfo->sensorResolution,
        pCamInfo->firmwareVersion,
        pCamInfo->firmwareBuildTime );
}

void PrintFormat7Capabilities( Format7Info fmt7Info )
{
    printf(
        "Max image pixels: (%u, %u)\n"
        "Image Unit size: (%u, %u)\n"
        "Offset Unit size: (%u, %u)\n"
        "Pixel format bitfield: 0x%08x\n",
        fmt7Info.maxWidth,
        fmt7Info.maxHeight,
        fmt7Info.imageHStepSize,
        fmt7Info.imageVStepSize,
        fmt7Info.offsetHStepSize,
        fmt7Info.offsetVStepSize,
        fmt7Info.pixelFormatBitField );
}

void PrintError( Error error )
{
    error.PrintErrorTrace();
}

int main(int argc, char* argv[])
{
	if (argc != NUM_ARGS)
	{
		printf("Bad Parameter Count %d\n", argc);
		printf("Positional parameters: offsetX, offsetY, width, height, number of images.\n\n");
		return -1;
	}

    PrintBuildInfo();

    const Mode k_fmt7Mode = MODE_0; // set the desired imaging mode to Format7 - Mode0, i.e. custom imaging mode 0
    const PixelFormat k_fmt7PixFmt = PIXEL_FORMAT_MONO8; // set the desired pixel format to an 8 bit image
	const int k_numImages = atoi(argv[5]); // set the desired number of images
	//const int k_numImages = 5; // set the desired number of images

    Error error;

    // Since this application saves images in the current folder
    // we must ensure that we have permission to write to this folder.
    // If we do not have permission, fail right away.
	//FILE* tempFile = fopen("test.txt", "w+");
	//if (tempFile == NULL)
	//{
	//	printf("Failed to create file in current folder.  Please check permissions.\n");
	//	return -1;
	//}
	//fclose(tempFile);
	//remove("test.txt");

    BusManager busMgr;
    unsigned int numCameras;
    error = busMgr.GetNumOfCameras(&numCameras);
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    printf( "Number of cameras detected: %u\n", numCameras );

    if ( numCameras < 1 )
    {
        printf( "Insufficient number of cameras... exiting\n" );
        return -1;
    }

    PGRGuid guid;
    error = busMgr.GetCameraFromIndex(0, &guid);
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    Camera cam;

    // Connect to a camera
    error = cam.Connect(&guid);
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    // Get the camera information
    CameraInfo camInfo;
    error = cam.GetCameraInfo(&camInfo);
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    PrintCameraInfo(&camInfo);        

    // Query for available Format 7 modes
    Format7Info fmt7Info;
    bool supported;
    fmt7Info.mode = k_fmt7Mode;
	// tells you whether or not your desired imaging mode, mode 0, is supported by the camera's Format 7.
	// 'fmt7Info' gives you the capabilities of the specified mode and the current state in the specified mode.
	// 'supported' tells you whether the specified mode is supported.
    error = cam.GetFormat7Info( &fmt7Info, &supported ); 
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    PrintFormat7Capabilities( fmt7Info );

    if ( (k_fmt7PixFmt & fmt7Info.pixelFormatBitField) == 0 ) // tells you whether or not Mono 8, 8 bit depth, is supported
    {
        // Pixel format not supported!
		printf("Pixel format is not supported\n");
        return -1;
    }
    
	// Configure the desired image
    Format7ImageSettings fmt7ImageSettings;
	unsigned int offsetX_rounded, offsetY_rounded, width_rounded, height_rounded; // for 8 horizontal and 2 vertical px steps
    fmt7ImageSettings.mode = k_fmt7Mode;

	// the X offset must be a multiple of 2
	offsetX_rounded = atoi(argv[1]);
	if ((offsetX_rounded % 2) != 0)
		offsetX_rounded = offsetX_rounded + 1;
	fmt7ImageSettings.offsetX = offsetX_rounded; // horizontal offset from the top left corner
    //fmt7ImageSettings.offsetX = 200; // horizontal offset from the top left corner, original
   
	// the Y offset must be a multiple of 2
	offsetY_rounded = atoi(argv[2]);
	if ((offsetY_rounded % 2) != 0)
		offsetY_rounded = offsetY_rounded + 1;
	fmt7ImageSettings.offsetY = offsetY_rounded; // vertical offset from the top left corner
	//fmt7ImageSettings.offsetY = 244; // vertical offset from the top left corner, original
   
	// the width must be a multiple of 8
	int remainder;
	width_rounded = atoi(argv[3]) - offsetX_rounded;
	if ((remainder = (width_rounded % 8)) != 0)
	{
		if( remainder < 4 )
			width_rounded = width_rounded - remainder;
		else
			width_rounded = width_rounded + (8 - remainder);
	}
	fmt7ImageSettings.width = width_rounded; // x2-y2 entry
	//fmt7ImageSettings.width   = atoi(argv[3]); // width-height entry
	//fmt7ImageSettings.width   = 328; // fmt7Info.maxWidth, original
    
	// the height must be a multiple of 2
	height_rounded = atoi(argv[4]) - offsetY_rounded;
	if ((height_rounded % 2) != 0)
		height_rounded = height_rounded + 1;
	fmt7ImageSettings.height = height_rounded; // x2-y2 entry
	//fmt7ImageSettings.height  = atoi(argv[4]); // width-height entry
	//fmt7ImageSettings.height  = 244; // fmt7Info.maxHeight, original
   
	fmt7ImageSettings.pixelFormat = k_fmt7PixFmt;

    bool valid;
    Format7PacketInfo fmt7PacketInfo; // Packet size information that can be used to determine a valid packet size.

    // Validate the settings to make sure that they are valid
    error = cam.ValidateFormat7Settings(
        &fmt7ImageSettings,
        &valid,
        &fmt7PacketInfo );
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

	printf("ROI Width: %d\n", width_rounded);
	printf("ROI Height: %d\n", height_rounded); 

    if ( !valid )
    {
        // Settings are not valid
		printf("Format7 settings are not valid\n");
        return -1;
    }

    // Set the settings to the camera
    error = cam.SetFormat7Configuration(
        &fmt7ImageSettings,
		fmt7PacketInfo.maxBytesPerPacket ); // recommendedBytesPerPacket or maxBytesPerPacket

	printf("\nCurrent Bytes Per Packet: %d\n", fmt7PacketInfo.maxBytesPerPacket);

    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    // Start capturing images
    error = cam.StartCapture();
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    // Retrieve frame rate property
    Property frmRate;
    frmRate.type = FRAME_RATE;
    error = cam.GetProperty( &frmRate );
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    printf( "Frame rate is %3.2f fps\n", frmRate.absValue );

    printf( "Grabbing %d images\n", k_numImages );

    Image rawImage;    
    for ( int imageCount=0; imageCount < k_numImages; imageCount++ )
    {
        // Retrieve an image
        error = cam.RetrieveBuffer( &rawImage );
        if (error != PGRERROR_OK)
        {
            PrintError( error );
            continue;
        }

        printf( "." );

        // Get the raw image dimensions
        PixelFormat pixFormat;
        unsigned int rows, cols, stride;
        rawImage.GetDimensions( &rows, &cols, &stride, &pixFormat );

		// Crop the raw image here
		//
		//

        // Create a converted image
        Image convertedImage;

        // Convert the raw image
        error = rawImage.Convert( PIXEL_FORMAT_BGRU, &convertedImage );
        if (error != PGRERROR_OK)
        {
            PrintError( error );
            return -1;
        }  

        // Create a unique filename
        char filename[512];
        sprintf( filename, "..\\Camera_Output\\%u-%d.bmp", camInfo.serialNumber, imageCount );

        // Save the image. If a file format is not passed in, then the file
        // extension is parsed to attempt to determine the file format.
        error = convertedImage.Save( filename );
        if (error != PGRERROR_OK)
        {
            PrintError( error );
		    printf( "\nMake sure the directory 'Camera_Output' exists.\n" );
            return -1;
        }  
    }

    printf( "\nFinished grabbing images\n" );

    // Stop capturing images
    error = cam.StopCapture();
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }      

    // Disconnect the camera
    error = cam.Disconnect();
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    printf( "Done! Press Enter to exit...\n" );
    getchar();

	return 0;
}
