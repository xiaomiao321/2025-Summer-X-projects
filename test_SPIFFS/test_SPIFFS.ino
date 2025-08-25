#include <SPI.h>
#include <TFT_eSPI.h>
#include <SPIFFS.h>

// =======================================================================================
// Global variables
// =======================================================================================
TFT_eSPI tft = TFT_eSPI();

// Structure to hold image data (filename, width, height)
struct ImageData {
  const char* filename;
  int width;
  int height;
};

// IMPORTANT: Update these dimensions (width, height) to match your converted .bin files!
const ImageData images[] = {
  {"/Animation.bin", 200, 200} // <<<--- 请将这里的 64, 64 替换为 click.bin 的实际宽度和高度！
};
const int numImages = sizeof(images) / sizeof(images[0]);
int currentImageIndex = 0;

// =======================================================================================
// Main Setup and Loop
// =======================================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n--- SPIFFS Raw Image Test ---");

  // Initialize TFT
  tft.begin();
  tft.setRotation(1); // Adjust rotation as needed (0-3)
  tft.fillScreen(TFT_BLACK);
  // tft.setSwapBytes(true); // <<<--- 移除这一行，因为 .bin 文件现在已经是正确的大端格式了

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    tft.setTextColor(TFT_RED);
    tft.drawString("SPIFFS Mount Failed!", 20, 20);
    return;
  }

  // List files in SPIFFS for debugging
  listDir("/");

  // Display the first image
  drawImageRaw(images[currentImageIndex]);
}

void loop() {
  // Wait for 3 seconds
  delay(3000);

  // Move to the next image
  currentImageIndex++;
  if (currentImageIndex >= numImages) {
    currentImageIndex = 0; // Loop back to the first image
  }

  // Display the next image
  drawImageRaw(images[currentImageIndex]);
}

// =======================================================================================
// Helper functions
// =======================================================================================

// Function to draw a raw RGB565 image from SPIFFS
void drawImageRaw(ImageData imgData) {
  tft.fillScreen(TFT_BLACK); // Clear screen before drawing new image
  
  File imgFile = SPIFFS.open(imgData.filename, "r");
  if (!imgFile) {
    Serial.printf("Failed to open %s\n", imgData.filename);
    tft.setTextColor(TFT_RED);
    tft.drawString("Error opening file:", 20, 20);
    tft.drawString(imgData.filename, 20, 40);
    return;
  }

  // Calculate position to center the image
  int16_t x = (tft.width() - imgData.width) / 2;
  int16_t y = (tft.height() - imgData.height) / 2;

  // Allocate buffer for one line of pixels
  // For larger images, you might need to read line by line
  // For simplicity, this example reads the whole image into RAM if it fits.
  // If your images are very large, consider reading line by line.
  uint16_t* pixelBuffer = (uint16_t*)malloc(imgData.width * imgData.height * sizeof(uint16_t));
  if (!pixelBuffer) {
    Serial.println("Failed to allocate pixel buffer!");
    imgFile.close();
    tft.setTextColor(TFT_RED);
    tft.drawString("Memory Error!", 20, 60);
    return;
  }

  // Read the entire image data into the buffer
  imgFile.read((uint8_t*)pixelBuffer, imgData.width * imgData.height * sizeof(uint16_t));
  imgFile.close();

  // Push the image to the TFT
  tft.pushImage(x, y, imgData.width, imgData.height, pixelBuffer);
  
  free(pixelBuffer); // Free the allocated memory
  Serial.printf("Displayed %s (%dx%d)\n", imgData.filename, imgData.width, imgData.height);
}

// Function to list files in a directory
void listDir(const char * dirname) {
    Serial.printf("Listing directory: %s\n", dirname);

    File root = SPIFFS.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}
