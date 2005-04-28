
//#include <crtdbg.h>
#include "stdafx.h"
#include "BlissApp.h"

#include "BlissMainFrame.h"
#include "core/rip/Rip.h"

//#define BLISS_WINDOW_STYLE WS_OVERLAPPED | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
#define BLISS_WINDOW_STYLE WS_OVERLAPPEDWINDOW
#define BLISS_FULLSCREEN_STYLE WS_POPUP

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(BlissMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(BlissMainFrame, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_RESET, OnFileReset)
    ON_UPDATE_COMMAND_UI(ID_FILE_RESET, OnCheckMenuItems)
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
    ON_UPDATE_COMMAND_UI(ID_FILE_CLOSE, OnCheckMenuItems)
	ON_COMMAND(ID_SETTINGS, OnSettings)
	ON_COMMAND(ID_APP_EXIT, OnClose)
    ON_COMMAND(ID_VIEW_FULLSCREENMODE, OnFullScreenMode)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FULLSCREENMODE, OnCheckMenuItems)
    ON_WM_NCHITTEST()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_ENTERMENULOOP()
    ON_WM_EXITMENULOOP()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()


BlissMainFrame::BlissMainFrame()
  : direct3d(NULL),
	direct3dDevice(NULL),
	directSound(NULL),
	directSoundBuffer(NULL),
    manager(NULL),
    running(FALSE),
    currentEmu(NULL),
    currentRip(NULL),
    openDialog(NULL),
    optionsDialog(NULL),
    menuInFullscreen(TRUE)
{
    //configure the presentation parameters
	ZeroMemory(&presentParams, sizeof(presentParams));
	presentParams.Windowed = TRUE;
    presentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
    presentParams.EnableAutoDepthStencil = false;
    presentParams.BackBufferCount = 1;
    presentParams.BackBufferFormat = D3DFMT_UNKNOWN;
    presentParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    presentParams.BackBufferWidth = 640;
    presentParams.BackBufferHeight = 480;
}

BOOL BlissMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;

	//Modify the Window class or styles here by modifying the CREATESTRUCT cs
    cs.style = BLISS_WINDOW_STYLE;
    cs.dwExStyle = 0;
	//cs.lpszClass = AfxRegisterWndClass(0);

    //load the accelerators
    accel = LoadAccelerators(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_ACCELERATOR));
    
    return TRUE;
}

int BlissMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

    //set up our menu items
    this->SetWindowPos(NULL, 0, 0, 800, 600, SWP_NOOWNERZORDER | SWP_NOMOVE);

    //load the configuration
    OnStartUp();
    
	return 0;
}

BOOL BlissMainFrame::PreTranslateMessage(MSG* pMsg) 
{ 
    if (WM_KEYFIRST <= pMsg->message && 
        pMsg->message <= WM_KEYLAST) 
    { 
        if (TranslateAccelerator(m_hWnd, accel, pMsg)) 
            return TRUE; 
    } 
    return CFrameWnd::PreTranslateMessage(pMsg); 
}

void BlissMainFrame::OnCheckMenuItems(CCmdUI* pCmdUI)
{
    switch (pCmdUI->m_nID) {
        case ID_FILE_RESET:
        case ID_FILE_CLOSE:
            pCmdUI->Enable(running);
            break;
        case ID_VIEW_FULLSCREENMODE:
            pCmdUI->SetCheck(!presentParams.Windowed);
            break;
    }
}

void BlissMainFrame::OnDestroy()
{
    //save the configuration
    ShutDown();

    //destroy instance data
    if (optionsDialog) {
        delete optionsDialog;
        optionsDialog = NULL;
    }
    if (openDialog) {
        delete openDialog;
        openDialog = NULL;
    }
    if (currentRip) {
        delete currentRip;
        currentRip = NULL;
    }
    if (manager) {
        manager->releaseInputProducers();
        delete manager;
        manager = NULL;
    }
    if (directSoundBuffer) {
        directSoundBuffer->Release();
        directSoundBuffer = NULL;
    }
    if (directSound) {
        directSound->Release();
        directSound = NULL;
    }
    if (direct3dDevice) {
        direct3dDevice->Release();
        direct3dDevice = NULL;
    }
    if (direct3d) {
        direct3d->Release();
        direct3d = NULL;
    }
    //_ASSERT(_CrtCheckMemory());
}

void BlissMainFrame::OnPaint()
{
    CPaintDC dc(this);
    RECT r;
    GetClientRect(&r);
    CBrush b(RGB(0,0,0));
	dc.FillRect(&r, &b);
    /*
    if (!presentParams.Windowed && menuInFullscreen)
        DrawMenuBar();
    */
}

void BlissMainFrame::OnFileOpen()
{
    if (openDialog == NULL) {
        openDialog = new CFileDialog(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING,
            "All Files (*.*)|*.*|Emulation Rips (*.rip)|*.rip|Intellivision ROMs (*.rom)|*.rom|Raw ROM Images (*.bin,*.int)|*.bin;*.int|Zipped Raw ROM Images (*.zip)|*.zip||",
            this, 0);
    }

    if (running)
        directSoundBuffer->SetVolume(DSBVOLUME_MIN);
    if (openDialog->DoModal() == IDCANCEL) {
        if (running)
            directSoundBuffer->SetVolume(DSBVOLUME_MAX);
        return;
    }
    if (running)
        directSoundBuffer->SetVolume(DSBVOLUME_MAX);

    LoadAndRunRip(openDialog->GetPathName());
}

void BlissMainFrame::LoadAndRunRip(const CHAR* filename)
{
    if (running) {
        //the run method is already in progress, so release the current emu and rip
        ReleaseEmulatorInputs();
        ReleaseEmulator();
        if (currentRip) {
            delete currentRip;
            currentRip = NULL;
        }
    }

    //try to load the new rip
    if (!LoadRip(filename)) {
        MessageBox("Unable to load the specified file.", "Error", MB_OK);
        running = FALSE;
        return;
    }

    const CHAR* name = currentRip->GetName();
    if (name == NULL) {
        SetWindowText("Bliss");
    }
    else {
        CHAR* title = new CHAR[strlen(name) + strlen(" - Bliss") + 1];
        strcpy(title, name);
        strcat(title, " - Bliss");
        SetWindowText(title);
        delete[] title;
    }

    if (running) {
        //the run method is already in progress, so just re-initialize it and return
        if (!InitializeEmulator())
            running = FALSE;
        InitializeEmulatorInputs();
    }
    else {
        if (!InitializeEmulator())
            return;
        InitializeEmulatorInputs();
        Run();
        ReleaseEmulatorInputs();
        ReleaseEmulator();
        SetWindowText("Bliss");
        if (currentRip) {
            delete currentRip;
            currentRip = NULL;
        }
    }
}

BOOL BlissMainFrame::LoadRip(const CHAR* filename)
{
    if (strlen(filename) < 5)
        return FALSE;

    //convert .bin and .rom to .rip, since our emulation only knows how to run .rip
    //TODO: filename.MakeLower();
    const CHAR* extStart = filename + strlen(filename) - 4;
    if (strcmpi(extStart, ".int") == 0 || strcmpi(extStart, ".bin") == 0)
    {
        //load the bin file as a Rip
        CHAR* cfgFilename = new CHAR[strlen(theApp.StartUpPath) + strlen("knowncarts.cfg") + 2];
        strcpy(cfgFilename, theApp.StartUpPath);
        strcat(cfgFilename, "\\knowncarts.cfg");
        currentRip = Rip::LoadBin(filename, cfgFilename);
        delete[] cfgFilename;
        if (currentRip == NULL)
            return FALSE;

        CHAR fileSubname[MAX_PATH];
        CHAR* filenameStart = strrchr(filename, '\\')+1;
        strncpy(fileSubname, filenameStart, strlen(filenameStart)-4);
        *(fileSubname+strlen(filenameStart)-4) = NULL;
        SaveRip(fileSubname);
    }
    else if (strcmpi(extStart, ".rom") == 0)
    {
        //load the rom file as a Rip
        currentRip = Rip::LoadRom(filename);
        if (currentRip == NULL)
            return FALSE;

        CHAR fileSubname[MAX_PATH];
        CHAR* filenameStart = strrchr(filename, '\\')+1;
        strncpy(fileSubname, filenameStart, strlen(filenameStart)-4);
        *(fileSubname+strlen(filenameStart)-4) = NULL;
        SaveRip(fileSubname);
    }
    else if (strcmpi(extStart, ".zip") == 0)
    {
        //load the zip file as a Rip
        CHAR* cfgFilename = new CHAR[strlen(theApp.StartUpPath) + strlen("knowncarts.cfg") + 2];
        strcpy(cfgFilename, theApp.StartUpPath);
        strcat(cfgFilename, "\\knowncarts.cfg");
        currentRip = Rip::LoadZip(filename, cfgFilename);
        if (currentRip == NULL)
            return FALSE;

        CHAR fileSubname[MAX_PATH];
        CHAR* filenameStart = strrchr(filename, '\\')+1;
        strncpy(fileSubname, filenameStart, strlen(filenameStart)-4);
        *(fileSubname+strlen(filenameStart)-4) = NULL;
        SaveRip(fileSubname);
    }
    else {
        //load the designated Rip
        currentRip = Rip::LoadRip(filename);
        if (currentRip == NULL)
            return FALSE;
    }

    return TRUE;
}

BOOL BlissMainFrame::SaveRip(const CHAR* fileSubname)
{
    //save the Rip to the "rips" subdirectory
    CHAR newFileName[MAX_PATH];

    strcpy(newFileName, theApp.StartUpPath);
    strcat(newFileName, "\\rips");
    //stop here to make sure our "rips" subdirectory actually exists
    CFileStatus status;
    CFileFind fndFile;
    if (fndFile.FindFile(newFileName)) {
        //file exists; ensure it is a directory
        fndFile.FindNextFile();
        if (!fndFile.IsDirectory())
            return FALSE;
    }
    //directory does not exist; try to create it
    else if (!CreateDirectory(newFileName, NULL))
        return FALSE;
    strcat(newFileName, "\\");
    strcat(newFileName, fileSubname);
    strcat(newFileName, ".rip");
	if (fndFile.FindFile(newFileName))
        CFile::Remove(newFileName);

    currentRip->SaveRip(newFileName);
    return TRUE;
}

void BlissMainFrame::OnFileReset()
{
    if (!running || !currentEmu || !currentRip)
        return;

    currentEmu->Reset();
}

void BlissMainFrame::OnFileClose()
{
    //if (!presentParams.Windowed)
    //    UnlockWindowUpdate();
    running = FALSE;
    this->RedrawWindow();
}

void BlissMainFrame::OnSettings()
{
    if (!optionsDialog) {
        optionsDialog = new BlissOptionsDialog(this);
    }

    if (running)
        directSoundBuffer->SetVolume(DSBVOLUME_MIN);
    if (optionsDialog->DoModal() == IDCANCEL) {
        if (running)
            directSoundBuffer->SetVolume(DSBVOLUME_MAX);
        return;
    }
    if (running)
        directSoundBuffer->SetVolume(DSBVOLUME_MAX);

    if (running) {
        ReleaseEmulatorInputs();
        InitializeEmulatorInputs();
    }
}

void BlissMainFrame::OnClose()
{
    if (running) {
        OnFileClose();
        PostMessage(WM_CLOSE);
    }
    else
        CFrameWnd::OnClose();
}

void BlissMainFrame::Run()
{
	MSG msg; 

    //as long as the user has not selected the close or exit menu items, keep truckin'
    directSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
    running = TRUE;
	while(running) {
        //poll the input
		manager->pollInputProducers();

        //run the emulation
		currentEmu->Run();

		//render and display the video
		direct3dDevice->BeginScene();
		currentEmu->Render();
		direct3dDevice->EndScene();
		direct3dDevice->Present(NULL, NULL, NULL, NULL);
        /*
        if (!presentParams.Windowed && menuInFullscreen)
            DrawMenuBar();
        */

        //flush the audio
        currentEmu->FlushAudio();

        //check for messages
		while (running && PeekMessage(&msg, m_hWnd,  0, 0, PM_REMOVE))  {
            if (PreTranslateMessage(&msg))
                continue;

			TranslateMessage(&msg); 
			DispatchMessage(&msg); 
		} 

        //make sure we still have the d3d device
        if (!CheckDevice())
            running = FALSE;
	}
    directSoundBuffer->Stop();
}

HRESULT BlissMainFrame::InitializeDirect3D()
{
    ReleaseDirect3D();

    //initialize Direct3D
	direct3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (direct3d == NULL)
        return D3DERR_NOTAVAILABLE;

    if (!presentParams.Windowed) {
        GetWindowPlacement(&wp);
        ModifyStyle(m_hWnd, BLISS_WINDOW_STYLE, BLISS_FULLSCREEN_STYLE, SWP_DRAWFRAME | SWP_FRAMECHANGED);
    }

	HRESULT res = direct3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &presentParams, &direct3dDevice);
    if (res != D3D_OK)
        return res;

    return res;
}

HRESULT BlissMainFrame::InitializeDirectSound()
{
    //release any currently held DirectSound resources
    ReleaseDirectSound();

    //initialize the DirectSound device
    DirectSoundCreate8(NULL, &directSound, NULL);
	HRESULT res;
    if ((res = directSound->SetCooperativeLevel(m_hWnd, DSSCL_PRIORITY)) != DS_OK)
		return res;

    //only support single-channel, 16-bit, PCM sound
    WAVEFORMATEX wfx;
	ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    //configure the sample rate
	wfx.nSamplesPerSec = 22050;
    wfx.nChannels = 1;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    //the length of the buffer must be divisible by 4 for implementation reasons
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec *(wfx.wBitsPerSample/8);
    DSBUFFERDESC bdesc;
	ZeroMemory(&bdesc, sizeof(bdesc));
	bdesc.dwSize = sizeof(DSBUFFERDESC);
	bdesc.lpwfxFormat = &wfx;
    bdesc.dwFlags = DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY |
        DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS;

	int ms = 200;
    bdesc.dwBufferBytes = ((wfx.nAvgBytesPerSec*ms)/1000) & 0x7FFFFFFC;

    //finally, create the actual buffer
	IDirectSoundBuffer* buffer;
	if ((res = directSound->CreateSoundBuffer(&bdesc, &buffer, NULL)) != DS_OK)
		return res;

	res = buffer->QueryInterface(IID_IDirectSoundBuffer8, (void**)&directSoundBuffer);
    buffer->Release();

    return DS_OK;
}

void BlissMainFrame::InitializeDirectInput()
{
    //release any currently held DirectInput resources
    ReleaseDirectInput();
    
    manager = new InputProducerManager(m_hWnd);
}

BOOL BlissMainFrame::InitializeEmulator()
{
    //release the current emulator
    ReleaseEmulator();

    //find the currentEmulator required to run this RIP
	currentEmu = Emulator::GetEmulatorByID(currentRip->GetTargetSystemID());
    if (currentEmu == NULL) {
        MessageBox("Unable to find an appropriate emulation driver for\nthe specified file.", "Error", MB_OK);
        return FALSE;
    }
    
    //load the BIOS files required for this currentEmulator
    if (!LoadPeripheralRoms(currentEmu))
        return FALSE;

    //load peripheral roms
    INT32 count = currentEmu->GetPeripheralCount();
    for (INT32 i = 0; i < count; i++) {
        Peripheral* p = currentEmu->GetPeripheral(i);
        PeripheralCompatibility usage = currentRip->GetPeripheralUsage(p->GetShortName());
        if (usage == PERIPH_INCOMPATIBLE || usage == PERIPH_COMPATIBLE) {
            currentEmu->UsePeripheral(i, FALSE);
            continue;
        }

        BOOL loaded = LoadPeripheralRoms(p);
        if (loaded) {
            //peripheral loaded, might as well use it.
            currentEmu->UsePeripheral(i, TRUE);
        }
        else if (usage == PERIPH_OPTIONAL) {
            //didn't load, but the peripheral is optional, so just skip it
            currentEmu->UsePeripheral(i, FALSE);
        }
        else //usage == PERIPH_REQUIRED, but it didn't load 
            return FALSE;
    }
    
    //put the RIP in the currentEmulator
    //currentEmu.AudioMixer.HighQualityMixing = config.Get("Audio.HighQualityMixing", "True") == "True" ? true : false;
    currentEmu->InitVideo(direct3dDevice);
    currentEmu->InitAudio(directSoundBuffer);
    currentEmu->SetRip(currentRip);
    
    //finally, run everything
    currentEmu->Reset();

    return TRUE;
}

void BlissMainFrame::InitializeEmulatorInputs()
{
    ReleaseEmulatorInputs();

    InitializePeripheralInputs(currentEmu);
    UINT32 count = currentEmu->GetPeripheralCount();
    for (UINT32 i = 0; i < count; i++) {
        InitializePeripheralInputs(currentEmu->GetPeripheral(i));
    }
}

void BlissMainFrame::InitializePeripheralInputs(Peripheral* periph)
{
    //iterate through all the emulated input consumers in the current emulator.
    //these consumers represent the emulated joysticks, keyboards, etc. that were
    //originally used to provide input to the emulated system
    UINT16 count = periph->GetInputConsumerCount();
    for (UINT16 i = 0; i < count; i++) {
        InputConsumer* nextInputConsumer = periph->GetInputConsumer(i);
        //iterate through each object on this consumer (buttons, keys, etc.)
        int iccount = nextInputConsumer->getInputConsumerObjectCount();
        for (int j = 0; j < iccount; j++) {
            InputConsumerObject* nextObject = nextInputConsumer->getInputConsumerObject(j);

            //find the producer to bind with this object
            GUID g = nextObject->getDefaultDeviceGuid();
            CHAR* key = new CHAR[strlen(nextInputConsumer->getName()) + strlen(nextObject->getName()) + 13];
            strcpy(key, nextInputConsumer->getName());
            strcat(key, ".");
            strcat(key, nextObject->getName());
            strcat(key, ".DeviceGUID");
            UINT8* buffer;
            UINT32 size;
            theApp.GetProfileBinary("Input", key, &buffer, &size);
            delete[] key;
            if (buffer != NULL) {
                if (size == sizeof(GUID))
                    memcpy(&g, buffer, sizeof(GUID));
                delete[] buffer;
            }
            InputProducer* producer = manager->acquireInputProducer(g);
            if (producer == NULL) {
                nextObject->setDeviceInput(NULL, -1);
                continue;
            }

            //now get the input enum associated with this object
            INT32 e = nextObject->getEnum();
            if (e < 0 || e >= producer->getInputCount()) { 
                e = nextObject->getDefaultEnum();
                CHAR* key = new CHAR[strlen(nextInputConsumer->getName()) + strlen(nextObject->getName()) + 12];
                strcpy(key, nextInputConsumer->getName());
                strcat(key, ".");
                strcat(key, nextObject->getName());
                strcat(key, ".InputEnum");
                e = theApp.GetProfileInt("Input", key, e);
                delete[] key;
                if (e < 0 || e >= producer->getInputCount()) {
                    nextObject->setDeviceInput(NULL, -1);
                    continue;
                }
            }

            //attach the desired producer enum to the consumer object
            nextObject->setDeviceInput(producer, e);
        }
    }
}

void BlissMainFrame::ReleaseEmulatorInputs()
{
    if (!currentEmu)
        return;

    ReleasePeripheralInputs(currentEmu);
    UINT32 count = currentEmu->GetPeripheralCount();
    for (UINT32 i = 0; i < count; i++) {
        ReleasePeripheralInputs(currentEmu->GetPeripheral(i));
    }
}

void BlissMainFrame::ReleasePeripheralInputs(Peripheral* periph)
{
    UINT16 count = periph->GetInputConsumerCount();
    for (UINT16 i = 0; i < count; i++) {
        InputConsumer* nextInputConsumer = periph->GetInputConsumer(i);
        //iterate through each object on this consumer (buttons, keys, etc.)
        int iccount = nextInputConsumer->getInputConsumerObjectCount();
        for (int j = 0; j < iccount; j++) {
            InputConsumerObject* nextObject = nextInputConsumer->getInputConsumerObject(j);
            nextObject->setDeviceInput(NULL, -1);
        }
    }
}

void BlissMainFrame::ReleaseEmulator()
{
    //release the emulator
    if (currentEmu) {
        currentEmu->SetRip(NULL);
        currentEmu->ReleaseAudio();
        currentEmu->ReleaseVideo();
        currentEmu = NULL;
    }

    //repaint the window back to normal
    this->RedrawWindow();
}

void BlissMainFrame::ReleaseDirect3D()
{
    if (direct3dDevice != NULL) {
        direct3dDevice->Release();
        direct3dDevice = NULL;
    }
    if (direct3d != NULL) {
        direct3d->Release();
        direct3d = NULL;
    }
}

void BlissMainFrame::ReleaseDirectSound()
{
    //release existing devices
    if (directSoundBuffer != NULL) {
        directSoundBuffer->Release();
        directSoundBuffer = NULL;
    }
    if (directSound != NULL) {
        directSound->Release();
        directSound = NULL;
    }
}

void BlissMainFrame::ReleaseDirectInput()
{
    if (manager) {
        manager->releaseInputProducers();
        delete manager;
        manager = NULL;
    }
}

BOOL BlissMainFrame::LoadPeripheralRoms(Peripheral* peripheral)
{
	UINT16 count = peripheral->GetROMCount();
	for (UINT16 i = 0; i < count; i++) {
		ROM* r = peripheral->GetROM(i);
		if (r->isLoaded())
			continue;

		//TODO: get filenames from config file
		//TODO: handle file loading errors
        CHAR nextFile[MAX_PATH];
        strcpy(nextFile, theApp.StartUpPath);
        strcat(nextFile, "\\");
        strcat(nextFile, r->getDefaultFileName());
        if (!r->load(nextFile, r->getDefaultFileOffset())) {
            CHAR error[MAX_PATH];
            strcpy(error, "Unable to load '");
            strcat(error, r->getDefaultFileName());
            strcat(error, "'");
            MessageBox(error, "Error");
            return FALSE;
        }
	}

    return TRUE;
}

LRESULT BlissMainFrame::OnStartUp()
{
    presentParams.hDeviceWindow = m_hWnd;

    //initialize directx stuff
    HRESULT res;
	if ((res = InitializeDirect3D()) != D3D_OK) {
		MessageBox("Unable to initialize Direct3D.", "Error", MB_OK);
		return ERROR;
	}
	if ((res = InitializeDirectSound()) != DS_OK) {
        ReleaseDirect3D();
		MessageBox("Unable to initialize DirectSound.", "Error", MB_OK);
		return ERROR;
	}
    //TODO: handle direct input initialization errors
    InitializeDirectInput();

    return OK;
}

void BlissMainFrame::OnFullScreenMode()
{
    SetFullScreen(presentParams.Windowed);
}

void BlissMainFrame::SetFullScreen(BOOL fullScreen)
{
    if (presentParams.Windowed == !fullScreen)
        return;

    presentParams.Windowed = !fullScreen;

    if (!presentParams.Windowed) {
        GetWindowPlacement(&wp);
        ModifyStyle(m_hWnd, BLISS_WINDOW_STYLE, BLISS_FULLSCREEN_STYLE, SWP_DRAWFRAME | SWP_FRAMECHANGED);
    }

    //BOOL tmp = presentParams.Windowed;
	//presentParams.Windowed = TRUE;
    if (currentEmu)
        currentEmu->ReleaseVideo();

    if (direct3dDevice->Reset(&presentParams) != D3D_OK) {
		MessageBox("Unable to initialize Direct3D.", "Error", MB_OK);
		return;
    }

    if (currentEmu)
        currentEmu->InitVideo(direct3dDevice);

    if (presentParams.Windowed) {
        ModifyStyle(m_hWnd, BLISS_FULLSCREEN_STYLE, BLISS_WINDOW_STYLE, SWP_DRAWFRAME | SWP_FRAMECHANGED);
        SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPlacement(&wp);
    }
}

BOOL BlissMainFrame::CheckDevice()
{
    HRESULT res = direct3dDevice->TestCooperativeLevel();
    if (res == D3D_OK)
        return TRUE;

    if (currentEmu)
        currentEmu->ReleaseVideo();

    if (res == D3DERR_DEVICENOTRESET) {
        if (direct3dDevice->Reset(&presentParams) != D3D_OK)
            //attempt more drastic measures
            res = D3DERR_DEVICELOST;
    }

    if (res == D3DERR_DEVICELOST) {
        ReleaseDirect3D();
        HRESULT res;
	    if ((res = InitializeDirect3D()) != D3D_OK) {
		    MessageBox("Lost Direct3D Device.", "Error", MB_OK);
		    return FALSE;
	    }
    }

    if (currentEmu)
        currentEmu->InitVideo(direct3dDevice);

    return TRUE;
}

afx_msg UINT BlissMainFrame::OnNcHitTest(CPoint point)
{
    if (!presentParams.Windowed)
        return HTCLIENT;

    return CFrameWnd::OnNcHitTest(point);
}

void BlissMainFrame::ShutDown()
{
    ReleaseDirectInput();
    ReleaseDirectSound();
    ReleaseDirect3D();
}

void BlissMainFrame::OnEnterMenuLoop(BOOL)
{
    if (running)
        directSoundBuffer->SetVolume(DSBVOLUME_MIN);
}

void BlissMainFrame::OnExitMenuLoop(BOOL)
{
    if (running)
        directSoundBuffer->SetVolume(DSBVOLUME_MAX);
}

void BlissMainFrame::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE && !presentParams.Windowed)
        OnFullScreenMode();

    CFrameWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

#ifdef _DEBUG
void BlissMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void BlissMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}
#endif //_DEBUG
