#pragma once

#include <stdint.h>
#include <esp_gatt_defs.h>

#pragma region Constant attribute types
static const uint16_t GATT_INVALID_UUID = ESP_GATT_ILLEGAL_UUID;
static const uint16_t GATT_IMMEDIATE_ALERT_UUID = ESP_GATT_UUID_IMMEDIATE_ALERT_SVC;
static const uint16_t GATT_LINK_LOSS_UUID = ESP_GATT_UUID_LINK_LOSS_SVC;
static const uint16_t GATT_TX_POWER_UUID = ESP_GATT_UUID_TX_POWER_SVC;
static const uint16_t GATT_CURRENT_TIME_UUID = ESP_GATT_UUID_CURRENT_TIME_SVC;
static const uint16_t GATT_REFERENCE_TIME_UPDATE_UUID = ESP_GATT_UUID_REF_TIME_UPDATE_SVC;
static const uint16_t GATT_NEXT_DESTINATION_CHANGE_UUID = ESP_GATT_UUID_NEXT_DST_CHANGE_SVC;
static const uint16_t GATT_GLUCOSE_UUID = ESP_GATT_UUID_GLUCOSE_SVC;
static const uint16_t GATT_HEALTH_THERMOMETER_UUID = ESP_GATT_UUID_HEALTH_THERMOM_SVC;
static const uint16_t GATT_DEVICE_INFORMATION_UUID = ESP_GATT_UUID_DEVICE_INFO_SVC;
static const uint16_t GATT_HEART_RATE_UUID = ESP_GATT_UUID_HEART_RATE_SVC;
static const uint16_t GATT_PHONE_ALERT_STATUS_UUID = ESP_GATT_UUID_PHONE_ALERT_STATUS_SVC;
static const uint16_t GATT_BATTERY_UUID = ESP_GATT_UUID_BATTERY_SERVICE_SVC;
static const uint16_t GATT_BLOOD_PRESSURE_UUID = ESP_GATT_UUID_BLOOD_PRESSURE_SVC;
static const uint16_t GATT_ALERT_NOTIFICATION_UUID = ESP_GATT_UUID_ALERT_NTF_SVC;
static const uint16_t GATT_HID_UUID = ESP_GATT_UUID_HID_SVC;
static const uint16_t GATT_SCAN_PARAMETERS_UUID = ESP_GATT_UUID_SCAN_PARAMETERS_SVC;
static const uint16_t GATT_RUNNING_SPEED_AND_CADANCE_UUID = ESP_GATT_UUID_RUNNING_SPEED_CADENCE_SVC;
static const uint16_t GATT_AUTOMATION_IO_UUID = ESP_GATT_UUID_Automation_IO_SVC;
static const uint16_t GATT_CYCLING_SPEED_AND_CADANCE_UUID = ESP_GATT_UUID_CYCLING_SPEED_CADENCE_SVC;
static const uint16_t GATT_CYCLING_POWER_UUID = ESP_GATT_UUID_CYCLING_POWER_SVC;
static const uint16_t GATT_LOCATION_AND_NAVIGATION_UUID = ESP_GATT_UUID_LOCATION_AND_NAVIGATION_SVC;
static const uint16_t GATT_ENVIRONMENTAL_SENSING_UUID = ESP_GATT_UUID_ENVIRONMENTAL_SENSING_SVC;
static const uint16_t GATT_BODY_COMPOSITION_UUID = ESP_GATT_UUID_BODY_COMPOSITION;
static const uint16_t GATT_USER_DATA_UUID = ESP_GATT_UUID_USER_DATA_SVC;
static const uint16_t GATT_WEIGHT_SCALE_UUID = ESP_GATT_UUID_WEIGHT_SCALE_SVC;
static const uint16_t GATT_BOND_MANAGEMENT_UUID = ESP_GATT_UUID_BOND_MANAGEMENT_SVC;
static const uint16_t GATT_CONTINUOUS_GLUCOSE_MONITORING_UUID = ESP_GATT_UUID_CONT_GLUCOSE_MONITOR_SVC;
static const uint16_t GATT_PRIMARY_SERVICE_UUID = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t GATT_SECONDARY_SERVICE_UUID = ESP_GATT_UUID_SEC_SERVICE;
static const uint16_t GATT_INCLUDE_SERVICE_UUID = ESP_GATT_UUID_INCLUDE_SERVICE;
static const uint16_t GATT_CHARACTERISTIC_DECLARATION_UUID = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t GATT_CHARACTERISTIC_EXTENDED_PROPERTIES_UUID = ESP_GATT_UUID_CHAR_EXT_PROP;
static const uint16_t GATT_CHARACTERISTIC_USER_DESCRIPTION_UUID = ESP_GATT_UUID_CHAR_DESCRIPTION;
static const uint16_t GATT_CLIENT_CHARACTERISTIC_CONFIGURATION_UUID = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint16_t GATT_SERVER_CHARACTERISTIC_CONFIGURATION_UUID = ESP_GATT_UUID_CHAR_SRVR_CONFIG;
static const uint16_t GATT_CHARACTERISTIC_PRESENTATION_FORMAT_UUID = ESP_GATT_UUID_CHAR_PRESENT_FORMAT;
static const uint16_t GATT_CHARACTERISTIC_AGGREGATE_FORMAT_UUID = ESP_GATT_UUID_CHAR_AGG_FORMAT;
static const uint16_t GATT_CHARACTERISTIC_VALID_RANGE_UUID = ESP_GATT_UUID_CHAR_VALID_RANGE;
static const uint16_t GATT_EXTERNAL_REPORT_REFERENCE_UUID = ESP_GATT_UUID_EXT_RPT_REF_DESCR;
static const uint16_t GATT_REPORT_REFERENCE_DESCRIPTOR_UUID = ESP_GATT_UUID_RPT_REF_DESCR;
static const uint16_t GATT_NUMBER_OF_DIGITALS_DESCRIPTOR_UUID = ESP_GATT_UUID_NUM_DIGITALS_DESCR;
static const uint16_t GATT_VALUE_TRIGGER_SETTING_DESCRIPTOR_UUID = ESP_GATT_UUID_VALUE_TRIGGER_DESCR;
static const uint16_t GATT_ENVIRONMENTAL_SENSING_CONFIGURATION_DESCRIPTION_UUID = ESP_GATT_UUID_ENV_SENSING_CONFIG_DESCR;
static const uint16_t GATT_ENVIRONMENTAL_SENSING_MEASUREMENT_DESCRIPTION_UUID = ESP_GATT_UUID_ENV_SENSING_MEASUREMENT_DESCR;
static const uint16_t GATT_ENVIRONMENTAL_SENSING_TRIGGER_SETTING_DESCRIPTION_UUID = ESP_GATT_UUID_ENV_SENSING_TRIGGER_DESCR;
static const uint16_t GATT_TIME_TRIGGER_SETTING_DESCRIPTOR_UUID = ESP_GATT_UUID_TIME_TRIGGER_DESCR;
#pragma endregion

enum EGattAttributeType
{
    ImmediateAlert = GATT_IMMEDIATE_ALERT_UUID,
    LinkLoss = GATT_LINK_LOSS_UUID,
    TxPower = GATT_TX_POWER_UUID,
    CurrentTime = GATT_CURRENT_TIME_UUID,
    ReferenceTimeUpdate = GATT_REFERENCE_TIME_UPDATE_UUID,
    NextDestinationChange = GATT_NEXT_DESTINATION_CHANGE_UUID,
    Glucose = GATT_GLUCOSE_UUID,
    HealthThermometer = GATT_HEALTH_THERMOMETER_UUID,
    DeviceInformation = GATT_DEVICE_INFORMATION_UUID,
    HeartRate = GATT_HEART_RATE_UUID,
    PhoneAlertStatus = GATT_PHONE_ALERT_STATUS_UUID,
    Battery = GATT_BATTERY_UUID,
    BloodPressure = GATT_BLOOD_PRESSURE_UUID,
    AlertNotification = GATT_ALERT_NOTIFICATION_UUID,
    HID = GATT_HID_UUID,
    ScanParameters = GATT_SCAN_PARAMETERS_UUID,
    RunningSpeedAndCadance = GATT_RUNNING_SPEED_AND_CADANCE_UUID,
    AutomationIO = GATT_AUTOMATION_IO_UUID,
    CyclingSpeedAndCadance = GATT_CYCLING_SPEED_AND_CADANCE_UUID,
    CyclingPower = GATT_CYCLING_POWER_UUID,
    LocationAndNavigation = GATT_LOCATION_AND_NAVIGATION_UUID,
    EnviromentalSensing = GATT_ENVIRONMENTAL_SENSING_UUID,
    BodyComposition = GATT_BODY_COMPOSITION_UUID,
    UserData = GATT_USER_DATA_UUID,
    WeightScale = GATT_WEIGHT_SCALE_UUID,
    BondManagement = GATT_BOND_MANAGEMENT_UUID,
    ContinuousGlucoseMonitoring = GATT_CONTINUOUS_GLUCOSE_MONITORING_UUID,
    PrimaryService = GATT_PRIMARY_SERVICE_UUID,
    SecondaryService = GATT_SECONDARY_SERVICE_UUID,
    IncludeService = GATT_INCLUDE_SERVICE_UUID,
    CharacteristicDecleration = GATT_CHARACTERISTIC_DECLARATION_UUID,
    CharacteristicExtendedProperties = GATT_CHARACTERISTIC_EXTENDED_PROPERTIES_UUID,
    CharacteristicUserDescription = GATT_CHARACTERISTIC_USER_DESCRIPTION_UUID,
    ClientCharacteristicConfiguration = GATT_CLIENT_CHARACTERISTIC_CONFIGURATION_UUID,
    ServerCharacteristicConfiguration = GATT_SERVER_CHARACTERISTIC_CONFIGURATION_UUID,
    CharacteristicPresentationFormat = GATT_CHARACTERISTIC_PRESENTATION_FORMAT_UUID,
    CharacteristicAggregateFormat = GATT_CHARACTERISTIC_AGGREGATE_FORMAT_UUID,
    CharacteristicValidRange = GATT_CHARACTERISTIC_VALID_RANGE_UUID,
    ExternalReportReference = GATT_EXTERNAL_REPORT_REFERENCE_UUID,
    ReportReferenceDescriptor = GATT_REPORT_REFERENCE_DESCRIPTOR_UUID,
    NumberOfDigitalsDescriptor = GATT_NUMBER_OF_DIGITALS_DESCRIPTOR_UUID,
    ValueTriggerSettingDescriptor = GATT_VALUE_TRIGGER_SETTING_DESCRIPTOR_UUID,
    EnviromentalSensingConfigurationDescription = GATT_ENVIRONMENTAL_SENSING_CONFIGURATION_DESCRIPTION_UUID,
    EnviromentalSensingMeasurementDescription = GATT_ENVIRONMENTAL_SENSING_MEASUREMENT_DESCRIPTION_UUID,
    EnviromentalSensingTriggerSettingDescription = GATT_ENVIRONMENTAL_SENSING_TRIGGER_SETTING_DESCRIPTION_UUID,
    TimeTriggerSettingDescriptor = GATT_TIME_TRIGGER_SETTING_DESCRIPTOR_UUID
};

const uint16_t* GetAttributeType(EGattAttributeType attributeType)
{
    switch (attributeType)
    {
        case EGattAttributeType::ImmediateAlert:
            return &GATT_IMMEDIATE_ALERT_UUID;
        case EGattAttributeType::LinkLoss:
            return &GATT_LINK_LOSS_UUID;
        case EGattAttributeType::TxPower:
            return &GATT_TX_POWER_UUID;
        case EGattAttributeType::CurrentTime:
            return &GATT_CURRENT_TIME_UUID;
        case EGattAttributeType::ReferenceTimeUpdate:
            return &GATT_REFERENCE_TIME_UPDATE_UUID;
        case EGattAttributeType::NextDestinationChange:
            return &GATT_NEXT_DESTINATION_CHANGE_UUID;
        case EGattAttributeType::Glucose:
            return &GATT_GLUCOSE_UUID;
        case EGattAttributeType::HealthThermometer:
            return &GATT_HEALTH_THERMOMETER_UUID;
        case EGattAttributeType::DeviceInformation:
            return &GATT_DEVICE_INFORMATION_UUID;
        case EGattAttributeType::HeartRate:
            return &GATT_HEART_RATE_UUID;
        case EGattAttributeType::PhoneAlertStatus:
            return &GATT_PHONE_ALERT_STATUS_UUID;
        case EGattAttributeType::Battery:
            return &GATT_BATTERY_UUID;
        case EGattAttributeType::BloodPressure:
            return &GATT_BLOOD_PRESSURE_UUID;
        case EGattAttributeType::AlertNotification:
            return &GATT_ALERT_NOTIFICATION_UUID;
        case EGattAttributeType::HID:
            return &GATT_HID_UUID;
        case EGattAttributeType::ScanParameters:
            return &GATT_SCAN_PARAMETERS_UUID;
        case EGattAttributeType::RunningSpeedAndCadance:
            return &GATT_RUNNING_SPEED_AND_CADANCE_UUID;
        case EGattAttributeType::AutomationIO:
            return &GATT_AUTOMATION_IO_UUID;
        case EGattAttributeType::CyclingSpeedAndCadance:
            return &GATT_CYCLING_SPEED_AND_CADANCE_UUID;
        case EGattAttributeType::CyclingPower:
            return &GATT_CYCLING_POWER_UUID;
        case EGattAttributeType::LocationAndNavigation:
            return &GATT_LOCATION_AND_NAVIGATION_UUID;
        case EGattAttributeType::EnviromentalSensing:
            return &GATT_ENVIRONMENTAL_SENSING_UUID;
        case EGattAttributeType::BodyComposition:
            return &GATT_BODY_COMPOSITION_UUID;
        case EGattAttributeType::UserData:
            return &GATT_USER_DATA_UUID;
        case EGattAttributeType::WeightScale:
            return &GATT_WEIGHT_SCALE_UUID;
        case EGattAttributeType::BondManagement:
            return &GATT_BOND_MANAGEMENT_UUID;
        case EGattAttributeType::ContinuousGlucoseMonitoring:
            return &GATT_CONTINUOUS_GLUCOSE_MONITORING_UUID;
        case EGattAttributeType::PrimaryService:
            return &GATT_PRIMARY_SERVICE_UUID;
        case EGattAttributeType::SecondaryService:
            return &GATT_SECONDARY_SERVICE_UUID;
        case EGattAttributeType::IncludeService:
            return &GATT_INCLUDE_SERVICE_UUID;
        case EGattAttributeType::CharacteristicDecleration:
            return &GATT_CHARACTERISTIC_DECLARATION_UUID;
        case EGattAttributeType::CharacteristicExtendedProperties:
            return &GATT_CHARACTERISTIC_EXTENDED_PROPERTIES_UUID;
        case EGattAttributeType::CharacteristicUserDescription:
            return &GATT_CHARACTERISTIC_USER_DESCRIPTION_UUID;
        case EGattAttributeType::ClientCharacteristicConfiguration:
            return &GATT_CLIENT_CHARACTERISTIC_CONFIGURATION_UUID;
        case EGattAttributeType::ServerCharacteristicConfiguration:
            return &GATT_SERVER_CHARACTERISTIC_CONFIGURATION_UUID;
        case EGattAttributeType::CharacteristicPresentationFormat:
            return &GATT_CHARACTERISTIC_PRESENTATION_FORMAT_UUID;
        case EGattAttributeType::CharacteristicAggregateFormat:
            return &GATT_CHARACTERISTIC_AGGREGATE_FORMAT_UUID;
        case EGattAttributeType::CharacteristicValidRange:
            return &GATT_CHARACTERISTIC_VALID_RANGE_UUID;
        case EGattAttributeType::ExternalReportReference:
            return &GATT_EXTERNAL_REPORT_REFERENCE_UUID;
        case EGattAttributeType::ReportReferenceDescriptor:
            return &GATT_REPORT_REFERENCE_DESCRIPTOR_UUID;
        case EGattAttributeType::NumberOfDigitalsDescriptor:
            return &GATT_NUMBER_OF_DIGITALS_DESCRIPTOR_UUID;
        case EGattAttributeType::ValueTriggerSettingDescriptor:
            return &GATT_VALUE_TRIGGER_SETTING_DESCRIPTOR_UUID;
        case EGattAttributeType::EnviromentalSensingConfigurationDescription:
            return &GATT_ENVIRONMENTAL_SENSING_CONFIGURATION_DESCRIPTION_UUID;
        case EGattAttributeType::EnviromentalSensingMeasurementDescription:
            return &GATT_ENVIRONMENTAL_SENSING_MEASUREMENT_DESCRIPTION_UUID;
        case EGattAttributeType::EnviromentalSensingTriggerSettingDescription:
            return &GATT_ENVIRONMENTAL_SENSING_TRIGGER_SETTING_DESCRIPTION_UUID;
        case EGattAttributeType::TimeTriggerSettingDescriptor:
            return &GATT_TIME_TRIGGER_SETTING_DESCRIPTOR_UUID;
        default:
            return &GATT_INVALID_UUID;
    }
}
