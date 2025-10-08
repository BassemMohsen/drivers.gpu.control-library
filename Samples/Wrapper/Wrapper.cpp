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
}