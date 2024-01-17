#include <Arduino.h>

#if 0
#include <can.h>
#include <mcp2515.h>

#include <CanHacker.h>
#include <CanHackerLineReader.h>
#include <lib.h>

#include <SPI.h>

const int SPI_CS_PIN = GPIO_NUM_7;
const int INT_PIN = GPIO_NUM_10;

#pragma region CAN Hacker
#if 0
CanHackerLineReader *lineReader = NULL;
CanHacker *canHacker = NULL;

void handleError(const CanHacker::ERROR error);

void setup()
{
    Serial.begin(115200);
    SPI.begin();
	vTaskDelay(1000 / portTICK_PERIOD_MS);

    Stream *interfaceStream = &Serial;
    Stream *debugStream = &Serial;
    
    canHacker = new CanHacker(interfaceStream, debugStream, SPI_CS_PIN);
    canHacker->enableLoopback(); // remove to disable loopback test mode
    lineReader = new CanHackerLineReader(canHacker);

	pinMode(INT_PIN, INPUT);
}

void loop()
{
    CanHacker::ERROR error;
    
    // if (digitalRead(INT_PIN) == LOW) {
    //     error = canHacker->processInterrupt();
    //     handleError(error);
    // }
    
    // uncomment that lines for Leonardo, Pro Micro or Esplora
    error = lineReader->process();
    handleError(error);
}

void handleError(const CanHacker::ERROR error)
{
    switch (error)
	{
        case CanHacker::ERROR_OK:
			// printf("OK");
			return;
        case CanHacker::ERROR_UNKNOWN_COMMAND:
			printf("Unknown command");
			return;
        case CanHacker::ERROR_NOT_CONNECTED:
			printf("Not connected");
			return;
        case CanHacker::ERROR_MCP2515_ERRIF:
			printf("MCP2515 error");
			return;
        case CanHacker::ERROR_INVALID_COMMAND:
			printf("Invalid command");
            return;
        default:
            break;
    }
  
    printf("Failure (code %d)", (int)error);
  
    while (true)
        delay(2000);
}
#pragma endregion

#pragma region MCP2515
#else
#define log_error(error) check_error(error, __LINE__)

MCP2515 mcp2515(SPI_CS_PIN);

void check_error(MCP2515::ERROR error, int line)
{
	if (error != MCP2515::ERROR_OK)
	{
		printf("Error: ");
		printf("%d : %010x", line, error);
		printf("\n");
		// vTaskDelay(5000 / portTICK_PERIOD_MS);
		// esp_restart();
	}
}

void setup()
{
	Serial.begin(115200);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	printf("CAN init\n");

	SPI.begin();

	log_error(mcp2515.reset());
	log_error(mcp2515.setBitrate(CAN_250KBPS, MCP_20MHZ));
	log_error(mcp2515.setListenOnlyMode());

	pinMode(INT_PIN, INPUT_PULLUP);

	printf("CAN init OK\n");
}

void loop()
{
	vTaskDelay(500 / portTICK_PERIOD_MS);
	// if (digitalRead(INT_PIN) == LOW)
	// {
		can_frame message;
		MCP2515::ERROR error = mcp2515.readMessage(&message);
		log_error(error);
		if (error != MCP2515::ERROR_OK)
			return;
		printf("ID: ");
		printf("%010x", message.can_id);
		printf(" Data: ");
		for (__u8 i = 0; i < message.can_dlc; i++)
		{
			printf("%010x", message.data[i]);
			printf(" ");
		}
		printf("\n");
	// }
}
#endif
#pragma endregion
#else
#include <mcp_can.h>
#include <mcp_can_dfs.h>
#include <SPI.h>
#include <driver/gpio.h>
#include <iso-tp.h>

#define ISO_TP_DEBUG
#define MCP_CS GPIO_NUM_0 //VSPI CS0
#define MCP_INT GPIO_NUM_1

MCP_CAN* CAN0;
IsoTp* isotp;

struct Message_t rxMsg;

void setup()
{
  // serial
  Serial.begin(115200);
  // interrupt
  pinMode(MCP_INT, INPUT);

  SPIClass* hspi = new SPIClass(FSPI);
  hspi->begin(SCK, MISO, MOSI, MCP_CS);
  CAN0 = new MCP_CAN(hspi, MCP_CS);
  isotp = new IsoTp(CAN0, MCP_INT);

  // CAN
  CAN0->begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ);
  CAN0->setMode(MCP_NORMAL);
  // buffers
  rxMsg.Buffer = (uint8_t *)calloc(MAX_MSGBUF, sizeof(uint8_t));
}

void loop()
{
  // CAN
  if (digitalRead(MCP_INT) == 0) {
    rxMsg.tx_id = 0x7ED;
    rxMsg.rx_id = 0x7E5;
    printf("Receive...\n");
    isotp->receive(&rxMsg);
    // isotp.print_buffer(rxMsg.rx_id, rxMsg.Buffer, rxMsg.len);

	uint16_t i=0;

	printf("Buffer: ");
	printf("%010x", rxMsg.rx_id);
	printf(" [");
	printf("%d", rxMsg.len);
	printf("] ");
	for(i=0;i<rxMsg.len;i++)
	{
		if(rxMsg.Buffer[i] < 0x10) printf("0");
		printf("%x", rxMsg.Buffer[i]);
		printf(" ");
	}
	printf("\n");
	}
}
#endif
