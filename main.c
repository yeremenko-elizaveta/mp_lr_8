#include "mcu_support_package/inc/stm32f10x.h"
#include <math.h>
#define SET_SIZE( f ) (f / freqIntr_mcs )

static volatile uint32_t count = 0;      // Счётчик прерываний
static const uint16_t period = 72 * 6;   // Период таймера
static const uint32_t freqIntr_mcs = 6;  // Период вызова прерываний в мкс
static const double Pi = 3.14159265358979323846;  // Значение пи с точностью до 20-и знаков после запятой

// Длины массивов значений синусов
static const uint32_t sizeSinArr[8] = {SET_SIZE( 954 ), SET_SIZE( 852 ), SET_SIZE( 756 ), SET_SIZE( 714 ),
                                       SET_SIZE( 636 ), SET_SIZE( 582 ), SET_SIZE( 504 ), SET_SIZE( 480 )};

// Двумерный массив значений синусов. Каждая строка - значения синуса для одной частоты
static uint16_t sinArr[8][SET_SIZE( 954 )] = {0};

void SysTick_Handler (void) {
    count++;
}

// Функция, отвечающая за проигрывание звуков
static void play(uint32_t numBut) {
    // Индексы эл-тов из sinArr, которые воспроизвели в последний раз
    uint32_t index[8] = {0};
    
    // Пока нажата та же комбинация кнопок, играем
    while (numBut == (((GPIOA->IDR & 0xFE) >> 1) | ((GPIOC->IDR & GPIO_Pin_4) << 3))) {
        uint32_t prev = count;
        uint16_t countBut = 0;  // Сколько зажато кнопок
        uint16_t sound = 0;  // Звук, который надо проиграть
        
        // Суммируем значения синусов нажатых кнопок  
        for (uint32_t i = 0; i < 8; i++) {
            if (numBut & (1 << i)) {
                sound += sinArr[i][index[i]];
                index[i] = (index[i] + 1) % sizeSinArr[i];
                countBut++;
            }
        }
        
        // Вычисляем среднее и играем
        sound /= countBut;
        TIM_SetCompare1(TIM3, sound);
        while (count - prev < 1) { }
    }
}

// Функция отвечающая за сканирование клавиш и вызов play при необходдимости
static void scan() {
    // Сохраняем состояния кнопок PA1-PA7 и PC4
    uint32_t numBut = ((GPIOA->IDR & 0xFE) >> 1) | ((GPIOC->IDR & GPIO_Pin_4) << 3);  
    if (numBut > 0) {
        play(numBut);  // Проигрываем соответсвующие звуки
    }
    TIM_SetCompare1(TIM3, 0);
}

// Функция, заполняющая значениями синуса массив
static void fillSin(uint16_t sinArr[][SET_SIZE( 954 )], const uint32_t * sizeSinX) {
    for (uint32_t i = 0; i < 8; i++) {
        for (uint32_t j = 0; j < sizeSinX[i]; j++) {
            sinArr[i][j] = (sin((Pi * 2.0 * j / sizeSinX[i])) + 1.0) / 2 * period;  
        }
    }
}

int main(void)
{
    __disable_irq();
    
    // PC6 - динамик, PA1-PA7, PC4 - клавиатура
    // Подаём тактирование на порты А, С
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    
    // Настраиваем ножки для клавиатуры
    GPIO_InitTypeDef pinBut;
    pinBut.GPIO_Mode = GPIO_Mode_IPD;
    pinBut.GPIO_Pin  =  GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    pinBut.GPIO_Pin |= (GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7);
    GPIO_Init(GPIOA, &pinBut);
    pinBut.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOC, &pinBut);
    
    // Настраиваем ножку PC6 для динамика
    GPIO_InitTypeDef pinDyn;
    pinDyn.GPIO_Mode = GPIO_Mode_AF_PP;
    pinDyn.GPIO_Speed = GPIO_Speed_2MHz;
    pinDyn.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOC, &pinDyn);
    
    // Переназначаем ножку динамика
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);
    
    // Подаём тактирование
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    
    // Настраиваем таймер
    TIM_TimeBaseInitTypeDef timeBaseInitTypeDef;
    TIM_TimeBaseStructInit(&timeBaseInitTypeDef);
    timeBaseInitTypeDef.TIM_Prescaler = 1;
    timeBaseInitTypeDef.TIM_Period = period;
    TIM_TimeBaseInit(TIM3, &timeBaseInitTypeDef);
    
    // Настраиваем таймерный канал
    TIM_OCInitTypeDef ocInitTypeDef;
    TIM_OCStructInit(&ocInitTypeDef);
    ocInitTypeDef.TIM_OCMode = TIM_OCMode_PWM1;
    ocInitTypeDef.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OC1Init(TIM3, &ocInitTypeDef);
    
    // Заполняем массив синусов
    fillSin(sinArr, sizeSinArr);
    
    __enable_irq();
    
    // Вызываем прерываниия каждые 6 мкс
    SysTick_Config(SystemCoreClock / (1000 * 1000) * freqIntr_mcs); 
    
    // Запускаем таймер
    TIM_Cmd(TIM3, ENABLE);
    
    while(1) {
        scan();
    }

    return 0;
}









#ifdef USE_FULL_ASSERT

// эта функция вызывается, если assert_param обнаружил ошибку
void assert_failed(uint8_t * file, uint32_t line)
{ 
    /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
     
    (void)file;
    (void)line;

    __disable_irq();
    while(1)
    {
        // это ассемблерная инструкция "отладчик, стой тут"
        // если вы попали сюда, значит вы ошиблись в параметрах вызова функции из SPL. 
        // Смотрите в call stack, чтобы найти ее
        __BKPT(0xAB);
    }
}

#endif