#include <stdint.h>
#include "st7789_bootloader.h"
#include "st7789.h"


/* Layout constants */
#define BOOT_MSG_Y              10
#define DOWNLOAD_MSG_Y          45
#define DOTS_Y                  70

#define PROGRESS_BAR_X          10
#define PROGRESS_BAR_Y          100
#define PROGRESS_BAR_W          (ST7789_WIDTH - 2 * PROGRESS_BAR_X)
#define PROGRESS_BAR_H          16

#define DONE_MSG_Y              60
#define DONE_SUBMSG_Y           90

/* Colors – adjust to taste */
#define BOOT_BG                 BLACK
#define BOOT_FG                 WHITE
#define BOOT_ACCENT             GBLUE
#define BOOT_BAR_OUTLINE        WHITE
#define BOOT_BAR_FILL           GREEN

/* Internal state for dot animation */
static uint8_t s_dot_count = 0;

/* Helper: clamp 0..100 */
static uint8_t clamp_percent(uint8_t p)
{
    if (p > 100) return 100;
    return p;
}

/* Draw empty progress bar frame */
static void draw_progress_bar_frame(void)
{
    ST7789_DrawRectangle(PROGRESS_BAR_X,
                         PROGRESS_BAR_Y,
                         PROGRESS_BAR_X + PROGRESS_BAR_W,
                         PROGRESS_BAR_Y + PROGRESS_BAR_H,
                         BOOT_BAR_OUTLINE);
}

/* Fill progress bar according to percent */
static void draw_progress_bar_fill(uint8_t percent)
{
    uint8_t p = clamp_percent(percent);

    /* Compute filled width (inside the rectangle) */
    uint16_t inner_x      = PROGRESS_BAR_X + 1;
    uint16_t inner_y      = PROGRESS_BAR_Y + 1;
    uint16_t inner_w_max  = PROGRESS_BAR_W - 1;   /* leave 1px margin on right */
    uint16_t inner_h      = PROGRESS_BAR_H - 1;

    uint16_t filled_w = (inner_w_max * p) / 100;

    if (filled_w == 0) {
        return;
    }

    ST7789_DrawFilledRectangle(inner_x,
                               inner_y,
                               filled_w,
                               inner_h,
                               BOOT_BAR_FILL);
}

/* Clear dots area */
static void clear_dots_area(void)
{
    ST7789_Fill(DOWNLOAD_MSG_Y,
                DOTS_Y,
                ST7789_WIDTH - 1,
                DOTS_Y + Font_11x18.height,
                BOOT_BG);
}

/* ------------------------------------------------------------------------- */
/* PUBLIC API                                                                */
/* ------------------------------------------------------------------------- */

void UI_Bootloader_ShowStart(void)
{
    ST7789_Fill_Color(BOOT_BG);

    /* Title */
    ST7789_WriteString(10,
                       BOOT_MSG_Y,
                       "In Bootloader",
                       Font_16x26,
                       BOOT_ACCENT,
                       BOOT_BG);

    /* Download message */
    ST7789_WriteString(10,
                       DOWNLOAD_MSG_Y,
                       "Downloading Image",
                       Font_11x18,
                       BOOT_FG,
                       BOOT_BG);

    /* Initial empty dots and progress bar */
    s_dot_count = 0;
    clear_dots_area();

    draw_progress_bar_frame();
    draw_progress_bar_fill(0);
}

/* Call this when you have real % progress (0–100). */
void UI_Bootloader_UpdateProgress(uint8_t percent)
{
    /* Re-draw bar frame once (cheap), then fill portion */
    draw_progress_bar_frame();
    draw_progress_bar_fill(percent);
}

/* Call this periodically (e.g. in your download loop) if you
 * DON'T have real progress yet. It animates "Downloading Image" with dots.
 */
void UI_Bootloader_TickDots(void)
{
    char buf[32];

    s_dot_count = (s_dot_count + 1) % 4;   /* 0,1,2,3 */

    /* Build string: "Downloading Image", "Downloading Image.", "..", "..." */
    if (s_dot_count == 0) {
        snprintf(buf, sizeof(buf), "Downloading Image");
    } else if (s_dot_count == 1) {
        snprintf(buf, sizeof(buf), "Downloading Image.");
    } else if (s_dot_count == 2) {
        snprintf(buf, sizeof(buf), "Downloading Image..");
    } else {
        snprintf(buf, sizeof(buf), "Downloading Image...");
    }

    /* Clear dots/text area and re-print */
    clear_dots_area();
    ST7789_WriteString(10,
                       DOWNLOAD_MSG_Y,
                       buf,
                       Font_11x18,
                       BOOT_FG,
                       BOOT_BG);
}

/* Call once when update completes successfully */
void UI_Bootloader_ShowDone(void)
{
    ST7789_Fill_Color(BOOT_BG);

    ST7789_WriteString(10,
                       DONE_MSG_Y,
                       "Update Complete",
                       Font_16x26,
                       GREEN,
                       BOOT_BG);

    ST7789_WriteString(10,
                       DONE_SUBMSG_Y,
                       "Reset board",
                       Font_11x18,
                       BOOT_FG,
                       BOOT_BG);
}

void UI_Bootloader_Failed(void)
{
    ST7789_Fill_Color(BOOT_BG);

    ST7789_WriteString(10,
                       DONE_MSG_Y,
                       "Update Failed",
                       Font_16x26,
                       RED,
                       BOOT_BG);

    ST7789_WriteString(10,
                       DONE_SUBMSG_Y,
                       "Reset board",
                       Font_11x18,
                       BOOT_FG,
                       BOOT_BG);
}

