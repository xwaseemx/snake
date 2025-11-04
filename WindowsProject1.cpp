#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <ctime>
#include <cstdlib>

using namespace std;
using namespace Gdiplus;

#pragma comment (lib,"Gdiplus.lib")

// Constants
const int WIDTH = 800;
const int HEIGHT = 600;
const int BLOCK_SIZE = 20; // Snake block size
const int FPS = 10;        // Game speed

// Colors
Color RED(255, 255, 0, 0);
Color GREEN(255, 0, 255, 0);
Color WHITE(255, 255, 255, 255);
Color BLACK(255, 0, 0, 0);

// Snake direction
enum Direction { STOP = 0, LEFT, RIGHT, UP, DOWN };

// Snake point struct
struct SnakePoint {
	int x, y;
};

// Global GDI+ objects
HWND hwnd = nullptr;
HDC hdc = nullptr;
HDC hdcMem = nullptr;
HBITMAP hbmMem = nullptr;
HBITMAP hbmOld = nullptr;
Graphics* graphics = nullptr;

// Initialize GDI+
class GDIPlusManager {
public:
	GDIPlusManager() {
		GdiplusStartupInput gdiplusStartupInput;
		GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
	}
	~GDIPlusManager() {
		GdiplusShutdown(gdiplusToken);
	}
private:
	ULONG_PTR gdiplusToken;
};
GDIPlusManager gdiManager;

// Utility functions
void initwindow(int width, int height, const char* title) {
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = DefWindowProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = L"SnakeWindow";
	if (!RegisterClass(&wc)) {
		MessageBox(nullptr, L"Failed to register window class!", L"Error", MB_OK);
		exit(1);
	}

	// Adjust window size so client area is exactly width x height
	RECT wr = { 0, 0, width, height };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	hwnd = CreateWindowW(
		L"SnakeWindow",
		L"Snake Game",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wr.right - wr.left,
		wr.bottom - wr.top,
		nullptr, nullptr, wc.hInstance, nullptr
	);

	if (!hwnd) {
		MessageBox(nullptr, L"Failed to create window!", L"Error", MB_OK);
		exit(1);
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	hdc = GetDC(hwnd);
	if (!hdc) {
		MessageBox(nullptr, L"Failed to get HDC!", L"Error", MB_OK);
		exit(1);
	}

	// Create memory DC for double-buffering
	hdcMem = CreateCompatibleDC(hdc);
	hbmMem = CreateCompatibleBitmap(hdc, width, height);
	hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

	graphics = new Graphics(hdcMem);
	if (!graphics) {
		MessageBox(nullptr, L"Failed to create Graphics object!", L"Error", MB_OK);
		exit(1);
	}
}

void closegraph() {
	if (graphics) delete graphics;

	if (hdcMem) {
		SelectObject(hdcMem, hbmOld);
		DeleteObject(hbmMem);
		DeleteDC(hdcMem);
	}

	if (hwnd && hdc) ReleaseDC(hwnd, hdc);
	if (hwnd) DestroyWindow(hwnd);

	hwnd = nullptr;
	hdc = nullptr;
	graphics = nullptr;
	hdcMem = nullptr;
	hbmMem = nullptr;
	hbmOld = nullptr;
}

void cleardevice() {
	if (graphics) {
		SolidBrush brush(WHITE);
		graphics->FillRectangle(&brush, 0.0f, 0.0f, (REAL)WIDTH, (REAL)HEIGHT);
	}
}

void bar(int x1, int y1, int x2, int y2, Color color) {
	if (graphics) {
		SolidBrush brush(color);
		// Always draw a proper rectangle using BLOCK_SIZE
		float fx = (REAL)min(x1, x2);
		float fy = (REAL)min(y1, y2);
		float fw = (REAL)abs(x2 - x1);
		float fh = (REAL)abs(y2 - y1);
		if (fw <= 0) fw = BLOCK_SIZE;
		if (fh <= 0) fh = BLOCK_SIZE;
		graphics->FillRectangle(&brush, fx, fy, fw, fh);
	}
}


void outtextxy(int x, int y, const char* text, Color color, int size = 24) {
	if (graphics) {
		FontFamily fontFamily(L"Arial");
		Font font(&fontFamily, size, FontStyleRegular, UnitPixel);
		SolidBrush brush(color);
		WCHAR wtext[256];
		MultiByteToWideChar(CP_ACP, 0, text, -1, wtext, 256);
		graphics->DrawString(wtext, -1, &font, PointF((REAL)x, (REAL)y), &brush);
	}
}

void delay(int ms) {
	Sleep(ms);
}

// Snake Game Class
class SnakeGame {
private:
	vector<SnakePoint> snake;
	SnakePoint food;
	Direction dir;
	bool gameOver;
	int score;

public:
	SnakeGame() {
		dir = STOP;
		gameOver = false;
		score = 0;

		// Start with 2 blocks
		snake.push_back({ WIDTH / 2, HEIGHT / 2 });                 // Head
		snake.push_back({ WIDTH / 2 - BLOCK_SIZE, HEIGHT / 2 });    // Tail

		generateFood();
	}

	void startMenu() {
		initwindow(WIDTH, HEIGHT, "Snake Game");
		cleardevice();
		outtextxy(WIDTH / 2 - 100, HEIGHT / 2 - 100, "SNAKE GAME", BLACK, 36);
		outtextxy(WIDTH / 2 - 100, HEIGHT / 2, "Press ENTER to Start", BLACK, 24);
		outtextxy(WIDTH / 2 - 100, HEIGHT / 2 + 50, "Press ESC to Exit", BLACK, 24);

		BitBlt(hdc, 0, 0, WIDTH, HEIGHT, hdcMem, 0, 0, SRCCOPY);

		// Wait for ENTER or ESC while processing messages
		while (true) {
			MSG msg;
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			if (GetAsyncKeyState(VK_RETURN)) break;
			if (GetAsyncKeyState(VK_ESCAPE)) {
				closegraph();
				exit(0);
			}

			delay(50);
		}

		dir = RIGHT;
	}

	void generateFood() {
		bool onSnake;
		do {
			onSnake = false;
			food.x = (rand() % (WIDTH / BLOCK_SIZE)) * BLOCK_SIZE;
			food.y = (rand() % (HEIGHT / BLOCK_SIZE)) * BLOCK_SIZE;

			for (auto segment : snake) {
				if (food.x == segment.x && food.y == segment.y) {
					onSnake = true;
					break;
				}
			}
		} while (onSnake);
	}

	void input() {
		if (GetAsyncKeyState(VK_LEFT) & 0x8000 && dir != RIGHT) dir = LEFT;
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000 && dir != LEFT) dir = RIGHT;
		if (GetAsyncKeyState(VK_UP) & 0x8000 && dir != DOWN) dir = UP;
		if (GetAsyncKeyState(VK_DOWN) & 0x8000 && dir != UP) dir = DOWN;
	}

	void logic() {
		if (dir == STOP) return;

		SnakePoint newHead = snake.front();

		switch (dir) {
		case LEFT: newHead.x -= BLOCK_SIZE; break;
		case RIGHT: newHead.x += BLOCK_SIZE; break;
		case UP: newHead.y -= BLOCK_SIZE; break;
		case DOWN: newHead.y += BLOCK_SIZE; break;
		default: break;
		}

		if (newHead.x < 0 || newHead.x >= WIDTH || newHead.y < 0 || newHead.y >= HEIGHT) {
			gameOver = true;
			return;
		}

		for (auto segment : snake) {
			if (newHead.x == segment.x && newHead.y == segment.y) {
				gameOver = true;
				return;
			}
		}

		snake.insert(snake.begin(), newHead);

		if (newHead.x == food.x && newHead.y == food.y) {
			score += 10;
			generateFood();
		}
		else {
			snake.pop_back();
		}
	}

	void draw() {
		cleardevice();

		bar(food.x, food.y, food.x + BLOCK_SIZE, food.y + BLOCK_SIZE, RED);

		for (auto segment : snake) {
			bar(segment.x, segment.y, segment.x + BLOCK_SIZE, segment.y + BLOCK_SIZE, GREEN);
		}

		char s[50];
		sprintf_s(s, sizeof(s), "Score: %d", score);
		outtextxy(10, 10, s, BLACK, 24);

		BitBlt(hdc, 0, 0, WIDTH, HEIGHT, hdcMem, 0, 0, SRCCOPY);
	}

	void run() {
		while (!gameOver) {
			MSG msg;
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			input();
			logic();
			draw();

			delay(1000 / FPS);
		}

		cleardevice();
		outtextxy(WIDTH / 2 - 150, HEIGHT / 2 - 50, "GAME OVER!", BLACK, 36);
		char s[50];
		sprintf_s(s, sizeof(s), "Final Score: %d", score);
		outtextxy(WIDTH / 2 - 100, HEIGHT / 2 + 20, s, BLACK, 24);
		outtextxy(WIDTH / 2 - 150, HEIGHT / 2 + 80, "Press ESC to Exit", BLACK, 24);
		BitBlt(hdc, 0, 0, WIDTH, HEIGHT, hdcMem, 0, 0, SRCCOPY);

		while (!GetAsyncKeyState(VK_ESCAPE)) {
			MSG msg;
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			delay(50);
		}

		closegraph();
	}
};

// Correct WinMain outside the class
int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow
) {
	srand(static_cast<unsigned int>(time(0)));
	SnakeGame game;
	game.startMenu();
	game.run();
	return 0;
}
