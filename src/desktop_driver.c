#include  "desktop_driver.h"


static HKEY
CreateDeviceKey(LPCTSTR szMpName)
{
	HKEY hKeyProfileMirror = (HKEY)0;
	if (RegCreateKey(
			HKEY_LOCAL_MACHINE,
			(MINIPORT_REGISTRY_PATH),
			&hKeyProfileMirror) != ERROR_SUCCESS){
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
struct desktop_driver * start_driver(){
    struct desktop_driver * driver = malloc(sizeof(*driver));
    ZeroMemory(&driver->display_dev, sizeof(DISPLAY_DEVICE));
    driver->display_dev.cb = sizeof(DISPLAY_DEVICE);
    driver->display_dev.cb = sizeof(DISPLAY_DEVICE);
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
        return NULL;
    }
    driver->desktop_rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    driver->desktop_rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    driver->desktop_rect.right = driver->desktop_rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    driver->desktop_rect.bottom = driver->desktop_rect.top+ GetSystemMetrics(SM_CYVIRTUALSCREEN);

    DEVMODE devmode;
    FillMemory(&devmode, sizeof(DEVMODE), 0);
    devmode.dmSize = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;
    BOOL change = EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    // we always have to set position or
    // a stale position info in registry would come into effect
    devmode.dmFields |= DM_POSITION;
    devmode.dmPosition.x = driver->desktop_rect.left;
    devmode.dmPosition.y = driver->desktop_rect.top;
    devmode.dmPelsWidth = driver->desktop_rect.right - driver->desktop_rect.left;
    devmode.dmPelsHeight = driver->desktop_rect.bottom - driver->desktop_rect.top;
    devmode.dmDeviceName[0] = '\0';
    HKEY hKeyDevice = CreateDeviceKey(MINI_PORT_NAME);
    if (hKeyDevice == NULL)
        return NULL;

    // TightVNC does not use these features
    RegDeleteValue(hKeyDevice, ("Screen.ForcedBpp"));
    RegDeleteValue(hKeyDevice, ("Pointer.Enabled"));
    DWORD dwVal = 3;
    // NOTE that old driver ignores it and mapping is always ON with it
    if (RegSetValueEx(
                hKeyDevice,
                ("Cap.DfbBackingMode"),0,REG_DWORD,(unsigned char *)&dwVal,4) != ERROR_SUCCESS){
        return NULL;
    }

    dwVal = 1;
    if (RegSetValueEx(hKeyDevice,("Order.BltCopyBits.Enabled"),0,REG_DWORD,(unsigned char *)&dwVal,4) != ERROR_SUCCESS){
        return NULL;
    }
    dwVal = 1;
    if (RegSetValueEx(hKeyDevice,("Attach.ToMyDesktop"),0,REG_DWORD,(unsigned char *)&dwVal,4) != ERROR_SUCCESS){
        return NULL;
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
        NULL;
    }

    // Reset MyDesktop
    SetThreadDesktop(hdeskCurrent);
    // Close the input MyDesktop
    CloseDesktop(hdeskInput);
    RegCloseKey(hKeyDevice);
    HDC gdc= CreateDC(driver->display_dev.DeviceName, NULL, NULL, NULL);
    if (gdc){
        goto error;
    }
    int drvCr = ExtEscape(gdc,DMF_ESCAPE_BASE_1_VB,0, NULL,sizeof(struct GETCHANGESBUF), (LPSTR) &driver->driver_buf);
    DeleteDC(gdc);
    if (drvCr <= 0){
        goto error;
    }
    return driver;
error:
    free(driver);
    return NULL;
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
        int pos  =  driver->driver_buf.buffer->pointrect+position%MAXCHANGES_BUF; //mod ringbuffer
        tmp = CreateRectRgnIndirect(driver->driver_buf.buffer->pointrect+pos);
        if (CombineRgn(region, region, tmp, RGN_OR) == NULLREGION)
             goto error;
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
error:
    DeleteObject(region);
    DeleteObject(tmp);
    free(list);
    return NULL;
}



