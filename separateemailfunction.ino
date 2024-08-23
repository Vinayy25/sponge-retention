#include <WiFi.h>
#include <HTTPClient.h>

// Replace with your network credentials

const char *ssid = "poseidon";
const char *password = "123456789";
const char *serviceID = "";   // Replace with your actual service ID
const char *templateID = ""; // Replace with your actual template ID
const char *userID = "";    // Replace with your actual public key
const char *accessToken = "";

// Replace with your FastAPI endpoint URL
const char *serverUrl = "https://627a-2401-4900-901a-9074-3ad-ca36-949c-a267.ngrok-free.app/send-email/";
void setup()
{
    // Initialize Serial Monitor
    Serial.begin(115200);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Make HTTP POST request
    if (WiFi.status() == WL_CONNECTED)
    {
        sendEmail("vinaychandra166@gmail.com", "Dr. Smith", "Sponge Usage Report",
                  20, 5, "2024-08-23T10:00:00", "2024-08-23T11:00:00", "Summary of the procedure...");
    }
    else
    {
        Serial.println("Error in WiFi connection");
    }
}

void loop()
{
    // Nothing to do here
}

void sendEmail(const String &to_email, const String &from_name, const String &subject,
               int sponge_count, int total_retention, const String &procedure_start_time,
               const String &procedure_end_time, const String &report_summary)
{
    if (WiFi.status() == WL_CONNECTED)
    {
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
                          report_summary + "\""
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
    }
    else
    {
        Serial.println("Error in WiFi connection");
    }
}
