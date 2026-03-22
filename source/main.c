#include <nds.h>
#include <nds/arm9/console.h>
#include <fat.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

#define SETTINGS_PATH "fat:/_pico/settings.json"
#define THEMES_DIR    "fat:/_pico/themes"
#define MAX_THEMES    32
#define NAME_LEN      64
#define SCREEN_SIZE   (256 * 192 * 2)

static char themes[MAX_THEMES][NAME_LEN];
static int  themeCount = 0;

PrintConsole uiConsole;

void scanThemes(void) {
    DIR *dir = opendir(THEMES_DIR);
    if (!dir) {
        printf("ERROR: Can't open themes dir\n");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && themeCount < MAX_THEMES) {
        if (entry->d_type == DT_DIR
            && strcmp(entry->d_name, ".") != 0
            && strcmp(entry->d_name, "..") != 0) {
            strncpy(themes[themeCount], entry->d_name, NAME_LEN - 1);
            themes[themeCount][NAME_LEN - 1] = '\0';
            themeCount++;
        }
    }
    closedir(dir);
}

bool loadBG(const char *path, uint16_t *vramDest) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fread(vramDest, 1, SCREEN_SIZE, f);
    fclose(f);
    return true;
}

void loadThemePreviews(int index) {
    char path[256];

    snprintf(path, sizeof(path), "fat:/_pico/themes/%s/topbg.bin", themes[index]);
    if (!loadBG(path, (uint16_t *) BG_BMP_RAM(0))) {
        dmaFillHalfWords(0x4210, BG_BMP_RAM(0), SCREEN_SIZE);
    }

    snprintf(path, sizeof(path), "fat:/_pico/themes/%s/bottombg.bin", themes[index]);
    if (!loadBG(path, (uint16_t *) BG_BMP_RAM_SUB(2))) {
        dmaFillHalfWords(0x4210, BG_BMP_RAM_SUB(2), SCREEN_SIZE);
    }
}

void drawList(int selected) {
    consoleSelect(&uiConsole);
    consoleClear();
    printf("\x1b[1;1H-- Pico Theme Switcher --\n\n");
    for (int i = 0; i < themeCount; i++) {
        if (i == selected)
            printf(" > %s\n", themes[i]);
        else
            printf("   %s\n", themes[i]);
    }
    printf("\n\nUP/DOWN: select\nA: apply\nB: cancel");
}

bool writeTheme(const char *newTheme) {
    FILE *f = fopen(SETTINGS_PATH, "r");
    if (!f) { printf("ERROR: Can't open settings.json\n"); return false; }

    char buf[4096] = {0};
    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[len] = '\0';

    char *pos = strstr(buf, "\"theme\"");
    if (!pos) { printf("ERROR: no 'theme' key found\n"); return false; }
    pos = strchr(pos, ':');
    if (!pos) return false;
    pos++;
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos != '"') return false;
    pos++;

    char *end = strchr(pos, '"');
    if (!end) return false;

    char out[4096];
    size_t prefixLen = (size_t) (pos - buf);
    size_t newLen = strlen(newTheme);
    size_t suffixStart = (size_t) (end - buf);

    if (prefixLen + newLen + strlen(buf + suffixStart) >= sizeof(out))
        return false;

    memcpy(out, buf, prefixLen);
    memcpy(out + prefixLen, newTheme, newLen);
    strcpy(out + prefixLen + newLen, buf + suffixStart);

    f = fopen(SETTINGS_PATH, "w");
    if (!f) { printf("ERROR: Can't write settings.json\n"); return false; }
    fputs(out, f);
    fclose(f);
    return true;
}

int main(void) {
    // Top screen: 16-bit bitmap for topbg preview
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    // Bottom screen: 16-bit bitmap for bottombg preview + console overlay
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 2, 0);
    consoleInit(&uiConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);
    consoleSelect(&uiConsole);

    printf("Init FAT...\n");
    if (!fatInitDefault()) {
        printf("FAT FAILED!\n");
        while (1) swiWaitForVBlank();
    }

    printf("Scanning themes...\n");
    scanThemes();

    if (themeCount == 0) {
        printf("No themes found!\n");
        while (1) {
            scanKeys();
            if (keysDown()) exit(0);
            swiWaitForVBlank();
        }
    }

    int selected = 0;
    loadThemePreviews(selected);
    drawList(selected);

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        u32 down = keysDown();

        if (down & KEY_DOWN) {
            selected = (selected + 1) % themeCount;
            loadThemePreviews(selected);
            drawList(selected);
        }
        if (down & KEY_UP) {
            selected = (selected - 1 + themeCount) % themeCount;
            loadThemePreviews(selected);
            drawList(selected);
        }
        if (down & KEY_B) {
            exit(0);
        }
        if (down & KEY_A) {
            consoleSelect(&uiConsole);
            consoleClear();
            printf("Applying: %s\n\nWriting settings.json...", themes[selected]);
            if (writeTheme(themes[selected])) {
                printf("\nDone! Exiting...");
                for (int i = 0; i < 120; i++) swiWaitForVBlank();
                exit(0);
            }
            else {
                printf("\nFailed!\nPress any key to exit.");
                while (1) {
                    scanKeys();
                    if (keysDown()) exit(0);
                    swiWaitForVBlank();
                }
            }
        }
    }
}