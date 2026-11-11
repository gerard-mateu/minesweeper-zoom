# minesweeper-zoom
A simple program that makes the Windows NT Minesweeper bigger

You might also need to download Minesweeper for Windows NT, you can create a virtual machine with Windows NT and copy the program llocated in C:\WINNT\System32 or simply download it from /winmine


You will also need to run them as an administrator and set the WinMine program to Windows 95 compatibility mode, always launching WinMine first.

I’ve added comments in Spanish, you might need to translate them.

1. General concept:

    I. Locate the `winmine.exe` process and its window.
  
    II. Every 200 ms, capture an image of the Minesweeper client area.
   
    III. Display this image—scaled (at a zoom level of 1x to 6x)—within a separate window.
   
    IV. Any mouse clicks or keystrokes performed in the magnified window are translated back to the original coordinates (by dividing by the zoom factor) and sent to the actual Minesweeper application.

3. Main Components

Locating the Game

  FindProcessId: Iterates through processes using CreateToolhelp32Snapshot until winmine.exe is found.
  FindMinesweeperWindow + EnumWindowsProc: Enumerates windows and returns the visible one belonging to that process.

Capture

  GetMinesweeperClientSize: Retrieves the size of the client area.
  CaptureMinesweeper: Creates a compatible bitmap and copies the content using PrintWindow (with the PW_CLIENTONLY flag). If this fails, it uses BitBlt as a fallback. It then replaces the previous bitmap (releasing the
  old one).

Drawing

  DrawScaledBitmap: Uses StretchBlt with the COLORONCOLOR flag (disabling smoothing to ensure pixels remain sharp) and renders it during the WM_PAINT message.
  SetZoom: Constrains the zoom level to a range of 1–6, recalculates the window size using AdjustWindowRectEx, and resizes the window accordingly.

The Smiley Face

  GetFaceRect: Calculates the bounding rectangle for the smiley face (26x26 px, centered at the top, with y=5).
  IsFaceClicked: Checks whether a click—after being converted to the original coordinate system—falls within that rectangle. The smiley face is handled separately because clicking it restarts the game, requiring a coordinated "mouse down" and "mouse up" sequence.


3. Input handling (WindowProc)

Left button (down): Converts coordinates. If it’s the smiley face, it sets `g_faceButtonDown` to true, captures the mouse, and forwards only the `WM_LBUTTONDOWN` message. If it’s a cell, it forwards the "down" event and captures the mouse.

Movement: If the left button is pressed, it forwards `WM_MOUSEMOVE` to the Minesweeper game (so that the "sunken cell" effect is visible). If the smiley face is pressed, it does nothing.

Left button (up): Forwards the corresponding `WM_LBUTTONUP` message, releases the mouse capture, and schedules a re-capture.

Right button: Sends "down" and "up" events separately (for placing/removing flags).

Keyboard: The +/- keys or the 1–6 keys adjust the zoom level; all other keys are forwarded to the Minesweeper game.

`WM_DESTROY`: Releases the bitmap and device context (DC), and closes the program.


4. Timers

  I.	  200 ms Continuous refresh (capture + repaint)
  
  II.	  30 ms	Triggered by SendMinesweeperClick (a helper function not currently in use)
  
  III.	30 ms	Recapture after clicking the smiley face (to show its “pressed” state)
  
  IV.	  30 ms	Recapture after releasing the smiley face
