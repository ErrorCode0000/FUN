#include <windows.h>
#include <vector>
#include <stack>
#include <ctime>
#include <algorithm>

// Labirent Ayarları
#define CELL_SIZE 30  // Her hücrenin piksel boyutu
#define COLS 25       // Sütun sayısı
#define ROWS 20       // Satır sayısı

// Yönler
enum Direction { TOP = 0, RIGHT, BOTTOM, LEFT };

struct Cell {
    int x, y;
    bool visited;
    bool walls[4]; // Top, Right, Bottom, Left

    Cell(int _x, int _y) : x(_x), y(_y), visited(false) {
        walls[TOP] = walls[RIGHT] = walls[BOTTOM] = walls[LEFT] = true;
    }
};

class MazeGenerator {
private:
    std::vector<Cell> grid;
    int playerX, playerY;
    int endX, endY;
    int level;

    int index(int x, int y) {
        if (x < 0 || y < 0 || x >= COLS || y >= ROWS) return -1;
        return y * COLS + x;
    }

    void removeWalls(Cell* a, Cell* b) {
        int x = a->x - b->x;
        if (x == 1) { a->walls[LEFT] = false; b->walls[RIGHT] = false; }
        else if (x == -1) { a->walls[RIGHT] = false; b->walls[LEFT] = false; }

        int y = a->y - b->y;
        if (y == 1) { a->walls[TOP] = false; b->walls[BOTTOM] = false; }
        else if (y == -1) { a->walls[BOTTOM] = false; b->walls[TOP] = false; }
    }

public:
    MazeGenerator() {
        level = 1;
        resetMaze();
    }

    void resetMaze() {
        grid.clear();
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) {
                grid.push_back(Cell(x, y));
            }
        }

        // Recursive Backtracker Algoritması
        std::stack<Cell*> stack;
        Cell* current = &grid[0];
        current->visited = true;
        stack.push(current);

        while (!stack.empty()) {
            current = stack.top();
            std::vector<Cell*> neighbors;

            // Komşuları kontrol et
            int neighborsIndices[] = { 
                index(current->x, current->y - 1), // Top
                index(current->x + 1, current->y), // Right
                index(current->x, current->y + 1), // Bottom
                index(current->x - 1, current->y)  // Left
            };

            for (int i : neighborsIndices) {
                if (i != -1 && !grid[i].visited) {
                    neighbors.push_back(&grid[i]);
                }
            }

            if (!neighbors.empty()) {
                Cell* next = neighbors[rand() % neighbors.size()];
                removeWalls(current, next);
                next->visited = true;
                stack.push(next);
            } else {
                stack.pop();
            }
        }

        // Oyuncu ve Hedef sıfırlama
        playerX = 0;
        playerY = 0;
        endX = COLS - 1;
        endY = ROWS - 1;
    }

    void draw(HDC hdc) {
        // Arka planı temizle
        RECT rect = { 0, 0, COLS * CELL_SIZE + 20, ROWS * CELL_SIZE + 40 };
        FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

        // Duvarları çiz
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        SelectObject(hdc, hPen);

        for (const auto& cell : grid) {
            int px = cell.x * CELL_SIZE + 10;
            int py = cell.y * CELL_SIZE + 10;

            if (cell.walls[TOP]) {
                MoveToEx(hdc, px, py, NULL);
                LineTo(hdc, px + CELL_SIZE, py);
            }
            if (cell.walls[RIGHT]) {
                MoveToEx(hdc, px + CELL_SIZE, py, NULL);
                LineTo(hdc, px + CELL_SIZE, py + CELL_SIZE);
            }
            if (cell.walls[BOTTOM]) {
                MoveToEx(hdc, px + CELL_SIZE, py + CELL_SIZE, NULL);
                LineTo(hdc, px, py + CELL_SIZE);
            }
            if (cell.walls[LEFT]) {
                MoveToEx(hdc, px, py + CELL_SIZE, NULL);
                LineTo(hdc, px, py);
            }
        }
        DeleteObject(hPen);

        // Oyuncuyu Çiz (Mavi Daire)
        HBRUSH hBrushPlayer = CreateSolidBrush(RGB(0, 120, 215));
        SelectObject(hdc, hBrushPlayer);
        Ellipse(hdc, playerX * CELL_SIZE + 15, playerY * CELL_SIZE + 15, 
                     playerX * CELL_SIZE + 15 + (CELL_SIZE - 10), playerY * CELL_SIZE + 15 + (CELL_SIZE - 10));
        DeleteObject(hBrushPlayer);

        // Hedefi Çiz (Kırmızı Kare)
        HBRUSH hBrushEnd = CreateSolidBrush(RGB(220, 20, 60)); // Kırmızı
        SelectObject(hdc, hBrushEnd);
        Rectangle(hdc, endX * CELL_SIZE + 15, endY * CELL_SIZE + 15,
                       endX * CELL_SIZE + 15 + (CELL_SIZE - 10), endY * CELL_SIZE + 15 + (CELL_SIZE - 10));
        DeleteObject(hBrushEnd);

        // Seviye Yazısı (İsim kaldırıldı)
        char text[32];
        wsprintf(text, "Seviye: %d", level);
        TextOut(hdc, 10, ROWS * CELL_SIZE + 15, text, strlen(text));
    }

    bool movePlayer(int direction) {
        Cell& current = grid[index(playerX, playerY)];
        
        if (direction == TOP && !current.walls[TOP]) playerY--;
        else if (direction == RIGHT && !current.walls[RIGHT]) playerX++;
        else if (direction == BOTTOM && !current.walls[BOTTOM]) playerY++;
        else if (direction == LEFT && !current.walls[LEFT]) playerX--;
        else return false; // Hareket edilemedi

        // Hedefe ulaşıldı mı?
        if (playerX == endX && playerY == endY) {
            level++;
            // Mesaj kutusu içeriği genel hale getirildi
            MessageBox(NULL, "Tebrikler! Seviye atladin.", "Basarili", MB_OK);
            resetMaze();
            return true; // Yeniden çizim gerekli
        }
        return true;
    }
};

MazeGenerator maze;

// Windows Procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        maze.draw(hdc);
        EndPaint(hwnd, &ps);
    } return 0;

    case WM_KEYDOWN: {
        bool needRedraw = false;
        switch (wParam) {
        case VK_UP:    needRedraw = maze.movePlayer(TOP); break;
        case VK_DOWN:  needRedraw = maze.movePlayer(BOTTOM); break;
        case VK_LEFT:  needRedraw = maze.movePlayer(LEFT); break;
        case VK_RIGHT: needRedraw = maze.movePlayer(RIGHT); break;
        }
        if (needRedraw) InvalidateRect(hwnd, NULL, TRUE);
    } return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Main Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    srand((unsigned int)time(0)); 

    const char CLASS_NAME[] = "MazeWindow";
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // Pencere başlığı genel hale getirildi
    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "Infinite Maze Generator - GDI",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, COLS * CELL_SIZE + 50, ROWS * CELL_SIZE + 80,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
