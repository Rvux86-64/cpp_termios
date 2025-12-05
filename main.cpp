#include <iostream>
#include <cmath>
#include <cstring>
#include <thread>
#include <chrono>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <vector>
#include <algorithm>

// --- helper to read key presses without Enter ---
int getch_safe() {
    if (!isatty(STDIN_FILENO)) return -1; // no TTY --> no input
    struct termios oldt, newt;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) return -1;
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) return -1;
    int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int main() {
    float A = 0, B = 0;
    float R1 = 0.9f, R2 = 1.8f;  // inner / outer radii
    float K2 = 7.0f;             // camera distance
    const float theta_spacing = 0.07f, phi_spacing = 0.02f;

    std::cout << "\033[2J"; // clear once

    // --- listen for arrow keys in a thread only if we have a TTY ---
    if (isatty(STDIN_FILENO)) {
        std::thread inputThread([&]() {
            while (true) {
                int ch = getch_safe();
                if (ch == 27) {
                    int c2 = getchar();
                    if (c2 == '[') {
                        int arr = getchar();
                        if (arr == 'A') { // ↑
                            R2 += 0.2f;
                        } else if (arr == 'B') { // ↓
                            R2 = std::max(0.8f, R2 - 0.2f);
                        }
                    }
                } else if (ch == -1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        });
        inputThread.detach();
    }

    while (true) {
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
            // fallback to defaults
            w.ws_row = 24;
            w.ws_col = 80;
        }
        int height = std::max(1, (int)w.ws_row);
        int width  = std::max(1, (int)w.ws_col);

        const float K1 = 40.0f; // projection scale

        // Prevent runaway allocations in CI by capping size (tune as needed)
        const size_t max_cells = 2000 * 2000; // 4M cells cap (about 4MB char + 16MB float -> ~20MB)
        size_t cells = (size_t)height * (size_t)width;
        if (cells > max_cells) {
            // scale down to a reasonable maximum by shrinking width/height proportionally
            float scale = std::sqrt((double)max_cells / (double)cells);
            width = std::max(1, (int)(width * scale));
            height = std::max(1, (int)(height * scale));
            cells = (size_t)height * (size_t)width;
        }

        std::vector<char> output(cells, ' ');
        std::vector<float> zbuffer(cells, 0.0f);

        for (float theta = 0; theta < 2 * M_PI; theta += theta_spacing)
            for (float phi = 0; phi < 2 * M_PI; phi += phi_spacing) {
                float sinA = sin(A), cosA = cos(A);
                float sinB = sin(B), cosB = cos(B);
                float sinT = sin(theta), cosT = cos(theta);
                float sinP = sin(phi), cosP = cos(phi);

                float circlex = R2 + R1 * cosT;
                float circley = R1 * sinT;

                float x = circlex * (cosB * cosP + sinA * sinB * sinP) - circley * cosA * sinB;
                float y = circlex * (sinB * cosP - sinA * cosB * sinP) + circley * cosA * cosB;
                float z = K2 + cosA * circlex * sinP + circley * sinA;
                float ooz = 1.0f / z;

                int xp = (int)(width  / 2 + K1 * ooz * x);
                int yp = (int)(height / 2 - K1 * ooz * y * 0.5f);

                if (xp >= 0 && xp < width && yp >= 0 && yp < height) {
                    size_t idx = (size_t)xp + (size_t)width * (size_t)yp;
                    float L = cosP * cosT * sinB
                            - cosA * cosT * sinP
                            - sinA * sinT
                            + cosB * (cosA * sinT - cosT * sinA * sinP);
                    if (L > 0.0f && ooz > zbuffer[idx]) {
                        zbuffer[idx] = ooz;
                        const char lum[] = ".,-~:;=!*#$@";
                        int li = (int)(L * 8.0f);
                        li = std::clamp(li, 0, (int)(sizeof(lum)-2)); // -2 because lum has terminating '\0'
                        output[idx] = lum[li];
                    }
                }
            }

        std::cout << "\033[H"; // move cursor to top
        for (int r = 0; r < height; ++r) {
            for (int c = 0; c < width; ++c) {
                putchar(output[c + r * width]);
            }
            putchar('\n');
        }

        std::cout << "\nUse ↑ / ↓ to change donut size (R2=" << R2 << ")\n";

        A += 0.04f;
        B += 0.08f;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}
