#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/twai.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>

#include <MCP2515.h>

#define TX_PIN GPIO_NUM_21
#define TR_PIN GPIO_NUM_20
#define LED_PIN GPIO_NUM_8

AsyncWebServer server(80);
const char* ssid = "BT-S3A2W2";
const char* password = "Uq3IA2I56D4W";

bool initialized = false;

// Define SPI bus parameters
#define SPI_BUS_HOST    SPI2_HOST
#define SPI_DMA_CHAN    SPI_DMA_CH_AUTO
#define SPI_CLOCK_SPEED SPI_MASTER_FREQ_8M
#define MOSI_PIN        GPIO_NUM_1
#define MISO_PIN        GPIO_NUM_2
#define SCK_PIN         GPIO_NUM_0
#define CS_PIN          GPIO_NUM_3
spi_device_handle_t spi;
MCP2515* mcp2515;
can_frame frame;

void log(const char* format, ...)
{
	char buffer[128];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	printf(buffer);
	WebSerial.print(buffer);
}

void setup()
{
	//Turn on LED (Used to indicate the board).
	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, LOW); //Low is on.

	Serial.begin(115200);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	printf("Setup\n");

	// WiFi.mode(WIFI_STA);
    // WiFi.begin(ssid, password);
    // if (WiFi.waitForConnectResult() != WL_CONNECTED)
	// {
    //     log("WiFi Failed!\n");
    // }
	// else
	// {
	// 	log(WiFi.localIP().toString().c_str());
	// 	WebSerial.begin(&server);
	// 	server.begin();
	// }
	
	twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, TR_PIN, TWAI_MODE_NORMAL);
	twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
	twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
	if (esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK)
	{
		log("Driver install error: %x %s\n", err, esp_err_to_name(err));
		return;
	}
	if (esp_err_t err = twai_start() != ESP_OK)
	{
		log("Start error: %x %s\n", err, esp_err_to_name(err));
		return;
	}

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

	mcp2515 = new MCP2515(&spi);

	printf("%d\n", mcp2515->reset());
    printf("%d\n", mcp2515->setBitrate(CAN_250KBPS, MCP_8MHZ));
    printf("%d\n", mcp2515->setNormalMode());

	log("Setup OK\n");
	initialized = true;
}

void loop()
{
	if (!initialized)
		vTaskDelete(NULL);

	vTaskDelay(1000 / portTICK_PERIOD_MS);

	// printf("\n");

	// twai_status_info_t status;
	// if (esp_err_t err = twai_get_status_info(&status) != ESP_OK)
	// {
	// 	log("Status error: %s\n", esp_err_to_name(err));
	// 	return;
	// }
	// log("State: %d, QT/R: %d/%d, Bus: %d\n", status.state, status.msgs_to_tx, status.bus_error_count);

	// twai_message_t message;
	// if (esp_err_t err = twai_receive(&message, 5000 / portTICK_PERIOD_MS) != ESP_OK)
	// {
	// 	log("Receive error: %x %s\n", err, esp_err_to_name(err));
	// 	return;
	// }
	// else
	// {
	// 	log("Receive OK\n");
	// 	log("ID: %x, Flags: %x, DLC: %d, Data: %x\n", message.identifier, message.flags, message.data_length_code, message.data[0]);
	// }

	MCP2515::ERROR res = mcp2515->readMessage(&frame);
    if (res == MCP2515::ERROR_OK)
    {
		//Get ID in either standard or extended format.
		uint32_t id = frame.can_id & CAN_EFF_FLAG ? frame.can_id & CAN_EFF_MASK : frame.can_id & CAN_SFF_MASK;
        printf("ID: %x\n", id);
        printf("Size: %d\n", frame.can_dlc);
        for (int i = 0; i < frame.can_dlc; i++)
            printf("%x", frame.data[i]);
        printf("\n\n");
    }
    else
    {
        printf("%d\n", res);
		return;
    }

	// //Relay on MCP2515.
	// frame.can_id = message.identifier | (message.flags & TWAI_MSG_FLAG_EXTD ? CAN_EFF_FLAG : 0);
	// frame.can_dlc = message.data_length_code;
	// memcpy(frame.data, message.data, message.data_length_code);
	// MCP2515::ERROR res = mcp2515->sendMessage(&frame);
	// if (res != MCP2515::ERROR_OK)
	// 	log("Send error: %d\n", res);
	// else
	// 	log("Send OK\n");

	//Relay on TWAI.
	twai_message_t message;
	message.identifier = frame.can_id & CAN_EFF_FLAG ? frame.can_id & CAN_EFF_MASK : frame.can_id & CAN_SFF_MASK;
	message.flags = frame.can_id & CAN_EFF_FLAG ? TWAI_MSG_FLAG_EXTD : 0;
	message.data_length_code = frame.can_dlc;
	memcpy(message.data, frame.data, frame.can_dlc);
	if (esp_err_t err = twai_transmit(&message, 5000 / portTICK_PERIOD_MS) != ESP_OK)
	{
		log("Transmit error: %x %s\n", err, esp_err_to_name(err));
		return;
	}
	else
	{
		log("Transmit OK\n");
	}
}
