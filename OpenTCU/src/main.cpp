#include <Arduino.h>
#include <hal/spi_types.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <SPI.h>
#include "mcp2515.h"
// #include <mcp_can.h>

// Define SPI bus parameters
#define SPI_BUS_HOST    SPI2_HOST
#define SPI_DMA_CHAN    SPI_DMA_CH_AUTO
#define SPI_CLOCK_SPEED SPI_MASTER_FREQ_8M
#define MOSI_PIN        GPIO_NUM_2
#define MISO_PIN        GPIO_NUM_1
#define SCK_PIN         GPIO_NUM_0
#define CS_PIN          GPIO_NUM_3

spi_device_handle_t spi;
MCP2515* mcp2515;
can_frame frame;

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128]; 
SPIClass* spi2;
// MCP_CAN* can0;

void setup()
{
    Serial.begin(115200);
    //Use printf/puts for logging.
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    puts("setup");

    esp_err_t ret;

    // Configure the bus settings
    spi_bus_config_t bus_config = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE, // 0 means default, otherwise set to your desired size
    };

    // Initialize the SPI bus
    ret = spi_bus_initialize(SPI_BUS_HOST, &bus_config, SPI_DMA_CHAN);
    assert(ret == ESP_OK);

    // Configure the device settings
    spi_device_interface_config_t dev_config = {
        .mode = 0, // SPI mode 0
        .clock_speed_hz = SPI_CLOCK_SPEED,
        .spics_io_num = CS_PIN,
        .queue_size = 7, // We want to be able to queue 1 transaction at a time
    };
    // Attach the device to the SPI bus
    ret = spi_bus_add_device(SPI_BUS_HOST, &dev_config, &spi);
    assert(ret == ESP_OK);

    // spi2 = new SPIClass(SPI_BUS_HOST);
    // spi2->setFrequency(SPI_CLOCK_SPEED);
    // spi2->begin(SCK_PIN, MISO_PIN, MOSI_PIN);
    // spi2->setHwCs(true);
    // can0 = new MCP_CAN(spi2, CS_PIN);
    // if(can0->begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK)
    //     puts("MCP2515 Initialized Successfully!");
    // else
    //     puts("Error Initializing MCP2515...");
    // can0->setMode(MCP_NORMAL);
    // puts("MCP2515 Library Receive Example...");

    // pinMode(CS_PIN, OUTPUT);

    mcp2515 = new MCP2515(&spi);

    // mcp2515 = new MCP2515(CS_PIN, SPI_CLOCK_SPEED, spi2);

    printf("%d\n", mcp2515->reset());
    printf("%d\n", mcp2515->setBitrate(CAN_250KBPS, MCP_8MHZ));
    printf("%d\n", mcp2515->setNormalMode());

    puts("setup complete");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
}

void loop()
{
    MCP2515::ERROR res = mcp2515->readMessage(&frame);
    if (res == MCP2515::ERROR_OK)
    {
        printf("ID: %x\n", frame.can_id);
        printf("Size: %d\n", frame.can_dlc);
        for (int i = 0; i < frame.can_dlc; i++)
            printf("%x", frame.data[i]);
        printf("\n\n");
    }
    else
    {
        printf("%d\n", res);
    }

    // can0->readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    
    // if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
    //   sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    // else
    //   sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
  
    // puts(msgString);
  
    // if((rxId & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
    //   sprintf(msgString, " REMOTE REQUEST FRAME");
    //   puts(msgString);
    // } else {
    //   for(byte i = 0; i<len; i++){
    //     sprintf(msgString, " 0x%.2X", rxBuf[i]);
    //     puts(msgString);
    //   }
    // }

    vTaskDelay(100 / portTICK_PERIOD_MS);
}
