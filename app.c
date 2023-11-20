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

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/

#include <gpio.h>
#include <capsense.h>

#include "sl_board_control.h"
#include "em_assert.h"
#include "glib.h"
#include "dmd.h"
#include "os.h"
#include "app.h"
#include <em_emu.h>
#include <os_trace.h>
#include <stdio.h>
#include <string.h>
#include <cmu.h>




/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/
//Task Variables
static OS_TCB idle_tcb;
static CPU_STK idle_stack[IDLE_TASK_STACK_SIZE];
static OS_TCB physics_tcb;
static CPU_STK physics_stack[PHYSICS_TASK_STACK_SIZE];
static OS_TCB button0_tcb;
static CPU_STK button0_stack[BUTTON0_TASK_STACK_SIZE];
static OS_TCB button1_tcb;
static CPU_STK button1_stack[BUTTON1_TASK_STACK_SIZE];
static OS_TCB capsense_tcb;
static CPU_STK capsense_stack[CAPSENSE_TASK_STACK_SIZE];
static OS_TCB lcd_tcb;
static CPU_STK lcd_stack[LCD_TASK_STACK_SIZE];
static OS_TCB led_tcb;
static CPU_STK led_stack[LED_TASK_STACK_SIZE];


// tasks
static void LCDDisplay_task(void *arg);
static void LEDOutput_task(void *arg);
static void idle_task(void *arg);
static void Capsense_task(void);
static void Button_task(void);
//static void read_capsense(void);

// Data Structures

typedef struct{
  float velocity;
  int force;
  float position;
  bool attempt_to_activate;
  bool activated;
}PLATFORM_TYPEDEF;

typedef struct{
  float time_pressed;
  int x_location;
  int y_location;
  int castle_health; // 0 = nothing, 1 = foundation hit once, 2 = foundation hit twice or castle was hit
}PROJECTILE_TYPEDEF;

typedef struct{
  float x_pos;
  float y_pos;
  float x_velocity;
  float y_velocity;
}SATCHEL_TYPEDEF;

//Global Variables
int foundation_strength;
PLATFORM_TYPEDEF Platform;
PROJECTILE_TYPEDEF Projectile;
SATCHEL_TYPEDEF Satchel1;

bool gameEnd = false;
bool gameWon = false;
bool BUTTON0;
bool BUTTON1;
uint32_t BUTTON_STATE; // 0: none pressed/ both pressed, 1: b0 pressed, 2: b1 pressed
uint32_t SLIDER_DIRECTION;
static GLIB_Context_t glibContext;
bool channel0 = false;
bool channel1 = false;
bool channel2 = false;
bool channel3 = false;
int capEnergy = CAPACITIVE_ENERGY_MAX;
bool CastleDestroyed;
bool PrisonersKilled;

//static OS_TMR timer;
//static void   *timer_callback_arg;
RTOS_ERR     err;
int btn0State;
int btn1State;


//Inter task communication:
OS_FLAG_GRP   BUTTON_FLAG;
OS_FLAG_GRP   CASTLE_FLAG;
OS_MUTEX      PROJECTILE_MUTEX;
OS_MUTEX      PLATFORM_MUTEX;
OS_MUTEX      SATCHEL_MUTEX;
OS_SEM        BUTTON1_SEMAPHORE;
OS_SEM        BUTTON0_SEMAPHORE;

//OS_TMR        CAPSENSE_TIMER;
//OS_TMR        LCD_TIMER;
//OS_TMR        PHYSICS_TIMER;
//
//OS_TMR  App_Timer;


int pressed = 0;
int button1Pressed = 0;
CPU_BOOLEAN started;
CPU_BOOLEAN stopped;
int buttonDuration = 0;

void Button_Timer_Callback(){
  buttonDuration++;

}

void resource_create(void)
{

    OSFlagCreate(&BUTTON_FLAG, "button flags",0,  &err);
    OSFlagCreate(&CASTLE_FLAG, "castle flag", 0,&err);
    OSMutexCreate(&PROJECTILE_MUTEX, "Projectile mutex", &err);
    OSMutexCreate(&PLATFORM_MUTEX, "platform mutex", &err);
    OSMutexCreate(&SATCHEL_MUTEX, "satchel mutex", &err);
    OSSemCreate(&BUTTON1_SEMAPHORE, "btn1 sem", 1, &err);
    OSSemCreate(&BUTTON0_SEMAPHORE, "btn0 sem", 1, &err);



//    OSTmrCreate(&App_Timer, "button timer", 0, 1, OS_OPT_TMR_PERIODIC, &Button_Timer_Callback, DEF_NULL, &err); // every tenth of a second
}



//void start_timer(void){
//  started = OSTmrStart(&BUTTON_TIMER, &err);
//}
//void stop_timer(void){
//  stopped = OSTmrStop(&BUTTON_TIMER, OS_OPT_TMR_NONE, DEF_NULL, &err);
//}
/***************************************************************************//**
 * @brief
 *   Interrupt handler to service pressing of buttons
 ******************************************************************************/
void GPIO_EVEN_IRQHandler(void)
{
  uint32_t int_flag = GPIO->IF & GPIO->IEN;
  GPIO->IFC = int_flag;
  OS_SEM_CTR ctr;
  btn0State = GPIO_PinInGet(BUTTON0_port, BUTTON0_pin);
  if(btn0State == 0){
      ctr = OSSemPost(&BUTTON0_SEMAPHORE, OS_OPT_POST_1, &err);
  }
}

/***************************************************************************//**
 * @brief
 *   Interrupt handler to service pressing of buttons
 ******************************************************************************/
void GPIO_ODD_IRQHandler(void)
{

  uint32_t int_flag = GPIO->IF & GPIO->IEN;
  GPIO->IFC = int_flag;
  OS_SEM_CTR ctr;
  btn1State = GPIO_PinInGet(BUTTON1_port, BUTTON1_pin);
  if(btn1State == 0){
      ctr = OSSemPost(&BUTTON1_SEMAPHORE, OS_OPT_POST_1, &err);
  }

}



/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/

void Physics_task(void){
  float accel;
  float deltaX;
  float updatedVelocity;
  int updatedPosition;
  float projectile_x_velocity;
  float projectile_y_velocity;
  int projectileVelocity;
  int previousForce = 0;
  int random_velocity;
  time_t t;
  srand(time(&t));


  while(1){

      OSTimeDlyHMSM(0,0,0, 50, OS_OPT_TIME_DLY, &err);
      if(capEnergy <= CAPACITIVE_ENERGY_MAX){
          capEnergy += 15;
      }
      random_velocity = rand() % 100; // return random number between 0 and 10

      OSMutexPend(&SATCHEL_MUTEX, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);

        if(Satchel1.y_pos <= 0){
            Satchel1.x_velocity = random_velocity;
            Satchel1.y_velocity = 0;
            Satchel1.y_pos = 120;
            Satchel1.x_pos = 0;
        }

        Satchel1.x_pos = Satchel1.x_pos + Satchel1.x_velocity * PHYSICS_SAMPLE_TIME;
        Satchel1.y_velocity = Satchel1.y_velocity + (-4.9 * PHYSICS_SAMPLE_TIME);
        Satchel1.y_pos = Satchel1.y_pos + Satchel1.y_velocity + (0.5 * (-9.8) * PHYSICS_SAMPLE_TIME * PHYSICS_SAMPLE_TIME);





      OSMutexPend(&PLATFORM_MUTEX, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);

//        if(previousForce == Platform.force){
//            updatedVelocity = Platform.velocity;
//        }
//        else{
//            accel = (Platform.force / PLATFORM_MASS);
//            updatedVelocity = (Platform.velocity + accel);
//        }
//        deltaX = 0.5 * (Platform.velocity + updatedVelocity);
//        Platform.velocity = updatedVelocity;
//        Platform.position = Platform.position + deltaX;
//

//        previousForce = Platform.force;
      if(Platform.attempt_to_activate == true && Satchel1.x_pos >= Platform.position - 30 && Satchel1.x_pos <= Platform.position + 30){
          if(capEnergy >= 250){
              capEnergy -= 400;
              if(Satchel1.y_pos <= 20){
                  Satchel1.x_pos = 0;
                  Satchel1.y_pos = 0;
                  Platform.activated = true;
              }
          }
      }

      if(Satchel1.x_pos >= Platform.position - 15 && Satchel1.x_pos <= Platform.position + 15){
          if(Satchel1.y_pos <= 5){
              gameEnd = true;
          }
      }

      accel = (Platform.force/ PLATFORM_MASS);
      updatedVelocity = Platform.velocity + accel * 0.01;
      Platform.velocity = updatedVelocity;
      deltaX = Platform.velocity * 0.01 + 0.5 * accel * 0.0001;
      Platform.position = Platform.position + deltaX;
      if(Platform.position < 10 || Platform.position > MAX_POSITION - 10){ // platform is 10 meters wide and position is calculated in the center so offset by 5
          if(Platform.velocity > TERMINAL_VELOCITY || Platform.velocity < -1 * TERMINAL_VELOCITY  ){
              gameEnd = true;
          }
          else{
              Platform.velocity = Platform.velocity * -1;
              if(Platform.position < 64){
                  Platform.position = 10;
              }
              else{
                  Platform.position = 118;
              }
          }

      }



      OSMutexPost(&SATCHEL_MUTEX,  OS_OPT_POST_1, &err);


      OSMutexPend(&PROJECTILE_MUTEX, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);


        if(projectileVelocity * 10 > capEnergy){
            projectileVelocity = capEnergy / 10;
        }
        else{
            if(Projectile.time_pressed != 0){
                projectileVelocity = Projectile.time_pressed;
                Projectile.x_location = Platform.position;
                Projectile.y_location = 0;

            }
            capEnergy -= Projectile.time_pressed * 10;
        }
        Projectile.x_location = Projectile.x_location - projectileVelocity * 0.5 * PHYSICS_SAMPLE_TIME; // 0.5 is for cos(60) for x portion of velocity
        Projectile.y_location = Projectile.y_location + projectileVelocity * (0.93969262078) * PHYSICS_SAMPLE_TIME + (0.5 * (-15) * PHYSICS_SAMPLE_TIME * PHYSICS_SAMPLE_TIME); // 0.8660254 is approximation for sqrt(3)/2

        if(Projectile.x_location <= 0 && Projectile.x_location >= -5){
            if(Projectile.y_location >= 100 && Projectile.y_location <= 128){ // castle is from 600 meters to 1000 meters

                    Projectile.castle_health = 2;
                    CastleDestroyed = true;
            }
            else if(Projectile.y_location < 100 && Projectile.y_location >= 80){

                           if(Projectile.castle_health == 1){
                               Projectile.castle_health = 2;
                               CastleDestroyed = true;

                           }
                           else{
                               Projectile.castle_health = 1;
                           }
                       }


            }



      Projectile.time_pressed = 0;
      Platform.attempt_to_activate = false;

      OSMutexPost(&PROJECTILE_MUTEX, OS_OPT_POST_1, &err);
      OSMutexPost(&PLATFORM_MUTEX, OS_OPT_POST_1, &err);


  }
}



static void LCDDisplay_task(void *arg){

    char * gameOver = "You have lost :(";
    char * gameWasBeat = "You win!";
    char * prisonerWasKilled = "You lost because you killed a prisoner.";
//  uint32_t sLength = 15;
//  uint32_t  x0 = 20;
//  uint32_t  y0 = 20;
//  bool opaque = 0;



  PP_UNUSED_PARAM(arg);

//  uint32_t current_direction;
  char * output_direction;
  char output_speed[10];

  uint32_t status;

  /* Enable the memory lcd */
  status = sl_board_enable_display();
  EFM_ASSERT(status == SL_STATUS_OK);

  /* Initialize the DMD support for memory lcd display */
  status = DMD_init(0);
  EFM_ASSERT(status == DMD_OK);

  /* Initialize the glib context */
  status = GLIB_contextInit(&glibContext);
  EFM_ASSERT(status == GLIB_OK);

  glibContext.backgroundColor = White;
  glibContext.foregroundColor = Black;

  /* Fill lcd with background color */
  GLIB_clear(&glibContext);

//  GLIB_Font_t * glibFont;
//  glibFont->fontHeight = 20;
//  glibFont->fontWidth = 20;

  /* Use Narrow font */
  GLIB_setFont(&glibContext, (GLIB_Font_t *) &GLIB_FontNarrow6x8);
  GLIB_Rectangle_t  capRect;
  GLIB_Rectangle_t  healthRect;
  capRect.xMin = 64;
  capRect.yMin = 1;
  capRect.yMax = 10;
  healthRect.xMin = 123;
  healthRect.xMax = 128;
  healthRect.yMax = 100;
  healthRect.yMin = 40;
   const int32_t castlePoints[20] = {0,0,   0,55,   20,40,     23,35,   30,35,   30,25,   20, 25,   20,15,    30,15,    30,0};


  RTOS_ERR err;


  while (1)
  {
      OSTimeDlyHMSM(0,0,0, 150, OS_OPT_TIME_DLY, &err);
      EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));

      GLIB_clear(&glibContext);
      if(gameEnd == true){
          if(gameWon == true){
               GLIB_drawStringOnLine(&glibContext, gameWasBeat, 1, GLIB_ALIGN_CENTER, 5, 5, true);
               CastleDestroyed = false;
               GPIO_PinOutClear(LED1_port, LED1_pin);

          }
          else{
              if(PrisonersKilled == true){
                  GLIB_drawStringOnLine(&glibContext, prisonerWasKilled, 1, GLIB_ALIGN_CENTER, 5, 5, true);
              }
              GLIB_drawStringOnLine(&glibContext, gameOver, 1, GLIB_ALIGN_CENTER, 5, 5, true);

          }

      }
      else{
          GLIB_drawPolygonFilled(&glibContext, 10, castlePoints);

//          GLIB_drawPolygon();

          OSMutexPend(&PLATFORM_MUTEX, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);

            GLIB_drawLine(&glibContext, (Platform.position - 4.5), 118, Platform.position, 125);
            GLIB_drawLine(&glibContext, (Platform.position - 3.5), 118, Platform.position - 1, 125);

            if(Platform.activated == true){
                GLIB_drawCircle(&glibContext, Platform.position, 128, 15);
            }
            Platform.activated = false;

            GLIB_drawLine(&glibContext, (Platform.position - 10), 125, (Platform.position + 10), 125);
            GLIB_drawLine(&glibContext, (Platform.position - 10), 126, (Platform.position + 10), 126);
            GLIB_drawLine(&glibContext, (Platform.position - 10), 127, (Platform.position + 10), 127);
            GLIB_drawLine(&glibContext, (Platform.position - 10), 128, (Platform.position + 10), 128);


          OSMutexPost(&PLATFORM_MUTEX, OS_OPT_POST_1, &err);

          OSMutexPend(&PROJECTILE_MUTEX, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);

            GLIB_drawCircle(&glibContext, Projectile.x_location, 128 - Projectile.y_location, 3);
            if(Projectile.castle_health == 1){
                healthRect.yMin = 70;

            }
            if(Projectile.castle_health == 2){
                healthRect.yMin = 98;
            }
            GLIB_drawRectFilled(&glibContext, &healthRect);


          OSMutexPost(&PROJECTILE_MUTEX, OS_OPT_POST_1, &err);

          OSMutexPend(&SATCHEL_MUTEX, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);
              //Still need to decide how to do the satchel

             GLIB_drawCircleFilled(&glibContext, Satchel1.x_pos, 128 - Satchel1.y_pos, 2);
          OSMutexPost(&SATCHEL_MUTEX,  OS_OPT_POST_1, &err);



          capRect.xMax = (capEnergy / 25) + 64;
          if(capRect.xMax < 0){
              capRect.xMax = 0;
          }

          GLIB_drawRectFilled(&glibContext, &capRect);


      }



      DMD_updateDisplay();
  }
}




static void LEDOutput_task(void *arg){ //pend on event flag posted by vmonitor then change led state
  int counter = 0;
  int counter2 = 0;
  int counter3 = 0;
  while(1){
      OSTimeDlyHMSM(0,0,0, 100, OS_OPT_TIME_DLY, &err);
      if(CastleDestroyed == true){
          counter++;
          if(counter % 10 == 0){
              GPIO_PinOutSet(LED1_port, LED1_pin);
              counter2++;
          }
          if(counter2 % 2 == 0){
              GPIO_PinOutClear(LED1_port, LED1_pin);
          }
          if(counter == 50){
              gameWon = true;
              gameEnd = true;
          }
      }

      OSMutexPend(&PROJECTILE_MUTEX, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);
      counter3++;
      if(Projectile.time_pressed == 50){
          GPIO_PinOutSet(LED0_port, LED0_pin);
      }
      if(counter3 % 10 == 0){
          GPIO_PinOutClear(LED0_port, LED0_pin);
      }

      OSMutexPost(&PROJECTILE_MUTEX, OS_OPT_POST_1, &err);


  }

}

static void Capsense_task(void){
  while(1){
      OSTimeDlyHMSM(0,0,0, 100, OS_OPT_TIME_DLY, &err);

//      channel0 = false;
//      channel1 = false;
//      channel2 = false;
//      channel3 = false;
//      CAPSENSE_Init();

      CAPSENSE_Sense(); // change
      channel0 = CAPSENSE_getPressed(0);
      channel1 = CAPSENSE_getPressed(1);
      channel2 = CAPSENSE_getPressed(2);
      channel3 = CAPSENSE_getPressed(3);

//      GPIO_PinOutClear(LED0_port, LED0_pin);
//      GPIO_PinOutClear(LED1_port, LED1_pin);



         if (channel2 || channel3)
           {
             Platform.force = MAX_FORCE;
           }
         else if (channel0 || channel1){
             Platform.force = -1 * MAX_FORCE;
         }
         else if(channel0 && channel1 && channel2 && channel3)
           {
             Platform.force = 0;
           }

    }
  }


static void Button0_task(void){
int btnPressed = 0;
//CPU_BOOLEAN started;
  while(1){
      OSTimeDlyHMSM(0,0,0, 100, OS_OPT_TIME_DLY, &err);

      OS_SEM_CTR ctr;
      ctr = OSSemPend(&BUTTON0_SEMAPHORE, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);
//      GPIO_PinOutSet(LED1_port, LED1_pin);
//      GPIO_PinOutSet(LED0_port, LED0_pin);
      btnPressed++;
      OSMutexPend(&PROJECTILE_MUTEX, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);

      Projectile.time_pressed = 50;
      GPIO_PinOutSet(LED0_port, LED0_pin);


      OSMutexPost(&PROJECTILE_MUTEX, OS_OPT_POST_1, &err);



  }


}

static void Button1_task(void){

  while(1){
      OSTimeDlyHMSM(0,0,0, 100, OS_OPT_TIME_DLY, &err);

      OS_SEM_CTR ctr;
      ctr = OSSemPend(&BUTTON1_SEMAPHORE, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);

      OSMutexPend(&PLATFORM_MUTEX, 0, OS_OPT_PEND_BLOCKING, DEF_NULL, &err);

      Platform.attempt_to_activate = true;

      OSMutexPost(&PLATFORM_MUTEX, OS_OPT_POST_1, &err);
//      GPIO_PinOutSet(LED1_port, LED1_pin);
//      GPIO_PinOutSet(LED0_port, LED0_pin);

  }

}





static void idle_task(void *arg){

  PP_UNUSED_PARAM(arg);

  RTOS_ERR err;
  while (1)
  {
      //OSTimeDly(delay_time, OS_OPT_TIME_DLY, &err);
      EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));
      EMU_EnterEM1();
  }

}







 void app_init(void)
 {
   CAPSENSE_Init();
   resource_create();
   gpio_open();
   LCDDisplay_Task_Init();
   LEDOutput_Task_Init();
   Idle_Task_Init();
   Physics_Task_Init();
   Capsense_Task_Init();
   Button_Task_Init();


 }

 void Idle_Task_Init(void){

   RTOS_ERR     err;

   OSTaskCreate(&idle_tcb,                // Pointer to the task's TCB.
                "idle task",                    // Name to help debugging.
                idle_task,                   // Pointer to the task's code.
                 DEF_NULL,                          // Pointer to task's argument.
                 IDLE_TASK_PRIO,             // Task's priority.
                &idle_stack[0],             // Pointer to base of stack.
                (IDLE_TASK_STACK_SIZE / 10u),  // Stack limit, from base.
                IDLE_TASK_STACK_SIZE,         // Stack size, in CPU_STK.
                 0u,                               // Messages in task queue.
                 0u,                                // Round-Robin time quanta.
                 DEF_NULL,                          // External TCB data.
                 (OS_OPT_TASK_STK_CLR),               // Task options.
                &err);
   EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));


 }

 void LEDOutput_Task_Init(void){

   RTOS_ERR     err;

   OSTaskCreate(&led_tcb,                // Pointer to the task's TCB.
                "led task",                    // Name to help debugging.
                LEDOutput_task,                   // Pointer to the task's code.
                 DEF_NULL,                  // Pointer to task's argument.
                 LED_TASK_PRIO,             // Task's priority.
                &led_stack[0],             // Pointer to base of stack.
                (LED_TASK_STACK_SIZE / 10u),  // Stack limit, from base.
                LED_TASK_STACK_SIZE,         // Stack size, in CPU_STK.
                 0u,                               // Messages in task queue.
                 0u,                                // Round-Robin time quanta.
                 DEF_NULL,                          // External TCB data.
                 (OS_OPT_TASK_STK_CLR),               // Task options.
                &err);
   EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));

 }

 void LCDDisplay_Task_Init(void){

   RTOS_ERR     err;

   OSTaskCreate(&lcd_tcb,                            // Pointer to the task's TCB.
                "lcdDisplay task",                   // Name to help debugging.
                LCDDisplay_task,                     // Pointer to the task's code.
                 DEF_NULL,                           // Pointer to task's argument.
                 LCD_TASK_PRIO,                      // Task's priority.
                &lcd_stack[0],                       // Pointer to base of stack.
                (LCD_TASK_STACK_SIZE / 10u) ,        // Stack limit, from base.
                LCD_TASK_STACK_SIZE,                 // Stack size, in CPU_STK.
                 0u,                                 // Messages in task queue.
                 0u,                                 // Round-Robin time quanta.
                 DEF_NULL,                           // External TCB data.
                 (OS_OPT_TASK_STK_CLR),              // Task options.
                &err);
   EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));

 }

 void Physics_Task_Init(void){

   RTOS_ERR     err;

   OSTaskCreate(&physics_tcb,                            // Pointer to the task's TCB.
                "Physics Task",                   // Name to help debugging.
                Physics_task,                     // Pointer to the task's code.
                 DEF_NULL,                           // Pointer to task's argument.
                 PHYSICS_TASK_PRIO,                      // Task's priority.
                &physics_stack[0],                       // Pointer to base of stack.
                (PHYSICS_TASK_STACK_SIZE / 10u) ,        // Stack limit, from base.
                PHYSICS_TASK_STACK_SIZE,                 // Stack size, in CPU_STK.
                 0u,                                 // Messages in task queue.
                 0u,                                 // Round-Robin time quanta.
                 DEF_NULL,                           // External TCB data.
                 (OS_OPT_TASK_STK_CLR),              // Task options.
                &err);
   EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));

 }

 void Capsense_Task_Init(void){

   RTOS_ERR     err;

   OSTaskCreate(&capsense_tcb,                            // Pointer to the task's TCB.
                "Capsense task",                   // Name to help debugging.
                Capsense_task,                     // Pointer to the task's code.
                 DEF_NULL,                           // Pointer to task's argument.
                 CAPSENSE_TASK_PRIO,                      // Task's priority.
                &capsense_stack[0],                       // Pointer to base of stack.
                (CAPSENSE_TASK_STACK_SIZE / 10u) ,        // Stack limit, from base.
                CAPSENSE_TASK_STACK_SIZE,                 // Stack size, in CPU_STK.
                 0u,                                 // Messages in task queue.
                 0u,                                 // Round-Robin time quanta.
                 DEF_NULL,                           // External TCB data.
                 (OS_OPT_TASK_STK_CLR),              // Task options.
                &err);
   EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));

 }

 void Button_Task_Init(void){

   RTOS_ERR     err;

   OSTaskCreate(&button0_tcb,                            // Pointer to the task's TCB.
                "Button task",                   // Name to help debugging.
                Button0_task,                     // Pointer to the task's code.
                 DEF_NULL,                           // Pointer to task's argument.
                 BUTTON0_TASK_PRIO,                      // Task's priority.
                &button0_stack[0],                       // Pointer to base of stack.
                (BUTTON0_TASK_STACK_SIZE / 10u) ,        // Stack limit, from base.
                BUTTON0_TASK_STACK_SIZE,                 // Stack size, in CPU_STK.
                 0u,                                 // Messages in task queue.
                 0u,                                 // Round-Robin time quanta.
                 DEF_NULL,                           // External TCB data.
                 (OS_OPT_TASK_STK_CLR),              // Task options.
                &err);
   EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));

   OSTaskCreate(&button1_tcb,                            // Pointer to the task's TCB.
                "Button task",                   // Name to help debugging.
                Button1_task,                     // Pointer to the task's code.
                 DEF_NULL,                           // Pointer to task's argument.
                 BUTTON1_TASK_PRIO,                      // Task's priority.
                &button1_stack[0],                       // Pointer to base of stack.
                (BUTTON1_TASK_STACK_SIZE / 10u) ,        // Stack limit, from base.
                BUTTON1_TASK_STACK_SIZE,                 // Stack size, in CPU_STK.
                 0u,                                 // Messages in task queue.
                 0u,                                 // Round-Robin time quanta.
                 DEF_NULL,                           // External TCB data.
                 (OS_OPT_TASK_STK_CLR),              // Task options.
                &err);
   EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));

 }



