#define WIFI_SSID           "router-wifi-2098"
#define WIFI_PASSWORD       "govtech123"
#define PROJECT_ID          "tflitemicro"
#define CLIENT_EMAIL        "tflite-model-data@tflitemicro.iam.gserviceaccount.com"
#define SPREADSHEET_ID      "10lDtDvtX3gYcUmB5cjaqJvrExTP5DvIAbabOjkHh6dQ" 
#define COPY_SPREADSHEET_ID "14btqANCxqC-HhbXxwSu5VolObFxZFzvdpE0jYIRYi1U"

// Amazon S3 OTA details
#define S3_HOST             "eg3301rota.s3.ap-southeast-1.amazonaws.com" // Host => bucket-name.s3.region.amazonaws.com
#define S3_BIN              "/firmware.bin"                              // bin file name with a slash in front.
#define S3_PORT             80                                           // Non https. For HTTPS 443. As of today, HTTPS doesn't work.

// Blynk details
#define BLYNK_TEMPLATE_ID       "TMPLbMbSLqQv"
#define BLYNK_TEMPLATE_NAME     "Norika Water Meter and Pressure Sensor"
#define BLYNK_AUTH_TOKEN        "WJPexy_64oJzZqY9dcOe-nQKiEtQDSz6"
#define OTA_VPIN                 V0
#define FIRMWARE_VERSION_VPIN    V3
#define FIRMWARE_VERSION        "1.0.0"

#include <pgmspace.h>
const char GOOGLESHEETS_PRIVATE_KEY[] PROGMEM =  "-----BEGIN PRIVATE KEY-----\n"
                                    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQC58qKO4efX5flV\n"
                                    "zi/uOJsPwHKdyG3FSLlW0QPf4MsaQk3/OIMQK5UGFzcBZ4+njJvHaiUX1qyU+6ts\n"
                                    "ljiaYhDoQ9vAwfMDYnw3pfU1bI+Yq7mrnL9ARLgu+ow97pYIxQg7kXnGvknqAH2s\n"
                                    "KhmCW51qL1hC95F0AMmdreYbPcVEy2jrzrXt2sVyWkdA27z09Yu9Jd3jAHPt9vmK\n"
                                    "k6tnJEB0CTuSq45EMGv2JuscUBLdYjtgLFmuoLAu0YPeKg6sv4zkUBR0zbwjTiGS\n"
                                    "DmGxJxsNmfAWtWD+ZqqbUQkfJPnFyoYNu02U6h8rlTJK6Ssn9uBqs3Qc2L2kBK3i\n"
                                    "yQ4CrcotAgMBAAECggEAHaf3KaMZYxtMRhVVmyB/iIZYsVXxPeTwnEpHHCphsh0v\n"
                                    "i9wjOQ2+XYOP1ANEi8QW5znEUzY7faJaz31w5xxrVmPWNTqpi5lmNsja4AuhpkeD\n"
                                    "G/3qmbEsD+AJRGpcFud3vepgeeRI7Q9pe68WNO5wH2qQINQTqGTkwudkeuVB5Evn\n"
                                    "pZjbP/Kr3DH9oUCkjmEQ8d+A8oI3dqXxXI35EgLLQ+1v9qZDmi4OHCThg3h3yGeX\n"
                                    "Q0i+oxcdt9bdYNTNBed19VURG/wj2g00anu1oClz6RKgTwl0d8YmMVPzosx8h3GB\n"
                                    "o6JUb/TJZsdda46x3I6LvjoTFPvN0oA8iiDRqeGFQQKBgQD2DGHqnMC1w1QxHMrt\n"
                                    "PoXBkkL3lEhTN5GNVMxoD1bqUJcb2bb2x7TD95If9Q0JXWIljhjF7JPYWmk5wOd8\n"
                                    "IrBNzES2qZAEBXj0M+axIcEZWFdxe9t/GQ13Q771l3PsL7Kg6VHs00H8DZQ/tM/f\n"
                                    "v9K5tWR8TXMesf+q0Renw7AdbQKBgQDBd/aRPO8uDSOkCZIOVTetDDFMQaxOBtAc\n"
                                    "brcA0ns8ztaTZcxjNXBs7rRMkYECPbu5GH90od3+cvf+Mgy2Epk9WQjA1WGEzOcN\n"
                                    "IZte1RboKwSa9ogdWG3OOEvmiA/nYteeenjqpb4GScJEUCI2NWnqmO0jWAYsQjly\n"
                                    "9zU9mJwnwQKBgQCbe3uszF2i/soHNxCtFyNjDVAwL438uFLMItgAsiDUdRToTo4m\n"
                                    "KFD8vUej1jDkyBQrcz4IZNWQlKGGE3a0pR8QKpMJcuFFCUhD2UBgktn5cC/h0MkF\n"
                                    "6gjuYusbOxfQGtwgfxB8PYunAdW65EGwPQGmxQ+41SB4Nzc+9F6kby6tiQKBgDqN\n"
                                    "zz9X3N/oNCBCkUTrP8WNCiKVQcv/vd7NF3AVRB08UK8dwUVJeDRP7pu58fy2qGk1\n"
                                    "4+Vt1B1duHbjuPsmF+D8YGzUaAZkaY2M3VRPU/aChotMEBgpmlouqbIk/gM+5Blf\n"
                                    "4dbKwP9wNW6tfh0//0V0cVkgHAYrKO4FqcRxutlBAoGBAPW4Y5vBFzQw2D1sA4Y3\n"
                                    "WbummGlr8Es/ZZAipg06+2Ur8zipChsPQzMRAcvxiVnBfYdQqip9xGewUo7uI88K\n"
                                    "MNiBblG+rIK2I67zHSv1OmIlQY7WlPItb429Vy5NX0/UV4cNcHPnsQTTlZiHJhU8\n"
                                    "98qytCJOL0RY2oletzzdLNsZ\n"
                                    "-----END PRIVATE KEY-----\n";