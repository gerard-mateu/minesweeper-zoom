#include <windows.h>
#include <windowsx.h>
#include <tlhelp32.h>
#include <wchar.h>

// ============================================================
// CONFIGURACIÓN
// ============================================================

int g_zoom = 2;

HWND g_minesweeper = NULL;
HWND g_zoomWindow = NULL;

HBITMAP g_bitmap = NULL;
HDC g_memDC = NULL;

bool g_leftButtonDown = false;
bool g_rightButtonDown = false;
bool g_faceButtonDown = false;
int originalX = 0;
int originalY = 0;

int g_mouseDownX = 0;
int g_mouseDownY = 0;

int g_srcWidth = 0;
int g_srcHeight = 0;

// ============================================================
// BUSCAR PROCESO
// ============================================================

DWORD FindProcessId(const wchar_t* processName)
{
    HANDLE snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0);

    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    DWORD result = 0;

    if (Process32FirstW(snapshot, &pe))
    {
        do
        {
            if (_wcsicmp(
                    pe.szExeFile,
                    processName) == 0)
            {
                result = pe.th32ProcessID;
                break;
            }

        } while (Process32NextW(snapshot, &pe));
    }

    CloseHandle(snapshot);

    return result;
}


// ============================================================
// BUSCAR VENTANA
// ============================================================

struct FindWindowData
{
    DWORD pid;
    HWND hwnd;
};

BOOL CALLBACK EnumWindowsProc(
    HWND hwnd,
    LPARAM lParam)
{
    FindWindowData* data =
        reinterpret_cast<FindWindowData*>(lParam);

    DWORD pid = 0;

    GetWindowThreadProcessId(
        hwnd,
        &pid);

    if (pid == data->pid &&
        IsWindowVisible(hwnd))
    {
        data->hwnd = hwnd;
        return FALSE;
    }

    return TRUE;
}


HWND FindMinesweeperWindow()
{
    DWORD pid =
        FindProcessId(L"winmine.exe");

    if (!pid)
        return NULL;

    FindWindowData data;

    data.pid = pid;
    data.hwnd = NULL;

    EnumWindows(
        EnumWindowsProc,
        reinterpret_cast<LPARAM>(&data));

    return data.hwnd;
}

// ============================================================
// TAMAÑO DEL BUSCAMINAS
// ============================================================

bool GetMinesweeperClientSize()
{
    if (!g_minesweeper)
        return false;

    RECT rc;

    if (!GetClientRect(
            g_minesweeper,
            &rc))
        return false;

    g_srcWidth =
        rc.right - rc.left;

    g_srcHeight =
        rc.bottom - rc.top;

    return
        g_srcWidth > 0 &&
        g_srcHeight > 0;
}


// ============================================================
// CAPTURA
// ============================================================

bool CaptureMinesweeper()
{
    if (!g_minesweeper)
        return false;

    if (!GetMinesweeperClientSize())
        return false;

    HDC sourceDC =
        GetDC(g_minesweeper);

    if (!sourceDC)
        return false;

    HDC newDC =
        CreateCompatibleDC(sourceDC);

    if (!newDC)
    {
        ReleaseDC(
            g_minesweeper,
            sourceDC);

        return false;
    }

    HBITMAP newBitmap =
        CreateCompatibleBitmap(
            sourceDC,
            g_srcWidth,
            g_srcHeight);

    if (!newBitmap)
    {
        DeleteDC(newDC);

        ReleaseDC(
            g_minesweeper,
            sourceDC);

        return false;
    }

    HBITMAP oldBitmap =
        (HBITMAP)SelectObject(
            newDC,
            newBitmap);

    // Forzar que Winmine actualice su contenido
    // RedrawWindow(
    //     g_minesweeper,
    //     NULL,
    //     NULL,
    //     RDW_INVALIDATE |
    //     RDW_ERASE |
    //     RDW_UPDATENOW |
    //     RDW_ALLCHILDREN);

    // // Forzar procesamiento de mensajes pendientes
    // UpdateWindow(
    //     g_minesweeper);

    BOOL success =
        PrintWindow(
            g_minesweeper,
            newDC,
            PW_CLIENTONLY);

    if (!success)
    {
        BitBlt(
            newDC,
            0,
            0,
            g_srcWidth,
            g_srcHeight,
            sourceDC,
            0,
            0,
            SRCCOPY);
    }

    SelectObject(
        newDC,
        oldBitmap);

    if (g_bitmap)
        DeleteObject(g_bitmap);

    if (g_memDC)
        DeleteDC(g_memDC);

    g_bitmap = newBitmap;
    g_memDC = newDC;

    ReleaseDC(
        g_minesweeper,
        sourceDC);

    return true;
}


// ============================================================
// DIBUJAR ZOOM
// ============================================================

void DrawScaledBitmap(
    HDC targetDC,
    int width,
    int height)
{
    if (!g_bitmap || !g_memDC)
        return;

    HBITMAP oldBitmap =
        (HBITMAP)SelectObject(
            g_memDC,
            g_bitmap);

    SetStretchBltMode(
        targetDC,
        COLORONCOLOR);

    StretchBlt(
        targetDC,
        0,
        0,
        width,
        height,
        g_memDC,
        0,
        0,
        g_srcWidth,
        g_srcHeight,
        SRCCOPY);

    SelectObject(
        g_memDC,
        oldBitmap);
}


// ============================================================
// CAMBIAR ZOOM
// ============================================================

void SetZoom(int zoom)
{
    if (zoom < 1)
        zoom = 1;

    if (zoom > 6)
        zoom = 6;

    g_zoom = zoom;

    if (!g_zoomWindow)
        return;

    if (!GetMinesweeperClientSize())
        return;

    int clientWidth =
        g_srcWidth * g_zoom;

    int clientHeight =
        g_srcHeight * g_zoom;

    RECT rc =
    {
        0,
        0,
        clientWidth,
        clientHeight
    };

    AdjustWindowRectEx(
        &rc,
        WS_OVERLAPPEDWINDOW,
        FALSE,
        0);

    int windowWidth =
        rc.right - rc.left;

    int windowHeight =
        rc.bottom - rc.top;

    SetWindowPos(
        g_zoomWindow,
        HWND_TOP,
        100,
        100,
        windowWidth,
        windowHeight,
        SWP_NOACTIVATE);

    InvalidateRect(
        g_zoomWindow,
        NULL,
        FALSE);
}


// ============================================================
// OBTENER RECTÁNGULO DE LA CARA
// ============================================================

bool GetFaceRect(RECT* rect)
{
    if (!g_minesweeper)
        return false;

    RECT client;

    if (!GetClientRect(
            g_minesweeper,
            &client))
        return false;

    int width =
        client.right - client.left;

    // --------------------------------------------------------
    // Posición de la carita de Winmine clásico
    // --------------------------------------------------------

    const int faceWidth = 26;
    const int faceHeight = 26;

    int faceX =
        (width - faceWidth) / 2;

    int faceY = 5;

    rect->left =
        faceX;

    rect->top =
        faceY;

    rect->right =
        faceX + faceWidth;

    rect->bottom =
        faceY + faceHeight;

    return true;
}


// ============================================================
// ¿EL CLIC ES SOBRE LA CARA?
// ============================================================

bool IsFaceClicked(
    LPARAM lParam)
{
    int zoomX =
        GET_X_LPARAM(lParam);

    int zoomY =
        GET_Y_LPARAM(lParam);

    int originalX =
        zoomX / g_zoom;

    int originalY =
        zoomY / g_zoom;

    RECT faceRect;

    if (!GetFaceRect(&faceRect))
        return false;

    POINT p;

    p.x = originalX;
    p.y = originalY;

    return PtInRect(
        &faceRect,
        p);
}


// ============================================================
// SOLO MOUSE DOWN
// ============================================================

void SendMinesweeperMouseDown(
    int x,
    int y,
    bool rightButton)
{
    if (!g_minesweeper)
        return;

    LPARAM mousePos = MAKELPARAM(x, y);

    if (rightButton)
    {
        SendMessageW(
            g_minesweeper,
            WM_RBUTTONDOWN,
            MK_RBUTTON,
            mousePos);

        g_rightButtonDown = true;
    }
    else
    {
        SendMessageW(
            g_minesweeper,
            WM_LBUTTONDOWN,
            MK_LBUTTON,
            mousePos);

        g_leftButtonDown = true;
    }
}


// ============================================================
// SOLO MOUSE UP
// ============================================================

void SendMinesweeperMouseUp(
    int x,
    int y,
    bool rightButton)
{
    if (!g_minesweeper)
        return;

    LPARAM mousePos = MAKELPARAM(x, y);

    if (rightButton)
    {
        if (!g_rightButtonDown)
            return;

        SendMessageW(
            g_minesweeper,
            WM_RBUTTONUP,
            0,
            mousePos);

        g_rightButtonDown = false;
    }
    else
    {
        if (!g_leftButtonDown)
            return;

        SendMessageW(
            g_minesweeper,
            WM_LBUTTONUP,
            0,
            mousePos);

        g_leftButtonDown = false;
    }
}


// ============================================================
// CLICK DIRECTO EN BUSCAMINAS
// ============================================================

void SendMinesweeperClick(
    LPARAM lParam,
    bool rightButton)
{
    if (!g_minesweeper)
        return;

    int zoomX = GET_X_LPARAM(lParam);
    int zoomY = GET_Y_LPARAM(lParam);

    int originalX = zoomX / g_zoom;
    int originalY = zoomY / g_zoom;

    LPARAM mousePos =
        MAKELPARAM(
            originalX,
            originalY);

    UINT downMessage;
    UINT upMessage;
    WPARAM downFlags;

    if (rightButton)
    {
        downMessage = WM_RBUTTONDOWN;
        upMessage   = WM_RBUTTONUP;
        downFlags   = MK_RBUTTON;
    }
    else
    {
        downMessage = WM_LBUTTONDOWN;
        upMessage   = WM_LBUTTONUP;
        downFlags   = MK_LBUTTON;
    }

    // --------------------------------------------------------
    // PULSAR
    // --------------------------------------------------------

    PostMessageW(
        g_minesweeper,
        downMessage,
        downFlags,
        mousePos);

    // Dar tiempo a que Winmine procese la pulsación
   // Sleep(5);

    // --------------------------------------------------------
    // SOLTAR
    // --------------------------------------------------------

    PostMessageW(
        g_minesweeper,
        upMessage,
        0,
        mousePos);

    // --------------------------------------------------------
    // Actualizar Winmine
    // --------------------------------------------------------

    RedrawWindow(
        g_minesweeper,
        NULL,
        NULL,
        RDW_INVALIDATE |
        RDW_UPDATENOW |
        RDW_ALLCHILDREN);

    // --------------------------------------------------------
    // Actualizar el zoom después de que Winmine
    // haya procesado el clic
    // --------------------------------------------------------

    SetTimer(
        g_zoomWindow,
        2,
        30,
        NULL);
}


// ============================================================
// WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
        // ====================================================
        // PINTAR
        // ====================================================

        case WM_PAINT:
        {
            PAINTSTRUCT ps;

            HDC dc =
                BeginPaint(
                    hwnd,
                    &ps);

            RECT rc;

            GetClientRect(
                hwnd,
                &rc);

            DrawScaledBitmap(
                dc,
                rc.right,
                rc.bottom);

            EndPaint(
                hwnd,
                &ps);

            return 0;
        }


    // ====================================================
    // BOTÓN IZQUIERDO - DOWN
    // ====================================================

        case WM_LBUTTONDOWN:
{
    int zoomX = GET_X_LPARAM(lParam);
    int zoomY = GET_Y_LPARAM(lParam);

    int originalX = zoomX / g_zoom;
    int originalY = zoomY / g_zoom;

    g_mouseDownX = originalX;
    g_mouseDownY = originalY;

    // ========================================================
    // CARITA
    // ========================================================

    if (IsFaceClicked(lParam))
    {
        g_faceButtonDown = true;
        g_leftButtonDown = false;

        // Capturar el ratón para que recibamos el UP
        // aunque se mueva fuera de nuestra ventana.
        SetCapture(hwnd);

        // Mandar SOLO el DOWN a Winmine.
        SendMessageW(
            g_minesweeper,
            WM_LBUTTONDOWN,
            MK_LBUTTON,
            MAKELPARAM(
                originalX,
                originalY));

        // Capturar la cara pulsada después de que
        // Winmine haya procesado el DOWN.
        SetTimer(
            hwnd,
            3,
            30,
            NULL);

        return 0;
    }

    // ========================================================
    // CASILLA
    // ========================================================

    SendMinesweeperMouseDown(
        originalX,
        originalY,
        false);

    SetCapture(hwnd);

    return 0;
}


        // ====================================================
        // MOVIMIENTO DEL RATÓN
        // ====================================================

        case WM_MOUSEMOVE:
{
    // ========================================================
    // CARITA
    // ========================================================

    if (g_faceButtonDown)
    {
        // No hacemos absolutamente nada.
        //
        // Esto es intencionado:
        // si el usuario mueve el ratón mientras mantiene
        // pulsada la carita, no modificamos el estado.
        return 0;
    }

    // ========================================================
    // CASILLA
    // ========================================================

    if (g_leftButtonDown)
    {
        int zoomX = GET_X_LPARAM(lParam);
        int zoomY = GET_Y_LPARAM(lParam);

        int originalX = zoomX / g_zoom;
        int originalY = zoomY / g_zoom;

        LPARAM mousePos =
            MAKELPARAM(
                originalX,
                originalY);

        SendMessageW(
            g_minesweeper,
            WM_MOUSEMOVE,
            MK_LBUTTON,
            mousePos);

        return 0;
    }

    return 0;
}


    // ====================================================
    // BOTÓN IZQUIERDO - UP
    // ====================================================

    case WM_LBUTTONUP:
{
    int zoomX = GET_X_LPARAM(lParam);
    int zoomY = GET_Y_LPARAM(lParam);

    int originalX = zoomX / g_zoom;
    int originalY = zoomY / g_zoom;

    // ========================================================
    // CARITA
    // ========================================================

    if (g_faceButtonDown)
    {
        // SOLTAR la carita en Winmine.
        SendMessageW(
            g_minesweeper,
            WM_LBUTTONUP,
            0,
            MAKELPARAM(
                g_mouseDownX,
                g_mouseDownY));

        g_faceButtonDown = false;

        ReleaseCapture();

        // Capturar inmediatamente después del UP.
        SetTimer(
            hwnd,
            4,
            30,
            NULL);

        return 0;
    }

    // ========================================================
    // CASILLA
    // ========================================================

    if (g_leftButtonDown)
    {
        SendMinesweeperMouseUp(
            originalX,
            originalY,
            false);

        g_leftButtonDown = false;

        ReleaseCapture();

        return 0;
    }

    return 0;
}

    // =================================================
    // CASILLA
    // =================================================

    if (g_leftButtonDown)
    {
        SendMinesweeperMouseUp(
            originalX,
            originalY,
            false);
    }

    return 0;


        // ====================================================
        // BOTÓN DERECHO - DOWN
        // ====================================================

        case WM_RBUTTONDOWN:
        {
            int zoomX =
                GET_X_LPARAM(lParam);

            int zoomY =
                GET_Y_LPARAM(lParam);

            int originalX =
                zoomX / g_zoom;

            int originalY =
                zoomY / g_zoom;

            SendMinesweeperMouseDown(
                originalX,
                originalY,
                true);

            return 0;
        }

        // ====================================================
        // BOTÓN DERECHO - UP
        // ====================================================

        case WM_RBUTTONUP:
        {
            int zoomX =
                GET_X_LPARAM(lParam);

            int zoomY =
                GET_Y_LPARAM(lParam);

            int originalX =
                zoomX / g_zoom;

            int originalY =
                zoomY / g_zoom;

            SendMinesweeperMouseUp(
                originalX,
                originalY,
                true);

            return 0;
        }


        // ====================================================
        // TIMER
        // ====================================================

       case WM_TIMER:
{
    if (wParam == 1)
    {
        // Mientras la carita está pulsada,
        // dejamos que el timer 3 capture su estado.
        if (g_faceButtonDown)
            return 0;

        if (g_minesweeper)
        {
            CaptureMinesweeper();

            InvalidateRect(
                hwnd,
                NULL,
                FALSE);
        }

        return 0;
    }

    // --------------------------------------------------------
    // CAPTURA DESPUÉS DEL DOWN
    // --------------------------------------------------------

    if (wParam == 3)
    {
        KillTimer(hwnd, 3);

        if (g_minesweeper)
        {
            CaptureMinesweeper();

            InvalidateRect(
                hwnd,
                NULL,
                FALSE);

            UpdateWindow(hwnd);
        }

        return 0;
    }

    // --------------------------------------------------------
    // CAPTURA DESPUÉS DEL UP
    // --------------------------------------------------------

    if (wParam == 4)
    {
        KillTimer(hwnd, 4);

        if (g_minesweeper)
        {
            CaptureMinesweeper();

            InvalidateRect(
                hwnd,
                NULL,
                FALSE);

            UpdateWindow(hwnd);
        }

        return 0;
    }

    return 0;
}

        // ====================================================
        // TECLADO
        // ====================================================

        case WM_KEYDOWN:
        {
            if (wParam == VK_ADD ||
                wParam == VK_OEM_PLUS)
            {
                SetZoom(
                    g_zoom + 1);

                return 0;
            }

            if (wParam == VK_SUBTRACT ||
                wParam == VK_OEM_MINUS)
            {
                SetZoom(
                    g_zoom - 1);

                return 0;
            }

            if (wParam >= '1' &&
                wParam <= '6')
            {
                SetZoom(
                    wParam - '0');

                return 0;
            }

            PostMessageW(
                g_minesweeper,
                WM_KEYDOWN,
                wParam,
                lParam);

            return 0;
        }


        case WM_KEYUP:
        {
            PostMessageW(
                g_minesweeper,
                WM_KEYUP,
                wParam,
                lParam);

            return 0;
        }


        // ====================================================
        // CERRAR
        // ====================================================

        case WM_DESTROY:
        {
            if (g_bitmap)
            {
                DeleteObject(
                    g_bitmap);

                g_bitmap = NULL;
            }

            if (g_memDC)
            {
                DeleteDC(
                    g_memDC);

                g_memDC = NULL;
            }

            PostQuitMessage(0);

            return 0;
        }
    }

    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam);
}
// ============================================================
// MAIN
// ============================================================

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int)
{
    // --------------------------------------------------------
    // BUSCAR BUSCAMINAS
    // --------------------------------------------------------
        g_minesweeper =
        FindMinesweeperWindow();

    if (!g_minesweeper)
    {
        MessageBoxW(
            NULL,
            L"No se ha encontrado winmine.exe.\n\n"
            L"Abre primero el Buscaminas.",
            L"Minesweeper Zoom V9.4",
            MB_ICONERROR);

        return 1;
    }
    // --------------------------------------------------------
    // REGISTRAR CLASE
    // --------------------------------------------------------

    const wchar_t CLASS_NAME[] =
        L"MinesweeperZoomV9_4";

    WNDCLASSW wc = {};

    wc.lpfnWndProc =
        WindowProc;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        CLASS_NAME;

    wc.hCursor =
        LoadCursor(
            NULL,
            IDC_ARROW);

    wc.hbrBackground =
        (HBRUSH)(COLOR_WINDOW + 1);

    wc.style =
        CS_HREDRAW |
        CS_VREDRAW |
        CS_DBLCLKS;

    RegisterClassW(&wc);


    // --------------------------------------------------------
    // CAPTURA INICIAL
    // --------------------------------------------------------

    if (!CaptureMinesweeper())
    {
        MessageBoxW(
            NULL,
            L"No se pudo capturar "
            L"el Buscaminas.",
            L"Minesweeper Zoom V9.4",
            MB_ICONERROR);

        return 1;
    }


    // --------------------------------------------------------
    // TAMAÑO
    // --------------------------------------------------------

    int clientWidth =
        g_srcWidth * g_zoom;

    int clientHeight =
        g_srcHeight * g_zoom;

    RECT rc =
    {
        0,
        0,
        clientWidth,
        clientHeight
    };

    AdjustWindowRectEx(
        &rc,
        WS_OVERLAPPEDWINDOW,
        FALSE,
        0);

    int windowWidth =
        rc.right - rc.left;

    int windowHeight =
        rc.bottom - rc.top;


    // --------------------------------------------------------
    // CREAR VENTANA
    // --------------------------------------------------------

    g_zoomWindow =
        CreateWindowExW(
            WS_EX_APPWINDOW,

            CLASS_NAME,

            L"Buscaminas - Zoom V9.4",

            WS_OVERLAPPEDWINDOW,

            100,
            100,

            windowWidth,
            windowHeight,

            NULL,
            NULL,

            hInstance,
            NULL);

    if (!g_zoomWindow)
    {
        MessageBoxW(
            NULL,
            L"No se pudo crear "
            L"la ventana de zoom.",
            L"Minesweeper Zoom V9.4",
            MB_ICONERROR);

        return 1;
    }


    // --------------------------------------------------------
    // MOSTRAR
    // --------------------------------------------------------

    ShowWindow(
        g_zoomWindow,
        SW_SHOW);

    UpdateWindow(
        g_zoomWindow);


    // --------------------------------------------------------
    // ACTUALIZACIÓN CADA 50 ms
    // --------------------------------------------------------

    SetTimer(
        g_zoomWindow,
        1,
        200,
        NULL);


    // --------------------------------------------------------
    // BUCLE
    // --------------------------------------------------------

    MSG msg;

    while (GetMessageW(
        &msg,
        NULL,
        0,
        0))
    {
        TranslateMessage(&msg);

        DispatchMessage(&msg);
    }


    KillTimer(
        g_zoomWindow,
        1);

    return 0;
}
