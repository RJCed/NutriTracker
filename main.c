/*
    NutriTracker - terminal-based nutrition and diet management system.

    CMSC 18 C Programming project. C11, standard libraries only.
    Persistence is implemented with plain CSV files under data/.
*/

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* 1. Constants                                                        */
/* ------------------------------------------------------------------ */

#define FOODS_PATH    "data/foods.csv"
#define FOODS_TMP     "data/foods.csv.tmp"
#define LOG_PATH      "data/food_log.csv"
#define LOG_TMP       "data/food_log.csv.tmp"
#define SETTINGS_PATH "data/settings.csv"
#define SETTINGS_TMP  "data/settings.csv.tmp"

#define MAX_FOODS        100
#define MAX_LOG_ENTRIES  200
#define FOOD_NAME_LEN    100   /* including the terminating NUL */
#define NAME_MAX_CHARS   99
#define CSV_LINE_LEN     512
#define INPUT_LINE_LEN   64

#define DEFAULT_DAILY_TARGET 2000.0
#define DEFAULT_CURRENT_DIET 1
#define DIET_COUNT           8

#define FOODS_HEADER    "id,name,calories,protein,carbohydrates,fat,fiber,total_sugar,added_sugar,sodium"
#define LOG_HEADER      "date,food_id,servings"
#define SETTINGS_HEADER "setting,value"

/* Nutritional screening thresholds (from the project pseudocode). */
#define HIGH_PROTEIN_MIN      20.0   /* grams */
#define LOW_CARB_MAX          15.0   /* grams */
#define LOW_FAT_MAX           3.0    /* grams */
#define LOW_SODIUM_MAX        140.0  /* milligrams */
#define LOW_ADDED_SUGAR_MAX   5.0    /* grams */
#define HIGH_FIBER_MIN        5.0    /* grams */

#define DISCLAIMER \
    "Note: NutriTracker evaluations are educational screening results only " \
    "and are not medical recommendations.\n"

/* ------------------------------------------------------------------ */
/* 2. Structure definitions                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    int id;
    char name[FOOD_NAME_LEN];
    double calories;       /* kcal per serving            */
    double protein;        /* grams per serving           */
    double carbohydrates;  /* grams per serving           */
    double fat;            /* grams per serving           */
    double fiber;          /* grams per serving           */
    double totalSugar;     /* grams per serving           */
    double addedSugar;     /* grams per serving           */
    double sodium;         /* milligrams per serving      */
} Food;

typedef struct {
    char date[11];         /* YYYY-MM-DD */
    int foodId;
    double servings;
} FoodLog;

typedef struct {
    double dailyTarget;    /* kcal */
    int currentDiet;       /* 1-8  */
} Settings;

/* ------------------------------------------------------------------ */
/* 3. Global data (fixed-size arrays, no dynamic allocation)           */
/* ------------------------------------------------------------------ */

static Food foodDatabase[MAX_FOODS];
static int foodCount = 0;

static FoodLog foodLog[MAX_LOG_ENTRIES];
static int logCount = 0;

static Settings userSettings;

static const char *const DIET_NAMES[DIET_COUNT] = {
    "Balanced Diet",
    "Calorie Deficit Tracking",
    "High Protein",
    "Low Carbohydrate",
    "Low Fat",
    "Low Sodium",
    "Low Added Sugar",
    "High Fiber"
};

/* Initial food database used when foods.csv does not exist yet.
    Values are per reference serving: kcal for calories, grams for
    protein/carbs/fat/fiber/sugars, milligrams for sodium. */
static const Food DEFAULT_FOODS[15] = {
    {1,  "Boiled Egg",     78,  6.3,  0.6,  5.3,  0,   0.6,  0,  62},
    {2,  "Oatmeal",        154, 5.4,  27.4, 2.6,  4.1, 0.5,  0,  115},
    {3,  "Chicken Breast", 165, 31,   0,    3.6,  0,   0,    0,  74},
    {4,  "Banana",         105, 1.3,  27,   0.4,  3.1, 14.4, 0,  1},
    {5,  "White Rice",     205, 4.3,  44.5, 0.4,  0.6, 0.1,  0,  2},
    {6,  "Greek Yogurt",   100, 17,   6,    0.7,  0,   4,    0,  60},
    {7,  "Tuna",           132, 29,   0,    1,    0,   0,    0,  300},
    {8,  "Salmon",         206, 22,   0,    12,   0,   0,    0,  59},
    {9,  "Avocado",        160, 2,    8.5,  14.7, 6.7, 0.7,  0,  7},
    {10, "Sweet Potato",   180, 4,    41,   0.3,  6.6, 13,   0,  72},
    {11, "Broccoli",       55,  3.7,  11.2, 0.6,  5.1, 2.2,  0,  64},
    {12, "Apple",          95,  0.5,  25,   0.3,  4.4, 19,   0,  2},
    {13, "Almonds",        164, 6,    6.1,  14.2, 3.5, 1.2,  0,  0},
    {14, "Peanut Butter",  188, 8,    6,    16,   2,   3,    1,  140},
    {15, "Mango",          99,  1.4,  25,   0.6,  2.6, 23,   0,  2}
};

/* ------------------------------------------------------------------ */
/* 4. Function prototypes                                              */
/* ------------------------------------------------------------------ */

/* Input handling */
typedef enum { INPUT_OK, INPUT_EOF, INPUT_TOO_LONG, INPUT_INVALID } InputStatus;
static InputStatus readLine(char *buf, size_t size);
static InputStatus readInteger(int *out);
static InputStatus promptInteger(const char *label, int min, int max, int *out);
static InputStatus promptDouble(const char *label, int allowZero, double *out);

/* Validation helpers */
static int parseIntStrict(const char *s, int *out);
static int parseDoubleStrict(const char *s, double *out);
static int validFoodName(const char *s);
static int validDate(const char *s);
static void stripLineEnd(char *s);
static int splitCsvLine(char *line, char *fields[], int maxFields);

/* Food database */
static int findFoodIndex(int id);
static int nextFoodId(void);
static void viewFoodDatabase(void);
static void addCustomFood(void);
static void evaluateFood(void);

/* Food log */
static void recordIntake(void);
static void dailyReport(void);
static void getTodayDate(char out[11]);

/* Settings */
static void selectDiet(void);
static void setCalorieTarget(void);

/* File handling */
static int fileExists(const char *path);
static int saveFileAtomic(const char *path, const char *tmpPath, int (*writer)(FILE *));
static int initializeFoods(void);
static int loadFoods(void);
static int saveFoods(void);
static int loadFoodLog(void);
static int saveFoodLog(void);
static int loadSettings(void);
static int saveSettings(void);
static int initializeDataFiles(void);

/* Menu */
static void printMenu(void);
static void menuLoop(void);

int main(void);

/* ------------------------------------------------------------------ */
/* 5. Main                                                             */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("=== NutriTracker ===\n");

    if (initializeDataFiles() != 0) {
        fprintf(stderr, "Fatal: could not initialize the data files; the program will not start.\n");
        fprintf(stderr, "Run the program from the project root and make sure the data/ directory\n");
        fprintf(stderr, "exists (it ships with the repository as data/.gitkeep). Do not delete or\n");
        fprintf(stderr, "corrupt the existing CSV files; they are not replaced automatically.\n");
        return EXIT_FAILURE;
    }

    printf("Loaded %d food(s) and %d log entr%s.\n", foodCount, logCount, logCount == 1 ? "y" : "ies");
    printf("Daily target: %.10g kcal | Diet: %s\n\n", userSettings.dailyTarget, DIET_NAMES[userSettings.currentDiet - 1]);

    menuLoop();
    return EXIT_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* 6. Menu                                                             */
/* ------------------------------------------------------------------ */

static void printMenu(void) {
    printf("---------- Main Menu ----------\n");
    printf("1. Select Diet Category\n");
    printf("2. View Food Database\n");
    printf("3. Evaluate Food for My Diet\n");
    printf("4. Add Custom Food Product\n");
    printf("5. Set Daily Calorie Target\n");
    printf("6. Record Food Intake\n");
    printf("7. View Daily Nutrition Report\n");
    printf("8. Exit Program\n");
    printf("Choice: ");
    fflush(stdout);
}

static void menuLoop(void) {
    for (;;) {
        printMenu();
        int choice;
        InputStatus st = readInteger(&choice);
        if (st == INPUT_EOF) {
            printf("\nInput stream closed. Exiting.\n");
            return;
        }
        if (st != INPUT_OK) {
            printf("Invalid input. Please enter a number between 1 and 8.\n\n");
            continue;
        }
        switch (choice) {
            case 1: selectDiet();        break;
            case 2: viewFoodDatabase();  break;
            case 3: evaluateFood();      break;
            case 4: addCustomFood();     break;
            case 5: setCalorieTarget();  break;
            case 6: recordIntake();      break;
            case 7: dailyReport();       break;
            case 8:
                printf("Goodbye!\n");
                return;
            default:
                printf("Invalid choice. Please enter a number between 1 and 8.\n\n");
                break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* 7. Input-handling functions                                         */
/* ------------------------------------------------------------------ */

/* Read one line with fgets(). The trailing newline (and optional CR) is
    removed. If the line does not fit in the buffer, the remainder is
    consumed and INPUT_TOO_LONG is returned so callers never see a partial
    line. INPUT_EOF is returned when stdin is closed before any input. */
static InputStatus readLine(char *buf, size_t size) {
    if (size < 2) {
        return INPUT_INVALID;
    }
    if (!fgets(buf, (int)size, stdin)) {
        return INPUT_EOF;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
        if (len > 0 && buf[len - 1] == '\r') {
            buf[--len] = '\0';
        }
        return INPUT_OK;
    }
    if (len == size - 1) {
        /* Buffer filled without a newline: the line is too long. Consume
            the rest of the line so the next read starts on a fresh line. */
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {
            /* discard */
        }
        return INPUT_TOO_LONG;
    }
    return INPUT_OK; /* final line at EOF without a trailing newline */
}

/* Parse a whole line as an int using strtol with full error checking.
    Trailing whitespace is allowed; anything else is rejected. */
static InputStatus readInteger(int *out) {
    char line[INPUT_LINE_LEN];
    InputStatus st = readLine(line, sizeof line);
    if (st != INPUT_OK) {
        return st;
    }
    int value;
    if (parseIntStrict(line, &value) != 0) {
        return INPUT_INVALID;
    }
    *out = value;
    return INPUT_OK;
}

/* Repeatedly prompt until the user enters an integer in [min, max].
    Returns INPUT_EOF if stdin closes; the caller should then abort the
    current action. */
static InputStatus promptInteger(const char *label, int min, int max, int *out) {
    for (;;) {
        printf("%s", label);
        fflush(stdout);
        int value;
        InputStatus st = readInteger(&value);
        if (st != INPUT_OK) {
            return st; /* EOF, too long, or non-numeric */
        }
        if (value < min || value > max) {
            printf("Please enter a whole number between %d and %d.\n", min, max);
            continue;
        }
        *out = value;
        return INPUT_OK;
    }
}

/* Repeatedly prompt until the user enters a finite double. If allowZero
    is zero, values <= 0 are rejected; otherwise negative values are
    rejected. Used for nutrition values (>= 0) and quantities (> 0). */
static InputStatus promptDouble(const char *label, int allowZero, double *out) {
    for (;;) {
        printf("%s", label);
        fflush(stdout);
        char line[INPUT_LINE_LEN];
        InputStatus st = readLine(line, sizeof line);
        if (st != INPUT_OK) {
            return st;
        }
        double value;
        if (parseDoubleStrict(line, &value) != 0) {
            printf("Invalid number. Please enter a plain numeric value.\n");
            continue;
        }
        if (allowZero) {
            if (value < 0.0) {
                printf("Value must not be negative.\n");
                continue;
            }
        } else if (value <= 0.0) {
            printf("Value must be greater than zero.\n");
            continue;
        }
        *out = value;
        return INPUT_OK;
    }
}

/* ------------------------------------------------------------------ */
/* 8. Validation helpers                                               */
/* ------------------------------------------------------------------ */

static int parseIntStrict(const char *s, int *out) {
    if (s == NULL || *s == '\0') {
        return -1;
    }
    errno = 0;
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || errno == ERANGE) {
        return -1;
    }
    while (isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return -1; /* trailing garbage such as "12abc" or "1.5" */
    }
    if (v < INT_MIN || v > INT_MAX) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

static int parseDoubleStrict(const char *s, double *out) {
    if (s == NULL || *s == '\0') {
        return -1;
    }
    errno = 0;
    char *end;
    double v = strtod(s, &end);
    if (end == s || errno == ERANGE) {
        return -1; /* no conversion, or overflow/underflow */
    }
    while (isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return -1;
    }
    if (!isfinite(v)) {
        return -1; /* reject NaN and infinity */
    }
    *out = v;
    return 0;
}

/* Food names may contain spaces but no commas, quotes, or control
    characters, and must contain at least one visible character. */
static int validFoodName(const char *s) {
    size_t n = strlen(s);
    if (n == 0 || n > NAME_MAX_CHARS) {
        return 0;
    }
    int hasVisible = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == ',' || c == '"' || c == '\'' || c < 32 || c == 127) {
            return 0;
        }
        if (!isspace(c)) {
            hasVisible = 1;
        }
    }
    return hasVisible;
}

/* Accept YYYY-MM-DD with plausible month and day ranges. */
static int validDate(const char *s) {
    if (strlen(s) != 10 || s[4] != '-' || s[7] != '-') {
        return 0;
    }
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (!isdigit((unsigned char)s[i])) {
            return 0;
        }
    }
    int month = (s[5] - '0') * 10 + (s[6] - '0');
    int day = (s[8] - '0') * 10 + (s[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

static void stripLineEnd(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

/* Split a CSV line in place on commas. No quoting is supported (food
    names reject commas and quotes), so this stays predictable. Returns the
    number of fields, or -1 if there are more than maxFields. */
static int splitCsvLine(char *line, char *fields[], int maxFields) {
    int count = 1;
    fields[0] = line;
    for (char *p = line; *p != '\0'; p++) {
        if (*p == ',') {
            *p = '\0';
            if (count >= maxFields) {
                return -1;
            }
            fields[count++] = p + 1;
        }
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* 9. File handling                                                    */
/* ------------------------------------------------------------------ */

static int fileExists(const char *path) {
    FILE *f = fopen(path, "r");
    if (f != NULL) {
        fclose(f);
        return 1;
    }
    return 0;
}

/* Safe save: write everything to a temporary file in the same directory,
    verify the writes, close it, then atomically replace the original with
    rename(). On any failure the original file is left untouched. */
static int saveFileAtomic(const char *path, const char *tmpPath, int (*writer)(FILE *)) {
    FILE *f = fopen(tmpPath, "w");
    if (f == NULL) {
        fprintf(stderr, "Error: cannot create temporary file %s: %s\n", tmpPath, strerror(errno));
        if (errno == ENOENT) {
            fprintf(stderr, "Hint: the data/ directory is missing. Run the program from the\n");
            fprintf(stderr, "project root, or recreate data/ (the repository ships data/.gitkeep).\n");
        }
        return -1;
    }
    int rc = 0;
    if (writer(f) != 0) {
        fprintf(stderr, "Error: a write to %s failed.\n", tmpPath);
        rc = -1;
    }
    if (rc == 0 && ferror(f)) {
        fprintf(stderr, "Error: stream error detected while writing %s.\n", tmpPath);
        rc = -1;
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "Error: failed to close %s: %s\n", tmpPath, strerror(errno));
        rc = -1;
    }
    if (rc != 0) {
        remove(tmpPath);
        return -1;
    }
    if (rename(tmpPath, path) != 0) {
        fprintf(stderr, "Error: cannot replace %s with %s: %s\n", path, tmpPath, strerror(errno));
        remove(tmpPath);
        return -1;
    }
    return 0;
}

/* ---- foods.csv ---- */

static int writeFoodsFile(FILE *f) {
    if (fprintf(f, "%s\n", FOODS_HEADER) < 0) {
        return -1;
    }
    for (int i = 0; i < foodCount; i++) {
        const Food *fd = &foodDatabase[i];
        if (fprintf(f, "%d,%s,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g\n",
            fd->id, fd->name, fd->calories, fd->protein, fd->carbohydrates,
            fd->fat, fd->fiber, fd->totalSugar, fd->addedSugar, fd->sodium) < 0) {
            return -1;
        }
    }
    return 0;
}

static int saveFoods(void) {
    return saveFileAtomic(FOODS_PATH, FOODS_TMP, writeFoodsFile);
}

static int initializeFoods(void) {
    for (int i = 0; i < 15; i++) {
        foodDatabase[i] = DEFAULT_FOODS[i];
    }
    foodCount = 15;
    if (saveFoods() != 0) {
        foodCount = 0;
        return -1;
    }
    return 0;
}

static int loadFoods(void) {
    FILE *f = fopen(FOODS_PATH, "r");
    if (f == NULL) {
        fprintf(stderr, "Error: cannot open %s: %s\n", FOODS_PATH, strerror(errno));
        return -1;
    }
    char line[CSV_LINE_LEN];
    int lineNo = 0;
    int headerSeen = 0;
    int rc = 0;
    foodCount = 0;

    while (fgets(line, (int)sizeof line, f) != NULL) {
        lineNo++;
        size_t len = strlen(line);
        if (len == sizeof line - 1 && line[len - 1] != '\n') {
            fprintf(stderr, "Error: %s line %d: line is too long.\n", FOODS_PATH, lineNo);
            rc = -1;
            break;
        }
        stripLineEnd(line);

        if (!headerSeen) {
            if (strcmp(line, FOODS_HEADER) != 0) {
                fprintf(stderr, "Error: %s line %d: bad header, expected \"%s\".\n",
                    FOODS_PATH, lineNo, FOODS_HEADER);
                rc = -1;
                break;
            }
            headerSeen = 1;
            continue;
        }
        if (line[0] == '\0') {
            continue; /* tolerate blank lines */
        }
        if (foodCount >= MAX_FOODS) {
            fprintf(stderr, "Error: %s line %d: more than %d food records; refusing to load.\n",
                FOODS_PATH, lineNo, MAX_FOODS);
            rc = -1;
            break;
        }

        char *fields[10];
        int nf = splitCsvLine(line, fields, 10);
        Food food;
        double vals[8];
        if (nf != 10 ||
            parseIntStrict(fields[0], &food.id) != 0 || food.id <= 0 ||
            !validFoodName(fields[1])) {
            fprintf(stderr, "Error: %s line %d: malformed food record.\n", FOODS_PATH, lineNo);
            rc = -1;
            break;
        }
        int badValue = 0;
        for (int i = 0; i < 8; i++) {
            if (parseDoubleStrict(fields[2 + i], &vals[i]) != 0 || vals[i] < 0.0) {
                badValue = 1;
                break;
            }
        }
        if (!badValue && vals[6] > vals[5]) {
            badValue = 1; /* added sugar must not exceed total sugar */
        }
        if (badValue) {
            fprintf(stderr, "Error: %s line %d: invalid numeric value (negative, NaN, infinite, or added sugar > total sugar).\n",
                FOODS_PATH, lineNo);
            rc = -1;
            break;
        }

        strcpy(food.name, fields[1]); /* safe: validFoodName bounds the length */
        food.calories = vals[0];
        food.protein = vals[1];
        food.carbohydrates = vals[2];
        food.fat = vals[3];
        food.fiber = vals[4];
        food.totalSugar = vals[5];
        food.addedSugar = vals[6];
        food.sodium = vals[7];

        int duplicate = 0;
        for (int i = 0; i < foodCount; i++) {
            if (foodDatabase[i].id == food.id) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            fprintf(stderr, "Error: %s line %d: duplicate food ID %d.\n", FOODS_PATH, lineNo, food.id);
            rc = -1;
            break;
        }
        foodDatabase[foodCount++] = food;
    }

    if (rc == 0 && !headerSeen) {
        fprintf(stderr, "Error: %s is empty or has no header line.\n", FOODS_PATH);
        rc = -1;
    }
    if (rc == 0 && ferror(f)) {
        fprintf(stderr, "Error: read error while reading %s.\n", FOODS_PATH);
        rc = -1;
    }
    fclose(f);
    return rc;
}

/* ---- food_log.csv ---- */

static int writeFoodLogFile(FILE *f) {
    if (fprintf(f, "%s\n", LOG_HEADER) < 0) {
        return -1;
    }
    for (int i = 0; i < logCount; i++) {
        if (fprintf(f, "%s,%d,%.10g\n", foodLog[i].date, foodLog[i].foodId, foodLog[i].servings) < 0) {
            return -1;
        }
    }
    return 0;
}

static int saveFoodLog(void) {
    return saveFileAtomic(LOG_PATH, LOG_TMP, writeFoodLogFile);
}

static int loadFoodLog(void) {
    FILE *f = fopen(LOG_PATH, "r");
    if (f == NULL) {
        fprintf(stderr, "Error: cannot open %s: %s\n", LOG_PATH, strerror(errno));
        return -1;
    }
    char line[CSV_LINE_LEN];
    int lineNo = 0;
    int headerSeen = 0;
    int rc = 0;
    logCount = 0;

    while (fgets(line, (int)sizeof line, f) != NULL) {
        lineNo++;
        size_t len = strlen(line);
        if (len == sizeof line - 1 && line[len - 1] != '\n') {
            fprintf(stderr, "Error: %s line %d: line is too long.\n", LOG_PATH, lineNo);
            rc = -1;
            break;
        }
        stripLineEnd(line);

        if (!headerSeen) {
            if (strcmp(line, LOG_HEADER) != 0) {
                fprintf(stderr, "Error: %s line %d: bad header, expected \"%s\".\n",
                    LOG_PATH, lineNo, LOG_HEADER);
                rc = -1;
                break;
            }
            headerSeen = 1;
            continue;
        }
        if (line[0] == '\0') {
            continue;
        }
        if (logCount >= MAX_LOG_ENTRIES) {
            fprintf(stderr, "Error: %s line %d: more than %d log entries; refusing to load.\n",
                LOG_PATH, lineNo, MAX_LOG_ENTRIES);
            rc = -1;
            break;
        }

        char *fields[3];
        FoodLog entry;
        double servings;
        if (splitCsvLine(line, fields, 3) != 3 ||
            !validDate(fields[0]) ||
            parseIntStrict(fields[1], &entry.foodId) != 0 ||
            parseDoubleStrict(fields[2], &servings) != 0 || servings <= 0.0) {
            fprintf(stderr, "Error: %s line %d: malformed log entry.\n", LOG_PATH, lineNo);
            rc = -1;
            break;
        }
        if (findFoodIndex(entry.foodId) < 0) {
            fprintf(stderr, "Error: %s line %d: references unknown food ID %d.\n",
                LOG_PATH, lineNo, entry.foodId);
            rc = -1;
            break;
        }
        strcpy(entry.date, fields[0]); /* safe: validDate guarantees 10 chars + NUL */
        entry.servings = servings;
        foodLog[logCount++] = entry;
    }

    if (rc == 0 && !headerSeen) {
        fprintf(stderr, "Error: %s is empty or has no header line.\n", LOG_PATH);
        rc = -1;
    }
    if (rc == 0 && ferror(f)) {
        fprintf(stderr, "Error: read error while reading %s.\n", LOG_PATH);
        rc = -1;
    }
    fclose(f);
    return rc;
}

/* ---- settings.csv ---- */

static int writeSettingsFile(FILE *f) {
    if (fprintf(f, "%s\n", SETTINGS_HEADER) < 0 ||
        fprintf(f, "daily_target,%.10g\n", userSettings.dailyTarget) < 0 ||
        fprintf(f, "current_diet,%d\n", userSettings.currentDiet) < 0) {
        return -1;
    }
    return 0;
}

static int saveSettings(void) {
    return saveFileAtomic(SETTINGS_PATH, SETTINGS_TMP, writeSettingsFile);
}

static int loadSettings(void) {
    FILE *f = fopen(SETTINGS_PATH, "r");
    if (f == NULL) {
        fprintf(stderr, "Error: cannot open %s: %s\n", SETTINGS_PATH, strerror(errno));
        return -1;
    }
    char line[CSV_LINE_LEN];
    int lineNo = 0;
    int headerSeen = 0;
    int seenTarget = 0;
    int seenDiet = 0;
    int rc = 0;

    while (fgets(line, (int)sizeof line, f) != NULL) {
        lineNo++;
        size_t len = strlen(line);
        if (len == sizeof line - 1 && line[len - 1] != '\n') {
            fprintf(stderr, "Error: %s line %d: line is too long.\n", SETTINGS_PATH, lineNo);
            rc = -1;
            break;
        }
        stripLineEnd(line);

        if (!headerSeen) {
            if (strcmp(line, SETTINGS_HEADER) != 0) {
                fprintf(stderr, "Error: %s line %d: bad header, expected \"%s\".\n",
                    SETTINGS_PATH, lineNo, SETTINGS_HEADER);
                rc = -1;
                break;
            }
            headerSeen = 1;
            continue;
        }
        if (line[0] == '\0') {
            continue;
        }

        char *fields[2];
        if (splitCsvLine(line, fields, 2) != 2) {
            fprintf(stderr, "Error: %s line %d: malformed setting entry.\n", SETTINGS_PATH, lineNo);
            rc = -1;
            break;
        }
        if (strcmp(fields[0], "daily_target") == 0) {
            double v;
            if (seenTarget || parseDoubleStrict(fields[1], &v) != 0 || v <= 0.0) {
                fprintf(stderr, "Error: %s line %d: duplicate or invalid daily_target.\n",
                SETTINGS_PATH, lineNo);
                rc = -1;
                break;
            }
            userSettings.dailyTarget = v;
            seenTarget = 1;
        } else if (strcmp(fields[0], "current_diet") == 0) {
            int v;
            if (seenDiet || parseIntStrict(fields[1], &v) != 0 || v < 1 || v > DIET_COUNT) {
                fprintf(stderr, "Error: %s line %d: duplicate or invalid current_diet (must be 1-%d).\n",
                SETTINGS_PATH, lineNo, DIET_COUNT);
                rc = -1;
                break;
            }
            userSettings.currentDiet = v;
            seenDiet = 1;
        } else {
            fprintf(stderr, "Error: %s line %d: unknown setting \"%s\".\n",
                SETTINGS_PATH, lineNo, fields[0]);
            rc = -1;
            break;
        }
    }

    if (rc == 0 && !headerSeen) {
        fprintf(stderr, "Error: %s is empty or has no header line.\n", SETTINGS_PATH);
        rc = -1;
    }
    if (rc == 0 && ferror(f)) {
        fprintf(stderr, "Error: read error while reading %s.\n", SETTINGS_PATH);
        rc = -1;
    }
    if (rc == 0 && (!seenTarget || !seenDiet)) {
        fprintf(stderr, "Error: %s is missing a required setting (daily_target and current_diet).\n",
                SETTINGS_PATH);
        rc = -1;
    }
    fclose(f);
    return rc;
}

/* ---- startup ---- */

static int initializeDataFiles(void) {
    if (!fileExists(FOODS_PATH)) {
        printf("No %s found; initializing with 15 default foods...\n", FOODS_PATH);
        if (initializeFoods() != 0) {
            return -1;
        }
    }
    if (loadFoods() != 0) {
        return -1;
    }

    if (!fileExists(LOG_PATH)) {
        printf("No %s found; creating it with a header only...\n", LOG_PATH);
        logCount = 0;
        if (saveFoodLog() != 0) {
            return -1;
        }
    }
    if (loadFoodLog() != 0) {
        return -1;
    }

    if (!fileExists(SETTINGS_PATH)) {
        printf("No %s found; writing defaults (target %.10g kcal, diet %d)...\n",
            SETTINGS_PATH, DEFAULT_DAILY_TARGET, DEFAULT_CURRENT_DIET);
        userSettings.dailyTarget = DEFAULT_DAILY_TARGET;
        userSettings.currentDiet = DEFAULT_CURRENT_DIET;
        if (saveSettings() != 0) {
            return -1;
        }
    }
    if (loadSettings() != 0) {
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* 10. Food database functions                                         */
/* ------------------------------------------------------------------ */

static int findFoodIndex(int id) {
    for (int i = 0; i < foodCount; i++) {
        if (foodDatabase[i].id == id) {
            return i;
        }
    }
    return -1;
}

/* New IDs are max(existing) + 1 so IDs of saved records are never reused. */
static int nextFoodId(void) {
    int maxId = 0;
    for (int i = 0; i < foodCount; i++) {
        if (foodDatabase[i].id > maxId) {
            maxId = foodDatabase[i].id;
        }
    }
    return maxId + 1;
}

static void printFoodRow(const Food *fd) {
    printf("%-4d %-24s %8.10g %8.10g %8.10g %8.10g %8.10g %10.10g %10.10g %8.10g\n",
            fd->id, fd->name, fd->calories, fd->protein, fd->carbohydrates,
            fd->fat, fd->fiber, fd->totalSugar, fd->addedSugar, fd->sodium);
}

static void viewFoodDatabase(void) {
    if (foodCount == 0) {
        printf("The food database is empty.\n\n");
        return;
    }
    printf("\n--- Food Database (%d of %d) ---\n", foodCount, MAX_FOODS);
    printf("Units: calories kcal; protein/carbs/fat/fiber/sugars g; sodium mg. Per reference serving.\n");
    printf("%-4s %-24s %8s %8s %8s %8s %8s %10s %10s %8s\n",
        "ID", "Name", "Cal", "Protein", "Carbs", "Fat", "Fiber", "TotSugar", "AddSugar", "Sodium");
    for (int i = 0; i < foodCount; i++) {
        printFoodRow(&foodDatabase[i]);
    }
    printf("\n");
}

static void addCustomFood(void) {
    if (foodCount >= MAX_FOODS) {
        printf("The food database is full (%d records). Cannot add more.\n\n", MAX_FOODS);
        return;
    }

    char name[FOOD_NAME_LEN];
    printf("Enter food name (letters, digits, spaces; no commas or quotes; max %d chars): ", NAME_MAX_CHARS);
    fflush(stdout);
    InputStatus st = readLine(name, sizeof name);
    if (st == INPUT_EOF) {
        return;
    }
    if (st == INPUT_TOO_LONG) {
        printf("Name is too long (max %d characters).\n\n", NAME_MAX_CHARS);
        return;
    }
    if (!validFoodName(name)) {
        printf("Invalid name: use 1-%d characters with no commas, quotes, or control characters.\n\n",
        NAME_MAX_CHARS);
        return;
    }

    Food food;
    food.id = nextFoodId();
    strcpy(food.name, name); /* safe: validFoodName bounds the length */

    if (promptDouble("Calories (kcal): ", 1, &food.calories) != INPUT_OK ||
        promptDouble("Protein (g): ", 1, &food.protein) != INPUT_OK ||
        promptDouble("Carbohydrates (g): ", 1, &food.carbohydrates) != INPUT_OK ||
        promptDouble("Fat (g): ", 1, &food.fat) != INPUT_OK ||
        promptDouble("Fiber (g): ", 1, &food.fiber) != INPUT_OK ||
        promptDouble("Total sugar (g): ", 1, &food.totalSugar) != INPUT_OK ||
        promptDouble("Added sugar (g): ", 1, &food.addedSugar) != INPUT_OK ||
        promptDouble("Sodium (mg): ", 1, &food.sodium) != INPUT_OK) {
        printf("Input closed; food was not added.\n\n");
        return;
    }
    if (food.addedSugar > food.totalSugar) {
        printf("Added sugar (%.10g g) cannot exceed total sugar (%.10g g). Food was not added.\n\n",
            food.addedSugar, food.totalSugar);
        return;
    }

    foodDatabase[foodCount++] = food;
    if (saveFoods() != 0) {
        foodCount--; /* roll back the in-memory copy so it matches the disk */
        printf("Error: could not save %s; the food was not added. Existing data was preserved.\n\n",
            FOODS_PATH);
        return;
    }
    printf("Added food ID %d: %s\n\n", food.id, food.name);
}

static void evaluateFood(void) {
    if (foodCount == 0) {
        printf("The food database is empty.\n\n");
        return;
    }
    int id;
    InputStatus st = promptInteger("Enter the food ID to evaluate: ", 1, INT_MAX, &id);
    if (st != INPUT_OK) {
        return;
    }
    int idx = findFoodIndex(id);
    if (idx < 0) {
        printf("No food with ID %d exists.\n\n", id);
        return;
    }
    const Food *f = &foodDatabase[idx];

    printf("\n--- %s (ID %d), per serving ---\n", f->name, f->id);
    printf("Calories %.10g kcal | Protein %.10g g | Carbs %.10g g | Fat %.10g g | Fiber %.10g g\n",
        f->calories, f->protein, f->carbohydrates, f->fat, f->fiber);
    printf("Total sugar %.10g g | Added sugar %.10g g | Sodium %.10g mg\n",
        f->totalSugar, f->addedSugar, f->sodium);
    printf("Screening for: %s\n", DIET_NAMES[userSettings.currentDiet - 1]);

    switch (userSettings.currentDiet) {
        case 1: /* Balanced Diet */
            printf("Balanced Diet guidance:\n");
            printf("- One serving provides %.10g kcal, about %.1f%% of your %.10g kcal daily target.\n",
                f->calories, f->calories / userSettings.dailyTarget * 100.0, userSettings.dailyTarget);
            printf("- Portion-size considerations: every additional serving multiplies all nutrients;\n");
            printf("  compare the serving count with the remaining calories for the day.\n");
            printf("- Food-variety considerations: pair this food with vegetables, whole grains, and\n");
            printf("  lean proteins so the day overall stays balanced.\n");
            break;
        case 2: { /* Calorie Deficit Tracking */
            double servings;
            if (promptDouble("Servings to evaluate: ", 0, &servings) != INPUT_OK) {
                printf("\n");
                return;
            }
            double total = f->calories * servings;
            double remaining = userSettings.dailyTarget - total;
            printf("Selected food: %.10g kcal per serving x %.10g serving(s) = %.10g kcal.\n",
                f->calories, servings, total);
            printf("Daily target: %.10g kcal (%.1f%% of the target used).\n",
                userSettings.dailyTarget, total / userSettings.dailyTarget * 100.0);
            if (remaining >= 0.0) {
                printf("This fits within the daily target; %.10g kcal remaining.\n", remaining);
            } else {
                printf("This exceeds the daily target by %.10g kcal.\n", -remaining);
            }
            break;
        }
        case 3:
            printf("High Protein requires protein >= %.10g g: %s (%.10g g).\n",
                HIGH_PROTEIN_MIN, f->protein >= HIGH_PROTEIN_MIN ? "MEETS" : "DOES NOT MEET", f->protein);
            break;
        case 4:
            printf("Low Carbohydrate requires carbs <= %.10g g: %s (%.10g g).\n",
                LOW_CARB_MAX, f->carbohydrates <= LOW_CARB_MAX ? "MEETS" : "DOES NOT MEET", f->carbohydrates);
            break;
        case 5:
            printf("Low Fat requires fat <= %.10g g: %s (%.10g g).\n",
                LOW_FAT_MAX, f->fat <= LOW_FAT_MAX ? "MEETS" : "DOES NOT MEET", f->fat);
            break;
        case 6:
            printf("Low Sodium requires sodium <= %.10g mg: %s (%.10g mg).\n",
                LOW_SODIUM_MAX, f->sodium <= LOW_SODIUM_MAX ? "MEETS" : "DOES NOT MEET", f->sodium);
            break;
        case 7:
            printf("Low Added Sugar requires added sugar <= %.10g g: %s (%.10g g).\n",
                LOW_ADDED_SUGAR_MAX, f->addedSugar <= LOW_ADDED_SUGAR_MAX ? "MEETS" : "DOES NOT MEET", f->addedSugar);
            break;
        case 8:
            printf("High Fiber requires fiber >= %.10g g: %s (%.10g g).\n",
                HIGH_FIBER_MIN, f->fiber >= HIGH_FIBER_MIN ? "MEETS" : "DOES NOT MEET", f->fiber);
            break;
        default:
            break;
    }
    printf(DISCLAIMER);
    printf("\n");
}

/* ------------------------------------------------------------------ */
/* 11. Food log and report functions                                   */
/* ------------------------------------------------------------------ */

static void getTodayDate(char out[11]) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (tm == NULL || strftime(out, 11, "%Y-%m-%d", tm) == 0) {
        /* Extremely unlikely fallback; keeps the buffer a valid string. */
        strcpy(out, "1970-01-01");
    }
}

static void recordIntake(void) {
    if (logCount >= MAX_LOG_ENTRIES) {
        printf("The food log is full (%d entries). Cannot record more.\n\n", MAX_LOG_ENTRIES);
        return;
    }
    int id;
    InputStatus st = promptInteger("Enter the food ID you ate: ", 1, INT_MAX, &id);
    if (st != INPUT_OK) {
        return;
    }
    int idx = findFoodIndex(id);
    if (idx < 0) {
        printf("No food with ID %d exists.\n\n", id);
        return;
    }
    double servings;
    if (promptDouble("Servings: ", 0, &servings) != INPUT_OK) {
        printf("\n");
        return;
    }

    FoodLog entry;
    getTodayDate(entry.date);
    entry.foodId = id;
    entry.servings = servings;
    foodLog[logCount++] = entry;

    if (saveFoodLog() != 0) {
        logCount--; /* roll back so memory matches the disk */
        printf("Error: could not save %s; the entry was not recorded. Existing data was preserved.\n\n",
            LOG_PATH);
        return;
    }
    printf("Recorded %s (ID %d) x %.10g serving(s) on %s.\n\n",
        foodDatabase[idx].name, id, servings, entry.date);
}

static void dailyReport(void) {
    char today[11];
    getTodayDate(today);

    double calories = 0.0, protein = 0.0, carbs = 0.0, fat = 0.0;
    double fiber = 0.0, totalSugar = 0.0, sodium = 0.0;
    int entries = 0;

    for (int i = 0; i < logCount; i++) {
        if (strcmp(foodLog[i].date, today) != 0) {
            continue; /* report covers only the current date */
        }
        int idx = findFoodIndex(foodLog[i].foodId);
        if (idx < 0) {
            continue; /* cannot happen: log entries are validated against the food IDs */
        }
        const Food *f = &foodDatabase[idx];
        double s = foodLog[i].servings;
        calories += f->calories * s;
        protein += f->protein * s;
        carbs += f->carbohydrates * s;
        fat += f->fat * s;
        fiber += f->fiber * s;
        totalSugar += f->totalSugar * s;
        sodium += f->sodium * s;
        entries++;
    }

    printf("\n--- Daily Nutrition Report for %s ---\n", today);
    if (entries == 0) {
        printf("No food intake recorded for today.\n");
    } else {
        printf("Entries today: %d\n", entries);
    }
    printf("Calories:    %10.10g kcal\n", calories);
    printf("Protein:     %10.10g g\n", protein);
    printf("Carbs:       %10.10g g\n", carbs);
    printf("Fat:         %10.10g g\n", fat);
    printf("Fiber:       %10.10g g\n", fiber);
    printf("Total sugar: %10.10g g\n", totalSugar);
    printf("Sodium:      %10.10g mg\n", sodium);
    printf("Daily target: %.10g kcal\n", userSettings.dailyTarget);
    double remaining = userSettings.dailyTarget - calories;
    if (remaining >= 0.0) {
        printf("Remaining within target: %.10g kcal\n", remaining);
    } else {
        printf("Exceeding target by: %.10g kcal\n", -remaining);
    }
    printf(DISCLAIMER);
    printf("\n");
}

/* ------------------------------------------------------------------ */
/* 12. Settings functions                                              */
/* ------------------------------------------------------------------ */

static void selectDiet(void) {
    printf("\n--- Select Diet Category ---\n");
    for (int i = 0; i < DIET_COUNT; i++) {
        printf("%d. %s%s\n", i + 1, DIET_NAMES[i],
            userSettings.currentDiet == i + 1 ? " (current)" : "");
    }
    int diet;
    if (promptInteger("Select diet (1-8): ", 1, DIET_COUNT, &diet) != INPUT_OK) {
        printf("\n");
        return;
    }
    int old = userSettings.currentDiet;
    userSettings.currentDiet = diet;
    if (saveSettings() != 0) {
        userSettings.currentDiet = old;
        printf("Error: could not save %s; the diet was not changed.\n\n", SETTINGS_PATH);
        return;
    }
    printf("Diet set to %s.\n\n", DIET_NAMES[diet - 1]);
}

static void setCalorieTarget(void) {
    double target;
    if (promptDouble("New daily calorie target (kcal, > 0): ", 0, &target) != INPUT_OK) {
        printf("\n");
        return;
    }
    double old = userSettings.dailyTarget;
    userSettings.dailyTarget = target;
    if (saveSettings() != 0) {
        userSettings.dailyTarget = old;
        printf("Error: could not save %s; the target was not changed.\n\n", SETTINGS_PATH);
        return;
    }
    printf("Daily calorie target set to %.10g kcal.\n\n", target);
}
