#include  "desktop_driver.h"
#include <stdio.h>
#include "DisplayEsc.h"

#define DRIVER_STRING "Mirage Driver"
#define DRIVER_STRING_ALT "DemoForge Mirage Driver"
#define MINI_PORT_NAME "dfmirage"
#define MINIPORT_REGISTRY_PATH	"SYSTEM\\CurrentControlSet\\Hardware Profiles\\Current\\System\\CurrentControlSet\\Services"


// bytes per row = width*bits_per_pixel/8
struct desktop_screen {
	int width;
	int height;
	int bits_per_pixel; // how many bits pet pipex,default 32 bit for mirror dirver 
};

struct  desktop_driver {
    char name[30];
    struct GETCHANGESBUF  driver_buf;
    struct desktop_screen  desktop_screen;
    DISPLAY_DEVICE  display_dev;
};

static HKEY  CreateDeviceKey(LPCTSTR name);
static int stop_driver(struct desktop_driver * driver);
//static  int  map_dirver_buffer(struct GETCHANGESBUF * buf);
static HKEY
CreateDeviceKey(LPCTSTR szMpName){
    HKEY hKeyProfileMirror = (HKEY)0;
    if (RegCreateKey(HKEY_LOCAL_MACHINE, (MINIPORT_REGISTRY_PATH),&hKeyProfileMirror) != ERROR_SUCCESS){
        return FALSE;
    }
    HKEY hKeyProfileMp = (HKEY)0;
    LONG cr = RegCreateKey(hKeyProfileMirror,szMpName,&hKeyProfileMp);
    RegCloseKey(hKeyProfileMirror);
    if (cr != ERROR_SUCCESS){
    return FALSE;
    }
    HKEY hKeyDevice = (HKEY)0;
    if (RegCreateKey(hKeyProfileMp,("DEVICE0"),&hKeyDevice) != ERROR_SUCCESS){
    }
    RegCloseKey(hKeyProfileMp);
    return hKeyDevice;
}
struct desktop_driver * alloc_driver(){
	struct desktop_driver * driver = malloc(sizeof(*driver));
	if(driver == NULL){
		return NULL;
	}
    ZeroMemory(&driver->display_dev, sizeof(DISPLAY_DEVICE));
    driver->display_dev.cb = sizeof(DISPLAY_DEVICE);
	return driver;
}

void free_driver(struct desktop_driver * driver){
	if(driver)
		free(driver);
}

int start_driver(struct desktop_driver * driver){
    BOOL result;
    int num =0;
    while (result=EnumDisplayDevicesA(NULL,num,&driver->display_dev, 0)){
        if (strcmp((const char *)driver->display_dev.DeviceString, DRIVER_STRING) == 0 ||
                strcmp((const char *)driver->display_dev.DeviceString, DRIVER_STRING_ALT) == 0) {
                break;
            }
        num++;
    }
    if(result == 0){
        return -1;
    }
	
	HDC gdc1= CreateDC(driver->display_dev.DeviceName, NULL, NULL, NULL);
	int tmp = ExtEscape(gdc1, dmf_esc_test, 0, NULL, 0, NULL);
	DeleteDC(gdc1);
	
	if (tmp>0){
		//driver not close normally,stop it
		stop_driver(driver);
	}
    driver->desktop_screen.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    driver->desktop_screen.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	driver->desktop_screen.bits_per_pixel = 32 ;
	
	
    DEVMODE devmode;
    FillMemory(&devmode, sizeof(DEVMODE), 0);
    devmode.dmSize = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;
    BOOL change = EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    // we always have to set position or
    // a stale position info in registry would come into effect
    devmode.dmFields |= DM_POSITION;
    //devmode.dmPosition.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    //devmode.dmPosition.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    //devmode.dmPelsWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    //devmode.dmPelsHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);;
    devmode.dmDeviceName[0] = '\0';
    HKEY hKeyDevice = CreateDeviceKey(MINI_PORT_NAME);
    if (hKeyDevice == NULL)
        return -1;
    // TightVNC does not use these features
    RegDeleteValue(hKeyDevice, ("Screen.ForcedBpp"));
    RegDeleteValue(hKeyDevice, ("Pointer.Enabled"));
    DWORD dwVal = 3;
    // NOTE that old driver ignores it and mapping is always ON with it
    if (RegSetValueEx(hKeyDevice,("Cap.DfbBackingMode"),0,REG_DWORD,(unsigned char *)&dwVal,4) != ERROR_SUCCESS){
        return -1;
    }
    dwVal = 1;
    if (RegSetValueEx(hKeyDevice,("Order.BltCopyBits.Enabled"),0,REG_DWORD,(unsigned char *)&dwVal,4) != ERROR_SUCCESS){
        return -1;
    }
    dwVal = 1;
    if (RegSetValueEx(hKeyDevice,("Attach.ToMyDesktop"),0,REG_DWORD,(unsigned char *)&dwVal,4) != ERROR_SUCCESS){
        return -1;
    }
    HDESK   hdeskInput;
    HDESK   hdeskCurrent;
    // Save the current MyDesktop
    hdeskCurrent = GetThreadDesktop(GetCurrentThreadId());
    if (hdeskCurrent != NULL){
        hdeskInput = OpenInputDesktop(0, FALSE, MAXIMUM_ALLOWED);
        if (hdeskInput != NULL)
            SetThreadDesktop(hdeskInput);
    }
    // 24 bpp screen mode is MUNGED to 32 bpp.
    // the underlying buffer format must be 32 bpp.
    // see MyDesktop::ThunkBitmapInfo()
    if (devmode.dmBitsPerPel==24) devmode.dmBitsPerPel = 32;
    LONG cr = ChangeDisplaySettingsExA((TCHAR *)driver->display_dev.DeviceName,&devmode,NULL,CDS_UPDATEREGISTRY,NULL);
    if (cr != DISP_CHANGE_SUCCESSFUL){
        goto error;
    }

	
    // Reset MyDesktop
    SetThreadDesktop(hdeskCurrent);
    // Close the input MyDesktop
    CloseDesktop(hdeskInput);
    RegCloseKey(hKeyDevice);
	
    HDC gdc= CreateDC(driver->display_dev.DeviceName, NULL, NULL, NULL);
    if (gdc == NULL){
		printf("get last error %d",GetLastError());
        goto error;
    }

    //map buffer
	
    int drvCr = ExtEscape(gdc,dmf_esc_usm_pipe_map,0, NULL,sizeof(struct GETCHANGESBUF), (LPSTR) &(driver->driver_buf));
    DeleteDC(gdc);
    if (drvCr <= 0){
        goto error;
    }
    return 0;
error:
    return -1;
}


struct driver_rect_list * get_rect_list(struct desktop_driver * driver,int position){
    ULONG snapshot = driver->driver_buf.buffer->counter;
    if (position == snapshot){
        return NULL;
    }
    if(snapshot < position){
        snapshot += MAXCHANGES_BUF;
    }
    HRGN region,tmp;
    region = CreateRectRgnIndirect(driver->driver_buf.buffer->pointrect+position);
    position++;
    while(position < snapshot){
        int pos  =  position%MAXCHANGES_BUF; //mod ringbuffer
        tmp = CreateRectRgnIndirect(driver->driver_buf.buffer->pointrect+pos);
        if (CombineRgn(region, region, tmp, RGN_OR) == NULLREGION)
             goto error;
		position++;
    }

    int buffsize;
    DWORD x;
    RGNDATA *buff;
    // Get the size of buffer required
    buffsize = GetRegionData(region, NULL, 0);
    buff = (RGNDATA *) malloc(buffsize);
    if (buff == NULL)
        return NULL;
    struct driver_rect_list * list = malloc(sizeof(*list));
    if (list == NULL)
        return NULL;
    // Now get the region data
    if (GetRegionData(region, buffsize, buff)){
        for (x=0; x<(buff->rdh.nCount); x++){
            // Obtain the rectangles from the list
            int m=sizeof(RGNDATAHEADER);
            int n=sizeof(RECT);
            RECT *rect = (RECT *) (((BYTE *) buff) + sizeof(RGNDATAHEADER) + x * sizeof(RECT));
            struct  driver_rect  * desk = malloc(sizeof(* desk));
            if(desk == NULL)
                goto error;
            desk->left = rect->left;
            desk->right = rect->right;
            desk->top = rect->top;
            desk->bottom = rect->bottom;
            if(x==0){
                list->head = desk;
                list->tail = desk;
            } else {
                list->tail->next = desk;
                list->tail = desk;
            }
        }
    }
    list->position = snapshot;
    list->buf = driver->driver_buf.Userbuffer;
	return list;
error:
    DeleteObject(region);
    DeleteObject(tmp);
    free(list);
    return NULL;
}

int free_rect_list(struct driver_rect_list * list){
	if(list){
		struct  driver_rect  * desk;
		while(desk = list->head){
			list->head = desk->next;
			free(desk);
		}
		free(list);
	}

}
int stop_free_driver(struct desktop_driver * driver){
	stop_driver(driver);
	free_driver(driver);
}
static 
int stop_driver(struct desktop_driver * driver){
    DEVMODE devmode;
    FillMemory(&devmode, sizeof(DEVMODE), 0);
    devmode.dmSize = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;
    BOOL change = EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    devmode.dmDeviceName[0] = '\0';
    HKEY hKeyDevice = CreateDeviceKey(MINI_PORT_NAME);
    if (hKeyDevice == NULL)
        return -1;

    int dwVal = 0;
    if (RegSetValueEx(hKeyDevice,("Attach.ToMyDesktop"),0,REG_DWORD,(unsigned char *)&dwVal,4) != ERROR_SUCCESS){
       ;
    }
     // reverting to default behavior
    RegDeleteValue(hKeyDevice, ("Cap.DfbBackingMode"));
    RegDeleteValue(hKeyDevice, ("Order.BltCopyBits.Enabled"));

    HDESK   hdeskInput;
    HDESK   hdeskCurrent;
    // Save the current MyDesktop
    hdeskCurrent = GetThreadDesktop(GetCurrentThreadId());
    if (hdeskCurrent != NULL){
        hdeskInput = OpenInputDesktop(0, FALSE, MAXIMUM_ALLOWED);
        if (hdeskInput != NULL)
            SetThreadDesktop(hdeskInput);
    }
    // 24 bpp screen mode is MUNGED to 32 bpp.
    // the underlying buffer format must be 32 bpp.
    // see MyDesktop::ThunkBitmapInfo()
    if (devmode.dmBitsPerPel==24) devmode.dmBitsPerPel = 32;
    LONG cr = ChangeDisplaySettingsExA((TCHAR *)driver->display_dev.DeviceName,&devmode,NULL,CDS_UPDATEREGISTRY,NULL);
  
    // Reset MyDesktop
    SetThreadDesktop(hdeskCurrent);
    // Close the input MyDesktop
    CloseDesktop(hdeskInput);
    RegCloseKey(hKeyDevice);
    HDC gdc= CreateDC(driver->display_dev.DeviceName, NULL, NULL, NULL);
    if (gdc==NULL){
        goto error;
    }
    //unmap buffer
    int drvCr = ExtEscape(gdc,dmf_esc_usm_pipe_unmap,sizeof(struct GETCHANGESBUF), (LPSTR) &driver->driver_buf,0,NULL);
    DeleteDC(gdc);
	// 0 return value is unlikely for Mirage because its DC is independent
	// from the reference device;
	// this happens with Quasar if its mode was changed externally.
	// nothing is particularly bad with it.

	if (drvCr <= 0){
		if (driver->driver_buf.buffer){
			UnmapViewOfFile(driver->driver_buf.buffer);
		}
		if (driver->driver_buf.Userbuffer){
			UnmapViewOfFile(driver->driver_buf.Userbuffer);
		}
	}
	
    return 0;
error:
    return -1;
}



