#include <iostream>
#include <cmath>
#include <cstring>
#include <thread>
#include <chrono>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

// --- helper to read key presses without Enter ---
int getch() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int main() {
    float A = 0, B = 0;
    float R1 = 0.9f, R2 = 1.8f;  // inner / outer radii
    float K2 = 7.0f;             // camera distance
    const float theta_spacing = 0.07f, phi_spacing = 0.02f;

    std::cout << "\033[2J"; // clear once

    // --- listen for arrow keys in a thread ---
    std::thread inputThread([&]() {
        while (true) {
            int ch = getch();
            if (ch == 27 && getchar() == '[') { // arrow keys start with ESC[
                switch (getchar()) {
                    case 'A': // ↑
                        R2 += 0.2f;
                        break;
                    case 'B': // ↓
                        R2 = std::max(0.8f, R2 - 0.2f);
                        break;
                }
            }
        }
    });
    inputThread.detach();

    while (true) {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        int height = w.ws_row;
        int width  = w.ws_col;

        const float K1 = 40.0f; // projection scale

        char output[height * width];
        float zbuffer[height * width];
        std::memset(output, ' ', sizeof(output));
        std::memset(zbuffer, 0, sizeof(zbuffer));

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
                float ooz = 1 / z;

                // 🟢 FIXED: adjust vertical stretch
                int xp = (int)(width  / 2 + K1 * ooz * x);
                int yp = (int)(height / 2 - K1 * ooz * y * 0.5);

                if (xp >= 0 && xp < width && yp >= 0 && yp < height) {
                    int idx = xp + width * yp;
                    float L = cosP * cosT * sinB
                            - cosA * cosT * sinP
                            - sinA * sinT
                            + cosB * (cosA * sinT - cosT * sinA * sinP);
                    if (L > 0 && ooz > zbuffer[idx]) {
                        zbuffer[idx] = ooz;
                        const char lum[] = ".,-~:;=!*#$@";
                        output[idx] = lum[(int)(L * 8) > 11 ? 11 : (int)(L * 8)];
                    }
                }
            }

        std::cout << "\033[H"; // move cursor to top
        for (int k = 0; k < height * width; k++)
            putchar(k % width ? output[k] : '\n');

        std::cout << "\nUse ↑ / ↓ to change donut size (R2=" << R2 << ")\n";

        A += 0.04f;
        B += 0.08f;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}
You, 3 min

