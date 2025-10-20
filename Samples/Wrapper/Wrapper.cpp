//===========================================================================
// Copyright (C) 2022 Intel Corporation
//
//
//
// SPDX-License-Identifier: MIT
//--------------------------------------------------------------------------

/**
 *
 * @file  Wrapper.cpp
 * @brief This file contains the 'main' function. Program execution begins and ends there.
 *
 */

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
using namespace std;

#define CTL_APIEXPORT // caller of control API DLL shall define this before
                      // including igcl_api.h
#include "igcl_api.h"
#include "GenericIGCLApp.h"
#include "ColorAlgorithms_App.h"
#include "GenericIGCLApp.h"


/* -------------- Definitions and variables from Color_Sample_App.cpp ------------------*/
ctl_result_t GResult = CTL_RESULT_SUCCESS;

#define SATURATION_FACTOR_BASE 1.0
#define DEFAULT_COLOR_SAT SATURATION_FACTOR_BASE
#define MIN_COLOR_SAT (SATURATION_FACTOR_BASE - 0.25)
#define MAX_COLOR_SAT (SATURATION_FACTOR_BASE + 0.25)
#define UV_MAX_POINT 0.4375
#define UV_MIN_POINT1 0.0029297
#define UV_MIN_POINT2 0.01074
#define UV_MAX_VAL 255.0
#define CLIP_DOUBLE(Val, Min, Max) (((Val) < (Min)) ? (Min) : (((Val) > (Max)) ? (Max) : (Val)))
#define ABS_DOUBLE(x) ((x) < 0 ? (-x) : (x))
#define DEFAULT_GAMMA_VALUE 2.2

// Convert RGB to YUV
const double RGB2YCbCr709[3][3] = { { 0.2126, 0.7152, 0.0722 }, { -0.1146, -0.3854, 0.5000 }, { 0.5000, -0.4542, -0.0458 } };

// Convert YUV to RGB
const double YCbCr2RGB709[3][3] = { { 1.0000, 0.0000, 1.5748 }, { 1.0000, -0.1873, -0.4681 }, { 1.0000, 1.8556, 0.0000 } };

// convert RGB to XYZ
const double RGB2XYZ_709[3][3] = { { 0.41239080, 0.35758434, 0.18048079 }, { 0.21263901, 0.71516868, 0.07219232 }, { 0.01933082, 0.11919478, 0.95053215 } };

const double Pi    = 3.1415926535897932;
const double Beta  = 0.018053968510807;
const double Alpha = 1.09929682680944;

typedef struct
{
    double R;
    double G;
    double B;
} IPIXELD;

typedef struct
{
    double Y;
    double Cb;
    double Cr;
} IPIXELD_YCbCr;

typedef enum HUE_ANCHOR
{
    HUE_ANCHOR_RED_SATURATION         = 0,
    HUE_ANCHOR_YELLOW_SATURATION      = 1,
    HUE_ANCHOR_GREEN_SATURATION       = 2,
    HUE_ANCHOR_CYAN_SATURATION        = 3,
    HUE_ANCHOR_BLUE_SATURATION        = 4,
    HUE_ANCHOR_MAGENTA_SATURATION     = 5,
    HUE_ANCHOR_COLOR_COUNT_SATURATION = 6
} HUE_ANCHOR;

typedef enum
{
    GAMMA_ENCODING_TYPE_SRGB    = 0, // Gamma encoding SRGB
    GAMMA_ENCODING_TYPE_REC709  = 1, // Gamma encoding REC709
    GAMMA_ENCODING_TYPE_REC2020 = 2, // Gamma encoding REC2020
} GAMMA_ENCODING_TYPE;

// https://en.wikipedia.org/wiki/CIE_1931_color_space
typedef struct
{
    double X;
    double Y;
    double Z;
} IPIXEL_XYZ;

// https://en.wikipedia.org/wiki/CIELAB_color_space
typedef struct
{
    double L;
    double A;
    double B;
} IPIXEL_Lab;

typedef struct
{
    double RedSat;
    double YellowSat;
    double GreenSat;
    double CyanSat;
    double BlueSat;
    double MagentaSat;
} PARTIAL_SATURATION_WEIGHTS;

const IPIXEL_XYZ RefWhitePointSRGB = { 95.0489, 100.0, 108.8840 };


/* -------------------------------------------------------------- */

typedef struct ctl_telemetry_data
{
    // GPU TDP
    bool gpuEnergySupported = false;
    double gpuEnergyValue;

    // GPU Voltage
    bool gpuVoltageSupported = false;
    double gpuVoltagValue;

    // GPU Core Frequency
    bool gpuCurrentClockFrequencySupported = false;
    double gpuCurrentClockFrequencyValue;

    // GPU Core Temperature
    bool gpuCurrentTemperatureSupported = false;
    double gpuCurrentTemperatureValue;

    // GPU Usage
    bool globalActivitySupported = false;
    double globalActivityValue;

    // Render Engine Usage
    bool renderComputeActivitySupported = false;
    double renderComputeActivityValue;

    // Media Engine Usage
    bool mediaActivitySupported = false;
    double mediaActivityValue;

    // VRAM Power Consumption
    bool vramEnergySupported = false;
    double vramEnergyValue;

    // VRAM Voltage
    bool vramVoltageSupported = false;
    double vramVoltageValue;

    // VRAM Frequency
    bool vramCurrentClockFrequencySupported = false;
    double vramCurrentClockFrequencyValue;

    // VRAM Read Bandwidth
    bool vramReadBandwidthSupported = false;
    double vramReadBandwidthValue;

    // VRAM Write Bandwidth
    bool vramWriteBandwidthSupported = false;
    double vramWriteBandwidthValue;

    // VRAM Temperature
    bool vramCurrentTemperatureSupported = false;
    double vramCurrentTemperatureValue;

    // Fanspeed (n Fans)
    bool fanSpeedSupported = false;
    double fanSpeedValue;
};

typedef struct ctl_fps_limiter_t
{
    bool isLimiterEnabled = false;
    int32_t fpsLimitValue = 30;
};

ctl_api_handle_t hAPIHandle;
ctl_device_adapter_handle_t* hDevices;

extern "C" {

    double deltatimestamp = 0;
    double prevtimestamp = 0;
    double curtimestamp = 0;
    double prevgpuEnergyCounter = 0;
    double curgpuEnergyCounter = 0;
    double curglobalActivityCounter = 0;
    double prevglobalActivityCounter = 0;
    double currenderComputeActivityCounter = 0;
    double prevrenderComputeActivityCounter = 0;
    double curmediaActivityCounter = 0;
    double prevmediaActivityCounter = 0;
    double curvramEnergyCounter = 0;
    double prevvramEnergyCounter = 0;
    double curvramReadBandwidthCounter = 0;
    double prevvramReadBandwidthCounter = 0;
    double curvramWriteBandwidthCounter = 0;
    double prevvramWriteBandwidthCounter = 0;

    ctl_result_t GetRetroScalingCaps(ctl_device_adapter_handle_t hDevice, ctl_retro_scaling_caps_t* RetroScalingCaps)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;
        RetroScalingCaps->Size = sizeof(ctl_retro_scaling_caps_t);

        Result = ctlGetSupportedRetroScalingCapability(hDevice, RetroScalingCaps);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSupportedRetroScalingCapability");

        cout << "======== GetRetroScalingCaps ========" << endl;
        cout << "RetroScalingCaps.SupportedRetroScaling: " << RetroScalingCaps->SupportedRetroScaling << endl;

    Exit:
        return Result;
    }

    ctl_result_t GetRetroScalingSettings(ctl_device_adapter_handle_t hDevice, ctl_retro_scaling_settings_t* RetroScalingSettings)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;
        RetroScalingSettings->Get = TRUE;
        RetroScalingSettings->Size = sizeof(ctl_retro_scaling_settings_t);

        Result = ctlGetSetRetroScaling(hDevice, RetroScalingSettings);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSetRetroScaling");

        cout << "======== GetRetroScalingSettings ========" << endl;
        cout << "RetroScalingSettings.Get: " << RetroScalingSettings->Get << endl;
        cout << "RetroScalingSettings.Enable: " << RetroScalingSettings->Enable << endl;
        cout << "RetroScalingSettings.RetroScalingType: " << RetroScalingSettings->RetroScalingType << endl;

    Exit:
        return Result;
    }

    ctl_result_t SetRetroScalingSettings(ctl_device_adapter_handle_t hDevice, ctl_retro_scaling_settings_t RetroScalingSettings)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;
        RetroScalingSettings.Get = FALSE;
        RetroScalingSettings.Size = sizeof(ctl_retro_scaling_settings_t);

        Result = ctlGetSetRetroScaling(hDevice, &RetroScalingSettings);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSetRetroScaling");

        cout << "======== SetRetroScalingSettings ========" << endl;
        cout << "RetroScalingSettings.Get: " << RetroScalingSettings.Get << endl;
        cout << "RetroScalingSettings.Enable: " << RetroScalingSettings.Enable << endl;
        cout << "RetroScalingSettings.RetroScalingType: " << RetroScalingSettings.RetroScalingType << endl;

    Exit:
        return Result;
    }

    ctl_result_t GetScalingCaps(ctl_device_adapter_handle_t hDevice, uint32_t idx, ctl_scaling_caps_t* ScalingCaps)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        ctl_display_output_handle_t* hDisplayOutput = NULL;
        uint32_t DisplayCount = 0;
        ScalingCaps->Size = sizeof(ctl_scaling_caps_t);

        // Enumerate all the possible target display's for the adapters
        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        if (DisplayCount <= 0)
        {
            printf("Invalid Display Count\n");
            goto Exit;
        }

        hDisplayOutput = (ctl_display_output_handle_t*)malloc(sizeof(ctl_display_output_handle_t) * DisplayCount);
        EXIT_ON_MEM_ALLOC_FAILURE(hDisplayOutput, "hDisplayOutput");

        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        Result = ctlGetSupportedScalingCapability(hDisplayOutput[idx], ScalingCaps);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSupportedScalingCapability");

        cout << "======== GetScalingCaps ========" << endl;
        cout << "ScalingCaps.SupportedScaling: " << ScalingCaps->SupportedScaling << endl;

    Exit:
        CTL_FREE_MEM(hDisplayOutput);
        return Result;
    }

    ctl_result_t GetScalingSettings(ctl_device_adapter_handle_t hDevice, uint32_t idx, ctl_scaling_settings_t* ScalingSetting)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        ctl_display_output_handle_t* hDisplayOutput = NULL;
        uint32_t DisplayCount = 0;
        ctl_scaling_caps_t ScalingCaps = { 0 };
        ScalingSetting->Size = sizeof(ctl_scaling_settings_t);

        Result = GetScalingCaps(hDevice, idx, &ScalingCaps);
        LOG_AND_EXIT_ON_ERROR(Result, "GetScalingCaps");

        // Enumerate all the possible target display's for the adapters
        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        if (DisplayCount <= 0)
        {
            printf("Invalid Display Count\n");
            goto Exit;
        }

        hDisplayOutput = (ctl_display_output_handle_t*)malloc(sizeof(ctl_display_output_handle_t) * DisplayCount);
        EXIT_ON_MEM_ALLOC_FAILURE(hDisplayOutput, "hDisplayOutput");

        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        if (0 != ScalingCaps.SupportedScaling)
        {
            Result = ctlGetCurrentScaling(hDisplayOutput[idx], ScalingSetting);
            LOG_AND_EXIT_ON_ERROR(Result, "ctlGetCurrentScaling");

            cout << "======== GetScalingSettings ========" << endl;
            cout << "ScalingSetting.Enable: " << ScalingSetting->Enable << endl;
            cout << "ScalingSetting.ScalingType: " << ScalingSetting->ScalingType << endl;
            cout << "ScalingSetting.HardwareModeSet: " << ScalingSetting->HardwareModeSet << endl;
            cout << "ScalingSetting.CustomScalingX: " << ScalingSetting->CustomScalingX << endl;
            cout << "ScalingSetting.CustomScalingY: " << ScalingSetting->CustomScalingY << endl;
        }

    Exit:
        CTL_FREE_MEM(hDisplayOutput);
        return Result;
    }

    ctl_result_t SetScalingSettings(ctl_device_adapter_handle_t hDevice, uint32_t idx, ctl_scaling_settings_t ScalingSetting)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        ctl_display_output_handle_t* hDisplayOutput = NULL;
        uint32_t DisplayCount = 0;
        ctl_scaling_caps_t ScalingCaps = { 0 };
        bool ModeSet;

        Result = GetScalingCaps(hDevice, idx, &ScalingCaps);
        LOG_AND_EXIT_ON_ERROR(Result, "GetScalingCaps");

        // Enumerate all the possible target display's for the adapters
        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        if (DisplayCount <= 0)
        {
            printf("Invalid Display Count\n");
            goto Exit;
        }

        hDisplayOutput = (ctl_display_output_handle_t*)malloc(sizeof(ctl_display_output_handle_t) * DisplayCount);
        EXIT_ON_MEM_ALLOC_FAILURE(hDisplayOutput, "hDisplayOutput");

        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        if (0 != ScalingCaps.SupportedScaling)
        {
            // filling custom scaling details
            ScalingSetting.Size = sizeof(ctl_scaling_settings_t);

            // fill custom scaling details only if it is supported
            if (0x1F == ScalingCaps.SupportedScaling)
            {
                // check if hardware modeset required to apply custom scaling
                ModeSet = ((TRUE == ScalingSetting.Enable) && (CTL_SCALING_TYPE_FLAG_CUSTOM == ScalingSetting.ScalingType)) ? FALSE : TRUE;
                ScalingSetting.HardwareModeSet = (TRUE == ModeSet) ? TRUE : FALSE;
            }

            cout << "======== SetScalingSettings ========" << endl;
            cout << "ScalingSetting.Enable: " << ScalingSetting.Enable << endl;
            cout << "ScalingSetting.ScalingType: " << ScalingSetting.ScalingType << endl;
            cout << "ScalingSetting.HardwareModeSet: " << ScalingSetting.HardwareModeSet << endl;
            cout << "ScalingSetting.CustomScalingX: " << ScalingSetting.CustomScalingX << endl;
            cout << "ScalingSetting.CustomScalingY: " << ScalingSetting.CustomScalingY << endl;
        }

        Result = ctlSetCurrentScaling(hDisplayOutput[idx], &ScalingSetting);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlSetCurrentScaling");

    Exit:
        CTL_FREE_MEM(hDisplayOutput);
        return Result;
    }

    ctl_result_t GetSharpnessCaps(ctl_device_adapter_handle_t hDevice, uint32_t idx, ctl_sharpness_caps_t* SharpnessCaps)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        ctl_display_output_handle_t* hDisplayOutput = NULL;
        uint32_t DisplayCount = 0;
        SharpnessCaps->Size = sizeof(ctl_sharpness_caps_t);

        // Enumerate all the possible target display's for the adapters
        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        if (DisplayCount <= 0)
        {
            printf("Invalid Display Count\n");
            goto Exit;
        }

        hDisplayOutput = (ctl_display_output_handle_t*)malloc(sizeof(ctl_display_output_handle_t) * DisplayCount);
        EXIT_ON_MEM_ALLOC_FAILURE(hDisplayOutput, "hDisplayOutput");

        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        // Get Sharpness caps
        Result = ctlGetSharpnessCaps(hDisplayOutput[idx], SharpnessCaps);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSharpnessCaps");

        cout << "======== GetSharpnessCaps ========" << endl;
        cout << "SharpnessCaps.SupportedFilterFlags: " << SharpnessCaps->SupportedFilterFlags << endl;

    Exit:
        CTL_FREE_MEM(hDisplayOutput);
        return Result;
    }

    ctl_result_t GetSharpnessSettings(ctl_device_adapter_handle_t hDevice, uint32_t idx, ctl_sharpness_settings_t* GetSharpness)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        ctl_display_output_handle_t* hDisplayOutput = NULL;
        uint32_t DisplayCount = 0;
        GetSharpness->Size = sizeof(ctl_sharpness_settings_t);

        // Enumerate all the possible target display's for the adapters
        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        if (DisplayCount <= 0)
        {
            printf("Invalid Display Count\n");
            goto Exit;
        }

        hDisplayOutput = (ctl_display_output_handle_t*)malloc(sizeof(ctl_display_output_handle_t) * DisplayCount);
        EXIT_ON_MEM_ALLOC_FAILURE(hDisplayOutput, "hDisplayOutput");

        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        Result = ctlGetCurrentSharpness(hDisplayOutput[idx], GetSharpness);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetCurrentSharpness");

        cout << "======== GetSharpnessSettings ========" << endl;
        cout << "GetSharpness.Enable: " << GetSharpness->Enable << endl;
        cout << "GetSharpness.FilterType: " << GetSharpness->FilterType << endl;
        cout << "GetSharpness.Intensity: " << GetSharpness->Intensity << endl;

    Exit:
        CTL_FREE_MEM(hDisplayOutput);
        return Result;
    }

    ctl_result_t SetSharpnessSettings(ctl_device_adapter_handle_t hDevice, uint32_t idx, ctl_sharpness_settings_t SetSharpness)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        ctl_display_output_handle_t* hDisplayOutput = NULL;
        uint32_t DisplayCount = 0;

        // Enumerate all the possible target display's for the adapters
        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        if (DisplayCount <= 0)
        {
            printf("Invalid Display Count\n");
            goto Exit;
        }

        hDisplayOutput = (ctl_display_output_handle_t*)malloc(sizeof(ctl_display_output_handle_t) * DisplayCount);
        EXIT_ON_MEM_ALLOC_FAILURE(hDisplayOutput, "hDisplayOutput");

        Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

        cout << "======== SetSharpnessSettings ========" << endl;
        cout << "SetSharpness.Enable: " << SetSharpness.Enable << endl;
        cout << "SetSharpness.FilterType: " << SetSharpness.FilterType << endl;
        cout << "SetSharpness.Intensity: " << SetSharpness.Intensity << endl;

        Result = ctlSetCurrentSharpness(hDisplayOutput[idx], &SetSharpness);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlSetCurrentSharpness");

    Exit:
        CTL_FREE_MEM(hDisplayOutput);
        return Result;
    }

    ctl_result_t GetEnduranceGamingCaps(ctl_device_adapter_handle_t hDevice,
        ctl_endurance_gaming_caps_t* pCaps)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        // --- First pass: get number of 3D features ---
        ctl_3d_feature_caps_t FeatureCaps3D = { 0 };
        FeatureCaps3D.Size = sizeof(ctl_3d_feature_caps_t);

        Result = ctlGetSupported3DCapabilities(hDevice, &FeatureCaps3D);
        if (Result != CTL_RESULT_SUCCESS)
        {
            APP_LOG_ERROR("ctlGetSupported3DCapabilities failed (pass 1): 0x%X", Result);
            return Result;
        }

        if (FeatureCaps3D.NumSupportedFeatures == 0)
        {
            APP_LOG_ERROR("No 3D features supported?");
            return CTL_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        // --- Allocate details array and zero it ---
        auto details = (ctl_3d_feature_details_t*)
            malloc(sizeof(ctl_3d_feature_details_t) * FeatureCaps3D.NumSupportedFeatures);
        if (!details)
            return CTL_RESULT_ERROR_INVALID_NULL_POINTER;

        memset(details, 0, sizeof(ctl_3d_feature_details_t) * FeatureCaps3D.NumSupportedFeatures);

        // --- Second pass: fill in the details array ---
        FeatureCaps3D.pFeatureDetails = details;
        Result = ctlGetSupported3DCapabilities(hDevice, &FeatureCaps3D);
        if (Result != CTL_RESULT_SUCCESS)
        {
            APP_LOG_ERROR("ctlGetSupported3DCapabilities failed (pass 2): 0x%X", Result);
            free(details);
            return Result;
        }

        // --- Search for the Endurance Gaming feature ---
        bool found = false;
        for (uint32_t i = 0; i < FeatureCaps3D.NumSupportedFeatures; ++i)
        {
            auto& D = details[i];
            if (D.FeatureType == CTL_3D_FEATURE_ENDURANCE_GAMING)
            {
                // if it's a custom property, we need to allocate pCustomValue
                if (D.ValueType == CTL_PROPERTY_VALUE_TYPE_CUSTOM && D.CustomValueSize > 0)
                {
                    D.pCustomValue = malloc(D.CustomValueSize);
                    if (!D.pCustomValue)
                    {
                        Result = CTL_RESULT_ERROR_INVALID_NULL_POINTER;
                        break;
                    }

                    // single-feature query to fill just this one custom block
                    ctl_3d_feature_caps_t single = { 0 };
                    single.Size = sizeof(single);
                    single.Version = 0;
                    single.NumSupportedFeatures = 1;
                    single.pFeatureDetails = &D;

                    Result = ctlGetSupported3DCapabilities(hDevice, &single);
                    if (Result != CTL_RESULT_SUCCESS)
                    {
                        APP_LOG_ERROR("Single-feature ctlGetSupported3DCapabilities failed: 0x%X", Result);
                    }
                }

                if (Result == CTL_RESULT_SUCCESS)
                {
                    // copy the filled‐in ctl_endurance_gaming_caps_t out
                    auto igclCaps = reinterpret_cast<ctl_endurance_gaming_caps_t*>(D.pCustomValue);
                    *pCaps = *igclCaps;
                    found = true;
                }

                // clean up
                if (D.pCustomValue)
                {
                    free(D.pCustomValue);
                    D.pCustomValue = nullptr;
                }

                break;
            }
        }

        if (!found && Result == CTL_RESULT_SUCCESS)
            Result = CTL_RESULT_ERROR_UNSUPPORTED_FEATURE;

        // final cleanup
        free(details);
        return Result;
    }

    ctl_result_t GetEnduranceGamingSettings(ctl_device_adapter_handle_t hDevice, ctl_endurance_gaming_t* pSettings)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        // zero out output
        memset(pSettings, 0, sizeof(*pSettings));

        // build the get structure
        ctl_3d_feature_getset_t Feature3D = { 0 };
        Feature3D.bSet = FALSE;  // GET
        Feature3D.FeatureType = CTL_3D_FEATURE_ENDURANCE_GAMING;
        Feature3D.Size = sizeof(Feature3D);
        Feature3D.ValueType = CTL_PROPERTY_VALUE_TYPE_CUSTOM;
        Feature3D.CustomValueSize = sizeof(*pSettings);
        Feature3D.pCustomValue = pSettings;

        // issue the call
        Result = ctlGetSet3DFeature(hDevice, &Feature3D);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSet3DFeature (GetEnduranceGamingSettings)");

        // log for debug
        cout << "======== GetEnduranceGamingSettings ========" << endl;
        cout << "EGControl:    " << pSettings->EGControl << endl;
        cout << "EGMode:       " << pSettings->EGMode << endl;
        /*
        cout << "IsFPRequired: " << pSettings->IsFPRequired << endl;
        cout << "TargetFPS:    " << pSettings->TargetFPS << endl;
        cout << "RefreshRate:  " << pSettings->RefreshRate << endl;
        */

    Exit:
        return Result;
    }

    ctl_result_t SetEnduranceGamingSettings(ctl_device_adapter_handle_t hDevice, ctl_endurance_gaming_t settings)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        // pack into v2 struct so we can use same API
        ctl_endurance_gaming_t eg2 = { 0 };
        eg2.EGControl = settings.EGControl;
        eg2.EGMode = settings.EGMode;

        // build the set structure
        ctl_3d_feature_getset_t Feature3D = { 0 };
        Feature3D.bSet = TRUE;   // SET
        Feature3D.FeatureType = CTL_3D_FEATURE_ENDURANCE_GAMING;
        Feature3D.Size = sizeof(Feature3D);
        Feature3D.ValueType = CTL_PROPERTY_VALUE_TYPE_CUSTOM;
        Feature3D.CustomValueSize = sizeof(eg2);
        Feature3D.pCustomValue = &eg2;

        // issue the call
        Result = ctlGetSet3DFeature(hDevice, &Feature3D);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSet3DFeature (SetEnduranceGamingSettings)");

        // log for debug
        cout << "======== SetEnduranceGamingSettings ========" << endl;
        cout << "EGControl now: " << eg2.EGControl << endl;
        cout << "EGMode now:    " << eg2.EGMode << endl;

    Exit:
        return Result;
    }


    ctl_result_t SetFramesPerSecondLimit(ctl_device_adapter_handle_t hDevice, bool isEnabled, int32_t fpslimit)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        // build the set structure
        ctl_3d_feature_getset_t Feature3D = { 0 };
        Feature3D.bSet = TRUE;
        Feature3D.FeatureType = CTL_3D_FEATURE_FRAME_LIMIT;
        Feature3D.Size = sizeof(Feature3D);
        Feature3D.ValueType = CTL_PROPERTY_VALUE_TYPE_INT32;
        Feature3D.CustomValueSize =  0;
        Feature3D.pCustomValue = NULL;
        Feature3D.Value.IntType.Enable = isEnabled; // Enable or Disable FPS limiter
        Feature3D.Value.IntType.Value  = fpslimit; // Set the actual frame limit
        Feature3D.Version              = 0;


        // issue the call
        Result = ctlGetSet3DFeature(hDevice, &Feature3D);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSet3DFeature (SetFramesPerSecondLimit)");

        // log for debug
        cout << "======== SetFramesPerSecondLimit ========" << endl;
        cout << "IntType.Enable: " << isEnabled << endl;
        cout << "IntType.Value:    " << fpslimit << endl;

    Exit:
        return Result;
    }


    ctl_result_t GetFramesPerSecondLimit(ctl_device_adapter_handle_t hDevice, ctl_fps_limiter_t* fpslimiter)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        // build the get structure
        ctl_3d_feature_getset_t Feature3D = { 0 };
        Feature3D.bSet = FALSE;  // GET
        Feature3D.FeatureType = CTL_3D_FEATURE_FRAME_LIMIT;
        Feature3D.Size = sizeof(Feature3D);
        Feature3D.ValueType = CTL_PROPERTY_VALUE_TYPE_INT32;
        Feature3D.CustomValueSize = 0;
        Feature3D.pCustomValue = NULL;
        Feature3D.Version     = 0;

        // issue the call
        Result = ctlGetSet3DFeature(hDevice, &Feature3D);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSet3DFeature (GetFramesPerSecondLimit)");

        // log for debug
        cout << "======== GetFramesPerSecondLimit ========" << endl;
        cout << "Value.IntType.Enable:    " << Feature3D.Value.IntType.Enable << endl;
        cout << "Value.IntType.Value:    " << Feature3D.Value.IntType.Value << endl;

        fpslimiter->isLimiterEnabled = Feature3D.Value.IntType.Enable;
        fpslimiter->fpsLimitValue = Feature3D.Value.IntType.Value;

    Exit:
        return Result;
    }


    ctl_result_t SetLowLatencySetting(ctl_device_adapter_handle_t hDevice, ctl_3d_low_latency_types_t setting)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        // build the set structure
        ctl_3d_feature_getset_t Feature3D = { 0 };
        Feature3D.bSet = TRUE;
        Feature3D.FeatureType = CTL_3D_FEATURE_LOW_LATENCY;
        Feature3D.Size = sizeof(Feature3D);
        Feature3D.CustomValueSize = 0;
        Feature3D.pCustomValue = NULL;
        Feature3D.ValueType = CTL_PROPERTY_VALUE_TYPE_ENUM;
        Feature3D.Value.EnumType.EnableType = setting; // Set Low Latency Setting
        Feature3D.Version = 0;

        // issue the call
        Result = ctlGetSet3DFeature(hDevice, &Feature3D);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSet3DFeature (SetLowLatencySetting)");

        // log for debug
        cout << "======== SetLowLatencySetting ========" << endl;
        cout << "Value.EnumType.EnableType: " << setting << endl;


    Exit:
        return Result;
    }


    ctl_result_t GetLowLatencySetting(ctl_device_adapter_handle_t hDevice, ctl_3d_low_latency_types_t* setting)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        // build the get structure
        ctl_3d_feature_getset_t Feature3D = { 0 };
        Feature3D.bSet = FALSE;  // GET
        Feature3D.FeatureType = CTL_3D_FEATURE_LOW_LATENCY;
        Feature3D.Size = sizeof(Feature3D);
        Feature3D.ValueType = CTL_PROPERTY_VALUE_TYPE_ENUM;
        Feature3D.CustomValueSize = 0;
        Feature3D.pCustomValue = NULL;
        Feature3D.Version = 0;

        // issue the call
        Result = ctlGetSet3DFeature(hDevice, &Feature3D);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSet3DFeature (GetFramesPerSecondLimit)");

        // log for debug
        cout << "======== GetFramesPerSecondLimit ========" << endl;
        *setting = (ctl_3d_low_latency_types_t)Feature3D.Value.EnumType.EnableType;
        cout << "Value.EnumType.EnableType:    " << *setting << endl;

    Exit:
        return Result;
    }


    ctl_result_t SetFrameSyncSetting(ctl_device_adapter_handle_t hDevice, ctl_gaming_flip_mode_flag_t setting)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        // build the set structure
        ctl_3d_feature_getset_t Feature3D = { 0 };
        Feature3D.bSet = TRUE;
        Feature3D.FeatureType = CTL_3D_FEATURE_GAMING_FLIP_MODES;
        Feature3D.Size = sizeof(Feature3D);
        Feature3D.CustomValueSize = 0;
        Feature3D.pCustomValue = NULL;
        Feature3D.ValueType = CTL_PROPERTY_VALUE_TYPE_ENUM;
        Feature3D.Value.EnumType.EnableType = setting; // Set Frame Sync Type
        Feature3D.Version = 0;

        // issue the call
        Result = ctlGetSet3DFeature(hDevice, &Feature3D);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSet3DFeature (SetFrameSyncSetting)");

        // log for debug
        cout << "======== SetFrameSyncSetting ========" << endl;
        cout << "Value.EnumType.EnableType: " << setting << endl;


    Exit:
        return Result;
    }


    ctl_result_t GetFrameSyncSetting(ctl_device_adapter_handle_t hDevice, ctl_gaming_flip_mode_flag_t* setting)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        // build the get structure
        ctl_3d_feature_getset_t Feature3D = { 0 };
        Feature3D.bSet = FALSE;  // GET
        Feature3D.FeatureType = CTL_3D_FEATURE_GAMING_FLIP_MODES;
        Feature3D.Size = sizeof(Feature3D);
        Feature3D.ValueType = CTL_PROPERTY_VALUE_TYPE_ENUM;
        Feature3D.CustomValueSize = 0;
        Feature3D.pCustomValue = NULL;
        Feature3D.Version = 0;

        // issue the call
        Result = ctlGetSet3DFeature(hDevice, &Feature3D);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetSet3DFeature (GetFrameSyncSetting)");

        // log for debug
        cout << "======== GetFrameSyncSetting ========" << endl;
        *setting = (ctl_gaming_flip_mode_flag_t)Feature3D.Value.EnumType.EnableType;
        cout << "Value.EnumType.EnableType:    " << *setting << endl;

    Exit:
        return Result;
    }

    // Get the list of Intel device handles
    ctl_device_adapter_handle_t* EnumerateDevices(uint32_t* pAdapterCount)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;
        hDevices = NULL;

        // Get the number of Intel Adapters
        Result = ctlEnumerateDevices(hAPIHandle, pAdapterCount, hDevices);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDevices");

        // Allocate memory for the device handles
        hDevices = (ctl_device_adapter_handle_t*)malloc(sizeof(ctl_device_adapter_handle_t) * (*pAdapterCount));
        EXIT_ON_MEM_ALLOC_FAILURE(hDevices, "EnumerateDevices");

        // Get the device handles
        Result = ctlEnumerateDevices(hAPIHandle, pAdapterCount, hDevices);
        LOG_AND_EXIT_ON_ERROR(Result, "ctlEnumerateDevices");

    Exit:
        return hDevices;
    }

    /***************************************************************
 * @brief
 * GenerateHueSaturationMatrix
 * @param Hue, Saturation, CoEff[3][3]
 * @return void
 ***************************************************************/
    void GenerateHueSaturationMatrix(double Hue, double Saturation, double CoEff[3][3])
    {
        double HueShift = Hue * Pi / 180.0;
        double C = cos(HueShift);
        double S = sin(HueShift);
        double HueRotationMatrix[3][3] = { { 1.0, 0.0, 0.0 }, { 0.0, C, -S }, { 0.0, S, C } };
        double SaturationEnhancementMatrix[3][3] = { { 1.0, 0.0, 0.0 }, { 0.0, Saturation, 0.0 }, { 0.0, 0.0, Saturation } };
        double YCbCr2RGB709[3][3] = { { 1.0000, 0.0000, 1.5748 }, { 1.0000, -0.1873, -0.4681 }, { 1.0000, 1.8556, 0.0000 } };
        double RGB2YCbCr709[3][3] = { { 0.2126, 0.7152, 0.0722 }, { -0.1146, -0.3854, 0.5000 }, { 0.5000, -0.4542, -0.0458 } };

        double Result[3][3];

        // Use Bt.709 coefficients for RGB to YCbCr conversion
        MatrixMult3x3(YCbCr2RGB709, SaturationEnhancementMatrix, Result);
        MatrixMult3x3(Result, HueRotationMatrix, Result);
        MatrixMult3x3(Result, RGB2YCbCr709, Result);

        memcpy_s(CoEff, sizeof(Result), Result, sizeof(Result));
    }

 /***************************************************************
 * @brief
 * ApplyHueSaturation
 * General steps for Hue Saturaion
 * Create a CSC matrix based on mentioned algorithm for given hue-sat values
 * Do a set CSC call with created matrix.
 * Iterate through blocks PixTxCaps and find the CSC block.
 * @param hDisplayOutput , pPixTxCaps, CscBlockIndex, Hue, Saturation
 * @return ctl_result_t
 ***************************************************************/
ctl_result_t ApplyHueSaturation(ctl_display_output_handle_t hDisplayOutput, ctl_pixtx_pipe_get_config_t *pPixTxCaps, int32_t CscBlockIndex, double Hue, double Saturation)
{
    ctl_result_t Result                = CTL_RESULT_SUCCESS;
    ctl_pixtx_block_config_t CSCConfig = pPixTxCaps->pBlockConfigs[CscBlockIndex];
    CSCConfig.Size                     = sizeof(ctl_pixtx_block_config_t);
    Saturation                         = CLIP_DOUBLE(Saturation, 0.0, 2.5);
    Hue                                = CLIP_DOUBLE(Hue, 0, 359);

    if ((0 == Hue) && (0 == Saturation))
    {
        // If Hue and Saturation both are set to default, CSC coefficients should be identity matrix.
        memset(CSCConfig.Config.MatrixConfig.Matrix, 0, sizeof(CSCConfig.Config.MatrixConfig.Matrix));

        CSCConfig.Config.MatrixConfig.Matrix[0][0] = CSCConfig.Config.MatrixConfig.Matrix[1][1] = CSCConfig.Config.MatrixConfig.Matrix[2][2] = 1.0;
    }
    else
    {
        // Below function generates CSC matrix(Non Linear) for given Hue and Satuartion values.
        GenerateHueSaturationMatrix(Hue, Saturation, CSCConfig.Config.MatrixConfig.Matrix);
    }

    ctl_pixtx_pipe_set_config_t SetPixTxArgs = { 0 };
    SetPixTxArgs.Size                        = sizeof(ctl_pixtx_pipe_set_config_t);
    SetPixTxArgs.OpertaionType               = CTL_PIXTX_CONFIG_OPERTAION_TYPE_SET_CUSTOM;
    SetPixTxArgs.NumBlocks                   = 1;          // We are trying to set only one block
    SetPixTxArgs.pBlockConfigs               = &CSCConfig; // for CSC block
    SetPixTxArgs.pBlockConfigs->BlockId      = pPixTxCaps->pBlockConfigs[CscBlockIndex].BlockId;

    Result = ctlPixelTransformationSetConfig(hDisplayOutput, &SetPixTxArgs);
    LOG_AND_EXIT_ON_ERROR(Result, "ctlPixelTransformationSetConfig");

Exit:
    return Result;
}

/***************************************************************
 * @brief
 * GetSRGBDecodingValue
 * @param Input
 * @return double
 ***************************************************************/
double GetSRGBDecodingValue(double Input)
{

    // https://en.wikipedia.org/wiki/SRGB#Transfer_function_(%22gamma%22)

    double Output;

    if (Input <= 0.04045)
    {
        Output = Input / 12.92;
    }
    else
    {
        Output = pow(((Input + 0.055) / 1.055), 2.4);
    }

    return Output;
}

/***************************************************************
 * @brief
 * GetSRGBEncodingValue
 * @param Input
 * @return double
 ***************************************************************/
double GetSRGBEncodingValue(double Input)
{
    /*
    https://en.wikipedia.org/wiki/SRGB#The_forward_transformation_.28CIE_xyY_or_CIE_XYZ_to_sRGB.29
    */

    double Output;

    if (Input <= 0.0031308)
    {
        Output = Input * 12.92;
    }
    else
    {
        Output = (1.055 * pow(Input, 1.0 / 2.4)) - 0.055;
    }

    return Output;
}


/***************************************************************
 * @brief
 * Set DeGamma Lut
 * @param hDisplayOutput
 * @param *pPixTxCaps
 * @param DGLUTIndex
 * @return
 ***************************************************************/
ctl_result_t SetDeGammaLut(ctl_display_output_handle_t hDisplayOutput, ctl_pixtx_pipe_get_config_t* pPixTxCaps, int32_t DGLUTIndex)
{
    ctl_result_t Result = CTL_RESULT_SUCCESS;

    if (nullptr == hDisplayOutput)
    {
        return CTL_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (nullptr == pPixTxCaps)
    {
        return CTL_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    ctl_pixtx_pipe_set_config_t SetPixTxArgs = { 0 };
    ctl_pixtx_block_config_t LutConfig = pPixTxCaps->pBlockConfigs[DGLUTIndex];
    LutConfig.Size = sizeof(ctl_pixtx_block_config_t);
    LutConfig.Config.OneDLutConfig.pSamplePositions = NULL;
    double* pLut;

    // Create a valid 1D LUT.
    const uint32_t LutSize = LutConfig.Config.OneDLutConfig.NumSamplesPerChannel * LutConfig.Config.OneDLutConfig.NumChannels;
    LutConfig.Config.OneDLutConfig.pSampleValues = (double*)malloc(LutSize * sizeof(double));

    EXIT_ON_MEM_ALLOC_FAILURE(LutConfig.Config.OneDLutConfig.pSampleValues, " LutConfig.Config.OneDLutConfig.pSampleValues");

    memset(LutConfig.Config.OneDLutConfig.pSampleValues, 0, LutSize * sizeof(double));

    SetPixTxArgs.Size = sizeof(ctl_pixtx_pipe_set_config_t);
    SetPixTxArgs.OpertaionType = CTL_PIXTX_CONFIG_OPERTAION_TYPE_SET_CUSTOM;
    SetPixTxArgs.NumBlocks = 1;          // We are enabling only one block
    SetPixTxArgs.pBlockConfigs = &LutConfig; // for 1DLUT block

    pLut = LutConfig.Config.OneDLutConfig.pSampleValues;

    for (uint32_t i = 0; i < (LutSize / LutConfig.Config.OneDLutConfig.NumChannels); i++)
    {
        double Input = (double)i / (double)(LutSize - 1);
        pLut[i] = GetSRGBDecodingValue(Input);
        // pLut[i] = Input; // unity
    }

    Result = ctlPixelTransformationSetConfig(hDisplayOutput, &SetPixTxArgs);

    LOG_AND_STORE_RESET_RESULT_ON_ERROR(Result, "ctlPixelTransformationSetConfig");

Exit:
    CTL_FREE_MEM(LutConfig.Config.OneDLutConfig.pSampleValues);
    return Result;
}

/***************************************************************
 * @brief
 * Get DeGamma
 * @param ctl_display_output_handle_t ,ctl_pixtx_pipe_get_config_t, int32_t
 * @return
 ***************************************************************/
ctl_result_t GetDeGammaLut(ctl_display_output_handle_t hDisplayOutput, ctl_pixtx_pipe_get_config_t* pPixTxCaps, int32_t DGLUTIndex)
{
    ctl_result_t Result = CTL_RESULT_SUCCESS;

    if (nullptr == hDisplayOutput)
    {
        return CTL_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (nullptr == pPixTxCaps)
    {
        return CTL_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    ctl_pixtx_pipe_get_config_t GetPixTxCurrentArgs = { 0 };
    GetPixTxCurrentArgs.Size = sizeof(ctl_pixtx_pipe_get_config_t);
    ctl_pixtx_block_config_t LutConfig = pPixTxCaps->pBlockConfigs[DGLUTIndex];

    GetPixTxCurrentArgs.QueryType = CTL_PIXTX_CONFIG_QUERY_TYPE_CURRENT;
    GetPixTxCurrentArgs.NumBlocks = 1;          // We are trying to query only one block
    GetPixTxCurrentArgs.pBlockConfigs = &LutConfig; // Providing Lut config
    LutConfig.Config.OneDLutConfig.pSampleValues = (double*)malloc(LutConfig.Config.OneDLutConfig.NumSamplesPerChannel * LutConfig.Config.OneDLutConfig.NumChannels * sizeof(double));

    EXIT_ON_MEM_ALLOC_FAILURE(LutConfig.Config.OneDLutConfig.pSampleValues, "LutConfig.Config.OneDLutConfig.pSampleValues");

    memset(LutConfig.Config.OneDLutConfig.pSampleValues, 0, LutConfig.Config.OneDLutConfig.NumSamplesPerChannel * LutConfig.Config.OneDLutConfig.NumChannels * sizeof(double));

    Result = ctlPixelTransformationGetConfig(hDisplayOutput, &GetPixTxCurrentArgs);
    LOG_AND_EXIT_ON_ERROR(Result, "ctlPixelTransformationGetConfig");

    APP_LOG_INFO("DEGamma values : LutConfig.Config.OneDLutConfig.pSampleValues");

    uint32_t LutDataSize = LutConfig.Config.OneDLutConfig.NumSamplesPerChannel * LutConfig.Config.OneDLutConfig.NumChannels;
    APP_LOG_INFO("LutDataSize = %d ", LutDataSize);

    APP_LOG_INFO("DeGamma values : LutConfig.Config.OneDLutConfig.pSampleValues");

    for (uint32_t i = 0; i < LutDataSize; i++)
    {
        APP_LOG_INFO("[%d] = %f", i, LutConfig.Config.OneDLutConfig.pSampleValues[i]);
    }

Exit:
    CTL_FREE_MEM(LutConfig.Config.OneDLutConfig.pSampleValues);
    return Result;
}

/***************************************************************
 * @brief
 * SetGammaLut
 * @param hDisplayOutput ,pPixTxCaps, OneDLUTIndex
 * @return ctl_result_t
 ***************************************************************/
ctl_result_t SetGammaLut(ctl_display_output_handle_t hDisplayOutput, ctl_pixtx_pipe_get_config_t* pPixTxCaps, int32_t OneDLUTIndex)
{
    ctl_result_t Result = CTL_RESULT_SUCCESS;
    ctl_pixtx_block_config_t LutConfig = pPixTxCaps->pBlockConfigs[OneDLUTIndex];

    LutConfig.Size = sizeof(ctl_pixtx_block_config_t);
    LutConfig.Config.OneDLutConfig.SamplingType = CTL_PIXTX_LUT_SAMPLING_TYPE_UNIFORM;
    LutConfig.Config.OneDLutConfig.NumChannels = 3;
    LutConfig.Config.OneDLutConfig.pSamplePositions = NULL;

    ctl_pixtx_pipe_set_config_t SetPixTxArgs = { 0 };
    SetPixTxArgs.Size = sizeof(ctl_pixtx_pipe_set_config_t);
    SetPixTxArgs.OpertaionType = CTL_PIXTX_CONFIG_OPERTAION_TYPE_SET_CUSTOM;
    SetPixTxArgs.NumBlocks = 1;          // We are enabling only one block
    SetPixTxArgs.pBlockConfigs = &LutConfig; // for 1DLUT block

    double* pRedLut, * pGreenLut, * pBlueLut;
    double LutMultiplier;

    // Create a valid 1D LUT.
    const uint32_t LutSize = LutConfig.Config.OneDLutConfig.NumSamplesPerChannel * LutConfig.Config.OneDLutConfig.NumChannels;
    LutConfig.Config.OneDLutConfig.pSampleValues = (double*)malloc(LutSize * sizeof(double));

    EXIT_ON_MEM_ALLOC_FAILURE(LutConfig.Config.OneDLutConfig.pSampleValues, " LutConfig.Config.OneDLutConfig.pSampleValues");

    pRedLut = LutConfig.Config.OneDLutConfig.pSampleValues;
    pGreenLut = pRedLut + LutConfig.Config.OneDLutConfig.NumSamplesPerChannel;
    pBlueLut = pGreenLut + LutConfig.Config.OneDLutConfig.NumSamplesPerChannel;

    // Applying a LUT which reduces the Red channel values by 30%.in linear format
    // Based on the Pixel Encoding type , encode the multiplier 0.7 with the same function
    // For example if Encoding is ST2084 then OETF2084(0.7) -> 0.962416136
    //             if Encoding is SRGB   then sRGB(0.7)     -> 0.854305832

    if (CTL_PIXTX_GAMMA_ENCODING_TYPE_ST2084 == pPixTxCaps->OutputPixelFormat.EncodingType)
    {
        LutMultiplier = 0.962416136;
    }
    else if (CTL_PIXTX_GAMMA_ENCODING_TYPE_SRGB == pPixTxCaps->OutputPixelFormat.EncodingType)
    {
        LutMultiplier = 0.854305832;
    }
    else
    {
        LutMultiplier = 1.0;
    }

    // When calling Set for just OneDLUT the LUT is expected to be a Relative Correction LUT
    for (uint32_t i = 0; i < LutConfig.Config.OneDLutConfig.NumSamplesPerChannel; i++)
    {
        double Input = (double)i / (double)(LutConfig.Config.OneDLutConfig.NumSamplesPerChannel - 1);
        pRedLut[i] = pGreenLut[i] = pBlueLut[i] = Input;
        pRedLut[i] *= LutMultiplier;
    }

    Result = ctlPixelTransformationSetConfig(hDisplayOutput, &SetPixTxArgs);
    LOG_AND_STORE_RESET_RESULT_ON_ERROR(Result, "ctlPixelTransformationSetConfig");

Exit:
    CTL_FREE_MEM(LutConfig.Config.OneDLutConfig.pSampleValues);
    return Result;
}

/***************************************************************
 * @brief
 * GetGammaLut
 * @param hDisplayOutput ,pPixTxCaps, OneDLUTIndex
 * @return ctl_result_t
 ***************************************************************/
ctl_result_t GetGammaLut(ctl_display_output_handle_t hDisplayOutput, ctl_pixtx_pipe_get_config_t* pPixTxCaps, int32_t OneDLUTIndex)
{
    ctl_pixtx_pipe_get_config_t GetPixTxCurrentArgs = { 0 };
    GetPixTxCurrentArgs.Size = sizeof(ctl_pixtx_pipe_get_config_t);
    ctl_pixtx_block_config_t LutConfig = pPixTxCaps->pBlockConfigs[OneDLUTIndex];
    ctl_result_t Result = CTL_RESULT_SUCCESS;

    GetPixTxCurrentArgs.QueryType = CTL_PIXTX_CONFIG_QUERY_TYPE_CURRENT;
    GetPixTxCurrentArgs.NumBlocks = 1;          // We are trying to query only one block
    GetPixTxCurrentArgs.pBlockConfigs = &LutConfig; // Providing Lut config
    LutConfig.Config.OneDLutConfig.pSampleValues = (double*)malloc(LutConfig.Config.OneDLutConfig.NumSamplesPerChannel * LutConfig.Config.OneDLutConfig.NumChannels * sizeof(double));

    EXIT_ON_MEM_ALLOC_FAILURE(LutConfig.Config.OneDLutConfig.pSampleValues, "LutConfig.Config.OneDLutConfig.pSampleValues");

    memset(LutConfig.Config.OneDLutConfig.pSampleValues, 0, LutConfig.Config.OneDLutConfig.NumSamplesPerChannel * LutConfig.Config.OneDLutConfig.NumChannels * sizeof(double));

    Result = ctlPixelTransformationGetConfig(hDisplayOutput, &GetPixTxCurrentArgs);
    LOG_AND_EXIT_ON_ERROR(Result, "ctlPixelTransformationGetConfig");

    uint32_t LutDataSize = LutConfig.Config.OneDLutConfig.NumSamplesPerChannel * LutConfig.Config.OneDLutConfig.NumChannels;
    APP_LOG_INFO("LutDataSize = %d ", LutDataSize);

    APP_LOG_INFO("Gamma values : LutConfig.Config.OneDLutConfig.pSampleValues");

    for (uint32_t i = 0; i < LutDataSize; i++)
    {
        APP_LOG_INFO("[%d] = %f", i, LutConfig.Config.OneDLutConfig.pSampleValues[i]);
    }

Exit:
    CTL_FREE_MEM(LutConfig.Config.OneDLutConfig.pSampleValues);
    return Result;
}

/***************************************************************
 * @brief
 * Get Set DeGamma
 * Iterate through blocks PixTxCaps and find the LUT.
 * @param ctl_display_output_handle_t ,ctl_pixtx_pipe_get_config_t
 * @return
 ***************************************************************/
void GetSetDeGamma(ctl_display_output_handle_t hDisplayOutput, ctl_pixtx_pipe_get_config_t *pPixTxCaps)
{
    // One approach could be check for CSC with offsets block, the block right before CSC with offset block is DGLUT.

    ctl_result_t Result = CTL_RESULT_SUCCESS;
    int32_t DGLUTIndex  = -1;

    for (uint32_t i = 0; i < pPixTxCaps->NumBlocks; i++)
    {
        if ((CTL_PIXTX_BLOCK_TYPE_1D_LUT == pPixTxCaps->pBlockConfigs[i].BlockType) && (CTL_PIXTX_BLOCK_TYPE_3X3_MATRIX_AND_OFFSETS == pPixTxCaps->pBlockConfigs[i + 1].BlockType))
        {
            DGLUTIndex = i;
            break;
        }
    }

    if (DGLUTIndex < 0)
    {
        APP_LOG_ERROR("Invalid DGLut Index");
        goto Exit;
    }

    // Set DeGamma
    Result = SetDeGammaLut(hDisplayOutput, pPixTxCaps, DGLUTIndex);
    if (CTL_RESULT_SUCCESS != Result)
    {
        APP_LOG_ERROR("SetDeGammaLut call failed");
        STORE_AND_RESET_ERROR(Result);
    }
    else
    {
        // Get DeGamma
        Result = GetDeGammaLut(hDisplayOutput, pPixTxCaps, DGLUTIndex);
        if (CTL_RESULT_SUCCESS != Result)
        {
            APP_LOG_ERROR("GetDeGamma call failed");
            LOG_AND_STORE_RESET_RESULT_ON_ERROR(Result, "GetDeGammaLut");
        }
    }

Exit:
    return;
}

/***************************************************************
 * @brief
 * Get Set Gamma
 * Iterate through blocks PixTxCaps and find the LUT.
 * @param ctl_display_output_handle_t ,ctl_pixtx_pipe_get_config_t
 * @return
 ***************************************************************/
void GetSetGamma(ctl_display_output_handle_t hDisplayOutput, ctl_pixtx_pipe_get_config_t *pPixTxCaps)
{
    ctl_result_t Result = CTL_RESULT_SUCCESS;

    // One approach could be check for CSC with offsets block, the block right after CSC with offset block is GLUT.
    // In HDR mode only one 1DLUT block will be reported and that is of GLUT. So, need to take last occurrence of 1DLUT block in consideration.

    int32_t OneDLUTIndex = -1;

    for (uint8_t i = 0; i < pPixTxCaps->NumBlocks; i++)
    {
        // Need to consider the last 1DLUT block for Gamma
        if (CTL_PIXTX_BLOCK_TYPE_1D_LUT == pPixTxCaps->pBlockConfigs[i].BlockType)
        {
            OneDLUTIndex = i;
        }
    }

    if (OneDLUTIndex < 0)
    {
        APP_LOG_ERROR("Invalid OneDLut Index");
        goto Exit;
    }

    // Set Gamma
    Result = SetGammaLut(hDisplayOutput, pPixTxCaps, OneDLUTIndex);

    if (CTL_RESULT_SUCCESS != Result)
    {
        APP_LOG_ERROR("SetGammaLut call failed");
        STORE_AND_RESET_ERROR(Result);
    }
    else
    {
        // Get Gamma
        Result = GetGammaLut(hDisplayOutput, pPixTxCaps, OneDLUTIndex);
        LOG_AND_STORE_RESET_RESULT_ON_ERROR(Result, "GetGammaLut");
    }

Exit:
    return;
}

/***************************************************************
 * @brief
 * ApplyLinearCSC DGLUT->CSC->GLUT
 * @param hDisplayOutput
 * @param pPixTxCaps
 * @return
 ***************************************************************/
ctl_result_t ApplyLinearCSC(ctl_display_output_handle_t hDisplayOutput, ctl_pixtx_pipe_get_config_t *pPixTxCaps)
{
    ctl_result_t Result = CTL_RESULT_SUCCESS;

    if (nullptr == hDisplayOutput)
    {
        return CTL_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (nullptr == pPixTxCaps)
    {
        return CTL_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    // One approach could be check for CSC with offsets block, the block right before CSC with offset block is DGLUT and the Block Right after CSC with Offsets block is GLUT.

    // In HDR mode only one 1DLUT block will be reported and that is of GLUT. So, need to take last occurrence of 1DLUT block in consideration.

    int32_t DGLUTIndex, CscIndex, GLUTIndex;
    DGLUTIndex = CscIndex = GLUTIndex = -1;
    uint32_t OneDLutOccurances        = 0;

    for (uint32_t i = 0; i < pPixTxCaps->NumBlocks; i++)
    {
        if (CTL_PIXTX_BLOCK_TYPE_1D_LUT == pPixTxCaps->pBlockConfigs[i].BlockType)
        {
            if ((CTL_PIXTX_BLOCK_TYPE_3X3_MATRIX_AND_OFFSETS == pPixTxCaps->pBlockConfigs[i + 1].BlockType) && (CTL_PIXTX_BLOCK_TYPE_1D_LUT == pPixTxCaps->pBlockConfigs[i + 2].BlockType))
            {
                DGLUTIndex = i;
                CscIndex   = i + 1;
                GLUTIndex  = i + 2;
            }
            break;
        }
    }

    if (DGLUTIndex < 0 || CscIndex < 0 || GLUTIndex < 0)
    {
        APP_LOG_ERROR("Invalid Index for DGLUT/CSC/GLUT");
        return CTL_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ctl_pixtx_block_config_t DGLUTConfig = pPixTxCaps->pBlockConfigs[DGLUTIndex];
    ctl_pixtx_block_config_t CSCConfig   = pPixTxCaps->pBlockConfigs[CscIndex];
    ctl_pixtx_block_config_t GLUTConfig  = pPixTxCaps->pBlockConfigs[GLUTIndex];

    ctl_pixtx_pipe_set_config_t SetPixTxArgs = { 0 };
    SetPixTxArgs.Size                        = sizeof(ctl_pixtx_pipe_set_config_t);
    SetPixTxArgs.OpertaionType               = CTL_PIXTX_CONFIG_OPERTAION_TYPE_SET_CUSTOM;
    SetPixTxArgs.NumBlocks                   = 3; // We are trying to set only one block
    SetPixTxArgs.pBlockConfigs               = (ctl_pixtx_block_config_t *)malloc(SetPixTxArgs.NumBlocks * sizeof(ctl_pixtx_block_config_t));
    EXIT_ON_MEM_ALLOC_FAILURE(SetPixTxArgs.pBlockConfigs, " SetPixTxArgs.pBlockConfigs");

    memset(SetPixTxArgs.pBlockConfigs, 0, SetPixTxArgs.NumBlocks * sizeof(ctl_pixtx_block_config_t));

    // DGLUT values
    const uint32_t DGLutSize                                                = DGLUTConfig.Config.OneDLutConfig.NumSamplesPerChannel * DGLUTConfig.Config.OneDLutConfig.NumChannels;
    SetPixTxArgs.pBlockConfigs[0].BlockId                                   = DGLUTConfig.BlockId;
    SetPixTxArgs.pBlockConfigs[0].BlockType                                 = DGLUTConfig.BlockType;
    SetPixTxArgs.pBlockConfigs[0].Config.OneDLutConfig.NumChannels          = DGLUTConfig.Config.OneDLutConfig.NumChannels;
    SetPixTxArgs.pBlockConfigs[0].Config.OneDLutConfig.NumSamplesPerChannel = DGLUTConfig.Config.OneDLutConfig.NumSamplesPerChannel;
    SetPixTxArgs.pBlockConfigs[0].Config.OneDLutConfig.SamplingType         = DGLUTConfig.Config.OneDLutConfig.SamplingType;
    SetPixTxArgs.pBlockConfigs[0].Config.OneDLutConfig.pSampleValues        = (double *)malloc(DGLutSize * sizeof(double));

    EXIT_ON_MEM_ALLOC_FAILURE(SetPixTxArgs.pBlockConfigs[0].Config.OneDLutConfig.pSampleValues, " SetPixTxArgs.pBlockConfigs[0].Config.OneDLutConfig.pSampleValues");

    memset(SetPixTxArgs.pBlockConfigs[0].Config.OneDLutConfig.pSampleValues, 0, DGLutSize * sizeof(double));

    for (uint32_t i = 0; i < (DGLutSize / DGLUTConfig.Config.OneDLutConfig.NumChannels); i++)
    {
        double Input                                                        = (double)i / (double)(DGLutSize - 1);
        SetPixTxArgs.pBlockConfigs[0].Config.OneDLutConfig.pSampleValues[i] = GetSRGBDecodingValue(Input);
    }

    // CSC Values
    double PostOffsets[3] = { 0, 0, 0 };
    double PreOffsets[3]  = { 0, 0, 0 };
    double Matrix[3][3]   = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } }; // Identity Matrix
    // double Matrix[3][3]                     = { { 0, 0, 1 }, { 0, 1, 0 }, { 1, 0, 0 } }; // Red Blue swap Matrix
    SetPixTxArgs.pBlockConfigs[1].BlockId   = CSCConfig.BlockId;
    SetPixTxArgs.pBlockConfigs[1].BlockType = CSCConfig.BlockType;

    // Create a valid CSC Matrix.
    for (uint32_t i = 0; i < 3; i++)
    {
        CSCConfig.Config.MatrixConfig.PreOffsets[i] = PreOffsets[i];
    }
    for (uint32_t i = 0; i < 3; i++)
    {
        CSCConfig.Config.MatrixConfig.PostOffsets[i] = PostOffsets[i];
    }

    memcpy_s(SetPixTxArgs.pBlockConfigs[1].Config.MatrixConfig.Matrix, sizeof(SetPixTxArgs.pBlockConfigs[1].Config.MatrixConfig.Matrix), Matrix, sizeof(Matrix));

    // GLUT Values
    // Create a valid 1D LUT.
    const uint32_t GLutSize                                                 = GLUTConfig.Config.OneDLutConfig.NumSamplesPerChannel * GLUTConfig.Config.OneDLutConfig.NumChannels;
    SetPixTxArgs.pBlockConfigs[2].BlockId                                   = GLUTConfig.BlockId;
    SetPixTxArgs.pBlockConfigs[2].BlockType                                 = GLUTConfig.BlockType;
    SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.NumChannels          = GLUTConfig.Config.OneDLutConfig.NumChannels;
    SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.NumSamplesPerChannel = GLUTConfig.Config.OneDLutConfig.NumSamplesPerChannel;
    SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.SamplingType         = GLUTConfig.Config.OneDLutConfig.SamplingType;
    SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.pSampleValues        = (double *)malloc(GLutSize * sizeof(double));

    EXIT_ON_MEM_ALLOC_FAILURE(SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.pSampleValues, " SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.pSampleValues");

    memset(SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.pSampleValues, 0, GLutSize * sizeof(double));

    double *pRedLut   = SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.pSampleValues;
    double *pGreenLut = pRedLut + SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.NumSamplesPerChannel;
    double *pBlueLut  = pGreenLut + SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.NumSamplesPerChannel;

    for (uint32_t i = 0; i < (GLutSize / SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.NumChannels); i++)
    {
        double Input = (double)i / (double)((GLutSize / SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.NumChannels) - 1);
        pRedLut[i] = pGreenLut[i] = pBlueLut[i] = GetSRGBEncodingValue(Input);
    }

    Result = ctlPixelTransformationSetConfig(hDisplayOutput, &SetPixTxArgs);
    LOG_AND_STORE_RESET_RESULT_ON_ERROR(Result, "ctlPixelTransformationSetConfig");

Exit:
    if (NULL != SetPixTxArgs.pBlockConfigs)
    {
        CTL_FREE_MEM(SetPixTxArgs.pBlockConfigs[0].Config.OneDLutConfig.pSampleValues);
        CTL_FREE_MEM(SetPixTxArgs.pBlockConfigs[2].Config.OneDLutConfig.pSampleValues);
    }
    CTL_FREE_MEM(SetPixTxArgs.pBlockConfigs);
    return Result;
}


/***************************************************************
 * @brief
 * GetPixTxCapability
 * @param hDisplayOutput ,pPixTxCaps
 * @return ctl_result_t
 ***************************************************************/
ctl_result_t GetPixTxCapability(ctl_display_output_handle_t hDisplayOutput, ctl_pixtx_pipe_get_config_t *pPixTxCaps)
{
    ctl_result_t Result = CTL_RESULT_SUCCESS;

    Result = ctlPixelTransformationGetConfig(hDisplayOutput, pPixTxCaps);
    LOG_AND_EXIT_ON_ERROR(Result, "ctlPixelTransformationGetConfig for query type capability");

    // Number of blocks
    APP_LOG_INFO("GetPixTxCapsArgs.NumBlocks = %d", pPixTxCaps->NumBlocks);

    if (NULL == pPixTxCaps->pBlockConfigs)
    {
        goto Exit;
    }

    for (uint8_t i = 0; i < pPixTxCaps->NumBlocks; i++)
    {
        ctl_pixtx_1dlut_config_t *pOneDLutConfig = &pPixTxCaps->pBlockConfigs[i].Config.OneDLutConfig;

        // Block specific information
        APP_LOG_INFO("pPixTxCaps->pBlockConfigs[%d].BlockId = %d", i, pPixTxCaps->pBlockConfigs[i].BlockId);
        if (CTL_PIXTX_BLOCK_TYPE_1D_LUT == pPixTxCaps->pBlockConfigs[i].BlockType)
        {
            APP_LOG_INFO("Block type is CTL_PIXTX_BLOCK_TYPE_1D_LUT");
            APP_LOG_INFO("pPixTxCaps->pBlockConfigs[%d].Config.OneDLutConfig.NumChannels = %d", i, pOneDLutConfig->NumChannels);
            APP_LOG_INFO("pPixTxCaps->pBlockConfigs[%d].Config.OneDLutConfig.NumSamplesPerChannel = %d", i, pOneDLutConfig->NumSamplesPerChannel);
            APP_LOG_INFO("pPixTxCaps->pBlockConfigs[%d].Config.OneDLutConfig.SamplingType = %d", i, pOneDLutConfig->SamplingType);
        }
        else if (CTL_PIXTX_BLOCK_TYPE_3X3_MATRIX == pPixTxCaps->pBlockConfigs[i].BlockType)
        {
            APP_LOG_INFO("Block type is CTL_PIXTX_BLOCK_TYPE_3X3_MATRIX");
        }
        else if (CTL_PIXTX_BLOCK_TYPE_3D_LUT == pPixTxCaps->pBlockConfigs[i].BlockType)
        {
            APP_LOG_INFO("Block type is CTL_PIXTX_BLOCK_TYPE_3D_LUT");
            APP_LOG_INFO("pPixTxCaps->pBlockConfigs[%d].Config.ThreeDLutConfig.NumSamplesPerChannel = %d", i, pPixTxCaps->pBlockConfigs[i].Config.ThreeDLutConfig.NumSamplesPerChannel);
        }
        else if (CTL_PIXTX_BLOCK_TYPE_3X3_MATRIX_AND_OFFSETS == pPixTxCaps->pBlockConfigs[i].BlockType)
        {
            APP_LOG_INFO("Block type is CTL_PIXTX_BLOCK_TYPE_3X3_MATRIX_AND_OFFSETS");
        }
    }

Exit:
    return Result;
}


    ctl_result_t GetDeviceProperties(ctl_device_adapter_handle_t hDevice, ctl_device_adapter_properties_t* StDeviceAdapterProperties)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        StDeviceAdapterProperties->Size = sizeof(ctl_device_adapter_properties_t);
        StDeviceAdapterProperties->pDeviceID = malloc(sizeof(LUID));
        StDeviceAdapterProperties->device_id_size = sizeof(LUID);
        StDeviceAdapterProperties->Version = 2;

        Result = ctlGetDeviceProperties(hDevice, StDeviceAdapterProperties);

        if (CTL_RESULT_ERROR_UNSUPPORTED_VERSION == Result) // reduce version if required & recheck
        {
            printf("ctlGetDeviceProperties() version mismatch - Reducing version to 0 and retrying\n");
            StDeviceAdapterProperties->Version = 0;
            Result = ctlGetDeviceProperties(hDevice, StDeviceAdapterProperties);
        }

        cout << "======== GetDeviceProperties ========" << endl;
        cout << "StDeviceAdapterProperties.Name: " << StDeviceAdapterProperties->name << endl;

        LOG_AND_EXIT_ON_ERROR(Result, "ctlGetDeviceProperties");

    Exit:
        return Result;
    }

    ctl_result_t GetTelemetryData(ctl_device_adapter_handle_t hDevice, ctl_telemetry_data* TelemetryData)
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;
        ctl_power_telemetry_t pPowerTelemetry = {};
        pPowerTelemetry.Size = sizeof(ctl_power_telemetry_t);

        Result = ctlPowerTelemetryGet(hDevice, &pPowerTelemetry);
        if (Result != CTL_RESULT_SUCCESS)
            goto Exit;

        prevtimestamp = curtimestamp;
        curtimestamp = pPowerTelemetry.timeStamp.value.datadouble;
        deltatimestamp = curtimestamp - prevtimestamp;

        if (pPowerTelemetry.gpuEnergyCounter.bSupported)
        {
            TelemetryData->gpuEnergySupported = true;
            prevgpuEnergyCounter = curgpuEnergyCounter;
            curgpuEnergyCounter = pPowerTelemetry.gpuEnergyCounter.value.datadouble;

            TelemetryData->gpuEnergyValue = (curgpuEnergyCounter - prevgpuEnergyCounter) / deltatimestamp;
        }

        TelemetryData->gpuVoltageSupported = pPowerTelemetry.gpuVoltage.bSupported;
        TelemetryData->gpuVoltagValue = pPowerTelemetry.gpuVoltage.value.datadouble;

        TelemetryData->gpuCurrentClockFrequencySupported = pPowerTelemetry.gpuCurrentClockFrequency.bSupported;
        TelemetryData->gpuCurrentClockFrequencyValue = pPowerTelemetry.gpuCurrentClockFrequency.value.datadouble;

        TelemetryData->gpuCurrentTemperatureSupported = pPowerTelemetry.gpuCurrentTemperature.bSupported;
        TelemetryData->gpuCurrentTemperatureValue = pPowerTelemetry.gpuCurrentTemperature.value.datadouble;

        if (pPowerTelemetry.globalActivityCounter.bSupported)
        {
            TelemetryData->globalActivitySupported = true;
            prevglobalActivityCounter = curglobalActivityCounter;
            curglobalActivityCounter = pPowerTelemetry.globalActivityCounter.value.datadouble;

            TelemetryData->globalActivityValue = 100 * (curglobalActivityCounter - prevglobalActivityCounter) / deltatimestamp;
        }

        if (pPowerTelemetry.renderComputeActivityCounter.bSupported)
        {
            TelemetryData->renderComputeActivitySupported = true;
            prevrenderComputeActivityCounter = currenderComputeActivityCounter;
            currenderComputeActivityCounter = pPowerTelemetry.renderComputeActivityCounter.value.datadouble;

            TelemetryData->renderComputeActivityValue = 100 * (currenderComputeActivityCounter - prevrenderComputeActivityCounter) / deltatimestamp;
        }

        if (pPowerTelemetry.mediaActivityCounter.bSupported)
        {
            TelemetryData->mediaActivitySupported = true;
            prevmediaActivityCounter = curmediaActivityCounter;
            curmediaActivityCounter = pPowerTelemetry.mediaActivityCounter.value.datadouble;

            TelemetryData->mediaActivityValue = 100 * (curmediaActivityCounter - prevmediaActivityCounter) / deltatimestamp;
        }

        if (pPowerTelemetry.vramEnergyCounter.bSupported)
        {
            TelemetryData->vramEnergySupported = true;
            prevvramEnergyCounter = curvramEnergyCounter;
            curvramEnergyCounter = pPowerTelemetry.vramEnergyCounter.value.datadouble;

            TelemetryData->vramEnergyValue = (curvramEnergyCounter - prevvramEnergyCounter) / deltatimestamp;
        }

        TelemetryData->vramVoltageSupported = pPowerTelemetry.vramVoltage.bSupported;
        TelemetryData->vramVoltageValue = pPowerTelemetry.vramVoltage.value.datadouble;

        TelemetryData->vramCurrentClockFrequencySupported = pPowerTelemetry.vramCurrentClockFrequency.bSupported;
        TelemetryData->vramCurrentClockFrequencyValue = pPowerTelemetry.vramCurrentClockFrequency.value.datadouble;

        if (pPowerTelemetry.vramReadBandwidthCounter.bSupported)
        {
            TelemetryData->vramReadBandwidthSupported = true;
            prevvramReadBandwidthCounter = curvramReadBandwidthCounter;
            curvramReadBandwidthCounter = pPowerTelemetry.vramReadBandwidthCounter.value.datadouble;

            TelemetryData->vramReadBandwidthValue = (curvramReadBandwidthCounter - prevvramReadBandwidthCounter) / deltatimestamp;
        }

        if (pPowerTelemetry.vramWriteBandwidthCounter.bSupported)
        {
            TelemetryData->vramWriteBandwidthSupported = true;
            prevvramWriteBandwidthCounter = curvramWriteBandwidthCounter;
            curvramWriteBandwidthCounter = pPowerTelemetry.vramWriteBandwidthCounter.value.datadouble;

            TelemetryData->vramWriteBandwidthValue = (curvramWriteBandwidthCounter - prevvramWriteBandwidthCounter) / deltatimestamp;
        }

        TelemetryData->vramCurrentTemperatureSupported = pPowerTelemetry.vramCurrentTemperature.bSupported;
        TelemetryData->vramCurrentTemperatureValue = pPowerTelemetry.vramCurrentTemperature.value.datadouble;

        TelemetryData->fanSpeedSupported = pPowerTelemetry.fanSpeed[0].bSupported;
        TelemetryData->fanSpeedValue = pPowerTelemetry.fanSpeed[0].value.datadouble;

    Exit:
    return Result;
    }

    ctl_result_t IntializeIgcl()
    {
        ctl_result_t Result = CTL_RESULT_SUCCESS;

        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

        ctl_init_args_t ctlInitArgs;
        ctlInitArgs.AppVersion = CTL_MAKE_VERSION(CTL_IMPL_MAJOR_VERSION, CTL_IMPL_MINOR_VERSION);
        ctlInitArgs.flags = CTL_INIT_FLAG_USE_LEVEL_ZERO;
        ctlInitArgs.Size = sizeof(ctlInitArgs);
        ctlInitArgs.Version = 0;
        ZeroMemory(&ctlInitArgs.ApplicationUID, sizeof(ctl_application_id_t));
        Result = ctlInit(&ctlInitArgs, &hAPIHandle);

        return Result;
    }

    void CloseIgcl()
    {
        ctlClose(hAPIHandle);

        if (hDevices != nullptr)
        {
            free(hDevices);
            hDevices = nullptr;
        }
    }

/***************************************************************
 * @brief
 * SetCsc
 * Iterate through blocks PixTxCaps and find the CSC block.
 * @param hDisplayOutput, pPixTxCaps, CscBlockIndex
 * @return ctl_result_t
 ***************************************************************/
ctl_result_t SetCsc(ctl_display_output_handle_t hDisplayOutput, ctl_pixtx_pipe_get_config_t *pPixTxCaps, int32_t CscBlockIndex)
{
    ctl_result_t Result                = CTL_RESULT_SUCCESS;
    ctl_pixtx_block_config_t CSCConfig = pPixTxCaps->pBlockConfigs[CscBlockIndex];
    CSCConfig.Size                     = sizeof(ctl_pixtx_block_config_t);
    CSCConfig.BlockType                = CTL_PIXTX_BLOCK_TYPE_3X3_MATRIX;
    double PostOffsets[3]              = { 1, 0, 0 };
    double PreOffsets[3]               = { 1, 0, 0 };
    double Matrix[3][3]                = { { 1.2, 0, 0 }, { 0, 1.2, 0 }, { 0, 0, 1.2 } }; // Contrast enhancement by 20%

    // Create a valid CSC Matrix.
    for (uint8_t i = 0; i < 3; i++)
    {
        CSCConfig.Config.MatrixConfig.PreOffsets[i] = PreOffsets[i];
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        CSCConfig.Config.MatrixConfig.PostOffsets[i] = PostOffsets[i];
    }

    memcpy_s(CSCConfig.Config.MatrixConfig.Matrix, sizeof(CSCConfig.Config.MatrixConfig.Matrix), Matrix, sizeof(Matrix));

    ctl_pixtx_pipe_set_config_t SetPixTxArgs = { 0 };
    SetPixTxArgs.Size                        = sizeof(ctl_pixtx_pipe_set_config_t);
    SetPixTxArgs.OpertaionType               = CTL_PIXTX_CONFIG_OPERTAION_TYPE_SET_CUSTOM;
    SetPixTxArgs.NumBlocks                   = 1;          // We are trying to set only one block
    SetPixTxArgs.pBlockConfigs               = &CSCConfig; // for CSC block
    SetPixTxArgs.pBlockConfigs->BlockId      = pPixTxCaps->pBlockConfigs[CscBlockIndex].BlockId;

    Result = ctlPixelTransformationSetConfig(hDisplayOutput, &SetPixTxArgs);
    LOG_AND_STORE_RESET_RESULT_ON_ERROR(Result, "ctlPixelTransformationSetConfig");

    return Result;
}

/***************************************************************
 * @brief
 * CIELabTxFn
 * @param Input
 * @return double
 ***************************************************************/
double CIELabTxFn(double Input)
{
    double RetVal = 0;

    /*
    https://en.wikipedia.org/wiki/CIELAB_color_space#From_CIEXYZ_to_CIELAB
    */

    if (Input > pow(6.0 / 29.0, 3.0))
    {
        RetVal = pow(Input, (1.0 / 3.0));
    }
    else
    {
        RetVal = Input * (pow(29.0 / 6.0, 2.0) / 3.0) + (4.0 / 29.0);
    }

    return RetVal;
}

/***************************************************************
 * @brief
 * GetCIELab
 * @param Color, WhitePoint, ColorLab
 * @return void
 ***************************************************************/
void GetCIELab(IPIXEL_XYZ Color, IPIXEL_XYZ WhitePoint, IPIXEL_Lab& ColorLab)
{
    /*
    https://en.wikipedia.org/wiki/CIELAB_color_space#From_CIEXYZ_to_CIELAB
    */

    Color.X /= WhitePoint.X;
    Color.Y /= WhitePoint.Y;
    Color.Z /= WhitePoint.Z;

    ColorLab.L = 116.0 * (CIELabTxFn(Color.Y)) - 16;
    ColorLab.A = 500.0 * (CIELabTxFn(Color.X) - CIELabTxFn(Color.Y));
    ColorLab.B = 200.0 * (CIELabTxFn(Color.Y) - CIELabTxFn(Color.Z));
}

/***************************************************************
 * @brief
 * CalculateColorAngle
 * @param R,G,B
 * @return void
 ***************************************************************/
double CalculateColorAngle(double R, double G, double B)
{
    double RGB[3] = { 0 };
    IPIXEL_XYZ XYZ;
    IPIXEL_Lab Lab;

    RGB[0] = GetSRGBDecodingValue(R);
    RGB[1] = GetSRGBDecodingValue(G);
    RGB[2] = GetSRGBDecodingValue(B);

    MatrixMult3x3With3x1(RGB2XYZ_709, RGB, (double*)&XYZ);

    XYZ.X *= 100.0;
    XYZ.Y *= 100.0;
    XYZ.Z *= 100.0;

    GetCIELab(XYZ, RefWhitePointSRGB, Lab);

    double Angle = atan2(Lab.B, Lab.A); // https://en.wikipedia.org/wiki/Hue

    // convert [-Pi, Pi] range to [0, 2 * Pi] range
    if (Angle < 0)
    {
        Angle = 2.0 * Pi - (ABS_DOUBLE(Angle));
    }

    Angle /= (2.0 * Pi);

    return Angle;
}

/***************************************************************
 * @brief
 * InterpolateSaturationFactor
 * @param ColorAngle, pBasicColors, pColorSaturation
 * @return double
 ***************************************************************/
double InterpolateSaturationFactor(double ColorAngle, double* pBasicColors, double* pColorSaturation)
{
    double SatFactor = SATURATION_FACTOR_BASE;
    double Slope, SatFactor1, SatFactor2, Diff;

    // ColorIndex1 represents index of anchor color previous to colorAngle
    // ColorIndex2 represents index of anchor color next to colorAngle
    HUE_ANCHOR ColorIndex1, ColorIndex2;
    ColorIndex1 = (HUE_ANCHOR)((uint8_t)HUE_ANCHOR_COLOR_COUNT_SATURATION - 1);

    for (uint8_t i = 0; i < (uint8_t)HUE_ANCHOR_COLOR_COUNT_SATURATION; i++)
    {
        if (pBasicColors[i] <= ColorAngle)
        {
            ColorIndex1 = (HUE_ANCHOR)i;
        }
    }

    ColorIndex2 = (HUE_ANCHOR)((ColorIndex1 + 1) % HUE_ANCHOR_COLOR_COUNT_SATURATION);

    SatFactor1 = pColorSaturation[(uint8_t)ColorIndex1];
    SatFactor2 = pColorSaturation[(uint8_t)ColorIndex2];

    if ((uint8_t)ColorIndex1 <= 4 || ColorAngle >= pBasicColors[(uint8_t)ColorIndex1])
    {
        Diff = ColorAngle - pBasicColors[ColorIndex1];
        Slope = (SatFactor2 - SatFactor1) / (pBasicColors[ColorIndex2] - pBasicColors[ColorIndex1]);
    }
    else
    {
        Diff = ColorAngle - pBasicColors[ColorIndex1] + 1.0;
        Slope = (SatFactor2 - SatFactor1) / (pBasicColors[ColorIndex2] - pBasicColors[ColorIndex1] + 1.0);
    }

    SatFactor = SatFactor1 + Slope * Diff;

    return SatFactor;
}

/***************************************************************
 * @brief
 * PerformRGB2YCbCr: Conversion of RGB to YCbCr
 * @param InPixel, OutPixel
 * @return void
 ***************************************************************/
void PerformRGB2YCbCr(ctl_pixtx_3dlut_sample_t InPixel, IPIXELD_YCbCr& OutPixel)
{
    // Numbers used are BT 701 coefficients used for RGB to YCbCr conversion.
    // https://en.wikipedia.org/wiki/YUV#Conversion_to/from_RGB

    OutPixel.Y = RGB2YCbCr709[0][0] * InPixel.Red + RGB2YCbCr709[0][1] * InPixel.Green + RGB2YCbCr709[0][2] * InPixel.Blue;
    OutPixel.Cb = RGB2YCbCr709[1][0] * InPixel.Red + RGB2YCbCr709[1][1] * InPixel.Green + RGB2YCbCr709[1][2] * InPixel.Blue;
    OutPixel.Cr = RGB2YCbCr709[2][0] * InPixel.Red + RGB2YCbCr709[2][1] * InPixel.Green + RGB2YCbCr709[2][2] * InPixel.Blue;
}


/***************************************************************
 * @brief
 * PerformYCbCr2RGB: Conversion of YCbCr to RGB
 * @param InPixel, OutPixel
 * @return void
 ***************************************************************/
void PerformYCbCr2RGB(IPIXELD_YCbCr InPixel, ctl_pixtx_3dlut_sample_t& OutPixel)
{
    // BT 701 coefficients
    // Numbers used are BT 701 coefficients used for YCbCr to RGB conversion.
    // https://en.wikipedia.org/wiki/YUV#Conversion_to/from_RGB

    OutPixel.Red = InPixel.Y + YCbCr2RGB709[0][2] * InPixel.Cr;
    OutPixel.Green = InPixel.Y + YCbCr2RGB709[1][1] * InPixel.Cb + YCbCr2RGB709[1][2] * InPixel.Cr;
    OutPixel.Blue = InPixel.Y + YCbCr2RGB709[2][1] * InPixel.Cb;

    OutPixel.Red = CLIP_DOUBLE(OutPixel.Red, 0.0, 1.0);
    OutPixel.Green = CLIP_DOUBLE(OutPixel.Green, 0.0, 1.0);
    OutPixel.Blue = CLIP_DOUBLE(OutPixel.Blue, 0.0, 1.0);
}

/***************************************************************
 * @brief
 * ChangePixelSaturation
 * @param PixelRGB, pBasicColors, pSatWeights
 * @return void
 ***************************************************************/
void ChangePixelSaturation(ctl_pixtx_3dlut_sample_t& PixelRGB, double* pBasicColors, double* pSatWeights)
{
    double SatFactor = SATURATION_FACTOR_BASE;
    double ColorAngle = 0;
    IPIXELD_YCbCr PixelYCbCr;

    if ((PixelRGB.Red == PixelRGB.Green) && (PixelRGB.Green == PixelRGB.Blue))
    {
        return; // Do not process Grey pixels
    }

    PerformRGB2YCbCr(PixelRGB, PixelYCbCr);

    ColorAngle = CalculateColorAngle(PixelRGB.Red, PixelRGB.Green, PixelRGB.Blue);

    //--- Calculate Sat Factor (Step 1) ---
    SatFactor = InterpolateSaturationFactor(ColorAngle, pBasicColors, pSatWeights);

    //--- Over Saturation Limiter (Step 2) ---
    double UVmax = max((PixelYCbCr.Cb), ABS_DOUBLE(PixelYCbCr.Cr));
    UVmax *= 2.0;

    if ((UVmax >= UV_MAX_POINT) && (SatFactor > SATURATION_FACTOR_BASE))
    {
        SatFactor = SATURATION_FACTOR_BASE;
    }
    else if (SatFactor > SATURATION_FACTOR_BASE)
    {
        // Limit SatFactor according to original saturation
        double a = (SatFactor - SATURATION_FACTOR_BASE) * (UV_MAX_POINT - UVmax) / UV_MAX_POINT;
        SatFactor = SATURATION_FACTOR_BASE + a;
    }

    //--- Grey Pixels Saturation Limiter (Step 3) ---
    if (SatFactor > SATURATION_FACTOR_BASE)
    {
        double dSat = SatFactor - SATURATION_FACTOR_BASE;

        if (UV_MIN_POINT1 >= UVmax)
        {
            dSat = 0;
        }
        else if (UV_MIN_POINT1 < UVmax && UVmax <= UV_MIN_POINT2)
        {
            dSat *= (UVmax - UV_MIN_POINT1);
        }

        SatFactor = SATURATION_FACTOR_BASE + dSat;
    }

    //--- Calculate New U,V values ---
    PixelYCbCr.Cb = SatFactor * PixelYCbCr.Cb;
    PixelYCbCr.Cr = SatFactor * PixelYCbCr.Cr;

    PerformYCbCr2RGB(PixelYCbCr, PixelRGB);

}

/***************************************************************
 * @brief
 * CreateDefault3DLut
 * @param pLUT, LutDepth, pSamplingPosition
 * @return void
 ***************************************************************/
void CreateDefault3DLut(ctl_pixtx_3dlut_sample_t* pLUT, uint8_t LutDepth, double* pSamplingPosition)
{
    for (uint8_t R = 0; R < LutDepth; R++)
    {
        for (uint8_t G = 0; G < LutDepth; G++)
        {
            for (uint8_t B = 0; B < LutDepth; B++)
            {
                pLUT->Red = pSamplingPosition[R];
                pLUT->Green = pSamplingPosition[G];
                pLUT->Blue = pSamplingPosition[B];
                pLUT++;
            }
        }
    }
}


/***************************************************************
 * @brief
 * InitializePartialSaturationAnchorValues
 * @param pBasicColors
 * @return void
 ***************************************************************/
void InitializePartialSaturationAnchorValues(double* pBasicColors)
{
    pBasicColors[HUE_ANCHOR_RED_SATURATION] = 0.11111097848067050; // Output of CalculateColorAngle() with pure red input (1.0, 0, 0);
    pBasicColors[HUE_ANCHOR_YELLOW_SATURATION] = 0.28571682845597457; // Output of CalculateColorAngle() with pure yellow input (1.0, 1.0, 0);
    pBasicColors[HUE_ANCHOR_GREEN_SATURATION] = 0.37782403959790056; // Output of CalculateColorAngle() with pure green input (0, 1.0, 0);
    pBasicColors[HUE_ANCHOR_CYAN_SATURATION] = 0.54552642779786065; // Output of CalculateColorAngle() with pure cyan input (0, 1.0, 1.0);
    pBasicColors[HUE_ANCHOR_BLUE_SATURATION] = 0.85079006538296154; // Output of CalculateColorAngle() with pure blue input (0, 0, 1.0);
    pBasicColors[HUE_ANCHOR_MAGENTA_SATURATION] = 0.91174274251260268; // Output of CalculateColorAngle() with pure magenta input (1.0, 0, 1.0);
}


/***************************************************************
 * @brief
 * IsDefaultPartialSaturationSettings
 * @param pSatWeights
 * @return bool
 ***************************************************************/
bool IsDefaultPartialSaturationSettings(double* pSatWeights)
{
    for (uint8_t i = 0; i < 6; i++)
    {
        if (DEFAULT_COLOR_SAT != pSatWeights[i])
        {
            return false;
        }
    }

    return true;
}


/***************************************************************
 * @brief
 * Generate3DLutFromPSWeights
 * @param pLUT, LutDepth, pSatWeights
 * @return ctl_result_t
 ***************************************************************/
ctl_result_t Generate3DLutFromPSWeights(ctl_pixtx_3dlut_sample_t *pLUT, uint8_t LutDepth, double *pSatWeights)
{
    ctl_result_t Result = CTL_RESULT_SUCCESS;

    ctl_pixtx_3dlut_sample_t Pix;
    double BasicColors[HUE_ANCHOR_COLOR_COUNT_SATURATION];

    bool IsDefaultConfig = IsDefaultPartialSaturationSettings(pSatWeights);

    InitializePartialSaturationAnchorValues(BasicColors);

    double *pSamplingPosition = (double *)malloc(LutDepth * sizeof(double));
    EXIT_ON_MEM_ALLOC_FAILURE(pSamplingPosition, "pSamplingPosition");

    for (uint8_t i = 0; i < LutDepth; i++)
    {
        pSamplingPosition[i] = (double)i / (double)(LutDepth - 1);
    }

    if (IsDefaultConfig)
    {
        CreateDefault3DLut(pLUT, LutDepth, pSamplingPosition);
        goto Exit;
    }

    for (uint8_t R = 0; R < LutDepth; R++)
    {
        for (uint8_t G = 0; G < LutDepth; G++)
        {
            for (uint8_t B = 0; B < LutDepth; B++)
            {
                Pix.Red   = pSamplingPosition[R];
                Pix.Green = pSamplingPosition[G];
                Pix.Blue  = pSamplingPosition[B];

                ChangePixelSaturation(Pix, BasicColors, pSatWeights);

                *pLUT++ = Pix;
            }
        }
    }

Exit:
    CTL_FREE_MEM(pSamplingPosition);
    return Result;
}

double MapSaturation(int sliderValue) {
    return 0.01 + sliderValue * 0.02;
}

double MapHue(double sliderValue) {
    double driverHue = fmod(sliderValue + 360.0, 360.0);
    if (driverHue < 0.0)  // handle negative wrap (just in case)
        driverHue += 360.0;
    return driverHue;
}

ctl_result_t SetHueSaturationValues(ctl_device_adapter_handle_t hDevice, double Hue, double Saturation)
{
    ctl_result_t Result = CTL_RESULT_SUCCESS;
    ctl_display_output_handle_t *hDisplayOutput = NULL;
    uint32_t DisplayCount                       = 0;


    // enumerate all the possible target display's for the adapters
    Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, NULL);

    if (CTL_RESULT_SUCCESS != Result)
    {
        APP_LOG_WARN("ctlEnumerateDisplayOutputs returned failure code: 0x%X", Result);
        STORE_AND_RESET_ERROR(Result);
    }
    else if (DisplayCount <= 0)
    {
        APP_LOG_WARN("Invalid Display Count. skipping display enumration for adapter:%d", 0);
    }

    hDisplayOutput = (ctl_display_output_handle_t *)malloc(sizeof(ctl_display_output_handle_t) * DisplayCount);
    EXIT_ON_MEM_ALLOC_FAILURE(hDisplayOutput, "hDisplayOutput");

    Result = ctlEnumerateDisplayOutputs(hDevice, &DisplayCount, hDisplayOutput);
    LOG_AND_STORE_RESET_RESULT_ON_ERROR(Result, "ctlEnumerateDisplayOutputs");

    // Todo: use Display Index in future, currently only Apply Hue Saturation on Display 0;
    ctl_display_output_handle_t display = hDisplayOutput[0];

    // 1st query about the number of blocks supported (pass PixTxCaps.pBlockConfigs as NULL to get number of blocks supported) and then allocate memeory accordingly in second call to get details of
    // each pBlockConfigs
    ctl_pixtx_pipe_get_config_t PixTxCaps = { 0 };
    PixTxCaps.Size = sizeof(ctl_pixtx_pipe_get_config_t);
    PixTxCaps.QueryType = CTL_PIXTX_CONFIG_QUERY_TYPE_CAPABILITY;

    Result = GetPixTxCapability(display, &PixTxCaps); // API call will return the number of blocks supported in PixTxCaps.NumBlocks.

    if (0 == PixTxCaps.NumBlocks)
    {
        Result = CTL_RESULT_ERROR_INVALID_SIZE;
        LOG_AND_EXIT_ON_ERROR(Result, "ctlPixelTransformationGetConfig for query type capability");
    }

    const uint8_t NumBlocksToQuery = PixTxCaps.NumBlocks; // Query about the blocks in the pipeline

    // Allocate required memory as per number of blocks supported.
    PixTxCaps.pBlockConfigs = (ctl_pixtx_block_config_t*)malloc(NumBlocksToQuery * sizeof(ctl_pixtx_block_config_t));
    EXIT_ON_MEM_ALLOC_FAILURE(PixTxCaps.pBlockConfigs, "PixTxCaps.pBlockConfigs");

    memset(PixTxCaps.pBlockConfigs, 0, NumBlocksToQuery * sizeof(ctl_pixtx_block_config_t));

    // Query capability of each block, number of blocks etc
    Result = GetPixTxCapability(display, &PixTxCaps);

    // Apply Hue Saturation
    int32_t HueSatBlockIndex = -1;

    for (uint8_t i = 0; i < PixTxCaps.NumBlocks; i++)
    {
        if (CTL_PIXTX_BLOCK_TYPE_3X3_MATRIX == PixTxCaps.pBlockConfigs[i].BlockType)
        {
            HueSatBlockIndex = i;
            break;
        }
    }

    // issue the call
    Result = ApplyHueSaturation(display, &PixTxCaps, HueSatBlockIndex, MapHue(Hue), MapSaturation(Saturation));
    LOG_AND_EXIT_ON_ERROR(Result, "ctl_result_t (ApplyHueSaturation)");

    // log for debug
    cout << "======== SetHueSaturationValues ========" << endl;
    cout << "Hue: " << MapHue(Hue) << endl;
    cout << "Saturation:    " << MapSaturation(Saturation) << endl;

Exit:
    return Result;
}

}