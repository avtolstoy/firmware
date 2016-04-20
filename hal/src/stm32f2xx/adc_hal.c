/**
 ******************************************************************************
 * @file    adc_hal.c
 * @authors Satish Nair
 * @version V1.0.0
 * @date    22-Dec-2014
 * @brief
 ******************************************************************************
  Copyright (c) 2013-2015 Particle Industries, Inc.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include "adc_hal.h"
#include "gpio_hal.h"
#include "pinmap_hal.h"
#include "pinmap_impl.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

#define ADC_CDR_ADDRESS     ((uint32_t)0x40012308)
#define ADC_DMA_DEFAULT_SAMPLES  10
#define ADC_SAMPLING_TIME   ADC_SampleTime_480Cycles

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

static __IO uint16_t ADC_ConvertedValues[ADC_DMA_DEFAULT_SAMPLES * 2];
static uint8_t adcInitFirstTime = true;
static uint8_t adcChannelConfigured = ADC_CHANNEL_NONE;
static uint8_t ADC_Sample_Time = ADC_SAMPLING_TIME;
static uint32_t adcMode = ADC_Mode_Independent;
static uint32_t adcDmaMode = ADC_DMAAccessMode_Disabled;
static uint8_t adcReconfigure = false;

/* Extern variables ----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void HAL_ADC_Init();
static void HAL_ADC_DMA_Init(void* sample_buffer, uint32_t size);

/*
 * @brief  Override the default ADC Sample time depending on requirement
 * @param  ADC_SampleTime: The sample time value to be set.
 * This parameter can be one of the following values:
 * @arg ADC_SampleTime_3Cycles: Sample time equal to 3 cycles
 * @arg ADC_SampleTime_15Cycles: Sample time equal to 15 cycles
 * @arg ADC_SampleTime_28Cycles: Sample time equal to 28 cycles
 * @arg ADC_SampleTime_56Cycles: Sample time equal to 56 cycles
 * @arg ADC_SampleTime_84Cycles: Sample time equal to 84 cycles
 * @arg ADC_SampleTime_112Cycles: Sample time equal to 112 cycles
 * @arg ADC_SampleTime_144Cycles: Sample time equal to 144 cycles
 * @arg ADC_SampleTime_480Cycles: Sample time equal to 480 cycles
 * @retval None
 */
void HAL_ADC_Set_Sample_Time(uint8_t ADC_SampleTime)
{
    if(ADC_SampleTime < ADC_SampleTime_3Cycles || ADC_SampleTime > ADC_SampleTime_480Cycles)
    {
        ADC_Sample_Time = ADC_SAMPLING_TIME;
    }
    else
    {
        ADC_Sample_Time = ADC_SampleTime;
    }

    adcChannelConfigured = ADC_CHANNEL_NONE;
}

void HAL_ADC_Set_Mode(uint32_t mode, uint32_t dma_mode, void* reserved)
{
    // Only independent and dual modes are supported
    if (mode >= ADC_Mode_Independent && mode <= ADC_DualMode_AlterTrig)
    {
        adcMode = mode;
    }
    else
    {
        adcMode = ADC_Mode_Independent;
    }

    if (IS_ADC_DMA_ACCESS_MODE(dma_mode))
    {
        adcDmaMode = mode;
    }
    else
    {
        adcDmaMode = ADC_DMAAccessMode_2;
    }

    if (adcMode == ADC_Mode_Independent)
        adcDmaMode = ADC_DMAAccessMode_Disabled;

    adcReconfigure = true;
}

/*
 * @brief Read the analog value of a pin.
 * Should return a 16-bit value, 0-65536 (0 = LOW, 65536 = HIGH)
 * Note: ADC is 12-bit. Currently it returns 0-4096
 */
int32_t HAL_ADC_Read(uint16_t pin)
{
    uint32_t size = ADC_DMA_DEFAULT_SAMPLES;
    if (adcMode != ADC_Mode_Independent)
        size *= 2;
    int32_t samples = HAL_ADC_Read_Samples(pin, (void*)ADC_ConvertedValues, size * sizeof(uint16_t));

    if (samples == 0)
        return 0;

    uint32_t ADC_SummatedValue = 0;
    uint16_t ADC_AveragedValue = 0;

    for(int i = 0; i < samples; i++)
    {
        ADC_SummatedValue += ADC_ConvertedValues[i];
    }

    ADC_AveragedValue = (uint16_t)(ADC_SummatedValue / (samples));

    // Return ADC averaged value
    return ADC_AveragedValue;
}

/*
 * @brief Sample analog value of a pin N times and store each sample in the supplied array
 * Should return the number of samples acquired.
 */
int32_t HAL_ADC_Read_Samples(uint16_t pin, void* sample_buffer, uint32_t size)
{
    int i = 0;
    STM32_Pin_Info* PIN_MAP = HAL_Pin_Map();

    if (sample_buffer == NULL || size == 0)
        return 0;

    if ((adcChannelConfigured != PIN_MAP[pin].adc_channel) && (HAL_Get_Pin_Mode(pin) != AN_INPUT))
    {
        HAL_GPIO_Save_Pin_Mode(PIN_MAP[pin].pin_mode);
        HAL_Pin_Mode(pin, AN_INPUT);
    }

    if (adcInitFirstTime == true || adcReconfigure == true)
    {
        HAL_ADC_Init();
        adcInitFirstTime = false;
        adcReconfigure = false;
    }

    if (adcChannelConfigured != PIN_MAP[pin].adc_channel)
    {
        // ADC1 regular channel configuration
        ADC_RegularChannelConfig(ADC1, PIN_MAP[pin].adc_channel, 1, ADC_Sample_Time);
        // ADC2 regular channel configuration
        ADC_RegularChannelConfig(ADC2, PIN_MAP[pin].adc_channel, 1, ADC_Sample_Time);
        // Save the ADC configured channel
        adcChannelConfigured = PIN_MAP[pin].adc_channel;
    }

    for(i = 0; i < size; i++)
    {
        ((uint8_t*)sample_buffer)[i] = 0;
    }

    HAL_ADC_DMA_Init(sample_buffer, size);

    uint32_t samples = DMA_GetCurrDataCounter(DMA2_Stream0);

    // Enable DMA2_Stream0
    DMA_Cmd(DMA2_Stream0, ENABLE);

    if (adcMode != ADC_Mode_Independent)
    {
        // Enable DMA request after last transfer (Multi-ADC mode)
        ADC_MultiModeDMARequestAfterLastTransferCmd(ENABLE);

        // Enable ADC2
        ADC_Cmd(ADC2, ENABLE);
    }
    else
    {
        ADC_Cmd(ADC2, DISABLE);

        ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);

        ADC_DMACmd(ADC1, ENABLE);
    }

    // Enable ADC1
    ADC_Cmd(ADC1, ENABLE);

    // Start ADC1 Software Conversion
    ADC_SoftwareStartConv(ADC1);

    // Test on DMA2 Stream0 DMA_FLAG_TCIF0 flag
    while(!DMA_GetFlagStatus(DMA2_Stream0, DMA_FLAG_TCIF0));

    uint32_t counter = DMA_GetCurrDataCounter(DMA2_Stream0);

    // Clear DMA2 Stream0 DMA_FLAG_TCIF0 flag
    DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF0);

    // Disable DMA2_Stream0
    DMA_Cmd(DMA2_Stream0, DISABLE);

    if (adcMode == ADC_Mode_Independent)
    {
        ADC_DMARequestAfterLastTransferCmd(ADC1, DISABLE);
        ADC_DMACmd(ADC1, DISABLE);
    }
    else
    {
        // Disable DMA request after last transfer (Multi-ADC mode)
        ADC_MultiModeDMARequestAfterLastTransferCmd(DISABLE);
    }

    // Disable ADC1
    ADC_Cmd(ADC1, DISABLE);

    // Disable ADC2
    ADC_Cmd(ADC2, DISABLE);

    return (samples - counter);
}

/*
 * @brief Initialize the ADC peripheral.
 */
void HAL_ADC_Init()
{
    ADC_CommonInitTypeDef ADC_CommonInitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    if (adcInitFirstTime)
    {
        // Enable DMA2 clock
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

        // Enable ADC1 and ADC2 clock
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_ADC2, ENABLE);
    }

    // Disable ADC1
    ADC_Cmd(ADC1, DISABLE);

    // Disable ADC2
    ADC_Cmd(ADC2, DISABLE);

    /* ADC Common Init */
    ADC_CommonInitStructure.ADC_Mode = adcMode;
    ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;
    ADC_CommonInitStructure.ADC_DMAAccessMode = adcDmaMode;
    ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&ADC_CommonInitStructure);

    // ADC1 configuration
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // ADC2 configuration
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = 1;
    ADC_Init(ADC2, &ADC_InitStructure);
}

void HAL_ADC_DMA_Init(void* sample_buffer, uint32_t size)
{
    DMA_InitTypeDef DMA_InitStructure;
    uint32_t sample_size = sizeof(uint16_t);

    DMA_DeInit(DMA2_Stream0);
    // DMA2 Stream0 channel0 configuration
    DMA_InitStructure.DMA_Channel = DMA_Channel_0;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;

    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)sample_buffer;
    if (adcMode == ADC_Mode_Independent)
        DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    else
        DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)ADC_CDR_ADDRESS;

    // Calculate sample size
    if (adcMode != ADC_Mode_Independent && adcDmaMode == ADC_DMAAccessMode_2)
    {
        sample_size = sizeof(uint32_t);
    }
    DMA_InitStructure.DMA_BufferSize = size / sample_size;

    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    if (sample_size == sizeof(uint16_t))
    {
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    }
    else
    {
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    }

    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream0, &DMA_InitStructure);

    DMA_SetCurrDataCounter(DMA2_Stream0, DMA_InitStructure.DMA_BufferSize);
}
