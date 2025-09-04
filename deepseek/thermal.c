#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// 16-level character set ordered by fill percentage (provided by user)
const char* CHARS_16 = " .':;+*oxX#&%B$@";

void print_thermal_ascii(float data[24][32]) {
    // Find min and max values
    float min_val = data[0][0];
    float max_val = data[0][0];
    
    for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 32; j++) {
            if (data[i][j] < min_val) min_val = data[i][j];
            if (data[i][j] > max_val) max_val = data[i][j];
        }
    }
    
    // Handle case where all values are the same
    if (max_val - min_val < 0.001) {
        min_val -= 1.0;
        max_val += 1.0;
    }
    
    // Generate and print ASCII art
    for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 32; j++) {
            // Normalize value to [0, 1] range
            float normalized = (data[i][j] - min_val) / (max_val - min_val);
            
            // Map to character index (0 to 15)
            int index = (int)(normalized * 15 + 0.5);
            
            // Ensure we stay within bounds
            if (index < 0) index = 0;
            if (index > 15) index = 15;
            
            putchar(CHARS_16[index]);
        }
        putchar('\n');
    }
    
    // Print the temperature range for reference
    printf("\nTemperature range: %.2f to %.2f\n", min_val, max_val);
}

// Helper function to generate sample data
void generate_sample_data(float data[24][32]) {
    for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 32; j++) {
            // Create a gradient with a hot spot in the center
            float dx = j - 16;
            float dy = i - 12;
            float distance = sqrt(dx*dx + dy*dy);
            data[i][j] = 20.0 + 30.0 * exp(-distance/8.0);
        }
    }
}

int main() {
    float thermal_data[24][32];
    
    // Generate sample thermal data
    generate_sample_data(thermal_data);
    
    // Print thermal image
    printf("Thermal image (16 levels):\n");
    print_thermal_ascii(thermal_data);
    
    return 0;
}