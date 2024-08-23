#include <MFRC522.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <unordered_map>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <HTTPClient.h>

using namespace std;

const char *ssid = "poseidon";
const char *password = "123456789";
const char *serviceID = "";   // Replace with your actual service ID
const char *templateID = ""; // Replace with your actual template ID
const char *userID = "";    // Replace with your actual public key
const char *accessToken = "";

const int pinRST = 5; // pin RST du module RC522
const int pinSDA = 4; // pin SDA du module RC522
const byte rxPin = 16;
const byte txPin = 17;
const byte rx2Pin = 33;
const byte tx2Pin = 32;
HardwareSerial mySerial(1);
SoftwareSerial barcodeScannerSerial(rx2Pin, tx2Pin);
MFRC522 rfid(pinSDA, pinRST);
std::unordered_map<int, bool> cottonId;

String scannedData = "";
String userAction = "";
int spongeCount = 0;
int maxCount = 0;
bool procedureStarted = false;

unsigned char Buffer[9];
unsigned char spongeCountHEX[8] = {0x5A, 0xA5, 0x05, 0x82, 0x60, 0x00, 0x00, 0x00};
unsigned char maxCountHEX[8] = {0x5A, 0xA5, 0x05, 0x82, 0x90, 0x00, 0x00, 0x00};
unsigned char barcodeScannerState[8] = {0x5A, 0xA5, 0x05, 0x82, 0x55, 0x00, 0x00, 0x00};
unsigned char rfidScannerState[8] = {0x5A, 0xA5, 0x05, 0x82, 0x56, 0x00, 0x00, 0x00};
TaskHandle_t task1Handle;
TaskHandle_t task2Handle;
TaskHandle_t task3Handle;
TaskHandle_t task4Handle;
TaskHandle_t task5Handle;
SemaphoreHandle_t cottonIdMutex;
SemaphoreHandle_t userActionMutex;

String procedureStartTime = "";
String procedureEndTime = "";
int attempts = 0;

void setup()
{
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Connected to WiFi");
        Serial.println("IP Address: " + WiFi.localIP().toString());
    }
    else
    {
        Serial.println("Failed to connect to WiFi");
    }

    SPI.begin();
    rfid.PCD_Init();
    
    barcodeScannerSerial.begin(9600);
    mySerial.begin(115200, SERIAL_8N1, rxPin, txPin);

    xTaskCreatePinnedToCore(manageUserInput, "Task2", 10000, NULL, 0, &task2Handle, 0);
    xTaskCreatePinnedToCore(manageCottonId, "Task1", 10000, NULL, 0, &task1Handle, 0);
    xTaskCreatePinnedToCore(handleScanners, "Task4", 1000, NULL, 0, &task4Handle, 1);
    xTaskCreatePinnedToCore(manageRFID, "Task5", 1000, NULL, 0, &task5Handle, 1);
    xTaskCreatePinnedToCore(updateUI, "Task3", 1000, NULL, 0, &task3Handle, 0);
    cottonIdMutex = xSemaphoreCreateMutex();
    userActionMutex = xSemaphoreCreateMutex();
}

void sendEmail(const String &to_email, const String &from_name, const String &subject,
               int sponge_count, int total_retention, const String &procedure_start_time,
               const String &procedure_end_time, const String &report_summary)
{

    // vTaskSuspend(task1Handle);
    // vTaskSuspend(task2Handle);
    // vTaskSuspend(task3Handle);
    // vTaskSuspend(task4Handle);
    // vTaskSuspend(task5Handle);
    
    HTTPClient http;

    // Your EmailJS API endpoint
    String url = "https://api.emailjs.com/api/v1.0/email/send";
    http.begin(url);

    // Specify content type and headers
    http.addHeader("Content-Type", "application/json");

    // Prepare JSON payload
    String jsonData = "{\"service_id\": \"" + String(serviceID) + "\","
                                                                  "\"template_id\": \"" +
                      String(templateID) + "\","
                                           "\"user_id\": \"" +
                      String(userID) + "\","
                                       "\"template_params\": {"
                                       "\"to_email\": \"" +
                      to_email + "\","
                                 "\"from_name\": \"" +
                      from_name + "\","
                                  "\"subject\": \"" +
                      subject + "\","
                                "\"sponge_count\": \"" +
                      String(sponge_count) + "\","
                                             "\"total_retention\": \"" +
                      String(total_retention) + "\","
                                                "\"procedure_start_time\": \"" +
                      procedure_start_time + "\","
                                             "\"procedure_end_time\": \"" +
                      procedure_end_time + "\","
                                           "\"report_summary\": \"" +
                      report_summary + "\","
                                       "\"patient_name\": \"" +
                      'vinay' + "\"" // Added patient_name here
                                     "},"
                                     "\"accessToken\": \"" +
                      String(accessToken) + "\"}";

    // Send HTTP POST request
    int httpResponseCode = http.POST(jsonData);

    // Handle response
    if (httpResponseCode > 0)
    {
        String response = http.getString();
        Serial.println("Response: " + response);
    }
    else
    {
        Serial.println("Error on sending POST: " + String(httpResponseCode));
    }

    // Free resources
    http.end();
    // vTaskResume(task1Handle);
    // vTaskResume(task2Handle);
    // vTaskResume(task3Handle);
    // vTaskResume(task4Handle);
    // vTaskResume(task5Handle);
}
void handleScanners(void *parameters)
{
    while (true)
    {
        if (barcodeScannerSerial.available() && procedureStarted == true && barcodeScannerState[7] == 1)
        {

            xSemaphoreTake(cottonIdMutex, portMAX_DELAY);
            scannedData = barcodeScannerSerial.readStringUntil('\n');
            Serial.println(scannedData);

            int startPos = 0;

            // while (startPos < scannedData.length()) {
            //   String rfid = "";
            //   // Extract the last 2 digits of the combined RFID ID

            //   rfid = scannedData.substring(startPos, startPos + 2);

            //   int rfidInt = rfid.toInt();
            //   // Add the cotton ID to the unordered map
            //   if (rfidInt == 0) break;
            //   if (barcodeScannerState[7] == 0x01) {
            //     cottonId[rfidInt] = true;
            //     //  Serial.println(rfidInt);
            //     //  Serial.println("added");
            //   }

            //   // Serial.println(rfidInt);
            //   // Move to the next RFID ID
            //   startPos += 2;
            // }
            String rfid = "";
            while (startPos < scannedData.length())
            {
                if (scannedData[startPos] == '*' || startPos == scannedData.length() - 1)
                {
                    cottonId[rfid.toInt()] = true;
                    // Serial.println(rfid);
                    rfid = "";
                    startPos++;
                }
                else
                {
                    rfid += scannedData[startPos];
                    startPos++;
                }
            }

            scannedData = ""; // Clear the scanned data for the next scan
            xSemaphoreGive(cottonIdMutex);
            // continue;
        }

        //    vTaskDelay(pdMS_TO_TICKS(1000));

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void manageRFID(void *parameter)
{

    while (true)
    {
        if (rfid.PICC_IsNewCardPresent() && procedureStarted == true && rfidScannerState[7] == 1 && rfid.PICC_ReadCardSerial())
        {

            xSemaphoreTake(cottonIdMutex, portMAX_DELAY);
            // Serial.println("reached here");
            int rfidData = (rfid.uid.uidByte[rfid.uid.size - 1]); // Read RFID data
            // for(int i=0;i<rfid.uid.size;i++){
            //   Serial.print(rfid.uid.uidByte[i]);
            // }

            if (cottonId.find(rfidData) != cottonId.end())
            {
                cottonId.erase(rfidData); // Remove the RFID ID from the unordered map
                Serial.println("Removed RFID ID: ");
                // Serial.println(rfidData);
            }
            else
            {
                Serial.println("RFID ID not found: ");
                Serial.println(rfidData);
            }
            xSemaphoreGive(cottonIdMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(110));
    }
}
void updateUI(void *parameter)
{
    while (true)
    {
        mySerial.write(spongeCountHEX, 8);
        vTaskDelay(pdMS_TO_TICKS(1000));
        mySerial.write(maxCountHEX, 8);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void manageUserInput(void *parameter)
{
    while (true)
    {

        if (mySerial.available())
        {
            xSemaphoreTake(userActionMutex, portMAX_DELAY);
            // Access the Buffer array safely
            for (int i = 0; i < 9; i++)
            {
                Buffer[i] = mySerial.read();
            }

            // Analyze the data in the buffer to handle user actions
            if (Buffer[0] == 0x5A)
            {
                switch (Buffer[4])
                {
                case 0x55: // for barcode scanner
                    if (Buffer[8] == 1)
                    {
                        Serial.println("Barcode scanner ON");
                        barcodeScannerState[7] = 1;
                    }
                    else if (Buffer[8] == 0)
                    {
                        Serial.println("Barcode scanner OFF");
                        barcodeScannerState[7] = 0;
                    }
                    break;

                case 0x56: // for RFID scanner
                    if (Buffer[8] == 1)
                    {
                        Serial.println("RFID scanner ON");
                        rfidScannerState[7] = 1;
                    }
                    else if (Buffer[8] == 0)
                    {
                        Serial.println("RFID scanner OFF");
                        rfidScannerState[7] = 0;
                    }
                    break;

                case 0x70: // Program control - start
                    if (Buffer[8] == 1)
                    {
                        // Serial.println("Start the program");
                        userAction = "start";
                        procedureStartTime = getFormattedTime();
                    }
                    break;

                case 0x71: // Program control - end
                    if (Buffer[8] == 1)
                    {
                        procedureEndTime = getFormattedTime();
                        sendEmail("vinaychandra166@gmail.com", "Dr. Smith", "Sponge Usage Report",
                                  maxCount, spongeCount, procedureStartTime , procedureEndTime, "Summary of the procedure...");
                        // Serial.println("End the program");
                        userAction = "end";
                    }
                    break;

                case 0x72: // Program control - force stop
                    if (Buffer[8] == 1)
                    {
                        // Serial.println("Force stop");
                        userAction = "forceStop";
                    }
                    break;
                }
            }
        }
        xSemaphoreGive(userActionMutex);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void manageCottonId(void *patameter)
{
    while (true)
    {
        xSemaphoreTake(cottonIdMutex, portMAX_DELAY);
        xSemaphoreTake(userActionMutex, portMAX_DELAY);

        // manage the data received

        spongeCount = cottonId.size();
        maxCount = max(spongeCount, maxCount);

        spongeCountHEX[6] = highByte(spongeCount);
        spongeCountHEX[7] = lowByte(spongeCount);
        maxCountHEX[6] = highByte(maxCount);
        maxCountHEX[7] = lowByte(maxCount);

        // Check if there's a new user action
        if (!userAction.isEmpty())
        {
            if (userAction == "start")
            {
                // Initialize a new Procedure instance
                procedureStarted = true;
            }
            else if (userAction == "end")
            {
                if (cottonId.size() == false)
                {
                    procedureStarted = false;
                    spongeCount = 0;
                    maxCount = 0;
                    cottonId.clear();
                }

            }
            else if (userAction == "forceStop")
            {
                procedureStarted = false;
                spongeCount = 0;
                maxCount = 0;
                cottonId.clear();
            }
            // Reset the shared resource
            userAction = "";
        }

        xSemaphoreGive(userActionMutex);
        xSemaphoreGive(cottonIdMutex);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void loop()
{
    delay(1);
}

String getFormattedTime()
{
    String jsonResponse = fetchCurrentTime();
    if (jsonResponse != "")
    {
        return parseTimeFromJson(jsonResponse);
    }
    else
    {
        return "Time not available";
    }
}

String fetchCurrentTime()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        http.begin("https://timeapi.io/api/Time/current/zone?timeZone=Asia/Kolkata");
        int httpResponseCode = http.GET();

        if (httpResponseCode > 0)
        {
            String payload = http.getString();
            Serial.println(httpResponseCode);
            Serial.println(payload);
            http.end();
            return payload;
        }
        else
        {
            Serial.print("Error on HTTP request: ");
            Serial.println(httpResponseCode);
            http.end();
            return "";
        }
    }
    else
    {
        Serial.println("Error in WiFi connection");
        return "";
    }
}

String parseTimeFromJson(String json)
{
    // Simple JSON parsing to extract dateTime
    int startIndex = json.indexOf("\"dateTime\":\"") + 12;
    int endIndex = json.indexOf("\",", startIndex);
    if (startIndex != -1 && endIndex != -1)
    {
        return json.substring(startIndex, endIndex);
    }
    return "Unknown Time";
}