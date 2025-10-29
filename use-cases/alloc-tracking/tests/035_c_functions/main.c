#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 1. fprintf - Write size to file, then read it back
int example_fprintf() {
    FILE* f = fopen("size.txt", "w");
    fprintf(f, "%d", 100);  // Write size to file
    fclose(f);
    
    f = fopen("size.txt", "r");
    int n;
    fscanf(f, "%d", &n);   // Read size back
    fclose(f);
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by fprintf
    free(arr);
    return 0;
}

// 2. printf - User input scenario (size written to stdout, read from stdin)
int example_printf() {
    printf("Enter array size: ");
    int n;
    scanf("%d", &n);  // Size comes from user after printf prompt
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by printf interaction
    free(arr);
    return 0;
}

// 3. sprintf - Format size into string, then parse it
int example_sprintf() {
    char buffer[50];
    sprintf(buffer, "size=%d", 75);  // Create formatted string with size
    
    int n;
    sscanf(buffer, "size=%d", &n);   // Parse size from formatted string
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by sprintf
    free(arr);
    return 0;
}

// 4. snprintf - Similar to sprintf but with bounds checking
int example_snprintf() {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%d", 150);  // Write size to buffer
    
    int n = atoi(buffer);  // Convert string to integer
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by snprintf
    free(arr);
    return 0;
}

// 5. scanf - Direct input of size
int example_scanf() {
    printf("Enter size: ");
    int n;
    scanf("%d", &n);  // Size comes directly from scanf
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by scanf
    free(arr);
    return 0;
}

// 6. sscanf - Parse size from existing string
int example_sscanf() {
    char data[] = "array_size:200";
    int n;
    sscanf(data, "array_size:%d", &n);  // Parse size from string
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by sscanf
    free(arr);
    return 0;
}

// 7. fread - Read binary size data
int example_fread() {
    FILE* f = fopen("binary_size.dat", "wb");
    int size_to_write = 80;
    fwrite(&size_to_write, sizeof(int), 1, f);  // Write binary size
    fclose(f);
    
    f = fopen("binary_size.dat", "rb");
    int n;
    fread(&n, sizeof(int), 1, f);  // Read binary size
    fclose(f);
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by fread
    free(arr);
    return 0;
}

// 8. fwrite - Write size data that affects subsequent malloc
int example_fwrite() {
    FILE* f = fopen("temp_size.dat", "wb");
    int original_size = 120;
    fwrite(&original_size, sizeof(int), 1, f);  // Write size to file
    fclose(f);
    
    f = fopen("temp_size.dat", "rb");
    int n;
    fread(&n, sizeof(int), 1, f);  // Size comes from what fwrite stored
    fclose(f);
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by fwrite
    free(arr);
    return 0;
}

// 9. fgetc - Read character and use ASCII value as size
int example_fgetc() {
    FILE* f = fopen("char_size.txt", "w");
    fputc('A', f);  // Write character 'A' (ASCII 65)
    fclose(f);
    
    f = fopen("char_size.txt", "r");
    int n = fgetc(f);  // Read character, get ASCII value
    fclose(f);
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by fgetc (ASCII value)
    free(arr);
    return 0;
}

// 10. getc - Similar to fgetc
int example_getc() {
    FILE* f = fopen("char_size2.txt", "w");
    fputc('B', f);  // Write character 'B' (ASCII 66)
    fclose(f);
    
    f = fopen("char_size2.txt", "r");
    int n = getc(f);  // Read character using getc
    fclose(f);
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by getc
    free(arr);
    return 0;
}

// 11. getchar - Read from stdin
int example_getchar() {
    printf("Enter a character: ");
    int n = getchar();  // Read character from stdin
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by getchar (ASCII value)
    free(arr);
    return 0;
}

// 12. fputc - Write character, then read it back for size
int example_fputc() {
    FILE* f = fopen("fputc_size.txt", "w");
    fputc('C', f);  // Write character using fputc
    fclose(f);
    
    f = fopen("fputc_size.txt", "r");
    int n = fgetc(f);  // Size comes from what fputc wrote
    fclose(f);
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by fputc
    free(arr);
    return 0;
}

// 13. putc - Similar to fputc
int example_putc() {
    FILE* f = fopen("putc_size.txt", "w");
    putc('D', f);  // Write character using putc
    fclose(f);
    
    f = fopen("putc_size.txt", "r");
    int n = getc(f);  // Size comes from what putc wrote
    fclose(f);
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by putc
    free(arr);
    return 0;
}

// 14. putchar - Write to stdout, simulate reading back
int example_putchar() {
    // Simulate scenario where putchar output influences size
    putchar('E');  // Write character to stdout
    printf("\nEnter the ASCII value of the character you saw: ");
    int n;
    scanf("%d", &n);  // Size influenced by putchar interaction
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by putchar
    free(arr);
    return 0;
}

// 15. fgets - Read string and use its length as size
int example_fgets() {
    FILE* f = fopen("string_size.txt", "w");
    fputs("Hello World", f);  // Write string
    fclose(f);
    
    f = fopen("string_size.txt", "r");
    char buffer[100];
    fgets(buffer, sizeof(buffer), f);  // Read string using fgets
    fclose(f);
    
    int n = strlen(buffer);  // Use string length as size
    int* arr = malloc(n * sizeof(int));  // Size influenced by fgets
    free(arr);
    return 0;
}

// 16. fputs - Write string, then use its length for malloc
int example_fputs() {
    FILE* f = fopen("fputs_size.txt", "w");
    fputs("Programming", f);  // Write string using fputs
    fclose(f);
    
    f = fopen("fputs_size.txt", "r");
    char buffer[100];
    fgets(buffer, sizeof(buffer), f);  // Read back what fputs wrote
    fclose(f);
    
    int n = strlen(buffer);  // Size influenced by fputs
    int* arr = malloc(n * sizeof(int));
    free(arr);
    return 0;
}

// 17. puts - Write to stdout, use length for size
int example_puts() {
    char str[] = "TestString";
    puts(str);  // Write string to stdout
    
    int n = strlen(str);  // Size influenced by puts (string length)
    int* arr = malloc(n * sizeof(int));
    free(arr);
    return 0;
}

// 18. fseek - Use seek position as size
int example_fseek() {
    FILE* f = fopen("seek_size.txt", "w");
    fputs("0123456789ABCDEF", f);  // Write some data
    fclose(f);
    
    f = fopen("seek_size.txt", "r");
    fseek(f, 0, SEEK_END);  // Seek to end
    long n = ftell(f);      // Get position (file size)
    fclose(f);
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by fseek/ftell
    free(arr);
    return 0;
}

// 19. ftell - Use file position as size
int example_ftell() {
    FILE* f = fopen("tell_size.txt", "w");
    fputs("SomeData", f);
    fclose(f);
    
    f = fopen("tell_size.txt", "r");
    fgetc(f); fgetc(f); fgetc(f);  // Read a few characters
    long n = ftell(f);  // Get current position
    fclose(f);
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by ftell
    free(arr);
    return 0;
}

// 20. rewind - Reset and read size
int example_rewind() {
    FILE* f = fopen("rewind_size.txt", "w");
    fprintf(f, "90");  // Write size
    fclose(f);
    
    f = fopen("rewind_size.txt", "r");
    fgetc(f);  // Read one character
    rewind(f); // Reset to beginning
    int n;
    fscanf(f, "%d", &n);  // Now read the size
    fclose(f);
    
    int* arr = malloc(n * sizeof(int));  // Size influenced by rewind
    free(arr);
    return 0;
}


int main() {
    
    //int t = example_fprintf();
    //int t = example_printf();
    //int t = example_sprintf();
    //int t = example_snprintf();
    //int t = example_scanf();
    //int t = example_sscanf();
    //int t = example_fread();
    //int t = example_fwrite();
    //int t = example_fgetc();
    //int t = example_getc();
    //int t = example_getchar();
    //int t = example_fputc();
    //int t = example_putc();
    //int t = example_putchar();
    //int t = example_fgets();
    //int t = example_fputs();
    //int t = example_puts();
    //int t = example_fseek();
    //int t = example_ftell();
    //int t = example_rewind();
    
    return 0;
}