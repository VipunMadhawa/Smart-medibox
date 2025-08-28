#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <time.h>

// OLED display parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Pin definitions
#define BUZZER 12
#define LED_1 14
#define PB_CANCEL 5
#define PB_OK 35
#define PB_UP 34
#define PB_DOWN 32
#define DHTPIN 19

#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET_DST 0

// Time zone variables
int UTC_OFFSET = 0;
int UTC_OFFSET_MIN = 0;
int UTC_OFFSET_HOUR = 0;

// Display objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

// Time variables
struct tm timeinfo;
char timeString[50];
char dateString[50];
char weekdayString[12];

// Alarm system
bool alarm_enabled = true;
const int MAX_ALARMS = 3;
int active_alarms = 2;
int alarm_hours[MAX_ALARMS] = {8, 20, 0};
int alarm_minutes[MAX_ALARMS] = {0, 0, 0};
bool alarm_triggered[MAX_ALARMS] = {false, false, false};
bool alarm_active[MAX_ALARMS] = {true, true, false};

// Snooze variables
bool snooze_active = false;
time_t snooze_end_time = 0;

// Musical notes
const int n_notes = 8;
const int notes[n_notes] = {262, 294, 330, 349, 392, 440, 494, 523};

// Menu system
int current_mode = 0;
const int max_modes = 7;
String modes[max_modes] = {
  "Set Time Zone",
  "Set Alarm 1",
  "Set Alarm 2",
  "View Alarms",
  "Delete Alarm",
  "Temp/Humidity",
  "Toggle Alarms"
};

void setup() {
  // Initialize hardware
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);

  Serial.begin(9600);
  
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Show splash screen
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  print_centered("Medibox", 15, 2);
  print_centered("Starting...", 40, 1);
  display.display();
  delay(1000);

  // Connect to WiFi
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_centered("Connecting to", 15, 1);
    print_centered("WiFi...", 30, 1);
    display.display();
  }
  display.clearDisplay();
  print_centered("Connected!", 20, 2);
  display.display();
  delay(1000);

  // Configure time
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

    // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Add double beep on startup
  tone(BUZZER, notes[0], 200); // First beep (middle C)
  delay(250);
  noTone(BUZZER);
  delay(50);
  tone(BUZZER, notes[4], 200); // Second beep (higher G)
  delay(250);
  noTone(BUZZER);

  // Show splash screen
  display.clearDisplay();
}

void loop() {
  update_time();
  display_clock();
  
  check_alarms();
  check_temp_humidity();
  
  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }
}

void update_time() {
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  // Apply timezone offset
  timeinfo.tm_hour += UTC_OFFSET_HOUR+1;
  timeinfo.tm_min += UTC_OFFSET_MIN;
  
  // Normalize time
  mktime(&timeinfo);
  
  // Format time and date strings
  strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
  strftime(dateString, sizeof(dateString), "%d %b %Y", &timeinfo);
  strftime(weekdayString, sizeof(weekdayString), "%A", &timeinfo);
}

void display_clock() {
  display.clearDisplay();
  
  // Draw decorative border
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  
  // Display weekday (top center)
  display.setTextSize(1);
  display.setCursor((SCREEN_WIDTH - strlen(weekdayString)*6)/2, 5);
  display.println(weekdayString);
  
  // Display date (below weekday)
  display.setCursor((SCREEN_WIDTH - strlen(dateString)*6)/2, 15);
  display.println(dateString);
  
  // Display time (big font, center)
  display.setTextSize(2);
  display.setCursor((SCREEN_WIDTH - strlen(timeString)*12)/2, 30);
  display.println(timeString);
  
  // Display status icons
  display.setTextSize(1);
  display.setCursor(5, 55);
  display.printf("A:%d/%d", active_alarms, MAX_ALARMS);
  
  // Display temp/humidity
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  display.setCursor(SCREEN_WIDTH - 50, 55);
  display.printf("%.1fC %.1f%%", data.temperature, data.humidity);
  
  display.display();
}

void check_alarms() {
  if (!alarm_enabled) return;

  // Check if snooze time is over
  check_snooze();

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarm_active[i] && 
        alarm_hours[i] == timeinfo.tm_hour && 
        alarm_minutes[i] == timeinfo.tm_min) {
      // If alarm was triggered but snooze is not active, reset the triggered flag
      if (alarm_triggered[i] && !snooze_active) {
        alarm_triggered[i] = false;
      }
      
      // If alarm wasn't triggered yet or snooze time is over
      if (!alarm_triggered[i] && !snooze_active) {
        ring_alarm(i);
        alarm_triggered[i] = true;
      }
    }
  }
}
void ring_alarm(int alarm_num) {
  unsigned long alarm_start = millis();
  
  while (true) { // Infinite loop until user cancels
    // If this is a snooze-triggered alarm
    if (snooze_active) {
      display.clearDisplay();
      display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
      
      // Show snooze header
      display.setTextSize(1);
      display.setCursor((SCREEN_WIDTH - 80)/2, 5);
      display.print("SNOOZE ALARM!");
      
      // Show current time
      display.setTextSize(2);
      display.setCursor((SCREEN_WIDTH - strlen(timeString)*12)/2, 20);
      display.println(timeString);
      
      // Show alarm info
      display.setTextSize(1);
      display.setCursor((SCREEN_WIDTH - 50)/2, 45);
      display.printf("Alarm %d: %02d:%02d", alarm_num+1, alarm_hours[alarm_num], alarm_minutes[alarm_num]);
      
      // Show instructions
      display.setCursor(10, 55);
      display.print("Press CANCEL to stop");
      
      display.display();
    } 
    else { // Normal alarm display
      display.clearDisplay();
      display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
      
      // Show alarm header
      display.setTextSize(1);
      display.setCursor((SCREEN_WIDTH - 80)/2, 5);
      display.print("MEDICINE TIME!");
      
      // Show current time
      display.setTextSize(2);
      display.setCursor((SCREEN_WIDTH - strlen(timeString)*12)/2, 20);
      display.println(timeString);
      
      // Show alarm info
      display.setTextSize(1);
      display.setCursor((SCREEN_WIDTH - 50)/2, 45);
      display.printf("Alarm %d: %02d:%02d", alarm_num+1, alarm_hours[alarm_num], alarm_minutes[alarm_num]);
      
      // Show instructions
      display.setCursor(10, 55);
      display.print("OK:Snooze  CANCEL:Stop");
      
      display.display();
    }
    
    digitalWrite(LED_1, HIGH);
    
    // Play alarm tone continuously
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        digitalWrite(LED_1, LOW);
        noTone(BUZZER);
        alarm_triggered[alarm_num] = false;
        snooze_active = false;
        return;
      }
      
      if (!snooze_active && digitalRead(PB_OK) == LOW) {
        delay(200);
        snooze_alarm();
        digitalWrite(LED_1, LOW);
        noTone(BUZZER);
        return;
      }
      
      tone(BUZZER, notes[i]);
      delay(300);
      noTone(BUZZER);
      delay(50);
    }
    digitalWrite(LED_1, LOW);
    delay(100);
  }
}

void snooze_alarm() {
  snooze_active = true;
  snooze_end_time = time(nullptr) + 300; // 5 minutes
  
  display.clearDisplay();
  print_centered("Snoozed for", 15, 1);
  print_centered("5 minutes", 30, 1);
  display.display();
  delay(1000);
}

void check_snooze() {
  if (snooze_active && time(nullptr) >= snooze_end_time) {
    // Snooze time is over - trigger the alarm again
    snooze_active = false;
    
    // Find which alarm was snoozed and trigger it again
    for (int i = 0; i < MAX_ALARMS; i++) {
      if (alarm_triggered[i]) {
        alarm_triggered[i] = false; // Reset so it can ring again
        ring_alarm(i); // Ring the alarm again with full melody
        return;
      }
    }
  }
}

void check_temp_humidity() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  if (data.temperature < 24 || data.temperature > 32 || 
      data.humidity < 65 || data.humidity > 80) {
    // Flash warning
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_1, HIGH);
      tone(BUZZER, notes[0], 200);
      display.invertDisplay(true);
      delay(200);
      digitalWrite(LED_1, LOW);
      noTone(BUZZER);
      display.invertDisplay(false);
      delay(200);
    }
    
    // Show warning message
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    print_centered("WARNING!", 10, 1);
    
    if (data.temperature < 24) print_centered("Temp too LOW", 25, 1);
    else if (data.temperature > 32) print_centered("Temp too HIGH", 25, 1);
    
    if (data.humidity < 65) print_centered("Humidity LOW", 40, 1);
    else if (data.humidity > 80) print_centered("Humidity HIGH", 40, 1);
    
    print_centered("Press any button", 55, 1);
    display.display();
    
    while (digitalRead(PB_CANCEL) == HIGH && digitalRead(PB_OK) == HIGH && 
           digitalRead(PB_UP) == HIGH && digitalRead(PB_DOWN) == HIGH) {
      delay(50);
    }
    delay(200);
  }
}

// Menu system functions
void go_to_menu() {
  int selected_item = 0;
  
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    
    // Show clock in menu header
    display.setTextSize(1);
    display.setCursor(5, 2);
    display.print(timeString);
    
    // Show menu title
    print_centered("- MENU -", 15, 1);
    
    // Display menu items
    for (int i = 0; i < 3 && (selected_item + i) < max_modes; i++) {
      int y_pos = 25 + (i * 12);
      if (i == 0) {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        print_centered(modes[selected_item + i], y_pos, 1);
        display.setTextColor(SSD1306_WHITE);
      } else {
        print_centered(modes[selected_item + i], y_pos, 1);
      }
    }
    
    display.display();
    
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      selected_item = (selected_item - 1 + max_modes) % max_modes;
    } else if (pressed == PB_DOWN) {
      selected_item = (selected_item + 1) % max_modes;
    } else if (pressed == PB_OK) {
      run_mode(selected_item);
    }
  }
}

int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) return PB_UP;
    if (digitalRead(PB_DOWN) == LOW) return PB_DOWN;
    if (digitalRead(PB_OK) == LOW) return PB_OK;
    if (digitalRead(PB_CANCEL) == LOW) return PB_CANCEL;
    delay(50);
  }
}

void run_mode(int mode) {
  switch (mode) {
    case 0: set_time_zone(); break;
    case 1: set_alarm(0); break;
    case 2: set_alarm(1); break;
    case 3: view_alarms(); break;
    case 4: delete_alarm(); break;
    case 5: show_temp_humidity(); break;
    case 6: toggle_alarms(); break;
  }
}

void set_time_zone() {
  int temp_hour = UTC_OFFSET_HOUR;
  int temp_min = UTC_OFFSET_MIN;
  bool setting_hour = true;
  
  while (true) {
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    
    print_centered("Set Time Zone", 5, 1);
    
    // Show current time
    display.setCursor((SCREEN_WIDTH - strlen(timeString)*6)/2, 20);
    display.println(timeString);
    
    // Show timezone setting
    display.setCursor(20, 35);
    display.print("UTC Offset:");
    
    if (setting_hour) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      display.printf(" %+02d", temp_hour);
      display.setTextColor(SSD1306_WHITE);
      display.print(":");
      display.printf("%02d", temp_min);
    } else {
      display.printf(" %+02d:", temp_hour);
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      display.printf("%02d", temp_min);
      display.setTextColor(SSD1306_WHITE);
    }
    
    // Show instructions
    display.setCursor(10, 55);
    display.print("OK:Next  CANCEL:Back");
    
    display.display();
    
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      if (setting_hour) temp_hour = (temp_hour + 1) % 24;
      else temp_min = (temp_min + 15) % 60;
    } else if (pressed == PB_DOWN) {
      if (setting_hour) temp_hour = (temp_hour - 1 + 24) % 24;
      else temp_min = (temp_min - 15 + 60) % 60;
    } else if (pressed == PB_OK) {
      if (setting_hour) setting_hour = false;
      else break;
    } else if (pressed == PB_CANCEL) {
      if (setting_hour) return;
      else setting_hour = true;
    }
  }
  
  UTC_OFFSET_HOUR = temp_hour;
  UTC_OFFSET_MIN = temp_min;
  UTC_OFFSET = (temp_hour * 3600/5.5)-1 + (temp_min * 60/5.5);
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  
  display.clearDisplay();
  print_centered("Time Zone Set!", 20, 1);
  display.display();
  delay(1000);
}

void set_alarm(int alarm_num) {
  int temp_hour = alarm_hours[alarm_num];
  int temp_min = alarm_minutes[alarm_num];
  bool setting_hour = true; // Start by setting hours
  
  while (true) {
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    
    // Show header
    char header[20];
    sprintf(header, "Set Alarm %d", alarm_num+1);
    print_centered(header, 5, 1);
    
    // Show current time for reference
    display.setCursor((SCREEN_WIDTH - strlen(timeString)*6)/2, 20);
    display.println(timeString);
    
    // Show alarm time being set
    display.setCursor(30, 35);
    display.print("Time: ");
    
    if (setting_hour) {
      // Highlight hours when setting them
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      display.printf("%02d", temp_hour);
      display.setTextColor(SSD1306_WHITE);
      display.print(":");
      display.printf("%02d", temp_min);
    } else {
      // Highlight minutes when setting them
      display.printf("%02d:", temp_hour);
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      display.printf("%02d", temp_min);
      display.setTextColor(SSD1306_WHITE);
    }
    
    // Show instructions
    display.setCursor(10, 55);
    if (setting_hour) {
      display.print("UP/DOWN:Hours  OK:Next");
    } else {
      display.print("UP/DOWN:Mins  OK:Save");
    }
    
    display.display();
    
    int pressed = wait_for_button_press();
    switch (pressed) {
      case PB_UP:
        if (setting_hour) {
          temp_hour = (temp_hour + 1) % 24;
        } else {
          temp_min = (temp_min + 1) % 60;
        }
        break;
        
      case PB_DOWN:
        if (setting_hour) {
          temp_hour = (temp_hour - 1 + 24) % 24;
        } else {
          temp_min = (temp_min - 1 + 60) % 60;
        }
        break;
        
      case PB_OK:
        if (setting_hour) {
          setting_hour = false; // Switch to setting minutes
        } else {
          // Save the alarm settings
          alarm_hours[alarm_num] = temp_hour;
          alarm_minutes[alarm_num] = temp_min;
          alarm_active[alarm_num] = true;
          if (alarm_num >= active_alarms) {
            active_alarms = alarm_num + 1;
          }
          
          display.clearDisplay();
          print_centered("Alarm Set!", 20, 2);
          display.display();
          delay(1000);
          return;
        }
        break;
        
      case PB_CANCEL:
        if (!setting_hour) {
          setting_hour = true; // Go back to setting hours
        } else {
          return; // Exit without saving
        }
        break;
    }
  }
}

void view_alarms() {
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  
  print_centered("Active Alarms", 5, 1);
  
  int y_pos = 20;
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarm_active[i]) {
      display.setCursor(20, y_pos);
      display.printf("Alarm %d: %02d:%02d", i+1, alarm_hours[i], alarm_minutes[i]);
      y_pos += 10;
    }
  }
  
  if (y_pos == 20) {
    print_centered("No active alarms", 30, 1);
  }
  
  display.setCursor(10, 55);
  display.print("Press any button");
  display.display();
  wait_for_button_press();
}

void delete_alarm() {
  int selected = 0;
  
  while (true) {
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    
    print_centered("Delete Alarm", 5, 1);
    
    int y_pos = 20;
    for (int i = 0; i < MAX_ALARMS; i++) {
      if (i == selected) {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      }
      
      display.setCursor(20, y_pos);
      if (alarm_active[i]) {
        display.printf("Alarm %d: %02d:%02d", i+1, alarm_hours[i], alarm_minutes[i]);
      } else {
        display.printf("Alarm %d: (inactive)", i+1);
      }
      
      if (i == selected) {
        display.setTextColor(SSD1306_WHITE);
      }
      y_pos += 10;
    }
    
    display.setCursor(10, 55);
    display.print("OK:Delete  CANCEL:Back");
    display.display();
    
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      selected = (selected - 1 + MAX_ALARMS) % MAX_ALARMS;
    } else if (pressed == PB_DOWN) {
      selected = (selected + 1) % MAX_ALARMS;
    } else if (pressed == PB_OK) {
      if (alarm_active[selected]) {
        alarm_active[selected] = false;
        alarm_triggered[selected] = false;
        active_alarms--;
      }
      break;
    } else if (pressed == PB_CANCEL) {
      return;
    }
  }
  
  display.clearDisplay();
  print_centered("Alarm Deleted!", 20, 2);
  display.display();
  delay(1000);
}

void show_temp_humidity() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  
  print_centered("Environment", 5, 1);
  
  display.setCursor(20, 20);
  display.printf("Temperature: %.1f C", data.temperature);
  
  display.setCursor(20, 30);
  display.printf("Humidity: %.1f %%", data.humidity);
  
  // Show status
  display.setCursor(20, 45);
  if (data.temperature >= 24 && data.temperature <= 32 && 
      data.humidity >= 65 && data.humidity <= 80) {
    display.print("Status: GOOD");
  } else {
    display.print("Status: WARNING");
  }
  
  display.setCursor(10, 55);
  display.print("Press any button");
  display.display();
  wait_for_button_press();
}

void toggle_alarms() {
  alarm_enabled = !alarm_enabled;
  
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  
  print_centered("Alarms", 15, 1);
  print_centered(alarm_enabled ? "ENABLED" : "DISABLED", 35, 2);
  
  display.display();
  delay(1000);
}

void print_centered(const char *text, int y, int size) {
  display.setTextSize(size);
  int x = (SCREEN_WIDTH - strlen(text)*6*size)/2;
  display.setCursor(x, y);
  display.println(text);
}

void print_centered(String text, int y, int size) {
  display.setTextSize(size);
  int x = (SCREEN_WIDTH - text.length()*6*size)/2;
  display.setCursor(x, y);
  display.println(text);
}