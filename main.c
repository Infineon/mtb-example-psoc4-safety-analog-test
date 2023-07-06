/******************************************************************************
* File Name:   main.c
* 
* Description:
* This file provides example usage of Analog self tests for PSoC 4.
*
* Note that all hardware blocks instances and routing were chosen for this
* test is based on pin availability on the device's corresponding
* development kit. The self test demonstrated will work on all instances
* of the hardware block.
*
* Related Document:See README.md
*
********************************************************************************
* Copyright 2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
********************************************************************************/

/*******************************************************************************
* Includes
********************************************************************************/

#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

#include "SelfTest_Analog.h"

/*******************************************************************************
* Macros
********************************************************************************/

#define MAX_INDEX_VAL (0xFFF0u)

/*******************************************************************************
* Global Variables
*******************************************************************************/
static cy_stc_lpcomp_context_t lpcomp0_context;
static uint32_t sar_mux_switch_hw_ctl_save;
static uint32_t mux_switch_save;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
static void restore_user_sar_adc_config(void);
static void reconfig_sar_adc(const cy_stc_sar_config_t* config);
static void reconfig_ctb(CTBM_Type * base, cy_en_ctb_opamp_sel_t opampNum, const cy_stc_ctb_opamp_config_t * config );
static void reconfig_lpcomp(LPCOMP_Type* base, cy_en_lpcomp_channel_t channel,
                            const cy_stc_lpcomp_config_t* config, cy_stc_lpcomp_context_t* context);

/*******************************************************************************
* SAR ADC SELFTEST HW CONFIGS
*******************************************************************************/
static const cy_stc_sar_routing_config_t sar_adc0_selftest_sar_adc_routing_config =
{
        .muxSwitch =         (CY_SAR_MUX_FW_AMUXBUSA_VPLUS|
                            CY_SAR_MUX_FW_AMUXBUSB_VPLUS),
        .muxSwitchHwCtrl =     (CY_SAR_MUX_HW_CTRL_AMUXBUSA |
                            CY_SAR_MUX_HW_CTRL_AMUXBUSB),
};
static const cy_stc_sar_channel_config_t sar_adc_dut_selftest_sar_adc_channel_0_config =
{
    .addr = CY_SAR_ADDR_SARMUX_AMUXBUS_A,
    .differential = false,
    .resolution = CY_SAR_MAX_RES,
    .avgEn = false,
    .sampleTimeSel = CY_SAR_SAMPLE_TIME_0,
    .rangeIntrEn = false,
    .satIntrEn = false,
};
#if (ANALOG_TEST_VREF != ANALOG_TEST_VREF_CSD_IDAC)
static const cy_stc_sar_channel_config_t sar_adc_dut_selftest_sar_adc_channel_1_config =
{
    .addr = CY_SAR_ADDR_SARMUX_AMUXBUS_B,
    .differential = false,
    .resolution = CY_SAR_MAX_RES,
    .avgEn = false,
    .sampleTimeSel = CY_SAR_SAMPLE_TIME_1,
    .rangeIntrEn = false,
    .satIntrEn = false,
};
#endif /* ANALOG_TEST_VREF != ANALOG_TEST_VREF_CSD_IDAC */

/* This adc config assumes 1.6MHz clk with no bypass cap */
static const cy_stc_sar_config_t sar_adc_dut_selftest_sar_adc_config =
{
    .vrefSel = CY_SAR_VREF_SEL_VDDA_DIV_2,
    .vrefBypCapEn = false,
    .negSel = CY_SAR_NEG_SEL_VREF,
    .negVref = CY_SAR_NEGVREF_HW,
    .boostPump = false,
    .power = CY_SAR_QUARTER_PWR,
    .sarMuxDsEn = false,
    .switchDisable = false,
    .subResolution = CY_SAR_SUB_RESOLUTION_8B,
    .leftAlign = false,
    .singleEndedSigned = true,
    .differentialSigned = false,
    .avgCnt = CY_SAR_AVG_CNT_2,
    .avgShift = true,
    .trigMode = CY_SAR_TRIGGER_MODE_FW_ONLY,
    .eosEn = false,
    .sampleTime0 = 66,
    .sampleTime1 = 2,
    .sampleTime2 = 2,
    .sampleTime3 = 2,
    .rangeThresLow = 0UL,
    .rangeThresHigh = 0UL,
    .rangeCond = CY_SAR_RANGE_COND_BELOW,
#if (ANALOG_TEST_VREF == ANALOG_TEST_VREF_CSD_IDAC)
    .chanEn = 1UL,
    .channelConfig = {&sar_adc_dut_selftest_sar_adc_channel_0_config, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
#else
    .chanEn = 3UL,
    .channelConfig = {&sar_adc_dut_selftest_sar_adc_channel_0_config, &sar_adc_dut_selftest_sar_adc_channel_1_config, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
#endif /* ANALOG_TEST_VREF == ANALOG_TEST_VREF_CSD_IDAC */
    .routingConfig = &sar_adc0_selftest_sar_adc_routing_config,
    .vrefMvValue = CY_CFG_PWR_VDDA_MV/2,
};

/*******************************************************************************
* OPAMP SELFTEST HW CONFIGS
*******************************************************************************/
static const cy_stc_sar_routing_config_t opamp_selftest_sar_adc_routing_config =
{
        .muxSwitch =         CY_SAR_MUX_FW_SARBUS0_VPLUS,
        .muxSwitchHwCtrl =     CY_SAR_MUX_HW_CTRL_SARBUS0,
};
static const cy_stc_sar_channel_config_t opamp_selftest_sar_adc_channel_0_config =
{
    .addr = CY_SAR_ADDR_CTB0_OA0,
    .differential = false,
    .resolution = CY_SAR_MAX_RES,
    .avgEn = false,
    .sampleTimeSel = CY_SAR_SAMPLE_TIME_0,
    .rangeIntrEn = false,
    .satIntrEn = false,
};

/* This adc config assumes 1.6MHz clk with no bypass cap */
static const cy_stc_sar_config_t opamp_selftest_sar_adc_config =
{
    .vrefSel = CY_SAR_VREF_SEL_VDDA_DIV_2,
    .vrefBypCapEn = false,
    .negSel = CY_SAR_NEG_SEL_VREF,
    .negVref = CY_SAR_NEGVREF_HW,
    .boostPump = false,
    .power = CY_SAR_QUARTER_PWR,
    .sarMuxDsEn = false,
    .switchDisable = false,
    .subResolution = CY_SAR_SUB_RESOLUTION_8B,
    .leftAlign = false,
    .singleEndedSigned = true,
    .differentialSigned = false,
    .avgCnt = CY_SAR_AVG_CNT_2,
    .avgShift = true,
    .trigMode = CY_SAR_TRIGGER_MODE_FW_ONLY,
    .eosEn = false,
    .sampleTime0 = 66,
    .sampleTime1 = 2,
    .sampleTime2 = 2,
    .sampleTime3 = 2,
    .rangeThresLow = 0UL,
    .rangeThresHigh = 0UL,
    .rangeCond = CY_SAR_RANGE_COND_BELOW,
    .chanEn = 1UL,
    .channelConfig = {&opamp_selftest_sar_adc_channel_0_config, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    .routingConfig = &opamp_selftest_sar_adc_routing_config,
    .vrefMvValue = CY_CFG_PWR_VDDA_MV/2,
};


static const cy_stc_ctb_opamp_config_t opamp_dut_selftest_opamp_config =
{
    .power = CY_CTB_POWER_LOW,
    .outputMode = CY_CTB_MODE_OPAMP_INTERNAL,
    .pump = true,
    .compEdge = CY_CTB_COMP_EDGE_DISABLE,
    .compLevel = CY_CTB_COMP_TRIGGER_OUT_PULSE,
    .compBypass = false,
    .compHyst = false,
    .compIntrEn = false,
#if (defined(APP_CY8CKIT_045S) || \
      defined(APP_CY8CKIT_041S_MAX) )
    .switchCtrl =     CY_CTB_SW_OA1_POS_PIN5|
                    CY_CTB_SW_OA1_OUT_SARBUS0|
                    CY_CTB_SW_OA1_NEG_OUT,
#endif
};

/***************************************************************************
* LPCOMP SELFTEST HW CONFIGS
***************************************************************************/
static const cy_stc_lpcomp_config_t lpcomp_dut_selftest_comp_config =
{
    .outputMode = CY_LPCOMP_OUT_DIRECT,
    .hysteresis = CY_LPCOMP_HYST_ENABLE,
    .power = CY_LPCOMP_MODE_FAST,
    .intType= CY_LPCOMP_INTR_DISABLE,
};

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* The main function performs the following tasks:
*    1. Initializes the device, board peripherals, and retarget-io for prints.
*    2. Save user SARSEQ ADC routing and Initialize the ADC and AMUXBUS 
*    3. configure AMUXBUS routing and Connect VREF as per the selected test modes
*    4. Run Comparator test
*    5. Run ADC test
*    6. Run OpAmp test
*    7. The code restores the user's SAR ADC configurations
*    
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/

int main(void)
{
    uint16_t count = 0u;    
    cy_rslt_t result;
    cy_en_sar_status_t sar_res;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();
    
    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("\r\nClass B Safety test: Analog Peripherals\r\n");
#if (ANALOG_TEST_VREF == ANALOG_TEST_VREF_EXTERNAL)
    printf("\r\nANALOG_TEST_VREF_EXTERNAL\r\n");
#elif (ANALOG_TEST_VREF == ANALOG_TEST_VREF_DUAL_MSC)
    printf("\r\nANALOG_TEST_VREF_DUAL_MSC\r\n");
#elif (ANALOG_TEST_VREF == ANALOG_TEST_VREF_CSD_IDAC)
    printf("\r\nANALOG_TEST_VREF_CSD_IDAC\r\n");
#endif
    Cy_SysLib_Delay(2000);

    /* Save user SARSEQ ADC routing configured by cybsp_init */
    sar_mux_switch_hw_ctl_save = SAR_MUX_SWITCH_HW_CTRL(CYBSP_DUT_SAR_ADC_HW);
    mux_switch_save = SAR_MUX_SWITCH0(CYBSP_DUT_SAR_ADC_HW);

    /* Debug tap ins to AMUXBUS A and B*/
    Cy_GPIO_Pin_FastInit(CYBSP_AMUX_A_DEBUG_PORT, CYBSP_AMUX_A_DEBUG_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXA);
    Cy_GPIO_Pin_FastInit(CYBSP_AMUX_B_DEBUG_PORT, CYBSP_AMUX_B_DEBUG_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXB);

    /* Initialize ADC as per config structure and enable ADC */
    sar_res = Cy_SAR_Init(CYBSP_DUT_SAR_ADC_HW, &CYBSP_DUT_SAR_ADC_config);
    if (sar_res != CY_SAR_SUCCESS)
    {
        CY_ASSERT(0);
    }   
    Cy_SAR_Enable(CYBSP_DUT_SAR_ADC_HW);

    /* Using the whole AMUXBUS is ONE possible configuration for the analog test.
     * Configure the routing to your application needs */

    Cy_GPIO_SetAmuxSplit(AMUX_SPLIT_CTL_0, CY_GPIO_AMUX_LR, CY_GPIO_AMUXBUSA);
    Cy_GPIO_SetAmuxSplit(AMUX_SPLIT_CTL_1, CY_GPIO_AMUX_LR, CY_GPIO_AMUXBUSA);

    Cy_GPIO_SetAmuxSplit(AMUX_SPLIT_CTL_0, CY_GPIO_AMUX_LR, CY_GPIO_AMUXBUSB);
    Cy_GPIO_SetAmuxSplit(AMUX_SPLIT_CTL_1, CY_GPIO_AMUX_LR, CY_GPIO_AMUXBUSB);

#if (defined(CY_DEVICE_SERIES_PSOC_4100S_PLUS) && (CY_FLASH_SIZE == 0x00040000UL)) || \
    (defined(CY_DEVICE_SERIES_PSOC_4500S)) || \
    (defined(CY_DEVICE_SERIES_PSOC_4100S_MAX))
    Cy_GPIO_SetAmuxSplit(AMUX_SPLIT_CTL_2, CY_GPIO_AMUX_LR, CY_GPIO_AMUXBUSA);
    Cy_GPIO_SetAmuxSplit(AMUX_SPLIT_CTL_2, CY_GPIO_AMUX_LR, CY_GPIO_AMUXBUSB);
#endif

#if (ANALOG_TEST_VREF == ANALOG_TEST_VREF_DUAL_MSC)
    /* Configure both MSCv3 blocks to output the voltage references
     * to the AMUXBUS A and B
     * NOTE: User must deinit the MSCv3 block and return it to its default state */
    SelfTest_Init_MSCv3_Vref_Amux_A(CYBSP_MSC0_HW);
    SelfTest_Init_MSCv3_Vdda_Div2_Amux_B(CYBSP_MSC1_HW);
#endif /*ANALOG_TEST_VREF == ANALOG_TEST_VREF_DUAL_MSC*/

#if (ANALOG_TEST_VREF == ANALOG_TEST_VREF_EXTERNAL)
    /* Configure both 2 GPIO pins that are connected to the external voltage reference
     * and connect it to the AMUXBUS */
    Cy_GPIO_Pin_FastInit(CYBSP_EXT_VREF1_PORT, CYBSP_EXT_VREF1_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXA);
    Cy_GPIO_Pin_FastInit(CYBSP_EXT_VREF2_PORT, CYBSP_EXT_VREF2_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXB);
#endif /*ANALOG_TEST_VREF == ANALOG_TEST_VREF_EXTERNAL*/

#if (ANALOG_TEST_VREF == ANALOG_TEST_VREF_CSD_IDAC)
    /* Configure IDAC A output to AMUX A and IDAC B to AMUX B and connect the GPIO
     * that are connected to the pull down resistors to the AMUX Bus.
     */
    Cy_GPIO_Pin_FastInit(CYBSP_PULLDOWN_RES_1_PORT, CYBSP_PULLDOWN_RES_1_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXA);
    Cy_GPIO_Pin_FastInit(CYBSP_PULLDOWN_RES_2_PORT, CYBSP_PULLDOWN_RES_2_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXB);
    SelfTest_Init_CSDv2_Dual_IDAC_Out(CYBSP_CSD_HW);

    /* configure SAR ADC for IDAC calibration */
    reconfig_sar_adc(&sar_adc_dut_selftest_sar_adc_config);
    /* Calibrate the IDAC you plan to use for the ADC and Op-amp test */
    SelfTests_IDACA_Analog_Calibration(CYBSP_CSD_HW, CYBSP_DUT_SAR_ADC_HW);
    restore_user_sar_adc_config();

#endif /* ANALOG_TEST_VREF == ANALOG_TEST_VREF_CSD_IDAC */

    for(;;)
    {
        /***********************************************************************
         * Run Comparator Self Test...
         ***********************************************************************/

        /* Configure lp-comp for test */
        reconfig_lpcomp(LPCOMP, CYBSP_DUT_LPCOMP_CHANNEL , &lpcomp_dut_selftest_comp_config, &lpcomp0_context);

#if(ANALOG_TEST_VREF == ANALOG_TEST_VREF_CSD_IDAC)
        SelfTest_IDACA_SetValue(CYBSP_CSD_HW, ANALOG_CSD_IDAC_VALUE1);
        SelfTest_IDACB_SetValue(CYBSP_CSD_HW, ANALOG_CSD_IDAC_VALUE2);

        /* Delay to stabilize IDAC output */
        Cy_SysLib_DelayUs(IDAC_SETTLE_TIME);
#endif
        /* Apply higher voltage to positive input */
        Cy_GPIO_Pin_FastInit(CYBSP_DUT_LPCOMP_VPLUS_PORT, CYBSP_DUT_LPCOMP_VPLUS_PIN,CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXB);
        Cy_GPIO_Pin_FastInit(CYBSP_DUT_LPCOMP_VMINUS_PORT, CYBSP_DUT_LPCOMP_VMINUS_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXA);

        if(OK_STATUS != SelfTests_Comparator(LPCOMP, CYBSP_DUT_LPCOMP_CHANNEL, ANALOG_COMP_RESULT1))
        {
            /* Process error */
            printf("\r\nLPCOMP_V1 test: error\r\n");
            while(1u);
        }

        /* Apply lower voltage to positive input */
        Cy_GPIO_Pin_FastInit(CYBSP_DUT_LPCOMP_VPLUS_PORT, CYBSP_DUT_LPCOMP_VPLUS_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXA);
        Cy_GPIO_Pin_FastInit(CYBSP_DUT_LPCOMP_VMINUS_PORT, CYBSP_DUT_LPCOMP_VMINUS_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXB);

        if(OK_STATUS != SelfTests_Comparator(LPCOMP, CYBSP_DUT_LPCOMP_CHANNEL, ANALOG_COMP_RESULT2))
        {
            /* Process error */
            printf("\r\nLPCOMP_V2 test: error\r\n");
            while(1u);
        }

        /**************************************************************************
         * Run Op-amp Self Test...
         **************************************************************************/

        /* Configure SAR ADC to read from voltage from opamp */
        reconfig_sar_adc(&opamp_selftest_sar_adc_config);
        /* Configure DUT OpAmp to connect to GPIO as mux and output to SARBUS0 */
        reconfig_ctb(CYBSP_DUT_CTB_HW, CYBSP_DUT_OPAMP_OA_NUM, &opamp_dut_selftest_opamp_config);

        /* Configure input to positive Opamp terminal to AMUXA*/
        Cy_GPIO_Pin_FastInit(CYBSP_DUT_OPAMP_VPLUS_PORT, CYBSP_DUT_OPAMP_VPLUS_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXA);

#if(ANALOG_TEST_VREF == ANALOG_TEST_VREF_CSD_IDAC)
        SelfTest_IDACA_SetValue(CYBSP_CSD_HW, ANALOG_CSD_IDAC_VALUE1);

        /* Delay to stabilize IDAC output */
        Cy_SysLib_DelayUs(IDAC_SETTLE_TIME);
#endif
        if(OK_STATUS != SelfTests_Opamp(CYBSP_DUT_SAR_ADC_HW, ANALOG_OPAMP_SAR_RESULT1, ANALOG_OPAMP_ACURACCY))
        {
            /* Process error */
            printf("\r\nOPAMP_V1 test: error\r\n");
            while(1u);
        }

#if(ANALOG_TEST_VREF == ANALOG_TEST_VREF_CSD_IDAC)
        SelfTest_IDACA_SetValue(CYBSP_CSD_HW, ANALOG_CSD_IDAC_VALUE2);

        /* Delay to stabilize IDAC output */
        Cy_SysLib_DelayUs(IDAC_SETTLE_TIME);
#else
        /* Configure input to positive Opamp terminal to AMUXB*/
        Cy_GPIO_Pin_FastInit(CYBSP_DUT_OPAMP_VPLUS_PORT, CYBSP_DUT_OPAMP_VPLUS_PIN, CY_GPIO_DM_ANALOG, 0u, HSIOM_SEL_AMUXB);
#endif

        if(OK_STATUS != SelfTests_Opamp(CYBSP_DUT_SAR_ADC_HW, ANALOG_OPAMP_SAR_RESULT2, ANALOG_OPAMP_ACURACCY))
        {
            /* Process error */
            printf("\r\nOPAMP_V2 test: error\r\n");
            while(1u);
        }

        /*****************************************************************************
         * Run ADC Self Test...
         *****************************************************************************/

        /* configure SAR ADC for test */
        reconfig_sar_adc(&sar_adc_dut_selftest_sar_adc_config);

#if(ANALOG_TEST_VREF == ANALOG_TEST_VREF_CSD_IDAC)
        SelfTest_IDACA_SetValue(CYBSP_CSD_HW, ANALOG_CSD_IDAC_VALUE1);

        /* Delay to stabilize IDAC output */
        Cy_SysLib_DelayUs(IDAC_SETTLE_TIME);
#endif
        /* Test the first refrence voltage */
        if(OK_STATUS != SelfTests_ADC(CYBSP_DUT_SAR_ADC_HW, ANALOG_ADC_CHNL_VREF1, ANALOG_ADC_SAR_RESULT1, ANALOG_ADC_ACURACCY))
        {
            /* Process error */
            printf("\r\nADC_V1 test: error\r\n");
            while(1u);
        }

#if (ANALOG_TEST_VREF == ANALOG_TEST_VREF_CSD_IDAC)
        SelfTest_IDACA_SetValue(CYBSP_CSD_HW, ANALOG_CSD_IDAC_VALUE2);

        /* Delay to stabilize IDAC output */
        Cy_SysLib_DelayUs(IDAC_SETTLE_TIME);
#endif
        /* Test the second refrence voltage */
        if(OK_STATUS != SelfTests_ADC(CYBSP_DUT_SAR_ADC_HW, ANALOG_ADC_CHNL_VREF2, ANALOG_ADC_SAR_RESULT2, ANALOG_ADC_ACURACCY))
        {
            /* Process error */
            printf("\r\nADC_V2 test: error\r\n");
            while(1u);
        }

        /* restore user SAR ADC configs */
        restore_user_sar_adc_config();

        /* Print test counter */
        printf("\rAnalog peripherals testing... count=%d", count);
        
        count++;
        if (count > MAX_INDEX_VAL)
        {
            count = 0u;
        }

    }

}


/*****************************************************************************
* Function Name: reconfig_lpcomp
******************************************************************************
*
* Summary:
*  reconfigures the lpcomp based on given config
*
* Parameters:
*  LPCOMP_Type const* lpcomp_base - The low-power comparator registers
*     structure-pointer.
*  cy_en_lpcomp_channel_t lpcomp_channel - The low-power comparator channel number.
*  const cy_stc_lpcomp_config_t* config - The pointer to the configuration structure.
*  cy_stc_lpcomp_context_t* context - The pointer to the context structure.
*
* return:
*  void
*
*****************************************************************************/
void reconfig_lpcomp(LPCOMP_Type* base, const cy_en_lpcomp_channel_t channel,
        const cy_stc_lpcomp_config_t* config, cy_stc_lpcomp_context_t* context)
{
    cy_en_lpcomp_status_t lpcomp_res;
    
    Cy_LPComp_Disable(base, channel, context);
    lpcomp_res = Cy_LPComp_Init(base, channel , config, context);
    if (lpcomp_res != CY_LPCOMP_SUCCESS)
    {
        CY_ASSERT(0);
    }       
    Cy_LPComp_Enable(base, channel, context);
}

/*****************************************************************************
* Function Name: reconfig_ctb
******************************************************************************
*
* Summary:
*  reconfigures the ctb based on given config
*
* Parameters:
*   CTBM_Type * base - The pointer to structure-describing registers.
*   cy_en_ctb_opamp_sel_t opampNum - CY_CTB_OPAMP_0 or CY_CTB_OPAMP_1
*   const cy_stc_ctb_opamp_config_t * config - The pointer to a structure
*     containing configuration data.
*
* return:
*  void
*
*****************************************************************************/
void reconfig_ctb(CTBM_Type * base, cy_en_ctb_opamp_sel_t opampNum, const cy_stc_ctb_opamp_config_t * config )
{
    cy_en_ctb_status_t ctb_res;

    ctb_res = Cy_CTB_DeInit(base, true);
    if (ctb_res != CY_CTB_SUCCESS)
    {
        CY_ASSERT(0);
    }           
    ctb_res = Cy_CTB_OpampInit(base, opampNum, config);
    if (ctb_res != CY_CTB_SUCCESS)
    {
        CY_ASSERT(0);
    }               
    Cy_CTB_Enable(base);
    
}

/*****************************************************************************
* Function Name: reconfig_sar_adc
******************************************************************************
*
* Summary:
*  reconfigures the CYBSP_DUT_SAR_ADC based on given config
*
* Parameters:
*  const cy_stc_sar_config_t* config -     Pointer to structure containing
*    configuration data.
*
* return:
*  void
*
*****************************************************************************/
static void reconfig_sar_adc(const cy_stc_sar_config_t* config)
{
    cy_en_sar_status_t sar_res;

    /* take control of SAR ADC instance */
    sar_res = Cy_SAR_DeInit(CYBSP_DUT_SAR_ADC_HW, true);
    if (sar_res != CY_SAR_SUCCESS)
    {
        CY_ASSERT(0);
    }                   
    sar_res = Cy_SAR_Init(CYBSP_DUT_SAR_ADC_HW, config);
    if (sar_res != CY_SAR_SUCCESS)
    {
        CY_ASSERT(0);
    }                       
    Cy_SAR_Enable(CYBSP_DUT_SAR_ADC_HW);

}

/*****************************************************************************
* Function Name: restore_user_sar_adc_config
******************************************************************************
*
* Summary:
*  restores the SAR_ADC to how it was configured by the user BSP.
*
* Parameters:
*  const cy_stc_sar_config_t* config -     Pointer to structure containing
*    configuration data.
*
* return:
*  void
*
*****************************************************************************/
static void restore_user_sar_adc_config(void)
{
    cy_en_sar_status_t sar_res;

    sar_res = Cy_SAR_DeInit(CYBSP_DUT_SAR_ADC_HW, true);
    if (sar_res != CY_SAR_SUCCESS)
    {
        CY_ASSERT(0);
    }                       

    /* restore user SARSEQ ADC routing configuration */
    SAR_MUX_SWITCH_HW_CTRL(CYBSP_DUT_SAR_ADC_HW) = sar_mux_switch_hw_ctl_save;
    SAR_MUX_SWITCH0(CYBSP_DUT_SAR_ADC_HW) = mux_switch_save;

    /* restore user configuration */
    sar_res = Cy_SAR_Init(CYBSP_DUT_SAR_ADC_HW, &CYBSP_DUT_SAR_ADC_config);
    if (sar_res != CY_SAR_SUCCESS)
    {
        CY_ASSERT(0);
    }                           
    Cy_SAR_Enable(CYBSP_DUT_SAR_ADC_HW);
}
/* [] END OF FILE */
