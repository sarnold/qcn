/*
 *  csensor_mac_usb_jw24f14.cpp
 *  qcn
 *
 *  Created by Carl Christensen on 08/11/2007.
 *  Copyright 2007 Stanford University.  All rights reserved.
 *
 * Implementation file for sensor classes
 */

#include "main.h"
#include "csensor_mac_usb_jw24f14.h"

// making sense of IOReturn (IOKit) error codes:  
// http://developer.apple.com/qa/qa2001/qa1075.html

// IOReturn codes in:
// /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/IOKit.framework/Headers/IOReturn.h

IOHIDDeviceInterface122** CSensorMacUSBJW24F14::CreateHIDDeviceInterface(io_object_t hidDevice)
{
    io_name_t             className;
    IOCFPlugInInterface** plugInInterface = NULL;
    HRESULT               plugInResult = S_OK;
    SInt32                score = 0;
    IOReturn              ioReturnValue = kIOReturnSuccess;
    IOHIDDeviceInterface122** pphidDeviceInterface = NULL;
	
    ioReturnValue = IOObjectGetClass(hidDevice, className);
    if (ioReturnValue != kIOReturnSuccess) {
        fprintf(stderr, "CreateHIDDeviceInterface: Failed to get class name.");
        return NULL;
    }
    ioReturnValue = IOCreatePlugInInterfaceForService (
         hidDevice, 
         kIOHIDDeviceUserClientTypeID,
         kIOCFPlugInInterfaceID, 
	 &plugInInterface, 
	 &score
    );
    if (ioReturnValue == kIOReturnSuccess) {
        // Call a method of the intermediate plug-in to create the device interface
        plugInResult = (*plugInInterface)->QueryInterface (plugInInterface,
                                CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID),
				(LPVOID *) &pphidDeviceInterface);
        if (plugInResult != S_OK) {
            fprintf(stderr, "CreateHIDDeviceInterface: Couldn't query HID class device interface from plugInInterface");
        }
        (*plugInInterface)->Release (plugInInterface);
    }
    else {
        fprintf(stderr, "CreateHIDDeviceInterface: Failed to create **plugInInterface via IOCreatePlugInInterfaceForService.");
        return NULL;
    }
    return pphidDeviceInterface;
}

CFMutableDictionaryRef CSensorMacUSBJW24F14::SetUpHIDMatchingDictionary (int inVendorID, int inDeviceID)
{
    CFMutableDictionaryRef 	refHIDMatchDictionary = NULL;
	
    // Set up a matching dictionary to search I/O Registry by class name for all IOWarrior devices.
    refHIDMatchDictionary = IOServiceMatching (kIOHIDDeviceKey);
    if (refHIDMatchDictionary != NULL) {
		CFNumberRef numberRef;
				
		numberRef = CFNumberCreate (kCFAllocatorDefault, kCFNumberIntType, &inVendorID);
		CFDictionarySetValue (refHIDMatchDictionary, CFSTR (kIOHIDVendorIDKey), numberRef);
		CFRelease (numberRef);

		numberRef = CFNumberCreate (kCFAllocatorDefault, kCFNumberIntType, &inDeviceID);
		CFDictionarySetValue (refHIDMatchDictionary, CFSTR (kIOHIDProductIDKey), numberRef);
		CFRelease (numberRef);

    }
    else {
        fprintf(stderr, "Failed to get HID CFMutableDictionaryRef via IOServiceMatching.");
    }
    return refHIDMatchDictionary;
}

// Returns an iterator object, which can be used to iterate through all hid devices available on the machine. 
// You have to release the iterator after usage be calling IOObjectRelease (hidObjectIterator).
io_iterator_t CSensorMacUSBJW24F14::FindHIDDevices (const mach_port_t masterPort, int inVendorID, int inDeviceID)
{
    CFMutableDictionaryRef	hidMatchDictionary = NULL;
    IOReturn				ioReturnValue = kIOReturnSuccess;
    io_iterator_t			hidObjectIterator;
	
    // Set up matching dictionary to search the I/O Registry for HID devices we are interested in. Dictionary reference is NULL if error.
    hidMatchDictionary = SetUpHIDMatchingDictionary (inVendorID, inDeviceID);
    if (NULL == hidMatchDictionary) {
        fprintf(stderr, "Couldn't create a matching dictionary.");
        return nil;
    }
	
    // Now search I/O Registry for matching devices.
    ioReturnValue = IOServiceGetMatchingServices (masterPort, hidMatchDictionary, &hidObjectIterator);
    // If error, print message and hang (for debugging purposes).
    if ((ioReturnValue != kIOReturnSuccess) || (!hidObjectIterator)) {
        return nil;
    }
	
    // IOServiceGetMatchingServices consumes a reference to the dictionary, so we don't need to release the dictionary ref.
    hidMatchDictionary = NULL;
	
    return hidObjectIterator;
}

CFMutableArrayRef CSensorMacUSBJW24F14::DiscoverHIDInterfaces(int vendorID, int deviceID)
{
	mach_port_t    masterPort = nil;
	io_iterator_t  hidObjectIterator = nil;
	IOReturn       ioReturnValue;
	CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault,0,nil);

	//ioReturnValue = IOMasterPort(bootstrap_port, &masterPort);
	ioReturnValue = IOMasterPort(MACH_PORT_NULL, &masterPort);
	if (masterPort == nil || ioReturnValue != kIOReturnSuccess) {
		fprintf(stderr, "DiscoverHIDInterfaces: Couldn't create a master I/O Kit Port.");
		return result;
	}
	hidObjectIterator = FindHIDDevices(masterPort, vendorID, deviceID); 
	if (hidObjectIterator) {
		io_object_t hidDevice = nil;
		IOReturn    ioReturnValue = kIOReturnSuccess;
		
		while ((hidDevice = IOIteratorNext(hidObjectIterator)))
		{
			kern_return_t             err;
			CFMutableDictionaryRef    properties = 0;
			IOHIDDeviceInterface122** hidInterface;
			CFNumberRef               hidInterfaceRef;
			
			err = IORegistryEntryCreateCFProperties (hidDevice, &properties, kCFAllocatorDefault, kNilOptions);
			hidInterface = CreateHIDDeviceInterface (hidDevice);
			
			hidInterfaceRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &hidInterface);
	
                    	CFArrayAppendValue(result,hidInterfaceRef);
			CFRelease (properties);
			ioReturnValue = IOObjectRelease(hidDevice);
            	}
	}
	IOObjectRelease (hidObjectIterator);
	mach_port_deallocate (mach_task_self(), masterPort);
	return result;
}


IOReturn CSensorMacUSBJW24F14::DisableCommandMode (IOHIDDeviceInterface122** hidInterface)
{
        UInt8 writeBuffer[8];
        IOReturn ioReturnValue;

        memset(writeBuffer, 0x00, sizeof (writeBuffer));
        writeBuffer[0] = 0x00;

        ioReturnValue = (*hidInterface)->setReport (hidInterface, kIOHIDReportTypeOutput, 0, writeBuffer, sizeof(writeBuffer), 0, NULL, NULL, NULL);
        if (ioReturnValue != kIOReturnSuccess)  {
           fprintf(stderr, "DisableCommandMode: Could not write setReport on hid device interface\n");
        }
        return ioReturnValue;
}


IOReturn CSensorMacUSBJW24F14::EnableCommandMode (IOHIDDeviceInterface122** hidInterface)
{

	UInt8	writeBuffer[8];
	IOReturn ioReturnValue;
	
	// enable Command mode
	bzero (writeBuffer, sizeof (writeBuffer));
	writeBuffer[0] = 0x80;
	
	ioReturnValue = (*hidInterface)->setReport (hidInterface, kIOHIDReportTypeOutput, 0, writeBuffer, sizeof(writeBuffer), 0, NULL, NULL, NULL);
        //if (ioReturnValue != kIOReturnSuccess) {
        //    fprintf(stderr, "EnableCommandMode: Could not write setReport on hid device interface\n");
        //} 
	return ioReturnValue;

}

IOReturn CSensorMacUSBJW24F14::ReadByteFromAddress (IOHIDDeviceInterface122** hidInterface, const UInt8 inAddress, UInt8 *result) // , bool bJoystick)
{

	UInt8	 readBuffer[8];
	UInt8	 writeBuffer[8];
	IOReturn ioReturnValue;
	UInt32   readDataSize;

	*result = 0x00;
	
	// open the interface
	//ioReturnValue = (*hidInterface)->open(hidInterface, kIOHIDOptionsTypeSeizeDevice);
	ioReturnValue = (*hidInterface)->open(hidInterface, kIOHIDOptionsTypeNone);
        if (ioReturnValue != kIOReturnSuccess) {
               fprintf(stderr, "ReadByteFromAddress: couldn't open interface 0x%x - err 0x%x\n",
                            (unsigned long) hidInterface, (unsigned long) ioReturnValue);
		return ioReturnValue;
	}
	
	if (kIOReturnSuccess != ( ioReturnValue = EnableCommandMode (hidInterface))) {
	   (*hidInterface)->close(hidInterface);
	    return ioReturnValue;
        }
	
	// enable command mode
	bzero (writeBuffer, sizeof (writeBuffer));
	writeBuffer[0] = 0x82;
	writeBuffer[1] = 0x80 | inAddress;
	
	ioReturnValue = (*hidInterface)->setReport (hidInterface, kIOHIDReportTypeOutput, 0, writeBuffer, sizeof(writeBuffer), 0, NULL, NULL, NULL);
        if (ioReturnValue != kIOReturnSuccess) {
           fprintf(stderr, "ReadByteFromAddress: Could not write setReport on hid device interface\n");
	   (*hidInterface)->close(hidInterface);
           return ioReturnValue;
        }  
	// read something from interface
	readDataSize = 8;
	ioReturnValue = (*hidInterface)->getReport (hidInterface, kIOHIDReportTypeInput,
		0, readBuffer, &readDataSize, 0, NULL, NULL, NULL);
        if (ioReturnValue != kIOReturnSuccess) {
           fprintf(stderr, "ReadByteFromAddress: Could not call getReport on hid device interface\n");
	   (*hidInterface)->close(hidInterface);
           return ioReturnValue;
	}
	*result = readBuffer[2] ;
	
	// disable command mode
	if (kIOReturnSuccess != ( ioReturnValue = DisableCommandMode (hidInterface))) {
	   (*hidInterface)->close(hidInterface);
           return ioReturnValue;
        }
	
	// close the interface
	ioReturnValue = (*hidInterface)->close(hidInterface);

	return ioReturnValue;

}


CSensorMacUSBJW24F14::CSensorMacUSBJW24F14()
  : CSensor()
{
   m_USBDevHandle[0] = m_USBDevHandle[1] = NULL;
   m_bFoundJW = false;
   m_maDeviceRef = NULL;
   m_bDevHandleOpen = false;
   closeHandles();
}

CSensorMacUSBJW24F14::~CSensorMacUSBJW24F14()
{
  closePort();
}

void CSensorMacUSBJW24F14::closePort()
{
  for (int i = 0; i < 2; i++) {
     if (m_USBDevHandle[i]) {
       try {
          // don't think we need the next line, just close & Release
          if (i==0) WriteData(m_USBDevHandle[0], 0x02, 0x00, 0x00, "closePort()::Free JW24F14");
          (*m_USBDevHandle[i])->close(m_USBDevHandle[i]);
          (*m_USBDevHandle[i])->Release(m_USBDevHandle[i]);
          m_USBDevHandle[i] = NULL;
        }
        catch(...) {
            fprintf(stderr, "Could not close JoyWarrior USB port %d...\n", i);
        }
     }
  }

  if (m_maDeviceRef) {
     CFRelease(m_maDeviceRef);
     m_maDeviceRef = NULL;
  }

  closeHandles();

  if (getPort() > -1) { // nothing really left to close, as it's just the joystick #
    fprintf(stdout, "Joywarrior 24F14 closed!\n");
    fflush(stdout);
    setPort(-1);
  }

}

void CSensorMacUSBJW24F14::closeHandles()
{
/*
      m_prdJW24F14 = NULL;
      m_prelJW24F14[0] = NULL;
      m_prelJW24F14[1] = NULL;
      m_prelJW24F14[2] = NULL;
 */
      closeDevHandle();
      m_bFoundJW = false;
      m_USBDevHandle[0] = NULL;
      m_USBDevHandle[1] = NULL;
      m_maDeviceRef = NULL;
}

bool CSensorMacUSBJW24F14::ReadData(IOHIDDeviceInterface122** hidInterface, const UInt8 addr, UInt8* cTemp, const char* strCallProc)
{
   *cTemp = 0x00;
   IOReturn ioReturnValue = ReadByteFromAddress(hidInterface, addr, cTemp);
   if (ioReturnValue != kIOReturnSuccess) {
       fprintf(stderr, "CSensorMacUSBJW24F14::ReadData():: couldn't open interface 0x%x - ioRetVal=0x%x  (sys=0x%x subsys=0x%x code=0x%x) - proc %s\n", 
         (unsigned long) hidInterface, (unsigned long) ioReturnValue, 
         err_get_system(ioReturnValue), err_get_sub(ioReturnValue), err_get_code(ioReturnValue),
         strCallProc ? strCallProc : "Unknown Proc");
       if (ioReturnValue == kIOReturnNotPrivileged) {
          fprintf(stderr, "CSensorMacUSBJW24F14::ReadData():: privilege violation 0x%x\n", (unsigned int) ioReturnValue);
       }
       return false;
   }
   return true;
}

bool CSensorMacUSBJW24F14::WriteData(IOHIDDeviceInterface122** hidInterface, const UInt8 cmd, const UInt8 addr, const UInt8 data, const char* strCallProc)
{
    UInt8	writeBuffer[8];
    IOReturn    ioReturnValue;
		
    // open the interface
    //ioReturnValue = (*hidInterface)->open(hidInterface, kIOHIDOptionsTypeSeizeDevice);
    ioReturnValue = (*hidInterface)->open(hidInterface, kIOHIDOptionsTypeNone);

    if (ioReturnValue != kIOReturnSuccess) {
		fprintf(stderr, "CSensorMacUSBJW24F14::WriteData():: couldn't open interface 0x%x - err 0x%x %s\n", 
                    (unsigned long) hidInterface, (unsigned long) ioReturnValue, strCallProc ? strCallProc : "Unknown Proc");
                (*hidInterface)->close(hidInterface);
		return false;
    }

    if (kIOReturnSuccess != ( ioReturnValue = EnableCommandMode (hidInterface))) {
                (*hidInterface)->close(hidInterface);
		return false; 
    }
	
    // write data
    memset(writeBuffer, 0x00, sizeof (writeBuffer));
    writeBuffer[0] = cmd;
    writeBuffer[1] = addr;
    writeBuffer[2] = data;

    ioReturnValue = (*hidInterface)->setReport (hidInterface, kIOHIDReportTypeOutput, 0, writeBuffer, sizeof(writeBuffer), 0, NULL, NULL, NULL);
    if (ioReturnValue != kIOReturnSuccess) {
       	   fprintf(stderr, "csensor_mac_ubs_jw.cpp:WriteData():: Could not write setReport on hid device interface retvalue=%d cmd=%d addr=%d data=%d\n", 
             ioReturnValue, cmd, addr, data);
           (*hidInterface)->close (hidInterface);
		return false;
    }  
	
    // disable command mode
    if (kIOReturnSuccess != ( ioReturnValue = DisableCommandMode(hidInterface))) {
                (*hidInterface)->close(hidInterface);
		return false;
    }
	
    // close the interface
    ioReturnValue = (*hidInterface)->close(hidInterface);
	
    return (ioReturnValue == kIOReturnSuccess);
}

/*
// for Apple HID Utilities -- walk the linked list of elements for a given USB device
void CSensorMacUSBJW24F14::printElement(const int level, const pRecElement pelem)
{
           fprintf(stdout, "%sCookie=0x%x  Min=%ld  Max=%ld  Size=%ld  UsagePg=%ld  Usage=%ld  Name=%s\n",
               level == 1 ? "      " : level == 2 ? "           " : "                ",
               (unsigned int) pelem->cookie,
               pelem->min,
               pelem->max,
               pelem->size,
               pelem->usagePage,
               pelem->usage,
               pelem->name
            );
}

bool CSensorMacUSBJW24F14::walkElement(const int level, const pRecElement pretmp)
{
     if (pretmp) {
        printElement(level, pretmp);

        if (m_bFoundJW)  {  // we're in the JW24F14 element we want? 
           switch ((int) pretmp->cookie)
           {
              case 0xb: 
              case 0xc: 
              case 0xd: 
                 m_prelJW24F14[(int) pretmp->cookie - 0xb] = pretmp;
                 // fprintf(stdout, "Found Cookie 0x%x\n", (int) pretmp->cookie);
           }
        }

        // walk the children, when null go to the siblings then their children
        walkElement(level+1, pretmp->pChild);
        walkElement(level, pretmp->pSibling);

        return true;
     }
     else {
        return false;
     }
}

*/


/*
// note this isn't part of CSensorMacUSBJW24F14::
void global_JoyWarriorAddedOrRemoved(void *refCon, io_iterator_t iterator)
{
    io_service_t            usbDevice;
    while ((usbDevice = IOIteratorNext(iterator)))
    {
        fprintf(stdout, "USB device added or removed\n");
        IOObjectRelease(usbDevice);
    }
    global_updateDeviceState();
}

void global_updateDeviceState()
{
    // delay so there is a timing error to force redetection
    fprintf(stdout, "JoyWarrior USB accelerometer removal detected!\n");
    fflush(stdout);
    boinc_sleep(2.5f);

}
*/

bool CSensorMacUSBJW24F14::openDevHandle()
{
    if (!m_USBDevHandle[0]) return false;  // handle isn't open yet
    if (m_bDevHandleOpen) closeDevHandle();

    IOReturn result = (*m_USBDevHandle[0])->open(m_USBDevHandle[0], kIOHIDOptionsTypeNone);
    if (result != kIOReturnSuccess) {
       m_bDevHandleOpen = false;
       fprintf(stderr, "CSensorMacUSBJW24F14::openDevHandle: couldn't open interface 0x%x - err 0x%x\n",
         (unsigned long) m_USBDevHandle[0], (unsigned long) result);
       return false;
    }
    m_bDevHandleOpen = true; // open was successful, set to true
    return true;
}

bool CSensorMacUSBJW24F14::closeDevHandle()
{
    m_bDevHandleOpen = false; // set our boolean to false
    if (!m_USBDevHandle[0]) return false;  // handle isn't open yet so can't use

    IOReturn result = (*m_USBDevHandle[0])->close(m_USBDevHandle[0]);
    if (result != kIOReturnSuccess) {
       fprintf(stderr, "CSensorMacUSBJW24F14::closeDevHandle: couldn't close interface 0x%x - err 0x%x\n",
         (unsigned long) m_USBDevHandle[0], (unsigned long) result);
       return false;
    }
    return true;
}

inline bool CSensorMacUSBJW24F14::read_xyz(float& x1, float& y1, float& z1)
{  
	/*
	// read LSB first (the order below will suffice)	
    const unsigned int ciNum = 9;
	UInt8 mReg[ciNum];
	memset(mReg, 0x00, sizeof(UInt8) * ciNum);
	for (unsigned int i = 0; i < ciNum; i++) {
		if (! ReadData(m_USBDevHandle[1], 0x00 + i, &mReg[i], "SetQCNState:R1")) {  // get current settings of device
			fprintf(stdout, "  * Could not read from JoyWarrior USB (SetQCNState:R1), exiting...\n");
			return false;
		}
	}
	 */
	/* 14 bits data e.g.
           	-2g == 10 0000 0000 0000
      -1.99975g == 10 0000 0000 0001
	  -0.00025g == 11 1111 1111 1111
	         0g == 00 0000 0000 0000
	  +0.00025g == 00 0000 0000 0001
	  +1.99950g == 01 1111 1111 1110
	  +1.99975g == 01 1111 1111 1111
	fprintf(stdout, "%x %x %x %x %x %x %x %x %x\n", 
			mReg[0],
			mReg[1],
			mReg[2],
			mReg[3],
			mReg[4],
			mReg[5],
			mReg[6],
			mReg[7],
			mReg[8]);

	
	 //                           0         1        2          3         4          5          6          7         8
	 // read values: from 0-8: chip id, version, acc_x_lsb, acc_x_msb, acc_y_lsb, acc_y_msb, acc_z_lsb, acc_z_msb, temp
	 
	 x1 = (float) mReg[3] / 128.0f;
	 y1 = (float) mReg[5] / 128.0f;
	 z1 = (float) mReg[7] / 128.0f;
	 
	*/
	 
	/*
        // CMC note -- this is the preferred way from codemercs.com but too slow for QCN -- have to use HID Joystick access
        UInt8 rawData[6];
        memset(rawData, 0x00, sizeof(UInt8) * 6);

        // open for direct joystick access (faster than using interface 1 which is via the accel chip controller)
	for (int i = 0; i < 6; i++)
        {
	    ReadByteFromAddress (m_USBDevHandle[0], 0x02 + i, &rawData[i]);
	}
	//x1 = CalcMsbLsb(rawData[0], rawData[1]);
        //y1 = CalcMsbLsb(rawData[2], rawData[3]);
        //z1 = CalcMsbLsb(rawData[4], rawData[5]);
		
		fprintf(stdout, "x=%f  y=%f  z=%f\n", x1, y1, z1);
        //	fflush(stdout);

        long lVal[3];
        for (int i = 0; i < 3; i++) { 
              lVal[i] = 512.0f * rand();
             // read each value from the cookie element in the USB device 
             //   - these were set in detect() by walking the linked list of the device
          //  lVal[i] = ::HIDGetElementValue(m_prdJW24F14, m_prelJW24F14[i]);
        }
	*/
	
    //static int iTestCtr = 0;  // static so we can detect every few seconds if USB stick is still plugged in
    IOReturn result = kIOReturnSuccess;
    IOHIDEventStruct hidEvent;
    float fVal[3];
	
#ifndef QCN_USB
    if (qcn_main::g_iStop) return false;
#endif
	
    /*
	 if (iTestCtr++ == 500) { // if DT=.02 this checks every 10 seconds that the JW is still plugged into USB port
	 iTestCtr = 0;  // reset counter
	 closeDevHandle();  // actually this doesn't seem to be working, probably need to close & re-detect?
	 }
	 */
	
    // major error if dev handle isn't open or can't be opened & read_xyz being called!
    if (!m_bDevHandleOpen && !openDevHandle()) { // this opens once at the start of reading to save CPU time (8%!)
		// but doesn't seem to detect if handle is bad i.e. when USB device is yanked out
		fprintf(stderr, "CSensorMacUSBJW::read_xyz: could not open Mac HID device handle!\n");
		return false; 
    }
	
    // cookies are 0xb for x-axis, 0xc for y-axis, 0xd for z-axis
    for (int i = 0; i < 3; i++) {
		fVal[i] = 0.0f;
		
		hidEvent.value = 0;
		hidEvent.longValueSize = 0;
		hidEvent.longValue = 0;
		
		result = (*m_USBDevHandle[0])->getElementValue(m_USBDevHandle[0], m_cookies.gAxisCookie[i], &hidEvent);
		// note that x/y/z should be scaled to +/- 2g, return values as +/- 2.0f*EARTH_G (in define.h: 9.78033 m/s^2)
		if (result == kIOReturnSuccess) fVal[i] = (float) hidEvent.value;
			//fVal[i]  = (((float) hidEvent.value - 32768.0f) / 32768.0f) * EARTH_G;
    }
	
    x1 = fVal[0]; y1 = fVal[1]; z1 = fVal[2];
	
	return true;
}

bool CSensorMacUSBJW24F14::detect()
{
   // first try and discover the HID interface (JoyWarrior)
   setType(SENSOR_NOTFOUND);  // initialize to no sensor until detected below
   closeHandles();  // reset the handles for JW24F14 detection
   
#ifndef QCN_USB
    if (qcn_main::g_iStop) return false;
#endif
	
   m_maDeviceRef = DiscoverHIDInterfaces(USB_VENDORID_JW, USB_DEVICEID_JW24F14); // from codemercs - inits the JW24F14 device in sys registry
   if (!m_maDeviceRef || CFArrayGetCount(m_maDeviceRef) < 2) { // not found, we'd have at least 2 interfaces for the JW24F14 USB
       closePort();
#ifdef _DEBUG
       fprintf(stdout, "No JoyWarrior USB device detected.\n");
#endif
       return false;
   }

   CFNumberRef interfaceRef = (CFNumberRef) CFArrayGetValueAtIndex(m_maDeviceRef, 0);
   CFNumberGetValue(interfaceRef, kCFNumberLongType, &m_USBDevHandle[0]);
   interfaceRef = (CFNumberRef) CFArrayGetValueAtIndex(m_maDeviceRef, 1);
   CFNumberGetValue(interfaceRef, kCFNumberLongType, &m_USBDevHandle[1]);
   CFRelease(interfaceRef);

   if (!m_USBDevHandle[0] || !m_USBDevHandle[1] || ! SetQCNState()) {
       // exit if this fails esp SetQCNState, can't communicate with JW24F14 properly...
       closePort();
       fprintf(stdout, "Could not setup JoyWarrior USB device.\n");
       return false;
   } 

	getHIDCookies(m_USBDevHandle[0], &m_cookies);

    // open port for read_xyz sequential reads...
    //(*m_USBDevHandle[0])->open(m_USBDevHandle[0], kIOHIDOptionsTypeSeizeDevice);
    //(*m_USBDevHandle[0])->open(m_USBDevHandle[0], kIOHIDOptionsTypeNone);

/*
   // start of using Mac HID Utilities
   // now use HID Utilities to open the JW24F14 USB sensor
   ::HIDBuildDeviceList(0, 0);

   pRecDevice newDevice = ::HIDGetFirstDevice();
   while (newDevice) { // search for the JoyWarrior "joystick" interface -- axis = 3 or inputs 11 (3 axis + 8 "buttons")
       // this joystick interface is much faster and fast enough for QCN (the other interface device is slow and can't keep up 50Hz)
       if (newDevice->vendorID == USB_VENDOR && newDevice->productID == USB_JOYWARRIOR) {
          if (newDevice->inputs == 11 || newDevice->axis == 3) { // this is the joystick interface (index 1)
             m_USBDevHandle[1] = (IOHIDDeviceInterface122**) newDevice->interface;
             m_prdJW24F14 = newDevice;
             m_bFoundJW = true;
             walkElement(1, m_prdJW24F14->pListElements);
             fprintf(stdout, "Found JW24F14 Joystick Interface at 0x%x\n", (unsigned int) m_USBDevHandle[1]);
          }
          else { // must be the accelerometer interface (index 0)
             m_USBDevHandle[0] = (IOHIDDeviceInterface122**) newDevice->interface;
             fprintf(stdout, "Found JW24F14 Accelerometer Interface at 0x%x\n", (unsigned int) m_USBDevHandle[0]);
          }
       }
       if (m_USBDevHandle[0] && m_USBDevHandle[1]) break; // found a JW24F14, break out of loop
       newDevice = ::HIDGetNextDevice(newDevice);
   }

   if (!m_USBDevHandle[0] || !m_USBDevHandle[1] || ! SetQCNState()) {
       // exit if this fails esp SetQCNState, can't communicate with JW24F14 properly...
       ::HIDReleaseDeviceList();  // cleanup HID devices
       return false;  // didn't find it
   } 

   fprintf(stdout, "JoyWarrior USB HID Detected - prDev 0x%x  XYZ = (0x%x, 0x%x, 0x%x)\n", 
         (unsigned int) m_prdJW24F14,
         (unsigned int) m_prelJW24F14[0],
         (unsigned int) m_prelJW24F14[1],
         (unsigned int) m_prelJW24F14[2]
   );
*/

/*
   CFMutableArrayRef interfaces, deviceProperties;
   CFNumberRef hidInterfaceRef[2];
   int iCount, iCountJW24F14;

   // JoyWarrior24 Force 8
   interfaces = (CFMutableArrayRef) ::DiscoverHIDInterfaces(USB_VENDOR, USB_JOYWARRIOR);
   iCountJW24F14 = ::CFArrayGetCount(interfaces);
   if (iCountJW24F14 < 2) { // should have 2 for JW24F14 USB
      return false;
   }

   // get the hidInterface refs for the member variables to the 0 & 1 interface (accelerometer & joystick interfaces)
   hidInterfaceRef[0] = (CFNumberRef) CFArrayGetValueAtIndex(interfaces, 0);
   CFNumberGetValue(hidInterfaceRef[0], kCFNumberLongType, &m_USBDevHandle[0]);
   hidInterfaceRef[1] = (CFNumberRef) CFArrayGetValueAtIndex(interfaces, 1);
   CFNumberGetValue(hidInterfaceRef[1], kCFNumberLongType, &m_USBDevHandle[1]);

   // get properties for the JW24F14 device (just to get the product name & serial number for now)
   deviceProperties = ::DiscoverHIDDeviceProperties(USB_VENDOR, USB_JOYWARRIOR);
   iCount = ::CFArrayGetCount(deviceProperties);
   CFStringRef cfstrProduct = NULL, cfstrSerial = NULL;
   if (iCount) {
        CFDictionaryRef properties = (CFDictionaryRef) ::CFArrayGetValueAtIndex(deviceProperties, 0);
        iCount = ::CFDictionaryGetCount(properties);
        if (iCount >= 2) {
          cfstrProduct = (CFStringRef) ::CFDictionaryGetValue(properties, CFSTR("Product"));
          cfstrSerial = (CFStringRef) ::CFDictionaryGetValue(properties, CFSTR("SerialNumber"));
          if (cfstrProduct && cfstrSerial) {
            fprintf(stdout, "Product %s  SerialNumber %s\n", 
                CFStringGetCStringPtr(cfstrProduct, NULL),
                CFStringGetCStringPtr(cfstrSerial, NULL)
            );
          }
        }
   }
   ::CFRelease(cfstrProduct);
   ::CFRelease(cfstrSerial);


   fprintf(stdout, "%d JoyWarrior Handles found: 0=0x%x 1=0x%x\n",  
                iCountJW24F14,
                (unsigned int) m_USBDevHandle[0],
                (unsigned int) m_USBDevHandle[1]
   );
*/

/*
   if (interfaces)
      ::CFRelease(interfaces);

   if (deviceProperties)
      ::CFRelease(deviceProperties);

    // close the current m_USBDevHandle's as we are going to use the Mac HID Utilties below for better speed    

    for (int i = 0; i < 2; i++) {
          if (m_USBDevHandle[i]) {
                WriteData(m_USBDevHandle[i], 0x02, 0x00, 0x00, "detect()::Free JW24F14");
                (*m_USBDevHandle[i])->close(m_USBDevHandle[i]);
                m_USBDevHandle[i] = NULL;
          }
    }

*/
	
//	bool CSensorMacUSBJW24F14::ReadData(IOHIDDeviceInterface122** hidInterface, const UInt8 addr, UInt8* cTemp, const char* strCallProc)

   // OK, we have a JoyWarrior USB sensor, and I/O is setup using Apple HID Utilities at 50Hz, +/- 2g
   setType(SENSOR_USB_JW24F14);
   setPort(getTypeEnum()); // set > -1 so we know we have a sensor
   //setSingleSampleDT(true);  // note the usb sensor just requires 1 sample per dt, hardware does the rest
   setSingleSampleDT(false);  // note the usb sensor just requires 1 sample per dt, hardware does the rest

/* CMC Note:  the USB add/remove device logic doesn't seem to work -- may need to revisit if this becomes important

   // CMC Note: the next two callbacks can be used to detect if the JW24F14 USB accelerometer was removed or added
   int result[2];
   if ((result[0] = ::AddUSBDeviceAddedCallback(USB_VENDOR, USB_JOYWARRIOR, global_JoyWarriorAddedOrRemoved)) == -1) {
     fprintf(stderr, "Could not add USB AddedCallback()\n");
   }

   if ((result[1] = ::AddUSBDeviceRemovedCallback(USB_VENDOR, USB_JOYWARRIOR, global_JoyWarriorAddedOrRemoved)) == -1) {
     fprintf(stderr, "Could not add USB RemovedCallback()\n");
   }
  
   fprintf(stdout, "AddUSBDeviceAddedCallback=%d  AddUSBDeviceRemovedCallback=%d\n", result[0], result[1]);
   fflush(stdout);
*/
    fprintf(stdout, "Joywarrior 24F14 opened!\n");

    return (bool)(getTypeEnum() == SENSOR_USB_JW24F14);
}

bool CSensorMacUSBJW24F14::SetQCNState()
{ // puts the Joystick Warrior USB sensor into the proper state for QCN (50Hz, +/- 2g)
  // and also writes these settings to EEPROM (so each device needs to just get set once hopefully)
	
	/*
	
   UInt8 mReg14 = 0x00;
   if (! ReadData(m_USBDevHandle[1], 0x14, &mReg14, "SetQCNState:R1")) {  // get current settings of device
       fprintf(stdout, "  * Could not read from JoyWarrior USB (SetQCNState:R1), exiting...\n");
       return false;
   }

   // if not set already, set it to +/-2g accel (0x00) and 50Hz internal bandwidth 0x01
   // NB: 0x08 & 0x10 means accel is set to 4 or 8g, if not bit-and with 0x01 bandwidth is other than 50Hz

   if ((mReg14 & 0x08) || (mReg14 & 0x10) || ((mReg14 & 0x01) != 0x01)) {
        fprintf(stdout, "Setting JoyWarrior 24F14 USB to QCN standard 50Hz sample rate, +/- 2g\n");

        UInt8 uiTmp = 0x00;
        if (! ReadData(m_USBDevHandle[1], 0x14, &uiTmp, "SetQCNState:R2") ) {
           fprintf(stdout, "  * Could not read from JoyWarrior 24F14 USB (SetQCNState:R2), exiting...\n");
           return false;
        }

        mReg14 = 0x01 | (uiTmp & 0xE0);

        // write settings to register
        if (! WriteData(m_USBDevHandle[1], 0x82, 0x14, mReg14, "SetQCNState:W1")) {
           fprintf(stdout, "  * Could not write to JoyWarrior 24F14 USB (SetQCNState:W1), exiting...\n");
           return false;
        }

        // write settings to EEPROM for persistent state
        if (! WriteData(m_USBDevHandle[1], 0x82, 0x0A, 0x10, "SetQCNState:W2")) {  // start EEPROM write
           fprintf(stdout, "  * Could not write to JoyWarrior 24F14 USB (SetQCNState:W2), exiting...\n");
           return false;
        }
        boinc_sleep(.050f);
        if (! WriteData(m_USBDevHandle[1], 0x82, 0x34, mReg14, "SetQCNState:W3")) {
           fprintf(stdout, "  * Could not write to JoyWarrior 24F14 USB (SetQCNState:W3), exiting...\n");
           return false;
        }
        boinc_sleep(.050f);
        if (! WriteData(m_USBDevHandle[1], 0x82, 0x0A, 0x02, "SetQCNState:W4")) {  // end EEPROM write
           fprintf(stdout, "  * Could not write to JoyWarrior 24F14 USB (SetQCNState:W4), exiting...\n");
           return false;
        }
        boinc_sleep(.100f);
   } 
	 */
/*
   else {
      fprintf(stdout, "JoyWarrior 24F14 USB already set to QCN standard 50Hz sample rate, +/- 2g\n");
   }
*/
   return true;
}

bool CSensorMacUSBJW24F14::getHIDCookies(IOHIDDeviceInterface122** handle, cookie_struct_t cookies)
{
	
	CFTypeRef                               object;
	long                                    number;
	IOHIDElementCookie                      cookie;
	long                                    usage;
	long                                    usagePage;
	CFArrayRef                              elements; //
	CFDictionaryRef                         element;
	IOReturn                                success;
	
	memset(cookies, 0x00, sizeof(struct cookie_struct));
	
	if (!handle || !(*handle)) return false;
	
	// Copy all elements, since we're grabbing most of the elements
	// for this device anyway, and thus, it's faster to iterate them
	// ourselves. When grabbing only one or two elements, a matching
	// dictionary should be passed in here instead of NULL.
	success = (*handle)->copyMatchingElements(handle, NULL, &elements);
	
	if (success == kIOReturnSuccess) {
		CFIndex i;
		//printf("ITERATING...\n");
		for (i=0; i<CFArrayGetCount(elements); i++)
		{
			element = (CFDictionaryRef) CFArrayGetValueAtIndex(elements, i);
			// printf("GOT ELEMENT.\n");
			
			//Get cookie
			object = (CFDictionaryGetValue(element,
										   CFSTR(kIOHIDElementCookieKey)));
			if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
				continue;
			if(!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType,
								 &number))
				continue;
			cookie = (IOHIDElementCookie) number;
			
			//Get usage
			object = CFDictionaryGetValue(element, CFSTR(kIOHIDElementUsageKey));
			if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
				continue;
			if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType,
								  &number))
				continue;
			usage = number;
			
			//Get usage page
			object = CFDictionaryGetValue(element,
										  CFSTR(kIOHIDElementUsagePageKey));
			if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
				continue;
			if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType,
								  &number))
				continue;
			usagePage = number;
			
			//Check for x axis
			if (usage == USB_COOKIE_X_JW24F14 && usagePage == 1)
				cookies->gAxisCookie[0] = cookie;
			//Check for y axis
			else if (usage == USB_COOKIE_Y_JW24F14 && usagePage == 1)
				cookies->gAxisCookie[1] = cookie;
			//Check for z axis
			else if (usage == USB_COOKIE_Z_JW24F14 && usagePage == 1)
				cookies->gAxisCookie[2] = cookie;
			//Check for buttons
			else if (usage == 1 && usagePage == 9)
				cookies->gButtonCookie[0] = cookie;
			else if (usage == 2 && usagePage == 9)
				cookies->gButtonCookie[1] = cookie;
			else if (usage == 3 && usagePage == 9)
				cookies->gButtonCookie[2] = cookie;
		}
		/*
		 fprintf(stdout, "JoyWarrior HID Cookies for X/Y/Z axes = (0x%x, 0x%x, 0x%x)\n", 
		 (unsigned int) cookies->gAxisCookie[0],
		 (unsigned int) cookies->gAxisCookie[1],
		 (unsigned int) cookies->gAxisCookie[2]
		 );
		 */
		 
	}
	else {
		fprintf(stderr, "copyMatchingElements failed with error %d\n", success);
	}
	
	return true;
}



/*
// Calculate a 10 bit value with MSB and LSB
short CSensorMacUSBJW24F14::CalcMsbLsb(unsigned char lsb, unsigned char msb)
{
	short erg;
	short LSB, MSB, EXEC;

	EXEC = (msb & 0x80) << 8;
	EXEC = EXEC & 0x8000;

	// Calculate negative value
	if(EXEC & 0x8000)
		EXEC = EXEC | 0x7C00;

	MSB = msb << 2;
	MSB = MSB & 0x03FC;
	LSB = (lsb & 0xC0) >> 6;
	LSB = LSB & 0x0003;

	erg = MSB | LSB | EXEC;

	return erg;
}

*/