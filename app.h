/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#ifndef APP_H
#define APP_H


#define CAPSENSE_TASK_STACK_SIZE        512
#define BUTTON0_TASK_STACK_SIZE         512
#define BUTTON1_TASK_STACK_SIZE         512
#define PHYSICS_TASK_STACK_SIZE         512
#define LCD_TASK_STACK_SIZE             512
#define LED_TASK_STACK_SIZE             512
#define CASTLE_TASK_STACK_SIZE          512
#define PLATFORM_TASK_STACK_SIZE        512
#define IDLE_TASK_STACK_SIZE            256

#define CAPSENSE_TASK_PRIO                9
#define BUTTON0_TASK_PRIO             10
#define BUTTON1_TASK_PRIO             10
#define PHYSICS_TASK_PRIO           10
#define LCD_TASK_PRIO                  10
#define LED_TASK_PRIO                  10
#define CASTLE_TASK_PRIO                  10
#define PLATFORM_TASK_PRIO                  10
#define IDLE_TASK_PRIO                 20


#define PLATFORM_MASS         100
#define CAPSESNE_SAMPLE_TIME  0.1 //seconds, 100 milliseconds
#define MAX_POSITION          128//find the end of the screen
#define TERMINAL_VELOCITY     250 //meters/seconds
#define RAILGUN_ANGLE         60 // in degrees
#define SHOT_MASS             50 // in kg
#define PHYSICS_SAMPLE_TIME   0.05 //seconds, 50 milliseconds
#define SHIELD_ENERGY_REQUIREMENT   30000000 // in Joules
#define MAX_FORCE             20000
#define HALF_FORCE            10000 // in Joules probably tune for game
#define BUTTON_0_PRESSED      01
#define BUTTON_1_PRESSED      10
#define ALL_BUTTON_FLAG       11
#define CAPACITIVE_ENERGY_MAX 1500
//
//static uint32_t CAP_DIRECTION;
//static bool BUTTON0;
//static bool BUTTON1;
//
//

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/




void app_init(void);
void resource_create(void);
void LCDDisplay_Task_Init(void);
void LEDOutput_Task_Init(void);
//void MyCallback(OS_TMR * p_tmr, void  *p_arg);
void Idle_Task_Init(void);
void Physics_Task_Init(void);
void Capsense_Task_Init(void);
void Button_Task_Init(void);


#endif  // APP_H
