# NutriTracker

A terminal-based nutrition and diet management system written in C (C11, standard
libraries only). It keeps a food database, records what you ate, screens foods
against a selectable diet category, and produces a daily nutrition report. All
data is persisted in plain CSV files under `data/`.

## Requirements

- A C11 compiler (developed with GCC 13 on Linux)
- Standard C library only — no external libraries, no database engine

## Compilation

Strict build (recommended):

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -g main.c -o nutritracker
```

Memory-safety debug build (AddressSanitizer + UndefinedBehaviorSanitizer):

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer main.c -o nutritracker-debug
```

## Execution

Run from the project root so the program can find its `data/` directory:

```sh
./nutritracker
```

The `data/` directory ships with the repository (`data/.gitkeep`). Do not delete
it; the program does not create directories itself. If it is missing, the
program prints an error explaining how to fix it and exits without touching
anything.

## Features (main menu)

1. **Select Diet Category** — one of 8 diets: Balanced, Calorie Deficit
   Tracking, High Protein (protein ≥ 20 g), Low Carbohydrate (carbs ≤ 15 g),
   Low Fat (fat ≤ 3 g), Low Sodium (sodium ≤ 140 mg), Low Added Sugar
   (added sugar ≤ 5 g), High Fiber (fiber ≥ 5 g).
2. **View Food Database** — lists all foods with their nutrients.
3. **Evaluate Food for My Diet** — screens one food against the selected diet.
4. **Add Custom Food Product** — adds a food (unique ID assigned automatically)
   and saves it permanently.
5. **Set Daily Calorie Target** — must be greater than zero.
6. **Record Food Intake** — logs food ID + servings under the current local date.
7. **View Daily Nutrition Report** — totals for today only, plus remaining or
   excess calories against the daily target.
8. **Exit Program**.

> Disclaimer: NutriTracker evaluations are educational screening results only
> and are not medical recommendations.

## CSV files

All paths are fixed application-controlled paths; user input is never used as
a file path.

### `data/foods.csv`

```csv
id,name,calories,protein,carbohydrates,fat,fiber,total_sugar,added_sugar,sodium
1,Boiled Egg,78,6.3,0.6,5.3,0,0.6,0,62
```

- One row per food. Capacity: 100 records.
- Units per reference serving: calories in kcal; protein, carbohydrates, fat,
  fiber, and sugars in grams; sodium in milligrams. Serving weights are not
  specified by the project documentation.
- On first run (file missing) it is initialized with 15 predefined foods.
- IDs are unique and never reused; new foods get `max(id) + 1`.

### `data/food_log.csv`

```csv
date,food_id,servings
2026-11-26,1,2
```

- One row per consumed food. Capacity: 200 records. Dates are `YYYY-MM-DD`
  (local system date via `time.h`). Entries reference foods by ID, so they stay
  valid regardless of row order. Historical entries are preserved; only entries
  matching the current date appear in the daily report.

### `data/settings.csv`

```csv
setting,value
daily_target,2000
current_diet,1
```

- `daily_target` in kcal (default 2000, must be > 0), `current_diet` 1–8
  (default 1 = Balanced Diet). Defaults are written only when the file is
  missing; a corrupted file is reported and never overwritten.

## How initialization and persistence work

At startup the program checks each CSV file:

- Missing `foods.csv` → initialized with the 15 default foods.
- Missing `food_log.csv` → created with a header only.
- Missing `settings.csv` → created with defaults.
- Existing files are loaded and validated (headers, field counts, numeric
  ranges, finite values, unique food IDs, log entries referencing existing food
  IDs, no duplicate setting keys). Malformed data is reported with the file
  name and line number, and the program exits rather than overwriting or
  guessing — a corrupted file is never replaced with defaults.

Saving is atomic: data is written to a separate temporary file in the same
directory (`*.csv.tmp`), every write and the close are checked, and only then
is the original replaced via `rename()`. If any step fails, the original file
is preserved and the error is reported.

## Known limitations

- Single local user; no accounts or authentication.
- CSV fields are not quoted; food names may contain spaces but reject commas,
  quotes, and line breaks (enforced on input and on load).
- Fixed capacities: 100 foods, 200 log entries (no dynamic allocation).
- Dates use the local system timezone; the report covers the current date only.

## Memory-safety checks

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer main.c -o nutritracker-debug
./nutritracker-debug        # exercise the menus, then exit with option 8

valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --error-exitcode=1 ./nutritracker-debug
```

The program uses only fixed-size static arrays (no `malloc`/`free`), so there
is no heap to leak; every opened file is closed on all paths, including error
and EOF paths. Sanitizer and Valgrind runs over the functional scenarios
(first run, add food, record intake, report, settings change, invalid input,
EOF, corrupted files, full database) report zero errors.
