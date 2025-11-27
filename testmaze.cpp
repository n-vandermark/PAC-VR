#include <iostream>
#include <vector>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <cmath>

using namespace std;

struct Entity {
    int x;
    int y;
    int dx;
    int dy;
};

struct TerminalRawMode {
    termios orig{};
    bool active{false};
    void enable() {
        if (active) return;
        tcgetattr(STDIN_FILENO, &orig);
        termios raw = orig;
        raw.c_lflag &= ~(ICANON);
        raw.c_lflag &= ~(ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        active = true;
    }
    void disable() {
        if (!active) return;
        tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        active = false;
    }
    ~TerminalRawMode() {
        disable();
    }
};

bool isHit() {
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    return (ret > 0 && FD_ISSET(STDIN_FILENO, &fds));
}

char getch_nowait() {
    char c = 0;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    return (n == 1) ? c : 0;
}

void clearScreen() {
    cout << "\x1B[2J\x1B[H";
}

vector<string> createMaze() {
    return {
"X X X X X X X X X X X X X X X X X X X X X X X X X X X X",
"X X X X X X X X X X X X X X X X X X X X X X X X X X X X",
"X . . . . . . . . . . . . X X . . . . . . . . . . . . X",
"X . X X X X . X X X X X . X X . X X X X X . X X X X . X",
"X . X X X X . X X X X X . X X . X X X X X . X X X X . X",
"X . X X X X . X X X X X . X X . X X X X X . X X X X . X",
"X . . . . . . . . . . . . . . . . . . . . . . . . . . X",
"X . X X X X . X X . X X X X X X X X . X X . X X X X . X",
"X . X X X X . X X . X X X X X X X X . X X . X X X X . X",
"X . . . . . . X X . . . . X X . . . . X X . . . . . . X",
"X X X X X X . X X X X X . X X . X X X X X . X X X X X X",
"X X X X X X . X X X X X . X X . X X X X X . X X X X X X",
"X X X X X X . X X . . . . . . . . . . X X . X X X X X X",
"X X X X X X . X X . X X X X A X X X . X X . X X X X X X",
"X X X X X X . X X . X X X X X X X X . X X . X X X X X X",
"X X X X X X . . . . X X X X X X X X . . . . X X X X X X",
"X X X X X X . X X . X X X X X X X X . X X . X X X X X X",
"X X X X X X . X X . X X X X X X X X . X X . X X X X X X",
"X X X X X X . X X . . . . . . . . . . X X . X X X X X X",
"X X X X X X . X X . X X X X X X X X . X X . X X X X X X",
"X X X X X X . X X . X X X X X X X X . X X . X X X X X X",
"X . . . . . . . . . . . . X X . . . . . . . . . . . . X",
"X . X X X X . X X X X X . X X . X X X X X . X X X X . X",
"X . X X X X . X X X X X . X X . X X X X X . X X X X . X",
"X . . . X X . . . . . . . . . . . . . . . . X X . . . X",
"X X X . X X . X X . X X X X X X X X . X X . X X . X X X",
"X X X . X X . X X . X X X X X X X X . X X . X X . X X X",
"X . . . . . . X X . . . . X X . . . . X X . . . . . . X",
"X . X X X X X X X X X X . X X . X X X X X X X X X X . X",
"X . X X X X X X X X X X . X X . X X X X X X X X X X . X",
"X C . . . . . . . . . . . . . . . . . . . . . . . . . X",
"X X X X X X X X X X X X X X X X X X X X X X X X X X X X",
"X X X X X X X X X X X X X X X X X X X X X X X X X X X X",
    };
}

bool isWall(const vector<string>& maze, int x, int y) {
    if (y < 0 || y >= (int)maze.size()) return true;
    if (x < 0 || x >= (int)maze[y].size()) return true;
    return maze[y][x] == 'X';
}

void draw(const vector<string>& maze, const Entity& pac, const Entity& ghost, int dotsRemaining) {
    clearScreen();
    for (int y = 0; y < (int)maze.size(); ++y) {
        for (int x = 0; x < (int)maze[y].size(); ++x) {
            if (x == pac.x && y == pac.y) cout << 'C';
            else if (x == ghost.x && y == ghost.y) cout << 'A';
            else cout << maze[y][x];
        }
        cout << '\n';
    }
    cout << "Dots remaining: " << dotsRemaining << "\n";
    cout << "JUST WASD for movement. Q=Quite\n";
}

void moveEntity(Entity& e, const vector<string>& maze) {
    int nx = e.x + e.dx;
    int ny = e.y + e.dy;
    if (!isWall(maze, nx, ny)) {
        e.x = nx;
        e.y = ny;
    }
}

void updateGhostDirection(Entity &ghost, const Entity &pac, const vector<string> &maze) {
    int bestDx = 0, bestDy = 0;
    int bestDist = 1000000000;

    const int dirs[4][2] = {
        { 2, 0 }, {-2, 0}, {0, 1}, {0, -1}
    };

    for (int i = 0; i < 4; ++i) {
        int dx = dirs[i][0];
        int dy = dirs[i][1];
        int nx = ghost.x + dx;
        int ny = ghost.y + dy;

        if (isWall(maze, nx, ny)) continue;

        int dist = abs(nx - pac.x) + abs(ny - pac.y);

        if (dist < bestDist || (dist == bestDist && rand() % 2)) {
            bestDist = dist;
            bestDx = dx;
            bestDy = dy;
        }
    }

    ghost.dx = bestDx;
    ghost.dy = bestDy;
}

void updateGhostDirection_BFS(Entity &ghost, const Entity &pac, const vector<string> &maze) {
    int H = maze.size();
    int W = maze[0].size();

    vector<vector<bool>> visited(H, vector<bool>(W,false));
    vector<vector<pair<int,int>>> parent(H, vector<pair<int,int>>(W,{-1,-1}));

    queue<pair<int,int>> q;
    q.push({ghost.y, ghost.x});
    visited[ghost.y][ghost.x] = true;

    const int dirs[4][2] = {
        {-1,0},{1,0},{0,-2},{0,2}
    };

    bool found = false;

    while (!q.empty() && !found) {
        auto [y, x] = q.front();
        q.pop();

        for (int i = 0; i < 4; ++i) {
            int ny = y + dirs[i][0];
            int nx = x + dirs[i][1];

            if (ny < 0 || ny >= H || nx < 0 || nx >= W) continue;
            if (isWall(maze, nx, ny) || visited[ny][nx]) continue;

            visited[ny][nx] = true;
            parent[ny][nx] = {y, x};

            if (ny == pac.y && nx == pac.x) {
                found = true;
                break;
            }

            q.push({ny, nx});
        }
    }

    if (!visited[pac.y][pac.x]) {
        ghost.dx = 0;
        ghost.dy = 0;
        return;
    }

    int py = pac.y, px = pac.x;
    while (parent[py][px] != make_pair(ghost.y, ghost.x)) {
        auto [ny, nx] = parent[py][px];
        py = ny;
        px = nx;
    }

    ghost.dy = py - ghost.y;
    ghost.dx = px - ghost.x;
}

int main() {
    while (true) {   // <-- restart loop

        vector<string> maze = createMaze();

        Entity pac{0,0,0,0};
        Entity ghost{0,0,0,0};
        int dotsRemaining = 0;

        for (int y = 0; y < (int)maze.size(); ++y) {
            for (int x = 0; x < (int)maze[y].size(); ++x) {
                char c = maze[y][x];
                if (c == 'C') {
                    pac.x = x;
                    pac.y = y;
                    pac.dx = 2;
                    pac.dy = 0;
                    dotsRemaining++;
                } else if (c == 'A') {
                    ghost.x = x;
                    ghost.y = y;
                    ghost.dx = 0;
                    ghost.dy = 0;
                    maze[y][x] = 'X';
                    dotsRemaining++;
                } else if (c == '.') {
                    dotsRemaining++;
                }
            }
        }

        srand((unsigned)time(nullptr));

        TerminalRawMode term;
        term.enable();

        bool gameOver = false;
        bool won = false;
        const int tick_ms = 120;

        int ghostTick = 0;
        long long ghostTimer = 0;

        while (!gameOver) {
            auto frameStart = chrono::steady_clock::now();

            if (isHit()) {
                char c = getch_nowait();
                if (c == 'q' || c == 'Q') {
                    term.disable();
                    return 0;
                }

                int ndx = pac.dx;
                int ndy = pac.dy;

                if (c == 'w' || c == 'W') { ndx = 0; ndy = -1; }
                else if (c == 's' || c == 'S') { ndx = 0; ndy = 1; }
                else if (c == 'a' || c == 'A') { ndx = -2; ndy = 0; }
                else if (c == 'd' || c == 'D') { ndx = 2; ndy = 0; }

                int nx = pac.x + ndx;
                int ny = pac.y + ndy;
                if (!isWall(maze, nx, ny)) {
                    pac.dx = ndx;
                    pac.dy = ndy;
                }
            }

            moveEntity(pac, maze);

            if (maze[pac.y][pac.x] == '.') {
                maze[pac.y][pac.x] = ' ';
                dotsRemaining--;
                if (dotsRemaining <= 0) {
                    won = true;
                    gameOver = true;
                }
            }

            ghostTimer += tick_ms;
            if (ghostTimer >= ghostTick) {
                ghostTimer = 0;
                updateGhostDirection_BFS(ghost, pac, maze);
                moveEntity(ghost, maze);
            }

            if (ghost.x == pac.x && ghost.y == pac.y) {
                gameOver = true;
                won = false;
            }

            draw(maze, pac, ghost, dotsRemaining);

            auto frameEnd = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(frameEnd - frameStart).count();
            if (elapsed < tick_ms) {
                this_thread::sleep_for(chrono::milliseconds(tick_ms - elapsed));
            }
        }

        term.disable();

        if (won) cout << "WOW you won!\n";
        else cout << "Dead! Failed to get dots and were eaten\n";

        cout << "Restart? (y/n): ";
        char r;
        cin >> r;
        if (r != 'y' && r != 'Y') break;
    }

    return 0;
}
