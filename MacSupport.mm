/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2016 Janus Lynggaard Thorborg (www.jthorborg.com)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	See \licenses\ for additional details on licenses associated with this program.

**************************************************************************************
 
	 file:MacSupport.mm
	 
		Objective-C code.
 
 *************************************************************************************/

// i dont know why. i seriously dont.
// but all hell breaks loose if this is not here.
//#define Point CarbonDummyPointName

// Include JUCE headers first to avoid OpenGL conflicts
#include "Common.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/NSString.h>
#import <AppKit/NSPanel.h>
#import <AppKit/AppKit.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/graphics/IOGraphicsLib.h>

#include "MacSupport.h"
#include "Misc.h"
#include "Exceptions.h"

#include <string.h>
/*********************************************************************************************
 
	Spawns a messagebox using NSRunAlertPanel
 
 *********************************************************************************************/

int MacBox(void * hwndParent, const char *text, const char *caption, int type)
{
	using namespace cpl::Misc;
	int ret=0;
	
	NSString *tit=(NSString *)CFStringCreateWithCString(NULL,caption?caption:"",kCFStringEncodingASCII);
	NSString *text2=(NSString *)CFStringCreateWithCString(NULL,text?text:"",kCFStringEncodingASCII);
	
	if ((type & 0xF) == sYesNoCancel)
	{
		ret = (int)NSRunAlertPanel(tit, @"%@", @"Yes", @"No", @"Cancel",text2);
		switch(ret)
		{
			case -1:
				ret = bCancel;
				break;
			case 0:
				ret = bNo;
				break;
			case 1:
				ret = bYes;
				break;
			case 2:
				ret = bCancel;
				break;
		};
	}
	else if ((type & 0xF) == sYesNo)
	{
		ret = (int)NSRunAlertPanel(tit, @"%@", @"Yes", @"No", @"",text2);
		switch(ret)
		{
			case -1:
				ret = bCancel;
				break;
			case 0:
				ret = bNo;
				break;
			case 1:
				ret = bYes;
				break;
		};
	}
	else if ((type & 0xF) == sConTryCancel)
	{
		ret = (int)NSRunAlertPanel(tit,@"%@",@"Continue",@"Try Again",@"Cancel",text2);
		switch(ret)
		{
			case -1:
				ret = bCancel;
				break;
			case 0:
				ret = bTryAgain;
				break;
			case 1:
				ret = bContinue;
				break;
			case 2:
				ret = bCancel;
				break;
		};
	}
	else if ((type & 0xF) == sOk)
	{
		NSRunAlertPanel(tit,@"%@",@"OK",@"",@"",text2);
		ret= bOk;
	}
	
	[text2 release];
	[tit release];
	
	return ret;
}

/*********************************************************************************************
 
	Reurns the path of our bundle.
	Broken? If called, causes segv fault in another thread at objc_release after autoreleasepoolpage()
 
 *********************************************************************************************/
std::size_t GetBundlePath(char * buf, std::size_t bufSize)
{
	CPL_NOTIMPLEMENTED_EXCEPTION();
}

int GetSystemFontSmoothingLevel()
{
	NSUserDefaults * dict = [NSUserDefaults standardUserDefaults];
	return static_cast<int>([dict integerForKey:@"AppleFontSmoothing"]);
	
}

double GetScreenRotation()
{
	/*
	* @field scalerFlags If the mode is scaled,
	*    kIOScaleStretchToFit may be set to allow stretching.
	*    kIOScaleRotateFlags is mask which may have the value given by kIOScaleRotate90, kIOScaleRotate180, kIOScaleRotate270 to display a rotated framebuffer.
	*/
	return 0.0;
	
}

double GetSystemGamma();

bool GetExtendedScreenInfo(long x, long y, OSXExtendedScreenInfo * info)
{
	if(!info)
		return false;
	auto baseLineY = [[NSScreen mainScreen] frame].size.height;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	{
		//NSScreen *defaultScreen = [[NSScreen screens] objectAtIndex:0];
		for (NSScreen * screen in [NSScreen screens])
		{
			
			NSRect rect = [screen frame];
			rect.origin.y = (baseLineY - rect.size.height) - rect.origin.y;
			// y-coordinates in OSX start from bottom and go up; it is reversed in juce.
			
			// check if the given x, y point in inside this screen
			if((x >= rect.origin.x && x < rect.origin.x + rect.size.width) &&
			   (y >= rect.origin.y && y < rect.origin.y + rect.size.height))
				
			{
				//https://developer.apple.com/library/mac/documentation/GraphicsImaging/Reference/Quartz_Services_Ref/index.html#//apple_ref/c/func/CGGetDisplaysWithPoint
				CGDirectDisplayID displayId = [[[screen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
				//https://developer.apple.com/library/mac/documentation/GraphicsImaging/Reference/Quartz_Services_Ref/index.html#//apple_ref/c/func/CGDisplayRotation
				// https://github.com/glfw/glfw/issues/165#issuecomment-30515700
				io_service_t iodisplay = CGDisplayIOServicePort(displayId);
				
				NSDictionary * infoDictionary = (NSDictionary *)IODisplayCreateInfoDictionary(iodisplay, kIODisplayOnlyPreferredName);
				unsigned int displaySubpixelLayout = [[infoDictionary objectForKey : @kDisplaySubPixelLayout] unsignedIntValue];
				//CGGetDisplayTransferByFormula
				float redGamma = [[infoDictionary objectForKey : @kDisplayRedGamma] floatValue];
				float greenGamma = [[infoDictionary objectForKey : @kDisplayGreenGamma] floatValue];
				float blueGamma = [[infoDictionary objectForKey : @kDisplayBlueGamma] floatValue];
				bool displayIsDigital = [[infoDictionary objectForKey: @"IODisplayIsDigital"] boolValue];
				[infoDictionary release];
				//https://developer.apple.com/library/mac/documentation/GraphicsImaging/Reference/Quartz_Services_Ref/index.html#//apple_ref/c/func/CGDisplayRotation
				info->screenRotation = CGDisplayRotation(displayId);
				info->subpixelOrientation = displaySubpixelLayout;
				info->fontSmoothingLevel = GetSystemFontSmoothingLevel();
				info->redGamma = redGamma;
				info->greenGamma = greenGamma;
				info->blueGamma = blueGamma;
				info->averageGamma = redGamma + blueGamma + greenGamma;
				info->averageGamma /= 3;
				info->displayIsDigital = displayIsDigital;
				
				return true;
			}
			
		}
		[pool release];
	}
	return false;
}

unsigned int GetSubpixelOrientationForScreen(long x, long y)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	{
		//NSScreen *defaultScreen = [[NSScreen screens] objectAtIndex:0];
		for (NSScreen * screen in [NSScreen screens])
		{

			NSRect rect = [screen visibleFrame];
			
			// check if the given x, y point in inside this screen
			if((x >= rect.origin.x && x < rect.origin.x + rect.size.width) &&
				(y >= rect.origin.y && y < rect.origin.y + rect.size.height))
				
			{
				CGDirectDisplayID displayId = [[[screen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
				// https://github.com/glfw/glfw/issues/165#issuecomment-30515700
				io_service_t iodisplay = CGDisplayIOServicePort(displayId);
				
				NSDictionary * infoDictionary = (NSDictionary *)IODisplayCreateInfoDictionary(iodisplay, kIODisplayOnlyPreferredName);
				unsigned int displaySubpixelLayout = [[infoDictionary objectForKey : @kDisplaySubPixelLayout] unsignedIntValue];
				[infoDictionary release];
				return displaySubpixelLayout;
				break;
			}
		}
		[pool release];
	}
	return kDisplaySubPixelLayoutUndefined;
}

