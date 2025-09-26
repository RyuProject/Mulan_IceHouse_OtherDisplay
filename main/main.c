/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_log.h"
#include "bsp/esp32_p4_function_ev_board.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "main";

#define WHITE_COLOR 0xFFFF
#define BLACK_COLOR 0x0000
#define RED_COLOR   0xF800
#define GREEN_COLOR 0x07E0
#define BLUE_COLOR  0x001F

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;
static QueueHandle_t touch_event_queue = NULL;

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} touch_event_t;

static void draw_button(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    // 创建颜色缓冲区
    uint16_t *color_buffer = malloc(width * height * sizeof(uint16_t));
    if (color_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }
    
    // 填充颜色
    for (int i = 0; i < width * height; i++) {
        color_buffer[i] = color;
    }
    
    // 绘制按钮背景
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + width, y + height, color_buffer);
    // 添加延迟避免绘图冲突
    vTaskDelay(pdMS_TO_TICKS(10));
    
    free(color_buffer);
}

static void draw_border(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    // 创建边框颜色缓冲区
    uint16_t *border_buffer = malloc(width * 2 * sizeof(uint16_t));
    if (border_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }
    
    // 填充边框颜色
    for (int i = 0; i < width * 2; i++) {
        border_buffer[i] = color;
    }
    
    // 绘制上边框
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + width, y + 2, border_buffer);
    vTaskDelay(pdMS_TO_TICKS(5));
    // 绘制下边框
    esp_lcd_panel_draw_bitmap(panel_handle, x, y + height - 2, x + width, y + height, border_buffer);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    free(border_buffer);
    
    // 创建垂直边框缓冲区
    uint16_t *v_border_buffer = malloc(2 * height * sizeof(uint16_t));
    if (v_border_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }
    
    // 填充垂直边框颜色
    for (int i = 0; i < 2 * height; i++) {
        v_border_buffer[i] = color;
    }
    
    // 绘制左边框
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + 2, y + height, v_border_buffer);
    vTaskDelay(pdMS_TO_TICKS(5));
    // 绘制右边框
    esp_lcd_panel_draw_bitmap(panel_handle, x + width - 2, y, x + width, y + height, v_border_buffer);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    free(v_border_buffer);
}

static void touch_event_handler(void* arg) {
    touch_event_t event;
    uint16_t touch_x[10] = {0};
    uint16_t touch_y[10] = {0};
    uint16_t touch_strength[10] = {0};
    uint8_t touch_num = 0;
    bool last_pressed = false;
    static uint32_t touch_count = 0;
    
    while (1) {
        if (touch_handle != NULL) {
            esp_err_t ret = esp_lcd_touch_read_data(touch_handle);
            if (ret == ESP_OK) {
                esp_lcd_touch_get_coordinates(touch_handle, touch_x, touch_y, touch_strength, &touch_num, 10);
                
                // 每100次循环打印一次触摸状态用于调试
                if (touch_count++ % 100 == 0) {
                    if (touch_num > 0) {
                        ESP_LOGI(TAG, "触摸状态: 点数=%d", touch_num);
                        for (int i = 0; i < touch_num; i++) {
                            ESP_LOGI(TAG, "  点%d: 坐标=(%d,%d), 强度=%d", 
                                    i, touch_x[i], touch_y[i], touch_strength[i]);
                        }
                    } else {
                        ESP_LOGI(TAG, "触摸状态: 点数=0");
                    }
                }
            } else {
                // 每100次循环打印一次错误用于调试
                if (touch_count++ % 100 == 0) {
                    ESP_LOGW(TAG, "触摸读取失败: %d", ret);
                }
            }
        } else {
            // 触摸手柄为空，触摸功能不可用
            if (touch_count++ % 100 == 0) {
                ESP_LOGW(TAG, "触摸功能不可用");
            }
            touch_num = 0;
        }
        
        bool current_pressed = (touch_num > 0);
        
        // 只在状态变化时发送事件
        if (current_pressed && !last_pressed) {
            // 坐标转换：触摸芯片坐标可能需要转换到屏幕坐标
            // 根据屏幕分辨率800x1280进行适配
            event.x = touch_x[0];
            event.y = touch_y[0];
            
            // 如果触摸坐标超出屏幕范围，进行缩放适配
            if (event.x > BSP_LCD_H_RES) {
                event.x = BSP_LCD_H_RES - 1;
            }
            if (event.y > BSP_LCD_V_RES) {
                event.y = BSP_LCD_V_RES - 1;
            }
            
            event.pressed = true;
            if (xQueueSend(touch_event_queue, &event, 0) == pdTRUE) {
                ESP_LOGI(TAG, "触摸按下: 点数=%d", touch_num);
                for (int i = 0; i < touch_num; i++) {
                    ESP_LOGI(TAG, "  点%d: X=%d, Y=%d, 强度=%d", i, touch_x[i], touch_y[i], touch_strength[i]);
                }
            }
        }
        
        last_pressed = current_pressed;
        vTaskDelay(pdMS_TO_TICKS(20)); // 更快的检测频率
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "开始屏幕和触控测试");
    
    // 初始化显示 - 使用空的配置结构
    bsp_display_config_t display_config = {
        .dummy = 0,
    };
    
    esp_lcd_panel_io_handle_t io_handle = NULL;
    if (bsp_display_new(&display_config, &panel_handle, &io_handle) != ESP_OK) {
        ESP_LOGE(TAG, "显示初始化失败");
        return;
    }
    
    // 初始化触摸 - 使用正确的配置结构
    bsp_touch_config_t touch_config = {
        .dummy = 0,
    };
    
    // 触摸初始化，允许重试
    esp_err_t touch_ret;
    int retry_count = 0;
    do {
        touch_ret = bsp_touch_new(&touch_config, &touch_handle);
        if (touch_ret != ESP_OK) {
            ESP_LOGW(TAG, "触摸初始化失败，重试 %d/3", retry_count + 1);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        retry_count++;
    } while (touch_ret != ESP_OK && retry_count < 3);
    
    if (touch_ret != ESP_OK) {
        ESP_LOGE(TAG, "触摸初始化最终失败，但继续运行程序");
        // 不返回，继续运行程序，触摸功能可能不可用
    } else {
        ESP_LOGI(TAG, "触摸初始化成功");
    }
    
    // 创建触摸事件队列
    touch_event_queue = xQueueCreate(10, sizeof(touch_event_t));
    
    // 创建触摸处理任务
    xTaskCreate(touch_event_handler, "touch_handler", 4096, NULL, 5, NULL);
    
    // 清屏为白色
    uint16_t *white_buffer = malloc(BSP_LCD_H_RES * 80 * sizeof(uint16_t));
    if (white_buffer) {
        for (int i = 0; i < BSP_LCD_H_RES * 80; i++) {
            white_buffer[i] = WHITE_COLOR;
        }
        // 分块绘制以避免内存不足
        for (int y = 0; y < BSP_LCD_V_RES; y += 80) {
            int height = (y + 80 <= BSP_LCD_V_RES) ? 80 : (BSP_LCD_V_RES - y);
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, BSP_LCD_H_RES, y + height, white_buffer);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        free(white_buffer);
    }
    
    // 绘制测试按钮
    uint16_t button_x = (BSP_LCD_H_RES - 200) / 2;
    uint16_t button_y = (BSP_LCD_V_RES - 80) / 2;
    
    draw_button(button_x, button_y, 200, 80, GREEN_COLOR);
    draw_border(button_x, button_y, 200, 80, BLACK_COLOR);
    
    // 设置背光为100%
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "背光已开启");
    
    ESP_LOGI(TAG, "屏幕测试程序已启动");
    ESP_LOGI(TAG, "屏幕分辨率: %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
    ESP_LOGI(TAG, "按钮位置: X=%d, Y=%d", button_x, button_y);
    
    // 主循环处理触摸事件
    touch_event_t event;
    while (1) {
        if (xQueueReceive(touch_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "收到触摸事件: X=%d, Y=%d", event.x, event.y);
            
            // 检查是否点击了按钮区域
            if (event.pressed && 
                event.x >= button_x && event.x <= button_x + 200 &&
                event.y >= button_y && event.y <= button_y + 80) {
                ESP_LOGI(TAG, "按钮被点击! 坐标范围: X[%d-%d], Y[%d-%d]", 
                        button_x, button_x + 200, button_y, button_y + 80);
                
                // 按钮点击反馈 - 改变颜色
                draw_button(button_x, button_y, 200, 80, BLUE_COLOR);
                draw_border(button_x, button_y, 200, 80, BLACK_COLOR);
                vTaskDelay(pdMS_TO_TICKS(200));
                draw_button(button_x, button_y, 200, 80, GREEN_COLOR);
                draw_border(button_x, button_y, 200, 80, BLACK_COLOR);
            } else {
                ESP_LOGI(TAG, "点击位置不在按钮区域");
            }
        }
    }
}
