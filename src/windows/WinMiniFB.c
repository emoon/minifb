#include <MiniFB.h> 

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static WNDCLASS s_wc;
static HWND s_wnd;
static int s_close = 0;
static int s_width;
static int s_height;
static HDC s_hdc;
static void* s_buffer;
static BITMAPINFO s_bitmapInfo;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int res = 0;

	switch (message)
	{
		case WM_PAINT:
		{
			if (s_buffer)
			{
				StretchDIBits(s_hdc, 0, 0, s_width, s_height, 0, 0, s_width, s_height, s_buffer, 
							  &s_bitmapInfo, DIB_RGB_COLORS, SRCCOPY);

				ValidateRect(hWnd, NULL);
			}

			break;
		}

		case WM_KEYDOWN:
		{
			if ((wParam&0xFF) == 27) 
				s_close = 1;

			break;
		}

		case WM_CLOSE:
		{
			s_close = 1;
			break;
		}

		default:
		{
			res = DefWindowProc(hWnd, message, wParam, lParam);
		}
	}

	return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_open(const char* title, int width, int height)
{
	RECT rect = { 0 };

	s_wc.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
	s_wc.lpfnWndProc = WndProc;
	s_wc.hCursor = LoadCursor(0, IDC_ARROW);
	s_wc.lpszClassName = title;
	RegisterClass(&s_wc);

	rect.right = width;
	rect.bottom = height;

	AdjustWindowRect(&rect, WS_POPUP | WS_SYSMENU | WS_CAPTION, 0);

	rect.right -= rect.left;
	rect.bottom -= rect.top;

	s_width = width;
	s_height = height;

	s_wnd = CreateWindowEx(0, 
		title, title,
		WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right, rect.bottom,
		0, 0, 0, 0);

	if (!s_wnd)
		return 0;

	ShowWindow(s_wnd, SW_NORMAL);

	s_bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	s_bitmapInfo.bmiHeader.biPlanes = 1;
	s_bitmapInfo.bmiHeader.biBitCount = 32;
	s_bitmapInfo.bmiHeader.biCompression = BI_BITFIELDS;
	s_bitmapInfo.bmiHeader.biWidth = width;
	s_bitmapInfo.bmiHeader.biHeight = -height;
	s_bitmapInfo.bmiColors[0].rgbRed = 0xff; 
	s_bitmapInfo.bmiColors[1].rgbGreen = 0xff; 
	s_bitmapInfo.bmiColors[2].rgbBlue = 0xff; 

	s_hdc = GetDC(s_wnd);

	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_update(void* buffer)
{
	MSG msg;
	
	s_buffer = buffer;

	InvalidateRect(s_wnd, NULL, TRUE);
	SendMessage(s_wnd, WM_PAINT, 0, 0);

	while (PeekMessage(&msg, s_wnd, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (s_close == 1)
		return -1;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mfb_close()
{
	s_buffer = 0;
	ReleaseDC(s_wnd, s_hdc);
	DestroyWindow(s_wnd);
}

